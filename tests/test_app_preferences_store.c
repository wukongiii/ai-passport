#include "app/app_preferences_store.h"
#include "nvs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static bool has_config,has_legacy,fail_read,fail_write,fail_commit,fail_init,fail_open,oversize;
static uint8_t config[APP_PREFERENCES_SIZE],legacy[APP_SETTINGS_SIZE];
static unsigned writes,closes;
esp_err_t nvs_flash_init(void){return fail_init ? ESP_FAIL : ESP_OK;}
esp_err_t nvs_open(const char *name,int mode,nvs_handle_t *h){
    if(fail_open)return ESP_FAIL;
    *h=!strcmp(name,"app_settings") ? 1 : 2;
    assert(mode==(*h==1 ? NVS_READWRITE : NVS_READONLY));return ESP_OK;
}
void nvs_close(nvs_handle_t h){assert(h==2);++closes;}
esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *b,size_t *size){
    (void)key;if(fail_read)return ESP_FAIL;if(oversize)return ESP_ERR_NVS_INVALID_LENGTH;
    if(!(h==1 ? has_config : has_legacy))return ESP_ERR_NVS_NOT_FOUND;
    unsigned n=h==1 ? sizeof(config) : sizeof(legacy);assert(*size==n);
    memcpy(b,h==1 ? config : legacy,n);*size=n;return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *b,size_t size){
    assert(h==1 && !strcmp(key,"config") && size==sizeof(config));++writes;
    if(fail_write)return ESP_FAIL;memcpy(config,b,size);has_config=true;return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t h){assert(h==1);return fail_commit ? ESP_FAIL : ESP_OK;}
int main(void){
    app_preferences_t p;
    fail_init=true;assert(app_preferences_load(&p)==APP_PREF_UNAVAILABLE);fail_init=false;
    fail_open=true;assert(app_preferences_load(&p)==APP_PREF_UNAVAILABLE);fail_open=false;
    assert(app_preferences_load(&p)==APP_PREF_DEFAULT && !writes && closes==1);
    has_legacy=true;app_settings_encode(APP_ROTATION_90,legacy);
    assert(app_preferences_load(&p)==APP_PREF_MIGRATED && p.rotation==APP_ROTATION_90 && !writes);
    fail_write=true;assert(app_preferences_save(&p)==ESP_FAIL);fail_write=false;
    fail_commit=true;assert(app_preferences_save(&p)==ESP_FAIL);fail_commit=false;
    p.speech_enabled=false;assert(app_preferences_save(&p)==ESP_OK);
    assert(app_preferences_load(&p)==APP_PREF_LOADED && p.rotation==APP_ROTATION_90 && !p.speech_enabled);
    config[2]=99;assert(app_preferences_load(&p)==APP_PREF_INVALID && p.rotation==APP_ROTATION_270);
    oversize=true;assert(app_preferences_load(&p)==APP_PREF_INVALID);oversize=false;
    fail_read=true;assert(app_preferences_load(&p)==APP_PREF_UNAVAILABLE);
    puts("Global settings NVS legacy migration, isolated writes and read/write/commit failures: PASS");
}
