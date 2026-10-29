#pragma once
#include "app_container.h"
bool app_ui_create(void);
/* Owner holds BSP LVGL lock. Only one module widget tree is mounted at a time. */
void app_ui_render(const app_container_t *app, int battery, const char *storage_overlay);
