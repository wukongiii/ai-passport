#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { AI_PROVIDER_COUNT=2, AI_KEY_CAPACITY=257, AI_MODEL_CAPACITY=97, AI_PROVIDER_NONE=255 };
#define AI_DOUBAO_DEFAULT_MODEL "doubao-seed-2-0-mini-260428"
typedef struct { const char *id,*name,*model,*signup_url; bool available; } ai_provider_t;
const ai_provider_t *ai_provider(unsigned index);
int ai_provider_index(const char *id);
/* Provider slots stay stable. Doubao uses an Ark Key and model identifier.
 * Credentials are never exposed through public snapshots. */
typedef struct {
    uint8_t active;
    char keys[AI_PROVIDER_COUNT][AI_KEY_CAPACITY];
    char ark_model[AI_MODEL_CAPACITY];
} ai_config_t;
typedef enum { AI_CONFIG_SAVE,AI_CONFIG_SELECT,AI_CONFIG_REMOVE } ai_config_action_t;
typedef struct {
    ai_config_action_t action; unsigned provider;
    char key[AI_KEY_CAPACITY],model[AI_MODEL_CAPACITY];
} ai_config_request_t;
void ai_config_default(ai_config_t *);
bool ai_config_valid(const ai_config_t *);
bool ai_config_parse(const char *form,ai_config_request_t *);
bool ai_config_apply(ai_config_t *,const ai_config_request_t *);

/* Network worker owns NVS; UI/HTTP copy bounded requests and public snapshots.
 * Save success means durable completion, not remote credential verification. */
typedef struct {
    bool ready,busy,configured[AI_PROVIDER_COUNT];
    char ark_model[AI_MODEL_CAPACITY];
    unsigned active,revision,submitted,completed;
    int error;
} ai_config_snapshot_t;
void ai_config_snapshot(ai_config_snapshot_t *);
bool ai_config_submit(const ai_config_request_t *,unsigned *request_id);
void ai_config_service_init(void);
void ai_config_service_poll(void);

/* Worker-only bounded copy for transport. Never expose/log this material. */
bool ai_config_credentials(unsigned provider,char key[AI_KEY_CAPACITY],char model[AI_MODEL_CAPACITY]);
