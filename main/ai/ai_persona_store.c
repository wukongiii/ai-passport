#include "ai_persona.h"
#include "freertos/FreeRTOS.h"
#include "nvs.h"
#include <stdlib.h>
#include <string.h>
static portMUX_TYPE lock=portMUX_INITIALIZER_UNLOCKED;
static ai_persona_snapshot_t view;
static ai_persona_t pending;
static nvs_handle_t store;
static bool opened,queued;
enum {RECORD_SIZE=4+AI_PERSONA_NAME_CAPACITY+AI_PERSONA_PROMPT_CAPACITY};
void ai_persona_snapshot(ai_persona_snapshot_t *out){portENTER_CRITICAL(&lock);*out=view;portEXIT_CRITICAL(&lock);}
unsigned ai_persona_revision(void){portENTER_CRITICAL(&lock);unsigned r=view.revision;portEXIT_CRITICAL(&lock);return r;}
void ai_persona_init(void){
 if(opened)nvs_close(store);
 esp_err_t error=nvs_open("ai_persona",NVS_READWRITE,&store);opened=error==ESP_OK;
 ai_persona_t *value=malloc(sizeof(*value));unsigned char *record=malloc(RECORD_SIZE);
 if(!value||!record){free(value);free(record);portENTER_CRITICAL(&lock);ai_persona_default(&view.value);view.ready=false;view.error=ESP_ERR_NO_MEM;++view.revision;portEXIT_CRITICAL(&lock);return;}
 ai_persona_default(value);
 if(opened){size_t size=RECORD_SIZE;error=nvs_get_blob(store,"persona",record,&size);
  if(error==ESP_ERR_NVS_NOT_FOUND)error=ESP_OK;
  else if(error==ESP_OK){
   if(size!=RECORD_SIZE||memcmp(record,"AP\1\0",4))error=ESP_ERR_INVALID_ARG;
   else{memcpy(value->name,record+4,sizeof(value->name));memcpy(value->prompt,record+4+sizeof(value->name),sizeof(value->prompt));
    if(!ai_persona_valid(value)){ai_persona_default(value);error=ESP_ERR_INVALID_ARG;}}
  }
 }
 portENTER_CRITICAL(&lock);unsigned revision=view.revision+1;view=(ai_persona_snapshot_t){.value=*value,.ready=opened,.revision=revision,.error=error};queued=false;portEXIT_CRITICAL(&lock);
 free(value);free(record);
}
bool ai_persona_submit(const ai_persona_t *p,unsigned revision,unsigned *id){
 if(!ai_persona_valid(p))return false;
 portENTER_CRITICAL(&lock);bool ok=view.ready&&!view.busy&&view.revision==revision;
 if(ok){pending=*p;queued=true;view.busy=true;view.error=0;++view.submitted;if(id)*id=view.submitted;}portEXIT_CRITICAL(&lock);return ok;
}
void ai_persona_poll(void){
 portENTER_CRITICAL(&lock);bool have=queued;portEXIT_CRITICAL(&lock);if(!have)return;
 // The network worker owns this poll; submissions are refused while busy.
 unsigned char *record=calloc(1,RECORD_SIZE);esp_err_t error=ESP_ERR_NO_MEM;
 if(record){memcpy(record,"AP\1\0",4);memcpy(record+4,pending.name,sizeof(pending.name));memcpy(record+4+sizeof(pending.name),pending.prompt,sizeof(pending.prompt));
  error=nvs_set_blob(store,"persona",record,RECORD_SIZE);if(error==ESP_OK)error=nvs_commit(store);free(record);}
 portENTER_CRITICAL(&lock);if(error==ESP_OK){view.value=pending;++view.revision;}view.error=error;view.completed=view.submitted;view.busy=false;queued=false;memset(&pending,0,sizeof(pending));portEXIT_CRITICAL(&lock);
}
