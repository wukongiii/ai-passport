#include "lvgl.h"
#include "score/score.h"
#include "score/score_ui.h"
#include "score/score_strings.h"
#include "ui/app_shell.h"
#include "app/app_ui.h"
#include "app/app_strings.h"
#include "games/eight_module.h"
#include "score/score_store.h"
#include "bsp_display_rounding.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
// In-memory storage adapter for the actual game module in host rendering.
score_store_status_t score_store_open(score_session_t *s) { (void)s; return SCORE_STORE_EMPTY; }
esp_err_t score_store_save(const score_session_t *s) { (void)s; return ESP_OK; }
static unsigned char frame[320*240*3];
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *data) {
    const uint16_t *p=(const uint16_t *)data;
    for(int y=a->y1;y<=a->y2;y++) for(int x=a->x1;x<=a->x2;x++) {
        uint16_t v=*p++; size_t at=(y*320+x)*3;
        if (bsp_display_pixel_outside_rounded_rect(x,y,320,240,30)) v=0;
        frame[at]=((v>>11)&31)*255/31; frame[at+1]=((v>>5)&63)*255/63; frame[at+2]=(v&31)*255/31;
    }
    lv_display_flush_ready(d);
}
static void capture(const char *name) {
    lv_obj_invalidate(lv_screen_active()); lv_refr_now(NULL);
    char path[256];snprintf(path,sizeof(path),"%s.ppm",name);
    FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n320 240\n255\n");
    fwrite(frame,1,sizeof(frame),f);fclose(f);
}
static void advance(unsigned ms) {
    for (unsigned t=0; t<ms; t+=20) { lv_tick_inc(20); lv_timer_handler(); }
}
int main(void) {
    lv_init();lv_display_t *d=lv_display_create(240,320);
    static uint16_t draw[320*40];
    lv_display_set_buffers(d,draw,NULL,sizeof(draw),LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d,flush);
    lv_display_set_rotation(d,LV_DISPLAY_ROTATION_270);
    extern const lv_font_t score_font_16;
    app_shell_t game_shell; app_shell_create(&game_shell, &score_font_16);
    lv_screen_load(game_shell.screen);
    assert(score_ui_create(game_shell.content));
    score_session_t s;score_session_init(&s);score_controller_t ui;score_controller_init(&ui,&s);
    for (unsigned r = 0; r < 2; ++r) {
        ui.rotation = r ? APP_ROTATION_90 : APP_ROTATION_270;
        const char *suffix = r ? "90" : "270";
        for (unsigned page = 0; page < SCORE_PAGE_COUNT; ++page) {
            ui.page = (score_page_t)page;
            ui.target = 35; ui.settings_focus = 2; ui.pending_rotation = ui.rotation;
            s.state.target = 35; s.state.points[0] = 10; s.state.points[1] = 9;
            app_bindings_t b; score_controller_bindings(&ui, &s, &b);
            app_shell_render(&game_shell, ui.rotation, &b);
            score_ui_render(&s,&ui,app_content_rect(ui.rotation,app_binding_rows(&b)).height,
                            85,TXT_RESTORED,TXT_SAVING_FAILED);
            char name[64]; snprintf(name,sizeof(name),"page-%u-%s",page,suffix); capture(name);
        }
    }
    // Exercise all nine shared-shell labels with a synthetic future module.
    lv_obj_t *old = lv_screen_active();
    extern const lv_font_t score_font_16;
    app_shell_t shell; app_shell_create(&shell, &score_font_16);
    lv_screen_load(shell.screen); lv_obj_delete(old);
    app_bindings_t bindings = {0};
    for (unsigned g=0; g<APP_GESTURE_COUNT; ++g)
        for (unsigned k=0; k<APP_KEY_COUNT; ++k)
            bindings.actions[g][k] = (app_action_hint_t){TXT_TARGET_BACK, true, 1, APP_ACTION_BACK};
    app_shell_render(&shell, APP_ROTATION_270, &bindings); capture("nine-actions-270");
    app_shell_render(&shell, APP_ROTATION_90, &bindings); capture("nine-actions-90");
    // Render each key container page once per orientation, without stress loops.
    old = lv_screen_active();
    assert(app_ui_create()); lv_obj_delete(old);
    unsigned count; const app_module_t *modules = app_registry(&count);
    app_modules_init(modules,count);
    app_container_t app; assert(app_container_init(&app, modules, count, APP_ROTATION_270));
    for (unsigned r=0; r<2; ++r) {
        app.rotation=r ? APP_ROTATION_90 : APP_ROTATION_270;
        app.page=APP_PAGE_HOME; app.focus=0; app_ui_render(&app,85,"");
        capture(r ? "launcher-90" : "launcher-270");
        app.focus=4; app_ui_render(&app,85,""); advance(260); capture(r ? "services-90" : "services-270");
        app.page=APP_PAGE_PLACEHOLDER; app_ui_render(&app,85,"");
        capture(r ? "mock-90" : "mock-270");
    }
    // One wrap, reversal during motion, then exit mid-animation.
    app.page=APP_PAGE_HOME; app.focus=0; app_ui_render(&app,85,"");
    app_container_input(&app,2,APP_GESTURE_SHORT); app_ui_render(&app,85,"");
    advance(100); capture("carousel-mid");
    advance(160); capture("carousel-chase");
    app_container_input(&app,0,APP_GESTURE_SHORT); app_ui_render(&app,85,"");
    advance(60);
    app_container_input(&app,0,APP_GESTURE_SHORT); app_ui_render(&app,85,"");
    advance(260); capture("carousel-wrap");
    app_container_input(&app,2,APP_GESTURE_SHORT); app_ui_render(&app,85,"");
    app_container_input(&app,1,APP_GESTURE_SHORT); app_ui_render(&app,85,"");
    advance(260); // Deleted cards must not be touched by the old callback.
    app_container_input(&app,1,APP_GESTURE_LONG); app_ui_render(&app,85,"");
    // One real entry/scoring/exit path; no default repetition loop.
    app.page=APP_PAGE_HOME; app.focus=0;
    app_container_input(&app,1,APP_GESTURE_SHORT); app_ui_render(&app,85,"");
    app_container_input(&app,0,APP_GESTURE_SHORT); app_ui_render(&app,85,""); capture("container-eight");
    app_container_input(&app,1,APP_GESTURE_LONG); app_ui_render(&app,85,""); capture("return-home");
    lv_mem_monitor_t memory; lv_mem_monitor(&memory);
    printf("LVGL max used: %u, remaining: %u, key screens rendered\n",
           (unsigned)memory.max_used,(unsigned)memory.free_size);
}
