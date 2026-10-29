#pragma once
#include <stdbool.h>

typedef enum { SCORE_INPUT_PRESS, SCORE_INPUT_CLICK, SCORE_INPUT_DOUBLE,
               SCORE_INPUT_LONG, SCORE_INPUT_RELEASE } score_input_event_t;
typedef struct { bool swallowing; bool long_press; } score_input_guard_t;

/* A short PRESS_UP precedes its delayed CLICK. Keep swallowing through that
 * release; a long hold ends at RELEASE because it has no following CLICK. */
static inline bool score_input_consume(score_input_guard_t *g, score_input_event_t event) {
    if (!g->swallowing) return false;
    if (event == SCORE_INPUT_LONG) g->long_press = true;
    if (event == SCORE_INPUT_CLICK || event == SCORE_INPUT_DOUBLE ||
        (event == SCORE_INPUT_RELEASE && g->long_press)) {
        g->swallowing = false;
        g->long_press = false;
    }
    return true;
}
