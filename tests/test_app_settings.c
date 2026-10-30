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
    assert(!m.submenu&&m.group==APP_SETTINGS_DISPLAY);
    app_settings_model_input(&m,&p,1,APP_GESTURE_SHORT);assert(m.submenu&&!m.editing);
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
    app_settings_model_enter(&m,&p);m.submenu=true;p.rotation=APP_ROTATION_90;m.draft=p;
    app_settings_model_input(&m,&p,1,APP_GESTURE_SHORT);
    app_settings_model_input(&m,&p,2,APP_GESTURE_SHORT);
    assert(m.draft.rotation==APP_ROTATION_270 && p.rotation==APP_ROTATION_90);
    assert(app_settings_model_input(&m,&p,1,APP_GESTURE_SHORT) && p.rotation==APP_ROTATION_270);
    app_settings_model_enter(&m,&p);m.submenu=true;m.group=APP_SETTINGS_SOUND;m.focus=APP_SETTING_VOLUME;
    app_settings_model_input(&m,&p,1,APP_GESTURE_SHORT);
    app_settings_model_input(&m,&p,0,APP_GESTURE_SHORT);
    assert(m.draft.volume==50&&p.volume==APP_VOLUME_DEFAULT);
    app_settings_model_input(&m,&p,1,APP_GESTURE_LONG);assert(p.volume==APP_VOLUME_DEFAULT);
    app_settings_model_input(&m,&p,1,APP_GESTURE_SHORT);
    for(unsigned i=0;i<20;++i)app_settings_model_input(&m,&p,2,APP_GESTURE_SHORT);
    assert(m.draft.volume==100&&app_settings_range_index(&m,&p)==10);
    assert(app_settings_model_input(&m,&p,1,APP_GESTURE_SHORT)&&p.volume==100);
    app_preferences_encode(&p,b);assert(app_preferences_decode(&decoded,b,sizeof(b))&&decoded.volume==100);
    // Previous format retains display settings and supplies the default volume.
    b[2]=1;b[9]=b[10]=b[11]=0;b[15]=0;for(unsigned i=0;i<15;++i)b[15]^=b[i];
    assert(app_preferences_decode(&decoded,b,sizeof(b))&&decoded.volume==APP_VOLUME_DEFAULT&&decoded.brightness==p.brightness);
    // Version 2 keeps volume/display and defaults the new operation preference.
    app_preferences_encode(&p,b);b[2]=2;b[10]=b[11]=0;b[15]=0;for(unsigned i=0;i<15;++i)b[15]^=b[i];
    assert(app_preferences_decode(&decoded,b,sizeof(b))&&decoded.volume==100&&decoded.double_click_ms==350);
    app_settings_model_enter(&m,&p);
    for(unsigned i=0;i<2;++i)app_settings_model_input(&m,&p,2,APP_GESTURE_SHORT);
    assert(m.group==APP_SETTINGS_OPERATION&&!m.submenu);
    app_settings_model_input(&m,&p,1,APP_GESTURE_SHORT);assert(m.focus==APP_SETTING_DOUBLE&&m.submenu);
    app_settings_model_input(&m,&p,1,APP_GESTURE_SHORT);
    app_settings_model_input(&m,&p,2,APP_GESTURE_SHORT);assert(m.draft.double_click_ms==400&&p.double_click_ms==350);
    assert(app_settings_model_input(&m,&p,1,APP_GESTURE_SHORT)&&p.double_click_ms==400);
    app_settings_model_input(&m,&p,1,APP_GESTURE_LONG);assert(!m.submenu&&m.group==APP_SETTINGS_OPERATION);
    puts("Settings codec, corruption, idle thresholds, preview/cancel/confirm and infinity endpoint: PASS");
}
