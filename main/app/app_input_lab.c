#include "app_input_lab.h"
#include "../input/text_input.h"
#include "../input/text_input_ui.h"
#include "../input/text_input_strings.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>
LV_FONT_DECLARE(score_font_14);
static const char *const tasks[]={TXT_INPUT_SHORT,TXT_INPUT_DIGITS,TXT_INPUT_MIXED,TXT_INPUT_FREE_TASK};
static const char *const targets[]={"there","1234567890","Ther_2026!",""};
static struct {text_input_t editor;unsigned task,page;uint64_t now,last_tick;bool active;} state;
static lv_obj_t *heading,*battery,*summary,*detail,*note,*target;
static text_input_ui_t ui;
static void begin(void){
    text_input_config_t config={.maximum_length=63,.numeric_only=state.task==1};
    text_input_begin(&state.editor,&config,state.now);state.page=1;
}
static bool enter(void *c,app_rotation_t r){(void)c;(void)r;state.page=0;state.active=true;return true;}
static bool leave(void *c){(void)c;state.active=false;text_input_dispose(&state.editor);return true;}
enum {PREV=400,OPEN,NEXT,AGAIN,TASKS};
static void bindings(void *c,app_bindings_t *b){
    (void)c;app_actions_clear(b);
    if(state.page==1){text_input_bindings(&state.editor,b);return;}
    if(!state.page){
        app_actions_register(b,0,APP_GESTURE_SHORT,PREV,TXT_INPUT_PREV,true,APP_ACTION_NAVIGATE);
        app_actions_register(b,1,APP_GESTURE_SHORT,OPEN,TXT_INPUT_START,true,APP_ACTION_CONFIRM);
        app_actions_register(b,2,APP_GESTURE_SHORT,NEXT,TXT_INPUT_NEXT,true,APP_ACTION_NAVIGATE);
    }else{
        app_actions_register(b,0,APP_GESTURE_SHORT,TASKS,TXT_INPUT_TASKS,true,APP_ACTION_NAVIGATE);
        app_actions_register(b,1,APP_GESTURE_SHORT,AGAIN,TXT_INPUT_AGAIN,true,APP_ACTION_CONFIRM);
        app_actions_register(b,2,APP_GESTURE_SHORT,TASKS,TXT_INPUT_TASKS,true,APP_ACTION_NAVIGATE);
    }
}
static app_module_effect_t input(void *c,app_rotation_t r,unsigned slot,app_gesture_t gesture){
    (void)c;
    if(state.page==1){
        text_input_gesture(&state.editor,slot,gesture,state.now);
        if(state.editor.page==TEXT_INPUT_DONE || state.editor.page==TEXT_INPUT_CANCELLED)state.page=2;
    }else if(gesture==APP_GESTURE_SHORT){
        if(slot==1)begin();
        else if(state.page==2){state.page=0;text_input_dispose(&state.editor);}
        else state.task=(state.task+4+(slot==0 ? -1 : 1))%4;
    }
    return (app_module_effect_t){false,r};
}
static void key(void *c,unsigned slot,bool pressed,int64_t now){
    (void)c;(void)slot;(void)pressed;state.now=(uint64_t)now/1000;
}
static bool tick(void *c,int64_t now){
    (void)c;state.now=(uint64_t)now/1000;
    if(!state.active || state.page!=1)return false;
    // Password reveal expires independently of statistics or physical key events.
    const bool mask=state.editor.config.secret && !state.editor.revealed &&
        state.last_tick<state.editor.reveal_until_ms && state.now>=state.editor.reveal_until_ms;
    state.last_tick=state.now;
    return mask;
}
static lv_obj_t *label(lv_obj_t *parent,int y,uint32_t color){
    lv_obj_t *o=lv_label_create(parent);lv_obj_set_pos(o,18,y);lv_obj_set_width(o,284);
    lv_obj_set_style_text_font(o,&score_font_14,0);lv_obj_set_style_text_color(o,lv_color_hex(color),0);
    lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);lv_label_set_long_mode(o,LV_LABEL_LONG_CLIP);lv_label_set_text(o,"");return o;
}
static bool create(void *c,void *p){
    (void)c;
    extern const uint32_t score_font_codepoints[];
    extern const unsigned score_font_codepoint_count;
    for(unsigned i=0;i<score_font_codepoint_count;++i){
        lv_font_glyph_dsc_t glyph={0};
        if(!lv_font_get_glyph_dsc(&score_font_14,&glyph,score_font_codepoints[i],0) || glyph.is_placeholder)return false;
    }
    heading=label(p,2,0xEDC56B);lv_obj_set_width(heading,210);
    battery=label(p,2,0xA6B5B7);lv_obj_set_x(battery,244);lv_obj_set_width(battery,54);
    summary=label(p,47,0xF2F3E9);detail=label(p,82,0x74C5B5);note=label(p,117,0xA6B5B7);target=label(p,145,0xEDC56B);
    text_input_ui_create(&ui,p,&score_font_14);return true;
}
static void show(lv_obj_t *o,bool yes){if(yes)lv_obj_remove_flag(o,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN);}
static void render(void *c,const app_module_view_t *v){
    (void)c;lv_label_set_text(heading,state.page==1 && *targets[state.task] ? targets[state.task] : TXT_INPUT_TITLE);
    if(v->battery<0)lv_label_set_text(battery,"--%");else lv_label_set_text_fmt(battery,"%d%%",v->battery);
    show(ui.root,state.page==1);show(summary,state.page!=1);show(detail,state.page!=1);show(note,state.page!=1);show(target,state.page!=1);
    if(state.page==1){text_input_ui_render(&ui,&state.editor,state.now,v->content_height);return;}
    if(!state.page){
        lv_label_set_text(summary,tasks[state.task]);lv_label_set_text(detail,TXT_INPUT_READY);
        if(*targets[state.task])lv_label_set_text_fmt(note,TXT_INPUT_TARGET,targets[state.task]);else lv_label_set_text(note,TXT_INPUT_FREE);
        lv_label_set_text(target,"");
    }else{
        bool cancelled=state.editor.page==TEXT_INPUT_CANCELLED;
        lv_label_set_text(summary,cancelled ? TXT_INPUT_CANCELLED : !*targets[state.task] ? TXT_INPUT_FINISHED :
            !strcmp(state.editor.text,targets[state.task]) ? TXT_INPUT_MATCH : TXT_INPUT_MISMATCH);
        char text[26];text_input_window(&state.editor,state.now,text,sizeof(text));lv_label_set_text(detail,text);
        if(*targets[state.task])lv_label_set_text_fmt(note,TXT_INPUT_TARGET,targets[state.task]);
        else lv_label_set_text(note,"");
        lv_label_set_text(target,"");
    }
}
static const app_module_ops_t ops={enter,leave,bindings,input,tick,create,render,NULL,key};
const app_module_t app_module_input_lab={6,TXT_INPUT_TITLE,TXT_INPUT_DESC,APP_ENTRY_SERVICE,&ops,NULL,APP_ART_SETTINGS};
