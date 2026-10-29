#include "score.h"
#include "score_config.h"

#include <string.h>

/* Add a rule descriptor here; callers dispatch by stable ID, never by UI index. */
static const score_rule_t *const RULES[] = { &score_rule_chinese_eight };

const score_rule_t *score_rule_find(uint16_t id) {
    for (size_t i = 0; i < sizeof(RULES) / sizeof(RULES[0]); ++i) {
        if (RULES[i]->id == id) return RULES[i];
    }
    return NULL;
}

bool score_state_valid(const score_state_t *s) {
    if (!s || s->player_count < 1 || s->player_count > SCORE_MAX_PLAYERS) return false;
    const score_rule_t *rule = score_rule_find(s->rule_id);
    return rule && rule->valid(s);
}

int score_winner(const score_state_t *s) {
    return score_state_valid(s) ? score_rule_find(s->rule_id)->winner(s) : -1;
}

void score_session_init(score_session_t *s) {
    memset(s, 0, sizeof(*s));
    const score_config_t *config = score_config_default();
    s->state = (score_state_t){ .rule_id = config->rule_id,
        .player_count = config->player_count, .target = config->default_target };
}

static void push_history(score_session_t *s) {
    if (s->history_count == SCORE_HISTORY_CAPACITY) {
        memmove(s->history, s->history + 1, (SCORE_HISTORY_CAPACITY - 1) * sizeof(s->history[0]));
        --s->history_count;
    }
    s->history[s->history_count++] = s->state;
}

score_result_t score_dispatch(score_session_t *s, const score_command_t *c) {
    if (!s || !c || !score_state_valid(&s->state) ||
        s->history_count > SCORE_HISTORY_CAPACITY) return SCORE_INVALID;
    if (c->expected_revision != SCORE_ANY_REVISION && c->expected_revision != s->revision)
        return SCORE_STALE;
    // Reserve the wildcard revision and prevent wraparound from accepting stale commands.
    if (s->revision >= SCORE_ANY_REVISION - 2) return SCORE_INVALID;
    if (c->type == SCORE_CMD_UNDO) {
        if (!s->history_count) return SCORE_NO_HISTORY;
        const score_state_t previous = s->history[s->history_count - 1];
        if (!score_state_valid(&previous)) return SCORE_INVALID;
        s->state = previous;
        --s->history_count;
    } else {
        score_state_t next = s->state;
        if (c->type == SCORE_CMD_NEW_MATCH) {
            next = (score_state_t){ .rule_id = c->rule_id,
                                   .player_count = c->player_count, .target = c->target };
            if (!score_state_valid(&next)) return SCORE_INVALID;
        } else {
            const score_result_t result = score_rule_find(next.rule_id)->apply(&next, c);
            if (result != SCORE_OK) return result;
            if (!score_state_valid(&next)) return SCORE_INVALID;
        }
        push_history(s);
        s->state = next;
    }
    ++s->revision;
    return SCORE_OK;
}

static void put32(uint8_t *p, uint32_t n) {
    for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(n >> (i * 8));
}

static uint32_t get32(const uint8_t *p) {
    uint32_t n = 0;
    for (unsigned i = 0; i < 4; ++i) n |= (uint32_t)p[i] << (i * 8);
    return n;
}

static uint32_t checksum(const uint8_t *p, size_t size) {
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < size; ++i) {
        crc ^= p[i];
        for (unsigned b = 0; b < 8; ++b)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

static void encode_state(uint8_t *p, const score_state_t *s) {
    p[0] = (uint8_t)s->rule_id;
    p[1] = (uint8_t)(s->rule_id >> 8);
    p[2] = s->player_count;
    p[3] = s->target;
    for (unsigned i = 0; i < SCORE_MAX_PLAYERS; ++i) put32(p + 4 + 4 * i, (uint32_t)s->points[i]);
}

static void decode_state(score_state_t *s, const uint8_t *p) {
    *s = (score_state_t){ .rule_id = (uint16_t)(p[0] | (uint16_t)p[1] << 8),
                          .player_count = p[2], .target = p[3] };
    for (unsigned i = 0; i < SCORE_MAX_PLAYERS; ++i) {
        uint32_t n = get32(p + 4 + 4 * i);
        s->points[i] = n <= INT32_MAX ? (int32_t)n : -1 - (int32_t)(UINT32_MAX - n);
    }
}

bool score_encode(const score_session_t *s, uint8_t *p, size_t size) {
    if (!s || !p || size != SCORE_RECORD_SIZE || !score_state_valid(&s->state) ||
        s->history_count > SCORE_HISTORY_CAPACITY || s->revision >= SCORE_ANY_REVISION - 1)
        return false;
    for (unsigned i = 0; i < s->history_count; ++i)
        if (!score_state_valid(&s->history[i])) return false;
    memset(p, 0, size);
    memcpy(p, "SCOR", 4);
    p[4] = 1; // Schema version; never silently reinterpret unknown data.
    p[5] = s->history_count;
    put32(p + 8, s->revision);
    put32(p + 12, SCORE_RECORD_SIZE);
    encode_state(p + 16, &s->state);
    for (unsigned i = 0; i < s->history_count; ++i) encode_state(p + 32 + 16 * i, &s->history[i]);
    put32(p + size - 4, checksum(p, size - 4));
    return true;
}

bool score_decode(score_session_t *s, const uint8_t *p, size_t size) {
    if (!s || !p || size != SCORE_RECORD_SIZE || memcmp(p, "SCOR", 4) ||
        p[4] != 1 || p[5] > SCORE_HISTORY_CAPACITY || p[6] || p[7] ||
        get32(p + 12) != size || get32(p + size - 4) != checksum(p, size - 4) ||
        get32(p + 8) >= SCORE_ANY_REVISION - 1) return false;
    score_session_t next = { .history_count = p[5], .revision = get32(p + 8) };
    decode_state(&next.state, p + 16);
    if (!score_state_valid(&next.state)) return false;
    for (unsigned i = 0; i < next.history_count; ++i) {
        decode_state(&next.history[i], p + 32 + 16 * i);
        if (!score_state_valid(&next.history[i])) return false;
    }
    *s = next;
    return true;
}
