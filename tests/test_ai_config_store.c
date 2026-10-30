#include "ai/ai_config.h"
#include "nvs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum {SIZE=4+AI_PROVIDER_COUNT*(AI_NAME_CAPACITY+AI_KEY_CAPACITY+AI_MODEL_CAPACITY+AI_URL_CAPACITY)};
static unsigned char disk[SIZE];static size_t disk_size;
static bool stored,fail_write,fail_commit,bad_record;
esp_err_t nvs_flash_init(void){return ESP_OK;}
esp_err_t nvs_open(const char *name,int mode,nvs_handle_t *out){assert(!strcmp(name,"ai_voice")&&mode==NVS_READWRITE);*out=1;return ESP_OK;}
void nvs_close(nvs_handle_t h){assert(h==1);}
esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *out,size_t *size){
 assert(h==1&&!strcmp(key,"profiles"));if(!stored)return ESP_ERR_NVS_NOT_FOUND;
 assert(*size>=disk_size);memcpy(out,disk,disk_size);*size=disk_size;if(bad_record)((char *)out)[2]=99;return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *data,size_t size){
 assert(h==1&&!strcmp(key,"profiles")&&size==SIZE);if(fail_write)return ESP_FAIL;
 if(!fail_commit){memcpy(disk,data,size);disk_size=size;stored=true;}return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t h){assert(h==1);return fail_commit?ESP_FAIL:ESP_OK;}
static ai_config_snapshot_t snapshot(void){ai_config_snapshot_t s;ai_config_snapshot(&s);return s;}
int main(void){
 ai_config_service_init();assert(snapshot().ready&&snapshot().active==AI_PROVIDER_NONE&&!*snapshot().entries[0].model);
 ai_config_request_t r={.action=AI_CONFIG_SAVE,.name="Primary",.key="test-only-key",.model="test-model",.url="https://example.com/v1/chat/completions"};unsigned id;
 assert(ai_config_submit(&r,&id)&&snapshot().busy);assert(!ai_config_submit(&r,NULL));ai_config_service_poll();
 assert(snapshot().completed==id&&!snapshot().busy&&snapshot().configured[0]&&disk[2]==1);
 ai_config_service_init();assert(snapshot().active==0&&!strcmp(snapshot().entries[0].model,"test-model")&&!strcmp(snapshot().entries[0].url,r.url));
 char key[AI_KEY_CAPACITY],model[AI_MODEL_CAPACITY],url[AI_URL_CAPACITY];
 assert(ai_config_connection(0,key,model,url,NULL)&&!strcmp(key,r.key)&&!strcmp(model,r.model)&&!strcmp(url,r.url));
 assert(!ai_config_connection(1,key,model,url,NULL)&&!key[0]&&!model[0]&&!url[0]);
 strcpy(r.model,"new-model");fail_write=true;assert(ai_config_submit(&r,&id));ai_config_service_poll();assert(snapshot().error&&!strcmp(snapshot().entries[0].model,"test-model"));fail_write=false;
 fail_commit=true;assert(ai_config_submit(&r,&id));ai_config_service_poll();assert(snapshot().error&&!strcmp(snapshot().entries[0].model,"test-model"));fail_commit=false;
 assert(ai_config_submit(&r,NULL));ai_config_service_poll();assert(!strcmp(snapshot().entries[0].model,"new-model"));
 assert(ai_check_request(0));unsigned provider,generation;assert(!ai_check_request(0));assert(ai_check_take(&provider,&generation)&&provider==0);
 ai_check_publish(0,generation,AI_CHECK_TOOLS,0);ai_check_snapshot_t check;ai_check_snapshot(0,&check);assert(check.text&&!check.tools);
 ai_check_publish(0,generation,AI_CHECK_AUDIO,0);ai_check_publish(0,generation,AI_CHECK_PASS,0);ai_check_snapshot(0,&check);assert(check.audio&&check.tools&&check.state==AI_CHECK_PASS);
 strcpy(r.model,"next-model");assert(ai_config_submit(&r,NULL));ai_config_service_poll();ai_check_snapshot(0,&check);assert(check.state==AI_CHECK_STALE&&!check.audio&&!check.text);
 assert(ai_check_request(0));assert(ai_check_take(&provider,&generation));ai_check_publish(provider,generation,AI_CHECK_FAILED,401);ai_check_snapshot(0,&check);assert(check.error==401&&check.state==AI_CHECK_FAILED);
 r.action=AI_CONFIG_REMOVE;assert(ai_config_submit(&r,&id));ai_config_service_poll();assert(snapshot().active==AI_PROVIDER_NONE&&!snapshot().configured[0]);ai_check_snapshot(0,&check);assert(check.state==AI_CHECK_STALE&&!check.error);
 r=(ai_config_request_t){.action=AI_CONFIG_SAVE,.provider=2,.name="备用模型",.key="other-key",.model="other-model",.url="https://other.example/api"};
 assert(ai_config_submit(&r,NULL));ai_config_service_poll();ai_config_service_init();
 ai_runtime_snapshot_t runtime;ai_config_runtime(&runtime);assert(runtime.ready&&!runtime.busy&&runtime.active==2&&runtime.configured[2]);
 assert(snapshot().active==2&&snapshot().configured[2]&&!snapshot().configured[0]&&!strcmp(snapshot().entries[2].name,"备用模型"));
 assert(ai_config_connection(2,key,model,url,NULL)&&!strcmp(key,"other-key")&&!strcmp(model,"other-model"));
 assert(ai_check_request(2));assert(ai_check_take(&provider,&generation));ai_check_publish(2,generation,AI_CHECK_PASS,0);
 r.provider=1;assert(ai_config_submit(&r,NULL));ai_config_service_poll();
 ai_check_snapshot(2,&check);assert(check.state==AI_CHECK_PASS&&check.audio);
 r.action=AI_CONFIG_SELECT;r.provider=2;assert(ai_config_submit(&r,NULL));ai_config_service_poll();ai_check_snapshot(2,&check);assert(check.state==AI_CHECK_PASS);
 assert(ai_check_request(1));assert(ai_check_take(&provider,&generation));
 r.action=AI_CONFIG_REMOVE;assert(ai_config_submit(&r,NULL));ai_config_service_poll();assert(snapshot().configured[1]&&!snapshot().configured[2]);
 ai_check_publish(1,generation,AI_CHECK_PASS,0);ai_check_snapshot(1,&check);assert(check.state==AI_CHECK_STALE&&!check.audio);
 assert(ai_check_request(1));assert(ai_check_take(&provider,&generation));ai_check_publish(1,generation,AI_CHECK_FAILED,401);
 bad_record=true;ai_config_service_init();assert(snapshot().error&&snapshot().active==AI_PROVIDER_NONE&&!*snapshot().entries[0].model);
 puts("Unified connection storage: restart, atomic failure, secret copy, capability generation invalidation PASS");
}
