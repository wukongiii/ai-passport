#pragma once
#include "score_controller.h"

/* Caller holds the BSP LVGL lock. UI owns one persistent object tree. */
bool score_ui_create(void);
void score_ui_render(const score_session_t *session, const score_controller_t *controller,
                     int battery, const char *status, const char *storage_status);
