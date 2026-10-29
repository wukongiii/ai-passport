#include "score_config.h"

static const score_config_t CONFIGS[] = {
    { .rule_id = SCORE_RULE_CHINESE_EIGHT, .player_count = 2,
      .default_target = 10, .min_target = 1, .max_target = 35 },
};

const score_config_t *score_config_default(void) {
    return &CONFIGS[0];
}

const score_config_t *score_config_find(uint16_t rule_id) {
    for (size_t i = 0; i < sizeof(CONFIGS) / sizeof(CONFIGS[0]); ++i)
        if (CONFIGS[i].rule_id == rule_id) return &CONFIGS[i];
    return NULL;
}
