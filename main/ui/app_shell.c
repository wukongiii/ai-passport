#include "app_shell.h"
#include "app_strings.h"

void app_shell_create(app_shell_t *s, const lv_font_t *font) {
    *s = (app_shell_t){0};
    s->screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(s->screen);
    lv_obj_set_style_bg_color(s->screen, lv_color_hex(0x101c25), 0);
    lv_obj_set_style_bg_opa(s->screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(s->screen, LV_OBJ_FLAG_SCROLLABLE);
    s->content = lv_obj_create(s->screen);
    lv_obj_remove_style_all(s->content);
    lv_obj_set_size(s->content, APP_WIDTH, APP_CONTENT_HEIGHT);
    lv_obj_remove_flag(s->content, LV_OBJ_FLAG_SCROLLABLE);
    s->keybar = lv_obj_create(s->screen);
    lv_obj_remove_style_all(s->keybar);
    lv_obj_set_size(s->keybar, APP_WIDTH, APP_KEYBAR_HEIGHT);
    lv_obj_set_style_bg_color(s->keybar, lv_color_hex(0x263b47), 0);
    lv_obj_set_style_bg_opa(s->keybar, LV_OPA_COVER, 0);
    lv_obj_remove_flag(s->keybar, LV_OBJ_FLAG_SCROLLABLE);
    for (unsigned slot = 0; slot < APP_KEY_COUNT; ++slot) {
        if (slot) {
            s->dividers[slot - 1] = lv_obj_create(s->keybar);
            lv_obj_remove_style_all(s->dividers[slot - 1]);
            lv_obj_set_style_bg_color(s->dividers[slot - 1], lv_color_hex(0x52646d), 0);
            lv_obj_set_style_bg_opa(s->dividers[slot - 1], LV_OPA_COVER, 0);
        }
        for (unsigned gesture = 0; gesture < APP_GESTURE_COUNT; ++gesture) {
            lv_obj_t *label = lv_label_create(s->keybar);
            s->key_labels[gesture][slot] = label;
            // Insets protect text from physical rounded corners.
            lv_obj_set_size(label, 88, 21);
            lv_obj_set_style_radius(label, 5, 0);
            lv_obj_set_style_bg_opa(label, LV_OPA_COVER, 0);
            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_style_text_font(label, font, 0);
            lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
            lv_label_set_text(label, "");
        }
    }
}

void app_shell_render(app_shell_t *s, app_rotation_t rotation,
                      const app_bindings_t *bindings) {
    lv_display_t *display = lv_obj_get_display(s->screen);
    const lv_display_rotation_t desired = rotation == APP_ROTATION_90 ?
        LV_DISPLAY_ROTATION_90 : LV_DISPLAY_ROTATION_270;
    if (lv_display_get_rotation(display) != desired) lv_display_set_rotation(display, desired);
    const unsigned rows = app_binding_rows(bindings);
    const app_rect_t content = app_content_rect(rotation, rows);
    const app_rect_t keybar = app_keybar_rect(rotation, rows);
    lv_obj_set_pos(s->content, content.x, content.y);
    lv_obj_set_size(s->content, content.width, content.height);
    lv_obj_set_pos(s->keybar, keybar.x, keybar.y);
    lv_obj_set_size(s->keybar, keybar.width, keybar.height);
    for (unsigned i = 0; i < APP_KEY_COUNT - 1; ++i) {
        lv_obj_set_pos(s->dividers[i], 112 + (int)i * 94, 3);
        lv_obj_set_size(s->dividers[i], 1, keybar.height - 6);
    }
    unsigned row = 0;
    for (unsigned gesture = 0; gesture < APP_GESTURE_COUNT; ++gesture) {
        const bool visible = app_binding_has_gesture(bindings, (app_gesture_t)gesture);
        for (unsigned slot = 0; slot < APP_KEY_COUNT; ++slot) {
            lv_obj_t *label = s->key_labels[gesture][slot];
            if (!visible) { lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN); continue; }
            lv_obj_remove_flag(label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(label, 22 + (int)slot * 94, 2 + (int)row * APP_KEYBAR_ROW_HEIGHT);
            const app_action_hint_t *hint = &bindings->actions[gesture][slot];
            static const char *prefixes[] = {TXT_SHORT_PREFIX, TXT_LONG_PREFIX, TXT_DOUBLE_PREFIX};
            if (hint->label && *hint->label) lv_label_set_text_fmt(label, "%s %s", prefixes[gesture], hint->label);
            else lv_label_set_text(label, "--");
            // Fixed semantic palette shared by every sport and gesture.
            static const uint32_t backgrounds[APP_ACTION_ROLE_COUNT] = {
                0x304B5C, 0x246B4E, 0x934C32, 0x48545C,
                0x246B4E, 0x934C32, 0x304B5C
            };
            const bool active = hint->label && *hint->label && hint->enabled;
            const unsigned role = (unsigned)hint->role < APP_ACTION_ROLE_COUNT ? hint->role : APP_ACTION_NEUTRAL;
            lv_obj_set_style_bg_color(label, lv_color_hex(active ? backgrounds[role] : 0x263B47), 0);
            lv_obj_set_style_text_color(label, lv_color_hex(active ? 0xF2F3E9 : 0x7B8B94), 0);
        }
        if (visible) ++row;
    }
}
