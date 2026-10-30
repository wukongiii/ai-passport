#include "app_settings_model.h"
void app_settings_model_enter(app_settings_model_t *m, const app_preferences_t *p) { *m=(app_settings_model_t){.draft=*p}; }
static uint16_t step(uint16_t value, const uint16_t *values, unsigned count, int direction) {
    unsigned i=0; while(i+1<count && values[i]!=value) ++i;
    int next=(int)i+direction;
    if(next<0) next=0;
    if(next>=(int)count) next=(int)count-1;
    return values[next];
}
bool app_settings_model_input(app_settings_model_t *m, app_preferences_t *p, unsigned slot, app_gesture_t gesture) {
    if(slot>=3) return false;
    if(gesture==APP_GESTURE_LONG && slot==1) {
        if(m->editing){m->editing=false;m->draft=*p;}else m->submenu=false;
        return false;
    }
    if(gesture!=APP_GESTURE_SHORT) return false;
    static const unsigned first[]={APP_SETTING_DIRECTION,APP_SETTING_VOLUME,APP_SETTING_DOUBLE,APP_SETTING_INFO,APP_SETTING_NETWORK};
    if(!m->submenu){
        if(slot==1){m->submenu=true;m->focus=first[m->group];}
        else m->group=(m->group+APP_SETTINGS_GROUP_COUNT+(slot==0?-1:1))%APP_SETTINGS_GROUP_COUNT;
        return false;
    }
    if(slot==1) {
        if(m->focus==APP_SETTING_INFO || m->focus==APP_SETTING_NETWORK) return false;
        if(!m->editing) {m->editing=true; m->draft=*p; return false;}
        *p=m->draft; m->editing=false; return true;
    }
    const int delta=slot==0 ? -1 : 1;
    if(!m->editing) {if(m->group==APP_SETTINGS_DISPLAY)m->focus=(m->focus+4+delta)%4;return false;}
    static const uint16_t dims[]={15,30,60,120,0}, blanks[]={60,120,300,600,0};
    switch(m->focus) {
    case APP_SETTING_DIRECTION: m->draft.rotation=m->draft.rotation==APP_ROTATION_270 ? APP_ROTATION_90 : APP_ROTATION_270; break;
    case APP_SETTING_BRIGHTNESS:
        if((delta<0 && m->draft.brightness>20)||(delta>0 && m->draft.brightness<100)) m->draft.brightness+=10*delta;
        break;
    case APP_SETTING_DOUBLE:
        if((delta<0 && m->draft.double_click_ms>APP_DOUBLE_MIN)||(delta>0 && m->draft.double_click_ms<APP_DOUBLE_MAX))m->draft.double_click_ms+=APP_DOUBLE_STEP*delta;
        break;
    case APP_SETTING_VOLUME:
        if((delta<0 && m->draft.volume>0)||(delta>0 && m->draft.volume<100))m->draft.volume+=APP_VOLUME_STEP*delta;
        break;
    case APP_SETTING_DIM: m->draft.dim_seconds=step(m->draft.dim_seconds,dims,5,delta); break;
    case APP_SETTING_BLANK: m->draft.blank_seconds=step(m->draft.blank_seconds,blanks,5,delta); break;
    default: break;
    }
    return false;
}

unsigned app_settings_range_index(const app_settings_model_t *m, const app_preferences_t *p) {
    const app_preferences_t *v=m->editing ? &m->draft : p;
    if(m->focus==APP_SETTING_DOUBLE)return (v->double_click_ms-APP_DOUBLE_MIN)/APP_DOUBLE_STEP;
    if(m->focus==APP_SETTING_VOLUME)return v->volume/APP_VOLUME_STEP;
    if(m->focus==APP_SETTING_BRIGHTNESS) return (v->brightness-20)/10;
    static const uint16_t dims[]={15,30,60,120,0}, blanks[]={60,120,300,600,0};
    const uint16_t *values=m->focus==APP_SETTING_DIM ? dims : blanks;
    const uint16_t value=m->focus==APP_SETTING_DIM ? v->dim_seconds : v->blank_seconds;
    for(unsigned i=0;i<5;++i) if(values[i]==value) return i;
    return 0;
}
