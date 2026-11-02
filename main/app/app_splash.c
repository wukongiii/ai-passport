#include "app_splash.h"

LV_FONT_DECLARE(startup_font_24);

static lv_obj_t *disc(lv_obj_t *parent, int x, int y, int size, uint32_t color) {
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, size, size);
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    return obj;
}

lv_obj_t *app_splash_create(void) {
    static const uint32_t title_codepoints[] = {0x9017, 0x5B9D, 0x53F0, 0x7403, 0x8BB0, 0x5206, 0x5668};
    for (unsigned i = 0; i < sizeof(title_codepoints) / sizeof(title_codepoints[0]); ++i) {
        lv_font_glyph_dsc_t glyph = {0};
        if (!lv_font_get_glyph_dsc(&startup_font_24, &glyph, title_codepoints[i], 0) || glyph.is_placeholder)
            return NULL;
    }
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(screen);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x123D34), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    disc(screen, 110, 30, 100, 0xA99257);
    disc(screen, 112, 32, 96, 0x123D34);
    lv_obj_t *ball = disc(screen, 120, 40, 80, 0x0B171A);
    lv_obj_set_style_border_width(ball, 2, 0);
    lv_obj_set_style_border_color(ball, lv_color_hex(0x45665F), 0);
    lv_obj_t *spot = disc(ball, 21, 18, 38, 0xF3EEDB);
    lv_obj_t *eight = lv_label_create(spot);
    lv_obj_set_style_text_font(eight, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(eight, lv_color_hex(0x102421), 0);
    lv_label_set_text(eight, "8");
    lv_obj_center(eight);

    lv_obj_t *wordmark = lv_label_create(screen);
    lv_obj_set_style_text_font(wordmark, &startup_font_24, 0);
    lv_obj_set_style_text_color(wordmark, lv_color_hex(0xF3EEDB), 0);
    lv_label_set_text_static(wordmark, APP_SPLASH_TITLE);
    lv_obj_align(wordmark, LV_ALIGN_TOP_MID, 0, 148);

    lv_obj_t *rule = lv_obj_create(screen);
    lv_obj_remove_style_all(rule);
    lv_obj_set_size(rule, 44, 2);
    lv_obj_set_style_bg_color(rule, lv_color_hex(0xC3A96B), 0);
    lv_obj_set_style_bg_opa(rule, LV_OPA_COVER, 0);
    lv_obj_align(rule, LV_ALIGN_TOP_MID, 0, 198);
    return screen;
}
