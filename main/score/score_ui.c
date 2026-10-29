#include "score_ui.h"
#include "score_strings.h"
#include "score_config.h"
#include "../ui/app_range.h"
#include "lvgl.h"
#include <stdio.h>

LV_FONT_DECLARE(score_font_16);

#define BG 0x101C25
#define SURFACE 0x1C2D38
#define INK 0xF2F3E9
#define MUTED 0xA6B5B7
#define GOLD 0xEDC56B
#define BLUE 0x74C5D9

static lv_obj_t *s_screen, *s_board, *s_dialog, *s_battery, *s_race, *s_status, *s_storage;
static lv_obj_t *s_cards[2], *s_points[2], *s_menu_rows[2];
static lv_obj_t *s_title, *s_detail, *s_target_hint;
static app_range_t s_target_range;

static lv_obj_t *panel(lv_obj_t *parent, int x, int y, int w, int h) {
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, lv_color_hex(SURFACE), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(o, 12, 0);
    lv_obj_set_style_border_width(o, 2, 0);
    lv_obj_set_style_border_color(o, lv_color_hex(SURFACE), 0);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

static lv_obj_t *label(lv_obj_t *parent, int x, int y, int w, const char *text,
                       const lv_font_t *font, uint32_t color) {
    lv_obj_t *o = lv_label_create(parent);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_width(o, w);
    lv_obj_set_style_text_font(o, font, 0);
    lv_obj_set_style_text_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(o, LV_LABEL_LONG_CLIP);
    lv_label_set_text(o, text);
    return o;
}

static void selected(lv_obj_t *o, bool yes, uint32_t color) {
    lv_obj_set_style_border_color(o, lv_color_hex(yes ? color : SURFACE), 0);
}

bool score_ui_create(void *content) {
    /* Validate the same font assigned to every Chinese widget. A missing glyph
     * aborts initialization instead of silently displaying placeholders. */
    extern const uint32_t score_font_codepoints[];
    extern const unsigned score_font_codepoint_count;
    for (unsigned i = 0; i < score_font_codepoint_count; ++i) {
        lv_font_glyph_dsc_t glyph = {0};
        if (!lv_font_get_glyph_dsc(&score_font_16, &glyph, score_font_codepoints[i], 0) ||
            glyph.is_placeholder) return false;
    }
    s_screen = content;
    label(s_screen, 26, 7, 100, TXT_TITLE, &score_font_16, INK);
    s_battery = label(s_screen, 247, 9, 48, "--%", &lv_font_montserrat_14, MUTED);
    s_board = panel(s_screen, 0, 28, 320, 128);
    lv_obj_set_style_bg_color(s_board, lv_color_hex(BG), 0);
    lv_obj_set_style_border_width(s_board, 0, 0);
    for (unsigned i = 0; i < 2; ++i) {
        s_cards[i] = panel(s_board, i == 0 ? 16 : 174, 4, 130, 88);
        label(s_cards[i], 8, 3, 114, i == 0 ? TXT_PLAYER_ONE : TXT_PLAYER_TWO,
                           &score_font_16, i == 0 ? GOLD : BLUE);
        s_points[i] = label(s_cards[i], 5, 23, 120, "0", &lv_font_montserrat_48, INK);
    }
    label(s_board, 145, 30, 30, ":", &lv_font_montserrat_32, MUTED);
    s_race = label(s_screen, 132, 7, 82, "", &score_font_16, GOLD);
    s_dialog = panel(s_screen, 16, 29, 288, 124);
    s_title = label(s_dialog, 8, 5, 272, "", &score_font_16, GOLD);
    s_detail = label(s_dialog, 4, 38, 280, "", &score_font_16, INK);
    s_target_hint = label(s_dialog, 4, 86, 280, "", &score_font_16, MUTED);
    static const char *menu[] = {TXT_NEW, TXT_UNDO};
    for (unsigned i = 0; i < 2; ++i) {
        s_menu_rows[i] = panel(s_dialog, 30, 26 + (int)i * 30, 228, 28);
        label(s_menu_rows[i], 3, 2, 222, menu[i], &score_font_16, INK);
    }
    s_target_range=(app_range_t){0};
    s_status = label(s_screen, 12, 156, 296, TXT_EMPTY, &score_font_16, INK);
    s_storage = label(s_screen, 12, 175, 296, "", &score_font_16, MUTED);
    return true;
}

void score_ui_render(const score_session_t *s, const score_controller_t *ui,
                     int height, int battery, const char *status, const char *storage) {
    lv_obj_set_height(s_board, height - 68);
    for (unsigned i = 0; i < 2; ++i) {
        lv_obj_set_height(s_cards[i], height - 76);
        lv_obj_set_y(s_points[i], 24 + (height - 192) / 2);
    }
    lv_obj_set_y(s_status, height - 36);
    lv_obj_set_y(s_storage, height - 17);
    if (battery < 0) lv_label_set_text(s_battery, "--%");
    else lv_label_set_text_fmt(s_battery, "%d%%", battery);
    lv_label_set_text(s_status, status);
    lv_label_set_text(s_storage, storage);
    const bool board = ui->page == SCORE_PAGE_BOARD;
    if (board) {
        lv_obj_remove_flag(s_board, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_dialog, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_board, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(s_dialog, LV_OBJ_FLAG_HIDDEN);
    }
    lv_label_set_text_fmt(s_race, TXT_RACE, (unsigned)s->state.target);
    for (unsigned i = 0; i < 2; ++i) {
        if (ui->page == SCORE_PAGE_SETTINGS) lv_obj_remove_flag(s_menu_rows[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s_menu_rows[i], LV_OBJ_FLAG_HIDDEN);
    }
    if(ui->page==SCORE_PAGE_TARGET && !s_target_range.root)
        app_range_create(&s_target_range,s_dialog,20,28,248,&score_font_16);
    if(ui->page!=SCORE_PAGE_TARGET && s_target_range.root) {
        lv_obj_delete(s_target_range.root); s_target_range=(app_range_t){0};
    }
    lv_label_set_text(s_detail, "");
    lv_label_set_text(s_target_hint, "");
    if (board) {
        for (unsigned i = 0; i < 2; ++i) {
            lv_label_set_text_fmt(s_points[i], "%ld", (long)s->state.points[i]);
            selected(s_cards[i], true, i == 0 ? GOLD : BLUE);
        }
    } else if (ui->page == SCORE_PAGE_SETTINGS) {
        lv_label_set_text(s_title, TXT_SETTINGS);
        for (unsigned i = 0; i < 2; ++i) selected(s_menu_rows[i], ui->settings_focus == i, GOLD);
    } else if (ui->page == SCORE_PAGE_TARGET) {
        lv_label_set_text(s_title, TXT_RACE_TITLE);
        const score_config_t *config = score_config_find(s->state.rule_id);
        char current[32],minimum[12],maximum[12];
        snprintf(current,sizeof(current),TXT_RACE,(unsigned)ui->target);
        snprintf(minimum,sizeof(minimum),"%u",config->min_target);
        snprintf(maximum,sizeof(maximum),"%u",config->max_target);
        app_range_render(&s_target_range,ui->target-config->min_target,config->max_target-config->min_target,
                         current,minimum,maximum,true);
    } else if (ui->page == SCORE_PAGE_NEW) {
        lv_label_set_text(s_title, TXT_NEW_TITLE);
        lv_label_set_text_fmt(s_detail, TXT_RACE, (unsigned)ui->target);
        lv_label_set_text(s_target_hint, TXT_NEW_NOTE);
    }
}
