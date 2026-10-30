#include "ai_config.h"
#include "freertos/FreeRTOS.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <string.h>
#include <stdlib.h>

static ai_check_snapshot_t checks[AI_PROVIDER_COUNT];
static unsigned checking=AI_PROVIDER_NONE;
static portMUX_TYPE lock=portMUX_INITIALIZER_UNLOCKED;
static ai_config_snapshot_t view={.active=AI_PROVIDER_NONE};
static ai_config_t current;
static ai_config_request_t pending;
static bool has_pending,opened;
static nvs_handle_t store;
/* Explicit versioned byte format, independent of struct padding. */
enum { ENTRY_SIZE=AI_NAME_CAPACITY+AI_KEY_CAPACITY+AI_MODEL_CAPACITY+AI_URL_CAPACITY, RECORD_SIZE=4+AI_PROVIDER_COUNT*ENTRY_SIZE };

static void publish(int error){
    portENTER_CRITICAL(&lock);
    view.active=current.active;view.error=error;++view.revision;
    for(unsigned i=0;i<AI_PROVIDER_COUNT;++i){
     memcpy(view.entries[i].name,current.entries[i].name,AI_NAME_CAPACITY);
     memcpy(view.entries[i].model,current.entries[i].model,AI_MODEL_CAPACITY);
     memcpy(view.entries[i].url,current.entries[i].url,AI_URL_CAPACITY);
     view.configured[i]=*current.entries[i].name!=0;
    }
    portEXIT_CRITICAL(&lock);
}
void ai_config_snapshot(ai_config_snapshot_t *out){portENTER_CRITICAL(&lock);*out=view;portEXIT_CRITICAL(&lock);}
void ai_config_runtime(ai_runtime_snapshot_t *out){
 portENTER_CRITICAL(&lock);out->ready=view.ready;out->busy=view.busy;out->active=view.active;out->generation=view.generation;
 memcpy(out->configured,view.configured,sizeof(out->configured));portEXIT_CRITICAL(&lock);
}
void ai_config_service_init(void){
    if(opened){nvs_close(store);opened=false;}
    portENTER_CRITICAL(&lock);view.ready=false;view.busy=false;has_pending=false;memset(checks,0,sizeof(checks));checking=AI_PROVIDER_NONE;portEXIT_CRITICAL(&lock);
    portENTER_CRITICAL(&lock);ai_config_default(&current);portEXIT_CRITICAL(&lock);
    esp_err_t e=nvs_flash_init();if(e==ESP_OK)e=nvs_open("ai_voice",NVS_READWRITE,&store);
    if(e!=ESP_OK){publish(e);return;}
    opened=true;
    uint8_t *record=calloc(1,RECORD_SIZE);ai_config_t *next=calloc(1,sizeof(*next));
    if(!record||!next){free(record);free(next);publish(ESP_ERR_NO_MEM);return;}
    size_t size=RECORD_SIZE;
    e=nvs_get_blob(store,"profiles",record,&size);
    if(e==ESP_OK){
        ai_config_default(next);next->active=record[3];
        bool valid=record[0]=='A'&&record[1]=='I'&&record[2]==1&&size==RECORD_SIZE;
        if(valid)for(unsigned i=0;i<AI_PROVIDER_COUNT;++i){
         const uint8_t *p=record+4+i*ENTRY_SIZE;ai_connection_t *e=&next->entries[i];
         memcpy(e->name,p,AI_NAME_CAPACITY);p+=AI_NAME_CAPACITY;memcpy(e->key,p,AI_KEY_CAPACITY);p+=AI_KEY_CAPACITY;
         memcpy(e->model,p,AI_MODEL_CAPACITY);p+=AI_MODEL_CAPACITY;memcpy(e->url,p,AI_URL_CAPACITY);
        }
        if(!valid || !ai_config_valid(next))e=ESP_ERR_INVALID_ARG;
        else{portENTER_CRITICAL(&lock);current=*next;portEXIT_CRITICAL(&lock);}
        memset(next,0,sizeof(*next));
    }else if(e==ESP_ERR_NVS_NOT_FOUND)e=ESP_OK;
    memset(record,0,RECORD_SIZE);free(record);free(next);
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
    ai_config_t *next=malloc(sizeof(*next));uint8_t *record=calloc(1,RECORD_SIZE);
    esp_err_t e=ESP_ERR_NO_MEM;
    if(next&&record){*next=current;e=ESP_ERR_INVALID_ARG;
    if(ai_config_apply(next,&request)){
        memset(record,0,RECORD_SIZE);record[0]='A';record[1]='I';record[2]=1;record[3]=next->active;
        for(unsigned i=0;i<AI_PROVIDER_COUNT;++i){uint8_t *p=record+4+i*ENTRY_SIZE;const ai_connection_t *entry=&next->entries[i];
         memcpy(p,entry->name,AI_NAME_CAPACITY);p+=AI_NAME_CAPACITY;memcpy(p,entry->key,AI_KEY_CAPACITY);p+=AI_KEY_CAPACITY;
         memcpy(p,entry->model,AI_MODEL_CAPACITY);p+=AI_MODEL_CAPACITY;memcpy(p,entry->url,AI_URL_CAPACITY);
        }
        e=nvs_set_blob(store,"profiles",record,RECORD_SIZE);if(e==ESP_OK)e=nvs_commit(store);
        memset(record,0,RECORD_SIZE);if(e==ESP_OK){portENTER_CRITICAL(&lock);current=*next;++view.generation;
         if(request.action!=AI_CONFIG_SELECT){ai_check_snapshot_t *check=&checks[request.provider];
          if(check->state!=AI_CHECK_UNTESTED){check->state=AI_CHECK_STALE;check->text=check->tools=check->audio=false;check->error=0;++check->revision;}
         }portEXIT_CRITICAL(&lock);}
    }
    }
    if(next)memset(next,0,sizeof(*next));
    if(record)memset(record,0,RECORD_SIZE);
    free(next);free(record);memset(&request,0,sizeof(request));
    publish(e);
    portENTER_CRITICAL(&lock);view.completed=id;view.busy=false;++view.revision;portEXIT_CRITICAL(&lock);
}

