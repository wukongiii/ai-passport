#pragma once
#include "app_preferences.h"
#include "esp_err.h"
typedef enum { APP_PREF_DEFAULT, APP_PREF_LOADED, APP_PREF_INVALID, APP_PREF_UNAVAILABLE, APP_PREF_MIGRATED } app_pref_store_status_t;
/* Owner only, no UI lock. Never erases NVS. Legacy orientation remains intact. */
app_pref_store_status_t app_preferences_load(app_preferences_t *p);
esp_err_t app_preferences_save(const app_preferences_t *p);
