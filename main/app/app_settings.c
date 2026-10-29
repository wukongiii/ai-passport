#include "app_settings.h"
#include "app_settings_model.h"
#include "app_preferences_store.h"
#include "app_strings.h"
#include "../ui/app_range.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>
LV_FONT_DECLARE(score_font_16);
static struct {
    app_preferences_t preferences;
    app_settings_model_t model;
    bool dirty, initialized, active;
    int64_t last_save;
    int battery, millivolts;
    const char *version, *storage;
} state;
static lv_obj_t *heading, *name, *battery_label, *value, *hint, *status, *info;
static app_range_t range;
static const char *names[]={TXT_APP_DIRECTION,TXT_APP_BRIGHTNESS,TXT_APP_DIM,TXT_APP_BLANK,TXT_APP_INFO};
const app_preferences_t *app_settings_preferences(void) {return &state.preferences;}
uint8_t app_settings_preview_brightness(void) {
    return state.active && state.model.editing && state.model.focus==APP_SETTING_BRIGHTNESS ? state.model.draft.brightness : state.preferences.brightness;
}
const char *app_settings_storage_status(void) {return state.storage ? state.storage : "";}
void app_settings_device_info(int battery, int mv, const char *version) {state.battery=battery;state.millivolts=mv;state.version=version;}
static void init(void *c) {
    (void)c; if(state.initialized) return;
    app_pref_store_status_t result=app_preferences_load(&state.preferences);
    state.storage=result==APP_PREF_INVALID ? TXT_APP_PREF_BAD : result==APP_PREF_UNAVAILABLE ? TXT_APP_PREF_FAILED : "";
    state.dirty=result==APP_PREF_MIGRATED; state.initialized=true;
}
static bool enter(void *c, app_rotation_t r) {
    (void)c; (void)r; state.active=true;
    app_settings_model_enter(&state.model,&state.preferences);return true;
}
static bool leave(void *c) {(void)c;state.active=false;state.model.editing=false;return true;}
enum { PREV=100,NEXT,OPEN,BACK,MINUS,PLUS,APPLY,LEFT,RIGHT };
static void bindings(void *c, app_bindings_t *b) {
    (void)c; app_actions_clear(b);
    const bool edit=state.model.editing;
    if(edit) {
        const bool direction=state.model.focus==APP_SETTING_DIRECTION;
        const unsigned index=app_settings_range_index(&state.model,&state.preferences);
        const unsigned maximum=state.model.focus==APP_SETTING_BRIGHTNESS ? 8 : 4;
        app_actions_register(b,0,APP_GESTURE_SHORT,direction ? LEFT : MINUS,direction ? "270°" : TXT_APP_REDUCE,direction || index>0,APP_ACTION_NAVIGATE);
        app_actions_register(b,1,APP_GESTURE_SHORT,APPLY,TXT_APP_CONFIRM,true,APP_ACTION_CONFIRM);
        app_actions_register(b,2,APP_GESTURE_SHORT,direction ? RIGHT : PLUS,direction ? "90°" : TXT_APP_INCREASE,direction || index<maximum,APP_ACTION_NAVIGATE);
        app_actions_register(b,1,APP_GESTURE_LONG,BACK,TXT_APP_CANCEL,true,APP_ACTION_CANCEL);
    } else {
        app_actions_register(b,0,APP_GESTURE_SHORT,PREV,TXT_APP_PREVIOUS,true,APP_ACTION_NAVIGATE);
        app_actions_register(b,1,APP_GESTURE_SHORT,OPEN,TXT_APP_ADJUST,state.model.focus!=APP_SETTING_INFO,APP_ACTION_CONFIRM);
        app_actions_register(b,2,APP_GESTURE_SHORT,NEXT,TXT_APP_NEXT,true,APP_ACTION_NAVIGATE);
    }
}
static app_module_effect_t input(void *c, app_rotation_t rotation, unsigned slot, app_gesture_t gesture) {
    (void)c;
    if(app_settings_model_input(&state.model,&state.preferences,slot,gesture)) {state.dirty=true;state.last_save=0;}
    return (app_module_effect_t){state.preferences.rotation!=rotation,state.preferences.rotation};
}
static bool tick(void *c, int64_t now) {
    (void)c;
    if(!state.dirty || (state.last_save && now-state.last_save<5000000)) return false;
    state.last_save=now;state.dirty=app_preferences_save(&state.preferences)!=ESP_OK;
    state.storage=state.dirty ? TXT_APP_PREF_FAILED : TXT_APP_PREF_SAVED;return true;
}
static lv_obj_t *label(lv_obj_t *p,int x,int y,int w,uint32_t color) {
    lv_obj_t *o=lv_label_create(p);lv_obj_set_pos(o,x,y);lv_obj_set_width(o,w);
    lv_obj_set_style_text_font(o,&score_font_16,0);lv_obj_set_style_text_color(o,lv_color_hex(color),0);
    lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);lv_label_set_long_mode(o,LV_LABEL_LONG_CLIP);lv_label_set_text(o,"");return o;
}
static bool create(void *c, void *parent) {
    (void)c;
    heading=label(parent,22,7,210,0xEDC56B);battery_label=label(parent,244,9,54,0xA6B5B7);
    name=label(parent,20,37,280,0xF2F3E9);value=label(parent,20,85,280,0xEDC56B);
    app_range_create(&range,parent,36,70,248,&score_font_16);
    info=label(parent,20,75,280,0xF2F3E9);lv_label_set_long_mode(info,LV_LABEL_LONG_WRAP);
    hint=label(parent,16,168,288,0xA6B5B7);status=label(parent,16,190,288,0xA6B5B7);return true;
}
static void visible(lv_obj_t *o,bool show) {if(show)lv_obj_remove_flag(o,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN);}
static void render(void *c,const app_module_view_t *v) {
    (void)c;
    const app_settings_model_t *m=&state.model;
    const app_preferences_t *p=m->editing ? &m->draft : &state.preferences;
    const bool is_range=m->focus>=APP_SETTING_BRIGHTNESS && m->focus<=APP_SETTING_BLANK;
    lv_label_set_text(heading,TXT_APP_SETTINGS);lv_label_set_text(name,names[m->focus]);
    if(v->battery<0)lv_label_set_text(battery_label,"--%");else lv_label_set_text_fmt(battery_label,"%d%%",v->battery);
    visible(range.root,is_range);visible(value,m->focus==APP_SETTING_DIRECTION);visible(info,m->focus==APP_SETTING_INFO);
    if(is_range) {
        char current[32];const bool brightness=m->focus==APP_SETTING_BRIGHTNESS;
        unsigned seconds=m->focus==APP_SETTING_DIM ? p->dim_seconds : p->blank_seconds;
        if(brightness)snprintf(current,sizeof(current),"%u%%",p->brightness);
        else if(seconds)snprintf(current,sizeof(current),TXT_APP_SECONDS,seconds);
        else snprintf(current,sizeof(current),"%s",TXT_APP_UNLIMITED);
        app_range_render(&range,app_settings_range_index(m,&state.preferences),brightness ? 8 : 4,current,
            brightness ? "20%" : m->focus==APP_SETTING_DIM ? "15 s" : "60 s",brightness ? "100%" : TXT_APP_UNLIMITED,m->editing);
    } else if(m->focus==APP_SETTING_DIRECTION) lv_label_set_text(value,p->rotation==APP_ROTATION_270 ? TXT_APP_ROTATE_TOP : TXT_APP_ROTATE_BOTTOM);
    else {
        char bat[80];
        if(state.battery<0 || state.millivolts<0)snprintf(bat,sizeof(bat),"%s",TXT_APP_INFO_UNKNOWN);
        else snprintf(bat,sizeof(bat),TXT_APP_INFO_BATTERY,state.battery,state.millivolts);
        lv_label_set_text_fmt(info,TXT_APP_VERSION "\n%s\n" TXT_APP_DEVICE,state.version ? state.version : "--",bat);
    }
    lv_label_set_text(hint,m->focus==APP_SETTING_INFO ? "" : m->editing ? TXT_APP_EDIT_HINT : TXT_APP_SETTING_HINT);
    lv_obj_set_y(hint,v->content_height-38);lv_obj_set_y(status,v->content_height-18);
    lv_label_set_text(status,state.dirty || (state.storage && strcmp(state.storage,TXT_APP_PREF_SAVED)!=0) ? app_settings_storage_status() : "");
}
static const app_module_ops_t ops={enter,leave,bindings,input,tick,create,render,init};
const app_module_t app_module_settings={4,TXT_APP_SETTINGS,TXT_APP_SETTINGS_DESC,APP_ENTRY_SETTINGS,&ops,NULL,APP_ART_SETTINGS};
