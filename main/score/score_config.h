#pragma once
#include "score.h"

/* Product defaults and selectable bounds, separate from match state and UI.
 * Add one entry per new sport; existing saved matches retain their own target.
 * Changing bounds also affects old-record validity, so review compatibility. */
typedef struct {
    uint16_t rule_id;
    uint8_t player_count;
    uint8_t default_target;
    uint8_t min_target;
    uint8_t max_target;
} score_config_t;

const score_config_t *score_config_default(void);
const score_config_t *score_config_find(uint16_t rule_id);
