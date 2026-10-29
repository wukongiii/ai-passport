#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Bounded JSON reader/writer, independent of model/IDF and without allocation.
 * Nodes borrow input until the caller finishes decoding. Duplicate members,
 * malformed UTF-8/escapes, embedded NUL and excessive nesting are rejected. */
enum { AI_JSON_NODES=192, AI_JSON_DEPTH=12 };
typedef enum { AJ_OBJECT,AJ_ARRAY,AJ_STRING,AJ_NUMBER,AJ_BOOL,AJ_NULL } ai_json_type_t;
typedef struct { ai_json_type_t type; size_t start,end; int next,count; } ai_json_node_t;
typedef struct { const char *text; size_t size; int count; ai_json_node_t nodes[AI_JSON_NODES]; } ai_json_t;
bool ai_json_parse(ai_json_t *,const char *,size_t);
int ai_json_member(const ai_json_t *,int,const char *);
bool ai_json_string(const ai_json_t *,int,char *,size_t);
bool ai_json_integer(const ai_json_t *,int,int64_t *);
bool ai_json_fields(const ai_json_t *,int,const char *const *,unsigned);
bool ai_json_raw(const ai_json_t *,int,char *,size_t);
typedef struct { char *data; size_t size,used; bool ok; } ai_json_writer_t;
void ai_json_writer(ai_json_writer_t *,char *,size_t);
void ai_json_write(ai_json_writer_t *,const char *);
void ai_json_quote(ai_json_writer_t *,const char *);
void ai_json_int(ai_json_writer_t *,int64_t);
