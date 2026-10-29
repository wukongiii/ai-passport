#include "network_setup.h"
#include "../app/app_strings.h"
#include <string.h>
void net_setup_begin(net_setup_t *s){memset(s,0,sizeof(*s));}
void net_setup_dispose(net_setup_t *s){text_input_dispose(&s->editor);memset(s,0,sizeof(*s));}
void net_setup_update(net_setup_t *s,const app_net_scan_t *scan){
    // Snapshot has no references into the worker-owned scan buffer.
    if(scan->revision!=s->scan.revision){
        char selected[33]={0};bool secured=false,had=s->page==NET_SETUP_LIST && s->focus<s->scan.count;
        if(had){memcpy(selected,s->scan.entries[s->focus].ssid,sizeof(selected));secured=s->scan.entries[s->focus].secured;}
        s->scan=*scan;
        if(had){s->focus=0;for(unsigned i=0;i<s->scan.count;++i)if(!strcmp(selected,s->scan.entries[i].ssid) && secured==s->scan.entries[i].secured){s->focus=i;break;}}
        if(s->focus>s->scan.count+1 && s->page==NET_SETUP_LIST)s->focus=0;
    }
}
static const char *const menu[]={TXT_APP_NET_SELECT,TXT_APP_NET_PHONE,TXT_APP_NETWORK_RETRY,TXT_APP_NETWORK_FORGET};
const char *net_setup_item(const net_setup_t *s,unsigned item){
    if(s->page==NET_SETUP_MENU)return item<4 ? menu[item] : "";
    if(item<s->scan.count)return s->scan.entries[item].ssid;
    return item==s->scan.count ? TXT_APP_NET_MANUAL : TXT_APP_NET_RESCAN;
}
void net_setup_bindings(const net_setup_t *s,const app_network_snapshot_t *n,app_bindings_t *b){
    app_actions_clear(b);
    if(s->page==NET_SETUP_SSID || s->page==NET_SETUP_PASSWORD){text_input_bindings(&s->editor,b);return;}
    bool confirmation=s->page==NET_SETUP_CONFIRM || s->page==NET_SETUP_FORGET;
    app_actions_register(b,0,APP_GESTURE_SHORT,100,confirmation ? TXT_APP_CANCEL : TXT_APP_PREVIOUS,true,confirmation ? APP_ACTION_CANCEL : APP_ACTION_NAVIGATE);
    bool enabled=!n->busy;
    if(s->page==NET_SETUP_MENU){enabled=n->ready && (s->focus==1 || !n->busy);if(s->focus>=2)enabled=enabled && n->saved;}
    if(s->page==NET_SETUP_LIST && s->scan.scanning)enabled=false;
    app_actions_register(b,1,APP_GESTURE_SHORT,101,confirmation ? TXT_APP_CONFIRM : TXT_APP_ENTER,enabled,APP_ACTION_CONFIRM);
    app_actions_register(b,2,APP_GESTURE_SHORT,102,confirmation ? TXT_APP_BACK : TXT_APP_NEXT,true,confirmation ? APP_ACTION_BACK : APP_ACTION_NAVIGATE);
    app_actions_register(b,1,APP_GESTURE_LONG,103,TXT_APP_BACK,true,APP_ACTION_BACK);
}
static void begin_editor(net_setup_t *s,bool password){
    text_input_config_t c={.maximum_length=password ? 64 : 32,.secret=password};
    text_input_begin(&s->editor,&c,s->now);s->page=password ? NET_SETUP_PASSWORD : NET_SETUP_SSID;s->input_error=false;
}
net_setup_effect_t net_setup_input(net_setup_t *s,const app_network_snapshot_t *n,unsigned slot,app_gesture_t gesture){
    app_bindings_t b;net_setup_bindings(s,n,&b);if(!app_actions_resolve(&b,slot,gesture))return NET_SETUP_NONE;
    if(s->page==NET_SETUP_SSID || s->page==NET_SETUP_PASSWORD){
        text_input_gesture(&s->editor,slot,gesture,s->now);
        if(s->editor.page==TEXT_INPUT_CANCELLED){text_input_dispose(&s->editor);memset(&s->credentials,0,sizeof(s->credentials));s->page=NET_SETUP_LIST;s->focus=0;}
        else if(s->editor.page==TEXT_INPUT_DONE){
            if(s->page==NET_SETUP_SSID){
                if(!s->editor.length){s->editor.page=TEXT_INPUT_CHARACTERS;s->input_error=true;return NET_SETUP_NONE;}
                memcpy(s->credentials.ssid,s->editor.text,s->editor.length+1);text_input_dispose(&s->editor);begin_editor(s,true);
            }else{
                memcpy(s->credentials.password,s->editor.text,s->editor.length+1);
                if(!app_net_credentials_valid(&s->credentials)){s->editor.page=TEXT_INPUT_CHARACTERS;s->input_error=true;memset(s->credentials.password,0,sizeof(s->credentials.password));return NET_SETUP_NONE;}
                text_input_dispose(&s->editor);s->page=NET_SETUP_CONFIRM;s->input_error=false;
            }
        }return NET_SETUP_NONE;
    }
    if((slot==1 && gesture==APP_GESTURE_LONG) || ((s->page==NET_SETUP_CONFIRM || s->page==NET_SETUP_FORGET) && slot!=1)){
        if(s->page==NET_SETUP_MENU)return NET_SETUP_EXIT;
        if(s->page==NET_SETUP_CONFIRM){memset(&s->credentials,0,sizeof(s->credentials));s->page=NET_SETUP_LIST;s->focus=0;}
        else{s->page=NET_SETUP_MENU;s->focus=0;}return NET_SETUP_NONE;
    }
    if(gesture!=APP_GESTURE_SHORT)return NET_SETUP_NONE;
    if(slot!=1){unsigned count=s->page==NET_SETUP_MENU ? 4 : s->scan.count+2;s->focus=(s->focus+count+(slot==0 ? -1 : 1))%count;return NET_SETUP_NONE;}
    if(s->page==NET_SETUP_MENU){
        switch(s->focus){
        case 0:s->page=NET_SETUP_LIST;s->focus=0;return NET_SETUP_SCAN;
        case 1:return NET_SETUP_PORTAL;
        case 2:return NET_SETUP_RETRY;
        default:s->page=NET_SETUP_FORGET;return NET_SETUP_NONE;
        }
    }
    if(s->page==NET_SETUP_FORGET){s->page=NET_SETUP_MENU;s->focus=0;return NET_SETUP_ERASE;}
    if(s->page==NET_SETUP_CONFIRM){s->page=NET_SETUP_MENU;s->focus=0;return NET_SETUP_CONNECT;}
    if(s->focus==s->scan.count+1){s->focus=0;return NET_SETUP_SCAN;}
    memset(&s->credentials,0,sizeof(s->credentials));
    if(s->focus==s->scan.count){begin_editor(s,false);return NET_SETUP_NONE;}
    memcpy(s->credentials.ssid,s->scan.entries[s->focus].ssid,sizeof(s->credentials.ssid));
    if(s->scan.entries[s->focus].secured)begin_editor(s,true);else s->page=NET_SETUP_CONFIRM;
    return NET_SETUP_NONE;
}
