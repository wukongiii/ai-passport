#include "ai_provider.h"
#include "ai_json.h"
#include <string.h>
static bool encode(ai_json_t *workspace,const ai_model_request_t *r,char *out,size_t capacity){
    if(!workspace||!r||!r->game||!r->model||!r->input||!r->game->instructions||!out||!capacity)return false;
    if(!*r->model||strlen(r->model)>96||!*r->input||strlen(r->input)>=AI_INPUT_CAPACITY)return false;
    ai_json_t *j=workspace;
    if(!ai_json_parse(j,r->game->context,strlen(r->game->context))||j->nodes[0].type!=AJ_OBJECT)return false;
    if(!ai_json_parse(j,r->game->tools,strlen(r->game->tools))||j->nodes[0].type!=AJ_ARRAY)return false;
    ai_json_writer_t w;ai_json_writer(&w,out,capacity);
    ai_json_write(&w,"{\"model\":");ai_json_quote(&w,r->model);
    ai_json_write(&w,",\"messages\":[{\"role\":\"system\",\"content\":");
    ai_json_quote(&w,r->game->instructions);
    ai_json_write(&w,"},{\"role\":\"system\",\"content\":");ai_json_quote(&w,r->game->context);
    ai_json_write(&w,"},{\"role\":\"user\",\"content\":");ai_json_quote(&w,r->input);
    ai_json_write(&w,"}],\"tools\":[");
    for(int tool=1,index=0;tool<j->nodes[0].next;tool=j->nodes[tool].next,++index){
        const char *const fields[]={"name","description","parameters"};
        if(!ai_json_fields(j,tool,fields,3))return false;
        if(index)ai_json_write(&w,",");
        ai_json_write(&w,"{\"type\":\"function\",\"function\":");
        size_t length=j->nodes[tool].end-j->nodes[tool].start;
        if(!w.ok || length>=w.size-w.used)return false;
        memcpy(w.data+w.used,j->text+j->nodes[tool].start,length);w.used+=length;w.data[w.used]=0;
        ai_json_write(&w,"}");
    }
    ai_json_write(&w,"]");
    ai_json_write(&w,",\"tool_choice\":\"auto\",\"parallel_tool_calls\":false,\"thinking\":{\"type\":\"disabled\"},\"temperature\":0,\"max_tokens\":256,\"stream\":false}");
    return w.ok && ai_json_parse(j,out,w.used);
}
static bool decode(ai_json_t *workspace,const char *body,size_t size,ai_reply_t *out){
    if(!workspace||!out||size>AI_RESPONSE_CAPACITY)return false;
    ai_json_t *j=workspace;if(!ai_json_parse(j,body,size))return false;
    if(ai_json_member(j,0,"error")>=0)return false;
    int choices=ai_json_member(j,0,"choices");
    if(choices<0||j->nodes[choices].type!=AJ_ARRAY||j->nodes[choices].count!=1)return false;
    int choice=choices+1,message=ai_json_member(j,choice,"message");char finish[24],role[16];
    if(!ai_json_string(j,ai_json_member(j,choice,"finish_reason"),finish,sizeof(finish))||
       !ai_json_string(j,ai_json_member(j,message,"role"),role,sizeof(role))||strcmp(role,"assistant"))return false;
    ai_reply_t reply={0};int calls=ai_json_member(j,message,"tool_calls"),content=ai_json_member(j,message,"content");
    if(!strcmp(finish,"tool_calls")){
        if(calls<0||j->nodes[calls].type!=AJ_ARRAY||j->nodes[calls].count!=1)return false;
        int call=calls+1,function=ai_json_member(j,call,"function");char type[16];
        if(!ai_json_string(j,ai_json_member(j,call,"type"),type,sizeof(type))||strcmp(type,"function")||
           !ai_json_string(j,ai_json_member(j,function,"name"),reply.call.name,sizeof(reply.call.name))||
           !ai_json_string(j,ai_json_member(j,function,"arguments"),reply.call.arguments,sizeof(reply.call.arguments)))return false;
        /* Discard accompanying prose: it may claim an action already succeeded. */
        if(!strcmp(reply.call.name,"ask_clarification")){
            const char *const fields[]={"question"};
            if(!ai_json_parse(j,reply.call.arguments,strlen(reply.call.arguments))||
               !ai_json_fields(j,0,fields,1)||!ai_json_string(j,ai_json_member(j,0,"question"),reply.text,sizeof(reply.text))||!*reply.text)return false;
            reply.kind=AI_REPLY_CLARIFY;memset(&reply.call,0,sizeof(reply.call));
        }else reply.kind=AI_REPLY_TOOL;
    }else if(!strcmp(finish,"stop")){
        if(calls>=0&&!(j->nodes[calls].type==AJ_NULL||(j->nodes[calls].type==AJ_ARRAY&&!j->nodes[calls].count)))return false;
        if(!ai_json_string(j,content,reply.text,sizeof(reply.text))||!*reply.text)return false;
        reply.kind=AI_REPLY_TEXT;
    }else return false;
    *out=reply;return true;
}
const ai_model_adapter_t ai_ark_adapter={"doubao","https://ark.cn-beijing.volces.com/api/v3/chat/completions",encode,decode};
