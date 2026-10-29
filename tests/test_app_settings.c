#include "app/app_settings_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    app_preferences_t p=app_preferences_default(),decoded=p;
    assert(app_preferences_valid(&p));
    uint8_t b[APP_PREFERENCES_SIZE];app_preferences_encode(&p,b);
    assert(app_preferences_decode(&decoded,b,sizeof(b)));
    for(unsigned i=0;i<sizeof(b);++i) {b[i]^=1;assert(!app_preferences_decode(&decoded,b,sizeof(b)));b[i]^=1;}
    assert(app_preferences_backlight(&p,29999999)==80);
    assert(app_preferences_backlight(&p,30000000)==15);
    assert(app_preferences_backlight(&p,120000000)==0);
    p.dim_seconds=p.blank_seconds=0;assert(app_preferences_backlight(&p,99999999999)==80);
    app_settings_model_t m;app_settings_model_enter(&m,&p);
    app_settings_model_input(&m,&p,1,APP_GESTURE_SHORT);
    app_settings_model_input(&m,&p,2,APP_GESTURE_SHORT);
    assert(p.rotation==APP_ROTATION_270 && m.draft.rotation==APP_ROTATION_90);
    app_settings_model_input(&m,&p,1,APP_GESTURE_LONG);assert(p.rotation==APP_ROTATION_270 && !m.editing);
    app_settings_model_input(&m,&p,2,APP_GESTURE_SHORT);assert(m.focus==APP_SETTING_BRIGHTNESS);
    app_settings_model_input(&m,&p,1,APP_GESTURE_SHORT);
    for(unsigned i=0;i<4;++i)app_settings_model_input(&m,&p,2,APP_GESTURE_SHORT);
    assert(m.draft.brightness==100 && p.brightness==80);
    assert(app_settings_model_input(&m,&p,1,APP_GESTURE_SHORT) && p.brightness==100);
    app_settings_model_input(&m,&p,2,APP_GESTURE_SHORT);assert(m.focus==APP_SETTING_DIM);
    app_settings_model_input(&m,&p,1,APP_GESTURE_SHORT);
    assert(app_settings_range_index(&m,&p)==4);
    app_settings_model_input(&m,&p,2,APP_GESTURE_SHORT);assert(m.draft.dim_seconds==0);
    app_settings_model_input(&m,&p,0,APP_GESTURE_SHORT);assert(m.draft.dim_seconds==120);
    app_settings_model_input(&m,&p,0,APP_GESTURE_SHORT);assert(m.draft.dim_seconds==60);
    app_settings_model_input(&m,&p,1,APP_GESTURE_LONG);assert(p.dim_seconds==0);
    puts("Settings codec, corruption, idle thresholds, preview/cancel/confirm and infinity endpoint: PASS");
}
