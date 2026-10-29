#pragma once
#include "score_controller.h"

/* Caller holds the BSP LVGL lock. UI owns one persistent object tree. */
bool score_ui_create(void *content);
void score_ui_render(const score_session_t *session, const score_controller_t *controller,
                     int content_height, int battery, const char *status, const char *storage_status);
