#include "ai_persona.h"
#include "ai_config.h"
#include <stdint.h>
#include <string.h>
static const char prompt[]=
 "你是台球记分助手，使用配置的AI名字回答身份问题。理解语音和口语表达，按当前玩法接口处理记分、纠错、查询和撤销。"
 "用户已经说明选手和数值时直接调用工具，不重复确认；信息不足才问一个简短问题，保留原动作和数值。"
 "可以回答台球或日常问题。默认使用中文，每次只答一句，最多30个汉字；澄清最多20个汉字。"
 "不输出推理过程、寒暄、列表或工具使用说明。";
const char *ai_persona_default_prompt(void){return prompt;}
void ai_persona_default(ai_persona_t *p){*p=(ai_persona_t){.name="逗宝"};memcpy(p->prompt,prompt,sizeof(prompt));}
bool ai_persona_valid(const ai_persona_t *p){
 if(!p||!ai_config_name_valid(p->name))return false;
 const unsigned char *s=(const unsigned char *)p->prompt;
 const unsigned char *end=memchr(s,0,sizeof(p->prompt));if(!end||s==end)return false;
 bool visible=false;
 while(s<end){uint32_t cp=*s++;unsigned n=0;
  if(cp<128){if(cp<32&&cp!='\n'&&cp!='\t')return false;if(cp==127)return false;}
  else{
   if(cp>=0xc2&&cp<=0xdf){n=1;cp&=31;}
   else if(cp>=0xe0&&cp<=0xef){n=2;cp&=15;}
   else if(cp>=0xf0&&cp<=0xf4){n=3;cp&=7;}else return false;
   if((size_t)(end-s)<n)return false;
   for(unsigned i=0;i<n;++i){unsigned c=*s++;if((c&0xc0)!=0x80)return false;cp=(cp<<6)|(c&63);}
   if(cp<(n==1?0x80u:n==2?0x800u:0x10000u)||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff))return false;
  }
  if(cp>32&&cp!=0x3000)visible=true;
 }
 return visible;
}
