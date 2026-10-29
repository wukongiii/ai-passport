#pragma once
#include "app_network_model.h"
typedef enum { APP_NET_OFFLINE,APP_NET_CONNECTING,APP_NET_CONNECTED,APP_NET_FAILED } app_net_state_t;
typedef struct {
    app_net_state_t state;
    bool ready,portal,saved,busy,associated;
    unsigned revision,reason,seconds_left,attempt,attempt_seconds;
    char ap_name[32],ip[16],ssid[33];
    int error,save_error;
} app_network_snapshot_t;
/* All requests are bounded queue operations. Only the service task owns Wi-Fi/NVS. */
void app_network_init(void);
void app_network_snapshot(app_network_snapshot_t *);
bool app_network_portal(bool enable);
bool app_network_forget(void);
bool app_network_retry(void);
bool app_network_scan(void);
void app_network_scan_snapshot(app_net_scan_t *);
bool app_network_connect(const app_net_credentials_t *);
/* UI owner publishes / applies settings; HTTP task never touches LVGL or NVS settings. */
void app_network_publish_preferences(const app_preferences_t *);
bool app_network_take_preferences(app_preferences_t *);
