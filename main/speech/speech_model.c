#include "speech_model.h"
#include <string.h>
static bool valid(const char *s,size_t *length){
    if(!s)return false;
    size_t n=0;while(n<SPEECH_TEXT_CAPACITY && s[n])++n;
    if(!n || n==SPEECH_TEXT_CAPACITY)return false;
    bool spoken=false;
    for(size_t i=0;i<n;){
        unsigned char c=(unsigned char)s[i++];
        if(c<32 || c==127)return false;
        if(c<128){if(c!=' ')spoken=true;continue;}
        unsigned extra;uint32_t cp;
        if(c>=0xC2 && c<=0xDF){extra=1;cp=c&31;}
        else if(c>=0xE0 && c<=0xEF){extra=2;cp=c&15;}
        else if(c>=0xF0 && c<=0xF4){extra=3;cp=c&7;}
        else return false;
        if(i+extra>n)return false;
        for(unsigned j=0;j<extra;++j){unsigned char v=(unsigned char)s[i++];if((v&0xC0)!=0x80)return false;cp=(cp<<6)|(v&63);}
        if((extra==1 && cp<0x80)||(extra==2 && cp<0x800)||(extra==3 && cp<0x10000)||cp>0x10FFFF||(cp>=0xD800 && cp<=0xDFFF))return false;
        spoken=true;
    }
    *length=n;return spoken;
}
bool speech_model_submit(speech_model_t *m,const char *text,uint64_t now){
    size_t n;if(!valid(text,&n))return false;
    memcpy(m->text,text,n+1);m->due=now+SPEECH_MERGE_MS;m->pending=true;return true;
}
bool speech_model_take(speech_model_t *m,uint64_t now,char out[SPEECH_TEXT_CAPACITY]){
    if(!m->pending || now<m->due)return false;
    memcpy(out,m->text,sizeof(m->text));speech_model_cancel(m);return true;
}
void speech_model_cancel(speech_model_t *m){memset(m,0,sizeof(*m));}
size_t speech_model_segment(const char *text,char out[SPEECH_SEGMENT_CAPACITY]){
    size_t used=0;
    for(unsigned chars=0;text[used]&&chars<SPEECH_SEGMENT_CHARS;++chars){
        unsigned char c=(unsigned char)text[used];
        size_t n=c<128?1:c<0xe0?2:c<0xf0?3:4;
        bool end=(n==1&&strchr(",.!?;:",c)) ||
            (n==3&&(!memcmp(text+used,"，",3)||!memcmp(text+used,"。",3)||
                    !memcmp(text+used,"？",3)||!memcmp(text+used,"！",3)||!memcmp(text+used,"；",3)));
        memcpy(out+used,text+used,n);used+=n;if(end)break;
    }
    out[used]=0;return used;
}
