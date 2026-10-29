#include "ai/ai_config.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
    ai_config_t c;ai_config_default(&c);assert(ai_config_valid(&c));
    ai_config_request_t r;
    assert(ai_provider_index("doubao")==0 && ai_provider_index("openai")==1 && !ai_provider(2));
    assert(ai_provider(0)->available && !ai_provider(1)->available);
    assert(!ai_config_parse("provider=unknown&action=save&key=test&model=ep-test",&r));
    assert(!ai_config_parse("provider=doubao&provider=openai&action=save&key=test&model=ep-test",&r));
    assert(!ai_config_parse("provider=doubao&service=asr&action=save&key=test&model=ep-test",&r));
    assert(!ai_config_parse("provider=doubao&action=save&key=test&model=ep-test&service=ark",&r));
    assert(!ai_config_parse("provider=doubao&action=save&key=a%00b&model=ep-test",&r));
    assert(!ai_config_parse("provider=doubao&action=save&key=a+b&model=ep-test",&r));
    assert(ai_config_parse("provider=doubao&action=save&key=test%2B%2F%3D&model=ep-test",&r));
    assert(ai_config_apply(&c,&r) && c.active==0 && !strcmp(c.keys[0],"test+/=") && !strcmp(c.ark_model,"ep-test"));
    assert(!ai_config_parse("provider=openai&action=save&key=other&model=ep-test",&r));
    assert(ai_config_parse("provider=doubao&action=save&key=&model=other-model",&r));
    assert(ai_config_apply(&c,&r) && !strcmp(c.keys[0],"test+/=") && !strcmp(c.ark_model,"other-model"));
    assert(!ai_config_parse("provider=doubao&action=save&key=test&model=bad+model",&r));
    assert(!ai_config_parse("provider=doubao&action=save&key=test&model=one&model=two",&r));
    assert(ai_config_parse("provider=doubao&action=remove",&r));
    assert(ai_config_apply(&c,&r) && c.active==AI_PROVIDER_NONE && !*c.keys[0]);
    assert(ai_config_parse("provider=doubao&action=save&key=&model=ep-test",&r) && !ai_config_apply(&c,&r));
    strcpy(r.model,"ep-test");memset(r.key,'a',256);r.key[256]=0;assert(ai_config_apply(&c,&r));
    memset(r.key,'a',sizeof(r.key));assert(!ai_config_apply(&c,&r));
    r.provider=1;assert(!ai_config_apply(&c,&r));
    puts("Ark-only config, retired request rejection, model validation and bounds: PASS");
}
