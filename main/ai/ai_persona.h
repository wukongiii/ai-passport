#pragma once
#include <stdbool.h>
#include <stddef.h>
/* Independent of model credentials and gameplay. */
enum { AI_PERSONA_NAME_CAPACITY=37,AI_PERSONA_PROMPT_CAPACITY=1537 };
typedef struct {char name[AI_PERSONA_NAME_CAPACITY],prompt[AI_PERSONA_PROMPT_CAPACITY];} ai_persona_t;
typedef struct {ai_persona_t value;unsigned revision,submitted,completed;bool ready,busy;int error;} ai_persona_snapshot_t;
void ai_persona_default(ai_persona_t *);
bool ai_persona_valid(const ai_persona_t *);
const char *ai_persona_default_prompt(void);
void ai_persona_init(void);
void ai_persona_poll(void);
void ai_persona_snapshot(ai_persona_snapshot_t *);
unsigned ai_persona_revision(void);
bool ai_persona_submit(const ai_persona_t *,unsigned expected_revision,unsigned *request);
