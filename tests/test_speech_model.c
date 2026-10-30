#include "speech/speech_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
 speech_model_t m={0};char out[SPEECH_TEXT_CAPACITY];
 assert(!speech_model_take(&m,1000,out));
 assert(speech_model_submit(&m,"三比二",100));assert(!speech_model_take(&m,749,out));
 assert(speech_model_submit(&m,"四比二",700));assert(!speech_model_take(&m,1349,out));
 assert(speech_model_take(&m,1350,out));assert(!strcmp(out,"四比二"));assert(!speech_model_take(&m,2000,out));
 assert(speech_model_submit(&m,"一号选手获胜",3000));
 assert(!speech_model_submit(&m,"",3100));assert(!speech_model_submit(&m,NULL,3100));assert(!speech_model_submit(&m,"  ",3100));
 assert(!speech_model_submit(&m,"\xE4\xB8",3100));assert(!speech_model_submit(&m,"\xC0\x80",3100));assert(!speech_model_submit(&m,"\xED\xA0\x80",3100));
 assert(speech_model_take(&m,3650,out));assert(!strcmp(out,"一号选手获胜"));
 char long_text[SPEECH_TEXT_CAPACITY+1];memset(long_text,'x',sizeof(long_text));long_text[sizeof(long_text)-1]=0;
 assert(!speech_model_submit(&m,long_text,4000));long_text[SPEECH_TEXT_CAPACITY-1]=0;assert(speech_model_submit(&m,long_text,4000));
 speech_model_cancel(&m);assert(!speech_model_take(&m,9000,out));
 const char *text="先报比分，接着完整播放第二个较长句子直到结尾。😀";
 char joined[SPEECH_TEXT_CAPACITY]={0},phrase[SPEECH_SEGMENT_CAPACITY];size_t offset=0;
 assert(speech_model_segment(text,phrase)==strlen("先报比分，")&&!strcmp(phrase,"先报比分，"));
 while(text[offset]){size_t n=speech_model_segment(text+offset,phrase);assert(n&&n<sizeof(phrase));strcat(joined,phrase);offset+=n;}
 assert(!strcmp(joined,text));assert(!speech_model_segment("",phrase));
 assert(speech_model_segment("abcdefghijklmnop",phrase)==12&&!strcmp(phrase,"abcdefghijkl"));
 assert(speech_model_submit(&m,text,9000));

 puts("Speech merge deadline, latest-only replacement, UTF-8 bounds and cancellation: PASS");
}
