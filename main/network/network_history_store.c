#include "network_history_store.h"
#include <string.h>
esp_err_t net_history_load(nvs_handle_t store,app_net_history_t *out){
    app_net_history_init(out);app_net_history_t disk={0};size_t size=sizeof(disk);
    esp_err_t e=nvs_get_blob(store,"history",&disk,&size);
    if(e==ESP_OK){
        if(size!=sizeof(disk)||!app_net_history_valid(&disk))e=ESP_ERR_INVALID_STATE;
        else *out=disk;
    }else if(e==ESP_ERR_NVS_NOT_FOUND){
        app_net_credentials_t single={0};size=sizeof(single);
        e=nvs_get_blob(store,"station",&single,&size);
        if(e==ESP_OK){if(size!=sizeof(single)||!app_net_history_remember(out,&single))e=ESP_ERR_INVALID_STATE;}
        else if(e==ESP_ERR_NVS_NOT_FOUND)e=ESP_OK;
        memset(&single,0,sizeof(single));
    }
    memset(&disk,0,sizeof(disk));return e;
}
static esp_err_t persist(nvs_handle_t store,app_net_history_t *history,const app_net_history_t *next){
    esp_err_t e=nvs_set_blob(store,"history",next,sizeof(*next));
    if(e==ESP_OK)e=nvs_commit(store);
    if(e==ESP_OK)*history=*next;
    return e;
}
esp_err_t net_history_save(nvs_handle_t store,app_net_history_t *history,const app_net_credentials_t *c){
    app_net_history_t next=*history;
    if(!app_net_history_remember(&next,c))return ESP_ERR_INVALID_ARG;
    size_t size=0;esp_err_t e=nvs_get_blob(store,"history",NULL,&size);
    if(e==ESP_ERR_NVS_NOT_FOUND || (e==ESP_OK && memcmp(history,&next,sizeof(next))))e=persist(store,history,&next);
    memset(&next,0,sizeof(next));return e;
}
esp_err_t net_history_clear(nvs_handle_t store,app_net_history_t *history){
    app_net_history_t empty;app_net_history_init(&empty);
    /* Store an explicit empty record: the imported station must not reappear. */
    return persist(store,history,&empty);
}
