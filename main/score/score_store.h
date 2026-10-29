#pragma once
#include "score.h"
#include "esp_err.h"
#include "../ui/app_layout.h"

typedef enum { SCORE_STORE_EMPTY, SCORE_STORE_LOADED, SCORE_STORE_INVALID, SCORE_STORE_UNAVAILABLE } score_store_status_t;
/* Application owner only, outside LVGL lock. Never erases NVS on errors. */
score_store_status_t score_store_open(score_session_t *session);
esp_err_t score_store_save(const score_session_t *session);

score_store_status_t score_store_load_rotation(app_rotation_t *rotation);
esp_err_t score_store_save_rotation(app_rotation_t rotation);
