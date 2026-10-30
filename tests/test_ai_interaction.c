#include "ai/ai_session.h"
#include "ai/ai_persona.h"
#include "ai/ai_provider.h"
#include "ai/ai_json.h"
#include "app/app_container.h"
#include "games/eight_module.h"
#include "games/eight_ai.h"
#include "score/score_store.h"
#include "score/score_ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static game_preferences_snapshot_t names={.revision=1};
static score_session_t saved;
static unsigned saves;
void game_preferences_snapshot(game_preferences_snapshot_t *out){*out=names;}
score_store_status_t score_store_open(score_session_t *out){score_session_init(out);return SCORE_STORE_EMPTY;}
esp_err_t score_store_save(const score_session_t *s){saved=*s;++saves;return ESP_OK;}
bool score_ui_create(void *c){(void)c;return true;}
void score_ui_render(const score_session_t *s,const score_controller_t *ui,int h,int b,const char *status,const char *storage,
    const game_eight_preferences_t *p,bool player_status,unsigned player,int delta){
    (void)s;(void)ui;(void)h;(void)b;(void)status;(void)storage;(void)p;(void)player_status;(void)player;(void)delta;
}
static ai_json_t workspace;
static ai_game_snapshot_t snapshot;
static char body[AI_BODY_CAPACITY];
static ai_reply_t reply;
static const ai_model_adapter_t *adapter;
static ai_session_t session;
static app_container_t app;
static ai_completion_t completion;
static uint32_t token;
static const char *tool_response(const char *name,const char *args){
    static char buffer[2048];ai_json_writer_t w;ai_json_writer(&w,buffer,sizeof(buffer));
    ai_json_write(&w,"{\"choices\":[{\"finish_reason\":\"tool_calls\",\"message\":{\"role\":\"assistant\",\"content\":\"not executed yet\",\"tool_calls\":[{\"type\":\"function\",\"function\":{\"name\":");
    ai_json_quote(&w,name);ai_json_write(&w,",\"arguments\":");ai_json_quote(&w,args);ai_json_write(&w,"}}]}}]}");assert(w.ok);return buffer;
}
static void begin(void){
    assert(ai_session_begin(&session,app_container_ai_target(&app),100,&snapshot,&token)==AI_FLOW_OK);
    ai_model_request_t r={"ep-test","张三加一分",&snapshot,NULL,NULL};
    assert(adapter->encode(&workspace,&r,body,sizeof(body)));
}
static ai_flow_result_t finish(const char *name,const char *args){
    const char *json=tool_response(name,args);
    assert(adapter->decode(&workspace,json,strlen(json),&reply));
    return ai_session_finish(&session,app_container_ai_target(&app),token,200,&reply,&completion);
}
static void parser_tests(void){
    const char *valid="{\"名字\":\"\\u5f20\\u4e09\\ud83d\\ude00\",\"n\":-9223372036854775808}";
    assert(ai_json_parse(&workspace,valid,strlen(valid)));char text[32];int64_t n;
    assert(ai_json_string(&workspace,ai_json_member(&workspace,0,"名字"),text,sizeof(text))&&!strcmp(text,"张三😀"));
    assert(ai_json_integer(&workspace,ai_json_member(&workspace,0,"n"),&n)&&n==INT64_MIN);
    const char *invalid[]={"{\"a\":1,\"\\u0061\":2}","{\"a\":1,}","[1,]","{\"x\":01}","{\"x\":\"\\u0000\"}","{\"x\":\"\\ud800\"}","{\"x\":\"\xc0\xaf\"}","{} trailing"};
    for(unsigned i=0;i<sizeof(invalid)/sizeof(*invalid);++i)assert(!ai_json_parse(&workspace,invalid[i],strlen(invalid[i])));
    assert(ai_json_parse(&workspace,"1.0",3)&&!ai_json_integer(&workspace,0,&n));
    assert(ai_json_parse(&workspace,"9223372036854775808",19)&&!ai_json_integer(&workspace,0,&n));
    char small[4];ai_json_writer_t w;ai_json_writer(&w,small,sizeof(small));ai_json_quote(&w,"long");assert(!w.ok&&!small[0]);
}
int main(void){
    printf("AI bounded storage: snapshot=%zu parser=%zu session=%zu reply=%zu completion=%zu bytes\n",sizeof(snapshot),sizeof(workspace),sizeof(session),sizeof(reply),sizeof(completion));
    parser_tests();game_preferences_default(&names.eight);strcpy(names.eight.players[0],"张三");strcpy(names.eight.players[1],"李四");
    adapter=ai_model_adapter("compatible");assert(adapter&&!ai_model_adapter("unknown"));
    const app_module_t modules[]={app_module_eight,{2,"Chase","placeholder",APP_ENTRY_GAME,NULL,NULL,APP_ART_CHASE,NULL}};
    app_modules_init(modules,2);assert(app_container_init(&app,modules,2,APP_ROTATION_270));
    assert(ai_session_begin(&session,app_container_ai_target(&app),0,&snapshot,&token)==AI_FLOW_UNAVAILABLE);
    app_container_input(&app,1,APP_GESTURE_SHORT);begin();
    assert(strstr(snapshot.context,"张三")&&strstr(snapshot.context,"rack_wins")&&strstr(snapshot.context,"\"race_to\":10"));
    ai_model_request_t bounded={"ep-test","test",&snapshot,NULL,NULL};char smallbody[32];
    assert(!adapter->encode(&workspace,&bounded,smallbody,sizeof(smallbody)));
    assert(strstr(body,"ep-test")&&strstr(body,"adjust_score")&&strstr(body,"张三加一分"));
    // Audio framing must remain valid JSON, retain tools, and escape clarification.
    char head[AI_BODY_CAPACITY],tail[4096],audio_body[AI_BODY_CAPACITY+4096];
    ai_model_request_t audio={"ep-test","回答上个问题：张三 \"加分\"",&snapshot,NULL,NULL};
    assert(adapter->audio_parts(&workspace,&audio,head,sizeof(head),tail,sizeof(tail)));
    snprintf(audio_body,sizeof(audio_body),"%sUklGRg==%s",head,tail);
    assert(ai_json_parse(&workspace,audio_body,strlen(audio_body)));
    assert(strstr(audio_body,"input_audio")&&strstr(audio_body,"adjust_score")&&strstr(audio_body,"wav"));
    assert(!adapter->audio_parts(&workspace,&audio,head,20,tail,sizeof(tail)));
    assert(!adapter->audio_parts(&workspace,&audio,head,sizeof(head),tail,8));
    const ai_model_adapter_t *generic=ai_model_adapter("compatible");assert(generic);
    assert(generic->encode(&workspace,&bounded,body,sizeof(body))&&!strstr(body,"thinking"));
    assert(generic->audio_parts(&workspace,&audio,head,sizeof(head),tail,sizeof(tail))&&!strstr(tail,"thinking"));
    uint32_t oldtoken=token;
    ai_reply_t ignored={.kind=AI_REPLY_TEXT,.text="ignored"};
    assert(ai_session_finish(&session,app_container_ai_target(&app),token+1,150,&ignored,&completion)==AI_FLOW_DUPLICATE && session.pending);assert(ai_session_begin(&session,app_container_ai_target(&app),101,&snapshot,&token)==AI_FLOW_BUSY);
    assert(finish("adjust_score","{\"player_id\":\"p1\",\"delta\":1,\"expected_revision\":0}")==AI_FLOW_OK);
    assert(completion.executed&&completion.changed&&strstr(completion.text,"[1,0]"));
    assert(ai_session_finish(&session,app_container_ai_target(&app),oldtoken,201,&reply,&completion)==AI_FLOW_DUPLICATE);
    app_container_tick(&app,1000);assert(saves==1&&saved.state.points[0]==1&&saved.history_count==1);
    // Query never changes revision or history.
    begin();assert(finish("get_score","{\"expected_revision\":1}")==AI_FLOW_OK&&completion.executed&&!completion.changed);
    begin();assert(finish("adjust_score","{\"player_id\":\"p1\",\"delta\":0,\"expected_revision\":1}")==AI_FLOW_OK&&completion.game_result==AI_GAME_INVALID&&!completion.executed);
    begin();assert(finish("adjust_score","{\"player_id\":\"张三\",\"delta\":1,\"expected_revision\":1}")==AI_FLOW_OK&&completion.game_result==AI_GAME_INVALID);
    begin();assert(finish("adjust_score","{\"player_id\":\"p1\",\"delta\":1,\"expected_revision\":1,\"extra\":1}")==AI_FLOW_OK&&completion.game_result==AI_GAME_INVALID);
    begin();assert(finish("new_match","{\"expected_revision\":1}")==AI_FLOW_OK&&completion.game_result==AI_GAME_UNSUPPORTED);
    begin();assert(finish("adjust_score","{\"player_id\":\"p1\",\"delta\":1,\"expected_revision\":4294967295}")==AI_FLOW_OK&&completion.game_result==AI_GAME_INVALID);
    // Local command races model result: local score wins and old result is discarded.
    begin();app_container_input(&app,2,APP_GESTURE_SHORT);
    assert(finish("adjust_score","{\"player_id\":\"p1\",\"delta\":1,\"expected_revision\":1}")==AI_FLOW_STALE);
    // Rename races result, even before next application tick.
    begin();strcpy(names.eight.players[0],"王五");++names.revision;
    assert(finish("adjust_score","{\"player_id\":\"p1\",\"delta\":1,\"expected_revision\":2}")==AI_FLOW_STALE);
    // Leave and reenter the same module invalidates the request by activation.
    begin();app_container_input(&app,1,APP_GESTURE_LONG);app_container_input(&app,1,APP_GESTURE_SHORT);
    assert(finish("undo_last","{\"expected_revision\":2}")==AI_FLOW_STALE);
    begin();ai_session_cancel(&session);assert(finish("undo_last","{\"expected_revision\":2}")==AI_FLOW_DUPLICATE);
    begin();const char *raw=tool_response("undo_last","{\"expected_revision\":2}");assert(adapter->decode(&workspace,raw,strlen(raw),&reply));
    assert(ai_session_finish(&session,app_container_ai_target(&app),token,30000100,&reply,&completion)==AI_FLOW_EXPIRED);
    begin();assert(finish("ask_clarification","{\"question\":\"给哪位选手加分？\"}")==AI_FLOW_OK&&!completion.executed&&completion.kind==AI_REPLY_CLARIFY);
    begin();assert(finish("undo_last","{\"expected_revision\":2}")==AI_FLOW_OK&&completion.executed&&strstr(completion.text,"[1,0]"));
    begin();assert(finish("adjust_score","{\"player_id\":\"p1\",\"delta\":-1,\"expected_revision\":3}")==AI_FLOW_OK&&completion.executed&&strstr(completion.text,"[0,0]"));
    begin();assert(finish("adjust_score","{\"player_id\":\"p1\",\"delta\":-1,\"expected_revision\":4}")==AI_FLOW_OK&&completion.game_result==AI_GAME_INVALID);
    // Model format errors never reach gameplay.
    const char *bad[]={"{\"error\":{}}","{\"choices\":[]}","{\"choices\":[{\"finish_reason\":\"length\",\"message\":{\"role\":\"assistant\",\"content\":\"x\"}}]}","{\"choices\":[{\"finish_reason\":\"tool_calls\",\"message\":{\"role\":\"assistant\",\"tool_calls\":[]}}]}"};
    for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);++i)assert(!adapter->decode(&workspace,bad[i],strlen(bad[i]),&reply));
    const char *answer="{\"choices\":[{\"finish_reason\":\"stop\",\"message\":{\"role\":\"assistant\",\"content\":\"你好\"}}]}";
    begin();assert(adapter->decode(&workspace,answer,strlen(answer),&reply));
    assert(ai_session_finish(&session,app_container_ai_target(&app),token,200,&reply,&completion)==AI_FLOW_OK&&!completion.executed&&completion.kind==AI_REPLY_TEXT);
    const char *multi="{\"choices\":[{\"finish_reason\":\"tool_calls\",\"message\":{\"role\":\"assistant\",\"tool_calls\":[{},{}]}}]}";
    assert(!adapter->decode(&workspace,multi,strlen(multi),&reply));
    begin();assert(finish("adjust_score","{\"player_id\":\"p1\",\"delta\":1,\"delta\":-1,\"expected_revision\":4}")==AI_FLOW_OK&&completion.game_result==AI_GAME_INVALID);
    begin();assert(finish("adjust_score","{\"player_id\":\"p1\",\"delta\":1.0,\"expected_revision\":4}")==AI_FLOW_OK&&completion.game_result==AI_GAME_INVALID);
    // Non-board subpages and placeholders do not expose an executable context.
    app_container_input(&app,1,APP_GESTURE_SHORT);
    assert(ai_session_begin(&session,app_container_ai_target(&app),100,&snapshot,&token)==AI_FLOW_UNAVAILABLE);
    app_container_input(&app,1,APP_GESTURE_LONG);app_container_input(&app,1,APP_GESTURE_LONG);
    app_container_input(&app,2,APP_GESTURE_SHORT);app_container_input(&app,1,APP_GESTURE_SHORT);
    assert(!app_container_ai_target(&app).ops);
    // Pure scoring adapter retains rule authority at match end.
    score_session_t end;score_session_init(&end);end.state.points[0]=10;
    score_command_t c;ai_tool_call_t call={.name="adjust_score",.arguments="{\"player_id\":\"p2\",\"delta\":1,\"expected_revision\":0}"};
    assert(eight_ai_command(&end,&call,&c)==AI_GAME_OK&&score_dispatch(&end,&c)==SCORE_OK);
    // Explicit bulk and absolute corrections are one atomic undo entry.
    score_session_t flex;score_session_init(&flex);
    call=(ai_tool_call_t){.name="adjust_score",.arguments="{\"player_id\":\"p1\",\"delta\":3,\"expected_revision\":0}"};
    assert(eight_ai_command(&flex,&call,&c)==AI_GAME_OK&&score_dispatch(&flex,&c)==SCORE_OK&&flex.state.points[0]==3&&flex.history_count==1);
    c=(score_command_t){.type=SCORE_CMD_UNDO,.expected_revision=1};assert(score_dispatch(&flex,&c)==SCORE_OK&&flex.state.points[0]==0);
    call=(ai_tool_call_t){.name="set_scores",.arguments="{\"p1\":5,\"p2\":3,\"expected_revision\":2}"};
    assert(eight_ai_command(&flex,&call,&c)==AI_GAME_OK&&score_dispatch(&flex,&c)==SCORE_OK&&flex.state.points[0]==5&&flex.state.points[1]==3&&flex.history_count==1);
    score_session_t before=flex;
    call=(ai_tool_call_t){.name="set_scores",.arguments="{\"p1\":10,\"p2\":10,\"expected_revision\":3}"};
    assert(eight_ai_command(&flex,&call,&c)==AI_GAME_OK&&score_dispatch(&flex,&c)==SCORE_INVALID&&!memcmp(&before,&flex,sizeof(flex)));
    call=(ai_tool_call_t){.name="set_score",.arguments="{\"player_id\":\"p1\",\"score\":8,\"expected_revision\":3}"};
    assert(eight_ai_command(&flex,&call,&c)==AI_GAME_OK&&score_dispatch(&flex,&c)==SCORE_OK&&flex.state.points[0]==8);
    c=(score_command_t){.type=SCORE_CMD_UNDO,.expected_revision=4};assert(score_dispatch(&flex,&c)==SCORE_OK&&flex.state.points[0]==5&&flex.state.points[1]==3);
    // Configured persona is an independent system message for text and audio.
    assert(eight_ai_snapshot(&flex,&names.eight,1,&snapshot));
    bounded.assistant_name="逗宝";bounded.assistant_prompt="短句回复\n按用户指定数值修改";
    assert(adapter->encode(&workspace,&bounded,body,sizeof(body))&&strstr(body,"逗宝")&&strstr(body,"assistant_name")&&strstr(body,"短句回复"));
    audio.assistant_name=bounded.assistant_name;audio.assistant_prompt=bounded.assistant_prompt;
    assert(adapter->audio_parts(&workspace,&audio,head,sizeof(head),tail,sizeof(tail))&&strstr(head,"逗宝"));
    // Accepted worst-case UTF-8 names, escaped prompts/model IDs and follow-up text fit.
    char maximum_prompt[AI_PERSONA_PROMPT_CAPACITY],maximum_input[AI_INPUT_CAPACITY],maximum_model[97];
    memset(maximum_prompt,'"',sizeof(maximum_prompt)-1);maximum_prompt[sizeof(maximum_prompt)-1]=0;
    memset(maximum_input,'"',sizeof(maximum_input)-1);maximum_input[sizeof(maximum_input)-1]=0;
    memset(maximum_model,'"',sizeof(maximum_model)-1);maximum_model[sizeof(maximum_model)-1]=0;
    strcpy(names.eight.players[0],"一二三四五六七八");strcpy(names.eight.players[1],"二三四五六七八九");
    assert(eight_ai_snapshot(&flex,&names.eight,UINT32_MAX,&snapshot));
    bounded=(ai_model_request_t){maximum_model,maximum_input,&snapshot,names.eight.players[0],maximum_prompt};
    assert(adapter->encode(&workspace,&bounded,body,sizeof(body)));
    assert(adapter->audio_parts(&workspace,&bounded,head,sizeof(head),tail,sizeof(tail)));
    printf("Maximum escaped persona request: head=%zu tail=%zu body_limit=%u bytes\n",strlen(head),strlen(tail),AI_BODY_CAPACITY);
    // Invalid absolute and negative corrections cannot leak partial state.
    before=flex;c=(score_command_t){.type=SCORE_CMD_SET_POINTS,.player=1,.amount=11,.expected_revision=flex.revision};
    assert(score_dispatch(&flex,&c)==SCORE_INVALID&&!memcmp(&before,&flex,sizeof(flex)));
    c=(score_command_t){.type=SCORE_CMD_ADJUST_POINTS,.player=0,.amount=-3,.expected_revision=flex.revision};
    assert(score_dispatch(&flex,&c)==SCORE_OK&&flex.state.points[0]==2);
    puts("AI provider wire format, game capabilities, real scoring/undo, identity, stale/cancel/replay and bounded JSON: PASS");
}
