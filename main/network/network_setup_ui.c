#include "network_setup_ui.h"
#include "src/misc/lv_text_private.h"
#include "../app/app_strings.h"
#include "../ui/app_header.h"
#include <stdio.h>
#include <string.h>
LV_FONT_DECLARE(score_font_14);
LV_FONT_DECLARE(network_font_14);
static lv_obj_t *label(lv_obj_t *p,int y,int width){
    lv_obj_t *o=lv_label_create(p);lv_obj_set_pos(o,(320-width)/2,y);lv_obj_set_width(o,width);
    lv_obj_set_style_text_font(o,&score_font_14,0);lv_obj_set_style_text_color(o,lv_color_hex(0xF2F3E9),0);
    lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);lv_label_set_long_mode(o,LV_LABEL_LONG_CLIP);lv_label_set_text(o,"");return o;
}
void net_setup_display_ssid(const char *src,char *dst,unsigned capacity){
    unsigned used=0;
    for(unsigned i=0;src[i] && used+1<capacity;){
        unsigned start=i;uint32_t at=i;uint32_t cp=lv_text_encoded_next(src,&at);i=(unsigned)at;lv_font_glyph_dsc_t glyph={0};
        if(!cp || i<=start)break;
        unsigned length=i-start;
        if(lv_font_get_glyph_dsc(&network_font_14,&glyph,cp,0) && !glyph.is_placeholder){
            if(used+length>=capacity)break;
            memcpy(dst+used,src+start,length);used+=length;
        }else dst[used++]='?';
    }
    if(capacity)dst[used]=0;
}
void net_setup_ui_create(net_setup_ui_t *u,lv_obj_t *parent){
    *u=(net_setup_ui_t){0};u->root=lv_obj_create(parent);lv_obj_remove_style_all(u->root);
    lv_obj_set_size(u->root,320,192);lv_obj_remove_flag(u->root,LV_OBJ_FLAG_SCROLLABLE);
    u->summary=label(u->root,34,292);u->detail=label(u->root,54,292);
    lv_obj_set_style_text_font(u->detail,&network_font_14,0);
    for(unsigned i=0;i<3;++i){u->rows[i]=label(u->root,78+(int)i*27,288);lv_obj_set_height(u->rows[i],25);
        lv_obj_set_style_pad_top(u->rows[i],4,0);lv_obj_set_style_radius(u->rows[i],5,0);
        lv_obj_set_style_bg_opa(u->rows[i],LV_OPA_COVER,0);}
    u->note=label(u->root,164,292);
}
static void show(lv_obj_t *o,bool yes){if(yes)lv_obj_remove_flag(o,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN);}
static const char *status_text(const app_network_snapshot_t *n){
    return n->state==APP_NET_CONNECTED ? TXT_APP_NETWORK_SUCCESS : n->state==APP_NET_CONNECTING ? TXT_APP_NETWORK_CONNECTING : n->state==APP_NET_FAILED ? TXT_APP_NETWORK_FAILED : TXT_APP_NETWORK_IDLE;
}
void net_setup_ui_render(net_setup_ui_t *u,const net_setup_t *s,const app_network_snapshot_t *n,int height){
    lv_obj_set_height(u->root,height);
    bool keyboard=s->page==NET_SETUP_PASSWORD || s->page==NET_SETUP_SSID;
    if(keyboard && !u->keyboard){
        // Only one content set is mounted to respect the 32 KB LVGL pool.
        lv_obj_clean(u->root);text_input_ui_create(&u->input,u->root,&score_font_14);u->keyboard=true;
    }else if(!keyboard && u->keyboard){
        lv_obj_t *parent=lv_obj_get_parent(u->root);lv_obj_delete(u->root);net_setup_ui_create(u,parent);lv_obj_set_height(u->root,height);
    }
    if(keyboard){app_header_context(s->page==NET_SETUP_PASSWORD ? TXT_APP_NET_PASSWORD : TXT_APP_NET_SSID);
        text_input_ui_render(&u->input,&s->editor,s->now,height);
        if(s->input_error)lv_label_set_text(u->input.context,TXT_APP_NET_INVALID);
        return;}
    app_header_context(s->page==NET_SETUP_SAVED ? TXT_APP_NET_SAVED_LIST : s->page==NET_SETUP_LIST ? TXT_APP_NET_SELECT : s->page==NET_SETUP_CONFIRM ? TXT_APP_NET_CONNECT_ASK : s->page==NET_SETUP_FORGET ? TXT_APP_NETWORK_FORGET : TXT_APP_NETWORK);
    char ssid[100];net_setup_display_ssid(n->ssid,ssid,sizeof(ssid));
    lv_label_set_text(u->summary,status_text(n));
    lv_obj_set_style_text_color(u->summary,lv_color_hex(n->state==APP_NET_CONNECTED ? 0x8DE0B8 : n->state==APP_NET_FAILED ? 0xED9A76 : 0xF2F3E9),0);
    if(*ssid)lv_label_set_text(u->detail,ssid);else lv_label_set_text(u->detail,"");
    lv_label_set_text(u->note,"");
    show(u->detail,true);lv_obj_set_y(u->summary,34);
    for(unsigned i=0;i<3;++i)show(u->rows[i],false);
    if(s->page==NET_SETUP_CONFIRM || s->page==NET_SETUP_FORGET){
        lv_obj_set_y(u->note,height-30);
        lv_label_set_text(u->summary,s->page==NET_SETUP_CONFIRM ? TXT_APP_NET_CONNECT_ASK : TXT_APP_NETWORK_FORGET_ASK);
        net_setup_display_ssid(s->page==NET_SETUP_CONFIRM ? s->credentials.ssid : n->ssid,ssid,sizeof(ssid));lv_label_set_text(u->detail,ssid);
        lv_label_set_text(u->note,s->page==NET_SETUP_CONFIRM ? s->saved_choice ? TXT_APP_NET_SAVED_REUSE : *s->credentials.password ? TXT_APP_NET_MASKED : TXT_APP_NET_OPEN : TXT_APP_NETWORK_SAVED);return;
    }
    bool listing=s->page==NET_SETUP_LIST || s->page==NET_SETUP_SAVED;
    show(u->detail,!listing);lv_obj_set_y(u->summary,listing ? 30 : 34);
    unsigned count=s->page==NET_SETUP_MENU ? 5 : s->page==NET_SETUP_SAVED ? s->saved.count : s->scan.count+2;
    unsigned first=s->focus>1 ? s->focus-1 : 0;if(first+3>count)first=count>3 ? count-3 : 0;
    for(unsigned i=0;i<3 && first+i<count;++i){
        unsigned item=first+i;lv_obj_t *row=u->rows[i];show(row,true);
        lv_obj_set_y(row,(listing ? 56 : 78)+(int)i*(listing ? 34 : 27));
        lv_obj_set_height(row,listing ? 32 : 25);lv_obj_set_style_pad_top(row,listing ? 2 : 4,0);
        bool network=s->page==NET_SETUP_SAVED || (s->page==NET_SETUP_LIST && item<s->scan.count);
        lv_obj_set_style_text_font(row,network ? &network_font_14 : &score_font_14,0);
        net_setup_display_ssid(net_setup_item(s,item),ssid,sizeof(ssid));
        // Fixed menu labels use the application font, whose coverage is separately verified.
        if(s->page==NET_SETUP_MENU && item==1)lv_label_set_text_fmt(row,"%s · %s",net_setup_item(s,item),n->portal ? TXT_APP_NETWORK_OFF : TXT_APP_NETWORK_ON);
        else lv_label_set_text(row,network ? ssid : net_setup_item(s,item));
        lv_obj_set_style_bg_color(row,lv_color_hex(item==s->focus ? 0x246B4E : 0x1C303B),0);
    }
    if(s->page==NET_SETUP_SAVED){
        lv_label_set_text_fmt(u->note,"%u/%u",s->saved.count,APP_NET_SAVED_MAX);
    }else if(s->page==NET_SETUP_LIST){
        if(s->scan.scanning)lv_label_set_text(u->note,TXT_APP_NET_EMPTY);
        else if(s->scan.error)lv_label_set_text(u->note,TXT_APP_NET_BUSY);
        else if(s->focus<s->scan.count)lv_label_set_text_fmt(u->note,"%s · %d dBm · %u/%u",s->scan.entries[s->focus].secured ? TXT_APP_NET_PROTECTED : TXT_APP_NET_OPEN,s->scan.entries[s->focus].rssi,s->focus+1,count);
        else lv_label_set_text(u->note,s->scan.count ? TXT_APP_NET_MANUAL : TXT_APP_NET_NOT_FOUND);
    }else if(n->portal && s->focus==1)lv_label_set_text_fmt(u->note,"%s · 192.168.4.1",n->ap_name);
    else if(n->state==APP_NET_CONNECTED){
        if(n->save_error)lv_label_set_text(u->note,TXT_APP_NETWORK_SAVE_FAILED);else lv_label_set_text_fmt(u->note,"IP %s",n->ip);
    }
    else if(n->state==APP_NET_FAILED){
        const char *reason=app_net_issue(n->reason)==APP_NET_ISSUE_AUTH ? TXT_APP_NETWORK_AUTH : app_net_issue(n->reason)==APP_NET_ISSUE_MISSING ? TXT_APP_NETWORK_MISSING : TXT_APP_NETWORK_RETRY_HELP;
        lv_label_set_text(u->note,reason);
    }else if(n->portal)lv_label_set_text_fmt(u->note,TXT_APP_NETWORK_TIME,n->seconds_left);
    lv_obj_set_y(u->note,height-22);
}
