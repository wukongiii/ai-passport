#pragma once
#include "score.h"
#include "../ui/app_layout.h"
#include "../ui/app_actions.h"

typedef enum {
    SCORE_PAGE_BOARD, SCORE_PAGE_SETTINGS, SCORE_PAGE_TARGET,
    SCORE_PAGE_NEW, SCORE_PAGE_DIRECTION, SCORE_PAGE_COUNT
} score_page_t;
typedef struct {
    score_page_t page;
    uint8_t settings_focus; // New match, Undo, Direction
    app_rotation_t rotation;
    app_rotation_t pending_rotation;
    uint8_t target;
} score_controller_t;

void score_controller_init(score_controller_t *ui, const score_session_t *session);
/* Labels, enablement and dispatch share the same registered action IDs.
 * Owner executes/persists returned commands. Board keys always address A/B. */
void score_controller_bindings(const score_controller_t *ui, const score_session_t *session,
                               app_bindings_t *bindings);
bool score_controller_input(score_controller_t *ui, const score_session_t *session,
                            unsigned slot, app_gesture_t gesture, score_command_t *command);
