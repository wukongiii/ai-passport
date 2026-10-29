#include "games/game_preferences.h"
#include "nvs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned char disk[4 + 2 * GAME_PLAYER_NAME_CAPACITY];
static bool stored, fail_init, fail_open, fail_write, fail_commit, fail_read, bad_record, short_record, oversized;
static unsigned closes;
esp_err_t nvs_flash_init(void) { return fail_init ? ESP_FAIL : ESP_OK; }
esp_err_t nvs_open(const char *name, int mode, nvs_handle_t *out) {
    assert(!strcmp(name,"game_prefs") && mode==NVS_READWRITE); *out=1;
    return fail_open ? ESP_FAIL : ESP_OK;
}
void nvs_close(nvs_handle_t h) { assert(h==1); ++closes; }
esp_err_t nvs_get_blob(nvs_handle_t h, const char *key, void *out, size_t *size) {
    assert(h==1 && !strcmp(key,"eight") && *size==sizeof(disk));
    if(fail_read)return ESP_FAIL;
    if(!stored)return ESP_ERR_NVS_NOT_FOUND;
    if(oversized)return ESP_ERR_NVS_INVALID_LENGTH;
    memcpy(out,disk,sizeof(disk));*size=short_record ? 3 : sizeof(disk);
    if(bad_record)((char *)out)[2]=99;
    return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h, const char *key, const void *data, size_t size) {
    assert(h==1 && !strcmp(key,"eight") && size==sizeof(disk));
    if(fail_write)return ESP_FAIL;
    if(!fail_commit){memcpy(disk,data,size);stored=true;} return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t h) { assert(h==1); return fail_commit ? ESP_FAIL : ESP_OK; }
static game_preferences_snapshot_t snapshot(void) { game_preferences_snapshot_t s;game_preferences_snapshot(&s);return s; }
static game_preferences_request_t request(void) {
    game_preferences_request_t r={.eight=snapshot().eight,.expected_revision=snapshot().revision};
    strcpy(r.eight.players[0],"张三");strcpy(r.eight.players[1],"李四");return r;
}
int main(void) {
    game_preferences_service_init();assert(snapshot().ready && !snapshot().error && !stored);
    game_preferences_request_t r=request(); unsigned id;
    assert(game_preferences_submit(&r,&id) && snapshot().busy && !stored);
    assert(!strcmp(snapshot().eight.players[0],"选手 A"));
    assert(!game_preferences_submit(&r,NULL)); game_preferences_service_poll();
    assert(snapshot().completed==id && !snapshot().error && !strcmp(snapshot().eight.players[0],"张三"));
    assert(!game_preferences_submit(&r,NULL)); // stale revision
    game_preferences_service_init();assert(closes==1 && !strcmp(snapshot().eight.players[1],"李四"));
    r=request();strcpy(r.eight.players[0],"Alice");fail_write=true;
    assert(game_preferences_submit(&r,&id));game_preferences_service_poll();
    assert(snapshot().error && !strcmp(snapshot().eight.players[0],"张三"));fail_write=false;
    fail_commit=true;assert(game_preferences_submit(&r,&id));game_preferences_service_poll();
    assert(snapshot().error && snapshot().completed==id && !strcmp(snapshot().eight.players[0],"张三"));fail_commit=false;
    assert(game_preferences_submit(&r,&id));game_preferences_service_poll();
    game_preferences_service_init();assert(!strcmp(snapshot().eight.players[0],"Alice"));
    bad_record=true;game_preferences_service_init();assert(snapshot().error && !strcmp(snapshot().eight.players[0],"选手 A"));bad_record=false;
    short_record=true;game_preferences_service_init();assert(snapshot().error);short_record=false;
    oversized=true;game_preferences_service_init();assert(snapshot().error);oversized=false;
    fail_read=true;game_preferences_service_init();assert(snapshot().error);fail_read=false;
    fail_init=true;game_preferences_service_init();assert(!snapshot().ready && snapshot().error);fail_init=false;
    fail_open=true;game_preferences_service_init();assert(!snapshot().ready);fail_open=false;
    puts("Game preferences deferred save, stale/busy rejection, reload and NVS failures: PASS");
}
