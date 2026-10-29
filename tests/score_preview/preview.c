#include "lvgl.h"
#include "score/score.h"
#include "score/score_ui.h"
#include "score/score_strings.h"
#include "ui/app_shell.h"
#include "bsp_display_rounding.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
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
int main(void) {
    lv_init();lv_display_t *d=lv_display_create(240,320);
    static uint16_t draw[320*40];
    lv_display_set_buffers(d,draw,NULL,sizeof(draw),LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d,flush);
    lv_display_set_rotation(d,LV_DISPLAY_ROTATION_270);
    assert(score_ui_create());
    score_session_t s;score_session_init(&s);score_controller_t ui;score_controller_init(&ui,&s);
    for (unsigned r = 0; r < 2; ++r) {
        ui.rotation = r ? APP_ROTATION_90 : APP_ROTATION_270;
        const char *suffix = r ? "90" : "270";
        for (unsigned page = 0; page < SCORE_PAGE_COUNT; ++page) {
            ui.page = (score_page_t)page;
            ui.target = 35; ui.settings_focus = 2; ui.pending_rotation = ui.rotation;
            s.state.target = 35; s.state.points[0] = 10; s.state.points[1] = 9;
            score_ui_render(&s,&ui,85,TXT_RESTORED,TXT_SAVING_FAILED);
            char name[64]; snprintf(name,sizeof(name),"page-%u-%s",page,suffix); capture(name);
        }
    }
    lv_mem_monitor_t memory;lv_mem_monitor(&memory);
    const unsigned initial_free=memory.free_size;
    for(unsigned i=0;i<500;i++) {
        s.state.target=35;s.state.points[0]=i%36;s.state.points[1]=i%35;
        ui.page=(score_page_t)(i%SCORE_PAGE_COUNT);
        ui.rotation=i%2 ? APP_ROTATION_270 : APP_ROTATION_90;
        ui.settings_focus=i%3; ui.target=i%35+1;
        score_ui_render(&s,&ui,(int)(i%102)-1,TXT_AWARDED_TWO,
                        i%2 ? TXT_SAVING_FAILED : TXT_SAVED);
        lv_obj_invalidate(lv_screen_active());lv_refr_now(NULL);
    }
    lv_mem_monitor(&memory);
    assert(memory.free_size+1024>=initial_free);
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
    printf("LVGL max used: %u, remaining: %u, 500 renders stable\n",
           (unsigned)memory.max_used,(unsigned)memory.free_size);
}
