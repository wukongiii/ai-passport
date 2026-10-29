#pragma once
#include "app_module.h"
#include "app_preferences.h"
extern const app_module_t app_module_settings;
const app_preferences_t *app_settings_preferences(void);
uint8_t app_settings_preview_brightness(void);
const char *app_settings_storage_status(void);
/* Owner updates snapshots outside LVGL; rendering only consumes the snapshot. */
void app_settings_device_info(int battery, int millivolts, const char *version);
