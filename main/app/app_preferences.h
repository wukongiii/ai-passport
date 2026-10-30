#pragma once
#include "../ui/app_layout.h"
typedef struct {
    app_rotation_t rotation;
    uint8_t brightness;
    uint16_t dim_seconds, blank_seconds; // 0 disables the corresponding timer.
    uint16_t double_click_ms; // Release-to-next-press recognition window.
    uint8_t volume; // 0 mutes local speech.
} app_preferences_t;
enum { APP_VOLUME_DEFAULT=60, APP_VOLUME_STEP=10, APP_DOUBLE_MIN=200, APP_DOUBLE_MAX=450, APP_DOUBLE_STEP=50, APP_DOUBLE_DEFAULT=350 };
enum { APP_PREFERENCES_SIZE = 16 };
app_preferences_t app_preferences_default(void);
bool app_preferences_valid(const app_preferences_t *p);
void app_preferences_encode(const app_preferences_t *p, uint8_t out[APP_PREFERENCES_SIZE]);
bool app_preferences_decode(app_preferences_t *p, const uint8_t *data, unsigned size);
uint8_t app_preferences_backlight(const app_preferences_t *p, int64_t idle_us);
