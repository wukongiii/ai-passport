#include "score_store.h"
#include "nvs.h"
#include "nvs_flash.h"

static nvs_handle_t s_handle;
static bool s_ready;

score_store_status_t score_store_open(score_session_t *session) {
    s_ready = false;
    if (nvs_flash_init() != ESP_OK) return SCORE_STORE_UNAVAILABLE;
    if (nvs_open("billiards", NVS_READWRITE, &s_handle) != ESP_OK) return SCORE_STORE_UNAVAILABLE;
    s_ready = true;
    uint8_t record[SCORE_RECORD_SIZE];
    size_t size = sizeof(record);
    esp_err_t error = nvs_get_blob(s_handle, "session", record, &size);
    if (error == ESP_ERR_NVS_NOT_FOUND) return SCORE_STORE_EMPTY;
    if (error == ESP_ERR_NVS_INVALID_LENGTH) return SCORE_STORE_INVALID;
    if (error != ESP_OK) return SCORE_STORE_UNAVAILABLE;
    return score_decode(session, record, size) ? SCORE_STORE_LOADED : SCORE_STORE_INVALID;
}

esp_err_t score_store_save(const score_session_t *session) {
    if (!s_ready) return ESP_ERR_INVALID_STATE;
    uint8_t record[SCORE_RECORD_SIZE];
    if (!score_encode(session, record, sizeof(record))) return ESP_ERR_INVALID_ARG;
    esp_err_t error = nvs_set_blob(s_handle, "session", record, sizeof(record));
    return error == ESP_OK ? nvs_commit(s_handle) : error;
}

score_store_status_t score_store_load_rotation(app_rotation_t *rotation) {
    if (!s_ready) return SCORE_STORE_UNAVAILABLE;
    uint8_t record[APP_SETTINGS_SIZE];
    size_t size = sizeof(record);
    esp_err_t error = nvs_get_blob(s_handle, "ui", record, &size);
    if (error == ESP_ERR_NVS_NOT_FOUND) return SCORE_STORE_EMPTY;
    if (error == ESP_ERR_NVS_INVALID_LENGTH) return SCORE_STORE_INVALID;
    if (error != ESP_OK) return SCORE_STORE_UNAVAILABLE;
    return app_settings_decode(rotation, record, size) ? SCORE_STORE_LOADED : SCORE_STORE_INVALID;
}

esp_err_t score_store_save_rotation(app_rotation_t rotation) {
    if (!s_ready) return ESP_ERR_INVALID_STATE;
    if (!app_rotation_valid(rotation)) return ESP_ERR_INVALID_ARG;
    uint8_t record[APP_SETTINGS_SIZE];
    app_settings_encode(rotation, record);
    esp_err_t error = nvs_set_blob(s_handle, "ui", record, sizeof(record));
    return error == ESP_OK ? nvs_commit(s_handle) : error;
}
