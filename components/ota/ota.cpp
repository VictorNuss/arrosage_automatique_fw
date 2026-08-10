#include "ota.h"

#include "esp_log.h"
#include "esp_ota_ops.h"

namespace {

const char* TAG = "ota";

esp_ota_handle_t s_handle = 0;
const esp_partition_t* s_update_partition = nullptr;
bool s_in_progress = false;

}  // namespace

esp_err_t ota_begin(size_t image_size)
{
    if (s_in_progress) {
        return ESP_ERR_INVALID_STATE;
    }

    s_update_partition = esp_ota_get_next_update_partition(nullptr);
    if (s_update_partition == nullptr) {
        ESP_LOGE(TAG, "Aucune partition OTA disponible (table de partitions incompatible ?)");
        return ESP_FAIL;
    }

    esp_err_t err =
        esp_ota_begin(s_update_partition, image_size == 0 ? OTA_SIZE_UNKNOWN : image_size, &s_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin echouee (%s)", esp_err_to_name(err));
        return err;
    }

    s_in_progress = true;
    ESP_LOGI(TAG, "OTA demarree vers la partition '%s'", s_update_partition->label);
    return ESP_OK;
}

esp_err_t ota_write(const void* data, size_t len)
{
    if (!s_in_progress) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = esp_ota_write(s_handle, data, len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_write echouee (%s)", esp_err_to_name(err));
    }
    return err;
}

esp_err_t ota_finish(void)
{
    if (!s_in_progress) {
        return ESP_ERR_INVALID_STATE;
    }
    s_in_progress = false;

    esp_err_t err = esp_ota_end(s_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end echouee (%s) - image invalide ou incomplete, ancienne image conservee",
                 esp_err_to_name(err));
        return err;
    }

    err = esp_ota_set_boot_partition(s_update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition echouee (%s)", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "OTA terminee et validee, la nouvelle image sera utilisee au prochain demarrage");
    return ESP_OK;
}

void ota_abort(void)
{
    if (s_in_progress) {
        esp_ota_abort(s_handle);
        s_in_progress = false;
        ESP_LOGW(TAG, "OTA annulee, ancienne image conservee");
    }
}

void ota_confirm_boot_ok(void)
{
    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    if (err != ESP_OK && err != ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(TAG, "Confirmation du boot OTA impossible (%s)", esp_err_to_name(err));
    }
}
