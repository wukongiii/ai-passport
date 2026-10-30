#include "eight_module.h"
#include "eight_ai.h"
#include "game_preferences.h"
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
    game_eight_preferences_t preferences;
    unsigned preferences_revision, player;
    int delta;
    bool player_status;
} state;

/* Storage bootstrap runs once outside the LVGL lock. The old NVS namespace and
 * schema are retained. Entry never starts a new match or discards history. */
void eight_module_init(void) {
    if (state.loaded) return;
    score_session_init(&state.session);
    game_preferences_default(&state.preferences);
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
static score_result_t dispatch(const score_command_t *c) {
    const score_result_t result=score_dispatch(&state.session,c);
    state.status=result==SCORE_NO_HISTORY ? TXT_NO_HISTORY : result!=SCORE_OK ? TXT_REJECTED :
        c->type==SCORE_CMD_UNDO ? TXT_UNDONE : c->type==SCORE_CMD_NEW_MATCH ? TXT_NEW_STARTED :
        c->amount<0 ? (c->player ? TXT_SUBBED_TWO : TXT_SUBBED_ONE) : (c->player ? TXT_AWARDED_TWO : TXT_AWARDED_ONE);
    state.player_status=result==SCORE_OK && c->type==SCORE_CMD_ADJUST_POINTS;
    state.player=c->player;state.delta=c->amount;
    if(result==SCORE_OK){
        if(c->type==SCORE_CMD_NEW_MATCH)state.preserve_unknown=false;
        state.dirty=true;state.last_save=0;
    }
    return result;
}
static app_module_effect_t input(void *context, app_rotation_t rotation,
                                 unsigned slot, app_gesture_t gesture) {
    (void)context;state.controller.rotation=rotation;score_command_t command;
    if(score_controller_input(&state.controller,&state.session,slot,gesture,&command))dispatch(&command);
    return (app_module_effect_t){state.controller.rotation!=rotation,state.controller.rotation};
}
static void refresh_names(void){
    game_preferences_snapshot_t preferences;game_preferences_snapshot(&preferences);
    if(preferences.revision!=state.preferences_revision){state.preferences=preferences.eight;state.preferences_revision=preferences.revision;}
}
static bool ai_stamp(void *context,uint32_t *revision,uint32_t *configuration){
    (void)context;refresh_names();
    if(!state.loaded || state.controller.page!=SCORE_PAGE_BOARD)return false;
    *revision=state.session.revision;*configuration=state.preferences_revision;return true;
}
static bool ai_snapshot(void *context,ai_game_snapshot_t *out){
    uint32_t revision,configuration;if(!ai_stamp(context,&revision,&configuration))return false;
    return eight_ai_snapshot(&state.session,&state.preferences,configuration,out);
}
static ai_game_result_t ai_execute(void *context,uint32_t revision,uint32_t configuration,
                                  const ai_tool_call_t *call,char *result,size_t capacity){
    uint32_t current,names;
    if(!ai_stamp(context,&current,&names))return AI_GAME_UNAVAILABLE;
    if(revision!=current || configuration!=names)return AI_GAME_STALE;
    if(capacity<128 || !result)return AI_GAME_INVALID;
    score_command_t command;ai_game_result_t parsed=eight_ai_command(&state.session,call,&command);
    if(parsed!=AI_GAME_OK)return parsed;
    if(command.type){
        score_result_t applied=dispatch(&command);
        if(applied!=SCORE_OK)return applied==SCORE_FINISHED ? AI_GAME_FINISHED : applied==SCORE_NO_HISTORY ? AI_GAME_NO_HISTORY : applied==SCORE_STALE ? AI_GAME_STALE : AI_GAME_INVALID;
    }
    return eight_ai_result(&state.session,result,capacity) ? AI_GAME_OK : AI_GAME_INVALID;
}
static const ai_game_ops_t ai_ops={ai_snapshot,ai_stamp,ai_execute,eight_ai_definition};
static bool tick(void *context, int64_t now) {
    (void)context;
    game_preferences_snapshot_t preferences;game_preferences_snapshot(&preferences);
    bool changed = preferences.revision != state.preferences_revision;
    if(changed){state.preferences=preferences.eight;state.preferences_revision=preferences.revision;}
    if (state.dirty && !state.preserve_unknown && (!state.last_save || now - state.last_save >= 5000000)) {
        state.last_save = now;
        state.dirty = score_store_save(&state.session) != ESP_OK;
        state.storage = state.dirty ? TXT_SAVING_FAILED : TXT_SAVED;
        return true;
    }
    return changed;
}
static bool ui_create(void *context, void *content) {
    (void)context;
    return score_ui_create(content);
}
static void ui_render(void *context, const app_module_view_t *view) {
    (void)context;
    state.controller.rotation = view->rotation;
    score_ui_render(&state.session, &state.controller, view->content_height,
        view->battery, state.status, view->storage_overlay && *view->storage_overlay ? view->storage_overlay : state.storage,
        &state.preferences, state.player_status, state.player, state.delta);
}
static void init(void *context) { (void)context; eight_module_init(); }
static const app_module_ops_t ops = {enter, leave, bindings, input, tick, ui_create, ui_render, init, NULL};
const app_module_t app_module_eight = {
    1, TXT_APP_EIGHT, TXT_APP_EIGHT_DESC, APP_ENTRY_GAME, &ops, NULL, APP_ART_EIGHT, &ai_ops
};
