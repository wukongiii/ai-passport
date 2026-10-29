#include "eight_module.h"
#include "../app/app_strings.h"
#include "../score/score_controller.h"
#include "../score/score_store.h"
#include "../score/score_strings.h"
#include "../score/score_ui.h"

static struct {
    score_session_t session;
    score_controller_t controller;
    bool loaded, dirty, preserve_unknown;
    int64_t last_save;
    const char *status, *storage;
} state;

/* Storage bootstrap runs once outside the LVGL lock. The old NVS namespace and
 * schema are retained. Entry never starts a new match or discards history. */
void eight_module_init(void) {
    if (state.loaded) return;
    score_session_init(&state.session);
    const score_store_status_t store = score_store_open(&state.session);
    state.preserve_unknown = store == SCORE_STORE_INVALID || store == SCORE_STORE_UNAVAILABLE;
    state.storage = store == SCORE_STORE_UNAVAILABLE ? TXT_STORE_FAILED :
        store == SCORE_STORE_INVALID ? TXT_RECORD_BAD : store == SCORE_STORE_LOADED ? TXT_SAVED : "";
    state.status = store == SCORE_STORE_LOADED ? TXT_RESTORED : TXT_EMPTY;
    state.loaded = true;
}

static bool enter(void *context, app_rotation_t rotation) {
    (void)context;
    score_controller_init(&state.controller, &state.session);
    state.controller.rotation = rotation;
    return true;
}
static bool leave(void *context) {
    (void)context;
    // No workers, timers or callbacks own module widgets. Pending storage is
    // retained and serviced by tick even when this module is not visible.
    return true;
}
static void bindings(void *context, app_bindings_t *b) {
    (void)context;
    score_controller_bindings(&state.controller, &state.session, b);
}
static app_module_effect_t input(void *context, app_rotation_t rotation,
                                 unsigned slot, app_gesture_t gesture) {
    (void)context;
    state.controller.rotation = rotation;
    score_command_t c;
    if (score_controller_input(&state.controller, &state.session, slot, gesture, &c)) {
        const score_result_t result = score_dispatch(&state.session, &c);
        state.status = result == SCORE_NO_HISTORY ? TXT_NO_HISTORY : result != SCORE_OK ? TXT_REJECTED :
            c.type == SCORE_CMD_UNDO ? TXT_UNDONE : c.type == SCORE_CMD_NEW_MATCH ? TXT_NEW_STARTED :
            c.amount < 0 ? (c.player ? TXT_SUBBED_TWO : TXT_SUBBED_ONE) :
            (c.player ? TXT_AWARDED_TWO : TXT_AWARDED_ONE);
        if (result == SCORE_OK) {
            if (c.type == SCORE_CMD_NEW_MATCH) state.preserve_unknown = false;
            state.dirty = true;
            state.last_save = 0;
        }
    }
    return (app_module_effect_t){state.controller.rotation != rotation, state.controller.rotation};
}
static bool tick(void *context, int64_t now) {
    (void)context;
    if (state.dirty && !state.preserve_unknown && (!state.last_save || now - state.last_save >= 5000000)) {
        state.last_save = now;
        state.dirty = score_store_save(&state.session) != ESP_OK;
        state.storage = state.dirty ? TXT_SAVING_FAILED : TXT_SAVED;
        return true;
    }
    return false;
}
static bool ui_create(void *context, void *content) {
    (void)context;
    return score_ui_create(content);
}
static void ui_render(void *context, const app_module_view_t *view) {
    (void)context;
    const int winner = score_winner(&state.session.state);
    const char *message = winner < 0 ? state.status : winner ? TXT_WINNER_TWO : TXT_WINNER_ONE;
    state.controller.rotation = view->rotation;
    score_ui_render(&state.session, &state.controller, view->content_height,
        view->battery, message, view->storage_overlay && *view->storage_overlay ? view->storage_overlay : state.storage);
}
static void init(void *context) { (void)context; eight_module_init(); }
static const app_module_ops_t ops = {enter, leave, bindings, input, tick, ui_create, ui_render, init, NULL};
const app_module_t app_module_eight = {
    1, TXT_APP_EIGHT, TXT_APP_EIGHT_DESC, APP_ENTRY_GAME, &ops, NULL, APP_ART_EIGHT
};
