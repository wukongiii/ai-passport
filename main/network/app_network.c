#include "app_network.h"
#include "app_network_socket.h"
#include "cJSON.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <fcntl.h>
#include <string.h>

static const char *TAG="app_network";
typedef enum { CMD_PORTAL_ON,CMD_PORTAL_OFF,CMD_FORGET,CMD_RETRY,CMD_CONNECT,CMD_GOT_IP,CMD_DISCONNECTED,CMD_SCAN,CMD_SCAN_DONE,CMD_ASSOCIATED,CMD_CONNECT_DEVICE } command_type_t;
typedef struct {command_type_t type;app_net_credentials_t credentials;unsigned reason;uint32_t ip;} command_t;
static QueueHandle_t commands;
static portMUX_TYPE lock=portMUX_INITIALIZER_UNLOCKED;
static app_network_snapshot_t view;
static app_preferences_t preferences,pending_preferences;
static bool preferences_pending;
static httpd_handle_t server;
static int dns_socket=-1;
static esp_netif_t *station,*access_point;
static esp_event_handler_instance_t wifi_handler,ip_handler;
static bool handler_wifi,handler_ip,wifi_initialized,wifi_started;
static nvs_handle_t store;
static bool store_ready;
static app_net_credentials_t saved,candidate;
static bool candidate_new,waiting_disconnect;
static bool accepting_connect;
static app_net_scan_t scan_view;
static bool scan_active;
static int64_t scan_deadline;
static unsigned attempts;
static int64_t portal_deadline,connect_deadline,retry_at;
static char session_token[33];
extern const unsigned char portal_start[] asm("_binary_portal_html_start");
extern const unsigned char portal_end[] asm("_binary_portal_html_end");

void app_network_snapshot(app_network_snapshot_t *out){portENTER_CRITICAL(&lock);*out=view;portEXIT_CRITICAL(&lock);}
static void publish(app_network_snapshot_t *s){portENTER_CRITICAL(&lock);s->revision=view.revision+1;view=*s;portEXIT_CRITICAL(&lock);}
void app_network_publish_preferences(const app_preferences_t *p){portENTER_CRITICAL(&lock);preferences=*p;portEXIT_CRITICAL(&lock);}
bool app_network_take_preferences(app_preferences_t *p){
    portENTER_CRITICAL(&lock);bool have=preferences_pending;
    if(have){*p=pending_preferences;preferences_pending=false;}portEXIT_CRITICAL(&lock);return have;
}
static bool enqueue(command_type_t type){command_t c={.type=type};return commands && xQueueSend(commands,&c,0)==pdTRUE;}
bool app_network_portal(bool enable){return enqueue(enable ? CMD_PORTAL_ON : CMD_PORTAL_OFF);}
bool app_network_forget(void){return enqueue(CMD_FORGET);}
bool app_network_retry(void){return enqueue(CMD_RETRY);}
void app_network_scan_snapshot(app_net_scan_t *out){portENTER_CRITICAL(&lock);*out=scan_view;portEXIT_CRITICAL(&lock);}
bool app_network_scan(void){
    portENTER_CRITICAL(&lock);
    bool allowed=view.ready && !view.busy && !scan_view.scanning && !accepting_connect;
    if(allowed){scan_view.scanning=true;scan_view.error=0;++scan_view.revision;}
    portEXIT_CRITICAL(&lock);
    if(!allowed)return false;
    if(enqueue(CMD_SCAN))return true;
    portENTER_CRITICAL(&lock);scan_view.scanning=false;scan_view.error=ESP_ERR_NO_MEM;++scan_view.revision;portEXIT_CRITICAL(&lock);return false;
}
bool app_network_connect(const app_net_credentials_t *credentials){
    if(!app_net_credentials_valid(credentials))return false;
    command_t c={.type=CMD_CONNECT_DEVICE,.credentials=*credentials};
    portENTER_CRITICAL(&lock);bool ok=view.ready && !view.busy && !accepting_connect;
    if(ok)accepting_connect=true;
    portEXIT_CRITICAL(&lock);
    if(ok && xQueueSend(commands,&c,0)!=pdTRUE){portENTER_CRITICAL(&lock);accepting_connect=false;portEXIT_CRITICAL(&lock);ok=false;}
    memset(&c,0,sizeof(c));return ok;
}
static void network_event(void *arg,esp_event_base_t base,int32_t id,void *data){
    (void)arg;command_t c={0};
    if(base==IP_EVENT && id==IP_EVENT_STA_GOT_IP){c.type=CMD_GOT_IP;c.ip=((ip_event_got_ip_t *)data)->ip_info.ip.addr;}
    else if(base==WIFI_EVENT && id==WIFI_EVENT_STA_CONNECTED){c.type=CMD_ASSOCIATED;}
    else if(base==WIFI_EVENT && id==WIFI_EVENT_STA_DISCONNECTED){c.type=CMD_DISCONNECTED;c.reason=((wifi_event_sta_disconnected_t *)data)->reason;}
    else if(base==WIFI_EVENT && id==WIFI_EVENT_SCAN_DONE){c.type=CMD_SCAN_DONE;c.reason=((wifi_event_sta_scan_done_t *)data)->status;}
    else return;
    if(xQueueSend(commands,&c,0)!=pdTRUE)ESP_LOGW(TAG,"Network event queue full; connection timeout will recover");
}
/* Restrict management to sockets addressed to the physical setup AP, even when
 * AP and station are both active. Never expose the setup server on the LAN. */
