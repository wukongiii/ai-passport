#include "ai_response_filter.h"
#include <string.h>
static bool emit(ai_response_filter_t *f,char c){
 if(f->used+1>=f->capacity)return f->ok=false;
 f->out[f->used++]=c;f->out[f->used]=0;return true;
}
static int hex(unsigned char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;}
/* Validate every discarded byte too, including UTF-8 and surrogate pairs. */
static bool discarded(ai_response_filter_t *f,unsigned char c){
 if(f->hex_left){
  int n=hex(c);if(n<0)return false;f->code=(f->code<<4)|(unsigned)n;
  if(!--f->hex_left){
   if(f->high){if(f->code<0xdc00||f->code>0xdfff)return false;f->high=0;}
   else if(f->code>=0xd800&&f->code<=0xdbff){f->high=f->code;f->escape=3;}
   else if(!f->code||(f->code>=0xdc00&&f->code<=0xdfff))return false;
  }return true;
 }
 if(f->escape==3){if(c!='\\')return false;f->escape=4;return true;}
 if(f->escape==4){if(c!='u')return false;f->escape=0;f->hex_left=4;f->code=0;return true;}
 if(f->escape==1){
  f->escape=0;if(c=='u'){f->hex_left=4;f->code=0;return true;}
  return c&&strchr("\"\\/bfnrt",c);
 }
 if(f->utf_left){
  if((c&0xc0)!=0x80)return false;
  f->utf=(f->utf<<6)|(c&63);
  if(!--f->utf_left&&(f->utf<f->min||f->utf>0x10ffff||(f->utf>=0xd800&&f->utf<=0xdfff)))return false;
  return true;
 }
 if(c=='"'){f->string=f->drop=false;f->key_ready=true;f->key_size=0;return emit(f,'"');}
 if(c=='\\'){f->escape=1;return true;}
 if(c<32)return false;
 if(c<128)return true;
 if(c>=0xc2&&c<=0xdf){f->utf_left=1;f->utf=c&31;f->min=0x80;}
 else if(c>=0xe0&&c<=0xef){f->utf_left=2;f->utf=c&15;f->min=0x800;}
 else if(c>=0xf0&&c<=0xf4){f->utf_left=3;f->utf=c&7;f->min=0x10000;}
 else return false;
 return true;
}
void ai_response_filter_init(ai_response_filter_t *f,char *out,size_t capacity,const char *field){
 *f=(ai_response_filter_t){.out=out,.capacity=capacity,.discard_field=field,.ok=out&&capacity};if(f->ok)out[0]=0;
}
bool ai_response_filter_feed(ai_response_filter_t *f,const char *data,size_t size){
 if(!f->ok||size>AI_RESPONSE_WIRE_LIMIT-f->wire)return f->ok=false;
 f->wire+=size;
 for(size_t i=0;i<size;++i){unsigned char c=(unsigned char)data[i];
  if(!c)return f->ok=false;
  if(f->drop){if(!discarded(f,c))return f->ok=false;continue;}
  if(!emit(f,(char)c))return false;
  if(f->string){
   if(f->escaped){f->escaped=false;f->key_size=sizeof(f->key);continue;}
   if(c=='\\'){f->escaped=true;f->key_size=sizeof(f->key);continue;}
   if(c=='"'){
    f->string=false;f->key_ready=true;f->reasoning=f->discard_field&&strlen(f->discard_field)==f->key_size&&f->key_size<sizeof(f->key)&&!memcmp(f->key,f->discard_field,f->key_size);
   }else if(f->key_size<sizeof(f->key))f->key[f->key_size++]=(char)c;
   continue;
  }
  if(c==' '||c=='\r'||c=='\n'||c=='\t')continue;
  if(c==':'&&f->key_ready){f->value_pending=f->reasoning;f->key_ready=false;continue;}
  if(c=='"'){
   f->string=true;f->drop=f->value_pending;f->key_size=0;f->escaped=false;
  }
  f->value_pending=f->reasoning=f->key_ready=false;
 }
 return true;
}
bool ai_response_filter_finish(ai_response_filter_t *f){return f->ok&&!f->string&&!f->drop&&f->used>0;}
