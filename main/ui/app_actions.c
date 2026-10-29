#include "app_actions.h"

void app_actions_clear(app_bindings_t *bindings) {
    *bindings = (app_bindings_t){0};
}

bool app_actions_register(app_bindings_t *b, unsigned slot, app_gesture_t gesture,
                          uint16_t id, const char *label, bool enabled, app_action_role_t role) {
    if (!b || slot >= APP_ACTION_KEY_COUNT || (unsigned)gesture >= APP_GESTURE_COUNT ||
        (unsigned)role >= APP_ACTION_ROLE_COUNT || !id || !label || !*label || b->actions[gesture][slot].id) return false;
    b->actions[gesture][slot] = (app_action_hint_t){label, enabled, id, role};
    return true;
}

uint16_t app_actions_resolve(const app_bindings_t *b, unsigned slot, app_gesture_t gesture) {
    if (!b || slot >= APP_ACTION_KEY_COUNT || (unsigned)gesture >= APP_GESTURE_COUNT) return 0;
    const app_action_hint_t *action = &b->actions[gesture][slot];
    return action->enabled ? action->id : 0;
}

bool app_binding_has_gesture(const app_bindings_t *b, app_gesture_t gesture) {
    if (!b || (unsigned)gesture >= APP_GESTURE_COUNT) return false;
    for (unsigned key = 0; key < APP_ACTION_KEY_COUNT; ++key)
        if (b->actions[gesture][key].label && *b->actions[gesture][key].label) return true;
    return false;
}

unsigned app_binding_rows(const app_bindings_t *b) {
    unsigned rows = 0;
    for (unsigned gesture = 0; gesture < APP_GESTURE_COUNT; ++gesture)
        rows += app_binding_has_gesture(b, (app_gesture_t)gesture);
    return rows ? rows : 1;
}
