#include "app_network_model.h"
#include <stdlib.h>
#include <string.h>
static int hex(char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;}
bool app_net_form_value(const char *form,const char *key,char *out,size_t capacity){
    if(!form || !key || !out || !capacity || strlen(form)>APP_NET_FORM_MAX)return false;
    bool found=false;size_t keylen=strlen(key);out[0]=0;
    for(const char *p=form;*p;){
        const char *end=strchr(p,'&');if(!end)end=p+strlen(p);
        const char *eq=memchr(p,'=',(size_t)(end-p));
        if(eq && (size_t)(eq-p)==keylen && !memcmp(p,key,keylen)){
            if(found)return false;
            found=true;size_t n=0;
            for(const char *v=eq+1;v<end;++v){int c=(unsigned char)*v;
                if(c=='%'){if(end-v<3 || hex(v[1])<0 || hex(v[2])<0)return false;c=hex(v[1])*16+hex(v[2]);v+=2;}
                else if(c=='+')c=' ';
                if(!c || c<32 || c==127 || n+1>=capacity)return false;
                out[n++]=(char)c;
            }out[n]=0;
        }p=*end ? end+1 : end;
    }return found;
}
bool app_net_credentials_valid(const app_net_credentials_t *c){
    if(!c)return false;
    const char *ssid_end=memchr(c->ssid,0,sizeof(c->ssid)),*pass_end=memchr(c->password,0,sizeof(c->password));
    if(!ssid_end || !pass_end)return false;
    size_t s=(size_t)(ssid_end-c->ssid),p=(size_t)(pass_end-c->password);
    if(!s || s>32 || p>64 || (p && p<8))return false;
    for(size_t i=0;i<s;++i)if((unsigned char)c->ssid[i]<32 || (unsigned char)c->ssid[i]==127)return false;
    for(size_t i=0;i<p;++i)if((unsigned char)c->password[i]<32 || (unsigned char)c->password[i]>126)return false;
    if(p==64)for(size_t i=0;i<p;++i)if(hex(c->password[i])<0)return false;
    return true;
}
bool app_net_parse_credentials(const char *form,app_net_credentials_t *out){
    app_net_credentials_t c={0};bool ok=app_net_form_value(form,"ssid",c.ssid,sizeof(c.ssid)) &&
        app_net_form_value(form,"password",c.password,sizeof(c.password)) && app_net_credentials_valid(&c);
    if(ok)*out=c;
    memset(&c,0,sizeof(c));return ok;
}
static bool number(const char *form,const char *key,unsigned *out){
    char b[8];if(!app_net_form_value(form,key,b,sizeof(b)) || !*b)return false;
    for(char *p=b;*p;++p)if(*p<'0'||*p>'9')return false;
    *out=(unsigned)strtoul(b,NULL,10);return true;
}
bool app_net_parse_preferences(const char *form,app_preferences_t *out){
    unsigned r,b,d,k;
    if(!number(form,"rotation",&r)||!number(form,"brightness",&b)||!number(form,"dim",&d)||!number(form,"blank",&k) || b>255 || d>65535 || k>65535)return false;
    app_preferences_t p={.rotation=(app_rotation_t)r,.brightness=(uint8_t)b,.dim_seconds=(uint16_t)d,.blank_seconds=(uint16_t)k};
    if(!app_preferences_valid(&p))return false;
    *out=p;return true;
}
size_t app_net_dns_response(const uint8_t *q,size_t length,uint8_t *out,size_t capacity){
    if(!q || !out || length<17 || length>480 || (q[2]&0xF8) || q[4]!=0 || q[5]!=1)return 0;
    size_t end=12;
    while(end<length && q[end]){unsigned n=q[end++];if(n>63 || n>length-end)return 0;end+=n;}
    if(end>=length || length-end<5)return 0;
    ++end;
    if(q[end]!=0 || q[end+1]!=1 || q[end+2]!=0 || q[end+3]!=1)return 0;
    end+=4;if(end+16>capacity)return 0;
    memcpy(out,q,end);out[2]=0x81;out[3]=0x80;out[6]=0;out[7]=1;
    out[8]=out[9]=out[10]=out[11]=0;
    const uint8_t answer[]={0xC0,0x0C,0,1,0,1,0,0,0,0,0,4,192,168,4,1};
    memcpy(out+end,answer,sizeof(answer));return end+sizeof(answer);
}

