#include "ai/ai_config.h"
#include "nvs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned char disk[4+AI_PROVIDER_COUNT*AI_KEY_CAPACITY];
static bool stored,fail_write,fail_commit,bad_record;
esp_err_t nvs_flash_init(void){return ESP_OK;}
esp_err_t nvs_open(const char *name,int mode,nvs_handle_t *out){assert(!strcmp(name,"ai_voice") && mode==NVS_READWRITE);*out=1;return ESP_OK;}
esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *out,size_t *size){
    assert(h==1 && !strcmp(key,"models"));if(!stored)return ESP_ERR_NVS_NOT_FOUND;
    assert(*size==sizeof(disk));memcpy(out,disk,sizeof(disk));*size=sizeof(disk);if(bad_record)((char *)out)[2]=99;return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *data,size_t size){
    assert(h==1 && !strcmp(key,"models") && size==sizeof(disk));if(fail_write)return ESP_FAIL;
    if(!fail_commit){memcpy(disk,data,size);stored=true;}return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t h){assert(h==1);return fail_commit ? ESP_FAIL : ESP_OK;}
static ai_config_snapshot_t snapshot(void){ai_config_snapshot_t s;ai_config_snapshot(&s);return s;}
int main(void){
    ai_config_service_init();assert(snapshot().ready && snapshot().active==AI_PROVIDER_NONE);
    ai_config_request_t r={.provider=0,.action=AI_CONFIG_SAVE,.key="test-only"};unsigned id;
    assert(ai_config_submit(&r,&id) && snapshot().busy && !stored);
    assert(!ai_config_submit(&r,NULL));ai_config_service_poll();
    assert(snapshot().completed==id && !snapshot().busy && snapshot().configured[0]);
    ai_config_service_init();assert(snapshot().active==0 && snapshot().configured[0]);
    r.provider=1;fail_write=true;assert(ai_config_submit(&r,&id));ai_config_service_poll();
    assert(snapshot().error && snapshot().active==0 && !snapshot().configured[1]);fail_write=false;
    fail_commit=true;assert(ai_config_submit(&r,&id));ai_config_service_poll();assert(snapshot().error && !snapshot().configured[1]);fail_commit=false;
    assert(ai_config_submit(&r,&id));ai_config_service_poll();assert(snapshot().active==1 && snapshot().configured[0]);
    r.action=AI_CONFIG_REMOVE;assert(ai_config_submit(&r,&id));ai_config_service_poll();assert(snapshot().active==0 && !snapshot().configured[1]);
    bad_record=true;ai_config_service_init();assert(snapshot().error && snapshot().active==AI_PROVIDER_NONE);
    puts("AI config deferred NVS writes, restart, busy, write/commit failure and corrupt record: PASS");
}
