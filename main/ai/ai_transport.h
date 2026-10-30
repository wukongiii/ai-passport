#pragma once
#include "ai_contract.h"
#include "esp_err.h"
#include "voice_buffer.h"
typedef enum { AI_STAGE_PREPARE,AI_STAGE_CONNECT,AI_STAGE_UPLOAD,AI_STAGE_WAIT,AI_STAGE_READ,AI_STAGE_PARSE } ai_transport_stage_t;
/* Optional worker-owned diagnostic output; read only after completion. */
typedef struct {bool (*cancel)(void *);void *context;ai_transport_stage_t *stage;bool use_persona;} ai_transport_control_t;
/* Consumes/releases optional ADPCM blocks on all paths. Buffer struct remains
 * caller-owned. Synchronous worker-only request. */
esp_err_t ai_transport_request(unsigned,const ai_game_snapshot_t *,const char *,voice_buffer_t *audio,unsigned samples,ai_reply_t *,const ai_transport_control_t *);
