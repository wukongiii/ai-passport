#pragma once
#include "ai_session.h"
#include "../ui/app_actions.h"
typedef enum { GAME_VOICE_IDLE,GAME_VOICE_STARTING,GAME_VOICE_RECORDING,GAME_VOICE_WAITING,GAME_VOICE_REPLY,GAME_VOICE_ERROR,GAME_VOICE_CANCELLING } game_voice_state_t;
typedef struct { game_voice_state_t state;unsigned revision,milliseconds;int error;char text[AI_TEXT_CAPACITY]; } game_voice_snapshot_t;
bool app_game_voice_start(void);
/* Owner-task controller; bindings may run during the owner's LVGL render.
 * Worker publishes copies only and never accesses game operations or LVGL. */
void app_game_voice_bindings(ai_game_target_t,app_bindings_t *);
bool app_game_voice_input(ai_game_target_t,unsigned slot,app_gesture_t);
bool app_game_voice_tick(ai_game_target_t,int64_t now);
void app_game_voice_snapshot(game_voice_snapshot_t *);
void app_game_voice_cancel(void);
