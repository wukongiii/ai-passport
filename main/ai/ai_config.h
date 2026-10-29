#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { AI_PROVIDER_COUNT=2, AI_KEY_CAPACITY=257, AI_PROVIDER_NONE=255 };
typedef struct { const char *id,*name,*model,*signup_url; } ai_provider_t;
const ai_provider_t *ai_provider(unsigned index);
int ai_provider_index(const char *id);
typedef struct { uint8_t active; char keys[AI_PROVIDER_COUNT][AI_KEY_CAPACITY]; } ai_config_t;
typedef enum { AI_CONFIG_SAVE,AI_CONFIG_SELECT,AI_CONFIG_REMOVE } ai_config_action_t;
typedef struct { ai_config_action_t action; unsigned provider; char key[AI_KEY_CAPACITY]; } ai_config_request_t;
void ai_config_default(ai_config_t *);
bool ai_config_valid(const ai_config_t *);
bool ai_config_parse(const char *form,ai_config_request_t *);
bool ai_config_apply(ai_config_t *,const ai_config_request_t *);

/* Public snapshots never contain credentials. NVS work is owned by the network
 * worker; UI/HTTP requests only copy into one bounded pending slot. */
typedef struct {
    bool ready,busy,configured[AI_PROVIDER_COUNT];
    unsigned active,revision,submitted,completed;
    int error;
} ai_config_snapshot_t;
void ai_config_snapshot(ai_config_snapshot_t *);
bool ai_config_submit(const ai_config_request_t *,unsigned *request_id);
void ai_config_service_init(void);
void ai_config_service_poll(void);
