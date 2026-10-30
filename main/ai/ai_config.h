#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
enum { AI_PROVIDER_COUNT=1, AI_KEY_CAPACITY=257, AI_MODEL_CAPACITY=97, AI_URL_CAPACITY=257, AI_PROVIDER_NONE=255 };
typedef struct { const char *id; } ai_provider_t;
const ai_provider_t *ai_provider(unsigned);
int ai_provider_index(const char *);
typedef struct {uint8_t active;char key[AI_KEY_CAPACITY],model[AI_MODEL_CAPACITY],url[AI_URL_CAPACITY];} ai_config_t;
typedef enum { AI_CONFIG_SAVE,AI_CONFIG_SELECT,AI_CONFIG_REMOVE } ai_config_action_t;
typedef struct { ai_config_action_t action;unsigned provider;char key[AI_KEY_CAPACITY],model[AI_MODEL_CAPACITY],url[AI_URL_CAPACITY]; } ai_config_request_t;
void ai_config_default(ai_config_t *);
bool ai_config_valid(const ai_config_t *);
bool ai_config_parse(const char *,ai_config_request_t *);
bool ai_config_apply(ai_config_t *,const ai_config_request_t *);
bool ai_config_url_valid(const char *);
typedef struct {
 bool ready,busy,configured[AI_PROVIDER_COUNT];
 char model[AI_MODEL_CAPACITY],url[AI_URL_CAPACITY];
 unsigned active,revision,generation,submitted,completed;int error;
} ai_config_snapshot_t;
void ai_config_snapshot(ai_config_snapshot_t *);
bool ai_config_submit(const ai_config_request_t *,unsigned *);
void ai_config_service_init(void);
void ai_config_service_poll(void);
/* One atomic, worker-only copy; never expose Key in UI or logs. */
bool ai_config_connection(unsigned,char key[AI_KEY_CAPACITY],char model[AI_MODEL_CAPACITY],char url[AI_URL_CAPACITY],unsigned *generation);
/* Small shared status vocabulary, used verbatim by device and HTTP clients. */
typedef enum { AI_CHECK_UNTESTED,AI_CHECK_QUEUED,AI_CHECK_TEXT,AI_CHECK_TOOLS,AI_CHECK_AUDIO,AI_CHECK_PASS,AI_CHECK_FAILED,AI_CHECK_STALE } ai_check_state_t;
typedef struct {unsigned revision,provider,generation;ai_check_state_t state;bool text,tools,audio;int error;} ai_check_snapshot_t;
void ai_check_snapshot(ai_check_snapshot_t *);
bool ai_check_request(unsigned);
bool ai_check_take(unsigned *,unsigned *);
void ai_check_publish(unsigned,unsigned,ai_check_state_t,int);
const char *ai_check_label(ai_check_state_t);

const char *ai_check_error_label(int);
