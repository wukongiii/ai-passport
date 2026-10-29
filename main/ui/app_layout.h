#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum { APP_KEY_LEFT, APP_KEY_MIDDLE, APP_KEY_RIGHT, APP_KEY_COUNT } app_key_t;
typedef enum { APP_ROTATION_90 = 90, APP_ROTATION_270 = 270 } app_rotation_t;

enum { APP_WIDTH = 320, APP_HEIGHT = 240, APP_KEYBAR_ROW_HEIGHT = 24, APP_KEYBAR_HEIGHT = APP_KEYBAR_ROW_HEIGHT * 3,
       APP_CONTENT_HEIGHT = APP_HEIGHT - APP_KEYBAR_HEIGHT };
typedef struct { int x, y, width, height; } app_rect_t;

bool app_rotation_valid(unsigned rotation);
app_rect_t app_content_rect(app_rotation_t rotation, unsigned rows);
app_rect_t app_keybar_rect(app_rotation_t rotation, unsigned rows);
/* Translate a BSP hardware index into a visual left/middle/right action slot.
 * Modules bind actions to slots, never to the printed hardware key names. */
app_key_t app_key_from_hardware(app_rotation_t rotation, unsigned hardware_index);

#define APP_SETTINGS_SIZE 8
void app_settings_encode(app_rotation_t rotation, uint8_t record[APP_SETTINGS_SIZE]);
bool app_settings_decode(app_rotation_t *rotation, const uint8_t *record, unsigned size);
