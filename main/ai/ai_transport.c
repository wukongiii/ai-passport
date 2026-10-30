#include "ai_transport.h"
#include "ai_config.h"
#include "ai_provider.h"
#include "voice_capture.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static bool stopped(const ai_transport_control_t *c){return c&&c->cancel&&c->cancel(c->context);}
static bool write_all(esp_http_client_handle_t client,const char *data,size_t size,const ai_transport_control_t *control){
 while(size&&!stopped(control)){int n=esp_http_client_write(client,data,(int)size);if(n<=0)return false;data+=n;size-=(unsigned)n;}return !size&&!stopped(control);
}
static size_t base64(const uint8_t *in,size_t count,char *out){
 static const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";size_t used=0;
 for(size_t i=0;i<count;i+=3){unsigned n=(unsigned)in[i]<<16;if(i+1<count)n|=(unsigned)in[i+1]<<8;if(i+2<count)n|=in[i+2];
 out[used++]=alphabet[(n>>18)&63];out[used++]=alphabet[(n>>12)&63];out[used++]=i+1<count?alphabet[(n>>6)&63]:'=';out[used++]=i+2<count?alphabet[n&63]:'=';}return used;
}
esp_err_t ai_transport_request(unsigned selected,const ai_game_snapshot_t *game,const char *input,uint8_t *record,unsigned samples,ai_reply_t *decoded,const ai_transport_control_t *control){
 esp_err_t error=ESP_FAIL;char key[AI_KEY_CAPACITY]={0},model[AI_MODEL_CAPACITY]={0},auth[AI_KEY_CAPACITY+8]={0},url[AI_URL_CAPACITY]={0};
 if(stopped(control)){free(record);return ESP_ERR_INVALID_STATE;}
 if(!ai_config_connection(selected,key,model,url,NULL)){free(record);return ESP_ERR_INVALID_STATE;}
 if(stopped(control)){memset(key,0,sizeof(key));free(record);return ESP_ERR_INVALID_STATE;}
 const ai_model_adapter_t *adapter=ai_model_adapter(ai_provider(selected)->id);
 ai_json_t *json=malloc(sizeof(*json));char *head=malloc(AI_BODY_CAPACITY),*tail=malloc(4096),*response=NULL;
 esp_http_client_handle_t client=NULL;
 if(!json||!head||!tail){error=ESP_ERR_NO_MEM;goto done;}
 ai_model_request_t request={model,input,game};
 if(!adapter||(record?(!adapter->audio_parts||!adapter->audio_parts(json,&request,head,AI_BODY_CAPACITY,tail,4096)):!adapter->encode(json,&request,head,AI_BODY_CAPACITY))){error=ESP_ERR_INVALID_ARG;goto done;}
 free(json);json=NULL; // Codec workspace is not retained during the TLS handshake.
 esp_http_client_config_t config={.url=url,.method=HTTP_METHOD_POST,.timeout_ms=5000,.crt_bundle_attach=esp_crt_bundle_attach,.buffer_size=1024,.buffer_size_tx=1024,.disable_auto_redirect=true};
 client=esp_http_client_init(&config);if(!client){error=ESP_ERR_NO_MEM;goto done;}
 snprintf(auth,sizeof(auth),"Bearer %s",key);error=esp_http_client_set_header(client,"Authorization",auth);if(error==ESP_OK)error=esp_http_client_set_header(client,"Content-Type","application/json");if(error!=ESP_OK)goto done;
 if(!record)tail[0]=0;
 unsigned wav_size=record?44+samples*2:0;size_t total=strlen(head)+4*((wav_size+2)/3)+strlen(tail);
 error=esp_http_client_open(client,(int)total);memset(key,0,sizeof(key));memset(auth,0,sizeof(auth));if(error!=ESP_OK)goto done;
 esp_http_client_set_timeout_ms(client,1000);
 if(!write_all(client,head,strlen(head),control)){error=ESP_FAIL;goto done;}
 free(head);head=NULL; // Retain the small provider suffix until audio is uploaded.
 uint8_t wav[44],pcm[768];char encoded[1024];voice_wav_header(wav,samples);voice_adpcm_t decoder={0};unsigned position=0;
 while(position<wav_size&&!stopped(control)){
  size_t count=wav_size-position;if(count>sizeof(pcm))count=sizeof(pcm);
  for(size_t i=0;i<count;++i,++position){
   if(position<44)pcm[i]=wav[position];
   else{unsigned at=position-44;if(!(at&1)){unsigned sample=at/2;uint8_t n=(record[sample/2]>>(4*(sample&1)))&15;int16_t value=voice_capture_decode(&decoder,n);pcm[i]=(uint8_t)value;wav[0]=(uint8_t)((uint16_t)value>>8);}else pcm[i]=wav[0];}
  }
  if(!write_all(client,encoded,base64(pcm,count,encoded),control)){error=ESP_FAIL;goto done;}
 }
 free(record);record=NULL;
 if(stopped(control)){error=ESP_ERR_INVALID_STATE;goto done;}
 if(!write_all(client,tail,strlen(tail),control)){error=ESP_FAIL;goto done;}
 free(head);head=NULL;free(tail);tail=NULL;
 int64_t headers;
 do{headers=esp_http_client_fetch_headers(client);}while(headers==-ESP_ERR_HTTP_EAGAIN&&!stopped(control));
 if(headers<0||stopped(control)){error=ESP_FAIL;goto done;}
 response=malloc(AI_RESPONSE_CAPACITY+1);if(!response){error=ESP_ERR_NO_MEM;goto done;}
 int status=esp_http_client_get_status_code(client);if(status!=200){error=status;goto done;}
 size_t used=0;
 while(!stopped(control)){
  int n=esp_http_client_read(client,response+used,AI_RESPONSE_CAPACITY-used);
  if(n==-ESP_ERR_HTTP_EAGAIN)continue;
  if(n<0){error=ESP_FAIL;goto done;}
  if(!n){if(esp_http_client_is_complete_data_received(client))break;vTaskDelay(pdMS_TO_TICKS(10));continue;}
  used+=(unsigned)n;if(used==AI_RESPONSE_CAPACITY){error=ESP_ERR_INVALID_SIZE;goto done;}
 }
 esp_http_client_cleanup(client);client=NULL;
 json=malloc(sizeof(*json));if(!json){error=ESP_ERR_NO_MEM;goto done;}
 response[used]=0;error=!stopped(control)&&adapter->decode(json,response,used,decoded)?ESP_OK:ESP_ERR_INVALID_RESPONSE;
done:
 if(client)esp_http_client_cleanup(client);
 memset(key,0,sizeof(key));memset(auth,0,sizeof(auth));free(json);free(head);free(tail);free(response);free(record);return error;
}
