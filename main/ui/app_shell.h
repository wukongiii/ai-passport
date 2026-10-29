#pragma once
#include "app_layout.h"
#include "app_actions.h"
#include "lvgl.h"

/* Shared application shell. Modules declare up to nine slot/gesture actions
 * and render only inside content. All calls require the BSP LVGL lock. */
typedef struct {
    lv_obj_t *screen, *content, *keybar;
    lv_obj_t *key_labels[APP_GESTURE_COUNT][APP_KEY_COUNT];
    lv_obj_t *dividers[APP_KEY_COUNT - 1];
} app_shell_t;
void app_shell_create(app_shell_t *shell, const lv_font_t *font);
void app_shell_render(app_shell_t *shell, app_rotation_t rotation,
                      const app_bindings_t *bindings);
