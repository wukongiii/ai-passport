#include "ai_config.h"
#include "../app/app_error.h"
#include "../app/app_strings.h"
#include "../network/app_network_model.h"
#include <string.h>

static const ai_provider_t provider={"compatible"};
const ai_provider_t *ai_provider(unsigned i){return i<AI_PROVIDER_COUNT?&provider:NULL;}
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
bool ai_config_name_valid(const char *s){
 if(!s)return false;
 size_t n=0;while(n<AI_NAME_CAPACITY&&s[n])++n;
 if(!n||n==AI_NAME_CAPACITY||s[0]==' '||s[n-1]==' ')return false;
 unsigned count=0;
 for(size_t i=0;i<n;++count){
  unsigned char c=(unsigned char)s[i];uint32_t cp=c;unsigned bytes=1;
  if(c>=0xe0&&c<=0xef){
   if(n-i<3||((unsigned char)s[i+1]&0xc0)!=0x80||((unsigned char)s[i+2]&0xc0)!=0x80)return false;
   cp=((c&15)<<12)|(((unsigned char)s[i+1]&63)<<6)|((unsigned char)s[i+2]&63);bytes=3;
   if(cp<0x800||(cp>=0xd800&&cp<=0xdfff))return false;
  }else if(c>=128)return false;
  if(!((cp>=33&&cp<=126)||cp==32||(cp>=0x3000&&cp<=0x303f)||(cp>=0x4e00&&cp<=0x9fef)||(cp>=0xff01&&cp<=0xff60)))return false;
  if((i==0||i+bytes==n)&&cp==0x3000)return false;
  i+=bytes;
 }
 return count<=12;
}
bool ai_config_slot(const char *form,unsigned *slot){
 char value[4];
 if(!slot||!app_net_form_value(form,"slot",value,sizeof(value))||value[0]<'0'||value[0]>='0'+AI_PROVIDER_COUNT||value[1])return false;
 *slot=(unsigned)(value[0]-'0');return true;
}
static bool configured(const ai_connection_t *c){return *c->name&&*c->key&&*c->model&&ai_config_url_valid(c->url);}
bool ai_config_valid(const ai_config_t *c){
 if(!c)return false;
 for(unsigned i=0;i<AI_PROVIDER_COUNT;++i){const ai_connection_t *e=&c->entries[i];
  if(!ascii_valid(e->key,AI_KEY_CAPACITY,true)||!ascii_valid(e->model,AI_MODEL_CAPACITY,true)||!memchr(e->url,0,AI_URL_CAPACITY)||!memchr(e->name,0,AI_NAME_CAPACITY))return false;
  if(*e->name){if(!ai_config_name_valid(e->name)||!configured(e))return false;}
  else if(*e->key||*e->model||*e->url)return false;
 }
 return c->active==AI_PROVIDER_NONE||(c->active<AI_PROVIDER_COUNT&&configured(&c->entries[c->active]));
}
bool ai_config_parse(const char *form,ai_config_request_t *out){
    char action[16];ai_config_request_t r={0};
    if(!out || !app_net_form_value(form,"action",action,sizeof(action)) || !ai_config_slot(form,&r.provider))return false;
    bool ok=true;
    if(!strcmp(action,"save")){
        r.action=AI_CONFIG_SAVE;ok=app_net_form_value(form,"name",r.name,sizeof(r.name)) && ai_config_name_valid(r.name);
        ok=ok && app_net_form_value(form,"key",r.key,sizeof(r.key)) && ascii_valid(r.key,sizeof(r.key),true);
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
    unsigned i=r->provider;ai_connection_t *e=&c->entries[i];char *key=e->key;
    switch(r->action){
    case AI_CONFIG_SAVE:
        if(!ai_config_name_valid(r->name)||!ascii_valid(r->key,sizeof(r->key),true) ||
           (!*r->key && !*key) ||
           !ascii_valid(r->model,sizeof(r->model),false)||!ai_config_url_valid(r->url))return false;
        if(*r->key){memset(key,0,AI_KEY_CAPACITY);strcpy(key,r->key);}
        char *model=e->model;strcpy(e->name,r->name);
        memset(model,0,AI_MODEL_CAPACITY);strcpy(model,r->model);strcpy(e->url,r->url);
        c->active=(uint8_t)i;break;
    case AI_CONFIG_SELECT:if(!configured(e))return false;c->active=(uint8_t)i;break;
    case AI_CONFIG_REMOVE:
        memset(e,0,sizeof(*e));
        if(c->active==i)c->active=AI_PROVIDER_NONE;
        break;
    default:return false;
    }
    return true;
}

const char *ai_check_label(ai_check_state_t s){
 static const char *const labels[]={TXT_AI_CHECK_NONE,TXT_AI_CHECK_QUEUE,TXT_AI_CHECK_TEXT,TXT_AI_CHECK_TOOLS,TXT_AI_CHECK_AUDIO,TXT_AI_CHECK_PASS,TXT_AI_CHECK_FAIL,TXT_AI_CHECK_STALE};
 return (unsigned)s<sizeof(labels)/sizeof(*labels)?labels[s]:TXT_AI_CHECK_FAIL;
}

const char *ai_check_error_label(int e){return app_error_label(e);}
