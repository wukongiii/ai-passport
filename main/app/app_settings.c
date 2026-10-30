#include "app_settings.h"
#include "app_settings_model.h"
#include "app_preferences_store.h"
#include "app_strings.h"
#include "../speech/app_speech.h"
#include "../network/app_network.h"
#include "../network/network_setup_ui.h"
#include "../ui/app_header.h"
#include "../ui/app_range.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>
LV_FONT_DECLARE(score_font_16);
LV_FONT_DECLARE(score_font_14);
LV_FONT_DECLARE(network_font_14);
static struct {
    app_preferences_t preferences;
    app_settings_model_t model;
    bool dirty, initialized, active, network_page;
    net_setup_t setup;uint64_t now,last_tick;bool queue_error;
    app_network_snapshot_t network;
    int64_t last_save;
    int battery, millivolts;
    const char *version, *storage;
} state;
static lv_obj_t *content,*name,*value,*hint,*status,*info,*direction_rows[2],*group_rows[3];
static net_setup_ui_t network_ui;
static bool network_mounted;
static void create_settings_content(void);
static app_range_t range;
static const char *names[]={TXT_APP_DIRECTION,TXT_APP_BRIGHTNESS,TXT_APP_DIM,TXT_APP_BLANK,TXT_APP_INFO,TXT_APP_NETWORK,TXT_APP_VOLUME,TXT_SETTINGS_DOUBLE};
static const char *groups[]={TXT_SETTINGS_DISPLAY,TXT_SETTINGS_SOUND,TXT_SETTINGS_OPERATION,TXT_SETTINGS_ABOUT,TXT_SETTINGS_NETWORK};
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
    state.network_page=false;net_setup_dispose(&state.setup);app_network_snapshot(&state.network);
    app_settings_model_enter(&state.model,&state.preferences);return true;
}
static bool leave(void *c) {(void)c;state.active=false;state.model.editing=false;state.network_page=false;net_setup_dispose(&state.setup);return true;}
enum { PREV=100,NEXT,OPEN,BACK,MINUS,PLUS,APPLY,LEFT,RIGHT,PORTAL,RETRY,FORGET };
static void bindings(void *c, app_bindings_t *b) {
    (void)c; app_actions_clear(b);
    if(state.network_page){net_setup_bindings(&state.setup,&state.network,b);return;}
    if(state.model.submenu && !state.model.editing)app_actions_register(b,1,APP_GESTURE_LONG,BACK,TXT_APP_BACK,true,APP_ACTION_BACK);
    const bool edit=state.model.editing;
    if(edit) {
        const bool direction=state.model.focus==APP_SETTING_DIRECTION;
        const unsigned index=app_settings_range_index(&state.model,&state.preferences);
        const unsigned maximum=state.model.focus==APP_SETTING_DOUBLE ? (APP_DOUBLE_MAX-APP_DOUBLE_MIN)/APP_DOUBLE_STEP : state.model.focus==APP_SETTING_VOLUME ? 100/APP_VOLUME_STEP : state.model.focus==APP_SETTING_BRIGHTNESS ? 8 : 4;
        app_actions_register(b,0,APP_GESTURE_SHORT,direction ? LEFT : MINUS,direction ? TXT_APP_PREVIOUS : TXT_APP_REDUCE,direction || index>0,APP_ACTION_NAVIGATE);
        app_actions_register(b,1,APP_GESTURE_SHORT,APPLY,TXT_APP_CONFIRM,true,APP_ACTION_CONFIRM);
        app_actions_register(b,2,APP_GESTURE_SHORT,direction ? RIGHT : PLUS,direction ? TXT_APP_NEXT : TXT_APP_INCREASE,direction || index<maximum,APP_ACTION_NAVIGATE);
        app_actions_register(b,1,APP_GESTURE_LONG,BACK,TXT_APP_CANCEL,true,APP_ACTION_CANCEL);
    } else {
        app_actions_register(b,0,APP_GESTURE_SHORT,PREV,TXT_APP_PREVIOUS,!state.model.submenu || state.model.group==APP_SETTINGS_DISPLAY,APP_ACTION_NAVIGATE);
        app_actions_register(b,1,APP_GESTURE_SHORT,OPEN,!state.model.submenu || state.model.focus==APP_SETTING_NETWORK ? TXT_APP_ENTER : TXT_APP_ADJUST,!state.model.submenu || state.model.focus!=APP_SETTING_INFO,APP_ACTION_CONFIRM);
        app_actions_register(b,2,APP_GESTURE_SHORT,NEXT,TXT_APP_NEXT,!state.model.submenu || state.model.group==APP_SETTINGS_DISPLAY,APP_ACTION_NAVIGATE);
    }
}
static app_module_effect_t input(void *c, app_rotation_t rotation, unsigned slot, app_gesture_t gesture) {
    (void)c;
    if(state.network_page){
        net_setup_effect_t effect=net_setup_input(&state.setup,&state.network,slot,gesture);
        state.queue_error=false;
        switch(effect){
        case NET_SETUP_EXIT:state.network_page=false;net_setup_dispose(&state.setup);break;
        case NET_SETUP_SCAN:state.queue_error=!app_network_scan();break;
        case NET_SETUP_PORTAL:state.queue_error=!app_network_portal(!state.network.portal);break;
        case NET_SETUP_RETRY:state.queue_error=!app_network_retry();break;
        case NET_SETUP_ERASE:state.queue_error=!app_network_forget();break;
        case NET_SETUP_CONNECT_SAVED:
            state.queue_error=!app_network_connect_saved(state.setup.credentials.ssid);
            if(state.queue_error)state.setup.page=NET_SETUP_CONFIRM;
            else memset(&state.setup.credentials,0,sizeof(state.setup.credentials));
            break;
        case NET_SETUP_CONNECT:
            state.queue_error=!app_network_connect(&state.setup.credentials);
            if(state.queue_error)state.setup.page=NET_SETUP_CONFIRM;
            else memset(&state.setup.credentials,0,sizeof(state.setup.credentials));
            break;
        default:break;
        }
        return (app_module_effect_t){false,rotation};
    }
    if(state.model.submenu && !state.model.editing && state.model.focus==APP_SETTING_NETWORK && slot==1 && gesture==APP_GESTURE_SHORT){
        state.network_page=true;net_setup_begin(&state.setup);return (app_module_effect_t){false,rotation};
    }
    if(app_settings_model_input(&state.model,&state.preferences,slot,gesture)) {state.dirty=true;state.last_save=0;}
    return (app_module_effect_t){state.preferences.rotation!=rotation,state.preferences.rotation};
}
static bool tick(void *c, int64_t now) {
    (void)c;
    bool changed=false;state.now=(uint64_t)now/1000;state.setup.now=state.now;
    if(state.network_page){
        app_net_scan_t scan;app_network_scan_snapshot(&scan);
        if(scan.revision!=state.setup.scan.revision){net_setup_update(&state.setup,&scan);changed=state.active;}
        app_net_saved_t saved;app_network_saved_snapshot(&saved);
        if(saved.revision!=state.setup.saved.revision){net_setup_update_saved(&state.setup,&saved);changed=state.active;}
        const text_input_t *t=&state.setup.editor;
        if(t->config.secret && !t->revealed && state.last_tick<t->reveal_until_ms && state.now>=t->reveal_until_ms)changed=true;
    }
    state.last_tick=state.now;
    app_network_snapshot_t net;app_network_snapshot(&net);
    if(net.revision!=state.network.revision){state.network=net;changed=state.active;}
    app_preferences_t received;
    if(app_network_take_preferences(&received)){
        state.preferences=received;state.model.editing=false;state.model.draft=received;
        state.dirty=true;state.last_save=0;changed=true;
    }
    app_network_publish_preferences(&state.preferences);
    app_speech_set_volume(state.preferences.volume);
    if(!state.dirty || (state.last_save && now-state.last_save<5000000)) return changed;
    state.last_save=now;state.dirty=app_preferences_save(&state.preferences)!=ESP_OK;
    state.storage=state.dirty ? TXT_APP_PREF_FAILED : TXT_APP_PREF_SAVED;return true;
}
static lv_obj_t *label(lv_obj_t *p,int x,int y,int w,uint32_t color) {
    lv_obj_t *o=lv_label_create(p);lv_obj_set_pos(o,x,y);lv_obj_set_width(o,w);
    lv_obj_set_style_text_font(o,&score_font_16,0);lv_obj_set_style_text_color(o,lv_color_hex(color),0);
    lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);lv_label_set_long_mode(o,LV_LABEL_LONG_CLIP);lv_label_set_text(o,"");return o;
}
static void create_settings_content(void) {
    lv_obj_t *parent=content;
    name=label(parent,20,48,280,0xF2F3E9);value=label(parent,20,80,280,0xF2F3E9);
    lv_obj_set_height(value,network_font_14.line_height);
    lv_label_set_long_mode(value,LV_LABEL_LONG_MODE_DOTS);
    app_range_create(&range,parent,36,70,248,&score_font_16);
    info=label(parent,20,75,280,0xF2F3E9);lv_label_set_long_mode(info,LV_LABEL_LONG_WRAP);
    hint=label(parent,16,168,288,0xA6B5B7);status=label(parent,16,190,288,0xA6B5B7);
    for(unsigned i=0;i<3;++i){group_rows[i]=label(parent,22,35+(int)i*41,276,0xF2F3E9);
        lv_obj_set_height(group_rows[i],35);lv_obj_set_style_pad_top(group_rows[i],7,0);
        lv_obj_set_style_radius(group_rows[i],6,0);lv_obj_set_style_bg_opa(group_rows[i],LV_OPA_COVER,0);}
    for(unsigned i=0;i<2;++i){direction_rows[i]=label(parent,22,65+(int)i*42,276,0xF2F3E9);
        lv_obj_set_height(direction_rows[i],34);lv_obj_set_style_pad_top(direction_rows[i],8,0);
        lv_obj_set_style_radius(direction_rows[i],6,0);lv_obj_set_style_bg_opa(direction_rows[i],LV_OPA_COVER,0);}
}
static bool create(void *c,void *parent){(void)c;content=parent;network_mounted=false;create_settings_content();return true;
}
static void visible(lv_obj_t *o,bool show) {if(show)lv_obj_remove_flag(o,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN);}
static const char *network_status(void){
    return state.network.state==APP_NET_CONNECTED ? TXT_APP_NETWORK_SUCCESS : state.network.state==APP_NET_CONNECTING ? TXT_APP_NETWORK_CONNECTING : state.network.state==APP_NET_FAILED ? TXT_APP_NETWORK_FAILED : TXT_APP_NETWORK_IDLE;
}
static void key(void *c,unsigned slot,bool pressed,int64_t now){(void)c;(void)slot;(void)pressed;state.now=(uint64_t)now/1000;state.setup.now=state.now;}
static void render(void *c,const app_module_view_t *v) {
    (void)c;
    const app_settings_model_t *m=&state.model;
    const app_preferences_t *p=m->editing ? &m->draft : &state.preferences;
    if(state.network_page){
        if(!network_mounted){lv_obj_clean(content);net_setup_ui_create(&network_ui,content);network_mounted=true;}
        net_setup_ui_render(&network_ui,&state.setup,&state.network,v->content_height);
        if(state.queue_error && !network_ui.keyboard)lv_label_set_text(network_ui.note,TXT_APP_NET_BUSY);
        return;
    }
    if(network_mounted){lv_obj_clean(content);create_settings_content();network_mounted=false;}
    for(unsigned i=0;i<3;++i)visible(group_rows[i],!m->submenu);
    if(!m->submenu){
        app_header_context(groups[m->group]);
        visible(range.root,false);visible(name,false);visible(value,false);visible(info,false);visible(hint,false);visible(status,false);
        for(unsigned i=0;i<2;++i)visible(direction_rows[i],false);
        unsigned start=m->group>1?m->group-1:0;if(start>APP_SETTINGS_GROUP_COUNT-3)start=APP_SETTINGS_GROUP_COUNT-3;
        for(unsigned i=0;i<3;++i){unsigned at=start+i;lv_label_set_text(group_rows[i],groups[at]);lv_obj_set_style_bg_color(group_rows[i],lv_color_hex(at==m->group?0x246B4E:0x1C303B),0);}
        return;
    }
    visible(hint,true);visible(status,true);
    app_header_context(names[m->focus]);
    lv_obj_set_y(info,75);
    lv_obj_set_style_text_font(info,&score_font_16,0);
    lv_obj_set_style_text_font(status,&score_font_16,0);
    lv_obj_set_style_text_color(name,lv_color_hex(0xF2F3E9),0);
    const bool is_range=(m->focus>=APP_SETTING_BRIGHTNESS && m->focus<=APP_SETTING_BLANK) || m->focus==APP_SETTING_VOLUME || m->focus==APP_SETTING_DOUBLE;

    visible(range.root,is_range);visible(name,m->focus==APP_SETTING_NETWORK);visible(value,m->focus==APP_SETTING_NETWORK);visible(info,m->focus==APP_SETTING_INFO || m->focus==APP_SETTING_NETWORK);
    for(unsigned i=0;i<2;++i){
        visible(direction_rows[i],m->focus==APP_SETTING_DIRECTION);
        bool selected=p->rotation==(i ? APP_ROTATION_90 : APP_ROTATION_270);
        lv_label_set_text_fmt(direction_rows[i],"%s %s",selected ? ">" : " ",i ? TXT_APP_ROTATE_BOTTOM : TXT_APP_ROTATE_TOP);
        lv_obj_set_style_bg_color(direction_rows[i],lv_color_hex(selected ? 0x246B4E : 0x1C303B),0);
    }
    if(is_range) {
        char current[32];const bool brightness=m->focus==APP_SETTING_BRIGHTNESS,volume=m->focus==APP_SETTING_VOLUME,dbl=m->focus==APP_SETTING_DOUBLE;
        unsigned seconds=m->focus==APP_SETTING_DIM ? p->dim_seconds : p->blank_seconds;
        if(dbl)snprintf(current,sizeof(current),"%u ms",p->double_click_ms);
        else if(brightness || volume)snprintf(current,sizeof(current),"%u%%",volume ? p->volume : p->brightness);
        else if(seconds)snprintf(current,sizeof(current),TXT_APP_SECONDS,seconds);
        else snprintf(current,sizeof(current),"%s",TXT_APP_UNLIMITED);
        app_range_render(&range,app_settings_range_index(m,&state.preferences),dbl ? (APP_DOUBLE_MAX-APP_DOUBLE_MIN)/APP_DOUBLE_STEP : volume ? 100/APP_VOLUME_STEP : brightness ? 8 : 4,current,
            dbl ? "200 ms" : volume ? "0%" : brightness ? "20%" : m->focus==APP_SETTING_DIM ? "15 s" : "60 s",dbl ? "450 ms" : brightness || volume ? "100%" : TXT_APP_UNLIMITED,m->editing);
    } else if(m->focus==APP_SETTING_DIRECTION) {}
    else if(m->focus==APP_SETTING_NETWORK){
        char ssid[100];net_setup_display_ssid(state.network.ssid,ssid,sizeof(ssid));
        // Independent single-line rows: font metrics must not position the IP
        // through blank lines behind the separately rendered network name.
        lv_obj_set_style_text_font(name,&score_font_14,0);lv_label_set_text(name,network_status());
        lv_obj_set_style_text_font(info,&score_font_14,0);
        lv_obj_set_style_text_font(value,&network_font_14,0);lv_label_set_text_fmt(value,"Wi-Fi  %s",*ssid ? ssid : "--");
        lv_obj_set_y(info,112);lv_label_set_text_fmt(info,"IP  %s",*state.network.ip ? state.network.ip : "--");
    }
    else {
        char bat[80];
        if(state.battery<0 || state.millivolts<0)snprintf(bat,sizeof(bat),"%s",TXT_APP_INFO_UNKNOWN);
        else snprintf(bat,sizeof(bat),TXT_APP_INFO_BATTERY,state.battery,state.millivolts);
        lv_obj_set_y(info,42);lv_obj_set_style_text_font(info,&score_font_14,0);
        lv_label_set_text_fmt(info,TXT_APP_VERSION "\n%s\n" TXT_APP_DEVICE "\n%s\nIP %s",state.version ? state.version : "--",bat,network_status(),*state.network.ip ? state.network.ip : "--");
    }
    lv_label_set_text(hint,m->focus==APP_SETTING_DOUBLE ? TXT_SETTINGS_DOUBLE_HINT : m->focus==APP_SETTING_INFO ? "" : m->editing ? TXT_APP_EDIT_HINT : TXT_APP_SETTING_HINT);
    lv_obj_set_y(hint,v->content_height-38);lv_obj_set_y(status,v->content_height-18);
    lv_label_set_text(status,state.dirty || (state.storage && strcmp(state.storage,TXT_APP_PREF_SAVED)!=0) ? app_settings_storage_status() : "");
}
static const app_module_ops_t ops={enter,leave,bindings,input,tick,create,render,init,key};
const app_module_t app_module_settings={4,TXT_APP_SETTINGS,TXT_APP_SETTINGS_DESC,APP_ENTRY_SETTINGS,&ops,NULL,APP_ART_SETTINGS, NULL};
