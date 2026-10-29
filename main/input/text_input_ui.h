#pragma once
#include "text_input.h"
#include "lvgl.h"
typedef struct {
    lv_obj_t *root,*text,*context,*keyboard,*message;
    text_input_t snapshot;
} text_input_ui_t;
void text_input_ui_create(text_input_ui_t *,lv_obj_t *parent,const lv_font_t *font);
void text_input_ui_render(text_input_ui_t *,const text_input_t *,uint64_t now_ms,int content_height);
