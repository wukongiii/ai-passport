#pragma once
#include "app_preferences.h"
#include "../ui/app_actions.h"
typedef enum { APP_SETTING_DIRECTION, APP_SETTING_BRIGHTNESS, APP_SETTING_DIM, APP_SETTING_BLANK, APP_SETTING_INFO, APP_SETTING_NETWORK, APP_SETTING_VOLUME, APP_SETTING_DOUBLE, APP_SETTING_SPEECH, APP_SETTING_COUNT } app_setting_item_t;
typedef enum { APP_SETTINGS_DISPLAY, APP_SETTINGS_SOUND, APP_SETTINGS_OPERATION, APP_SETTINGS_ABOUT, APP_SETTINGS_NETWORK, APP_SETTINGS_GROUP_COUNT } app_settings_group_t;
typedef struct { unsigned focus,group; bool submenu,editing; app_preferences_t draft; } app_settings_model_t;
void app_settings_model_enter(app_settings_model_t *m, const app_preferences_t *p);
/* Lists use left/right selection and middle Enter. Value pages are immediately
 * editable; middle confirms and returns, middle hold discards and returns. */
bool app_settings_model_input(app_settings_model_t *m, app_preferences_t *p, unsigned slot, app_gesture_t gesture);
unsigned app_settings_range_index(const app_settings_model_t *m, const app_preferences_t *p);
