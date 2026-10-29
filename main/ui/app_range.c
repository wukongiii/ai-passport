#include "app_range.h"
static lv_obj_t *text(lv_obj_t *parent, int x, int y, int width, const lv_font_t *font) {
    lv_obj_t *o=lv_label_create(parent);
    lv_obj_set_pos(o,x,y); lv_obj_set_width(o,width);
    lv_obj_set_style_text_font(o,font,0); lv_obj_set_style_text_color(o,lv_color_hex(0xF2F3E9),0);
    lv_label_set_long_mode(o,LV_LABEL_LONG_CLIP); lv_label_set_text(o,""); return o;
}
static lv_obj_t *shape(lv_obj_t *parent, uint32_t color) {
    lv_obj_t *o=lv_obj_create(parent); lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(o,lv_color_hex(color),0); lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);
    lv_obj_set_style_radius(o,LV_RADIUS_CIRCLE,0); return o;
}
void app_range_create(app_range_t *r, lv_obj_t *parent, int x, int y, int width, const lv_font_t *font) {
    r->width=width;
    r->root=lv_obj_create(parent); lv_obj_remove_style_all(r->root);
    lv_obj_set_pos(r->root,x,y); lv_obj_set_size(r->root,width,94);
    lv_obj_remove_flag(r->root,LV_OBJ_FLAG_SCROLLABLE);
    r->value=text(r->root,0,0,width,font); lv_obj_set_style_text_align(r->value,LV_TEXT_ALIGN_CENTER,0);
    r->rail=shape(r->root,0x38505C); lv_obj_set_pos(r->rail,8,40); lv_obj_set_size(r->rail,width-16,6);
    r->fill=shape(r->root,0x74C5B5); lv_obj_set_pos(r->fill,8,40);
    r->knob=shape(r->root,0xF2F3E9); lv_obj_set_size(r->knob,16,22);
    lv_obj_set_style_radius(r->knob,5,0); lv_obj_set_style_border_width(r->knob,2,0);
    r->minimum=text(r->root,0,64,width/2,font);
    r->maximum=text(r->root,width/2,64,width/2,font);
    lv_obj_set_style_text_color(r->minimum,lv_color_hex(0xA6B5B7),0);
    lv_obj_set_style_text_color(r->maximum,lv_color_hex(0xA6B5B7),0);
    lv_obj_set_style_text_align(r->maximum,LV_TEXT_ALIGN_RIGHT,0);
}
void app_range_render(app_range_t *r, unsigned index, unsigned maximum,
                      const char *value, const char *minimum, const char *max_label, bool editing) {
    if(index>maximum) index=maximum;
    const int length=r->width-16;
    const int at=maximum ? (int)((uint64_t)length*index/maximum) : 0;
    lv_obj_set_size(r->fill,at ? at : 1,6);
    lv_obj_set_pos(r->knob,at,32);
    lv_obj_set_style_border_color(r->knob,lv_color_hex(editing ? 0xEDC56B : 0x74C5B5),0);
    lv_obj_set_style_text_color(r->value,lv_color_hex(editing ? 0xEDC56B : 0xF2F3E9),0);
    lv_label_set_text(r->value,value); lv_label_set_text(r->minimum,minimum); lv_label_set_text(r->maximum,max_label);
}
