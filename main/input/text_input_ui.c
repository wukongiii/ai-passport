#include "text_input_ui.h"
#include "text_input_strings.h"
#include <ctype.h>
#include <stdio.h>

static lv_obj_t *label(lv_obj_t *parent,const lv_font_t *font,uint32_t color){
    lv_obj_t *o=lv_label_create(parent);
    lv_obj_set_style_text_font(o,font,0);
    lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_set_style_text_color(o,lv_color_hex(color),0);
    lv_label_set_long_mode(o,LV_LABEL_LONG_CLIP);lv_label_set_text(o,"");return o;
}
static void show(lv_obj_t *o,bool visible){
    if(visible)lv_obj_remove_flag(o,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN);
}
static void cell(lv_obj_t *o,int x,int y,int width,int height,bool focused,bool secondary){
    lv_obj_set_pos(o,x,y);lv_obj_set_size(o,width,height);
    lv_obj_set_style_pad_top(o,(height-lv_obj_get_style_text_font(o,0)->line_height)/2,0);
    lv_obj_set_style_radius(o,4,0);lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);
    lv_obj_set_style_bg_color(o,lv_color_hex(focused ? (secondary ? 0x304B5C : 0x246B4E) : 0x1C303B),0);
    lv_obj_set_style_text_color(o,lv_color_hex(focused ? 0xFFFFFF : 0xB5C9C9),0);
}
static void draw_keyboard(lv_event_t *e){
    text_input_ui_t *u=lv_event_get_user_data(e);
    const text_input_t *t=&u->snapshot;
    lv_layer_t *layer=lv_event_get_layer(e);lv_area_t area;lv_obj_get_coords(u->keyboard,&area);
    const lv_font_t *font=lv_obj_get_style_text_font(u->keyboard,0);
    const unsigned rows=text_input_row_count(t),focus=text_input_choice_focus(t);
    if(!rows)return;
    const int gap=3,row_gap=2,width=lv_area_get_width(&area);
    const int height=(lv_area_get_height(&area)-(int)(rows-1)*row_gap)/(int)rows;
    unsigned index=0;
    for(unsigned row=0;row<rows;++row){
        unsigned n=text_input_row_size(t,row);int fit=(width-(int)(n-1)*gap)/(int)n;
        int w=t->page==TEXT_INPUT_CHARACTERS && row+1<rows && fit>25 ? 25 : fit;
        int x=area.x1+(width-((int)n*w+(int)(n-1)*gap))/2;
        for(unsigned col=0;col<n;++col,++index){
            lv_area_t key={x+(int)col*(w+gap),area.y1+(int)row*(height+row_gap),
                x+(int)col*(w+gap)+w-1,area.y1+(int)row*(height+row_gap)+height-1};
            if(key.x2<layer->_clip_area.x1 || key.x1>layer->_clip_area.x2 ||
               key.y2<layer->_clip_area.y1 || key.y1>layer->_clip_area.y2)continue;
            lv_draw_rect_dsc_t rect;lv_draw_rect_dsc_init(&rect);
            rect.radius=4;rect.bg_color=lv_color_hex(index==focus ? 0x246B4E : 0x1C303B);
            rect.bg_opa=LV_OPA_COVER;lv_draw_rect(layer,&rect,&key);
            char text[32];text_input_choice(t,index,text,sizeof(text));
            lv_draw_label_dsc_t d;lv_draw_label_dsc_init(&d);
            d.font=font;d.text=text;d.text_local=1;d.align=LV_TEXT_ALIGN_CENTER;
            d.color=lv_color_hex(index==focus ? 0xFFFFFF : 0xB5C9C9);
            key.y1+=(height-font->line_height)/2;lv_draw_label(layer,&d,&key);
        }
    }
}
void text_input_ui_create(text_input_ui_t *u,lv_obj_t *parent,const lv_font_t *font){
    u->root=lv_obj_create(parent);lv_obj_remove_style_all(u->root);
    lv_obj_remove_flag(u->root,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_size(u->root,320,144);
    u->text=label(u->root,font,0xF2F3E9);
    lv_obj_set_style_text_align(u->text,LV_TEXT_ALIGN_LEFT,0);lv_obj_set_style_pad_left(u->text,4,0);
    u->context=label(u->root,font,0x91ABAE);
    u->snapshot=(text_input_t){0};u->keyboard=lv_obj_create(u->root);lv_obj_remove_style_all(u->keyboard);
    lv_obj_remove_flag(u->keyboard,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_font(u->keyboard,font,0);
    lv_obj_add_event_cb(u->keyboard,draw_keyboard,LV_EVENT_DRAW_MAIN,u);
    u->message=label(u->root,font,0xEDC56B);
}
void text_input_ui_render(text_input_ui_t *u,const text_input_t *t,uint64_t now,int height){
    // Physical key count does not limit the number of visible candidates.
    const int top=24,inset=12,width=296,available=height-top;
    char buffer[48];
    const lv_font_t *font=lv_obj_get_style_text_font(u->text,0);
    unsigned capacity=sizeof(buffer);
    lv_point_t text_size;
    do {
        text_input_window(t,now,buffer,capacity--);
        lv_text_get_size(&text_size,buffer,font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_EXPAND);
    } while(capacity>=4 && text_size.x>width-8);
    lv_label_set_text(u->text,buffer);
    lv_obj_set_pos(u->root,0,top);lv_obj_set_height(u->root,available);
    cell(u->text,inset,0,width,24,false,false);
    lv_obj_set_pos(u->context,inset,26);lv_obj_set_size(u->context,width,18);
    const char *context=t->page==TEXT_INPUT_CHARACTERS ?
        (t->mode==TEXT_INPUT_ABC ? TXT_INPUT_QWERTY : t->mode==TEXT_INPUT_DIGITS ? TXT_INPUT_NUMBERS : TXT_INPUT_SYMBOLS) :
        t->page==TEXT_INPUT_CURSOR ? TXT_INPUT_CURSOR_HELP : TXT_INPUT_EDIT;
    lv_label_set_text(u->context,t->full ? TXT_INPUT_FULL : context);
    const unsigned count=text_input_choice_count(t);
    // Drawing needs selection metadata only; never retain a copy of entered text.
    u->snapshot=(text_input_t){.config={.numeric_only=t->config.numeric_only},
        .mode=t->mode,.page=t->page,.uppercase=t->uppercase,.revealed=t->revealed,
        .edit_focus=t->edit_focus,.focus={t->focus[0],t->focus[1],t->focus[2]}};
    lv_obj_set_pos(u->keyboard,inset,44);
    lv_obj_set_size(u->keyboard,width,available-48);show(u->keyboard,count>0);
    lv_obj_invalidate(u->keyboard);
    show(u->message,!count);lv_obj_set_pos(u->message,inset,54);lv_obj_set_size(u->message,width,available-54);
    lv_label_set_text(u->message,t->page==TEXT_INPUT_CLEAR ? TXT_INPUT_CLEAR_ASK :
        t->page==TEXT_INPUT_CANCEL ? TXT_INPUT_EXIT_ASK : "");
}
