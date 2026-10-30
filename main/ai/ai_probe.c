#include "ai_probe.h"
#include "ai_probe_audio.h"
#include "ai_config.h"
#include "ai_transport.h"
#include "ai_json.h"
#include "../network/app_network.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef AI_PROBE_HOST_TEST
extern const unsigned char probe_audio[];
#else
extern const unsigned char probe_audio[] asm("_binary_capability_probe_adpcm_start");
#endif
static ai_game_snapshot_t game;
static ai_reply_t reply;
static struct {unsigned generation;int64_t deadline;} control;
static bool cancelled(void *unused){
 (void)unused;ai_config_snapshot_t c;ai_config_snapshot(&c);
 return c.generation!=control.generation||c.busy||esp_timer_get_time()>=control.deadline;
}
static bool expected_tool(bool audio){
 if(reply.kind!=AI_REPLY_TOOL||strcmp(reply.call.name,audio?"probe_voice":"probe_tool"))return false;
 static ai_json_t j;
 const char *const fields[]={audio?"player":"value"};char value[24];
 return ai_json_parse(&j,reply.call.arguments,strlen(reply.call.arguments))&&ai_json_fields(&j,0,fields,1)&&
  ai_json_string(&j,ai_json_member(&j,0,fields[0]),value,sizeof(value))&&!strcmp(value,audio?"left":"ping");
}
void ai_probe_run(unsigned provider,unsigned generation){
 control.generation=generation;control.deadline=esp_timer_get_time()+60000000;
 app_network_snapshot_t network;app_network_snapshot(&network);int error=ESP_OK;
 if(network.state!=APP_NET_CONNECTED){error=ESP_ERR_INVALID_STATE;goto failed;}
 int64_t clock_deadline=esp_timer_get_time()+5000000;
 while(time(NULL)<1700000000&&!cancelled(NULL)&&esp_timer_get_time()<clock_deadline)vTaskDelay(pdMS_TO_TICKS(100));
 if(time(NULL)<1700000000||cancelled(NULL)){error=ESP_ERR_TIMEOUT;goto failed;}
 ai_transport_control_t c={cancelled,NULL};
 memset(&game,0,sizeof(game));strcpy(game.context,"{}");strcpy(game.tools,"[]");
 game.instructions="This is an isolated API capability test. Reply exactly pong. Do not call any tool.";
 error=ai_transport_request(provider,&game,"Reply exactly pong.",NULL,0,&reply,&c);
 if(cancelled(NULL)&&!error)error=ESP_ERR_INVALID_STATE;
 if(error||reply.kind!=AI_REPLY_TEXT||strcmp(reply.text,"pong")){if(!error)error=ESP_ERR_INVALID_RESPONSE;goto failed;}
 ai_check_publish(provider,generation,AI_CHECK_TOOLS,0);
 game.instructions="Isolated capability test. Call probe_tool with value ping exactly once. Do not answer in prose.";
 strcpy(game.tools,"[{\"name\":\"probe_tool\",\"description\":\"Report a test token; no real-world action.\",\"parameters\":{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"string\",\"enum\":[\"ping\"]}},\"required\":[\"value\"],\"additionalProperties\":false}}]");
 error=ai_transport_request(provider,&game,"Call probe_tool with value ping.",NULL,0,&reply,&c);
 if(cancelled(NULL)&&!error)error=ESP_ERR_INVALID_STATE;
 if(error||!expected_tool(false)){if(!error)error=ESP_ERR_INVALID_RESPONSE;goto failed;}
 ai_check_publish(provider,generation,AI_CHECK_AUDIO,0);
 game.instructions="Isolated audio capability test. Listen to the audio. Call probe_voice once, setting player to left or right according to the spoken player. No gameplay is executed. Do not answer in prose.";
 strcpy(game.tools,"[{\"name\":\"probe_voice\",\"description\":\"Report which player is named in the audio. No score change.\",\"parameters\":{\"type\":\"object\",\"properties\":{\"player\":{\"type\":\"string\",\"enum\":[\"left\",\"right\"]}},\"required\":[\"player\"],\"additionalProperties\":false}}]");
 uint8_t *audio=malloc((AI_PROBE_SAMPLES+1)/2);if(!audio){error=ESP_ERR_NO_MEM;goto failed;}
 memcpy(audio,probe_audio,(AI_PROBE_SAMPLES+1)/2);
 error=ai_transport_request(provider,&game,"Determine the player from the attached speech and report it with probe_voice.",audio,AI_PROBE_SAMPLES,&reply,&c);
 if(cancelled(NULL)&&!error)error=ESP_ERR_INVALID_STATE;
 if(error||!expected_tool(true)){if(!error)error=ESP_ERR_INVALID_RESPONSE;goto failed;}
 if(cancelled(NULL)){error=ESP_ERR_TIMEOUT;goto failed;}
 ai_check_publish(provider,generation,AI_CHECK_PASS,0);return;
failed:
 ai_check_publish(provider,generation,AI_CHECK_FAILED,error);
}