static bool local_request(httpd_req_t *r,bool write){
    app_network_snapshot_t s;app_network_snapshot(&s);if(!s.portal)return false;
    struct sockaddr_storage address={0};socklen_t length=sizeof(address);
    if(getsockname(httpd_req_to_sockfd(r),(struct sockaddr *)&address,&length)!=0 ||
       !app_net_address_is_setup(&address,length))return false;
    char host[48];
    if(strncmp(r->uri,"/api/",5)==0 && (httpd_req_get_hdr_value_str(r,"Host",host,sizeof(host))!=ESP_OK ||
        (strcmp(host,"192.168.4.1") && strcmp(host,"192.168.4.1:80"))))return false;
    char origin[64];size_t n=httpd_req_get_hdr_value_len(r,"Origin");
    if(n && (n>=sizeof(origin) || httpd_req_get_hdr_value_str(r,"Origin",origin,sizeof(origin))!=ESP_OK || strcmp(origin,"http://192.168.4.1")))return false;
    if(write){char token[40];if(httpd_req_get_hdr_value_str(r,"X-App-Token",token,sizeof(token))!=ESP_OK || strcmp(token,session_token))return false;}
    return true;
}
static esp_err_t deny(httpd_req_t *r){return httpd_resp_send_err(r,HTTPD_403_FORBIDDEN,"Open setup from the device hotspot");}
static void headers(httpd_req_t *r){
    httpd_resp_set_hdr(r,"Cache-Control","no-store");httpd_resp_set_hdr(r,"X-Content-Type-Options","nosniff");
    httpd_resp_set_hdr(r,"X-Frame-Options","DENY");
}
static esp_err_t redirect(httpd_req_t *r,httpd_err_code_t error);
static esp_err_t root(httpd_req_t *r){
    if(!local_request(r,false))return deny(r);
    char host[48];
    if(httpd_req_get_hdr_value_str(r,"Host",host,sizeof(host))!=ESP_OK ||
       (strcmp(host,"192.168.4.1") && strcmp(host,"192.168.4.1:80")))return redirect(r,HTTPD_404_NOT_FOUND);
    headers(r);httpd_resp_set_type(r,"text/html; charset=utf-8");
    httpd_resp_set_hdr(r,"Content-Security-Policy","default-src 'self'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'");
    return httpd_resp_send(r,(const char *)portal_start,portal_end-portal_start);
}
static esp_err_t status(httpd_req_t *r){
    if(!local_request(r,false))return deny(r);
    app_network_snapshot_t s;app_network_snapshot(&s);app_preferences_t p;
    portENTER_CRITICAL(&lock);p=preferences;portEXIT_CRITICAL(&lock);
    cJSON *body=cJSON_CreateObject();if(!body)return ESP_ERR_NO_MEM;
    cJSON_AddNumberToObject(body,"state",s.state);cJSON_AddBoolToObject(body,"saved",s.saved);
    cJSON_AddBoolToObject(body,"associated",s.associated);cJSON_AddNumberToObject(body,"reason",s.reason);
    cJSON_AddNumberToObject(body,"seconds",s.seconds_left);cJSON_AddStringToObject(body,"ip",s.ip);
    cJSON_AddStringToObject(body,"ssid",s.ssid);cJSON_AddStringToObject(body,"token",session_token);
    cJSON_AddNumberToObject(body,"rotation",p.rotation);cJSON_AddNumberToObject(body,"brightness",p.brightness);
    cJSON_AddNumberToObject(body,"dim",p.dim_seconds);cJSON_AddNumberToObject(body,"blank",p.blank_seconds);
    cJSON_AddNumberToObject(body,"error",s.error);cJSON_AddNumberToObject(body,"save_error",s.save_error);
    cJSON_AddNumberToObject(body,"issue",app_net_issue(s.reason));cJSON_AddNumberToObject(body,"attempt",s.attempt);
    cJSON_AddNumberToObject(body,"attempt_seconds",s.attempt_seconds);
    char *json=cJSON_PrintUnformatted(body);cJSON_Delete(body);if(!json)return ESP_ERR_NO_MEM;
    headers(r);httpd_resp_set_type(r,"application/json");esp_err_t e=httpd_resp_send(r,json,HTTPD_RESP_USE_STRLEN);cJSON_free(json);return e;
}
static esp_err_t networks(httpd_req_t *r){
    if(!local_request(r,false))return deny(r);
    app_net_scan_t snapshot;
    portENTER_CRITICAL(&lock);snapshot=scan_view;portEXIT_CRITICAL(&lock);
    cJSON *body=cJSON_CreateObject();if(!body)return ESP_ERR_NO_MEM;
    cJSON_AddBoolToObject(body,"scanning",snapshot.scanning);
    cJSON_AddBoolToObject(body,"attempted",snapshot.attempted);
    cJSON_AddNumberToObject(body,"revision",snapshot.revision);
    cJSON_AddNumberToObject(body,"error",snapshot.error);
    cJSON *list=cJSON_AddArrayToObject(body,"networks");
    if(!list){cJSON_Delete(body);return ESP_ERR_NO_MEM;}
    for(unsigned i=0;i<snapshot.count;++i){
        const app_net_access_point_t *ap=&snapshot.entries[i];cJSON *item=cJSON_CreateObject();
        if(!item){cJSON_Delete(body);return ESP_ERR_NO_MEM;}
        cJSON_AddStringToObject(item,"ssid",ap->ssid);cJSON_AddNumberToObject(item,"rssi",ap->rssi);
        cJSON_AddBoolToObject(item,"secured",ap->secured);cJSON_AddItemToArray(list,item);
    }
    char *json=cJSON_PrintUnformatted(body);cJSON_Delete(body);if(!json)return ESP_ERR_NO_MEM;
    headers(r);httpd_resp_set_type(r,"application/json");
    esp_err_t e=httpd_resp_send(r,json,HTTPD_RESP_USE_STRLEN);cJSON_free(json);return e;
}
static esp_err_t request_scan(httpd_req_t *r){
    if(!local_request(r,true))return deny(r);
    if(!app_network_scan())return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"Connection or scan busy");
    headers(r);httpd_resp_set_status(r,"202 Accepted");return httpd_resp_send(r,"Accepted",HTTPD_RESP_USE_STRLEN);
}
static esp_err_t submit(httpd_req_t *r){
    if(!local_request(r,true))return deny(r);
    if(!r->content_len || r->content_len>APP_NET_FORM_MAX)return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"Invalid request size");
    char form[APP_NET_FORM_MAX+1]={0};unsigned received=0;
    while(received<r->content_len){int n=httpd_req_recv(r,form+received,r->content_len-received);
        if(n<=0){memset(form,0,sizeof(form));return ESP_FAIL;}received+=(unsigned)n;}
    // Reject a hidden tail after a NUL, rather than validating a shorter prefix.
    if(memchr(form,0,received)){memset(form,0,sizeof(form));return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"Invalid form");}
    bool ok=false;
    if(!strcmp(r->uri,"/api/wifi")){
        command_t c={.type=CMD_CONNECT};ok=app_net_parse_credentials(form,&c.credentials);
        if(ok){portENTER_CRITICAL(&lock);ok=!accepting_connect && !view.busy;if(ok)accepting_connect=true;portEXIT_CRITICAL(&lock);
            if(ok && xQueueSend(commands,&c,0)!=pdTRUE){portENTER_CRITICAL(&lock);accepting_connect=false;portEXIT_CRITICAL(&lock);ok=false;}}
        memset(&c,0,sizeof(c));
    }else{
        app_preferences_t p;ok=app_net_parse_preferences(form,&p);
        if(ok){portENTER_CRITICAL(&lock);ok=!preferences_pending;if(ok){pending_preferences=p;preferences_pending=true;}portEXIT_CRITICAL(&lock);}
    }
    memset(form,0,sizeof(form));headers(r);
    if(!ok)return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"Invalid values or device busy; retry shortly");
    httpd_resp_set_status(r,"202 Accepted");return httpd_resp_send(r,"Accepted",HTTPD_RESP_USE_STRLEN);
}
static esp_err_t redirect(httpd_req_t *r,httpd_err_code_t error){
    (void)error;if(!local_request(r,false))return deny(r);
    headers(r);httpd_resp_set_status(r,"303 See Other");
    httpd_resp_set_hdr(r,"Location","http://192.168.4.1/");
    return httpd_resp_send(r,"Open http://192.168.4.1/",HTTPD_RESP_USE_STRLEN);
}
static esp_err_t start_dns(void){
    dns_socket=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);if(dns_socket<0)return ESP_FAIL;
    struct sockaddr_in a={.sin_family=AF_INET,.sin_port=htons(53),.sin_addr.s_addr=inet_addr("192.168.4.1")};
    if(bind(dns_socket,(struct sockaddr *)&a,sizeof(a)) || fcntl(dns_socket,F_SETFL,O_NONBLOCK)<0){close(dns_socket);dns_socket=-1;return ESP_FAIL;}
    return ESP_OK;
}
static void poll_dns(void){
    if(dns_socket<0)return;
    uint8_t query[480],answer[496];struct sockaddr_in peer;socklen_t size=sizeof(peer);
    int n=recvfrom(dns_socket,query,sizeof(query),0,(struct sockaddr *)&peer,&size);
    if(n<=0)return;
    size_t count=app_net_dns_response(query,(size_t)n,answer,sizeof(answer));
    if(count)sendto(dns_socket,answer,count,0,(struct sockaddr *)&peer,size);
}
static esp_err_t start_http(void){
    httpd_config_t cfg=HTTPD_DEFAULT_CONFIG();cfg.stack_size=6144;cfg.max_open_sockets=3;
    cfg.lru_purge_enable=true;cfg.recv_wait_timeout=3;cfg.send_wait_timeout=3;
    esp_err_t e=httpd_start(&server,&cfg);if(e!=ESP_OK)return e;
    const httpd_uri_t routes[]={
        {.uri="/",.method=HTTP_GET,.handler=root}, {.uri="/api/status",.method=HTTP_GET,.handler=status},
        {.uri="/api/wifi",.method=HTTP_POST,.handler=submit}, {.uri="/api/settings",.method=HTTP_POST,.handler=submit},
        {.uri="/api/networks",.method=HTTP_GET,.handler=networks}, {.uri="/api/scan",.method=HTTP_POST,.handler=request_scan}};
    for(unsigned i=0;i<sizeof(routes)/sizeof(routes[0]);++i){e=httpd_register_uri_handler(server,&routes[i]);if(e!=ESP_OK){httpd_stop(server);server=NULL;return e;}}
    e=httpd_register_err_handler(server,HTTPD_404_NOT_FOUND,redirect);
    if(e!=ESP_OK){httpd_stop(server);server=NULL;}
    return e;
}
static void finish_scan(esp_err_t error,bool collect){
    app_net_scan_t result={.attempted=true,.error=error};
    if(collect && error==ESP_OK){
        uint16_t count=0;result.error=esp_wifi_scan_get_ap_num(&count);
        for(unsigned i=0;result.error==ESP_OK && i<count;++i){
            wifi_ap_record_t record={0};result.error=esp_wifi_scan_get_ap_record(&record);
            if(result.error!=ESP_OK)break;
            app_net_access_point_t ap={.rssi=record.rssi,.secured=record.authmode!=WIFI_AUTH_OPEN};
            memcpy(ap.ssid,record.ssid,32);app_net_scan_add(&result,&ap);
        }
    }
    esp_wifi_clear_ap_list();scan_active=false;scan_deadline=0;
    portENTER_CRITICAL(&lock);result.revision=scan_view.revision+1;scan_view=result;portEXIT_CRITICAL(&lock);
}
static void cancel_scan(void){
    if(scan_active)esp_wifi_scan_stop();
    finish_scan(ESP_OK,false);
}
static void begin_scan(const app_network_snapshot_t *s){
    if(s->busy){finish_scan(ESP_ERR_INVALID_STATE,false);return;}
    portENTER_CRITICAL(&lock);scan_view.scanning=true;++scan_view.revision;portEXIT_CRITICAL(&lock);
    if(!wifi_started){esp_err_t e=esp_wifi_start();if(e!=ESP_OK){finish_scan(e,false);return;}wifi_started=true;}
    wifi_scan_config_t cfg={0};cfg.scan_type=WIFI_SCAN_TYPE_ACTIVE;
    cfg.scan_time.active.min=40;cfg.scan_time.active.max=100;
    esp_err_t e=esp_wifi_scan_start(&cfg,false);
    if(e!=ESP_OK){finish_scan(e,false);return;}
    scan_active=true;scan_deadline=esp_timer_get_time()+15000000;
}
static void stop_portal(app_network_snapshot_t *s){
    // Withdraw authorization first, then wait for handlers before destroying Wi-Fi AP.
    s->portal=false;s->seconds_left=0;publish(s);
    if(server){httpd_stop(server);server=NULL;}
    cancel_scan();
    if(dns_socket>=0){close(dns_socket);dns_socket=-1;}
    memset(session_token,0,sizeof(session_token));
    if(wifi_started){
        esp_err_t e=esp_wifi_set_mode(WIFI_MODE_STA);
        if(e!=ESP_OK)s->error=e;
        if(e!=ESP_OK || s->state==APP_NET_OFFLINE || s->state==APP_NET_FAILED){
            e=esp_wifi_stop();if(e==ESP_OK)wifi_started=false;else s->error=e;
        }
    }
}
static esp_err_t start_portal(app_network_snapshot_t *s){
    if(s->portal){portal_deadline=esp_timer_get_time()+(int64_t)APP_NET_PORTAL_SECONDS*1000000;return ESP_OK;}
    esp_err_t e;
    if(!wifi_started){e=esp_wifi_start();if(e!=ESP_OK)return e;wifi_started=true;}
    // Active Wi-Fi supplies RF entropy for the per-session setup secret.
    uint8_t mac[6];e=esp_read_mac(mac,ESP_MAC_WIFI_SOFTAP);if(e!=ESP_OK)goto rollback;
    snprintf(s->ap_name,sizeof(s->ap_name),"Billiards-%02X%02X",mac[4],mac[5]);
    uint32_t r[4];esp_fill_random(r,sizeof(r));
    for(unsigned i=0;i<4;++i)snprintf(session_token+i*8,9,"%08lx",(unsigned long)r[i]);
    wifi_config_t cfg={0};memcpy(cfg.ap.ssid,s->ap_name,strlen(s->ap_name));cfg.ap.ssid_len=strlen(s->ap_name);
    cfg.ap.channel=1;cfg.ap.max_connection=2;
    cfg.ap.authmode=WIFI_AUTH_OPEN;
    e=esp_wifi_set_mode(WIFI_MODE_APSTA);if(e==ESP_OK)e=esp_wifi_set_config(WIFI_IF_AP,&cfg);
    memset(&cfg,0,sizeof(cfg));if(e!=ESP_OK)goto rollback;
    e=start_http();if(e==ESP_OK)e=start_dns();
    if(e!=ESP_OK)goto rollback;
    portal_deadline=esp_timer_get_time()+(int64_t)APP_NET_PORTAL_SECONDS*1000000;
    s->portal=true;s->seconds_left=APP_NET_PORTAL_SECONDS;return ESP_OK;
rollback:
    stop_portal(s);return e;
}
static esp_err_t apply_connection(void){
    wifi_config_t cfg={0};memcpy(cfg.sta.ssid,candidate.ssid,strlen(candidate.ssid));
    memcpy(cfg.sta.password,candidate.password,strlen(candidate.password));
    cfg.sta.pmf_cfg.capable=true;
    esp_err_t e=esp_wifi_set_config(WIFI_IF_STA,&cfg);memset(&cfg,0,sizeof(cfg));
    if(e==ESP_OK && !wifi_started){e=esp_wifi_start();if(e==ESP_OK)wifi_started=true;}
    if(e==ESP_OK)e=esp_wifi_connect();
    connect_deadline=esp_timer_get_time()+20000000;return e;
}
static esp_err_t connect_to(const app_net_credentials_t *c,bool is_new,app_network_snapshot_t *s){
    candidate=*c;candidate_new=is_new;attempts=0;retry_at=0;s->reason=0;s->error=0;s->ip[0]=0;
    memcpy(s->ssid,c->ssid,sizeof(s->ssid));s->associated=false;s->save_error=0;
    s->attempt=1;s->attempt_seconds=20;s->state=APP_NET_CONNECTING;s->busy=true;
    waiting_disconnect=wifi_started && esp_wifi_disconnect()==ESP_OK;
    if(waiting_disconnect){connect_deadline=esp_timer_get_time()+3000000;return ESP_OK;}
    return apply_connection();
}
static void failed(app_network_snapshot_t *s,esp_err_t e){s->state=APP_NET_FAILED;s->busy=false;s->associated=false;s->attempt_seconds=0;s->error=e;connect_deadline=retry_at=0;}
static void cleanup(void){
    if(server){httpd_stop(server);server=NULL;}
    if(wifi_started){esp_wifi_stop();wifi_started=false;}
    if(handler_wifi){esp_event_handler_instance_unregister(WIFI_EVENT,ESP_EVENT_ANY_ID,wifi_handler);handler_wifi=false;}
    if(handler_ip){esp_event_handler_instance_unregister(IP_EVENT,IP_EVENT_STA_GOT_IP,ip_handler);handler_ip=false;}
    if(wifi_initialized){esp_wifi_deinit();wifi_initialized=false;}
    if(station){esp_netif_destroy_default_wifi(station);station=NULL;}
    if(access_point){esp_netif_destroy_default_wifi(access_point);access_point=NULL;}
    if(store_ready){nvs_close(store);store_ready=false;}
}
static esp_err_t initialize(void){
    esp_err_t e=nvs_flash_init();if(e!=ESP_OK)return e; // Never erase app/user data to recover networking.
    e=nvs_open("net_config",NVS_READWRITE,&store);if(e!=ESP_OK)return e;store_ready=true;
    size_t n=sizeof(saved);e=nvs_get_blob(store,"station",&saved,&n);
    if(e!=ESP_OK || n!=sizeof(saved) || !app_net_credentials_valid(&saved))memset(&saved,0,sizeof(saved));
    e=esp_netif_init();if(e!=ESP_OK)return e;
    e=esp_event_loop_create_default();if(e!=ESP_OK && e!=ESP_ERR_INVALID_STATE)return e;
    esp_netif_config_t sta_cfg=ESP_NETIF_DEFAULT_WIFI_STA(),ap_cfg=ESP_NETIF_DEFAULT_WIFI_AP();
    station=esp_netif_new(&sta_cfg);if(!station)return ESP_ERR_NO_MEM;
    e=esp_netif_attach_wifi_station(station);if(e!=ESP_OK)return e;
    e=esp_wifi_set_default_wifi_sta_handlers();if(e!=ESP_OK)return e;
    access_point=esp_netif_new(&ap_cfg);if(!access_point)return ESP_ERR_NO_MEM;
    e=esp_netif_attach_wifi_ap(access_point);if(e!=ESP_OK)return e;
    e=esp_wifi_set_default_wifi_ap_handlers();if(e!=ESP_OK)return e;
    esp_netif_dns_info_t dns={0};dns.ip.type=ESP_IPADDR_TYPE_V4;dns.ip.u_addr.ip4.addr=inet_addr("192.168.4.1");
    e=esp_netif_set_dns_info(access_point,ESP_NETIF_DNS_MAIN,&dns);if(e!=ESP_OK)return e;
    uint8_t offer_dns=1;
    e=esp_netif_dhcps_option(access_point,ESP_NETIF_OP_SET,ESP_NETIF_DOMAIN_NAME_SERVER,&offer_dns,sizeof(offer_dns));if(e!=ESP_OK)return e;
    wifi_init_config_t cfg=WIFI_INIT_CONFIG_DEFAULT();e=esp_wifi_init(&cfg);if(e!=ESP_OK)return e;wifi_initialized=true;
    e=esp_wifi_set_storage(WIFI_STORAGE_RAM);if(e!=ESP_OK)return e;
    e=esp_wifi_set_mode(WIFI_MODE_STA);if(e!=ESP_OK)return e;
    e=esp_event_handler_instance_register(WIFI_EVENT,ESP_EVENT_ANY_ID,network_event,NULL,&wifi_handler);if(e!=ESP_OK)return e;handler_wifi=true;
    e=esp_event_handler_instance_register(IP_EVENT,IP_EVENT_STA_GOT_IP,network_event,NULL,&ip_handler);if(e!=ESP_OK)return e;handler_ip=true;
    return ESP_OK;
}
static void worker(void *arg){
    (void)arg;app_network_snapshot_t s={0};esp_err_t e=initialize();s.saved=app_net_credentials_valid(&saved);
    if(e!=ESP_OK){s.state=APP_NET_FAILED;s.error=e;cleanup();publish(&s);vTaskDelete(NULL);return;}
    s.ready=true;if(s.saved){e=connect_to(&saved,false,&s);if(e!=ESP_OK)failed(&s,e);}publish(&s);
    ESP_LOGI(TAG,"Network service ready; free heap=%u largest=%u",(unsigned)esp_get_free_heap_size(),(unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    for(;;){
        command_t c={0};bool change=false;
        if(xQueueReceive(commands,&c,pdMS_TO_TICKS(250))==pdTRUE){change=true;e=ESP_OK;
            switch(c.type){
            case CMD_PORTAL_ON:e=start_portal(&s);break;
            case CMD_PORTAL_OFF:stop_portal(&s);break;
            case CMD_CONNECT:
            case CMD_CONNECT_DEVICE:
                if(s.portal || c.type==CMD_CONNECT_DEVICE){cancel_scan();e=connect_to(&c.credentials,true,&s);}
                portENTER_CRITICAL(&lock);accepting_connect=false;portEXIT_CRITICAL(&lock);break;
            case CMD_RETRY:if(s.saved && !s.busy){cancel_scan();e=connect_to(&saved,false,&s);}break;
            case CMD_SCAN:begin_scan(&s);break;
            case CMD_SCAN_DONE:if(scan_active)finish_scan(c.reason ? ESP_FAIL : ESP_OK,true);break;
            case CMD_ASSOCIATED:
                if(s.state==APP_NET_CONNECTING && !waiting_disconnect)s.associated=true;
                break;
            case CMD_FORGET:
                e=nvs_erase_key(store,"station");if(e==ESP_ERR_NVS_NOT_FOUND)e=ESP_OK;
                if(e==ESP_OK)e=nvs_commit(store);
                if(e==ESP_OK){memset(&saved,0,sizeof(saved));memset(&candidate,0,sizeof(candidate));s.saved=false;
                    s.state=APP_NET_OFFLINE;s.busy=false;s.associated=false;s.save_error=0;s.ssid[0]=0;
                    waiting_disconnect=false;connect_deadline=retry_at=0;s.ip[0]=0;esp_wifi_disconnect();}
                break;
            case CMD_GOT_IP:{
                wifi_ap_record_t connected_ap={0};
                if(s.state==APP_NET_CONNECTING && !waiting_disconnect &&
                   esp_wifi_sta_get_ap_info(&connected_ap)==ESP_OK &&
                   !strncmp((const char *)connected_ap.ssid,candidate.ssid,sizeof(connected_ap.ssid))){
                    s.state=APP_NET_CONNECTED;s.busy=false;s.associated=true;s.attempt_seconds=0;s.reason=0;s.error=0;connect_deadline=retry_at=0;attempts=0;
                    struct in_addr ip={.s_addr=c.ip};inet_ntoa_r(ip,s.ip,sizeof(s.ip));
                    if(candidate_new){e=nvs_set_blob(store,"station",&candidate,sizeof(candidate));if(e==ESP_OK)e=nvs_commit(store);
                        s.save_error=e;if(e==ESP_OK){saved=candidate;s.saved=true;candidate_new=false;}}
                    ESP_LOGI(TAG,"Station obtained IP; free heap=%u largest=%u",(unsigned)esp_get_free_heap_size(),(unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
                }break;}
            case CMD_DISCONNECTED:
                if(waiting_disconnect){waiting_disconnect=false;e=apply_connection();}
                else if(!retry_at && (s.state==APP_NET_CONNECTING || s.state==APP_NET_CONNECTED)){
                    s.reason=c.reason;s.associated=false;s.ip[0]=0;connect_deadline=0;
                    if(app_net_schedule_retry(&attempts,esp_timer_get_time(),&retry_at)){s.state=APP_NET_CONNECTING;s.busy=true;}
                    else failed(&s,ESP_FAIL);
                }break;
            }
            memset(&c,0,sizeof(c));if(e!=ESP_OK){s.error=e;if(s.state==APP_NET_CONNECTING)failed(&s,e);}
        }
        int64_t now=esp_timer_get_time();
        if(s.state==APP_NET_CONNECTING){
            unsigned remaining=app_net_remaining_seconds(retry_at ? retry_at : connect_deadline,now);
            unsigned attempt=attempts+1;
            if(remaining!=s.attempt_seconds || attempt!=s.attempt){s.attempt_seconds=remaining;s.attempt=attempt;change=true;}
        }
        if(scan_active && scan_deadline && now>=scan_deadline){esp_wifi_scan_stop();finish_scan(ESP_ERR_TIMEOUT,false);}
        if(s.portal){unsigned left=app_net_remaining_seconds(portal_deadline,now);
            if(!left){stop_portal(&s);change=true;}
            else if(left!=s.seconds_left){s.seconds_left=left;change=true;}}
        if(retry_at && now>=retry_at){retry_at=0;e=apply_connection();if(e!=ESP_OK)failed(&s,e);change=true;}
        if(connect_deadline && now>=connect_deadline){
            if(waiting_disconnect){waiting_disconnect=false;e=apply_connection();if(e!=ESP_OK)failed(&s,e);}
            else{
                // Ignore the disconnect caused by our timeout; retry is scheduled here.
                s.state=APP_NET_FAILED;s.busy=false;connect_deadline=0;esp_wifi_disconnect();
                if(app_net_schedule_retry(&attempts,now,&retry_at)){s.state=APP_NET_CONNECTING;s.busy=true;}
                else failed(&s,ESP_ERR_TIMEOUT);
            }change=true;
        }
        if(s.portal)poll_dns();
        if(change)publish(&s);
    }
}
void app_network_init(void){
    if(commands)return;
    commands=xQueueCreate(6,sizeof(command_t));
    if(!commands || xTaskCreate(worker,"network",6144,NULL,3,NULL)!=pdPASS){
        if(commands){vQueueDelete(commands);commands=NULL;}app_network_snapshot_t s={.error=ESP_ERR_NO_MEM};publish(&s);
    }
}
