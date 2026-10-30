#include "ai/ai_config.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
 ai_config_t c;ai_config_default(&c);assert(ai_config_valid(&c)&&c.active==AI_PROVIDER_NONE&&!*c.entries[0].model&&!*c.entries[0].url&&!*c.entries[0].key);
 ai_config_request_t r;
 const char *form="slot=0&name=Test&action=save&key=test%2B%2F%3D&model=audio-model&url=https%3A%2F%2Fexample.com%2Fv1%2Fchat%2Fcompletions";
 assert(ai_config_parse(form,&r)&&ai_config_apply(&c,&r));assert(c.active==0&&!strcmp(c.entries[0].key,"test+/=")&&!strcmp(c.entries[0].model,"audio-model"));
 assert(ai_config_parse("slot=0&name=Test&action=save&key=&model=other-model&url=https%3A%2F%2Fexample.com%2Fv1%2Fchat%2Fcompletions",&r));
 assert(ai_config_apply(&c,&r)&&!strcmp(c.entries[0].key,"test+/=")&&!strcmp(c.entries[0].model,"other-model"));
 const char *bad[]={"slot=0&name=Test&action=save&key=a%00b&model=x&url=https://host/api","slot=0&name=Test&action=save&key=a+b&model=x&url=https://host/api","slot=0&name=Test&action=save&key=x&model=x&model=y&url=https://host/api","slot=0&name=Test&action=save&key=x&model=has+space&url=https://host/api","slot=0&name=Test&action=save&key=x&model=x&url=https://host/api&url=https://evil/api","slot=0&name=Test&action=save&key=x&model=x"};
 for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);++i)assert(!ai_config_parse(bad[i],&r));
 assert(!ai_config_url_valid("ftp://host/api")&&!ai_config_url_valid("https://host")&&!ai_config_url_valid("https://key@host/api")&&!ai_config_url_valid("https://host/api#fragment")&&!ai_config_url_valid("https://host/api?secret=x"));
 assert(ai_config_url_valid("http://192.168.1.2:8000/v1/chat/completions"));
 strcpy(r.model,"vendor/audio-model:v2");assert(ai_config_apply(&c,&r));
 assert(ai_config_parse("slot=0&name=Test&action=remove",&r)&&ai_config_apply(&c,&r)&&!*c.entries[0].key&&c.active==AI_PROVIDER_NONE);
 assert(ai_config_parse("slot=0&name=Test&action=save&key=&model=x&url=https://host/api",&r)&&!ai_config_apply(&c,&r));
 memset(r.key,'a',256);r.key[256]=0;assert(ai_config_apply(&c,&r));memset(r.key,'a',sizeof(r.key));assert(!ai_config_apply(&c,&r));
 assert(ai_config_name_valid("便宜模型")&&!ai_config_name_valid(" leading")&&!ai_config_name_valid("😀")&&!ai_config_name_valid("\xe0\x81\xa1"));
 r=(ai_config_request_t){.provider=1,.action=AI_CONFIG_SAVE,.name="备用模型",.key="second-key",.model="model2",.url="https://second.example/api"};
 assert(ai_config_apply(&c,&r)&&c.active==1&&strlen(c.entries[0].key)==256&&!strcmp(c.entries[1].key,"second-key"));
 r.action=AI_CONFIG_SELECT;r.provider=0;assert(ai_config_apply(&c,&r)&&c.active==0);
 r.action=AI_CONFIG_REMOVE;r.provider=1;assert(ai_config_apply(&c,&r)&&c.active==0&&!*c.entries[1].name);
 r.provider=AI_PROVIDER_COUNT;assert(!ai_config_apply(&c,&r));
 assert(ai_config_name_valid("一二三四五六七八九十甲乙")&&!ai_config_name_valid("一二三四五六七八九十甲乙丙"));
 assert(!ai_config_parse("action=select&slot=4",&r)&&!ai_config_parse("action=select&slot=0&slot=1",&r));
 puts("Unified model configuration: empty defaults, URL/model/key bounds, save/remove/keep-Key and duplicate rejection PASS");
}
