#include "ai_config.h"
#include "../app/app_strings.h"
#include "../network/app_network_model.h"
#include <string.h>

static const ai_provider_t providers[AI_PROVIDER_COUNT]={
    {"doubao",TXT_AI_DOUBAO,"Seeduplex 3.0","https://console.volcengine.com/speech/app"},
    {"openai","OpenAI","Realtime","https://platform.openai.com/api-keys"}
};
const ai_provider_t *ai_provider(unsigned i){return i<AI_PROVIDER_COUNT ? &providers[i] : NULL;}
int ai_provider_index(const char *id){
    if(id)for(unsigned i=0;i<AI_PROVIDER_COUNT;++i)if(!strcmp(id,providers[i].id))return (int)i;
    return -1;
}
void ai_config_default(ai_config_t *c){memset(c,0,sizeof(*c));c->active=AI_PROVIDER_NONE;}
static bool key_valid(const char *key,bool empty){
    const char *end=memchr(key,0,AI_KEY_CAPACITY);if(!end || (!empty && end==key))return false;
    for(const char *p=key;p<end;++p)if((unsigned char)*p<33 || (unsigned char)*p>126)return false;
    return true;
}
bool ai_config_valid(const ai_config_t *c){
    if(!c)return false;
    bool any=false;
    for(unsigned i=0;i<AI_PROVIDER_COUNT;++i){if(!key_valid(c->keys[i],true))return false;any|=*c->keys[i]!=0;}
    return any ? c->active<AI_PROVIDER_COUNT && *c->keys[c->active] : c->active==AI_PROVIDER_NONE;
}
bool ai_config_parse(const char *form,ai_config_request_t *out){
    char provider[24],action[16];ai_config_request_t r={0};
    if(!app_net_form_value(form,"provider",provider,sizeof(provider)) ||
       !app_net_form_value(form,"action",action,sizeof(action)))return false;
    int index=ai_provider_index(provider);if(index<0)return false;r.provider=(unsigned)index;
    bool ok=true;
    if(!strcmp(action,"save")){
        r.action=AI_CONFIG_SAVE;ok=app_net_form_value(form,"key",r.key,sizeof(r.key)) && key_valid(r.key,false);
    }else if(!strcmp(action,"select"))r.action=AI_CONFIG_SELECT;
    else if(!strcmp(action,"remove"))r.action=AI_CONFIG_REMOVE;
    else ok=false;
    if(ok)*out=r;
    memset(&r,0,sizeof(r));return ok;
}
bool ai_config_apply(ai_config_t *c,const ai_config_request_t *r){
    if(!ai_config_valid(c) || !r || r->provider>=AI_PROVIDER_COUNT)return false;
    unsigned i=r->provider;
    switch(r->action){
    case AI_CONFIG_SAVE:
        if(!key_valid(r->key,false))return false;
        memset(c->keys[i],0,sizeof(c->keys[i]));strcpy(c->keys[i],r->key);c->active=(uint8_t)i;break;
    case AI_CONFIG_SELECT:if(!*c->keys[i])return false;c->active=(uint8_t)i;break;
    case AI_CONFIG_REMOVE:
        memset(c->keys[i],0,sizeof(c->keys[i]));
        if(c->active==i){c->active=AI_PROVIDER_NONE;for(unsigned j=0;j<AI_PROVIDER_COUNT;++j)if(*c->keys[j]){c->active=(uint8_t)j;break;}}
        break;
    default:return false;
    }
    return true;
}
