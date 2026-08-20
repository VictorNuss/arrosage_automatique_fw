#include "web_server.h"

#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "command.h"
#include "ota.h"
#include "state_json.h"

namespace {

const char* TAG = "web_server";

QueueHandle_t s_command_queue = nullptr;

// Page de test auto-suffisante (pas de dependance externe, pas de systeme
// de fichiers) : affiche capteurs/vannes via /api/state (rafraichi toutes
// les 3s) et pilote les vannes via /api/command, au meme format JSON que le
// contrat MQTT. Les lignes de vannes sont generees dynamiquement a partir
// des cles "vanne_*" de l'etat recu - fonctionne sans modification si des
// vannes sont ajoutees a la table de config.
//
// Decoupee en trois morceaux envoyes en chunked (voir index_get_handler) pour
// inserer conditionnellement la section OTA sans dupliquer toute la page
// selon CONFIG_ARROSAGE_ENABLE_OTA.
const char kIndexHtmlHead[] = R"HTML(<!doctype html>
<html lang="fr">
<head>
<meta charset="utf-8">
<title>Arrosage - test local</title>
<style>
body{font-family:sans-serif;max-width:640px;margin:2rem auto;padding:0 1rem}
table{width:100%;border-collapse:collapse;margin-bottom:1.5rem}
td,th{padding:.4rem;border-bottom:1px solid #ddd;text-align:left}
button{padding:.3rem .8rem;margin-right:.3rem}
input[type=number]{width:5rem}
#status{color:#666;font-size:.9rem}
</style>
</head>
<body>
<h1>Arrosage - test local</h1>
<p id="status">Chargement...</p>
<h2>Capteurs</h2>
<table id="sensors"></table>
<h2>Vannes</h2>
<table id="valves"></table>
<p><button onclick="stopAll()">Arret d'urgence (toutes les vannes)</button></p>
)HTML";

#if CONFIG_ARROSAGE_ENABLE_OTA
const char kOtaSectionHtml[] = R"HTML(<h2>Mise a jour firmware (OTA)</h2>
<p>
  <input type="file" id="ota_file" accept=".bin">
  <button onclick="uploadOta()">Envoyer et redemarrer</button>
</p>
<p id="ota_status"></p>
)HTML";
#endif

const char kIndexHtmlTail[] = R"HTML(<script>
async function refresh() {
  try {
    const r = await fetch('/api/state');
    const s = await r.json();
    document.getElementById('status').textContent = 'Derniere mise a jour : ' + s.ts;
    const sensors = document.getElementById('sensors');
    sensors.innerHTML = '';
    const valves = document.getElementById('valves');
    valves.innerHTML = '';
    for (const key in s) {
      if (key === 'ts') continue;
      if (key.startsWith('vanne_')) {
        valves.innerHTML += '<tr><td>' + key + '</td><td>' + s[key] + '</td>' +
          '<td><input type="number" id="d_' + key + '" value="60" min="1"> s ' +
          '<button onclick="openValve(\'' + key + '\')">Ouvrir</button> ' +
          '<button onclick="closeValve(\'' + key + '\')">Fermer</button></td></tr>';
      } else {
        sensors.innerHTML += '<tr><td>' + key + '</td><td>' + s[key] + '</td></tr>';
      }
    }
  } catch (e) {
    document.getElementById('status').textContent = "Erreur de lecture de l'etat";
  }
}
async function sendCommand(cmd) {
  await fetch('/api/command', {method: 'POST', body: JSON.stringify(cmd)});
  refresh();
}
function openValve(key) {
  const d = parseInt(document.getElementById('d_' + key).value, 10) || 60;
  sendCommand({vanne: key, action: 'open', duration_s: d});
}
function closeValve(key) {
  sendCommand({vanne: key, action: 'close'});
}
function stopAll() {
  sendCommand({action: 'stop_all'});
}
async function uploadOta() {
  const fileInput = document.getElementById('ota_file');
  const statusEl = document.getElementById('ota_status');
  if (!fileInput || !fileInput.files.length) { return; }
  const file = fileInput.files[0];
  statusEl.textContent = 'Envoi en cours (' + file.size + ' octets)...';
  try {
    const r = await fetch('/api/ota', {method: 'POST', body: file});
    const text = await r.text();
    statusEl.textContent = r.ok ? text : 'Erreur : ' + text;
  } catch (e) {
    statusEl.textContent = "Erreur reseau pendant l'envoi (le device a peut-etre deja redemarre)";
  }
}
refresh();
setInterval(refresh, 3000);
</script>
</body>
</html>
)HTML";

