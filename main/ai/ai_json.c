#include "ai_json.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>
static int hex(char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;}
static bool codepoint(const char *s,size_t end,size_t *at,uint32_t *cp){
    if(*at>=end)return false;
    unsigned char c=(unsigned char)s[(*at)++];*cp=c;
    if(c=='\\'){
        if(*at>=end)return false;
        c=(unsigned char)s[(*at)++];
        const char *esc="\"\\/bfnrt",*v=strchr(esc,c);
        if(v){static const unsigned char values[]={'"','\\','/','\b','\f','\n','\r','\t'};*cp=values[v-esc];return true;}
        if(c!='u'||end-*at<4)return false;
        *cp=0;for(unsigned i=0;i<4;++i){int n=hex(s[(*at)++]);if(n<0)return false;*cp=(*cp<<4)|(unsigned)n;}
        if(*cp>=0xd800&&*cp<=0xdbff){
            if(end-*at<6||s[*at]!='\\'||s[*at+1]!='u')return false;
            *at+=2;uint32_t low=0;for(unsigned i=0;i<4;++i){int n=hex(s[(*at)++]);if(n<0)return false;low=(low<<4)|(unsigned)n;}
            if(low<0xdc00||low>0xdfff)return false;
            *cp=0x10000+((*cp-0xd800)<<10)+(low-0xdc00);
        }else if(*cp>=0xdc00&&*cp<=0xdfff)return false;
        return *cp!=0;
    }
    if(c<32 || c=='"')return false;
    if(c<128)return true;
    unsigned more=c>=0xc2&&c<=0xdf?1:c>=0xe0&&c<=0xef?2:c>=0xf0&&c<=0xf4?3:0;
    if(!more||end-*at<more)return false;
    *cp=c&((1u<<(6-more))-1);
    for(unsigned i=0;i<more;++i){c=(unsigned char)s[(*at)++];if((c&0xc0)!=0x80)return false;*cp=(*cp<<6)|(c&63);}
    return *cp>=(more==1?0x80u:more==2?0x800u:0x10000u)&&*cp<=0x10ffff&& !(*cp>=0xd800&&*cp<=0xdfff);
}
static void whitespace(const char *s,size_t size,size_t *p){while(*p<size&&strchr(" \t\r\n",s[*p]))++*p;}
static bool equal(const ai_json_t *j,int a,int b){
    size_t x=j->nodes[a].start+1,y=j->nodes[b].start+1,xe=j->nodes[a].end-1,ye=j->nodes[b].end-1;
    while(x<xe&&y<ye){uint32_t u,v;if(!codepoint(j->text,xe,&x,&u)||!codepoint(j->text,ye,&y,&v)||u!=v)return false;}
    return x==xe&&y==ye;
}
static int value(ai_json_t *j,size_t *p,unsigned depth){
    whitespace(j->text,j->size,p);if(depth>AI_JSON_DEPTH||*p>=j->size||j->count==AI_JSON_NODES)return -1;
    int id=j->count++;ai_json_node_t *n=&j->nodes[id];n->start=*p;n->count=0;
    char c=j->text[*p];
    if(c=='{'||c=='['){
        n->type=c=='{'?AJ_OBJECT:AJ_ARRAY;++*p;whitespace(j->text,j->size,p);char close=c=='{'?'}':']';
        if(*p<j->size&&j->text[*p]==close)++*p;
        else for(;;){
            if(c=='{'){
                int key=value(j,p,depth+1);if(key<0||j->nodes[key].type!=AJ_STRING)return -1;
                for(int prev=id+1;prev<key;prev=j->nodes[j->nodes[prev].next].next)if(equal(j,prev,key))return -1;
                whitespace(j->text,j->size,p);if(*p>=j->size||j->text[(*p)++]!=':')return -1;
            }
            if(value(j,p,depth+1)<0)return -1;
            ++n->count;whitespace(j->text,j->size,p);if(*p>=j->size)return -1;
            c=j->text[n->start];char sep=j->text[(*p)++];if(sep==close)break;if(sep!=',')return -1;
        }
    }else if(c=='"'){
        n->type=AJ_STRING;++*p;
        while(*p<j->size&&j->text[*p]!='"'){uint32_t cp;if(!codepoint(j->text,j->size,p,&cp))return -1;}
        if(*p>=j->size)return -1;
        ++*p;
    }else if(c=='t'||c=='f'||c=='n'){
        const char *literal=c=='t'?"true":c=='f'?"false":"null";size_t len=strlen(literal);
        if(j->size-*p<len||memcmp(j->text+*p,literal,len))return -1;
        n->type=c=='n'?AJ_NULL:AJ_BOOL;*p+=len;
    }else{
        n->type=AJ_NUMBER;if(c=='-')++*p;
        if(*p>=j->size)return -1;
        if(j->text[*p]=='0')++*p;
        else{if(j->text[*p]<'1'||j->text[*p]>'9')return -1;while(*p<j->size&&j->text[*p]>='0'&&j->text[*p]<='9')++*p;}
        if(*p<j->size&&j->text[*p]=='.'){++*p;size_t start=*p;while(*p<j->size&&j->text[*p]>='0'&&j->text[*p]<='9')++*p;if(start==*p)return -1;}
        if(*p<j->size&&(j->text[*p]=='e'||j->text[*p]=='E')){++*p;if(*p<j->size&&(j->text[*p]=='+'||j->text[*p]=='-'))++*p;size_t start=*p;while(*p<j->size&&j->text[*p]>='0'&&j->text[*p]<='9')++*p;if(start==*p)return -1;}
    }
    n->end=*p;n->next=j->count;return id;
}
bool ai_json_parse(ai_json_t *j,const char *s,size_t size){
    if(!j||!s||!size||memchr(s,0,size))return false;
    j->text=s;j->size=size;j->count=0;size_t p=0;
    if(value(j,&p,0)<0)return false;
    whitespace(s,size,&p);return p==size;
}
bool ai_json_string(const ai_json_t *j,int id,char *out,size_t cap){
    if(!j||id<0||id>=j->count||j->nodes[id].type!=AJ_STRING||!out||!cap)return false;
    size_t p=j->nodes[id].start+1,end=j->nodes[id].end-1,used=0;
    while(p<end){uint32_t cp;if(!codepoint(j->text,end,&p,&cp))return false;unsigned n=cp<128?1:cp<0x800?2:cp<0x10000?3:4;
        if(used+n>=cap)return false;
        if(n==1)out[used++]=(char)cp;
        else{out[used++]=(char)((n==2?0xc0:n==3?0xe0:0xf0)|(cp>>(6*(n-1))));for(unsigned k=n-1;k>0;--k)out[used++]=(char)(0x80|((cp>>(6*(k-1)))&63));}
    }out[used]=0;return true;
}
int ai_json_member(const ai_json_t *j,int id,const char *name){
    if(!j||id<0||id>=j->count||j->nodes[id].type!=AJ_OBJECT)return -1;
    for(int key=id+1;key<j->nodes[id].next;key=j->nodes[j->nodes[key].next].next){char decoded[96];if(ai_json_string(j,key,decoded,sizeof(decoded))&&!strcmp(decoded,name))return j->nodes[key].next;}
    return -1;
}
bool ai_json_integer(const ai_json_t *j,int id,int64_t *out){
    if(!j||id<0||id>=j->count||j->nodes[id].type!=AJ_NUMBER||!out)return false;
    size_t p=j->nodes[id].start,end=j->nodes[id].end;bool negative=j->text[p]=='-';if(negative)++p;
    uint64_t n=0,limit=negative?(uint64_t)INT64_MAX+1:INT64_MAX;
    for(;p<end;++p){char c=j->text[p];if(c<'0'||c>'9'||n>(limit-(unsigned)(c-'0'))/10)return false;n=n*10+(unsigned)(c-'0');}
    *out=negative?(n==(uint64_t)INT64_MAX+1?INT64_MIN:-(int64_t)n):(int64_t)n;return true;
}
bool ai_json_fields(const ai_json_t *j,int id,const char *const *names,unsigned count){
    if(!j||id<0||id>=j->count||j->nodes[id].type!=AJ_OBJECT||j->nodes[id].count!=(int)count)return false;
    for(unsigned i=0;i<count;++i)if(ai_json_member(j,id,names[i])<0)return false;
    return true;
}
bool ai_json_raw(const ai_json_t *j,int id,char *out,size_t cap){
    if(!j||id<0||id>=j->count||!out)return false;
    size_t n=j->nodes[id].end-j->nodes[id].start;if(n>=cap)return false;
    memcpy(out,j->text+j->nodes[id].start,n);out[n]=0;return true;
}
void ai_json_writer(ai_json_writer_t *w,char *data,size_t cap){*w=(ai_json_writer_t){.data=data,.size=cap,.ok=data&&cap};if(w->ok)data[0]=0;}
void ai_json_write(ai_json_writer_t *w,const char *s){
    if(!w->ok)return;
    size_t n=strlen(s);if(n>=w->size-w->used){w->ok=false;w->data[0]=0;return;}
    memcpy(w->data+w->used,s,n+1);w->used+=n;
}
void ai_json_quote(ai_json_writer_t *w,const char *s){
    ai_json_write(w,"\"");for(const unsigned char *p=(const unsigned char *)s;*p;++p){char escaped[7];
        if(*p=='"'||*p=='\\'){escaped[0]='\\';escaped[1]=(char)*p;escaped[2]=0;}
        else if(*p<32)snprintf(escaped,sizeof(escaped),"\\u%04x",*p);
        else{escaped[0]=(char)*p;escaped[1]=0;}
        ai_json_write(w,escaped);
    }ai_json_write(w,"\"");
}
void ai_json_int(ai_json_writer_t *w,int64_t value){char n[24];snprintf(n,sizeof(n),"%lld",(long long)value);ai_json_write(w,n);}
