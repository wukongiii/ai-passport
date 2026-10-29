#include "app_layout.h"
#include "app_actions.h"

bool app_rotation_valid(unsigned rotation) {
    return rotation == APP_ROTATION_90 || rotation == APP_ROTATION_270;
}

static int bar_height(unsigned rows) {
    if (rows < 1) rows = 1;
    if (rows > APP_GESTURE_COUNT) rows = APP_GESTURE_COUNT;
    return (int)rows * APP_KEYBAR_ROW_HEIGHT;
}

app_rect_t app_content_rect(app_rotation_t rotation, unsigned rows) {
    const int height = bar_height(rows);
    return (app_rect_t){0, rotation == APP_ROTATION_270 ? height : 0,
                       APP_WIDTH, APP_HEIGHT - height};
}

app_rect_t app_keybar_rect(app_rotation_t rotation, unsigned rows) {
    const int height = bar_height(rows);
    return (app_rect_t){0, rotation == APP_ROTATION_270 ? 0 : APP_HEIGHT - height,
                       APP_WIDTH, height};
}

app_key_t app_key_from_hardware(app_rotation_t rotation, unsigned hardware_index) {
    if (hardware_index >= APP_KEY_COUNT) return APP_KEY_COUNT;
    // Device feedback: top-edge keys at 270 degrees run UP/DOWN/OK left to
    // right; turning to 90 degrees reverses their visual order.
    return (app_key_t)(rotation == APP_ROTATION_90 ? 2 - hardware_index : hardware_index);
}

void app_settings_encode(app_rotation_t rotation, uint8_t p[APP_SETTINGS_SIZE]) {
    p[0] = 'U'; p[1] = 'I'; p[2] = 1;
    p[3] = rotation == APP_ROTATION_90 ? 1 : 3;
    p[4] = (uint8_t)~p[3]; p[5] = 0; p[6] = 0; p[7] = 0;
}

bool app_settings_decode(app_rotation_t *rotation, const uint8_t *p, unsigned size) {
    if (!rotation || !p || size != APP_SETTINGS_SIZE || p[0] != 'U' || p[1] != 'I' ||
        p[2] != 1 || (p[3] != 1 && p[3] != 3) || (unsigned)p[3] + p[4] != 255 ||
        p[5] || p[6] || p[7]) return false;
    *rotation = p[3] == 1 ? APP_ROTATION_90 : APP_ROTATION_270;
    return true;
}
