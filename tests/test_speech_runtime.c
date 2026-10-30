/* Execute the actual speech playback path with a sleeping, stateful codec.
 * The regression must fail before the explicit wake is added. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../main/speech/app_speech.c"
const unsigned char voice_data[1]={0};
const esp_tts_voice_t esp_tts_voice_template={0};
static esp_tts_voice_t fake_voice;
static bool sleeping=true;
static int wakes,writes,sleeps,phase,creates,destroys,parses;
static bool interrupt_write,change_volume,disable_write;
static unsigned applied_volume;
static esp_err_t wake_error;
static size_t free_heap=100000,largest_block=90000;
size_t esp_get_free_heap_size(void){return free_heap;}
size_t heap_caps_get_largest_free_block(unsigned c){(void)c;return largest_block;}
int64_t esp_timer_get_time(void){return 1000000;}
void vTaskDelay(TickType_t t){(void)t;}
unsigned uxTaskGetStackHighWaterMark(TaskHandle_t t){(void)t;return 4000;}
unsigned ulTaskNotifyTake(int clear,TickType_t ticks){(void)clear;(void)ticks;return 0;}
void xTaskNotifyGive(TaskHandle_t t){(void)t;}
int xTaskCreate(void (*fn)(void *),const char *n,unsigned s,void *a,unsigned p,TaskHandle_t *out){(void)fn;(void)n;(void)s;(void)a;(void)p;*out=(void *)1;return pdPASS;}
esp_err_t bsp_audio_init(void){return ESP_OK;}
esp_err_t bsp_audio_wake(void){++wakes;if(wake_error)return wake_error;sleeping=false;return ESP_OK;}
esp_err_t bsp_audio_set_format(uint32_t hz,uint8_t bits,uint8_t ch){assert(hz==16000 && bits==16 && ch==1);return sleeping ? ESP_ERR_INVALID_STATE : ESP_OK;}
void bsp_audio_set_volume(uint8_t v){assert(!sleeping && v<=100);applied_volume=v;}
esp_err_t bsp_audio_write(const void *p,size_t n){assert(!sleeping && p && n==640);++writes;if(change_volume){app_speech_set_volume(25);change_volume=false;}if(interrupt_write)app_speech_stop();if(disable_write)app_speech_set_enabled(false);return ESP_OK;}
esp_err_t bsp_audio_sleep(void){sleeping=true;++sleeps;return ESP_OK;}
esp_err_t bsp_audio_release(void){return bsp_audio_sleep();}
esp_tts_voice_t *esp_tts_voice_set_init(const esp_tts_voice_t *t,void *d){assert(t==&esp_tts_voice_template && d==(void *)voice_data);return &fake_voice;}
void esp_tts_voice_set_free(esp_tts_voice_t *v){assert(v==&fake_voice);}
esp_tts_handle_t esp_tts_create(esp_tts_voice_t *v){assert(v==&fake_voice);++creates;return v;}
int esp_tts_parse_chinese(esp_tts_handle_t t,const char *s){assert(t && *s&&strlen(s)<SPEECH_SEGMENT_CAPACITY);++parses;phase=0;return 1;}
short *esp_tts_stream_play(esp_tts_handle_t t,int *n,unsigned speed){static short pcm[320];assert(t && speed==0);*n=phase++ ? 0 : 320;return pcm;}
void esp_tts_stream_reset(esp_tts_handle_t t){assert(t);}
void esp_tts_destroy(esp_tts_handle_t t){assert(t);++destroys;}
int main(void){
 assert(sleeping);
 free_heap=51104;largest_block=43008;assert(speak("三比二",0)==ESP_ERR_NO_MEM&&wakes==0&&creates==0);
 free_heap=100000;largest_block=16383;assert(speak("三比二",0)==ESP_ERR_NO_MEM&&wakes==0&&creates==0);
 // Reproduce post-HTTPS fragmentation: the native-speed path needs no 40KB block.
 largest_block=34816;free_heap=84096;
 assert(speak("三比二",0)==ESP_OK);assert(sleeping && wakes==1 && writes==1 && sleeps==1);
 assert(speak("四比二",0)==ESP_OK);assert(sleeping && wakes==2 && writes==2 && sleeps==2);
 assert(creates==2 && destroys==2);
 assert(applied_volume==APP_VOLUME_DEFAULT);
 app_speech_set_volume(0);int muted=writes;assert(speak("三比二",0)==ESP_OK&&writes==muted);
 app_speech_set_volume(40);change_volume=true;
 assert(speak("这是第一段比较长的回答，需要分段播放。",0)==ESP_OK&&applied_volume==25);
 app_speech_set_volume(APP_VOLUME_DEFAULT);
 int baseline_writes=writes,baseline_creates=creates;
 wake_error=ESP_FAIL;assert(speak("五比二",0)==ESP_FAIL);assert(sleeping && writes==baseline_writes && creates==baseline_creates);
 wake_error=ESP_OK;assert(speak("五比二",0)==ESP_OK);assert(sleeping && writes==baseline_writes+1 && creates==baseline_creates+1 && destroys==creates);
 // Long text is synthesized phrase by phrase, cancellation stops before the
 // next phrase and prevents any queued continuation.
 const char *long_text="这是第一段比较长的回答，需要分段播放。这是第二段回答。";
 int before=parses;assert(speak(long_text,0)==ESP_OK);assert(parses>before+1);
 before=parses;interrupt_write=true;unsigned epoch=cancel_epoch;
 assert(speak(long_text,epoch)==ESP_OK&&parses==before+1);interrupt_write=false;
 assert(!model.pending);
 before=parses;disable_write=true;epoch=cancel_epoch;
 assert(speak(long_text,epoch)==ESP_OK&&parses==before+1&&cancelled(epoch)&&!view.enabled);
 disable_write=false;app_speech_set_enabled(true);
 assert(app_speech_start());
 assert(app_speech_capture_acquire());assert(!app_speech_capture_acquire());
 assert(!app_speech_say("三比二"));app_speech_capture_release();
 assert(app_speech_say("三比二"));assert(!app_speech_capture_acquire());
 app_speech_stop();assert(app_speech_capture_acquire());app_speech_capture_release();
 view.state=SPEECH_PLAYING;assert(!app_speech_capture_acquire());
 view.state=SPEECH_STOPPING;assert(!app_speech_capture_acquire());
 // Master off cancels active and pending work, refuses new speech, keeps volume.
 view.state=SPEECH_PLAYING;assert(app_speech_say("二比一"));
 unsigned previous_epoch=cancel_epoch;app_speech_set_enabled(false);
 assert(!view.enabled&&!model.pending&&!view.pending&&view.state==SPEECH_STOPPING&&cancel_epoch!=previous_epoch);
 assert(!app_speech_say("三比一")&&cancelled(previous_epoch)&&view.volume==APP_VOLUME_DEFAULT);
 view.state=SPEECH_IDLE;assert(app_speech_capture_acquire());app_speech_capture_release();
 app_speech_set_enabled(true);assert(view.enabled&&!model.pending&&app_speech_say("三比一"));
 app_speech_stop();
 puts("Speech sleeping-codec playback, replay, wake failure and recovery: PASS");
}
