#pragma once
#include "../app/app_module.h"
extern const app_module_t app_module_eight;

/* Owner startup, outside LVGL lock. Keeps old match and preferences compatible. */
void eight_module_init(void);
