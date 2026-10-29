#pragma once
#include "ai_contract.h"
/* Owner-task state, one request in flight. Workers receive copied snapshots and
 * token, then enqueue a copied normalized reply; they never call game ops.
 * Owner calls finish on dequeue and cancel before navigation/lifecycle changes. */
typedef enum { AI_FLOW_OK,AI_FLOW_BUSY,AI_FLOW_UNAVAILABLE,AI_FLOW_INVALID,AI_FLOW_EXPIRED,AI_FLOW_STALE,AI_FLOW_DUPLICATE } ai_flow_result_t;
typedef struct {
    uint32_t sequence,token,revision,configuration;
    int64_t deadline;
    bool pending;
    ai_game_target_t target;
} ai_session_t;
typedef struct {
    ai_reply_kind_t kind;
    ai_game_result_t game_result;
    bool executed,changed; // Accepted tool and actual revision change, respectively.
    char text[AI_TEXT_CAPACITY];
} ai_completion_t;
/* Snapshot must be a caller-owned bounded mailbox, not an owner stack local.
 * Worker encodes this immutable copy with its own adapter workspace. */
ai_flow_result_t ai_session_begin(ai_session_t *,ai_game_target_t,int64_t now,
    ai_game_snapshot_t *snapshot,uint32_t *token);
ai_flow_result_t ai_session_finish(ai_session_t *,ai_game_target_t,uint32_t token,int64_t now,
    const ai_reply_t *,ai_completion_t *);
void ai_session_cancel(ai_session_t *);
