#pragma once
#include <stdbool.h>
#include <stdint.h>
/* Bounded UTF-8 text, one replaceable pending message, no heap allocations.
 * The owner serializes calls. Times are monotonic milliseconds. */
enum { SPEECH_TEXT_CAPACITY=121, SPEECH_MERGE_MS=650 };
typedef struct { char text[SPEECH_TEXT_CAPACITY]; uint64_t due; bool pending; } speech_model_t;
bool speech_model_submit(speech_model_t *,const char *,uint64_t now);
bool speech_model_take(speech_model_t *,uint64_t now,char out[SPEECH_TEXT_CAPACITY]);
void speech_model_cancel(speech_model_t *);
