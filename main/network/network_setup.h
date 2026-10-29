#pragma once
#include "app_network.h"
#include "../input/text_input.h"
typedef enum { NET_SETUP_MENU,NET_SETUP_LIST,NET_SETUP_SSID,NET_SETUP_PASSWORD,NET_SETUP_CONFIRM,NET_SETUP_FORGET } net_setup_page_t;
typedef enum { NET_SETUP_NONE,NET_SETUP_EXIT,NET_SETUP_SCAN,NET_SETUP_CONNECT,NET_SETUP_PORTAL,NET_SETUP_RETRY,NET_SETUP_ERASE } net_setup_effect_t;
typedef struct {
    net_setup_page_t page;
    unsigned focus;
    app_net_scan_t scan;
    app_net_credentials_t credentials;
    text_input_t editor;
    uint64_t now;
    bool input_error;
} net_setup_t;
void net_setup_begin(net_setup_t *);
void net_setup_dispose(net_setup_t *);
void net_setup_update(net_setup_t *,const app_net_scan_t *);
void net_setup_bindings(const net_setup_t *,const app_network_snapshot_t *,app_bindings_t *);
net_setup_effect_t net_setup_input(net_setup_t *,const app_network_snapshot_t *,unsigned,app_gesture_t);
const char *net_setup_item(const net_setup_t *,unsigned);
