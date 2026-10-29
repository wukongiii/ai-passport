#include "app_ui.h"
#include "app_strings.h"
#include "app_carousel.h"
#include "../ui/app_header.h"
#include "../ui/app_shell.h"
#include "lvgl.h"

LV_FONT_DECLARE(score_font_16);
static app_shell_t shell;
static const app_module_t *mounted;
static app_page_t mounted_page;
static lv_obj_t *detail, *note, *counter;
static bool failed;

static lv_obj_t *label(lv_obj_t *parent, int x, int y, int width, const char *text, uint32_t color) {
    lv_obj_t *o = lv_label_create(parent);
    lv_obj_set_pos(o, x, y); lv_obj_set_width(o, width);
    lv_obj_set_style_text_font(o, &score_font_16, 0);
    lv_obj_set_style_text_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(o, LV_LABEL_LONG_CLIP);
    lv_label_set_text(o, text);
    return o;
}

bool app_ui_create(void) {
    extern const uint32_t score_font_codepoints[];
    extern const unsigned score_font_codepoint_count;
    for (unsigned i = 0; i < score_font_codepoint_count; ++i) {
        lv_font_glyph_dsc_t glyph = {0};
        if (!lv_font_get_glyph_dsc(&score_font_16, &glyph, score_font_codepoints[i], 0) || glyph.is_placeholder)
            return false;
    }
    app_shell_create(&shell, &score_font_16);
    app_header_create(shell.screen);
    mounted = NULL; mounted_page = (app_page_t)-1;
    lv_screen_load(shell.screen);
    return true;
}

static void create_container_page(const app_container_t *app) {
    if (app->page == APP_PAGE_HOME)
        app_carousel_create(shell.content, app->modules, app->count, app->focus);
    detail = label(shell.content, 18, 178, 284, "", 0xB5C9C9);
    note = label(shell.content, 18, 115, 284, "", 0xA6B5B7);
    counter = label(shell.content, 115, 199, 90, "", 0x89A8AB);
}

void app_ui_render(const app_container_t *app, int battery, const char *overlay) {
    app_bindings_t bindings; app_container_bindings(app, &bindings);
    app_shell_render(&shell, app->rotation, &bindings);
    const app_module_t *active = app_container_active(app);
    if (mounted_page != app->page || mounted != active) {
        app_carousel_destroy();
        lv_obj_clean(shell.content);
        mounted_page = app->page; mounted = active; failed = false;
        if (app->page == APP_PAGE_MODULE) {
            failed = !active->ops->ui_create(active->context, shell.content);
            if (failed) label(shell.content, 20, 70, 280, TXT_APP_UI_FAILED, 0xEDC56B);
        }
        else create_container_page(app);
    }
    app_header_context("");
    if (app->page == APP_PAGE_MODULE) {
        if (!failed) {
            const app_module_view_t view = {app->rotation,
                app_content_rect(app->rotation, app_binding_rows(&bindings)).height, battery, overlay};
            active->ops->ui_render(active->context, &view);
        }
        app_header_render(app_content_rect(app->rotation,app_binding_rows(&bindings)).y,active->title,battery,false);
        return;
    }
    lv_label_set_text(note, overlay && *overlay ? overlay : "");
    if (app->page == APP_PAGE_HOME) {
        app_carousel_select(app->focus);
        lv_label_set_text_fmt(counter, "%u / %u", app->focus + 1, app->count);
        lv_obj_set_pos(note, 18, 199);
        if (overlay && *overlay) lv_obj_add_flag(counter, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_remove_flag(counter, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(detail, app->modules[app->focus].description);
    } else {
        lv_obj_set_pos(note, 18, 80);
        lv_label_set_text(note, TXT_APP_MOCK);
        lv_obj_set_pos(detail, 18, 119);
        lv_label_set_text(detail, TXT_APP_MOCK_NOTE);
    }
    app_header_render(app_content_rect(app->rotation,app_binding_rows(&bindings)).y,active ? active->title : "",battery,app->page==APP_PAGE_HOME);
}
