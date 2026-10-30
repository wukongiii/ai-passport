#pragma once
#include "ai_contract.h"
#include "esp_err.h"
typedef struct {bool (*cancel)(void *);void *context;} ai_transport_control_t;
/* Owns/frees optional ADPCM buffer on all paths. Synchronous worker-only request. */
esp_err_t ai_transport_request(unsigned,const ai_game_snapshot_t *,const char *,uint8_t *audio,unsigned samples,ai_reply_t *,const ai_transport_control_t *);
