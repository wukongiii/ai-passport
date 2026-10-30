#include "ai_config.h"
#include "../app/app_strings.h"
#include "../network/app_network_model.h"
#include <string.h>

static const ai_provider_t provider={"compatible"};
const ai_provider_t *ai_provider(unsigned i){return i==0?&provider:NULL;}
int ai_provider_index(const char *id){return id&&!strcmp(id,provider.id)?0:-1;}
void ai_config_default(ai_config_t *c){memset(c,0,sizeof(*c));c->active=AI_PROVIDER_NONE;}
static bool ascii_valid(const char *value,size_t capacity,bool empty){
    size_t n=0;while(n<capacity && value[n])++n;
    if(n==capacity || (!empty && !n))return false;
    for(size_t i=0;i<n;++i){unsigned char ch=(unsigned char)value[i];
        if(ch<33 || ch>126)return false;
    }
    return true;
}
bool ai_config_url_valid(const char *s){
    if(!s||!ascii_valid(s,AI_URL_CAPACITY,false))return false;
    const char *host=!strncmp(s,"https://",8)?s+8:!strncmp(s,"http://",7)?s+7:NULL;
    if(!host||!*host||strchr(host,'@')||strchr(host,'#')||strchr(host,'?')||strchr(host,'\\'))return false;
    const char *slash=strchr(host,'/');if(!slash||slash==host||!slash[1])return false;
    for(const char *p=host;p<slash;++p)if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||strchr(".-:[]",*p)))return false;
    return true;
}
static bool configured(const ai_config_t *c){return *c->key&&*c->model&&ai_config_url_valid(c->url);}
bool ai_config_valid(const ai_config_t *c){
 if(!c||!ascii_valid(c->key,AI_KEY_CAPACITY,true)||!ascii_valid(c->model,AI_MODEL_CAPACITY,true)||!memchr(c->url,0,AI_URL_CAPACITY)||(*c->url&&!ai_config_url_valid(c->url)))return false;
 return c->active==AI_PROVIDER_NONE || (c->active==0&&configured(c));
}
bool ai_config_parse(const char *form,ai_config_request_t *out){
    char action[16];ai_config_request_t r={0};
    if(!out || !app_net_form_value(form,"action",action,sizeof(action)))return false;
    bool ok=true;
    if(!strcmp(action,"save")){
        r.action=AI_CONFIG_SAVE;ok=app_net_form_value(form,"key",r.key,sizeof(r.key)) && ascii_valid(r.key,sizeof(r.key),true);
        ok=ok && app_net_form_value(form,"model",r.model,sizeof(r.model)) && ascii_valid(r.model,sizeof(r.model),false);
        ok=ok && app_net_form_value(form,"url",r.url,sizeof(r.url)) && ai_config_url_valid(r.url);
    }else if(!strcmp(action,"select"))r.action=AI_CONFIG_SELECT;
    else if(!strcmp(action,"remove"))r.action=AI_CONFIG_REMOVE;
    else ok=false;
    if(ok)*out=r;
    memset(&r,0,sizeof(r));return ok;
}
bool ai_config_apply(ai_config_t *c,const ai_config_request_t *r){
    if(!ai_config_valid(c) || !r || r->provider>=AI_PROVIDER_COUNT)return false;
    unsigned i=r->provider;char *key=c->key;
    switch(r->action){
    case AI_CONFIG_SAVE:
        if(!ascii_valid(r->key,sizeof(r->key),true) ||
           (!*r->key && !*key) ||
           !ascii_valid(r->model,sizeof(r->model),false)||!ai_config_url_valid(r->url))return false;
        if(*r->key){memset(key,0,AI_KEY_CAPACITY);strcpy(key,r->key);}
        char *model=c->model;
        memset(model,0,AI_MODEL_CAPACITY);strcpy(model,r->model);strcpy(c->url,r->url);
        c->active=(uint8_t)i;break;
    case AI_CONFIG_SELECT:if(!configured(c))return false;c->active=(uint8_t)i;break;
    case AI_CONFIG_REMOVE:
        memset(key,0,AI_KEY_CAPACITY);
        if(c->active==i && !configured(c))c->active=AI_PROVIDER_NONE;
        break;
    default:return false;
    }
    return true;
}

const char *ai_check_label(ai_check_state_t s){
 static const char *const labels[]={TXT_AI_CHECK_NONE,TXT_AI_CHECK_QUEUE,TXT_AI_CHECK_TEXT,TXT_AI_CHECK_TOOLS,TXT_AI_CHECK_AUDIO,TXT_AI_CHECK_PASS,TXT_AI_CHECK_FAIL,TXT_AI_CHECK_STALE};
 return (unsigned)s<sizeof(labels)/sizeof(*labels)?labels[s]:TXT_AI_CHECK_FAIL;
}

const char *ai_check_error_label(int e){
 if(!e)return "";
 if(e==401||e==403)return TXT_AI_CHECK_AUTH;
 if(e==404)return TXT_AI_CHECK_ROUTE;
 if(e==429)return TXT_AI_CHECK_LIMIT;
 if(e==0x108)return TXT_AI_CHECK_RESPONSE;
 if(e==0x101)return TXT_AI_CHECK_MEMORY;
 if(e==0x103)return TXT_AI_CHECK_NETWORK;
 if(e==0x107)return TXT_AI_CHECK_TIMEOUT;
 return TXT_AI_CHECK_REMOTE;
}
