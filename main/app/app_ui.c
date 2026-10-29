#include "app_ui.h"
#include "../ai/app_game_voice.h"
#include "app_strings.h"
#include "app_carousel.h"
#include "../ui/app_header.h"
#include "../ui/app_shell.h"
#include "lvgl.h"

LV_FONT_DECLARE(score_font_16);
LV_FONT_DECLARE(network_font_14);
static app_shell_t shell;
static const app_module_t *mounted;
static app_page_t mounted_page;
static lv_obj_t *detail, *note, *counter;
static bool failed;
static lv_obj_t *voice_status;

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
    ai_game_target_t voice_target=app_container_ai_target(app);
    app_game_voice_bindings(voice_target,&bindings);
    app_shell_render(&shell, app->rotation, &bindings);
    const app_module_t *active = app_container_active(app);
    if (mounted_page != app->page || mounted != active) {
        app_carousel_destroy();
        lv_obj_clean(shell.content);
        mounted_page = app->page; mounted = active; failed = false;voice_status=NULL;
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
        game_voice_snapshot_t voice;app_game_voice_snapshot(&voice);
        uint32_t voice_revision,voice_configuration;
        if(voice_target.ops && voice_target.ops->stamp(voice_target.context,&voice_revision,&voice_configuration) && voice.state!=GAME_VOICE_IDLE){
            if(!voice_status){voice_status=label(shell.content,12,0,296,"",0xF2F3E9);lv_obj_set_style_text_font(voice_status,&network_font_14,0);lv_obj_set_height(voice_status,32);lv_obj_set_style_bg_opa(voice_status,LV_OPA_COVER,0);lv_obj_set_style_radius(voice_status,5,0);lv_label_set_long_mode(voice_status,LV_LABEL_LONG_SCROLL_CIRCULAR);}
            lv_obj_remove_flag(voice_status,LV_OBJ_FLAG_HIDDEN);lv_obj_set_y(voice_status,app_content_rect(app->rotation,app_binding_rows(&bindings)).height-36);
            lv_obj_set_style_bg_color(voice_status,lv_color_hex(voice.state==GAME_VOICE_RECORDING ? 0x934C32 : voice.state==GAME_VOICE_ERROR ? 0x48545C : 0x246B4E),0);
            if(voice.state==GAME_VOICE_RECORDING)lv_label_set_text_fmt(voice_status,TXT_GAME_VOICE_RECORD,voice.milliseconds/1000);
            else lv_label_set_text(voice_status,voice.state==GAME_VOICE_CANCELLING ? TXT_GAME_VOICE_STOP : voice.text);
        }else if(voice_status)lv_obj_add_flag(voice_status,LV_OBJ_FLAG_HIDDEN);
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
