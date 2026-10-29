#pragma once
#include "lvgl.h"
/* Shared header: left module, centered context, right connectivity and battery.
 * All calls run in the UI owner under the LVGL lock. */
void app_header_create(lv_obj_t *screen);
void app_header_context(const char *text);
void app_header_render(int y,const char *module,int battery,bool home);
