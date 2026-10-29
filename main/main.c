#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_pins.h"
#include "key_guard.h"
#include "app/app_container.h"
#include "app/app_ui.h"
#include "score/score_input.h"
#include "app/app_settings.h"
#include "esp_app_desc.h"
#include "score/score_strings.h"
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
static app_container_t s_app;

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

void app_main(void) {
    ESP_LOGI(TAG, "Billiards application container");
    unsigned count;
    const app_module_t *modules = app_registry(&count);
    app_modules_init(modules, count);
    if (!app_container_init(&s_app, modules, count, app_settings_preferences()->rotation)) return;
    if (bsp_display_init() != ESP_OK) return;
    lv_display_t *display = bsp_lvgl_init();
    if (!display || !bsp_lvgl_lock(1000)) return;
    lv_display_set_rotation(display, s_app.rotation == APP_ROTATION_90 ?
                            LV_DISPLAY_ROTATION_90 : LV_DISPLAY_ROTATION_270);
    const bool ui_ready = app_ui_create();
    bsp_lvgl_unlock();
    if (!ui_ready) {
        ESP_LOGE(TAG, "UI font coverage check failed");
        return;
    }
    bsp_display_backlight(app_settings_preferences()->brightness);
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
    int millivolts = battery_ready ? bsp_battery_mv() : -1;
    app_settings_device_info(battery,millivolts,esp_app_get_description()->version);
    int64_t last_battery = esp_timer_get_time();
    int64_t last_input = last_battery;
    bool blanked = false;
    score_input_guard_t wake_guard = {0};
    uint8_t brightness = app_settings_preferences()->brightness;
    if (bsp_lvgl_lock(500)) {
        app_ui_render(&s_app, battery, "");
        bsp_lvgl_unlock();
    }
    ESP_LOGI(TAG, "Ready, free heap=%u largest=%u", (unsigned)esp_get_free_heap_size(),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    for (;;) {
        input_event_t input;
        bool render = true;
        const app_module_t *current=app_container_active(&s_app);
        const unsigned wait_ms=current && current->ops && current->ops->key ? 20 : 250;
        if (xQueueReceive(s_input_queue, &input, pdMS_TO_TICKS(wait_ms)) == pdTRUE) {
            last_input = esp_timer_get_time();
            if (blanked) {
                blanked = false;
                wake_guard = (score_input_guard_t){ .swallowing = true };
                brightness = app_settings_preferences()->brightness;
                bsp_display_backlight(brightness);
            }
            score_input_event_t event = input.event == BSP_BTN_PRESS ? SCORE_INPUT_PRESS :
                input.event == BSP_BTN_CLICK ? SCORE_INPUT_CLICK :
                input.event == BSP_BTN_DOUBLE ? SCORE_INPUT_DOUBLE :
                input.event == BSP_BTN_LONG ? SCORE_INPUT_LONG : SCORE_INPUT_RELEASE;
            if (!score_input_consume(&wake_guard, event)) {
                const unsigned hardware = input.button == BSP_BTN_UP ? 0 :
                                          input.button == BSP_BTN_DOWN ? 1 : 2;
                const unsigned slot = app_key_from_hardware(s_app.rotation, hardware);
                const app_module_t *active=app_container_active(&s_app);
                if(active && active->ops && active->ops->key &&
                   (input.event==BSP_BTN_PRESS || input.event==BSP_BTN_RELEASE))
                    active->ops->key(active->context,slot,input.event==BSP_BTN_PRESS,esp_timer_get_time());
                if(input.event==BSP_BTN_CLICK || input.event==BSP_BTN_DOUBLE || input.event==BSP_BTN_LONG) {
                    const app_gesture_t gesture=input.event==BSP_BTN_LONG ? APP_GESTURE_LONG :
                        input.event==BSP_BTN_DOUBLE ? APP_GESTURE_DOUBLE : APP_GESTURE_SHORT;
                    app_container_input(&s_app,slot,gesture);
                }
            }
        } else render = false;
        const int64_t now = esp_timer_get_time();
        // Inactive games still get persistence retries, outside the UI lock.
        if (app_container_tick(&s_app, now)) render = true;
        if (now - last_battery >= 30000000) {
            battery = battery_ready ? bsp_battery_soc() : -1;
            millivolts = battery_ready ? bsp_battery_mv() : -1;
            app_settings_device_info(battery,millivolts,esp_app_get_description()->version);
            last_battery = now;
            render = true;
        }
        // Backlight-only idle policy: full system sleep remains a later feature.
        const int64_t idle = now - last_input;
        app_preferences_t display_preferences=*app_settings_preferences();
        display_preferences.brightness=app_settings_preview_brightness();
        const uint8_t desired = app_preferences_backlight(&display_preferences,idle);
        if (desired != brightness) {
            brightness = desired;
            bsp_display_backlight(brightness);
            blanked = brightness == 0;
        }
        if (s_input_overflow) {
            ESP_LOGW(TAG, "Input queue overflow");
            s_input_overflow = false;
            render = true;
        }
        if (render && bsp_lvgl_lock(500)) {
            app_ui_render(&s_app, battery, "");
            bsp_lvgl_unlock();
        }
    }
}
