#pragma once
#include "app_module.h"
#include "lvgl.h"
/* Single mounted launcher. All functions require the LVGL context/lock.
 * Destroy cancels the animation before the parent content is deleted. */
void app_carousel_create(lv_obj_t *parent, const app_module_t *modules, unsigned count, unsigned focus);
void app_carousel_select(unsigned focus);
void app_carousel_destroy(void);
