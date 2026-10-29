#include "game_preferences.h"
#include "freertos/FreeRTOS.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <string.h>

enum { RECORD_SIZE = 4 + 2 * GAME_PLAYER_NAME_CAPACITY };
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static game_preferences_snapshot_t view;
static game_preferences_request_t pending;
static bool has_pending, opened;
static nvs_handle_t store;

void game_preferences_snapshot(game_preferences_snapshot_t *out) {
    portENTER_CRITICAL(&lock); *out = view; portEXIT_CRITICAL(&lock);
}
void game_preferences_service_init(void) {
    game_eight_preferences_t current; game_preferences_default(&current);
    if (opened) { nvs_close(store); opened = false; }
    esp_err_t e = nvs_flash_init();
    if (e == ESP_OK) { e = nvs_open("game_prefs", NVS_READWRITE, &store); opened = e == ESP_OK; }
    if (opened) {
        uint8_t record[RECORD_SIZE] = {0}; size_t size = sizeof(record);
        e = nvs_get_blob(store, "eight", record, &size);
        if (e == ESP_ERR_NVS_NOT_FOUND) e = ESP_OK;
        else if (e == ESP_OK) {
            game_eight_preferences_t loaded; memcpy(loaded.players, record + 4, sizeof(loaded.players));
            if (size != sizeof(record) || memcmp(record, "GP\1\0", 4) || !game_preferences_valid(&loaded)) e = ESP_ERR_INVALID_ARG;
            else current = loaded;
        }
    }
    portENTER_CRITICAL(&lock);
    unsigned revision = view.revision + 1;
    view = (game_preferences_snapshot_t){ .eight = current, .ready = opened, .revision = revision, .error = e };
    has_pending = false;
    portEXIT_CRITICAL(&lock);
}
bool game_preferences_submit(const game_preferences_request_t *request, unsigned *id) {
    if (!request || !game_preferences_valid(&request->eight)) return false;
    portENTER_CRITICAL(&lock);
    bool ok = view.ready && !view.busy && request->expected_revision == view.revision;
    if (ok) {
        pending = *request; has_pending = true; view.busy = true; view.error = 0;
        ++view.submitted; if (id) *id = view.submitted;
    }
    portEXIT_CRITICAL(&lock); return ok;
}
void game_preferences_service_poll(void) {
    game_preferences_request_t request={0}; unsigned id=0;
    portENTER_CRITICAL(&lock); bool have = has_pending;
    if (have) { request = pending; id = view.submitted; has_pending = false; }
    portEXIT_CRITICAL(&lock); if (!have) return;
    uint8_t record[RECORD_SIZE] = {'G', 'P', 1, 0};
    memcpy(record + 4, request.eight.players, sizeof(request.eight.players));
    esp_err_t e = nvs_set_blob(store, "eight", record, sizeof(record));
    if (e == ESP_OK) e = nvs_commit(store);
    portENTER_CRITICAL(&lock);
    if (e == ESP_OK) { view.eight = request.eight; ++view.revision; }
    view.error = e; view.completed = id; view.busy = false;
    portEXIT_CRITICAL(&lock);
}
