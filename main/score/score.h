#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SCORE_MAX_PLAYERS 3
#define SCORE_HISTORY_CAPACITY 32
#define SCORE_RULE_CHINESE_EIGHT 1
#define SCORE_ANY_REVISION UINT32_MAX

typedef struct {
    uint16_t rule_id;
    uint8_t player_count;
    uint8_t target;
    int32_t points[SCORE_MAX_PLAYERS];
} score_state_t;

typedef enum {
    SCORE_CMD_AWARD_FRAME = 1,
    SCORE_CMD_ADJUST_POINTS,
    SCORE_CMD_UNDO,
    SCORE_CMD_NEW_MATCH,
} score_command_type_t;

/* A future speech adapter resolves player identity before enqueueing a command.
 * expected_revision rejects delayed responses after manual changes. No adapter
 * may modify state directly. Network deduplication belongs in that adapter. */
typedef struct {
    score_command_type_t type;
    uint8_t player;
    int32_t amount;
    uint16_t rule_id;
    uint8_t player_count;
    uint8_t target;
    uint32_t expected_revision;
} score_command_t;

typedef enum {
    SCORE_OK = 0,
    SCORE_INVALID,
    SCORE_UNSUPPORTED,
    SCORE_FINISHED,
    SCORE_NO_HISTORY,
    SCORE_STALE,
} score_result_t;

/* Rules own sport-specific validity and scoring, independent of UI/ESP-IDF.
 * Extend the state/schema deliberately if a later sport needs ball/turn state. */
typedef struct {
    uint16_t id;
    const char *name;
    bool (*valid)(const score_state_t *state);
    score_result_t (*apply)(score_state_t *state, const score_command_t *command);
    int (*winner)(const score_state_t *state);
} score_rule_t;

typedef struct {
    score_state_t state;
    score_state_t history[SCORE_HISTORY_CAPACITY];
    uint8_t history_count;
    uint32_t revision;
} score_session_t;

const score_rule_t *score_rule_find(uint16_t id);
extern const score_rule_t score_rule_chinese_eight;
bool score_state_valid(const score_state_t *state);
void score_session_init(score_session_t *session);
score_result_t score_dispatch(score_session_t *session, const score_command_t *command);
int score_winner(const score_state_t *state);

/* Fixed-width little-endian schema, independent of compiler struct padding.
 * Includes bounded undo history so corrections survive a restart. */
#define SCORE_RECORD_SIZE (16 + (SCORE_HISTORY_CAPACITY + 1) * 16 + 4)
bool score_encode(const score_session_t *session, uint8_t *record, size_t size);
bool score_decode(score_session_t *session, const uint8_t *record, size_t size);
