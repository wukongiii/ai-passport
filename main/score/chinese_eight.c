#include "score.h"
#include "score_config.h"

static bool valid(const score_state_t *s) {
    const score_config_t *config = score_config_find(SCORE_RULE_CHINESE_EIGHT);
    if (s->rule_id != config->rule_id || s->player_count != config->player_count ||
        s->target < config->min_target || s->target > config->max_target || s->points[2] != 0)
        return false;
    for (unsigned i = 0; i < 2; ++i) {
        if (s->points[i] < 0 || s->points[i] > s->target) return false;
    }
    return !(s->points[0] == s->target && s->points[1] == s->target);
}

static int winner(const score_state_t *s) {
    for (unsigned i = 0; i < s->player_count; ++i) {
        if (s->points[i] == s->target) return (int)i;
    }
    return -1;
}

static score_result_t apply(score_state_t *s, const score_command_t *c) {
    if (c->type != SCORE_CMD_AWARD_FRAME && c->type != SCORE_CMD_ADJUST_POINTS)
        return SCORE_UNSUPPORTED;
    if (c->player >= s->player_count ||
        (c->amount != 1 && !(c->type == SCORE_CMD_ADJUST_POINTS && c->amount == -1)))
        return SCORE_INVALID;
    if (c->amount > 0 && winner(s) >= 0) return SCORE_FINISHED;
    if (c->amount < 0 && s->points[c->player] == 0) return SCORE_INVALID;
    s->points[c->player] += c->amount;
    return SCORE_OK;
}

const score_rule_t score_rule_chinese_eight = {
    .id = SCORE_RULE_CHINESE_EIGHT,
    .name = "chinese-eight",
    .valid = valid,
    .apply = apply,
    .winner = winner,
};
