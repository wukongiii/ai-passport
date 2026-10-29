#include "app_preferences_store.h"
#include "nvs.h"
#include "nvs_flash.h"
static nvs_handle_t handle;
static bool ready;
app_pref_store_status_t app_preferences_load(app_preferences_t *p) {
    *p=app_preferences_default(); ready=false;
    if(nvs_flash_init()!=ESP_OK || nvs_open("app_settings",NVS_READWRITE,&handle)!=ESP_OK) return APP_PREF_UNAVAILABLE;
    ready=true;
    uint8_t b[APP_PREFERENCES_SIZE]; size_t size=sizeof(b);
    esp_err_t e=nvs_get_blob(handle,"config",b,&size);
    if(e==ESP_OK) return app_preferences_decode(p,b,size) ? APP_PREF_LOADED : APP_PREF_INVALID;
    if(e==ESP_ERR_NVS_INVALID_LENGTH) return APP_PREF_INVALID;
    if(e!=ESP_ERR_NVS_NOT_FOUND) return APP_PREF_UNAVAILABLE;
    nvs_handle_t old;
    e=nvs_open("billiards",NVS_READONLY,&old);
    if(e==ESP_ERR_NVS_NOT_FOUND) return APP_PREF_DEFAULT;
    if(e!=ESP_OK) return APP_PREF_UNAVAILABLE;
    uint8_t legacy[APP_SETTINGS_SIZE]; size=sizeof(legacy);
    e=nvs_get_blob(old,"ui",legacy,&size); nvs_close(old);
    if(e==ESP_ERR_NVS_NOT_FOUND) return APP_PREF_DEFAULT;
    if(e==ESP_ERR_NVS_INVALID_LENGTH) return APP_PREF_INVALID;
    if(e!=ESP_OK) return APP_PREF_UNAVAILABLE;
    return app_settings_decode(&p->rotation,legacy,size) ? APP_PREF_MIGRATED : APP_PREF_INVALID;
}
esp_err_t app_preferences_save(const app_preferences_t *p) {
    if(!ready) return ESP_ERR_INVALID_STATE;
    if(!app_preferences_valid(p)) return ESP_ERR_INVALID_ARG;
    uint8_t b[APP_PREFERENCES_SIZE]; app_preferences_encode(p,b);
    esp_err_t e=nvs_set_blob(handle,"config",b,sizeof(b));
    return e==ESP_OK ? nvs_commit(handle) : e;
}
