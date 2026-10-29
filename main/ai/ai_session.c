#include "ai_session.h"
#include <limits.h>
#include <string.h>
void ai_session_cancel(ai_session_t *s){if(s)s->pending=false;}
ai_flow_result_t ai_session_begin(ai_session_t *s,ai_game_target_t target,int64_t now,
    ai_game_snapshot_t *game,uint32_t *token){
    if(!s||!token||!game||now<0||now>INT64_MAX-30000000)return AI_FLOW_INVALID;
    if(s->pending&&now<s->deadline)return AI_FLOW_BUSY;
    s->pending=false;
    if(!target.ops||!target.ops->snapshot||!target.ops->stamp||!target.ops->execute||!target.module_id)return AI_FLOW_UNAVAILABLE;
    if(s->sequence==UINT32_MAX)return AI_FLOW_UNAVAILABLE;
    memset(game,0,sizeof(*game));if(!target.ops->snapshot(target.context,game))return AI_FLOW_UNAVAILABLE;
    s->target=target;s->revision=game->revision;s->configuration=game->configuration;s->token=++s->sequence;
    s->deadline=now+30000000;s->pending=true;*token=s->token;return AI_FLOW_OK;
}
ai_flow_result_t ai_session_finish(ai_session_t *s,ai_game_target_t target,uint32_t token,int64_t now,
    const ai_reply_t *reply,ai_completion_t *out){
    if(!s||!out||!reply||!token||now<0)return AI_FLOW_INVALID;
    *out=(ai_completion_t){.game_result=AI_GAME_INVALID};
    if(!s->pending||token!=s->token)return AI_FLOW_DUPLICATE;
    s->pending=false; // Every matching response is consumed exactly once, even on rejection.
    if(now>=s->deadline)return AI_FLOW_EXPIRED;
    if(target.ops!=s->target.ops||target.context!=s->target.context||target.module_id!=s->target.module_id||
       target.activation!=s->target.activation)return AI_FLOW_STALE;
    uint32_t revision,configuration;
    if(!target.ops->stamp(target.context,&revision,&configuration))return AI_FLOW_UNAVAILABLE;
    if(revision!=s->revision||configuration!=s->configuration)return AI_FLOW_STALE;
    out->kind=reply->kind;
    if(reply->kind==AI_REPLY_TEXT||reply->kind==AI_REPLY_CLARIFY){
        if(!memchr(reply->text,0,sizeof(reply->text))||!*reply->text)return AI_FLOW_INVALID;
        memcpy(out->text,reply->text,sizeof(out->text));out->game_result=AI_GAME_OK;return AI_FLOW_OK;
    }
    if(reply->kind!=AI_REPLY_TOOL||!memchr(reply->call.name,0,sizeof(reply->call.name))||
       !memchr(reply->call.arguments,0,sizeof(reply->call.arguments)))return AI_FLOW_INVALID;
    out->game_result=target.ops->execute(target.context,s->revision,s->configuration,&reply->call,out->text,sizeof(out->text));
    out->executed=out->game_result==AI_GAME_OK;
    if(out->executed && target.ops->stamp(target.context,&revision,&configuration))out->changed=revision!=s->revision;
    return AI_FLOW_OK;
}
