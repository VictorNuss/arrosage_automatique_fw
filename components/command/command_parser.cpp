#include "command.h"

#include <cstring>

#include "cJSON.h"

namespace {

bool copy_valve_key(const cJSON* vanne_item, Command* out)
{
    if (!cJSON_IsString(vanne_item) || vanne_item->valuestring[0] == '\0') {
        return false;
    }
    std::strncpy(out->valve_key, vanne_item->valuestring, sizeof(out->valve_key) - 1);
    out->valve_key[sizeof(out->valve_key) - 1] = '\0';
    return true;
}

}  // namespace

bool command_parse(const char* json, size_t len, Command* out)
{
    *out = Command{};

    cJSON* root = cJSON_ParseWithLength(json, len);
    if (root == nullptr) {
        return false;
    }

    bool ok = false;
    const cJSON* action_item = cJSON_GetObjectItemCaseSensitive(root, "action");

    if (cJSON_IsString(action_item)) {
        if (std::strcmp(action_item->valuestring, "open") == 0) {
            const cJSON* vanne_item = cJSON_GetObjectItemCaseSensitive(root, "vanne");
            const cJSON* duration_item = cJSON_GetObjectItemCaseSensitive(root, "duration_s");
            // Pas de duree implicite : un arrosage est securite-critique, on
            // rejette plutot que de deviner une duree par defaut. La
            // validation porte sur la valeur UNE FOIS TRONQUEE en secondes
            // entieres (>= 1), pas sur le double brut : un duration_s dans
            // (0,1) validerait sinon "> 0" mais tronquerait vers 0, et 0 est
            // traite plus loin par valve_logic_clamp_duration comme "non
            // specifie" - ouvrant la vanne pour sa duree MAX configuree au
            // lieu d'etre rejete. La borne haute evite aussi un cast
            // double->uint32_t hors plage (comportement indefini).
            if (copy_valve_key(vanne_item, out) && cJSON_IsNumber(duration_item) &&
                duration_item->valuedouble >= 1.0 && duration_item->valuedouble <= (double)UINT32_MAX) {
                out->type = CommandType::OpenValve;
                out->duration_s = (uint32_t)duration_item->valuedouble;
                ok = true;
            }
        } else if (std::strcmp(action_item->valuestring, "close") == 0) {
            const cJSON* vanne_item = cJSON_GetObjectItemCaseSensitive(root, "vanne");
            if (copy_valve_key(vanne_item, out)) {
                out->type = CommandType::CloseValve;
                ok = true;
            }
        } else if (std::strcmp(action_item->valuestring, "stop_all") == 0) {
            out->type = CommandType::StopAll;
            ok = true;
        } else if (std::strcmp(action_item->valuestring, "get_status") == 0) {
            out->type = CommandType::GetStatus;
            ok = true;
        }
    }

    if (!ok) {
        *out = Command{};
    }

    cJSON_Delete(root);
    return ok;
}
