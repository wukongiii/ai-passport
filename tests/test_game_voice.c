/* Real controller/worker and provider codec; deterministic microphone/HTTPS boundaries. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static int fail_alloc;
static void *voice_malloc(size_t n){if(fail_alloc && !--fail_alloc)return NULL;return malloc(n);}
#define malloc voice_malloc
#include "../main/ai/ai_transport.c"
#include "../main/ai/app_game_voice.c"
#undef malloc
static int64_t now=100;
static ai_config_snapshot_t cfg={.ready=true,.active=0,.configured={true},.revision=1};
static app_network_snapshot_t net={.state=APP_NET_CONNECTED};
static unsigned reads,executes,closes,spoken,headers,read_attempts;
static unsigned revision,names_revision=1;
static int http_status=200;
static esp_err_t audio_error;
static bool silent,cancel_upload,cancel_during_read,lease_owned,send_early=true;
static size_t sent,expected;
static char first[1024];
static const char *response="{\"choices\":[{\"finish_reason\":\"tool_calls\",\"message\":{\"role\":\"assistant\",\"tool_calls\":[{\"type\":\"function\",\"function\":{\"name\":\"adjust_score\",\"arguments\":\"{}\"}}]}}]}";
static bool snap(void *c,ai_game_snapshot_t *s){(void)c;*s=(ai_game_snapshot_t){.revision=revision,.configuration=names_revision,.instructions="Follow game tools"};strcpy(s->context,"{}");strcpy(s->tools,"[]");return true;}
static bool stamp(void *c,uint32_t *r,uint32_t *n){(void)c;*r=revision;*n=names_revision;return true;}
static ai_game_result_t execute(void *c,uint32_t r,uint32_t n,const ai_tool_call_t *call,char *out,size_t cap){(void)c;assert(r==revision&&n==names_revision&&!strcmp(call->name,"adjust_score"));++revision;++executes;snprintf(out,cap,"{\"message\":\"当前比分一比零\"}");return AI_GAME_OK;}
static const ai_game_ops_t ops={snap,stamp,execute};
static ai_game_target_t target={&ops,NULL,1,1};
int64_t esp_timer_get_time(void){return now;}
void vTaskDelay(TickType_t n){now+=(int64_t)n*1000;}
unsigned ulTaskNotifyTake(int clear,TickType_t ticks){(void)clear;(void)ticks;return 0;}
void xTaskNotifyGive(TaskHandle_t h){assert(h);}
int xTaskCreate(void (*f)(void *),const char *name,unsigned size,void *a,unsigned p,TaskHandle_t *h){(void)f;(void)name;(void)size;(void)a;(void)p;*h=(void *)1;return pdPASS;}
const ai_provider_t *ai_provider(unsigned p){static const ai_provider_t a={.id="compatible"};assert(p<AI_PROVIDER_COUNT);return &a;}
void ai_config_snapshot(ai_config_snapshot_t *s){*s=cfg;}
void ai_config_runtime(ai_runtime_snapshot_t *out){out->ready=cfg.ready;out->busy=cfg.busy;out->active=cfg.active;out->generation=cfg.generation;memcpy(out->configured,cfg.configured,sizeof(out->configured));}
bool ai_config_connection(unsigned p,char *key,char *model,char *url,unsigned *gen){assert(p==cfg.active);strcpy(key,"test-only");strcpy(model,"test-model");strcpy(url,"https://example.com/v1/chat/completions");if(gen)*gen=cfg.generation;return true;}
bool ai_check_take(unsigned *p,unsigned *g){(void)p;(void)g;return false;}
void ai_probe_run(unsigned p,unsigned g){(void)p;(void)g;}
void app_network_snapshot(app_network_snapshot_t *s){*s=net;}
void app_speech_stop(void){}
bool app_speech_capture_acquire(void){assert(!lease_owned);lease_owned=true;return true;}
void app_speech_capture_release(void){assert(lease_owned);lease_owned=false;}
bool app_speech_say(const char *s){assert(*s&&!lease_owned);++spoken;return true;}
esp_err_t bsp_audio_init(void){return audio_error;}
esp_err_t bsp_audio_wake(void){return ESP_OK;}
esp_err_t bsp_audio_set_format(uint32_t hz,uint8_t bits,uint8_t ch){assert(hz==16000&&bits==16&&ch==1);return ESP_OK;}
esp_err_t bsp_audio_sleep(void){return ESP_OK;}
esp_err_t bsp_audio_read(void *data,size_t size){
 assert(lease_owned&&size==640);++reads;int16_t *pcm=data;
 for(unsigned i=0;i<320;++i)pcm[i]=silent?0:(int16_t)(5000*sin((reads*320+i)*0.12));
 now+=20000;
 if(cancel_during_read)app_game_voice_cancel();
 else if(send_early&&reads==20)assert(app_game_voice_input(target,1,APP_GESTURE_SHORT));
 return ESP_OK;
}
int esp_crt_bundle_attach(void *p){(void)p;return 0;}
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *c){assert(c->crt_bundle_attach&&c->disable_auto_redirect&&!strcmp(c->url,"https://example.com/v1/chat/completions"));headers=read_attempts=0;sent=0;first[0]=0;return (void *)2;}
esp_err_t esp_http_client_set_header(void *c,const char *k,const char *v){assert(c&&k&&v);return ESP_OK;}
esp_err_t esp_http_client_set_timeout_ms(void *c,int t){assert(c&&t==1000);return ESP_OK;}
esp_err_t esp_http_client_open(void *c,int n){assert(c&&n>0);expected=(unsigned)n;return ESP_OK;}
int esp_http_client_write(void *c,const char *p,int n){assert(c&&p&&n>0);if(!sent)snprintf(first,sizeof(first),"%.*s",n<1023?n:1023,p);sent+=(unsigned)n;if(cancel_upload)app_game_voice_cancel();return n;}
int64_t esp_http_client_fetch_headers(void *c){assert(c);assert(sent==expected);return ++headers<3?-ESP_ERR_HTTP_EAGAIN:(int64_t)strlen(response);}
int esp_http_client_get_status_code(void *c){assert(c);return http_status;}
int esp_http_client_read(void *c,char *p,int n){assert(c);if(++read_attempts==1)return -ESP_ERR_HTTP_EAGAIN;if(read_attempts>2)return 0;assert(n>(int)strlen(response));strcpy(p,response);return (int)strlen(response);}
bool esp_http_client_is_complete_data_received(void *c){assert(c);return true;}
esp_err_t esp_http_client_cleanup(void *c){assert(c);++closes;return ESP_OK;}
static void begin_job(void){reads=0;assert(app_game_voice_input(target,1,APP_GESTURE_DOUBLE));assert(view.state==GAME_VOICE_STARTING&&occupied);}
static void drain(void){app_game_voice_tick(target,now);assert(!occupied&&!session.pending&&!lease_owned&&!job);}
static void codec_tests(void){
 uint8_t encoded[VOICE_CAPACITY+1]={0},header[44];voice_adpcm_t enc={0},dec={0};int16_t pcm[16000];
 for(unsigned i=0;i<16000;++i)pcm[i]=(int16_t)(5000*sin(i*0.12));
 voice_capture_encode(&enc,encoded,pcm,16000);assert(enc.samples==8000&&voice_capture_audible(pcm,16000));
 double error=0;for(unsigned i=0;i<8000;++i){int got=voice_capture_decode(&dec,(encoded[i/2]>>(4*(i&1)))&15);int want=((int)pcm[2*i]+pcm[2*i+1])/2;error+=abs(got-want);}
 assert(error/8000<400);voice_wav_header(header,enc.samples);assert(!memcmp(header,"RIFF",4)&&!memcmp(header+36,"data",4)&&header[24]==0x40&&header[25]==0x1f);
 enc.samples=VOICE_RATE*VOICE_SECONDS;encoded[VOICE_CAPACITY]=0x5a;voice_capture_encode(&enc,encoded,pcm,16000);assert(encoded[VOICE_CAPACITY]==0x5a);
 memset(pcm,0,sizeof(pcm));assert(!voice_capture_audible(pcm,16000));
 char b64[8];assert(base64((const uint8_t *)"ab",2,b64)==4&&!memcmp(b64,"YWI=",4));
}
int main(void){
 codec_tests();assert(app_game_voice_start());
 assert(!app_game_voice_input(target,1,APP_GESTURE_SHORT));assert(!app_game_voice_input(target,1,APP_GESTURE_LONG));
 app_bindings_t b={0};app_game_voice_bindings(target,&b);assert(b.actions[APP_GESTURE_DOUBLE][1].id);
 cfg.configured[0]=false;assert(app_game_voice_input(target,1,APP_GESTURE_DOUBLE));assert(view.state==GAME_VOICE_ERROR&&!occupied);cfg.configured[0]=true;
 net.state=APP_NET_OFFLINE;assert(app_game_voice_input(target,1,APP_GESTURE_DOUBLE)&&!occupied);net.state=APP_NET_CONNECTED;
 begin_job();b.actions[APP_GESTURE_LONG][1]=(app_action_hint_t){"hall",true,1,APP_ACTION_BACK};app_game_voice_bindings(target,&b);assert(b.actions[APP_GESTURE_LONG][1].id==64002);process_job();assert(completed&&occupied);unsigned seq=session.sequence;
 assert(app_game_voice_input(target,1,APP_GESTURE_DOUBLE)&&session.sequence==seq);drain();assert(executes==1&&spoken==1&&headers==3&&read_attempts==3&&strstr(first,"input_audio"));
 cfg.active=2;cfg.configured[2]=true;begin_job();assert(provider==2);cancel_upload=true;process_job();drain();cancel_upload=false;cfg.active=0;
 // A manual score cancels even an already completed reply, before owner consumption.
 begin_job();process_job();assert(!app_game_voice_input(target,0,APP_GESTURE_SHORT));drain();assert(executes==1);
 begin_job();assert(app_game_voice_input(target,1,APP_GESTURE_LONG));process_job();drain();assert(view.state==GAME_VOICE_IDLE&&executes==1);
 begin_job();process_job();++revision;drain();assert(executes==1); // local score race
 begin_job();process_job();++cfg.generation;drain();assert(executes==1); // model change race
 begin_job();process_job();++target.activation;drain();assert(executes==1); // leave/reenter
 begin_job();process_job();now=session.deadline;drain();assert(executes==1);
 fail_alloc=1;assert(app_game_voice_input(target,1,APP_GESTURE_DOUBLE));assert(!occupied&&!job&&!session.pending&&view.error==ESP_ERR_NO_MEM);
 for(int allocation=2;allocation<=7;++allocation){fail_alloc=allocation;begin_job();process_job();drain();assert(view.state==GAME_VOICE_ERROR);}fail_alloc=0;
 audio_error=ESP_FAIL;begin_job();process_job();drain();assert(view.state==GAME_VOICE_ERROR);audio_error=ESP_OK;
 silent=true;begin_job();process_job();drain();assert(view.state==GAME_VOICE_ERROR&&!strcmp(view.text,TXT_GAME_VOICE_EMPTY));silent=false;
 cancel_during_read=true;begin_job();process_job();drain();assert(reads==1&&executes==1);cancel_during_read=false;
 cancel_upload=true;begin_job();process_job();drain();assert(executes==1);cancel_upload=false;
 http_status=401;begin_job();process_job();drain();assert(view.state==GAME_VOICE_ERROR&&view.error==401);http_status=200;
 response="{\"choices\":[{\"finish_reason\":\"stop\",\"message\":{\"role\":\"assistant\",\"content\":\"你好\"}}]}";
 send_early=false;begin_job();process_job();assert(reads==500&&view.milliseconds==10000);drain();assert(executes==1&&view.state==GAME_VOICE_REPLY&&spoken==2);
 response="{\"choices\":[{\"finish_reason\":\"tool_calls\",\"message\":{\"role\":\"assistant\",\"tool_calls\":[{\"type\":\"function\",\"function\":{\"name\":\"ask_clarification\",\"arguments\":\"{\\\"question\\\":\\\"给哪位选手加一分？\\\"}\"}}]}}]}";
 send_early=true;begin_job();process_job();drain();assert(follow_pending&&strstr(followup,"给哪位"));begin_job();assert(strstr(worker_input,"给哪位"));app_game_voice_cancel();process_job();drain();
 puts("Game voice actual worker: routing, audio codec, 10s bound, HTTP framing/retry, reply mailbox, stale/cancel, failures and clarification PASS");
}
