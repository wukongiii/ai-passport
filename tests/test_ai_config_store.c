#include "ai/ai_config.h"
#include "nvs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum {LEGACY=4+AI_PROVIDER_COUNT*AI_KEY_CAPACITY,V2=LEGACY+AI_KEY_CAPACITY+AI_MODEL_CAPACITY,SIZE=LEGACY+AI_MODEL_CAPACITY};
static unsigned char disk[V2];static size_t disk_size;
static bool stored,fail_write,fail_commit,bad_record;
esp_err_t nvs_flash_init(void){return ESP_OK;}
esp_err_t nvs_open(const char *name,int mode,nvs_handle_t *out){assert(!strcmp(name,"ai_voice") && mode==NVS_READWRITE);*out=1;return ESP_OK;}
void nvs_close(nvs_handle_t h){assert(h==1);}
esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *out,size_t *size){
    assert(h==1 && !strcmp(key,"models"));if(!stored)return ESP_ERR_NVS_NOT_FOUND;
    assert(*size>=disk_size);memcpy(out,disk,disk_size);*size=disk_size;if(bad_record)((char *)out)[2]=99;return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *data,size_t size){
    assert(h==1 && !strcmp(key,"models") && size==SIZE);if(fail_write)return ESP_FAIL;
    if(!fail_commit){memcpy(disk,data,size);disk_size=size;stored=true;}return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t h){assert(h==1);return fail_commit ? ESP_FAIL : ESP_OK;}
static ai_config_snapshot_t snapshot(void){ai_config_snapshot_t s;ai_config_snapshot(&s);return s;}
int main(void){
    ai_config_service_init();assert(snapshot().ready && snapshot().active==AI_PROVIDER_NONE);
    /* Version 1 credentials must not be mistaken for an Ark Key. */
    disk[0]='A';disk[1]='I';disk[2]=1;disk[3]=0;
    strcpy((char *)disk+4,"retired-key");strcpy((char *)disk+4+AI_KEY_CAPACITY,"legacy-other");
    disk_size=LEGACY;stored=true;ai_config_service_init();
    assert(snapshot().active==AI_PROVIDER_NONE && !snapshot().configured[0] && snapshot().configured[1]);
    /* Version 2 keeps Ark credentials/model and discards standalone credentials. */
    disk[2]=2;strcpy((char *)disk+LEGACY,"ark-test");
    strcpy((char *)disk+LEGACY+AI_KEY_CAPACITY,"ep-old");disk_size=V2;
    ai_config_service_init();assert(snapshot().active==0 && snapshot().configured[0] && !strcmp(snapshot().ark_model,"ep-old"));
    ai_config_request_t r={.provider=0,.action=AI_CONFIG_SAVE,.key="ark-new",.model="ep-test"};unsigned id;
    assert(ai_config_submit(&r,&id) && snapshot().busy && disk_size==V2);
    assert(!ai_config_submit(&r,NULL));ai_config_service_poll();
    assert(snapshot().completed==id && !snapshot().busy && snapshot().configured[0]);
    assert(disk[2]==3 && disk_size==SIZE && !strcmp((char *)disk+4,"ark-new"));
    assert(!strcmp((char *)disk+4+AI_KEY_CAPACITY,"legacy-other"));
    ai_config_service_init();assert(snapshot().active==0 && !strcmp(snapshot().ark_model,"ep-test"));
    strcpy(r.model,"ep-new");fail_write=true;assert(ai_config_submit(&r,&id));ai_config_service_poll();
    assert(snapshot().error && !strcmp(snapshot().ark_model,"ep-test"));fail_write=false;
    fail_commit=true;assert(ai_config_submit(&r,&id));ai_config_service_poll();assert(snapshot().error && !strcmp(snapshot().ark_model,"ep-test"));fail_commit=false;
    assert(ai_config_submit(&r,&id));ai_config_service_poll();assert(!strcmp(snapshot().ark_model,"ep-new"));
    r.action=AI_CONFIG_REMOVE;assert(ai_config_submit(&r,&id));ai_config_service_poll();
    assert(snapshot().active==AI_PROVIDER_NONE && !snapshot().configured[0] && snapshot().configured[1]);
    r.provider=1;assert(!ai_config_submit(&r,NULL));
    bad_record=true;ai_config_service_init();assert(snapshot().error && snapshot().active==AI_PROVIDER_NONE);
    puts("AI v1/v2 migration to Ark-only v3, restart and durable write failures: PASS");
}
