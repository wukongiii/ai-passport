#include "ai_config.h"
#include "freertos/FreeRTOS.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <string.h>

static portMUX_TYPE lock=portMUX_INITIALIZER_UNLOCKED;
static ai_config_snapshot_t view={.active=AI_PROVIDER_NONE};
static ai_config_t current;
static ai_config_request_t pending;
static bool has_pending,opened;
static nvs_handle_t store;
/* Explicit versioned byte format, independent of struct padding. */
enum { RECORD_SIZE=4+AI_KEY_CAPACITY+AI_MODEL_CAPACITY+AI_URL_CAPACITY };
static void publish(int error){
    portENTER_CRITICAL(&lock);
    view.active=current.active;view.error=error;++view.revision;
    memcpy(view.model,current.model,sizeof(view.model));memcpy(view.url,current.url,sizeof(view.url));
    view.configured[0]=*current.key&&*current.model&&*current.url;
    portEXIT_CRITICAL(&lock);
}
void ai_config_snapshot(ai_config_snapshot_t *out){portENTER_CRITICAL(&lock);*out=view;portEXIT_CRITICAL(&lock);}
void ai_config_service_init(void){
    if(opened){nvs_close(store);opened=false;}
    portENTER_CRITICAL(&lock);view.ready=false;view.busy=false;has_pending=false;portEXIT_CRITICAL(&lock);
    ai_config_t initial;ai_config_default(&initial);
    portENTER_CRITICAL(&lock);current=initial;portEXIT_CRITICAL(&lock);
    esp_err_t e=nvs_flash_init();if(e==ESP_OK)e=nvs_open("ai_voice",NVS_READWRITE,&store);
    if(e!=ESP_OK){publish(e);return;}
    opened=true;
    uint8_t record[RECORD_SIZE]={0};size_t size=sizeof(record);
    e=nvs_get_blob(store,"connection",record,&size);
    if(e==ESP_OK){
        ai_config_t loaded;ai_config_default(&loaded);loaded.active=record[3];
        bool valid=record[0]=='A'&&record[1]=='I'&&record[2]==1&&size==RECORD_SIZE;
        if(valid){memcpy(loaded.key,record+4,AI_KEY_CAPACITY);memcpy(loaded.model,record+4+AI_KEY_CAPACITY,AI_MODEL_CAPACITY);memcpy(loaded.url,record+4+AI_KEY_CAPACITY+AI_MODEL_CAPACITY,AI_URL_CAPACITY);}
        if(!valid || !ai_config_valid(&loaded))e=ESP_ERR_INVALID_ARG;
        else{portENTER_CRITICAL(&lock);current=loaded;portEXIT_CRITICAL(&lock);}
        memset(&loaded,0,sizeof(loaded));
    }else if(e==ESP_ERR_NVS_NOT_FOUND)e=ESP_OK;
    memset(record,0,sizeof(record));
    portENTER_CRITICAL(&lock);view.ready=true;++view.generation;portEXIT_CRITICAL(&lock);publish(e);
}
bool ai_config_submit(const ai_config_request_t *r,unsigned *id){
    if(!r || r->provider>=AI_PROVIDER_COUNT || r->action>AI_CONFIG_REMOVE)return false;
    portENTER_CRITICAL(&lock);bool ok=view.ready && !view.busy;
    if(ok){pending=*r;has_pending=true;view.busy=true;view.error=0;++view.submitted;++view.revision;if(id)*id=view.submitted;}
    portEXIT_CRITICAL(&lock);return ok;
}
void ai_config_service_poll(void){
    ai_config_request_t request={0};unsigned id=0;
    portENTER_CRITICAL(&lock);bool have=has_pending;
    if(have){request=pending;memset(&pending,0,sizeof(pending));has_pending=false;id=view.submitted;}
    portEXIT_CRITICAL(&lock);if(!have)return;
    ai_config_t next=current;esp_err_t e=ESP_ERR_INVALID_ARG;
    if(ai_config_apply(&next,&request)){
        uint8_t record[RECORD_SIZE]={'A','I',1,next.active};memcpy(record+4,next.key,AI_KEY_CAPACITY);
        memcpy(record+4+AI_KEY_CAPACITY,next.model,AI_MODEL_CAPACITY);memcpy(record+4+AI_KEY_CAPACITY+AI_MODEL_CAPACITY,next.url,AI_URL_CAPACITY);
        e=nvs_set_blob(store,"connection",record,sizeof(record));if(e==ESP_OK)e=nvs_commit(store);
        memset(record,0,sizeof(record));if(e==ESP_OK){portENTER_CRITICAL(&lock);current=next;++view.generation;portEXIT_CRITICAL(&lock);}
    }
    memset(&next,0,sizeof(next));memset(&request,0,sizeof(request));
    publish(e);
    portENTER_CRITICAL(&lock);view.completed=id;view.busy=false;++view.revision;portEXIT_CRITICAL(&lock);
}

