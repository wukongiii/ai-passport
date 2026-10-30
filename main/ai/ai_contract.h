#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The shared boundary has no provider, transport, scoring or UI dependency.
 * Each game supplies JSON context/tools and validates its own tool arguments. */
enum { AI_CONTEXT_CAPACITY=1536, AI_TOOLS_CAPACITY=3072, AI_ARGUMENTS_CAPACITY=256,
       AI_TOOL_NAME_CAPACITY=40, AI_TEXT_CAPACITY=385, AI_BODY_CAPACITY=8192,
       AI_RESPONSE_CAPACITY=6144, AI_INPUT_CAPACITY=512 };
typedef struct {
    uint32_t revision,configuration;
    const char *instructions;
    char context[AI_CONTEXT_CAPACITY];
    char tools[AI_TOOLS_CAPACITY]; // Neutral array: {name,description,parameters(JSON Schema)}.
} ai_game_snapshot_t;
typedef struct { char name[AI_TOOL_NAME_CAPACITY],arguments[AI_ARGUMENTS_CAPACITY]; } ai_tool_call_t;
typedef enum { AI_REPLY_TEXT,AI_REPLY_TOOL,AI_REPLY_CLARIFY } ai_reply_kind_t;
typedef struct { ai_reply_kind_t kind; char text[AI_TEXT_CAPACITY]; ai_tool_call_t call; } ai_reply_t;
typedef enum {
    AI_GAME_OK,AI_GAME_INVALID,AI_GAME_UNSUPPORTED,AI_GAME_STALE,AI_GAME_UNAVAILABLE,
    AI_GAME_FINISHED,AI_GAME_NO_HISTORY
} ai_game_result_t;
/* Owner-task only; instructions have static lifetime. Snapshot context/tools are
 * immutable copies. stamp must include every change affecting interpretation.
 * Reserved common tool ask_clarification({question}) never calls game execute. */
typedef struct { const char *instructions,*tools; } ai_game_definition_t;
typedef struct {
    bool (*snapshot)(void *,ai_game_snapshot_t *);
    bool (*stamp)(void *,uint32_t *revision,uint32_t *configuration);
    ai_game_result_t (*execute)(void *,uint32_t revision,uint32_t configuration,
                                 const ai_tool_call_t *,char *result,size_t capacity);
    /* Immutable metadata, safe for configuration readers without live game state. */
    const ai_game_definition_t *(*definition)(void);
} ai_game_ops_t;

typedef struct { const ai_game_ops_t *ops; void *context; uint16_t module_id; uint32_t activation; } ai_game_target_t;
