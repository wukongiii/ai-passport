#include "ai_transport.h"
#include "ai_config.h"
#include "ai_response_filter.h"
#include "ai_provider.h"
#include "voice_capture.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static const char *TRANSPORT_TAG="ai_transport";
static void memory_stage(const char *stage){
 (void)stage;
 ESP_LOGI(TRANSPORT_TAG,"Memory %s: free=%u largest=%u",stage,(unsigned)esp_get_free_heap_size(),(unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}
static void diagnostic_stage(const ai_transport_control_t *c,ai_transport_stage_t stage){if(c&&c->stage)*c->stage=stage;}
static bool stopped(const ai_transport_control_t *c){return c&&c->cancel&&c->cancel(c->context);}
enum { UPLOAD_WRITE_BYTES=512, UPLOAD_STALL_US=10000000 };
static esp_err_t write_all(esp_http_client_handle_t client,const char *data,size_t size,bool tls,const ai_transport_control_t *control){
 int64_t progress=esp_timer_get_time();unsigned waits=0;
 while(size){
  if(stopped(control))return ESP_ERR_INVALID_STATE;
  /* IDF's synchronous HTTP writer loses its accumulated byte count if a
   * subsequent transport write times out. Keep each call within one TLS
   * fragment (512 is the minimum negotiated fragment size). With the pinned
   * TLS transport a zero return then means poll timeout before any write.
   * Never retry negative/ambiguous failures or resubmit a model request. */
  size_t chunk=size>UPLOAD_WRITE_BYTES?UPLOAD_WRITE_BYTES:size;
  int n=esp_http_client_write(client,data,(int)chunk);
  if(n<0){ESP_LOGW(TRANSPORT_TAG,"Upload write failed: result=%d",n);return ESP_FAIL;}
  if(!n){
   if(!tls)return ESP_FAIL; // Plain TCP send may have hidden partial progress.
   ++waits;
   if(esp_timer_get_time()-progress>=UPLOAD_STALL_US){ESP_LOGW(TRANSPORT_TAG,"Upload stalled: polls=%u",waits);return ESP_ERR_TIMEOUT;}
   vTaskDelay(pdMS_TO_TICKS(10));continue;
  }
  if((size_t)n>chunk)return ESP_FAIL;
  data+=n;size-=(unsigned)n;progress=esp_timer_get_time();
 }
 if(waits)ESP_LOGI(TRANSPORT_TAG,"Upload resumed: polls=%u",waits);
 return stopped(control)?ESP_ERR_INVALID_STATE:ESP_OK;
}
static size_t base64(const uint8_t *in,size_t count,char *out){
 static const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";size_t used=0;
 for(size_t i=0;i<count;i+=3){unsigned n=(unsigned)in[i]<<16;if(i+1<count)n|=(unsigned)in[i+1]<<8;if(i+2<count)n|=in[i+2];
 out[used++]=alphabet[(n>>18)&63];out[used++]=alphabet[(n>>12)&63];out[used++]=i+1<count?alphabet[(n>>6)&63]:'=';out[used++]=i+2<count?alphabet[n&63]:'=';}return used;
}
esp_err_t ai_transport_request(unsigned selected,const ai_game_snapshot_t *game,const char *input,voice_buffer_t *record,unsigned samples,ai_reply_t *decoded,const ai_transport_control_t *control){
 diagnostic_stage(control,AI_STAGE_PREPARE);
 esp_err_t error=ESP_FAIL;char key[AI_KEY_CAPACITY]={0},model[AI_MODEL_CAPACITY]={0},auth[AI_KEY_CAPACITY+8]={0},url[AI_URL_CAPACITY]={0};
 if(stopped(control)){voice_buffer_release(record);return ESP_ERR_INVALID_STATE;}
 if(!ai_config_connection(selected,key,model,url,NULL)){voice_buffer_release(record);return ESP_ERR_INVALID_STATE;}
 if(stopped(control)){memset(key,0,sizeof(key));voice_buffer_release(record);return ESP_ERR_INVALID_STATE;}
 const ai_model_adapter_t *adapter=ai_model_adapter(ai_provider(selected)->id);
 const char *stage="request buffers";
 ai_json_t *json=malloc(sizeof(*json));char *head=malloc(AI_BODY_CAPACITY),*tail=record?malloc(4096):NULL,*response=NULL;
 esp_http_client_handle_t client=NULL;
 if(!json||!head||(record&&!tail)){error=ESP_ERR_NO_MEM;goto done;}
 ai_model_request_t request={model,input,game};
 if(!adapter||(record?(!adapter->audio_parts||!adapter->audio_parts(json,&request,head,AI_BODY_CAPACITY,tail,4096)):!adapter->encode(json,&request,head,AI_BODY_CAPACITY))){error=ESP_ERR_INVALID_ARG;goto done;}
 free(json);json=NULL; // Codec workspace is not retained during the TLS handshake.
 size_t head_size=strlen(head),tail_size=tail?strlen(tail):0;
 char *trimmed=realloc(head,head_size+1);if(trimmed)head=trimmed;
 if(tail){trimmed=realloc(tail,tail_size+1);if(trimmed)tail=trimmed;}
 stage="HTTP client";
 esp_http_client_config_t config={.url=url,.method=HTTP_METHOD_POST,.timeout_ms=5000,.crt_bundle_attach=esp_crt_bundle_attach,.buffer_size=1024,.buffer_size_tx=1024,.disable_auto_redirect=true};
 client=esp_http_client_init(&config);if(!client){error=ESP_ERR_NO_MEM;goto done;}
 snprintf(auth,sizeof(auth),"Bearer %s",key);error=esp_http_client_set_header(client,"Authorization",auth);if(error==ESP_OK)error=esp_http_client_set_header(client,"Content-Type","application/json");if(error!=ESP_OK)goto done;
 bool tls=!strncmp(url,"https://",8);
 unsigned wav_size=record?44+samples*2:0;size_t total=head_size+4*((wav_size+2)/3)+tail_size;
 diagnostic_stage(control,AI_STAGE_CONNECT);stage="TLS connect";memory_stage(stage);
 error=esp_http_client_open(client,(int)total);memset(key,0,sizeof(key));memset(auth,0,sizeof(auth));if(error!=ESP_OK)goto done;
 esp_http_client_set_timeout_ms(client,1000);
 diagnostic_stage(control,AI_STAGE_UPLOAD);stage="upload";
 error=write_all(client,head,head_size,tls,control);if(error!=ESP_OK)goto done;
 free(head);head=NULL; // Retain the small provider suffix until audio is uploaded.
 uint8_t wav[44],pcm[768];char encoded[1024];voice_wav_header(wav,samples);voice_adpcm_t decoder={0};unsigned position=0;
 while(position<wav_size&&!stopped(control)){
  size_t count=wav_size-position;if(count>sizeof(pcm))count=sizeof(pcm);
  for(size_t i=0;i<count;++i,++position){
   if(position<44)pcm[i]=wav[position];
   else{unsigned at=position-44;if(!(at&1)){unsigned sample=at/2;uint8_t n=(voice_buffer_byte(record,sample/2)>>(4*(sample&1)))&15;int16_t value=voice_capture_decode(&decoder,n);pcm[i]=(uint8_t)value;wav[0]=(uint8_t)((uint16_t)value>>8);}else pcm[i]=wav[0];}
  }
  error=write_all(client,encoded,base64(pcm,count,encoded),tls,control);if(error!=ESP_OK)goto done;
  voice_buffer_discard(record,position>44?(position-44)/4:0);
 }
 voice_buffer_release(record);record=NULL;
 if(stopped(control)){error=ESP_ERR_INVALID_STATE;goto done;}
 if(tail_size){error=write_all(client,tail,tail_size,tls,control);if(error!=ESP_OK)goto done;}
 free(head);head=NULL;free(tail);tail=NULL;
 diagnostic_stage(control,AI_STAGE_WAIT);stage="response headers";memory_stage(stage);
 int64_t headers;
 do{headers=esp_http_client_fetch_headers(client);}while(headers==-ESP_ERR_HTTP_EAGAIN&&!stopped(control));
 if(headers<0||stopped(control)){error=ESP_FAIL;goto done;}
 int status=esp_http_client_get_status_code(client);if(status!=200){error=status;goto done;}
 diagnostic_stage(control,AI_STAGE_READ);stage="response buffer";response=malloc(AI_RESPONSE_CAPACITY+1);if(!response){error=ESP_ERR_NO_MEM;goto done;}
 ai_response_filter_t filter;ai_response_filter_init(&filter,response,AI_RESPONSE_CAPACITY+1,adapter->discard_response_string);
 while(!stopped(control)){
  char chunk[512];int n=esp_http_client_read(client,chunk,sizeof(chunk));
  if(n==-ESP_ERR_HTTP_EAGAIN)continue;
  if(n<0){error=ESP_FAIL;goto done;}
  if(!n){if(esp_http_client_is_complete_data_received(client))break;vTaskDelay(pdMS_TO_TICKS(10));continue;}
  if(!ai_response_filter_feed(&filter,chunk,(unsigned)n)){ESP_LOGW(TRANSPORT_TAG,"Response rejected: wire=%u retained=%u",(unsigned)filter.wire,(unsigned)filter.used);error=ESP_ERR_INVALID_SIZE;goto done;}
 }
 if(!ai_response_filter_finish(&filter)){error=ESP_ERR_INVALID_RESPONSE;goto done;}
 ESP_LOGI(TRANSPORT_TAG,"Response envelope: wire=%u retained=%u",(unsigned)filter.wire,(unsigned)filter.used);
 size_t used=filter.used;
 esp_http_client_cleanup(client);client=NULL;
 diagnostic_stage(control,AI_STAGE_PARSE);stage="response parser";memory_stage(stage);
 json=malloc(sizeof(*json));if(!json){error=ESP_ERR_NO_MEM;goto done;}
 response[used]=0;error=!stopped(control)&&adapter->decode(json,response,used,decoded)?ESP_OK:ESP_ERR_INVALID_RESPONSE;
done:
 if(error!=ESP_OK)ESP_LOGW(TRANSPORT_TAG,"Request failed: stage=%s error=%d free=%u largest=%u",stage,error,(unsigned)esp_get_free_heap_size(),(unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
 if(client)esp_http_client_cleanup(client);
 memset(key,0,sizeof(key));memset(auth,0,sizeof(auth));free(json);free(head);free(tail);free(response);voice_buffer_release(record);memory_stage("request cleanup");return error;
}
