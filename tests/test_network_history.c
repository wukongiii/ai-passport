#include "network/network_history_store.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static app_net_history_t disk;static app_net_credentials_t legacy={.ssid="Original",.password="password"};
static bool stored,legacy_stored,fail_write,fail_commit;static unsigned writes;
esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *out,size_t *size){
    assert(h==1);const void *data;size_t length;
    if(!strcmp(key,"history")){if(!stored)return ESP_ERR_NVS_NOT_FOUND;data=&disk;length=sizeof(disk);}
    else{assert(!strcmp(key,"station"));if(!legacy_stored)return ESP_ERR_NVS_NOT_FOUND;data=&legacy;length=sizeof(legacy);}
    if(out){if(*size<length)return ESP_ERR_NVS_INVALID_LENGTH;memcpy(out,data,length);}*size=length;return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *data,size_t size){
    assert(h==1&&!strcmp(key,"history")&&size==sizeof(disk));++writes;if(fail_write)return ESP_FAIL;
    if(!fail_commit){memcpy(&disk,data,size);stored=true;}return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t h){assert(h==1);return fail_commit?ESP_FAIL:ESP_OK;}
static app_net_credentials_t network(unsigned i){app_net_credentials_t c={.password="password"};snprintf(c.ssid,sizeof(c.ssid),"Network-%u",i);return c;}
int main(void){
    app_net_history_t h,reloaded;assert(net_history_load(1,&h)==ESP_OK&&h.count==0);
    legacy_stored=true;assert(net_history_load(1,&h)==ESP_OK&&h.count==1&&!strcmp(h.entries[0].ssid,"Original"));
    assert(net_history_save(1,&h,&legacy)==ESP_OK&&stored&&writes==1);
    assert(net_history_save(1,&h,&legacy)==ESP_OK&&writes==1); // unchanged reconnect avoids flash wear
    assert(net_history_clear(1,&h)==ESP_OK&&h.count==0);
    assert(net_history_load(1,&reloaded)==ESP_OK&&reloaded.count==0); // no legacy resurrection
    for(unsigned i=0;i<10;++i){app_net_credentials_t c=network(i);assert(net_history_save(1,&h,&c)==ESP_OK);}
    assert(h.count==10&&h.next==0);app_net_saved_t list;app_net_history_list(&h,&list);
    assert(list.count==10&&!strcmp(list.ssids[0],"Network-9")&&!strcmp(list.ssids[9],"Network-0"));
    app_net_credentials_t c=network(4);strcpy(c.password,"updated-password");assert(net_history_save(1,&h,&c)==ESP_OK&&h.count==10&&h.next==0&&h.preferred==4);
    c=network(10);app_net_history_t before=h;
    fail_write=true;assert(net_history_save(1,&h,&c)==ESP_FAIL&&!memcmp(&before,&h,sizeof(h)));fail_write=false;
    fail_commit=true;assert(net_history_save(1,&h,&c)==ESP_FAIL&&!memcmp(&before,&h,sizeof(h)));fail_commit=false;
    assert(net_history_load(1,&reloaded)==ESP_OK&&!memcmp(&before,&reloaded,sizeof(h)));
    assert(net_history_save(1,&h,&c)==ESP_OK&&h.next==1&&h.count==10&&h.preferred==0);
    assert(app_net_history_find(&h,"Network-0")==-1&&app_net_history_find(&h,"Network-1")==1);
    c=network(11);assert(net_history_save(1,&h,&c)==ESP_OK&&h.next==2);
    assert(app_net_history_find(&h,"Network-1")==-1);
    assert(net_history_load(1,&reloaded)==ESP_OK&&!memcmp(&h,&reloaded,sizeof(h)));
    c=network(4);assert(!strcmp(h.entries[4].password,"updated-password"));
    strcpy(c.password,"bad");before=h;assert(net_history_save(1,&h,&c)==ESP_ERR_INVALID_ARG&&!memcmp(&before,&h,sizeof(h)));
    fail_write=true;assert(net_history_clear(1,&h)==ESP_FAIL&&!memcmp(&before,&h,sizeof(h)));fail_write=false;
    disk.count=11;assert(net_history_load(1,&reloaded)==ESP_ERR_INVALID_STATE&&reloaded.count==0);
    app_net_history_init(&h);h.count=1;h.next=1;h.entries[0]=network(1);h.preferred=1;assert(!app_net_history_valid(&h));
    puts("Wi-Fi history: 10-slot wrap, duplicate update, preferred network, reload, import, write failures and clear PASS");
}
