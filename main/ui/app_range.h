#pragma once
#include "lvgl.h"
/* Shared button-operated range. No touch/events/timers, caller holds LVGL lock.
 * Index follows selectable stops; the final stop may represent infinity. */
typedef struct { lv_obj_t *root, *value, *rail, *fill, *knob, *minimum, *maximum; int width; } app_range_t;
void app_range_create(app_range_t *range, lv_obj_t *parent, int x, int y, int width, const lv_font_t *font);
void app_range_render(app_range_t *range, unsigned index, unsigned maximum,
                      const char *value, const char *minimum, const char *max_label, bool editing);