esp_err_t index_get_handler(httpd_req_t* req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send_chunk(req, kIndexHtmlHead, HTTPD_RESP_USE_STRLEN);
#if CONFIG_ARROSAGE_ENABLE_OTA
    httpd_resp_send_chunk(req, kOtaSectionHtml, HTTPD_RESP_USE_STRLEN);
#endif
    httpd_resp_send_chunk(req, kIndexHtmlTail, HTTPD_RESP_USE_STRLEN);
    return httpd_resp_send_chunk(req, nullptr, 0);
}

esp_err_t state_get_handler(httpd_req_t* req)
{
    char* payload = state_json_build();
    if (payload == nullptr) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "construction du JSON d'etat echouee");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_send(req, payload, HTTPD_RESP_USE_STRLEN);
    cJSON_free(payload);
    return err;
}

esp_err_t command_post_handler(httpd_req_t* req)
{
    // Meme limite que la queue MQTT (Command::valve_key est petit) : un
    // payload de commande n'a pas de raison de depasser cette taille.
    constexpr size_t kMaxBodyLen = 256;

    if (req->content_len == 0 || req->content_len >= kMaxBodyLen) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "payload absent ou trop volumineux");
        return ESP_FAIL;
    }

    char buf[kMaxBodyLen];
    size_t received = 0;
    while (received < req->content_len) {
        int ret = httpd_req_recv(req, buf + received, req->content_len - received);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "lecture du corps de la requete echouee");
            return ESP_FAIL;
        }
        received += (size_t)ret;
    }
    buf[received] = '\0';

    Command cmd;
    if (!command_parse(buf, received, &cmd)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                             "commande invalide (voir docs/mqtt_contract.md pour le format attendu)");
        return ESP_FAIL;
    }

    if (xQueueSend(s_command_queue, &cmd, 0) != pdTRUE) {
        ESP_LOGW(TAG, "File de commandes pleine, commande recue via web ignoree");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "file de commandes pleine");
        return ESP_FAIL;
    }

    return httpd_resp_sendstr(req, "OK");
}

#if CONFIG_ARROSAGE_ENABLE_OTA
esp_err_t ota_post_handler(httpd_req_t* req)
{
    if (req->content_len == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "payload absent");
        return ESP_FAIL;
    }

    if (ota_begin(req->content_len) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                             "impossible de demarrer la mise a jour (voir logs serie)");
        return ESP_FAIL;
    }

    char buf[1024];
    size_t remaining = req->content_len;
    while (remaining > 0) {
        size_t to_read = remaining < sizeof(buf) ? remaining : sizeof(buf);
        int received = httpd_req_recv(req, buf, to_read);
        if (received <= 0) {
            ota_abort();
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "lecture du corps de la requete echouee");
            return ESP_FAIL;
        }

        if (ota_write(buf, (size_t)received) != ESP_OK) {
            ota_abort();
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "ecriture flash echouee (voir logs serie)");
            return ESP_FAIL;
        }
        remaining -= (size_t)received;
    }

    if (ota_finish() != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                             "image invalide, mise a jour annulee (voir logs serie)");
        return ESP_FAIL;
    }

    httpd_resp_sendstr(req, "OK, redemarrage en cours...");

    // Laisse le temps a la reponse HTTP de partir avant de couper la connexion.
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    return ESP_OK;  // jamais atteint
}
#endif

}  // namespace

void web_server_start(QueueHandle_t command_queue)
{
    s_command_queue = command_queue;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
#if CONFIG_ARROSAGE_ENABLE_OTA
    config.stack_size = 8192;  // ota_post_handler utilise un buffer de 1KB sur la pile
#endif
    httpd_handle_t server = nullptr;

    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Demarrage du serveur web echoue");
        return;
    }

    httpd_uri_t index_uri = {};
    index_uri.uri = "/";
    index_uri.method = HTTP_GET;
    index_uri.handler = &index_get_handler;
    httpd_register_uri_handler(server, &index_uri);

    httpd_uri_t state_uri = {};
    state_uri.uri = "/api/state";
    state_uri.method = HTTP_GET;
    state_uri.handler = &state_get_handler;
    httpd_register_uri_handler(server, &state_uri);

    httpd_uri_t command_uri = {};
    command_uri.uri = "/api/command";
    command_uri.method = HTTP_POST;
    command_uri.handler = &command_post_handler;
    httpd_register_uri_handler(server, &command_uri);

#if CONFIG_ARROSAGE_ENABLE_OTA
    httpd_uri_t ota_uri = {};
    ota_uri.uri = "/api/ota";
    ota_uri.method = HTTP_POST;
    ota_uri.handler = &ota_post_handler;
    httpd_register_uri_handler(server, &ota_uri);
    ESP_LOGW(TAG, "OTA active : POST /api/ota permet a quiconque sur le reseau local de reflasher ce device");
#endif

    ESP_LOGI(TAG, "Serveur web de test demarre sur le port %d", config.server_port);
}
