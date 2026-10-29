#include "ai/ai_config.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
    ai_config_t c;ai_config_default(&c);assert(ai_config_valid(&c));
    ai_config_request_t r;
    assert(ai_provider_index("doubao")==0 && ai_provider_index("openai")==1 && !ai_provider(2));
    assert(!ai_config_parse("provider=unknown&action=save&key=test",&r));
    assert(!ai_config_parse("provider=doubao&provider=openai&action=save&key=test",&r));
    assert(!ai_config_parse("provider=doubao&action=save&key=",&r));
    assert(!ai_config_parse("provider=doubao&action=save&key=a%00b",&r));
    assert(!ai_config_parse("provider=doubao&action=save&key=a+b",&r));
    assert(ai_config_parse("provider=doubao&action=save&key=test%2B%2F%3D",&r));
    assert(ai_config_apply(&c,&r) && c.active==0 && !strcmp(c.keys[0],"test+/="));
    assert(ai_config_parse("provider=openai&action=select",&r) && !ai_config_apply(&c,&r));
    assert(ai_config_parse("provider=openai&action=save&key=other",&r) && ai_config_apply(&c,&r));
    assert(c.active==1 && *c.keys[0] && *c.keys[1]);
    assert(ai_config_parse("provider=doubao&action=select",&r) && ai_config_apply(&c,&r) && c.active==0);
    assert(ai_config_parse("provider=doubao&action=remove",&r) && ai_config_apply(&c,&r) && c.active==1 && !*c.keys[0]);
    assert(ai_config_parse("provider=openai&action=remove",&r) && ai_config_apply(&c,&r) && c.active==AI_PROVIDER_NONE);
    r=(ai_config_request_t){.action=AI_CONFIG_SAVE,.provider=0};memset(r.key,'a',256);r.key[256]=0;
    assert(ai_config_apply(&c,&r));memset(r.key,'a',sizeof(r.key));assert(!ai_config_apply(&c,&r));
    puts("AI provider parsing, independent keys, activation, removal and bounds: PASS");
}
