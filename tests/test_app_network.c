#include "network/app_network_model.h"
#include "network/app_network_socket.h"
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
    struct sockaddr_storage address={0};
    struct sockaddr_in *v4=(struct sockaddr_in *)&address;
    v4->sin_family=AF_INET;
    assert(inet_pton(AF_INET,"192.168.4.1",&v4->sin_addr)==1);
    assert(app_net_address_is_setup(&address,sizeof(*v4)));
    assert(!app_net_address_is_setup(&address,sizeof(*v4)-1));
    assert(inet_pton(AF_INET,"192.168.1.10",&v4->sin_addr)==1);
    assert(!app_net_address_is_setup(&address,sizeof(*v4)));
    memset(&address,0,sizeof(address));
    struct sockaddr_in6 *v6=(struct sockaddr_in6 *)&address;
    v6->sin6_family=AF_INET6;
    assert(inet_pton(AF_INET6,"::ffff:192.168.4.1",&v6->sin6_addr)==1);
    assert(app_net_address_is_setup(&address,sizeof(*v6)));
    // Reproduce the old truncated-buffer check on a dual-stack HTTP socket.
    assert(!app_net_address_is_setup(&address,sizeof(struct sockaddr_in)));
    assert(inet_pton(AF_INET6,"::ffff:192.168.1.10",&v6->sin6_addr)==1);
    assert(!app_net_address_is_setup(&address,sizeof(*v6)));
    assert(inet_pton(AF_INET6,"2001:db8::c0a8:401",&v6->sin6_addr)==1);
    assert(!app_net_address_is_setup(&address,sizeof(*v6)));
    assert(!app_net_address_is_setup(&address,0));
    app_net_scan_t scan={0};
    app_net_access_point_t ap={.ssid="Table \"A\"",.rssi=-80,.secured=true};
    app_net_scan_add(&scan,&ap);ap.rssi=-40;app_net_scan_add(&scan,&ap);
    assert(scan.count==1 && scan.entries[0].rssi==-40);
    ap.rssi=-90;app_net_scan_add(&scan,&ap);assert(scan.entries[0].rssi==-40);
    ap.secured=false;app_net_scan_add(&scan,&ap);assert(scan.count==2);
    ap.ssid[0]=0;app_net_scan_add(&scan,&ap);assert(scan.count==2);
    memset(ap.ssid,'x',sizeof(ap.ssid));app_net_scan_add(&scan,&ap);assert(scan.count==2);
    for(unsigned i=0;i<20;++i){
        snprintf(ap.ssid,sizeof(ap.ssid),"Preview-%u",i);ap.rssi=-30-(int)i;
        app_net_scan_add(&scan,&ap);
    }
    assert(scan.count==APP_NET_SCAN_MAX && !strcmp(scan.entries[0].ssid,"Preview-0"));
    for(unsigned i=1;i<scan.count;++i)assert(scan.entries[i-1].rssi>=scan.entries[i].rssi);
    assert(scan.entries[scan.count-1].rssi==-44);
    strcpy(ap.ssid,"台球网络");ap.rssi=-10;app_net_scan_add(&scan,&ap);
    assert(scan.count==APP_NET_SCAN_MAX && !strcmp(scan.entries[0].ssid,"台球网络"));
    app_net_credentials_t c={0};char out[8];
    assert(app_net_parse_credentials("ssid=Test+Network&password=Abc%2B12%26%3D",&c));
    assert(!strcmp(c.ssid,"Test Network") && !strcmp(c.password,"Abc+12&="));
    assert(app_net_parse_credentials("ssid=%E5%8F%B0%E7%90%83&password=",&c));
    assert(!app_net_parse_credentials("ssid=x&password=short",&c));
    assert(!app_net_parse_credentials("ssid=x&password=12345678&ssid=y",&c));
    assert(!app_net_parse_credentials("ssid=x%00y&password=12345678",&c));
    assert(!app_net_parse_credentials("ssid=x%0Ay&password=12345678",&c));
    assert(!app_net_parse_credentials("ssid=%Q0&password=12345678",&c));
    assert(!app_net_parse_credentials("ssid=x%&password=12345678",&c));
    assert(!app_net_form_value("a=12345678","a",out,sizeof(out)));
    assert(!app_net_form_value("a=x&a=y","a",out,sizeof(out)));
    assert(!app_net_parse_credentials("ssid=123456789012345678901234567890123&password=12345678",&c));
    memset(&c,'a',sizeof(c));assert(!app_net_credentials_valid(&c));
    app_preferences_t p;
    assert(app_net_parse_preferences("speech_enabled=1&double_click_ms=350&rotation=90&volume=60&brightness=80&dim=0&blank=120",&p));
    assert(p.rotation==APP_ROTATION_90 && p.dim_seconds==0);
    assert(!app_net_parse_preferences("speech_enabled=1&double_click_ms=350&rotation=90&brightness=80&dim=0&blank=120&volume=101",&p));
    assert(!app_net_parse_preferences("speech_enabled=1&double_click_ms=350&rotation=90&brightness=80&dim=0&blank=120&volume=63",&p));
    assert(!app_net_parse_preferences("speech_enabled=1&double_click_ms=350&rotation=90&brightness=80&dim=0&blank=120",&p));
    assert(!app_net_parse_preferences("speech_enabled=1&double_click_ms=350&rotation=180&volume=60&brightness=80&dim=0&blank=120",&p));
    assert(!app_net_parse_preferences("speech_enabled=1&double_click_ms=350&rotation=90&volume=60&brightness=81&dim=0&blank=120",&p));
    assert(!app_net_parse_preferences("speech_enabled=1&double_click_ms=350&rotation=90&volume=60&brightness=80&dim=65536&blank=120",&p));
    assert(!app_net_parse_preferences("speech_enabled=1&double_click_ms=350&rotation=90&volume=60&brightness=80&dim=-1&blank=120",&p));
    assert(!app_net_parse_preferences("speech_enabled=1&double_click_ms=350&rotation=90&volume=60&brightness=80&dim=30",&p));
    p=app_preferences_default();app_preferences_t updated;
    assert(app_net_patch_preferences("section=operation&double_click_ms=450",&p,&updated)&&updated.double_click_ms==450&&updated.volume==p.volume&&updated.rotation==p.rotation);
    assert(!app_net_patch_preferences("section=operation&double_click_ms=451",&p,&updated));
    assert(!app_net_patch_preferences("section=operation&double_click_ms=300&double_click_ms=400",&p,&updated));
    assert(app_net_patch_preferences("section=sound&speech_enabled=0&volume=30",&p,&updated)&&updated.volume==30&&updated.double_click_ms==350&&!updated.speech_enabled&&updated.rotation==p.rotation);
    assert(app_net_patch_preferences("section=display&rotation=90&brightness=20&dim=0&blank=0",&p,&updated)&&updated.rotation==90&&updated.volume==60&&updated.double_click_ms==350);
    assert(!app_net_patch_preferences("section=sound&volume=30",&p,&updated));
    assert(!app_net_patch_preferences("section=sound&volume=30&speech_enabled=2",&p,&updated));
    assert(!app_net_patch_preferences("section=sound&volume=30&speech_enabled=1&speech_enabled=0",&p,&updated));
    p.speech_enabled=false;
    assert(app_net_patch_preferences("section=display&rotation=90&brightness=20&dim=0&blank=0",&p,&updated)&&!updated.speech_enabled);
    uint8_t q[]={0x12,0x34,1,0,0,1,0,0,0,0,0,0,3,'a','p','p',0,0,1,0,1},answer[64];
    size_t n=app_net_dns_response(q,sizeof(q),answer,sizeof(answer));
    assert(n==sizeof(q)+16 && answer[0]==0x12 && answer[7]==1 && answer[n-1]==1);
    assert(!app_net_dns_response(q,sizeof(q),answer,20));
    q[12]=0xC0;assert(!app_net_dns_response(q,sizeof(q),answer,sizeof(answer)));q[12]=3;
    q[18]=28;assert(!app_net_dns_response(q,sizeof(q),answer,sizeof(answer)));q[18]=1;
    q[5]=2;assert(!app_net_dns_response(q,sizeof(q),answer,sizeof(answer)));
    assert(app_net_issue(202)==APP_NET_ISSUE_AUTH);
    assert(app_net_issue(15)==APP_NET_ISSUE_AUTH);
    assert(app_net_issue(204)==APP_NET_ISSUE_AUTH);
    assert(app_net_issue(201)==APP_NET_ISSUE_MISSING);
    assert(app_net_issue(212)==APP_NET_ISSUE_MISSING);
    assert(app_net_issue(210)==APP_NET_ISSUE_SECURITY);
    assert(app_net_issue(0)==APP_NET_ISSUE_GENERAL);
    assert(app_net_issue(999)==APP_NET_ISSUE_GENERAL);
    unsigned attempts=0;int64_t retry=0;
    assert(app_net_schedule_retry(&attempts,100,&retry) && retry==2000100);
    assert(app_net_schedule_retry(&attempts,retry,&retry) && retry==6000100);
    assert(app_net_schedule_retry(&attempts,retry,&retry) && retry==12000100);
    assert(!app_net_schedule_retry(&attempts,retry,&retry) && retry==0 && attempts==3);
    attempts=0;assert(app_net_schedule_retry(&attempts,0,&retry) && retry==2000000);
    assert(app_net_remaining_seconds(300000000,0)==300);
    assert(app_net_remaining_seconds(300000000,299000001)==1);
    assert(app_net_remaining_seconds(300000000,300000000)==0);
    assert(app_net_remaining_seconds(300000000,301000000)==0);
    puts("Network strict form decoding, credential limits and shared setting validation: PASS");
}
