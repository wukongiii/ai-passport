#include "app_header.h"
#include "../network/app_network.h"
#include <stdio.h>
#include <string.h>
LV_FONT_DECLARE(score_font_14);
static lv_obj_t *root;
static struct {char module[64],context[80];int battery;bool home;app_network_snapshot_t network;} view;
static void text(lv_layer_t *layer,const char *str,int x,int y,int width,uint32_t color,lv_text_align_t align){
    lv_draw_label_dsc_t d;lv_draw_label_dsc_init(&d);d.font=&score_font_14;d.text=str;d.text_local=1;
    d.align=align;d.color=lv_color_hex(color);lv_area_t area={x,y,x+width-1,y+20};lv_draw_label(layer,&d,&area);
}
static void draw(lv_event_t *e){
    lv_layer_t *layer=lv_event_get_layer(e);lv_area_t a;lv_obj_get_coords(root,&a);
    if(!view.home){text(layer,view.module,a.x1+14,a.y1+6,86,0xA6B5B7,LV_TEXT_ALIGN_LEFT);
        text(layer,view.context,a.x1+98,a.y1+6,128,0xF2F3E9,LV_TEXT_ALIGN_CENTER);}
    const bool connected=view.network.state==APP_NET_CONNECTED;
    uint32_t color=connected ? 0x8DE0B8 : view.network.busy ? 0xEDC56B : view.network.portal ? 0x74C5D9 : 0x687C87;
    for(int i=0;i<3;++i){
        lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);d.bg_opa=LV_OPA_COVER;d.radius=1;
        d.bg_color=lv_color_hex(connected || (view.network.busy && i==0) || view.network.portal ? color : 0x48545C);
        lv_area_t bar={a.x1+232+i*5,a.y1+18-i*4,a.x1+234+i*5,a.y1+22};lv_draw_rect(layer,&d,&bar);
    }
    if(view.network.state==APP_NET_FAILED){
        text(layer,"!",a.x1+232,a.y1+4,16,0xED9A76,LV_TEXT_ALIGN_CENTER);
    }
    char battery[12];if(view.battery<0)snprintf(battery,sizeof(battery),"--%%");else snprintf(battery,sizeof(battery),"%d%%",view.battery);
    text(layer,battery,a.x1+253,a.y1+6,50,0xA6B5B7,LV_TEXT_ALIGN_RIGHT);
}
void app_header_create(lv_obj_t *screen){
    root=lv_obj_create(screen);lv_obj_remove_style_all(root);lv_obj_set_size(root,320,28);
    lv_obj_remove_flag(root,LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_event_cb(root,draw,LV_EVENT_DRAW_MAIN,NULL);
}
void app_header_context(const char *text_value){snprintf(view.context,sizeof(view.context),"%s",text_value ? text_value : "");}
void app_header_render(int y,const char *module,int battery,bool home){
    snprintf(view.module,sizeof(view.module),"%s",module ? module : "");view.battery=battery;view.home=home;
    app_network_snapshot(&view.network);lv_obj_set_y(root,y);
    lv_obj_set_style_bg_color(root,lv_color_hex(0x263B47),0);lv_obj_set_style_bg_opa(root,home ? LV_OPA_TRANSP : LV_OPA_COVER,0);
    lv_obj_move_foreground(root);lv_obj_invalidate(root);
}
