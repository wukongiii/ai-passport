#pragma once
#include "app_network_model.h"
#include "nvs.h"
/* Called only by the network worker. Load imports the existing single-network
 * record when no history exists; a persisted empty history also suppresses it. */
esp_err_t net_history_load(nvs_handle_t,app_net_history_t *);
esp_err_t net_history_save(nvs_handle_t,app_net_history_t *,const app_net_credentials_t *);
esp_err_t net_history_clear(nvs_handle_t,app_net_history_t *);
