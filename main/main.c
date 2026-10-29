#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_pins.h"
#include "key_guard.h"
#include "score/score.h"
#include "score/score_controller.h"
#include "score/score_input.h"
#include "score/score_store.h"
#include "score/score_strings.h"
#include "score/score_ui.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lvgl.h"

static const char *TAG = "scorekeeper";
static QueueHandle_t s_input_queue;
static volatile bool s_input_ready;
static volatile bool s_input_overflow;
static key_guard_t s_key_guard;
static score_session_t s_session;
static score_controller_t s_controller;

typedef struct { bsp_btn_t button; bsp_btn_ev_t event; } input_event_t;

static void on_key(bsp_btn_t button, bsp_btn_ev_t event, void *user) {
    (void)user;
    if (key_guard_consume(&s_key_guard, event == BSP_BTN_RELEASE, esp_timer_get_time())) return;
    if (!s_input_ready || !s_input_queue) return;
    // Queue double clicks as distinct gestures, never opening and accepting a dialog
    // from one rapid physical gesture. Each gesture has an explicit binding.
    if (event != BSP_BTN_PRESS && event != BSP_BTN_CLICK && event != BSP_BTN_DOUBLE &&
        event != BSP_BTN_LONG && event != BSP_BTN_RELEASE) return;
    const input_event_t input = {button, event};
    if (xQueueSend(s_input_queue, &input, 0) != pdTRUE) s_input_overflow = true;
}

static const char *command_status(const score_command_t *c, score_result_t result) {
    if (result == SCORE_NO_HISTORY) return TXT_NO_HISTORY;
    if (result != SCORE_OK) return TXT_REJECTED;
    if (c->type == SCORE_CMD_UNDO) return TXT_UNDONE;
    if (c->type == SCORE_CMD_NEW_MATCH) return TXT_NEW_STARTED;
    if (c->amount < 0) return c->player == 0 ? TXT_SUBBED_ONE : TXT_SUBBED_TWO;
    return c->player == 0 ? TXT_AWARDED_ONE : TXT_AWARDED_TWO;
}

