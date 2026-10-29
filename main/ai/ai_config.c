#include "ai_config.h"
#include "../app/app_strings.h"
#include "../network/app_network_model.h"
#include <string.h>

static const ai_provider_t providers[AI_PROVIDER_COUNT]={
    {"doubao",TXT_AI_DOUBAO,"Volcano Ark","https://console.volcengine.com/ark/",true},
    {"openai","OpenAI","Realtime","https://platform.openai.com/api-keys",false}
};
const ai_provider_t *ai_provider(unsigned i){return i<AI_PROVIDER_COUNT ? &providers[i] : NULL;}
int ai_provider_index(const char *id){
    if(id)for(unsigned i=0;i<AI_PROVIDER_COUNT;++i)if(!strcmp(id,providers[i].id))return (int)i;
    return -1;
}
void ai_config_default(ai_config_t *c){memset(c,0,sizeof(*c));c->active=AI_PROVIDER_NONE;strcpy(c->ark_model,AI_DOUBAO_DEFAULT_MODEL);}
static bool ascii_valid(const char *value,size_t capacity,bool empty,bool model){
    size_t n=0;while(n<capacity && value[n])++n;
    if(n==capacity || (!empty && !n))return false;
    for(size_t i=0;i<n;++i){unsigned char ch=(unsigned char)value[i];
        if(model){if(!((ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z')||(ch>='0'&&ch<='9')||ch=='-'||ch=='_'||ch=='.'))return false;}
        else if(ch<33 || ch>126)return false;
    }
    return true;
}
static bool configured(const ai_config_t *c,unsigned i){return *c->keys[i]!=0;}
bool ai_config_valid(const ai_config_t *c){
    if(!c)return false;
    for(unsigned i=0;i<AI_PROVIDER_COUNT;++i)if(!ascii_valid(c->keys[i],AI_KEY_CAPACITY,true,false))return false;
    if(!ascii_valid(c->ark_model,AI_MODEL_CAPACITY,false,true))return false;
    return c->active==AI_PROVIDER_NONE || (c->active<AI_PROVIDER_COUNT && configured(c,c->active));
}
bool ai_config_parse(const char *form,ai_config_request_t *out){
    char provider[24],action[16];ai_config_request_t r={0};
    if(!out || !app_net_form_value(form,"provider",provider,sizeof(provider)) ||
       !app_net_form_value(form,"action",action,sizeof(action)))return false;
    int index=ai_provider_index(provider);if(index<0 || !providers[index].available)return false;r.provider=(unsigned)index;
    /* Retired standalone-service requests must never overwrite the Ark Key. */
    if(!strncmp(form,"service=",8) || strstr(form,"&service="))return false;
    bool ok=true;
    if(!strcmp(action,"save")){
        r.action=AI_CONFIG_SAVE;ok=app_net_form_value(form,"key",r.key,sizeof(r.key)) && ascii_valid(r.key,sizeof(r.key),true,false);
        ok=ok && app_net_form_value(form,"model",r.model,sizeof(r.model)) && ascii_valid(r.model,sizeof(r.model),false,true);
    }else if(!strcmp(action,"select"))r.action=AI_CONFIG_SELECT;
    else if(!strcmp(action,"remove"))r.action=AI_CONFIG_REMOVE;
    else ok=false;
    if(ok)*out=r;
    memset(&r,0,sizeof(r));return ok;
}
bool ai_config_apply(ai_config_t *c,const ai_config_request_t *r){
    if(!ai_config_valid(c) || !r || r->provider>=AI_PROVIDER_COUNT || !providers[r->provider].available)return false;
    unsigned i=r->provider;char *key=c->keys[i];
    switch(r->action){
    case AI_CONFIG_SAVE:
        if(!ascii_valid(r->key,sizeof(r->key),true,false) ||
           (!*r->key && !*key) ||
           !ascii_valid(r->model,sizeof(r->model),false,true))return false;
        if(*r->key){memset(key,0,AI_KEY_CAPACITY);strcpy(key,r->key);}
        memset(c->ark_model,0,sizeof(c->ark_model));strcpy(c->ark_model,r->model);
        c->active=(uint8_t)i;break;
    case AI_CONFIG_SELECT:if(!configured(c,i))return false;c->active=(uint8_t)i;break;
    case AI_CONFIG_REMOVE:
        memset(key,0,AI_KEY_CAPACITY);
        if(c->active==i && !configured(c,i))c->active=AI_PROVIDER_NONE;
        break;
    default:return false;
    }
    return true;
}
