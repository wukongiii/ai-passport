#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
/* Bounded UTF-8 text, one replaceable pending message, no heap allocations.
 * The owner serializes calls. Times are monotonic milliseconds. */
enum { SPEECH_TEXT_CAPACITY=385, SPEECH_MERGE_MS=650,
       SPEECH_SEGMENT_CHARS=12, SPEECH_SEGMENT_CAPACITY=SPEECH_SEGMENT_CHARS*4+1 };
typedef struct { char text[SPEECH_TEXT_CAPACITY]; uint64_t due; bool pending; } speech_model_t;
bool speech_model_submit(speech_model_t *,const char *,uint64_t now);
bool speech_model_take(speech_model_t *,uint64_t now,char out[SPEECH_TEXT_CAPACITY]);
void speech_model_cancel(speech_model_t *);
/* Validated UTF-8 input; copy one punctuation-delimited, bounded phrase.
 * Returns bytes consumed, never splits a codepoint. */
size_t speech_model_segment(const char *,char out[SPEECH_SEGMENT_CAPACITY]);
