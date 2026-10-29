#pragma once
#include "score_controller.h"
#include "../games/game_preferences.h"

/* Caller holds the BSP LVGL lock. UI owns one persistent object tree. */
bool score_ui_create(void *content);
void score_ui_render(const score_session_t *session, const score_controller_t *controller,
                     int content_height, int battery, const char *status, const char *storage_status,
                     const game_eight_preferences_t *preferences, bool player_status, unsigned player, int delta);
