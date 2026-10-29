#include "ai_config.h"
#include "freertos/FreeRTOS.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <string.h>

static portMUX_TYPE lock=portMUX_INITIALIZER_UNLOCKED;
static ai_config_snapshot_t view={.active=AI_PROVIDER_NONE};
static ai_config_t current;
static ai_config_request_t pending;
static bool has_pending;
static nvs_handle_t store;
/* Explicit versioned byte format, independent of struct padding. */
enum { RECORD_SIZE=4+AI_PROVIDER_COUNT*AI_KEY_CAPACITY };
static void publish(int error){
    portENTER_CRITICAL(&lock);
    view.active=current.active;view.error=error;++view.revision;
    for(unsigned i=0;i<AI_PROVIDER_COUNT;++i)view.configured[i]=*current.keys[i]!=0;
    portEXIT_CRITICAL(&lock);
}
void ai_config_snapshot(ai_config_snapshot_t *out){portENTER_CRITICAL(&lock);*out=view;portEXIT_CRITICAL(&lock);}
void ai_config_service_init(void){
    ai_config_default(&current);
    esp_err_t e=nvs_flash_init();if(e==ESP_OK)e=nvs_open("ai_voice",NVS_READWRITE,&store);
    if(e!=ESP_OK){publish(e);return;}
    uint8_t record[RECORD_SIZE]={0};size_t size=sizeof(record);
    e=nvs_get_blob(store,"models",record,&size);
    if(e==ESP_OK){
        ai_config_t loaded={.active=record[3]};memcpy(loaded.keys,record+4,sizeof(loaded.keys));
        if(size!=sizeof(record) || record[0]!='A' || record[1]!='I' || record[2]!=1 || !ai_config_valid(&loaded))e=ESP_ERR_INVALID_ARG;
        else current=loaded;
        memset(&loaded,0,sizeof(loaded));
    }else if(e==ESP_ERR_NVS_NOT_FOUND)e=ESP_OK;
    memset(record,0,sizeof(record));
    portENTER_CRITICAL(&lock);view.ready=true;portEXIT_CRITICAL(&lock);publish(e);
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
        uint8_t record[RECORD_SIZE]={'A','I',1,next.active};memcpy(record+4,next.keys,sizeof(next.keys));
        e=nvs_set_blob(store,"models",record,sizeof(record));if(e==ESP_OK)e=nvs_commit(store);
        memset(record,0,sizeof(record));if(e==ESP_OK)current=next;
    }
    memset(&next,0,sizeof(next));memset(&request,0,sizeof(request));
    publish(e);
    portENTER_CRITICAL(&lock);view.completed=id;view.busy=false;++view.revision;portEXIT_CRITICAL(&lock);
}
