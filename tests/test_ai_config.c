#include "ai/ai_config.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
 ai_config_t c;ai_config_default(&c);assert(ai_config_valid(&c)&&c.active==AI_PROVIDER_NONE&&!*c.model&&!*c.url&&!*c.key);
 ai_config_request_t r;
 const char *form="action=save&key=test%2B%2F%3D&model=audio-model&url=https%3A%2F%2Fexample.com%2Fv1%2Fchat%2Fcompletions";
 assert(ai_config_parse(form,&r)&&ai_config_apply(&c,&r));assert(c.active==0&&!strcmp(c.key,"test+/=")&&!strcmp(c.model,"audio-model"));
 assert(ai_config_parse("action=save&key=&model=other-model&url=https%3A%2F%2Fexample.com%2Fv1%2Fchat%2Fcompletions",&r));
 assert(ai_config_apply(&c,&r)&&!strcmp(c.key,"test+/=")&&!strcmp(c.model,"other-model"));
 const char *bad[]={"action=save&key=a%00b&model=x&url=https://host/api","action=save&key=a+b&model=x&url=https://host/api","action=save&key=x&model=x&model=y&url=https://host/api","action=save&key=x&model=has+space&url=https://host/api","action=save&key=x&model=x&url=https://host/api&url=https://evil/api","action=save&key=x&model=x"};
 for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);++i)assert(!ai_config_parse(bad[i],&r));
 assert(!ai_config_url_valid("ftp://host/api")&&!ai_config_url_valid("https://host")&&!ai_config_url_valid("https://key@host/api")&&!ai_config_url_valid("https://host/api#fragment")&&!ai_config_url_valid("https://host/api?secret=x"));
 assert(ai_config_url_valid("http://192.168.1.2:8000/v1/chat/completions"));
 strcpy(r.model,"vendor/audio-model:v2");assert(ai_config_apply(&c,&r));
 assert(ai_config_parse("action=remove",&r)&&ai_config_apply(&c,&r)&&!*c.key&&c.active==AI_PROVIDER_NONE);
 assert(ai_config_parse("action=save&key=&model=x&url=https://host/api",&r)&&!ai_config_apply(&c,&r));
 memset(r.key,'a',256);r.key[256]=0;assert(ai_config_apply(&c,&r));memset(r.key,'a',sizeof(r.key));assert(!ai_config_apply(&c,&r));
 puts("Unified model configuration: empty defaults, URL/model/key bounds, save/remove/keep-Key and duplicate rejection PASS");
}
