#include "games/eight_module.h"
#include "score/score_store.h"
#include "score/score_ui.h"
#include "speech/app_speech.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static score_session_t disk;
static unsigned reads, saves, creates, renders;
static bool fail_save;
static unsigned spoken;static char last_speech[96];
bool app_speech_say(const char *text){++spoken;snprintf(last_speech,sizeof(last_speech),"%s",text);return true;}
static game_preferences_snapshot_t names={.revision=1};
void game_preferences_snapshot(game_preferences_snapshot_t *out) { *out=names; }
score_store_status_t score_store_open(score_session_t *s) { ++reads; *s = disk; return SCORE_STORE_LOADED; }
esp_err_t score_store_save(const score_session_t *s) {
    ++saves; if (fail_save) return ESP_FAIL; disk = *s; return ESP_OK;
}
bool score_ui_create(void *content) { assert(content); ++creates; return true; }
void score_ui_render(const score_session_t *s, const score_controller_t *ui,
                     int height, int battery, const char *status, const char *storage,
                     const game_eight_preferences_t *p, bool player_status, unsigned player, int delta) {
    assert(s && ui && height == 192 && battery == 85 && status && storage); ++renders;
    assert(!strcmp(p->players[0],"张三") && player_status && player==0 && delta==-1);
}
int main(void) {
    game_preferences_default(&names.eight);
    score_session_init(&disk); disk.state.target = 5; disk.state.points[0] = 2;
    eight_module_init(); eight_module_init(); assert(reads == 1);
    const app_module_t *m = &app_module_eight;
    assert(m->ops->enter(m->context, APP_ROTATION_270));
    app_bindings_t b; m->ops->bindings(m->context, &b);
    assert(!b.actions[APP_GESTURE_LONG][1].id); // Container may supply Home.
    const app_module_effect_t e = m->ops->input(m->context, APP_ROTATION_270, 0, APP_GESTURE_SHORT);
    assert(!e.rotation_changed && spoken==1 && !strcmp(last_speech,"当前比分 3 比 0"));
    fail_save = true; m->ops->tick(m->context, 100); assert(saves == 1 && disk.state.points[0] == 2);
    assert(m->ops->leave(m->context));
    fail_save = false; m->ops->tick(m->context, 5000100);
    assert(saves == 2 && disk.state.points[0] == 3 && disk.history_count == 1 && disk.state.target == 5);
    assert(m->ops->enter(m->context, APP_ROTATION_90));
    m->ops->input(m->context, APP_ROTATION_90, 0, APP_GESTURE_LONG);
    m->ops->tick(m->context, 5000200);
    assert(disk.state.points[0] == 2 && disk.history_count == 2 && spoken==2 && !strcmp(last_speech,"当前比分 2 比 0"));
    m->ops->input(m->context,APP_ROTATION_90,2,APP_GESTURE_LONG);assert(spoken==2); // Rejected zero-score decrement.
    assert(m->ops->ui_create(m->context, &disk));
    const app_module_view_t view = {APP_ROTATION_90, 192, 85, ""};
    strcpy(names.eight.players[0],"张三");++names.revision;
    assert(m->ops->tick(m->context,5000300));
    // Renaming preserves score/history and updates live views.
    assert(disk.state.points[0]==2 && disk.history_count==2);
    m->ops->input(m->context, APP_ROTATION_90, 1, APP_GESTURE_SHORT);
    m->ops->ui_render(m->context, &view);
    assert(creates == 1 && renders == 1 && reads == 1 && spoken==2);
    // Manual undo uses the same committed-score feedback path.
    m->ops->input(m->context,APP_ROTATION_90,2,APP_GESTURE_SHORT);
    m->ops->input(m->context,APP_ROTATION_90,1,APP_GESTURE_SHORT);
    assert(spoken==3&&!strcmp(last_speech,"当前比分 3 比 0"));
    puts("Eight-ball adapter re-entry, legacy restore, inactive save retry and view: PASS");
}
