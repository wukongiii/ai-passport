#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Independent, allocation-free action registry. Slots always mean visual
 * left/middle/right; hardware identity is resolved by app_layout beforehand.
 * Label strings must outlive the registry. ID zero means no action. */
enum { APP_ACTION_KEY_COUNT = 3 };
typedef enum { APP_GESTURE_SHORT, APP_GESTURE_LONG, APP_GESTURE_DOUBLE, APP_GESTURE_COUNT } app_gesture_t;
/* Shared semantic roles select the palette; modules never choose raw colors. */
typedef enum {
    APP_ACTION_NEUTRAL, APP_ACTION_CONFIRM, APP_ACTION_CANCEL, APP_ACTION_BACK,
    APP_ACTION_INCREASE, APP_ACTION_DECREASE, APP_ACTION_NAVIGATE, APP_ACTION_ROLE_COUNT
} app_action_role_t;
typedef struct { const char *label; bool enabled; uint16_t id; app_action_role_t role; } app_action_hint_t;
typedef struct { app_action_hint_t actions[APP_GESTURE_COUNT][APP_ACTION_KEY_COUNT]; } app_bindings_t;
void app_actions_clear(app_bindings_t *bindings);
bool app_actions_register(app_bindings_t *bindings, unsigned slot, app_gesture_t gesture,
                          uint16_t id, const char *label, bool enabled, app_action_role_t role);
uint16_t app_actions_resolve(const app_bindings_t *bindings, unsigned slot, app_gesture_t gesture);
unsigned app_binding_rows(const app_bindings_t *bindings);
bool app_binding_has_gesture(const app_bindings_t *bindings, app_gesture_t gesture);
