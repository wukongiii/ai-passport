#pragma once
#include "app_preferences.h"
#include "../ui/app_actions.h"
typedef enum { APP_SETTING_DIRECTION, APP_SETTING_BRIGHTNESS, APP_SETTING_DIM, APP_SETTING_BLANK, APP_SETTING_INFO, APP_SETTING_NETWORK, APP_SETTING_VOLUME, APP_SETTING_COUNT } app_setting_item_t;
typedef struct { unsigned focus; bool editing; app_preferences_t draft; } app_settings_model_t;
void app_settings_model_enter(app_settings_model_t *m, const app_preferences_t *p);
/* short left/right navigate or adjust; short middle edit/commit, long middle cancel. */
bool app_settings_model_input(app_settings_model_t *m, app_preferences_t *p, unsigned slot, app_gesture_t gesture);

unsigned app_settings_range_index(const app_settings_model_t *m, const app_preferences_t *p);
