#pragma once
#include "lvgl.h"

#define APP_SPLASH_TITLE "逗宝台球记分器"
enum { APP_SPLASH_DURATION_MS = 2000 };

/* Caller holds the BSP LVGL lock. No timers or callbacks are retained.
 * Load another screen before deleting the returned screen. */
lv_obj_t *app_splash_create(void);
