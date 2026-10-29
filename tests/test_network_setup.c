#include "network/network_setup.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static net_setup_effect_t press(net_setup_t *s,app_network_snapshot_t *n,unsigned slot,app_gesture_t g){return net_setup_input(s,n,slot,g);}
static void finish(net_setup_t *s,const char *value){
    memcpy(s->editor.text,value,strlen(value)+1);s->editor.length=s->editor.cursor=(unsigned)strlen(value);
    s->editor.page=TEXT_INPUT_EDIT;s->editor.edit_focus=3;
}
int main(void){
    net_setup_t s;app_network_snapshot_t n={.ready=true};net_setup_begin(&s);
    assert(press(&s,&n,1,APP_GESTURE_SHORT)==NET_SETUP_SCAN && s.page==NET_SETUP_LIST);
    app_net_scan_t scan={.count=2,.revision=1};strcpy(scan.entries[0].ssid,"台球室");scan.entries[0].secured=true;strcpy(scan.entries[1].ssid,"Guest");
    net_setup_update(&s,&scan);assert(press(&s,&n,1,APP_GESTURE_SHORT)==NET_SETUP_NONE && s.page==NET_SETUP_PASSWORD);
    assert(s.editor.config.secret && !strcmp(s.credentials.ssid,"台球室"));
    finish(&s,"short");press(&s,&n,1,APP_GESTURE_SHORT);assert(s.input_error && s.page==NET_SETUP_PASSWORD && !*s.credentials.password);
    finish(&s,"Preview1!");press(&s,&n,1,APP_GESTURE_SHORT);assert(s.page==NET_SETUP_CONFIRM && !s.editor.length);
    assert(press(&s,&n,1,APP_GESTURE_SHORT)==NET_SETUP_CONNECT && !strcmp(s.credentials.password,"Preview1!"));
    net_setup_dispose(&s);assert(!*s.credentials.password && !s.editor.length);
    net_setup_begin(&s);press(&s,&n,1,APP_GESTURE_SHORT);net_setup_update(&s,&scan);press(&s,&n,2,APP_GESTURE_SHORT);press(&s,&n,1,APP_GESTURE_SHORT);
    assert(s.page==NET_SETUP_CONFIRM && !strcmp(s.credentials.ssid,"Guest") && !*s.credentials.password);
    press(&s,&n,0,APP_GESTURE_SHORT);assert(s.page==NET_SETUP_LIST && !*s.credentials.ssid);
    s.focus=s.scan.count;press(&s,&n,1,APP_GESTURE_SHORT);assert(s.page==NET_SETUP_SSID);
    finish(&s,"");press(&s,&n,1,APP_GESTURE_SHORT);assert(s.input_error && s.page==NET_SETUP_SSID);
    finish(&s,"Hidden");press(&s,&n,1,APP_GESTURE_SHORT);assert(s.page==NET_SETUP_PASSWORD && !strcmp(s.credentials.ssid,"Hidden"));
    finish(&s,"");press(&s,&n,1,APP_GESTURE_SHORT);assert(s.page==NET_SETUP_CONFIRM);
    press(&s,&n,2,APP_GESTURE_SHORT);press(&s,&n,1,APP_GESTURE_LONG);assert(s.page==NET_SETUP_MENU);
    assert(press(&s,&n,1,APP_GESTURE_LONG)==NET_SETUP_EXIT);
    n.busy=true;assert(press(&s,&n,1,APP_GESTURE_SHORT)==NET_SETUP_NONE);
    n.busy=false;s.focus=1;assert(press(&s,&n,1,APP_GESTURE_SHORT)==NET_SETUP_PORTAL);
    s.focus=3;assert(press(&s,&n,1,APP_GESTURE_SHORT)==NET_SETUP_NONE);n.saved=true;
    press(&s,&n,1,APP_GESTURE_SHORT);assert(s.page==NET_SETUP_FORGET);
    assert(press(&s,&n,1,APP_GESTURE_SHORT)==NET_SETUP_ERASE);
    net_setup_dispose(&s);puts("Device network selection, shared keyboard, validation, confirmation and secret cleanup: PASS");
}
