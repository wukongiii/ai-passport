#include "ai/ai_response_filter.h"
#include "ai/ai_json.h"
#include "ai/ai_contract.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static char out[AI_RESPONSE_CAPACITY+1],wire[20000];
static ai_json_t json;
static bool run(const char *body,size_t chunk){
 ai_response_filter_t f;ai_response_filter_init(&f,out,sizeof(out),"reasoning_content");
 size_t length=strlen(body);
 for(size_t at=0;at<length;at+=chunk){size_t n=length-at;if(n>chunk)n=chunk;if(!ai_response_filter_feed(&f,body+at,n))return false;}
 return ai_response_filter_finish(&f)&&ai_json_parse(&json,out,f.used);
}
int main(void){
 const char *prefix="{\"choices\":[{\"message\":{\"reasoning_content\":\"";
 strcpy(wire,prefix);size_t n=strlen(wire);memset(wire+n,'x',12000);n+=12000;
 strcpy(wire+n,"\\\" escaped \\\\ \\uD83D\\uDE00 中文\",\"content\":\"简短回答\",\"role\":\"assistant\"},\"finish_reason\":\"stop\"}],\"usage\":{\"tokens\":999}}");
 const unsigned chunks[]={1,7,512};
 for(unsigned i=0;i<3;++i){assert(run(wire,chunks[i]));assert(strstr(out,"\"reasoning_content\":\"\"")&&strstr(out,"简短回答")&&strlen(out)<512);}
 assert(run("{\"message\":{\"reasoning_content\":null,\"content\":\"reasoning_content\"}}",1));
 assert(!run("{\"reasoning_content\":\"\\uD800x\"}",1));
 assert(!run("{\"reasoning_content\":\"\\q\"}",1));
 assert(!run("{\"reasoning_content\":\"\xc0\x80\"}",1));
 assert(!run("{\"reasoning_content\":\"unterminated",1));
 assert(!run("{\"reasoning_content\":\"one\",\"reasoning_content\":\"two\"}",1));
 assert(!run("{\"content\":\"x\"}garbage",1));
 strcpy(wire,"{\"content\":\"");n=strlen(wire);memset(wire+n,'x',7000);strcpy(wire+n+7000,"\"}");assert(!run(wire,512));
 // Exact output capacity, pass-through mode and encoded keys remain bounded.
 ai_response_filter_t f;
 strcpy(wire,"{\"content\":\"");n=strlen(wire);
 memset(wire+n,'x',AI_RESPONSE_CAPACITY-n-2);
 strcpy(wire+AI_RESPONSE_CAPACITY-2,"\"}");
 ai_response_filter_init(&f,out,sizeof(out),NULL);
 assert(ai_response_filter_feed(&f,wire,AI_RESPONSE_CAPACITY));
 assert(ai_response_filter_finish(&f)&&f.used==AI_RESPONSE_CAPACITY);
 assert(!ai_response_filter_feed(&f," ",1));
 const char *plain="{\"reasoning_content\":\"kept\"}";
 ai_response_filter_init(&f,out,sizeof(out),NULL);
 assert(ai_response_filter_feed(&f,plain,strlen(plain))&&ai_response_filter_finish(&f));
 assert(!strcmp(out,plain));
 assert(run("{\"reasoning_\\u0063ontent\":\"kept\"}",1));assert(strstr(out,"kept"));
 const char *arguments="{\"arguments\":\"{\\\"reasoning_content\\\":\\\"kept\\\"}\"}";
 assert(run(arguments,1)&&!strcmp(out,arguments));
 ai_response_filter_init(&f,out,sizeof(out),"reasoning_content");assert(ai_response_filter_feed(&f,prefix,strlen(prefix)));
 memset(wire,'x',sizeof(wire));for(unsigned i=0;i<3;++i)assert(ai_response_filter_feed(&f,wire,sizeof(wire)));
 assert(!ai_response_filter_feed(&f,wire,sizeof(wire)));
 puts("Response filter: large reasoning, fragmented strings, retained content, invalid JSON and wire bounds PASS");
}
