#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#define AI_PROBE_HOST_TEST 1
#include "../main/ai/ai_probe.c"
const unsigned char probe_audio[(AI_PROBE_SAMPLES+1)/2]={0};
static unsigned generation=2,requests;
static int fail_stage,wrong_audio;
static int64_t now=10;
static ai_check_snapshot_t result;
int64_t esp_timer_get_time(void){return now;}
void vTaskDelay(TickType_t t){now+=(int64_t)t*1000;}
void ai_config_snapshot(ai_config_snapshot_t *out){memset(out,0,sizeof(*out));out->generation=generation;}
void app_network_snapshot(app_network_snapshot_t *out){memset(out,0,sizeof(*out));out->state=APP_NET_CONNECTED;}
void ai_check_publish(unsigned p,unsigned g,ai_check_state_t state,int error){assert(p==0&&g==2);result.state=state;result.error=error;if(state==AI_CHECK_TOOLS)result.text=true;if(state==AI_CHECK_AUDIO)result.tools=true;if(state==AI_CHECK_PASS)result.audio=true;}
esp_err_t ai_transport_request(unsigned p,const ai_game_snapshot_t *g,const char *input,uint8_t *audio,unsigned samples,ai_reply_t *out,const ai_transport_control_t *c){
 assert(p==0&&g&&input&&!c->cancel(c->context));++requests;*out=(ai_reply_t){0};
 if(audio){assert(samples==AI_PROBE_SAMPLES&&strstr(g->tools,"probe_voice")&&!strstr(input,"left"));out->kind=AI_REPLY_TOOL;strcpy(out->call.name,"probe_voice");strcpy(out->call.arguments,wrong_audio?"{\"player\":\"right\"}":"{\"player\":\"left\"}");free(audio);}
 else if(requests==1){assert(!strcmp(g->tools,"[]"));out->kind=AI_REPLY_TEXT;strcpy(out->text,"pong");}
 else {out->kind=AI_REPLY_TOOL;strcpy(out->call.name,"probe_tool");strcpy(out->call.arguments,"{\"value\":\"ping\"}");}
 if(fail_stage==(int)requests)return 401;
 if(fail_stage==4)++generation;
 return ESP_OK;
}
static void reset(void){requests=0;generation=2;result=(ai_check_snapshot_t){0};}
int main(void){
 reset();ai_probe_run(0,2);assert(requests==3&&result.state==AI_CHECK_PASS&&result.text&&result.tools&&result.audio);
 for(fail_stage=1;fail_stage<=3;++fail_stage){reset();ai_probe_run(0,2);assert(requests==(unsigned)fail_stage&&result.state==AI_CHECK_FAILED&&result.error==401);}
 fail_stage=0;wrong_audio=1;reset();ai_probe_run(0,2);assert(result.state==AI_CHECK_FAILED&&!result.audio&&result.tools);
 wrong_audio=0;fail_stage=4;reset();ai_probe_run(0,2);assert(result.state==AI_CHECK_FAILED&&requests==1);
 puts("Isolated model probe: three real stages, wrong audio, partial failure, stale configuration PASS");
}
