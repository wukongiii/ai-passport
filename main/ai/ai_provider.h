#pragma once
#include "ai_contract.h"
#include "ai_json.h"

typedef struct { const char *model; const char *input; const ai_game_snapshot_t *game; } ai_model_request_t;
/* Credentials are a transport concern, deliberately excluded from wire bodies.
 * Worker owns a provider instance/workspace and returns one normalized reply. */
typedef struct {
    const char *id;
    bool (*encode)(ai_json_t *workspace,const ai_model_request_t *,char *,size_t);
    /* Optional streaming audio envelope: upload head + Base64 WAV + tail. */
    bool (*audio_parts)(ai_json_t *,const ai_model_request_t *,char *head,size_t,char *tail,size_t);
    bool (*decode)(ai_json_t *workspace,const char *,size_t,ai_reply_t *);
} ai_model_adapter_t;
const ai_model_adapter_t *ai_model_adapter(const char *id);
