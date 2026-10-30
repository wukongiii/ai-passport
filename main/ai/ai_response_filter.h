#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Incremental JSON string filter, no allocation. Only the selected field's string
 * values are replaced with ""; all other bytes and duplicate keys are retained
 * for the authoritative adapter parser. Never executes a partially read reply. */
enum { AI_RESPONSE_WIRE_LIMIT=65536 };
typedef struct {
 char *out;size_t capacity,used,wire,key_size;
 const char *discard_field;
 char key[64];
 bool ok,string,escaped,key_ready,reasoning,value_pending,drop;
 unsigned escape,hex_left,utf_left;uint32_t code,high,utf,min;
} ai_response_filter_t;
void ai_response_filter_init(ai_response_filter_t *,char *,size_t,const char *discard_field);
bool ai_response_filter_feed(ai_response_filter_t *,const char *,size_t);
bool ai_response_filter_finish(ai_response_filter_t *);
