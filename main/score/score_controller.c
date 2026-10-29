#include "score_controller.h"
#include "score_strings.h"
#include "score_config.h"

typedef enum {
    ACTION_ADD_A = 1, ACTION_SUB_A, ACTION_ADD_B, ACTION_SUB_B,
    ACTION_SETTINGS, ACTION_PREVIOUS, ACTION_NEXT, ACTION_OPEN, ACTION_BACK,
    ACTION_DECREASE, ACTION_INCREASE, ACTION_CONTINUE, ACTION_NEW,
    ACTION_ROTATE_270, ACTION_ROTATE_90, ACTION_APPLY_DIRECTION
} action_t;

void score_controller_init(score_controller_t *ui, const score_session_t *s) {
    *ui = (score_controller_t){ .page = SCORE_PAGE_BOARD,
        .target = s->state.target, .rotation = APP_ROTATION_270 };
}

void score_controller_bindings(const score_controller_t *ui, const score_session_t *s,
                               app_bindings_t *b) {
    const score_config_t *config = score_config_find(s->state.rule_id);
    app_actions_clear(b);
    if (ui->page == SCORE_PAGE_BOARD) {
        const bool can_add = score_winner(&s->state) < 0;
        app_actions_register(b, 0, APP_GESTURE_SHORT, ACTION_ADD_A, TXT_ADD_ONE, can_add, APP_ACTION_INCREASE);
        app_actions_register(b, 0, APP_GESTURE_LONG, ACTION_SUB_A, TXT_SUB_ONE, s->state.points[0] > 0, APP_ACTION_DECREASE);
        app_actions_register(b, 1, APP_GESTURE_SHORT, ACTION_SETTINGS, TXT_SETTINGS, true, APP_ACTION_NEUTRAL);
        app_actions_register(b, 2, APP_GESTURE_SHORT, ACTION_ADD_B, TXT_ADD_TWO, can_add, APP_ACTION_INCREASE);
        app_actions_register(b, 2, APP_GESTURE_LONG, ACTION_SUB_B, TXT_SUB_TWO, s->state.points[1] > 0, APP_ACTION_DECREASE);
        return;
    }
    app_actions_register(b, 1, APP_GESTURE_LONG, ACTION_BACK, TXT_TARGET_BACK, true, APP_ACTION_BACK);
    switch (ui->page) {
    case SCORE_PAGE_SETTINGS: {
        static const char *labels[] = {TXT_NEW, TXT_UNDO, TXT_ROTATION};
        app_actions_register(b, 0, APP_GESTURE_SHORT, ACTION_PREVIOUS, TXT_PREVIOUS, true, APP_ACTION_NAVIGATE);
        app_actions_register(b, 1, APP_GESTURE_SHORT, ACTION_OPEN, labels[ui->settings_focus],
                             ui->settings_focus != 1 || s->history_count > 0, APP_ACTION_CONFIRM);
        app_actions_register(b, 2, APP_GESTURE_SHORT, ACTION_NEXT, TXT_NEXT, true, APP_ACTION_NAVIGATE);
        break;
    }
    case SCORE_PAGE_TARGET:
        app_actions_register(b, 0, APP_GESTURE_SHORT, ACTION_DECREASE, TXT_DECREASE, ui->target > config->min_target, APP_ACTION_DECREASE);
        app_actions_register(b, 1, APP_GESTURE_SHORT, ACTION_CONTINUE, TXT_CONTINUE, true, APP_ACTION_CONFIRM);
        app_actions_register(b, 2, APP_GESTURE_SHORT, ACTION_INCREASE, TXT_INCREASE, ui->target < config->max_target, APP_ACTION_INCREASE);
        break;
    case SCORE_PAGE_NEW:
        app_actions_register(b, 0, APP_GESTURE_SHORT, ACTION_BACK, TXT_CANCEL, true, APP_ACTION_CANCEL);
        app_actions_register(b, 1, APP_GESTURE_SHORT, ACTION_NEW, TXT_CONFIRM, true, APP_ACTION_CONFIRM);
        app_actions_register(b, 2, APP_GESTURE_SHORT, ACTION_BACK, TXT_TARGET_BACK, true, APP_ACTION_BACK);
        break;
    case SCORE_PAGE_DIRECTION:
        app_actions_register(b, 0, APP_GESTURE_SHORT, ACTION_ROTATE_270, "270°", true, APP_ACTION_NAVIGATE);
        app_actions_register(b, 1, APP_GESTURE_SHORT, ACTION_APPLY_DIRECTION, TXT_CONFIRM, true, APP_ACTION_CONFIRM);
        app_actions_register(b, 2, APP_GESTURE_SHORT, ACTION_ROTATE_90, "90°", true, APP_ACTION_NAVIGATE);
        break;
    default: break;
    }
}

bool score_controller_input(score_controller_t *ui, const score_session_t *s,
                            unsigned slot, app_gesture_t gesture, score_command_t *c) {
    app_bindings_t bindings;
    score_controller_bindings(ui, s, &bindings);
    const uint16_t action = app_actions_resolve(&bindings, slot, gesture);
    *c = (score_command_t){ .expected_revision = s->revision };
    switch (action) {
    case ACTION_ADD_A: case ACTION_SUB_A: case ACTION_ADD_B: case ACTION_SUB_B:
        c->type = SCORE_CMD_ADJUST_POINTS;
        c->player = action == ACTION_ADD_B || action == ACTION_SUB_B ? 1 : 0;
        c->amount = action == ACTION_SUB_A || action == ACTION_SUB_B ? -1 : 1;
        return true;
    case ACTION_SETTINGS:
        ui->settings_focus = 0;
        ui->page = SCORE_PAGE_SETTINGS;
        break;
    case ACTION_PREVIOUS: ui->settings_focus = (ui->settings_focus + 2) % 3; break;
    case ACTION_NEXT: ui->settings_focus = (ui->settings_focus + 1) % 3; break;
    case ACTION_OPEN:
        if (ui->settings_focus == 0) {
            ui->target = score_config_find(s->state.rule_id)->default_target;
            ui->page = SCORE_PAGE_TARGET;
        } else if (ui->settings_focus == 1) {
            c->type = SCORE_CMD_UNDO;
            ui->page = SCORE_PAGE_BOARD;
            return true;
        } else {
            ui->pending_rotation = ui->rotation;
            ui->page = SCORE_PAGE_DIRECTION;
        }
        break;
    case ACTION_BACK:
        ui->page = ui->page == SCORE_PAGE_SETTINGS ? SCORE_PAGE_BOARD : SCORE_PAGE_SETTINGS;
        break;
    case ACTION_DECREASE: --ui->target; break;
    case ACTION_INCREASE: ++ui->target; break;
    case ACTION_CONTINUE: ui->page = SCORE_PAGE_NEW; break;
    case ACTION_NEW:
        c->type = SCORE_CMD_NEW_MATCH;
        c->rule_id = s->state.rule_id;
        c->player_count = s->state.player_count;
        c->target = ui->target;
        ui->page = SCORE_PAGE_BOARD;
        return true;
    case ACTION_ROTATE_270: ui->pending_rotation = APP_ROTATION_270; break;
    case ACTION_ROTATE_90: ui->pending_rotation = APP_ROTATION_90; break;
    case ACTION_APPLY_DIRECTION:
        ui->rotation = ui->pending_rotation;
        ui->page = SCORE_PAGE_SETTINGS;
        break;
    default: break;
    }
    return false;
}
