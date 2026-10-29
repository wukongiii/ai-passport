#include "app_game_voice.h"
#include "ai_config.h"
#include "ai_provider.h"
#include "voice_capture.h"
#include "../app/app_strings.h"
#include "../network/app_network.h"
#include "../speech/app_speech.h"
#include "bsp_audio.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include <time.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static portMUX_TYPE lock=portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t worker;
static game_voice_snapshot_t view;
static ai_session_t session;
static ai_game_snapshot_t game;
static ai_reply_t reply;
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
static void publish(game_voice_state_t state,int error,const char *text){
 portENTER_CRITICAL(&lock);
 if(cancelled && (state==GAME_VOICE_RECORDING || state==GAME_VOICE_WAITING)){portEXIT_CRITICAL(&lock);return;}
 view.state=state;view.error=error;snprintf(view.text,sizeof(view.text),"%s",text?text:"");++view.revision;portEXIT_CRITICAL(&lock);
}
static bool write_all(esp_http_client_handle_t client,const char *data,size_t size){
 while(size&&!stop()){int n=esp_http_client_write(client,data,(int)size);if(n<=0)return false;data+=n;size-=(unsigned)n;}return !size&&!stop();
}
static size_t base64(const uint8_t *in,size_t count,char *out){
 static const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";size_t used=0;
 for(size_t i=0;i<count;i+=3){unsigned n=(unsigned)in[i]<<16;if(i+1<count)n|=(unsigned)in[i+1]<<8;if(i+2<count)n|=in[i+2];
 out[used++]=alphabet[(n>>18)&63];out[used++]=alphabet[(n>>12)&63];out[used++]=i+1<count?alphabet[(n>>6)&63]:'=';out[used++]=i+2<count?alphabet[n&63]:'=';}return used;
}
static esp_err_t upload(uint8_t *record,unsigned samples,unsigned selected,ai_reply_t *decoded){
 esp_err_t error=ESP_FAIL;char key[AI_KEY_CAPACITY]={0},model[AI_MODEL_CAPACITY]={0},auth[AI_KEY_CAPACITY+8]={0};
 if(!ai_config_credentials(selected,key,model)){free(record);return ESP_ERR_INVALID_STATE;}
 const ai_model_adapter_t *adapter=ai_model_adapter(ai_provider(selected)->id);
 ai_json_t *json=malloc(sizeof(*json));char *head=malloc(AI_BODY_CAPACITY),*tail=malloc(4096),*response=NULL;
 esp_http_client_handle_t client=NULL;
 if(!json||!head||!tail){error=ESP_ERR_NO_MEM;goto done;}
 ai_model_request_t request={model,worker_input[0]?worker_input:"Understand the recorded request and choose an operation or ask for clarification.",&game};
 if(!adapter||!adapter->audio_parts||!adapter->audio_parts(json,&request,head,AI_BODY_CAPACITY,tail,4096)){error=ESP_ERR_INVALID_ARG;goto done;}
 free(json);json=NULL; // Codec workspace is not retained during the TLS handshake.
 esp_http_client_config_t config={.url=adapter->endpoint,.method=HTTP_METHOD_POST,.timeout_ms=5000,.crt_bundle_attach=esp_crt_bundle_attach,.buffer_size=1024,.buffer_size_tx=1024,.disable_auto_redirect=true};
 client=esp_http_client_init(&config);if(!client){error=ESP_ERR_NO_MEM;goto done;}
 snprintf(auth,sizeof(auth),"Bearer %s",key);esp_http_client_set_header(client,"Authorization",auth);esp_http_client_set_header(client,"Content-Type","application/json");
 unsigned wav_size=44+samples*2;size_t total=strlen(head)+4*((wav_size+2)/3)+strlen(tail);
 error=esp_http_client_open(client,(int)total);memset(key,0,sizeof(key));memset(auth,0,sizeof(auth));if(error!=ESP_OK)goto done;
 esp_http_client_set_timeout_ms(client,1000);
 if(!write_all(client,head,strlen(head))){error=ESP_FAIL;goto done;}
 free(head);head=NULL; // Retain the small provider suffix until audio is uploaded.
 uint8_t wav[44],pcm[768];char encoded[1024];voice_wav_header(wav,samples);voice_adpcm_t decoder={0};unsigned position=0;
 while(position<wav_size&&!stop()){
  size_t count=wav_size-position;if(count>sizeof(pcm))count=sizeof(pcm);
  for(size_t i=0;i<count;++i,++position){
   if(position<44)pcm[i]=wav[position];
   else{unsigned at=position-44;if(!(at&1)){unsigned sample=at/2;uint8_t n=(record[sample/2]>>(4*(sample&1)))&15;int16_t value=voice_capture_decode(&decoder,n);pcm[i]=(uint8_t)value;wav[0]=(uint8_t)((uint16_t)value>>8);}else pcm[i]=wav[0];}
  }
  if(!write_all(client,encoded,base64(pcm,count,encoded))){error=ESP_FAIL;goto done;}
 }
 free(record);record=NULL;
 if(stop()){error=ESP_ERR_INVALID_STATE;goto done;}
 if(!write_all(client,tail,strlen(tail))){error=ESP_FAIL;goto done;}
 free(head);head=NULL;free(tail);tail=NULL;
 int64_t headers;
 do{headers=esp_http_client_fetch_headers(client);}while(headers==-ESP_ERR_HTTP_EAGAIN&&!stop());
 if(headers<0||stop()){error=ESP_FAIL;goto done;}
 response=malloc(AI_RESPONSE_CAPACITY+1);if(!response){error=ESP_ERR_NO_MEM;goto done;}
 int status=esp_http_client_get_status_code(client);if(status!=200){error=status;goto done;}
 size_t used=0;
 while(!stop()){
  int n=esp_http_client_read(client,response+used,AI_RESPONSE_CAPACITY-used);
  if(n==-ESP_ERR_HTTP_EAGAIN)continue;
  if(n<0){error=ESP_FAIL;goto done;}
  if(!n){if(esp_http_client_is_complete_data_received(client))break;vTaskDelay(pdMS_TO_TICKS(10));continue;}
  used+=(unsigned)n;if(used==AI_RESPONSE_CAPACITY){error=ESP_ERR_INVALID_SIZE;goto done;}
 }
 esp_http_client_cleanup(client);client=NULL;
 json=malloc(sizeof(*json));if(!json){error=ESP_ERR_NO_MEM;goto done;}
 response[used]=0;error=!stop()&&adapter->decode(json,response,used,decoded)?ESP_OK:ESP_ERR_INVALID_RESPONSE;
done:
 if(client)esp_http_client_cleanup(client);
 memset(key,0,sizeof(key));memset(auth,0,sizeof(auth));free(json);free(head);free(tail);free(response);free(record);return error;
}
static void process_job(void){
  portENTER_CRITICAL(&lock);bool have=queued;queued=false;portEXIT_CRITICAL(&lock);if(!have)return;
  esp_err_t error=ESP_OK;uint8_t *record=NULL;bool lease=false,audible=false;voice_adpcm_t capture={0};
  // Network time is synchronized in the network service, before verified TLS.
  int64_t clock_deadline=esp_timer_get_time()+5000000;
  while(!stop() && time(NULL)<1700000000 && esp_timer_get_time()<clock_deadline)vTaskDelay(pdMS_TO_TICKS(100));
  if(stop() || time(NULL)<1700000000){error=ESP_ERR_TIMEOUT;goto finish;}
  app_speech_stop();int64_t deadline=esp_timer_get_time()+2000000;
  while(!stop()&&!(lease=app_speech_capture_acquire())&&esp_timer_get_time()<deadline)vTaskDelay(pdMS_TO_TICKS(20));
  if(!lease){error=ESP_ERR_INVALID_STATE;goto finish;}
  record=malloc(VOICE_CAPACITY);if(!record){error=ESP_ERR_NO_MEM;goto finish;}
  error=bsp_audio_init();if(error==ESP_OK)error=bsp_audio_wake();if(error==ESP_OK)error=bsp_audio_set_format(16000,16,1);
  if(error!=ESP_OK)goto finish;
  publish(GAME_VOICE_RECORDING,0,TXT_GAME_VOICE_START);
  int64_t record_deadline=esp_timer_get_time()+VOICE_SECONDS*1000000LL;
  for(unsigned blocks=0;blocks<VOICE_SECONDS*50&&!stop()&&esp_timer_get_time()<record_deadline;++blocks){
   portENTER_CRITICAL(&lock);bool send=send_requested;portEXIT_CRITICAL(&lock);if(send)break;
   int16_t pcm[320];error=bsp_audio_read(pcm,sizeof(pcm));if(error!=ESP_OK)break;
   audible|=voice_capture_audible(pcm,320);voice_capture_encode(&capture,record,pcm,320);
   portENTER_CRITICAL(&lock);view.milliseconds=capture.samples*1000/VOICE_RATE;++view.revision;portEXIT_CRITICAL(&lock);
  }
finish:
  if(lease){esp_err_t slept=bsp_audio_sleep();if(error==ESP_OK)error=slept;app_speech_capture_release();}
  if(error==ESP_OK&&!stop()){
   if(!audible||capture.samples<VOICE_RATE/4){error=ESP_ERR_INVALID_ARG;publish(GAME_VOICE_ERROR,error,TXT_GAME_VOICE_EMPTY);}
   else{publish(GAME_VOICE_WAITING,0,TXT_GAME_VOICE_WAIT);error=upload(record,capture.samples,provider,&reply);record=NULL;}
  }
  free(record);
  portENTER_CRITICAL(&lock);completed=true;view.error=error;portEXIT_CRITICAL(&lock);
}
static void run(void *arg){(void)arg;for(;;){ulTaskNotifyTake(pdTRUE,portMAX_DELAY);process_job();}}
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
 ai_config_snapshot_t config;ai_config_snapshot(&config);app_network_snapshot_t network;app_network_snapshot(&network);
 if(!config.ready||config.active>=AI_PROVIDER_COUNT||!config.configured[config.active]||!ai_model_adapter(ai_provider(config.active)->id)){publish(GAME_VOICE_ERROR,0,TXT_GAME_VOICE_NEED_KEY);return true;}
 if(network.state!=APP_NET_CONNECTED){publish(GAME_VOICE_ERROR,0,TXT_GAME_VOICE_NEED_WIFI);return true;}
 if(!worker){publish(GAME_VOICE_ERROR,ESP_ERR_NO_MEM,TXT_GAME_VOICE_REJECT);return true;}
 if(ai_session_begin(&session,target,esp_timer_get_time(),&game,&job_token)!=AI_FLOW_OK)return true;
 if(!(follow_pending&&follow_model_revision==config.revision&&follow_revision==r&&follow_configuration==c&&follow_activation==target.activation&&follow_module==target.module_id))followup[0]=0;
 snprintf(worker_input,sizeof(worker_input),"%s",followup);
 follow_pending=false;provider=config.active;config_revision=config.revision;active_target=target;
 portENTER_CRITICAL(&lock);cancelled=false;send_requested=false;completed=false;occupied=true;queued=true;worker_deadline=session.deadline;view.milliseconds=0;portEXIT_CRITICAL(&lock);
 publish(GAME_VOICE_STARTING,0,TXT_GAME_VOICE_START);xTaskNotifyGive(worker);return true;
}
bool app_game_voice_tick(ai_game_target_t target,int64_t now){
 static unsigned rendered;game_voice_snapshot_t s;app_game_voice_snapshot(&s);
 if(s.state!=GAME_VOICE_IDLE && (target.ops!=active_target.ops || target.context!=active_target.context || target.module_id!=active_target.module_id || target.activation!=active_target.activation))app_game_voice_cancel();
 if(session.pending){uint32_t r,c;ai_config_snapshot_t config;ai_config_snapshot(&config);
  if(target.ops!=active_target.ops||target.module_id!=active_target.module_id||target.activation!=active_target.activation||!target.ops||!target.ops->stamp(target.context,&r,&c)||r!=session.revision||c!=session.configuration||config.revision!=config_revision||now>=session.deadline)app_game_voice_cancel();
 }
 portENTER_CRITICAL(&lock);bool done=completed;completed=false;if(done)occupied=false;bool was_cancelled=cancelled;int error=view.error;portEXIT_CRITICAL(&lock);
 if(done){
  if(was_cancelled){publish(GAME_VOICE_IDLE,0,"");}
  else if(error){ai_session_cancel(&session);if(strcmp(s.text,TXT_GAME_VOICE_EMPTY)){char text[96];snprintf(text,sizeof(text),TXT_GAME_VOICE_FAILED,error);publish(GAME_VOICE_ERROR,error,text);}}
  else{ai_completion_t result;ai_flow_result_t flow=ai_session_finish(&session,target,job_token,now,&reply,&result);
   if(flow!=AI_FLOW_OK||result.game_result!=AI_GAME_OK)publish(GAME_VOICE_ERROR,0,TXT_GAME_VOICE_REJECT);
   else if(result.executed){
    static ai_json_t json;char text[AI_TEXT_CAPACITY];
    if(ai_json_parse(&json,result.text,strlen(result.text))&&ai_json_string(&json,ai_json_member(&json,0,"message"),text,sizeof(text))){publish(GAME_VOICE_REPLY,0,text);app_speech_say(text);}
    else publish(GAME_VOICE_REPLY,0,TXT_GAME_VOICE_DONE);
   }else{
    publish(GAME_VOICE_REPLY,0,result.text);app_speech_say(result.text);
    if(result.kind==AI_REPLY_CLARIFY){snprintf(followup,sizeof(followup),"Clarification previously asked: %s. The recording answers this question.",result.text);follow_pending=true;follow_model_revision=config_revision;follow_revision=session.revision;follow_configuration=session.configuration;follow_activation=target.activation;follow_module=target.module_id;}
   }
  }
 }
 app_game_voice_snapshot(&s);bool changed=rendered!=s.revision;rendered=s.revision;return changed;
}
