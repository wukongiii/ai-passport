#include "ai/ai_persona.h"
#include "network/app_network_model.h"
#include "nvs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned char disk[2048];static size_t disk_size;
static bool stored,fail_write,fail_commit;
esp_err_t nvs_open(const char *name,int mode,nvs_handle_t *out){assert(!strcmp(name,"ai_persona")&&mode==NVS_READWRITE);*out=1;return ESP_OK;}
void nvs_close(nvs_handle_t h){assert(h==1);}
esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *out,size_t *size){assert(h==1&&!strcmp(key,"persona"));if(!stored)return ESP_ERR_NVS_NOT_FOUND;assert(*size>=disk_size);memcpy(out,disk,disk_size);*size=disk_size;return ESP_OK;}
esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *data,size_t size){assert(h==1&&!strcmp(key,"persona")&&size<=sizeof(disk));if(fail_write)return ESP_FAIL;if(!fail_commit){memcpy(disk,data,size);disk_size=size;stored=true;}return ESP_OK;}
esp_err_t nvs_commit(nvs_handle_t h){assert(h==1);return fail_commit?ESP_FAIL:ESP_OK;}
static ai_persona_snapshot_t snapshot(void){ai_persona_snapshot_t out;ai_persona_snapshot(&out);return out;}
int main(void){
 ai_persona_t p;ai_persona_default(&p);assert(ai_persona_valid(&p)&&!strcmp(p.name,"逗宝"));
 char decoded[64];assert(app_net_form_value("prompt=line1%0Aline2","prompt",decoded,sizeof(decoded))&&!strcmp(decoded,"line1\nline2"));
 assert(!app_net_form_value("name=line1%0Aline2","name",decoded,sizeof(decoded)));
 strcpy(p.prompt,"\xc0\x80");assert(!ai_persona_valid(&p));strcpy(p.prompt," \n\t");assert(!ai_persona_valid(&p));
 memset(p.prompt,'x',sizeof(p.prompt));assert(!ai_persona_valid(&p));p.prompt[sizeof(p.prompt)-1]=0;assert(ai_persona_valid(&p));
 ai_persona_init();assert(snapshot().ready&&!strcmp(snapshot().value.name,"逗宝"));
 unsigned revision=ai_persona_revision(),id;strcpy(p.name,"小宝");strcpy(p.prompt,"请使用短句。\n按指定数值记分。");
 assert(!ai_persona_submit(&p,revision+1,NULL));assert(ai_persona_submit(&p,revision,&id));assert(!ai_persona_submit(&p,revision,NULL));
 ai_persona_poll();assert(snapshot().completed==id&&!snapshot().busy&&!snapshot().error&&snapshot().revision>revision);
 ai_persona_init();assert(!strcmp(snapshot().value.name,"小宝")&&!strcmp(snapshot().value.prompt,p.prompt));
 revision=ai_persona_revision();strcpy(p.name,"改变");fail_write=true;assert(ai_persona_submit(&p,revision,NULL));ai_persona_poll();assert(snapshot().error&&!strcmp(snapshot().value.name,"小宝"));fail_write=false;
 fail_commit=true;assert(ai_persona_submit(&p,revision,NULL));ai_persona_poll();assert(snapshot().error&&!strcmp(snapshot().value.name,"小宝"));fail_commit=false;
 disk[2]=99;ai_persona_init();assert(snapshot().error&&!strcmp(snapshot().value.name,"逗宝"));
 puts("AI persona defaults, UTF-8/multiline, bounds, independent NVS, stale/busy, restart and write failures PASS");
}
