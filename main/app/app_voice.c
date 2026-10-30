#include "app_voice.h"
#include "app_strings.h"
#include "../ai/ai_config.h"
#include "../network/app_network.h"
#include "../speech/app_speech.h"
#include "../ui/app_header.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>
LV_FONT_DECLARE(score_font_16);
LV_FONT_DECLARE(score_font_14);
LV_FONT_DECLARE(network_font_14);
static const char *const phrases[]={TXT_VOICE_PHRASE_SCORE,TXT_VOICE_PHRASE_WIN,TXT_VOICE_PHRASE_NUMBERS};
typedef enum { MODELS,PROVIDER,PHONE_JOIN,PHONE_OPEN,LISTEN } page_t;
static struct {
    bool active,rejected,portal_requested,portal_seen;
    unsigned focus,selected,provider;
    bool phone_from_list;
    page_t page;
    ai_config_snapshot_t config;
    ai_check_snapshot_t check;
    app_network_snapshot_t network;
    app_speech_snapshot_t speech;
} state;
static lv_obj_t *content,*rows[3],*detail,*note,*qr,*qr_text,*model_label,*url_label,*caps;
static int mounted=-1;
static char qr_value[192];
static bool qr_failed;
static void snapshots(void){ai_config_snapshot(&state.config);app_network_snapshot(&state.network);app_speech_snapshot(&state.speech);ai_check_snapshot(state.provider,&state.check);}
static bool enter(void *c,app_rotation_t r){
    (void)c;(void)r;state.active=true;state.rejected=false;state.page=MODELS;
    snapshots();state.focus=0;state.provider=0;state.phone_from_list=false;return true;
}
static bool leave(void *c){(void)c;state.active=false;app_speech_stop();return true;}
static void bindings(void *c,app_bindings_t *b){
    (void)c;app_actions_clear(b);
    if(state.page==MODELS){
        app_actions_register(b,0,APP_GESTURE_SHORT,500,TXT_APP_PREVIOUS,true,APP_ACTION_NAVIGATE);
        app_actions_register(b,1,APP_GESTURE_SHORT,501,TXT_APP_ENTER,true,APP_ACTION_CONFIRM);
        app_actions_register(b,2,APP_GESTURE_SHORT,502,TXT_APP_NEXT,true,APP_ACTION_NAVIGATE);
        return;
    }
    app_actions_register(b,1,APP_GESTURE_LONG,503,TXT_APP_BACK,true,APP_ACTION_BACK);
    if(state.page==LISTEN){
        app_actions_register(b,0,APP_GESTURE_SHORT,504,TXT_VOICE_CHANGE,true,APP_ACTION_NAVIGATE);
        bool playing=state.speech.pending||state.speech.state==SPEECH_PLAYING||state.speech.state==SPEECH_STOPPING;
        app_actions_register(b,1,APP_GESTURE_SHORT,505,playing?TXT_VOICE_STOP:TXT_VOICE_PLAY,true,playing?APP_ACTION_CANCEL:APP_ACTION_CONFIRM);
        app_actions_register(b,2,APP_GESTURE_SHORT,506,TXT_VOICE_STOP,true,APP_ACTION_CANCEL);
    }else if(state.page==PROVIDER){
        app_actions_register(b,0,APP_GESTURE_SHORT,508,TXT_AI_PHONE,true,APP_ACTION_NAVIGATE);
        app_actions_register(b,1,APP_GESTURE_SHORT,509,TXT_AI_CHECK,state.config.configured[state.provider] && !state.config.busy && !(state.check.state>=AI_CHECK_QUEUED&&state.check.state<=AI_CHECK_AUDIO),APP_ACTION_CONFIRM);
        app_actions_register(b,2,APP_GESTURE_SHORT,513,TXT_AI_APPLY,state.config.configured[state.provider] && state.config.active!=state.provider && !state.config.busy,APP_ACTION_CONFIRM);

    }else{

        app_actions_register(b,1,APP_GESTURE_SHORT,511,state.page==PHONE_JOIN ? TXT_AI_NEXT : TXT_AI_HOTSPOT,state.network.portal,APP_ACTION_CONFIRM);
        app_actions_register(b,2,APP_GESTURE_SHORT,512,TXT_AI_PHONE,!state.network.portal,APP_ACTION_NAVIGATE);
    }
}
static void phone(void){
    if(state.page==MODELS || state.page==PROVIDER)state.phone_from_list=state.page==MODELS;
    state.page=PHONE_JOIN;state.portal_seen=state.network.portal;
    state.portal_requested=app_network_portal(true);state.rejected=!state.portal_requested;
}
static app_module_effect_t input(void *c,app_rotation_t r,unsigned slot,app_gesture_t g){
    (void)c;
    if(g==APP_GESTURE_LONG && slot==1 && state.page!=MODELS){
        if(state.page==LISTEN){app_speech_stop();state.page=MODELS;}
        else state.page=state.page==PROVIDER || state.phone_from_list ? MODELS : PROVIDER;
        state.rejected=false;
    }else if(g==APP_GESTURE_SHORT){
        state.rejected=false;
        switch(state.page){
        case MODELS:{
            unsigned entries[AI_PROVIDER_COUNT+2],count=0;entries[count++]=AI_PROVIDER_NONE;
            for(unsigned i=0;i<AI_PROVIDER_COUNT;++i)if(state.config.configured[i])entries[count++]=i;
            entries[count++]=AI_PROVIDER_COUNT;
            if(state.focus>=count)state.focus=0;
            if(slot==0)state.focus=(state.focus+count-1)%count;
            else if(slot==2)state.focus=(state.focus+1)%count;
            else if(entries[state.focus]==AI_PROVIDER_NONE)phone();
            else if(entries[state.focus]==AI_PROVIDER_COUNT)state.page=LISTEN;
            else{state.provider=entries[state.focus];state.page=PROVIDER;}
            break;
        }
        case PROVIDER:
            if(slot==0)phone();
            else if(slot==1)state.rejected=!ai_check_request(state.provider);
            else {ai_config_request_t request={.action=AI_CONFIG_SELECT,.provider=state.provider};state.rejected=!ai_config_submit(&request,NULL);}
            break;
        case PHONE_JOIN:case PHONE_OPEN:
            if(slot==1 && state.network.portal)state.page=state.page==PHONE_JOIN ? PHONE_OPEN : PHONE_JOIN;
            else if(slot==2)phone();
            break;
        case LISTEN:
            if(slot==0)state.selected=(state.selected+1)%3;
            else if(slot==1){
                app_speech_snapshot_t speech;app_speech_snapshot(&speech);
                if(speech.pending||speech.state==SPEECH_PLAYING||speech.state==SPEECH_STOPPING)app_speech_stop();
                else state.rejected=!app_speech_say(phrases[state.selected]);
            }
            else app_speech_stop();
            break;
        }
    }
    snapshots();return (app_module_effect_t){false,r};
}
static bool tick(void *c,int64_t now){
    (void)c;(void)now;unsigned config=state.config.revision,network=state.network.revision,speech=state.speech.revision,check=state.check.revision;
    snapshots();
    if(state.network.portal)state.portal_seen=true;
    return state.active && (config!=state.config.revision || network!=state.network.revision || speech!=state.speech.revision || check!=state.check.revision);
}
static lv_obj_t *label(lv_obj_t *p,int x,int y,int w,uint32_t color){
    lv_obj_t *o=lv_label_create(p);lv_obj_set_pos(o,x,y);lv_obj_set_width(o,w);
    lv_obj_set_style_text_font(o,&score_font_14,0);lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_set_style_text_color(o,lv_color_hex(color),0);lv_label_set_long_mode(o,LV_LABEL_LONG_WRAP);lv_label_set_text(o,"");return o;
}
static bool create(void *c,void *parent){(void)c;content=parent;mounted=-1;return true;}
static void mount(void){
    lv_obj_clean(content);mounted=state.page;qr_value[0]=0;qr_failed=false;
    if(state.page==MODELS){
        detail=label(content,12,29,296,0xA6B5B7);lv_obj_set_height(detail,28);lv_label_set_long_mode(detail,LV_LABEL_LONG_SCROLL_CIRCULAR);
        for(unsigned i=0;i<3;++i){rows[i]=label(content,16,58+(int)i*34,288,0xF2F3E9);
            lv_obj_set_style_text_font(rows[i],&network_font_14,0);lv_label_set_long_mode(rows[i],LV_LABEL_LONG_SCROLL_CIRCULAR);
            lv_obj_set_height(rows[i],32);lv_obj_set_style_pad_top(rows[i],2,0);
            lv_obj_set_style_radius(rows[i],6,0);lv_obj_set_style_bg_opa(rows[i],LV_OPA_COVER,0);}
    }else if(state.page==PHONE_JOIN || state.page==PHONE_OPEN){
        qr=lv_qrcode_create(content);lv_obj_set_pos(qr,10,34);lv_qrcode_set_size(qr,132);
        lv_qrcode_set_dark_color(qr,lv_color_black());lv_qrcode_set_light_color(qr,lv_color_white());lv_qrcode_set_quiet_zone(qr,true);
        qr_text=label(content,149,43,160,0xF2F3E9);detail=label(content,149,91,160,0x8DE0B8);
    }else if(state.page==PROVIDER){
        detail=label(content,12,29,296,0x8DE0B8);lv_obj_set_height(detail,28);lv_obj_set_style_text_font(detail,&network_font_14,0);lv_label_set_long_mode(detail,LV_LABEL_LONG_SCROLL_CIRCULAR);
        model_label=label(content,12,60,296,0xF2F3E9);lv_obj_set_height(model_label,22);lv_label_set_long_mode(model_label,LV_LABEL_LONG_SCROLL_CIRCULAR);
        url_label=label(content,12,87,296,0xA6B5B7);lv_obj_set_height(url_label,22);lv_label_set_long_mode(url_label,LV_LABEL_LONG_SCROLL_CIRCULAR);
        caps=label(content,12,118,296,0x8DE0B8);lv_obj_set_height(caps,50);
    }else{
        detail=label(content,18,51,284,0x8DE0B8);
        if(state.page==LISTEN){qr_text=label(content,18,88,284,0xF2F3E9);lv_obj_set_style_text_font(qr_text,&score_font_16,0);}
        else qr_text=label(content,18,89,284,0xA6B5B7);
    }
    note=label(content,12,170,296,0xA6B5B7);
}
static const char *config_status(unsigned i){
    return !state.config.configured[i] ? TXT_AI_NONE : state.config.active==i ? TXT_AI_ACTIVE : TXT_AI_SAVED;
}
static void code(const char *value){
    if(strcmp(qr_value,value)){
        snprintf(qr_value,sizeof(qr_value),"%s",value);qr_failed=lv_qrcode_update(qr,value,strlen(value))!=LV_RESULT_OK;
    }
    if(qr_failed)lv_label_set_text(note,TXT_AI_LINK_FAILED);
}
static void render(void *c,const app_module_view_t *v){
    (void)c;if(mounted!=(int)state.page)mount();
    lv_obj_set_y(note,v->content_height-20);lv_label_set_text(note,"");
    if(state.page==MODELS){
        app_header_context(TXT_AI_MODELS);
        if(state.config.active<AI_PROVIDER_COUNT)lv_label_set_text_fmt(detail,TXT_AI_CURRENT,state.config.entries[state.config.active].name);
        else lv_label_set_text(detail,TXT_AI_EMPTY);
        lv_obj_set_style_text_font(detail,&network_font_14,0);
        unsigned entries[AI_PROVIDER_COUNT+2],count=0;entries[count++]=AI_PROVIDER_NONE;
        for(unsigned i=0;i<AI_PROVIDER_COUNT;++i)if(state.config.configured[i])entries[count++]=i;
        entries[count++]=AI_PROVIDER_COUNT;if(state.focus>=count)state.focus=0;
        unsigned start=state.focus>=3?state.focus-2:0;
        for(unsigned row=0;row<3;++row){unsigned at=start+row;
            if(at>=count){lv_obj_add_flag(rows[row],LV_OBJ_FLAG_HIDDEN);continue;}
            lv_obj_remove_flag(rows[row],LV_OBJ_FLAG_HIDDEN);unsigned i=entries[at];
            if(i==AI_PROVIDER_NONE)lv_label_set_text(rows[row],TXT_AI_HOTSPOT_CONFIG);
            else if(i==AI_PROVIDER_COUNT)lv_label_set_text(rows[row],TXT_VOICE_TITLE);
            else lv_label_set_text_fmt(rows[row],"%s | %s",state.config.entries[i].name,config_status(i));
            lv_obj_set_style_bg_color(rows[row],lv_color_hex(state.focus==at ? 0x246B4E : 0x1C303B),0);
        }
        return;
    }
    if(state.page==LISTEN){
        app_header_context(TXT_VOICE_TITLE);lv_label_set_text(qr_text,phrases[state.selected]);
        static const char *const messages[]={TXT_VOICE_READY,TXT_VOICE_WAIT,TXT_VOICE_PLAYING,TXT_VOICE_DONE,TXT_VOICE_FAILED,TXT_VOICE_STOPPING};
        const app_speech_snapshot_t *s=&state.speech;
        if(state.rejected || s->state==SPEECH_FAILED)lv_label_set_text_fmt(detail,"%s (%d)",TXT_VOICE_FAILED,s->error);
        else lv_label_set_text(detail,messages[s->state]);
        lv_label_set_text(note,s->pending && s->state==SPEECH_PLAYING ? TXT_VOICE_PENDING : TXT_VOICE_NOTE);return;
    }
    app_header_context(TXT_AI_MODELS);
    if(state.page==PROVIDER){
        lv_label_set_text_fmt(detail,"%s | %s",state.config.entries[state.provider].name,config_status(state.provider));
        const char *model=state.config.entries[state.provider].model;
        lv_label_set_text_fmt(model_label,"%s: %s",TXT_AI_MODEL,*model?model:"-");
        lv_label_set_text_fmt(url_label,"%s: %s",TXT_AI_URL,*state.config.entries[state.provider].url?state.config.entries[state.provider].url:"-");
        const ai_check_snapshot_t *check=&state.check;
        if(check->provider==state.provider){
            lv_label_set_text_fmt(caps,"%s%s%d\n" TXT_AI_CHECK_RESULT,ai_check_label(check->state),check->error?" · ":"",check->error,
                check->text?TXT_AI_CHECK_OK:TXT_AI_CHECK_NO,check->tools?TXT_AI_CHECK_OK:TXT_AI_CHECK_NO,check->audio?TXT_AI_CHECK_OK:TXT_AI_CHECK_NO);
            if(!check->error)lv_label_set_text_fmt(caps,"%s\n" TXT_AI_CHECK_RESULT,ai_check_label(check->state),check->text?TXT_AI_CHECK_OK:TXT_AI_CHECK_NO,check->tools?TXT_AI_CHECK_OK:TXT_AI_CHECK_NO,check->audio?TXT_AI_CHECK_OK:TXT_AI_CHECK_NO);
        }else lv_label_set_text(caps,ai_check_label(AI_CHECK_UNTESTED));
        lv_label_set_text(note,check->provider==state.provider && check->error?ai_check_error_label(check->error):TXT_AI_CHECK_COST);
    }else{
        bool ready=state.network.portal;
        lv_label_set_text(qr_text,state.page==PHONE_JOIN ? TXT_AI_JOIN : TXT_AI_OPEN);
        if(ready){
            char data[192];
            if(state.page==PHONE_JOIN)snprintf(data,sizeof(data),"WIFI:T:nopass;S:%s;;",state.network.ap_name);
            else snprintf(data,sizeof(data),"http://192.168.4.1/#ai");
            lv_label_set_text_fmt(detail,"%s\n%u s",state.page==PHONE_JOIN ? state.network.ap_name : "192.168.4.1",state.network.seconds_left);
            lv_label_set_text(note,state.page==PHONE_JOIN ? TXT_APP_NETWORK_OPEN : TXT_AI_PHONE_NOTE);code(data);
        }else{
            lv_label_set_text(detail,state.portal_requested && !state.portal_seen && !state.network.error ? TXT_AI_STARTING : TXT_AI_ERROR);
            lv_obj_add_flag(qr,LV_OBJ_FLAG_HIDDEN);qr_value[0]=0;
        }
        if(ready)lv_obj_remove_flag(qr,LV_OBJ_FLAG_HIDDEN);
    }
    if(state.rejected || state.config.error)lv_label_set_text(note,TXT_AI_ERROR);
    else if(state.config.busy)lv_label_set_text(note,TXT_AI_WAIT);
}
static const app_module_ops_t ops={enter,leave,bindings,input,tick,create,render,NULL,NULL};
const app_module_t app_module_voice={5,TXT_APP_VOICE,TXT_APP_VOICE_DESC,APP_ENTRY_SERVICE,&ops,NULL,APP_ART_VOICE, NULL};