void app_main(void) {
    ESP_LOGI(TAG, "Offline Chinese eight-ball scorekeeper");
    score_session_init(&s_session);
    const score_store_status_t store = score_store_open(&s_session);
    bool preserve_unknown_record = store == SCORE_STORE_INVALID || store == SCORE_STORE_UNAVAILABLE;
    bool dirty = false;
    const char *storage = store == SCORE_STORE_UNAVAILABLE ? TXT_STORE_FAILED :
                          store == SCORE_STORE_INVALID ? TXT_RECORD_BAD :
                          store == SCORE_STORE_LOADED ? TXT_SAVED : "";
    const char *status = store == SCORE_STORE_LOADED ? TXT_RESTORED : TXT_EMPTY;
    score_controller_init(&s_controller, &s_session);

    const score_store_status_t ui_store = score_store_load_rotation(&s_controller.rotation);
    bool ui_dirty = false;
    int64_t last_ui_save = 0;
    const char *ui_storage = ui_store == SCORE_STORE_INVALID ? TXT_UI_BAD :
                             ui_store == SCORE_STORE_UNAVAILABLE ? TXT_STORE_FAILED : "";
    if (bsp_display_init() != ESP_OK) return;
    lv_display_t *display = bsp_lvgl_init();
    if (!display || !bsp_lvgl_lock(1000)) return;
    lv_display_set_rotation(display, s_controller.rotation == APP_ROTATION_90 ?
                            LV_DISPLAY_ROTATION_90 : LV_DISPLAY_ROTATION_270);
    const bool ui_ready = score_ui_create();
    bsp_lvgl_unlock();
    if (!ui_ready) {
        ESP_LOGE(TAG, "UI font coverage check failed");
        return;
    }
    bsp_display_backlight(80);
    const bool battery_ready = bsp_battery_init() == ESP_OK;
    // Audio is unused. Suspend the powered codec even if PCM was never opened.
    if (bsp_audio_init() == ESP_OK && bsp_audio_sleep() != ESP_OK)
        ESP_LOGW(TAG, "Codec suspend failed; power draw remains unverified");
    s_input_queue = xQueueCreate(16, sizeof(input_event_t));
    if (!s_input_queue) {
        ESP_LOGE(TAG, "Input queue allocation failed");
        return;
    }
    if (esp_sleep_get_wakeup_causes() & (1u << ESP_SLEEP_WAKEUP_GPIO))
        key_guard_arm(&s_key_guard, esp_timer_get_time(), 10000000);
    if (bsp_button_init(on_key, NULL) != ESP_OK) {
        ESP_LOGE(TAG, "Button initialization failed");
        vQueueDelete(s_input_queue);
        s_input_queue = NULL;
        return;
    }
    key_guard_sync_released(&s_key_guard, bsp_button_read_mv() >= BSP_BTN_MV_RELEASED_MIN);
    s_input_ready = true;
    int battery = battery_ready ? bsp_battery_soc() : -1;
    int64_t last_battery = esp_timer_get_time();
    int64_t last_input = last_battery;
    int64_t last_save = 0;
    bool blanked = false;
    score_input_guard_t wake_guard = {0};
    uint8_t brightness = 80;
    if (bsp_lvgl_lock(500)) {
        score_ui_render(&s_session, &s_controller, battery, status, *ui_storage ? ui_storage : storage);
        bsp_lvgl_unlock();
    }
    ESP_LOGI(TAG, "Ready, free heap=%u largest=%u", (unsigned)esp_get_free_heap_size(),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    for (;;) {
        input_event_t input;
        bool render = true;
        if (xQueueReceive(s_input_queue, &input, pdMS_TO_TICKS(250)) == pdTRUE) {
            last_input = esp_timer_get_time();
            if (blanked) {
                blanked = false;
                wake_guard = (score_input_guard_t){ .swallowing = true };
                brightness = 80;
                bsp_display_backlight(brightness);
            }
            score_input_event_t event = input.event == BSP_BTN_PRESS ? SCORE_INPUT_PRESS :
                input.event == BSP_BTN_CLICK ? SCORE_INPUT_CLICK :
                input.event == BSP_BTN_DOUBLE ? SCORE_INPUT_DOUBLE :
                input.event == BSP_BTN_LONG ? SCORE_INPUT_LONG : SCORE_INPUT_RELEASE;
            if (!score_input_consume(&wake_guard, event) &&
                (input.event == BSP_BTN_CLICK || input.event == BSP_BTN_DOUBLE || input.event == BSP_BTN_LONG)) {
                const unsigned hardware = input.button == BSP_BTN_UP ? 0 :
                                          input.button == BSP_BTN_DOWN ? 1 : 2;
                const unsigned slot = app_key_from_hardware(s_controller.rotation, hardware);
                const app_gesture_t gesture = input.event == BSP_BTN_LONG ? APP_GESTURE_LONG :
                    input.event == BSP_BTN_DOUBLE ? APP_GESTURE_DOUBLE : APP_GESTURE_SHORT;
                const app_rotation_t before_rotation = s_controller.rotation;
                score_command_t command;
                if (score_controller_input(&s_controller, &s_session, slot, gesture, &command)) {
                    const score_result_t result = score_dispatch(&s_session, &command);
                    status = command_status(&command, result);
                    if (result == SCORE_OK) {
                        if (command.type == SCORE_CMD_NEW_MATCH) preserve_unknown_record = false;
                        dirty = true;
                        last_save = 0;
                    }
                }
                if (s_controller.rotation != before_rotation) {
                    ui_dirty = true;
                    last_ui_save = 0;
                }
            }
        } else render = false;
        const int64_t now = esp_timer_get_time();
        if (ui_dirty && (last_ui_save == 0 || now - last_ui_save >= 5000000)) {
            last_ui_save = now;
            ui_dirty = score_store_save_rotation(s_controller.rotation) != ESP_OK;
            ui_storage = ui_dirty ? TXT_UI_FAILED : "";
            render = true;
        }
        if (dirty && !preserve_unknown_record && (last_save == 0 || now - last_save >= 5000000)) {
            last_save = now;
            const esp_err_t error = score_store_save(&s_session);
            dirty = error != ESP_OK;
            storage = dirty ? TXT_SAVING_FAILED : TXT_SAVED;
            render = true;
        }
        if (now - last_battery >= 30000000) {
            battery = battery_ready ? bsp_battery_soc() : -1;
            last_battery = now;
            render = true;
        }
        // Backlight-only idle policy: full system sleep remains a later feature.
        const int64_t idle = now - last_input;
        const uint8_t desired = idle >= 120000000 ? 0 : idle >= 30000000 ? 15 : 80;
        if (desired != brightness) {
            brightness = desired;
            bsp_display_backlight(brightness);
            blanked = brightness == 0;
        }
        if (s_input_overflow) {
            status = TXT_INPUT_BUSY;
            s_input_overflow = false;
            render = true;
        }
        if (render && bsp_lvgl_lock(500)) {
            const int winner = score_winner(&s_session.state);
            const char *message = winner < 0 ? status : winner == 0 ? TXT_WINNER_ONE : TXT_WINNER_TWO;
            score_ui_render(&s_session, &s_controller, battery, message, *ui_storage ? ui_storage : storage);
            bsp_lvgl_unlock();
        }
    }
}
