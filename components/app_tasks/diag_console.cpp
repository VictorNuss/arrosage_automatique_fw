#include "diag_console.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "cJSON.h"
#include "esp_console.h"
#include "esp_log.h"

#include "sensor_manager.h"
#include "valve_manager.h"

namespace {

// atoi() retourne silencieusement 0 sur une entree non numerique (faute de
// frappe) : sur la console diag, 0 est un index de vanne valide, ce qui
// ferait agir sur la mauvaise vanne physique sans le moindre avertissement.
bool parse_index(const char* str, int* out)
{
    char* endptr = nullptr;
    long value = strtol(str, &endptr, 10);
    if (endptr == str || *endptr != '\0' || value < 0) {
        return false;
    }
    *out = (int)value;
    return true;
}

int cmd_valve(int argc, char** argv)
{
    if (argc < 3) {
        printf("usage: valve <open|close> <index> [duree_s]\n");
        return 1;
    }

    int idx;
    if (!parse_index(argv[2], &idx)) {
        printf("index invalide : '%s' (attendu un entier positif)\n", argv[2]);
        return 1;
    }

    if (strcmp(argv[1], "open") == 0) {
        uint32_t duration = argc > 3 ? (uint32_t)atoi(argv[3]) : 0;
        printf(valve_manager_open(idx, duration) ? "OK\n" : "index invalide\n");
    } else if (strcmp(argv[1], "close") == 0) {
        printf(valve_manager_close(idx) ? "OK\n" : "index invalide\n");
    } else {
        printf("action inconnue : '%s' (attendu open|close)\n", argv[1]);
        return 1;
    }
    return 0;
}

int cmd_sensor(int /*argc*/, char** /*argv*/)
{
    cJSON* root = cJSON_CreateObject();
    sensor_manager_collect(root);
    char* json = cJSON_Print(root);
    if (json != nullptr) {
        printf("%s\n", json);
        cJSON_free(json);
    }
    cJSON_Delete(root);
    return 0;
}

}  // namespace

void diag_console_start(void)
{
    esp_console_repl_t* repl = nullptr;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "arrosage>";

    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();

    esp_console_register_help_command();

    esp_console_cmd_t valve_cmd = {};
    valve_cmd.command = "valve";
    valve_cmd.help = "Piloter une vanne : valve <open|close> <index> [duree_s]";
    valve_cmd.func = &cmd_valve;
    esp_console_cmd_register(&valve_cmd);

    esp_console_cmd_t sensor_cmd = {};
    sensor_cmd.command = "sensor";
    sensor_cmd.help = "Lire tous les capteurs et afficher le JSON resultant";
    sensor_cmd.func = &cmd_sensor;
    esp_console_cmd_register(&sensor_cmd);

    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