bool ai_config_connection(unsigned provider,char key[AI_KEY_CAPACITY],char model[AI_MODEL_CAPACITY],char url[AI_URL_CAPACITY],unsigned *generation){
 if(!key||!model||!url)return false;
 portENTER_CRITICAL(&lock);
 bool ok=view.ready && !view.busy && provider<AI_PROVIDER_COUNT && *current.key && *current.model && *current.url;
 if(ok){memcpy(key,current.key,AI_KEY_CAPACITY);memcpy(model,current.model,AI_MODEL_CAPACITY);memcpy(url,current.url,AI_URL_CAPACITY);if(generation)*generation=view.generation;}
 portEXIT_CRITICAL(&lock);if(!ok){key[0]=0;model[0]=0;url[0]=0;}return ok;
}
static ai_check_snapshot_t check={.provider=AI_PROVIDER_NONE};
void ai_check_snapshot(ai_check_snapshot_t *out){
 portENTER_CRITICAL(&lock);*out=check;
 if(out->provider!=AI_PROVIDER_NONE && out->generation!=view.generation){out->state=AI_CHECK_STALE;out->text=out->tools=out->audio=false;out->error=0;}
 portEXIT_CRITICAL(&lock);
}
bool ai_check_request(unsigned provider){
 portENTER_CRITICAL(&lock);
 bool running=check.state>=AI_CHECK_QUEUED&&check.state<=AI_CHECK_AUDIO;
 bool ok=view.ready&&!view.busy&&!running&&provider<AI_PROVIDER_COUNT&&view.configured[provider];
 if(ok)check=(ai_check_snapshot_t){.provider=provider,.generation=view.generation,.state=AI_CHECK_QUEUED,.revision=check.revision+1};
 portEXIT_CRITICAL(&lock);return ok;
}
bool ai_check_take(unsigned *provider,unsigned *generation){
 portENTER_CRITICAL(&lock);bool yes=check.state==AI_CHECK_QUEUED;
 if(yes){*provider=check.provider;*generation=check.generation;check.state=AI_CHECK_TEXT;++check.revision;}
 portEXIT_CRITICAL(&lock);return yes;
}
void ai_check_publish(unsigned provider,unsigned generation,ai_check_state_t state,int error){
 portENTER_CRITICAL(&lock);
 if(check.provider==provider&&check.generation==generation){
  check.state=generation==view.generation?state:AI_CHECK_STALE;check.error=error;++check.revision;
  if(check.state==AI_CHECK_STALE){check.text=check.tools=check.audio=false;check.error=0;}
  if(check.state==AI_CHECK_TOOLS)check.text=true;
  if(check.state==AI_CHECK_AUDIO)check.tools=true;
  if(check.state==AI_CHECK_PASS)check.audio=true;
 }
 portEXIT_CRITICAL(&lock);
}
