#pragma once
#include "network_setup.h"
#include "../input/text_input_ui.h"
typedef struct {lv_obj_t *root,*summary,*detail,*rows[3],*note; text_input_ui_t input; bool keyboard;} net_setup_ui_t;
void net_setup_ui_create(net_setup_ui_t *,lv_obj_t *);
void net_setup_ui_render(net_setup_ui_t *,const net_setup_t *,const app_network_snapshot_t *,int);
/* Unsupported display code points become '?' without changing the network bytes. */
void net_setup_display_ssid(const char *,char *,unsigned);