bool app_net_schedule_retry(unsigned *attempts,int64_t now,int64_t *retry_at){
    if(*attempts>=APP_NET_RETRY_LIMIT){*retry_at=0;return false;}
    ++*attempts;*retry_at=now+(int64_t)*attempts*2000000;return true;
}
unsigned app_net_remaining_seconds(int64_t deadline,int64_t now){
    return now>=deadline ? 0 : (unsigned)((deadline-now+999999)/1000000);
}
void app_net_scan_add(app_net_scan_t *scan,const app_net_access_point_t *ap){
    app_net_credentials_t check={0};memcpy(check.ssid,ap->ssid,sizeof(check.ssid));
    if(!app_net_credentials_valid(&check))return;
    unsigned pos=scan->count;
    for(unsigned i=0;i<scan->count;++i){
        if(!strcmp(scan->entries[i].ssid,ap->ssid) && scan->entries[i].secured==ap->secured){
            if(scan->entries[i].rssi>=ap->rssi)return;
            pos=i;break;
        }
    }
    if(pos==scan->count){
        if(pos==APP_NET_SCAN_MAX){
            if(ap->rssi<=scan->entries[pos-1].rssi)return;
            --pos;
        }else ++scan->count;
    }
    scan->entries[pos]=*ap;
    while(pos && scan->entries[pos].rssi>scan->entries[pos-1].rssi){
        app_net_access_point_t swap=scan->entries[pos-1];
        scan->entries[pos-1]=scan->entries[pos];scan->entries[pos]=swap;--pos;
    }
}
app_net_issue_t app_net_issue(unsigned reason){
    // ESP-IDF Wi-Fi disconnect reason values; authentication is not proof of a bad password.
    if(reason==15 || reason==23 || reason==202 || reason==204)return APP_NET_ISSUE_AUTH;
    if(reason==201 || reason==212)return APP_NET_ISSUE_MISSING;
    if(reason==210 || reason==211)return APP_NET_ISSUE_SECURITY;
    return APP_NET_ISSUE_GENERAL;
}

void app_net_history_init(app_net_history_t *h){memset(h,0,sizeof(*h));h->version=APP_NET_HISTORY_VERSION;}
bool app_net_history_valid(const app_net_history_t *h){
    if(!h || h->version!=APP_NET_HISTORY_VERSION || h->count>APP_NET_SAVED_MAX ||
       h->next>=APP_NET_SAVED_MAX || (h->count && h->preferred>=h->count) ||
       (!h->count && h->preferred) || (h->count<APP_NET_SAVED_MAX && h->next!=h->count))return false;
    for(unsigned i=0;i<h->count;++i){
        if(!app_net_credentials_valid(&h->entries[i]))return false;
        for(unsigned j=0;j<i;++j)if(!strcmp(h->entries[i].ssid,h->entries[j].ssid))return false;
    }return true;
}
int app_net_history_find(const app_net_history_t *h,const char *ssid){
    if(!ssid)return -1;
    for(unsigned i=0;i<h->count;++i)if(!strcmp(h->entries[i].ssid,ssid))return (int)i;
    return -1;
}
bool app_net_history_remember(app_net_history_t *h,const app_net_credentials_t *c){
    if(!app_net_history_valid(h)||!app_net_credentials_valid(c))return false;
    int found=app_net_history_find(h,c->ssid);unsigned slot=found<0 ? h->next : (unsigned)found;
    h->entries[slot]=*c;h->preferred=(uint8_t)slot;
    if(found<0){if(h->count<APP_NET_SAVED_MAX)++h->count;h->next=(uint8_t)((slot+1)%APP_NET_SAVED_MAX);}
    return true;
}
void app_net_history_list(const app_net_history_t *h,app_net_saved_t *out){
    memset(out,0,sizeof(*out));out->count=h->count;
    for(unsigned i=0;i<h->count;++i){
        unsigned slot=(h->next+APP_NET_SAVED_MAX-1-i)%APP_NET_SAVED_MAX;
        memcpy(out->ssids[i],h->entries[slot].ssid,sizeof(out->ssids[i]));
    }
}