bool ai_config_connection(unsigned provider,char key[AI_KEY_CAPACITY],char model[AI_MODEL_CAPACITY],char url[AI_URL_CAPACITY],unsigned *generation){
 if(!key||!model||!url)return false;
 portENTER_CRITICAL(&lock);
 bool ok=view.ready && !view.busy && provider<AI_PROVIDER_COUNT && *current.entries[provider].name;
 if(ok){memcpy(key,current.entries[provider].key,AI_KEY_CAPACITY);memcpy(model,current.entries[provider].model,AI_MODEL_CAPACITY);memcpy(url,current.entries[provider].url,AI_URL_CAPACITY);if(generation)*generation=view.generation;}
 portEXIT_CRITICAL(&lock);if(!ok){key[0]=0;model[0]=0;url[0]=0;}return ok;
}
/* Results belong to slots; one check runs globally at a time. */
void ai_check_snapshot(unsigned provider,ai_check_snapshot_t *out){
 portENTER_CRITICAL(&lock);
 *out=provider<AI_PROVIDER_COUNT?checks[provider]:(ai_check_snapshot_t){0};out->provider=provider;
 portEXIT_CRITICAL(&lock);
}
bool ai_check_request(unsigned provider){
 portENTER_CRITICAL(&lock);
 bool ok=view.ready&&!view.busy&&checking==AI_PROVIDER_NONE&&provider<AI_PROVIDER_COUNT&&view.configured[provider];
 if(ok){checking=provider;checks[provider]=(ai_check_snapshot_t){.provider=provider,.generation=view.generation,.state=AI_CHECK_QUEUED,.revision=checks[provider].revision+1};}
 portEXIT_CRITICAL(&lock);return ok;
}
bool ai_check_take(unsigned *provider,unsigned *generation){
 portENTER_CRITICAL(&lock);bool yes=checking<AI_PROVIDER_COUNT;
 if(yes){*provider=checking;*generation=checks[checking].generation;checks[checking].state=AI_CHECK_TEXT;++checks[checking].revision;}
 portEXIT_CRITICAL(&lock);return yes;
}
void ai_check_publish(unsigned provider,unsigned generation,ai_check_state_t state,int error){
 portENTER_CRITICAL(&lock);
 if(checking==provider&&checks[provider].generation==generation){
  ai_check_snapshot_t *check=&checks[provider];
  check->state=generation==view.generation?state:AI_CHECK_STALE;check->error=error;++check->revision;
  if(check->state==AI_CHECK_STALE){check->text=check->tools=check->audio=false;check->error=0;}
  if(check->state==AI_CHECK_TOOLS)check->text=true;
  if(check->state==AI_CHECK_AUDIO)check->tools=true;
  if(check->state==AI_CHECK_PASS)check->audio=true;
  if(check->state>=AI_CHECK_PASS)checking=AI_PROVIDER_NONE;
 }
 portEXIT_CRITICAL(&lock);
}
