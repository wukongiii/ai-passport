#include "app_speech.h"
#include "bsp_audio.h"
#include "esp_tts.h"
#include "esp_tts_voice_template.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
static const char *TAG="app_speech";
static portMUX_TYPE lock=portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t worker;
static speech_model_t model;
static app_speech_snapshot_t view={.volume=65};
static unsigned cancel_epoch;
static bool capture_owned;
extern const unsigned char voice_data[] asm("_binary_esp_tts_voice_data_xiaoxin_small_dat_start");
static bool cancelled(unsigned epoch){portENTER_CRITICAL(&lock);bool yes=epoch!=cancel_epoch;portEXIT_CRITICAL(&lock);return yes;}
static esp_err_t speak(const char *text,unsigned epoch){
    // The vendor's prebuilt routines assume their small allocations succeed.
    // Leave headroom for AMR decoding, codec DMA and networking.
    if(esp_get_free_heap_size()<70000 || heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)<40000)return ESP_ERR_NO_MEM;
    esp_err_t error=bsp_audio_init();
    if(error==ESP_OK)error=bsp_audio_wake();
    if(error==ESP_OK)error=bsp_audio_set_format(16000,16,1);
    if(error!=ESP_OK)return error;
    bsp_audio_set_volume(65);
    esp_tts_voice_t *voice=esp_tts_voice_set_init(&esp_tts_voice_template,(void *)voice_data);
    esp_tts_handle_t tts=voice ? esp_tts_create(voice) : NULL;
    if(!tts)error=ESP_ERR_NO_MEM;
    else if(!cancelled(epoch)){
        unsigned samples=0;size_t offset=0;
        const int64_t deadline=esp_timer_get_time()+30000000;
        while(text[offset]&&!cancelled(epoch)&&error==ESP_OK){
            char phrase[SPEECH_SEGMENT_CAPACITY];
            size_t size=speech_model_segment(text+offset,phrase);offset+=size;
            if(!size||esp_timer_get_time()>deadline){error=ESP_FAIL;break;}
            if(!esp_tts_parse_chinese(tts,phrase)){error=ESP_ERR_INVALID_ARG;break;}
            while(!cancelled(epoch)){
                // Speed zero streams original voice samples, bypassing the
                // vendor's CPU-heavy stretcher (observed task-watchdog PC).
                int count=0;short *pcm=esp_tts_stream_play(tts,&count,0);
                if(count==0)break;
                if(!pcm || count<0 || count>16000 || esp_timer_get_time()>deadline){error=ESP_FAIL;break;}
                for(int at=0;at<count && !cancelled(epoch);){
                    int n=count-at;if(n>320)n=320;
                    error=bsp_audio_write(pcm+at,(size_t)n*sizeof(*pcm));
                    if(error!=ESP_OK)break;
                    samples+=(unsigned)n;at+=n;
                }
                if(error!=ESP_OK)break;
                vTaskDelay(1);
            }
            esp_tts_stream_reset(tts);
            vTaskDelay(1);
        }
        ESP_LOGI(TAG,"Synthesis finished: samples=%u free=%u largest=%u stack=%u",samples,
            (unsigned)esp_get_free_heap_size(),(unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
            (unsigned)uxTaskGetStackHighWaterMark(NULL));
        if(!samples && !cancelled(epoch) && error==ESP_OK)error=ESP_FAIL;
    }
    if(tts){esp_tts_stream_reset(tts);esp_tts_destroy(tts);}
    if(voice)esp_tts_voice_set_free(voice);
    // Drain the short DMA tail; a stop may discard it. No UI references here.
    if(!cancelled(epoch) && error==ESP_OK)vTaskDelay(pdMS_TO_TICKS(100));
    esp_err_t sleep=bsp_audio_sleep();if(error==ESP_OK)error=sleep;
    return error;
}
static void run(void *arg){
    (void)arg;
    for(;;){
        char text[SPEECH_TEXT_CAPACITY];unsigned epoch;
        portENTER_CRITICAL(&lock);
        bool have=!capture_owned && speech_model_take(&model,(uint64_t)esp_timer_get_time()/1000,text);
        epoch=cancel_epoch;
        if(have){view.state=SPEECH_PLAYING;view.pending=false;view.error=0;++view.revision;}
        portEXIT_CRITICAL(&lock);
        if(!have){ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(25));continue;}
        ESP_LOGI(TAG,"Synthesis started: free=%u largest=%u",(unsigned)esp_get_free_heap_size(),(unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        esp_err_t error=speak(text,epoch);memset(text,0,sizeof(text));
        portENTER_CRITICAL(&lock);
        view.pending=model.pending;
        view.state=model.pending ? SPEECH_WAITING : epoch!=cancel_epoch ? SPEECH_IDLE : error==ESP_OK ? SPEECH_DONE : SPEECH_FAILED;
        view.error=error;++view.revision;
        portEXIT_CRITICAL(&lock);
        if(error!=ESP_OK)ESP_LOGW(TAG,"Speech failed: %s",esp_err_to_name(error));
    }
}
bool app_speech_start(void){
    // Called only from the application owner, after BSP startup.
    if(worker)return true;
    if(xTaskCreate(run,"speech",8192,NULL,4,&worker)==pdPASS)return true;
    worker=NULL;portENTER_CRITICAL(&lock);view.state=SPEECH_FAILED;view.error=ESP_ERR_NO_MEM;++view.revision;portEXIT_CRITICAL(&lock);return false;
}
bool app_speech_say(const char *text){
    if(!worker)return false;
    portENTER_CRITICAL(&lock);bool ok=!capture_owned && speech_model_submit(&model,text,(uint64_t)esp_timer_get_time()/1000);
    if(ok){view.pending=true;if(view.state!=SPEECH_PLAYING && view.state!=SPEECH_STOPPING)view.state=SPEECH_WAITING;view.error=0;++view.revision;}
    portEXIT_CRITICAL(&lock);if(ok)xTaskNotifyGive(worker);return ok;
}
void app_speech_stop(void){
    portENTER_CRITICAL(&lock);speech_model_cancel(&model);++cancel_epoch;view.pending=false;
    view.state=view.state==SPEECH_PLAYING || view.state==SPEECH_STOPPING ? SPEECH_STOPPING : SPEECH_IDLE;view.error=0;++view.revision;
    portEXIT_CRITICAL(&lock);if(worker)xTaskNotifyGive(worker);
}
void app_speech_snapshot(app_speech_snapshot_t *out){portENTER_CRITICAL(&lock);*out=view;portEXIT_CRITICAL(&lock);}

bool app_speech_capture_acquire(void){
 portENTER_CRITICAL(&lock);bool ok=!capture_owned && !model.pending && view.state!=SPEECH_PLAYING && view.state!=SPEECH_STOPPING;
 if(ok)capture_owned=true;
 portEXIT_CRITICAL(&lock);return ok;
}
void app_speech_capture_release(void){portENTER_CRITICAL(&lock);capture_owned=false;portEXIT_CRITICAL(&lock);}
