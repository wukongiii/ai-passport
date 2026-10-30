#include "app_game_voice.h"
#include "ai_config.h"
#include "ai_provider.h"
#include "voice_capture.h"
#include "ai_transport.h"
#include "ai_probe.h"
#include "../app/app_strings.h"
#include "../network/app_network.h"
#include "../speech/app_speech.h"
#include "bsp_audio.h"
#include <time.h>
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG="game_voice";
static portMUX_TYPE lock=portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t worker;
static game_voice_snapshot_t view;
static ai_session_t session;
typedef struct {ai_game_snapshot_t game;ai_reply_t reply;} voice_job_t;
/* Owned from input reservation through owner-side completion; the worker must
 * finish before this mailbox can be freed, including cancellation paths. */
static voice_job_t *job;
static bool queued,cancelled,send_requested,completed,occupied;
static unsigned provider,config_revision;
static uint32_t job_token;
static ai_game_target_t active_target;
static char followup[AI_INPUT_CAPACITY];
static int64_t worker_deadline;
static char worker_input[AI_INPUT_CAPACITY];
static uint32_t follow_revision,follow_configuration,follow_activation;
static uint16_t follow_module;
static unsigned follow_model_revision;
static bool follow_pending;

static bool stop(void){portENTER_CRITICAL(&lock);bool yes=cancelled || esp_timer_get_time()>=worker_deadline;portEXIT_CRITICAL(&lock);return yes;}
static bool transport_stop(void *unused){(void)unused;return stop();}
static void publish(game_voice_state_t state,int error,const char *text){
 portENTER_CRITICAL(&lock);
 if(cancelled && (state==GAME_VOICE_RECORDING || state==GAME_VOICE_WAITING)){portEXIT_CRITICAL(&lock);return;}
 view.state=state;view.error=error;snprintf(view.text,sizeof(view.text),"%s",text?text:"");++view.revision;portEXIT_CRITICAL(&lock);
}
static void process_job(void){
  portENTER_CRITICAL(&lock);bool have=queued;queued=false;portEXIT_CRITICAL(&lock);if(!have)return;
  esp_err_t error=ESP_OK;uint8_t *record=NULL;size_t capacity=0;bool lease=false,audible=false;voice_adpcm_t capture={0};
  // Network time is synchronized in the network service, before verified TLS.
  int64_t clock_deadline=esp_timer_get_time()+5000000;
  while(!stop() && time(NULL)<1700000000 && esp_timer_get_time()<clock_deadline)vTaskDelay(pdMS_TO_TICKS(100));
  if(stop() || time(NULL)<1700000000){error=ESP_ERR_TIMEOUT;goto finish;}
  app_speech_stop();int64_t deadline=esp_timer_get_time()+2000000;
  while(!stop()&&!(lease=app_speech_capture_acquire())&&esp_timer_get_time()<deadline)vTaskDelay(pdMS_TO_TICKS(20));
  if(!lease){error=ESP_ERR_INVALID_STATE;goto finish;}
  error=bsp_audio_init();if(error==ESP_OK)error=bsp_audio_wake();if(error==ESP_OK)error=bsp_audio_set_format(16000,16,1);
  if(error!=ESP_OK)goto finish;
  publish(GAME_VOICE_RECORDING,0,TXT_GAME_VOICE_START);
  int64_t record_deadline=esp_timer_get_time()+VOICE_SECONDS*1000000LL;
  for(unsigned blocks=0;blocks<VOICE_SECONDS*50&&!stop()&&esp_timer_get_time()<record_deadline;++blocks){
   portENTER_CRITICAL(&lock);bool send=send_requested;portEXIT_CRITICAL(&lock);if(send)break;
   /* Grow in small steps; short commands never reserve the full 10-second clip.
    * A failed growth keeps the old pointer for the single cleanup path. */
   size_t needed=(capture.samples+160+1)/2;
   if(needed>capacity){
    size_t next=capacity+2048;if(next>VOICE_CAPACITY)next=VOICE_CAPACITY;
    uint8_t *grown=realloc(record,next);
    if(!grown){error=ESP_ERR_NO_MEM;ESP_LOGW(TAG,"Recording growth failed: need=%u free=%u largest=%u",(unsigned)next,(unsigned)esp_get_free_heap_size(),(unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));break;}
    record=grown;capacity=next;
   }
   int16_t pcm[320];error=bsp_audio_read(pcm,sizeof(pcm));if(error!=ESP_OK)break;
   audible|=voice_capture_audible(pcm,320);voice_capture_encode(&capture,record,pcm,320);
   portENTER_CRITICAL(&lock);view.milliseconds=capture.samples*1000/VOICE_RATE;++view.revision;portEXIT_CRITICAL(&lock);
  }
finish:
  if(lease){esp_err_t slept=bsp_audio_sleep();if(error==ESP_OK)error=slept;app_speech_capture_release();}
  if(error==ESP_OK&&!stop()){
   if(!audible||capture.samples<VOICE_RATE/4){error=ESP_ERR_INVALID_ARG;publish(GAME_VOICE_ERROR,error,TXT_GAME_VOICE_EMPTY);}
   else{
    size_t used=(capture.samples+1)/2;uint8_t *trimmed=realloc(record,used);if(trimmed){record=trimmed;capacity=used;}
    ESP_LOGI(TAG,"Upload recording: bytes=%u capacity=%u free=%u largest=%u",(unsigned)used,(unsigned)capacity,(unsigned)esp_get_free_heap_size(),(unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    publish(GAME_VOICE_WAITING,0,TXT_GAME_VOICE_WAIT);ai_transport_control_t control={transport_stop,NULL};error=ai_transport_request(provider,&job->game,worker_input[0]?worker_input:"Understand the recorded request and choose an operation or ask for clarification.",record,capture.samples,&job->reply,&control);record=NULL;}
  }
  free(record);
  portENTER_CRITICAL(&lock);completed=true;view.error=error;portEXIT_CRITICAL(&lock);
}
static void run(void *arg){(void)arg;for(;;){
 ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(100));process_job();
 portENTER_CRITICAL(&lock);bool busy=occupied;portEXIT_CRITICAL(&lock);
 if(!busy){
 portENTER_CRITICAL(&lock);busy=occupied;if(!busy)occupied=true;portEXIT_CRITICAL(&lock);
 if(!busy){unsigned selected,generation;if(ai_check_take(&selected,&generation)){
  ai_probe_run(selected,generation);
 }
  portENTER_CRITICAL(&lock);occupied=false;portEXIT_CRITICAL(&lock);
 }}
}}
bool app_game_voice_start(void){if(worker)return true;if(xTaskCreate(run,"game_voice",12288,NULL,3,&worker)==pdPASS)return true;worker=NULL;return false;}
void app_game_voice_snapshot(game_voice_snapshot_t *out){portENTER_CRITICAL(&lock);*out=view;portEXIT_CRITICAL(&lock);}
void app_game_voice_cancel(void){
 ai_session_cancel(&session);follow_pending=false;
 portENTER_CRITICAL(&lock);bool already=cancelled;cancelled=true;bool busy=occupied;view.state=busy?GAME_VOICE_CANCELLING:GAME_VOICE_IDLE;view.text[0]=0;if(!already || !busy)++view.revision;portEXIT_CRITICAL(&lock);
}
static bool busy_state(game_voice_state_t s){return s==GAME_VOICE_STARTING||s==GAME_VOICE_RECORDING||s==GAME_VOICE_WAITING||s==GAME_VOICE_CANCELLING;}
void app_game_voice_bindings(ai_game_target_t target,app_bindings_t *b){
 if(!target.ops)return;
 uint32_t r,c;if(!target.ops->stamp(target.context,&r,&c))return;
 game_voice_snapshot_t s;app_game_voice_snapshot(&s);
 if(busy_state(s.state)){
  b->actions[APP_GESTURE_LONG][1]=(app_action_hint_t){0};
  b->actions[APP_GESTURE_SHORT][1]=(app_action_hint_t){0};b->actions[APP_GESTURE_DOUBLE][1]=(app_action_hint_t){0};
  if(s.state==GAME_VOICE_RECORDING)app_actions_register(b,1,APP_GESTURE_SHORT,64001,TXT_GAME_VOICE_SEND,true,APP_ACTION_CONFIRM);
  app_actions_register(b,1,APP_GESTURE_LONG,64002,TXT_GAME_VOICE_CANCEL,true,APP_ACTION_CANCEL);
 }else app_actions_register(b,1,APP_GESTURE_DOUBLE,64000,TXT_GAME_VOICE,true,APP_ACTION_NAVIGATE);
}
bool app_game_voice_input(ai_game_target_t target,unsigned slot,app_gesture_t gesture){
 game_voice_snapshot_t s;app_game_voice_snapshot(&s);
 if(busy_state(s.state)){
  if(slot!=1){app_game_voice_cancel();return false;}
  if(gesture==APP_GESTURE_LONG)app_game_voice_cancel();
  else if(gesture==APP_GESTURE_SHORT&&s.state==GAME_VOICE_RECORDING){portENTER_CRITICAL(&lock);send_requested=true;view.state=GAME_VOICE_WAITING;snprintf(view.text,sizeof(view.text),"%s",TXT_GAME_VOICE_WAIT);++view.revision;portEXIT_CRITICAL(&lock);}
  return true;
 }
 if(slot!=1||gesture!=APP_GESTURE_DOUBLE||!target.ops){
  if(s.state!=GAME_VOICE_IDLE)app_game_voice_cancel();
  return false;
 }
 uint32_t r,c;if(!target.ops->stamp(target.context,&r,&c))return false;
 portENTER_CRITICAL(&lock);bool unavailable=occupied || completed;portEXIT_CRITICAL(&lock);if(unavailable)return true;
 active_target=target;
 portENTER_CRITICAL(&lock);cancelled=false;portEXIT_CRITICAL(&lock);
 ai_runtime_snapshot_t config;ai_config_runtime(&config);app_network_snapshot_t network;app_network_snapshot(&network);
 if(!config.ready||config.active>=AI_PROVIDER_COUNT||!config.configured[config.active]||!ai_model_adapter(ai_provider(config.active)->id)){publish(GAME_VOICE_ERROR,0,TXT_GAME_VOICE_NEED_KEY);return true;}
 if(network.state!=APP_NET_CONNECTED){publish(GAME_VOICE_ERROR,0,TXT_GAME_VOICE_NEED_WIFI);return true;}
 if(!worker){publish(GAME_VOICE_ERROR,ESP_ERR_NO_MEM,TXT_GAME_VOICE_REJECT);return true;}
 portENTER_CRITICAL(&lock);unavailable=occupied||completed;if(!unavailable)occupied=true;portEXIT_CRITICAL(&lock);
 if(unavailable)return true;
 job=malloc(sizeof(*job));
 if(!job){portENTER_CRITICAL(&lock);occupied=false;portEXIT_CRITICAL(&lock);publish(GAME_VOICE_ERROR,ESP_ERR_NO_MEM,TXT_GAME_VOICE_REJECT);return true;}
 if(ai_session_begin(&session,target,esp_timer_get_time(),&job->game,&job_token)!=AI_FLOW_OK){free(job);job=NULL;portENTER_CRITICAL(&lock);occupied=false;portEXIT_CRITICAL(&lock);return true;}
 if(!(follow_pending&&follow_model_revision==config.generation&&follow_revision==r&&follow_configuration==c&&follow_activation==target.activation&&follow_module==target.module_id))followup[0]=0;
 snprintf(worker_input,sizeof(worker_input),"%s",followup);
 follow_pending=false;provider=config.active;config_revision=config.generation;active_target=target;
 portENTER_CRITICAL(&lock);cancelled=false;send_requested=false;completed=false;occupied=true;queued=true;worker_deadline=session.deadline;view.milliseconds=0;portEXIT_CRITICAL(&lock);
 publish(GAME_VOICE_STARTING,0,TXT_GAME_VOICE_START);xTaskNotifyGive(worker);return true;
}
bool app_game_voice_tick(ai_game_target_t target,int64_t now){
 static unsigned rendered;game_voice_snapshot_t s;app_game_voice_snapshot(&s);
 if(s.state!=GAME_VOICE_IDLE && (target.ops!=active_target.ops || target.context!=active_target.context || target.module_id!=active_target.module_id || target.activation!=active_target.activation))app_game_voice_cancel();
 if(session.pending){uint32_t r,c;ai_runtime_snapshot_t config;ai_config_runtime(&config);
  if(target.ops!=active_target.ops||target.module_id!=active_target.module_id||target.activation!=active_target.activation||!target.ops||!target.ops->stamp(target.context,&r,&c)||r!=session.revision||c!=session.configuration||config.generation!=config_revision||now>=session.deadline)app_game_voice_cancel();
 }
 portENTER_CRITICAL(&lock);bool done=completed;completed=false;if(done)occupied=false;bool was_cancelled=cancelled;int error=view.error;portEXIT_CRITICAL(&lock);
 if(done){
  if(was_cancelled){publish(GAME_VOICE_IDLE,0,"");}
  else if(error){ai_session_cancel(&session);if(strcmp(s.text,TXT_GAME_VOICE_EMPTY)){char text[96];snprintf(text,sizeof(text),TXT_GAME_VOICE_FAILED,error);publish(GAME_VOICE_ERROR,error,text);}}
  else{ai_completion_t result;ai_flow_result_t flow=ai_session_finish(&session,target,job_token,now,&job->reply,&result);
   if(flow!=AI_FLOW_OK||result.game_result!=AI_GAME_OK)publish(GAME_VOICE_ERROR,0,TXT_GAME_VOICE_REJECT);
   else if(result.executed){
    ai_json_t *json=malloc(sizeof(*json));char text[AI_TEXT_CAPACITY];
    bool message=json&&ai_json_parse(json,result.text,strlen(result.text))&&ai_json_string(json,ai_json_member(json,0,"message"),text,sizeof(text));free(json);
    if(message){publish(GAME_VOICE_REPLY,0,text);app_speech_say(text);}
    else publish(GAME_VOICE_REPLY,0,TXT_GAME_VOICE_DONE);
   }else{
    publish(GAME_VOICE_REPLY,0,result.text);app_speech_say(result.text);
    if(result.kind==AI_REPLY_CLARIFY){snprintf(followup,sizeof(followup),"Clarification previously asked: %s. The recording answers this question.",result.text);follow_pending=true;follow_model_revision=config_revision;follow_revision=session.revision;follow_configuration=session.configuration;follow_activation=target.activation;follow_module=target.module_id;}
   }
  }
  free(job);job=NULL;
 }
 app_game_voice_snapshot(&s);bool changed=rendered!=s.revision;rendered=s.revision;return changed;
}
