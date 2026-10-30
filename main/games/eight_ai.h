#pragma once
#include "../ai/ai_contract.h"
#include "../score/score.h"
#include "game_preferences.h"
/* Pure game adapter. Owner passes its own session/preferences. */
bool eight_ai_snapshot(const score_session_t *,const game_eight_preferences_t *,uint32_t configuration,ai_game_snapshot_t *);
ai_game_result_t eight_ai_command(const score_session_t *,const ai_tool_call_t *,score_command_t *);
bool eight_ai_result(const score_session_t *,char *,size_t);

const ai_game_definition_t *eight_ai_definition(void);
