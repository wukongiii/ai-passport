#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "../main/score/score_store.c"

static int fail_init, fail_open, fail_read, fail_set, fail_commit, oversized, commit_calls;
static bool has_record, has_ui;
static uint8_t saved_ui[APP_SETTINGS_SIZE];
static uint8_t saved[SCORE_RECORD_SIZE];
esp_err_t nvs_flash_init(void) { return fail_init ? ESP_FAIL : ESP_OK; }
esp_err_t nvs_open(const char *name, int mode, nvs_handle_t *h) {
    assert(!strcmp(name, "billiards") && mode == NVS_READWRITE);
    if (fail_open) return ESP_FAIL;
    *h = 1; return ESP_OK;
}
esp_err_t nvs_get_blob(nvs_handle_t h, const char *key, void *data, size_t *size) {
    assert(h == 1);
    const bool ui = !strcmp(key, "ui");
    assert(ui || !strcmp(key, "session"));
    if (fail_read) return ESP_FAIL;
    if (oversized) return ESP_ERR_NVS_INVALID_LENGTH;
    if (!(ui ? has_ui : has_record)) return ESP_ERR_NVS_NOT_FOUND;
    size_t length = ui ? sizeof(saved_ui) : sizeof(saved);
    assert(*size == length);
    memcpy(data, ui ? saved_ui : saved, length);
    *size = length;
    return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h, const char *key, const void *data, size_t size) {
    assert(h == 1);
    const bool ui = !strcmp(key, "ui");
    assert(ui || !strcmp(key, "session"));
    assert(size == (ui ? sizeof(saved_ui) : sizeof(saved)));
    if (fail_set) return ESP_FAIL;
    memcpy(ui ? saved_ui : saved, data, size);
    if (ui) has_ui = true; else has_record = true;
    return ESP_OK;
}

esp_err_t nvs_commit(nvs_handle_t h) {
    assert(h == 1); ++commit_calls;
    return fail_commit ? ESP_FAIL : ESP_OK;
}

int main(void) {
    score_session_t s, restored;
    score_session_init(&s); score_session_init(&restored);
    fail_init = 1;
    assert(score_store_open(&s) == SCORE_STORE_UNAVAILABLE);
    assert(score_store_save(&s) == ESP_ERR_INVALID_STATE);
    fail_init = 0; fail_open = 1;
    assert(score_store_open(&s) == SCORE_STORE_UNAVAILABLE);
    fail_open = 0;
    assert(score_store_open(&s) == SCORE_STORE_EMPTY);
    score_command_t c = { .type = SCORE_CMD_AWARD_FRAME, .player = 1, .amount = 1,
                          .expected_revision = SCORE_ANY_REVISION };
    assert(score_dispatch(&s, &c) == SCORE_OK);
    fail_set = 1;
    assert(score_store_save(&s) == ESP_FAIL && !commit_calls);
    fail_set = 0; fail_commit = 1;
    assert(score_store_save(&s) == ESP_FAIL && commit_calls == 1);
    fail_commit = 0;
    assert(score_store_save(&s) == ESP_OK);
    assert(score_store_open(&restored) == SCORE_STORE_LOADED);
    assert(restored.state.points[1] == 1 && restored.history_count == 1);
    app_rotation_t rotation = APP_ROTATION_270;
    assert(score_store_load_rotation(&rotation) == SCORE_STORE_EMPTY);
    uint8_t previous[SCORE_RECORD_SIZE]; memcpy(previous, saved, sizeof(previous));
    assert(score_store_save_rotation(APP_ROTATION_90) == ESP_OK);
    assert(!memcmp(previous, saved, sizeof(previous)));
    assert(score_store_load_rotation(&rotation) == SCORE_STORE_LOADED && rotation == APP_ROTATION_90);
    fail_set = 1; assert(score_store_save_rotation(APP_ROTATION_270) == ESP_FAIL); fail_set = 0;
    fail_commit = 1; assert(score_store_save_rotation(APP_ROTATION_270) == ESP_FAIL); fail_commit = 0;
    saved_ui[2] = 99;
    assert(score_store_load_rotation(&rotation) == SCORE_STORE_INVALID && rotation == APP_ROTATION_90);
    oversized = 1; assert(score_store_load_rotation(&rotation) == SCORE_STORE_INVALID); oversized = 0;
    fail_read = 1; assert(score_store_load_rotation(&rotation) == SCORE_STORE_UNAVAILABLE); fail_read = 0;
    assert(score_store_save_rotation((app_rotation_t)180) == ESP_ERR_INVALID_ARG);
    saved[10] ^= 1;
    const score_session_t before = restored;
    assert(score_store_open(&restored) == SCORE_STORE_INVALID);
    assert(!memcmp(&before, &restored, sizeof(before)));
    oversized = 1;
    assert(score_store_open(&restored) == SCORE_STORE_INVALID);
    oversized = 0; fail_read = 1;
    assert(score_store_open(&restored) == SCORE_STORE_UNAVAILABLE);
    puts("Score NVS missing/corrupt/oversized/read/write/commit failure tests: PASS");
}
