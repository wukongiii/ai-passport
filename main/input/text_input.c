#include "text_input.h"
#include "text_input_strings.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
const text_input_layout_t text_input_qwerty_layout={{"qwertyuiop","asdfghjkl","zxcvbnm"},3};
static const text_input_layout_t digits={{"1234567890"},1};
static const text_input_layout_t symbols={{"._-@!?#$%&*","+=/\\:;()[]{","}<>\"'`~,^|"},3};
static const char *const edits[]={TXT_INPUT_CURSOR,TXT_INPUT_REVEAL,TXT_INPUT_CLEAR,TXT_INPUT_FINISH,TXT_INPUT_EXIT};
static const text_input_layout_t *layout(const text_input_t *t){
    return t->mode==TEXT_INPUT_ABC ? &text_input_qwerty_layout : t->mode==TEXT_INPUT_DIGITS ? &digits : &symbols;
}
static bool active(const text_input_t *t){return t->page<TEXT_INPUT_DONE;}
static unsigned characters(const text_input_t *t){
    const text_input_layout_t *l=layout(t);unsigned n=0;
    for(unsigned r=0;r<l->row_count;++r)n+=(unsigned)strlen(l->rows[r]);
    return n;
}
unsigned text_input_row_count(const text_input_t *t){
    return t->page==TEXT_INPUT_CHARACTERS ? layout(t)->row_count+1 : t->page==TEXT_INPUT_EDIT ? 2 : 0;
}
unsigned text_input_row_size(const text_input_t *t,unsigned row){
    if(row>=text_input_row_count(t))return 0;
    if(t->page==TEXT_INPUT_EDIT)return row ? 2 : 3;
    return row<layout(t)->row_count ? (unsigned)strlen(layout(t)->rows[row]) : t->config.numeric_only ? 2 : 5;
}
unsigned text_input_choice_count(const text_input_t *t){
    if(t->page==TEXT_INPUT_EDIT)return 5;
    return t->page==TEXT_INPUT_CHARACTERS ? characters(t)+(t->config.numeric_only ? 2 : 5) : 0;
}
unsigned text_input_choice_focus(const text_input_t *t){return t->page==TEXT_INPUT_EDIT ? t->edit_focus : t->focus[t->mode];}
text_input_candidate_t text_input_candidate(const text_input_t *t,unsigned index){
    if(t->page!=TEXT_INPUT_CHARACTERS || index>=text_input_choice_count(t))return (text_input_candidate_t){0};
    const text_input_layout_t *l=layout(t);
    for(unsigned r=0;r<l->row_count;++r){unsigned n=(unsigned)strlen(l->rows[r]);
        if(index<n){char c=l->rows[r][index];if(t->mode==TEXT_INPUT_ABC && t->uppercase)c=(char)toupper((unsigned char)c);
            return (text_input_candidate_t){c,TEXT_KEY_CHARACTER};}index-=n;}
    static const text_input_key_action_t footer[]={TEXT_KEY_DIGITS,TEXT_KEY_SYMBOLS,TEXT_KEY_SPACE,TEXT_KEY_MENU,TEXT_KEY_FINISH};
    return (text_input_candidate_t){0,t->config.numeric_only ? (index ? TEXT_KEY_FINISH : TEXT_KEY_MENU) :
        index==0 && t->mode!=TEXT_INPUT_ABC ? TEXT_KEY_ABC : index==1 && t->mode==TEXT_INPUT_SYMBOLS ? TEXT_KEY_DIGITS : footer[index]};
}
void text_input_choice(const text_input_t *t,unsigned index,char *out,unsigned capacity){
    if(!out || !capacity)return;
    out[0]=0;
    if(index>=text_input_choice_count(t))return;
    if(t->page==TEXT_INPUT_EDIT){snprintf(out,capacity,"%s",index==1 && t->revealed ? TXT_INPUT_HIDE : edits[index]);return;}
    text_input_candidate_t k=text_input_candidate(t,index);
    static const char *const names[]={"","ABC","123","#+=",TXT_INPUT_SPACE,TXT_INPUT_MENU,TXT_INPUT_FINISH};
    if(k.action==TEXT_KEY_CHARACTER)snprintf(out,capacity,"%c",k.character);else snprintf(out,capacity,"%s",names[k.action]);
}
bool text_input_begin(text_input_t *t,const text_input_config_t *cfg,uint64_t now){
    (void)now;if(!t || !cfg || !cfg->maximum_length || cfg->maximum_length>TEXT_INPUT_CAPACITY)return false;
    const char *s=cfg->initial ? cfg->initial : "";unsigned n=(unsigned)strlen(s);
    if(n>cfg->maximum_length)return false;
    for(unsigned i=0;i<n;++i)if((unsigned char)s[i]<32 || (unsigned char)s[i]>126 || (cfg->numeric_only && !isdigit((unsigned char)s[i])))return false;
    *t=(text_input_t){.config=*cfg,.length=n,.cursor=n,.mode=cfg->numeric_only ? TEXT_INPUT_DIGITS : TEXT_INPUT_ABC};
    t->config.initial=NULL;memcpy(t->text,s,n+1);return true;
}
static void horizontal(text_input_t *t,int direction){
    if(t->page==TEXT_INPUT_CURSOR){if(direction<0 && t->cursor)--t->cursor;else if(direction>0 && t->cursor<t->length)++t->cursor;return;}
    unsigned *focus=t->page==TEXT_INPUT_EDIT ? &t->edit_focus : &t->focus[t->mode];
    unsigned start=0;
    for(unsigned r=0;r<text_input_row_count(t);++r){unsigned n=text_input_row_size(t,r);
        if(*focus<start+n){*focus=start+(*focus-start+n+direction)%n;return;}start+=n;}
}
static void vertical(text_input_t *t,int direction){
    const unsigned rows=text_input_row_count(t);if(!rows)return;
    unsigned *focus=t->page==TEXT_INPUT_EDIT ? &t->edit_focus : &t->focus[t->mode];
    unsigned row=0,start=0;
    while(row+1<rows && *focus>=start+text_input_row_size(t,row))start+=text_input_row_size(t,row++);
    unsigned old_size=text_input_row_size(t,row),col=*focus-start;
    unsigned next=(row+rows+direction)%rows,next_start=0;
    for(unsigned r=0;r<next;++r)next_start+=text_input_row_size(t,r);
    unsigned new_size=text_input_row_size(t,next);
    *focus=next_start+((2*col+1)*new_size)/(2*old_size);
}
static void insert(text_input_t *t,char c,uint64_t now){
    if(t->length>=t->config.maximum_length){t->full=true;return;}
    memmove(t->text+t->cursor+1,t->text+t->cursor,t->length-t->cursor+1);
    t->text[t->cursor++]=c;t->length++;t->reveal_until_ms=now+1000;t->full=false;
}
static void erase(text_input_t *t){
    if(!t->cursor)return;
    memmove(t->text+t->cursor-1,t->text+t->cursor,t->length-t->cursor+1);
    --t->cursor;--t->length;t->full=false;t->reveal_until_ms=0;
}
static void select(text_input_t *t,uint64_t now){
    switch(t->page){
    case TEXT_INPUT_CHARACTERS:{
        text_input_candidate_t k=text_input_candidate(t,t->focus[t->mode]);
        switch(k.action){
        case TEXT_KEY_CHARACTER:insert(t,k.character,now);break;
        case TEXT_KEY_SPACE:insert(t,' ',now);break;
        case TEXT_KEY_ABC:t->mode=TEXT_INPUT_ABC;break;
        case TEXT_KEY_DIGITS:t->mode=TEXT_INPUT_DIGITS;break;
        case TEXT_KEY_SYMBOLS:t->mode=TEXT_INPUT_SYMBOLS;break;
        case TEXT_KEY_MENU:t->page=TEXT_INPUT_EDIT;break;
        case TEXT_KEY_FINISH:t->page=TEXT_INPUT_DONE;break;
        }break;}
    case TEXT_INPUT_EDIT:
        if(t->edit_focus==0)t->page=TEXT_INPUT_CURSOR;
        else if(t->edit_focus==1)t->revealed=!t->revealed;
        else if(t->edit_focus==2)t->page=TEXT_INPUT_CLEAR;
        else if(t->edit_focus==3)t->page=TEXT_INPUT_DONE;
        else t->page=TEXT_INPUT_CANCEL;
        break;
    case TEXT_INPUT_CURSOR:t->page=TEXT_INPUT_CHARACTERS;break;
    case TEXT_INPUT_CLEAR:memset(t->text,0,sizeof(t->text));t->cursor=t->length=0;t->full=false;t->page=TEXT_INPUT_EDIT;break;
    case TEXT_INPUT_CANCEL:memset(t->text,0,sizeof(t->text));t->cursor=t->length=0;t->page=TEXT_INPUT_CANCELLED;break;
    default:break;
    }
}
void text_input_gesture(text_input_t *t,unsigned slot,app_gesture_t gesture,uint64_t now){
    if(!active(t) || slot>=3)return;
    const bool confirm=t->page==TEXT_INPUT_CLEAR || t->page==TEXT_INPUT_CANCEL;
    if(confirm){if(slot==1 && gesture==APP_GESTURE_SHORT)select(t,now);
        else if(gesture==APP_GESTURE_SHORT || (slot==1 && gesture==APP_GESTURE_LONG))t->page=TEXT_INPUT_EDIT;
        return;
    }
    if(gesture==APP_GESTURE_SHORT){if(slot==1)select(t,now);else horizontal(t,slot==0 ? -1 : 1);}
    else if(gesture==APP_GESTURE_LONG){
        if(slot!=1)vertical(t,slot==0 ? -1 : 1);
        else if(t->page==TEXT_INPUT_CHARACTERS || t->page==TEXT_INPUT_CURSOR)erase(t);
        else t->page=TEXT_INPUT_CHARACTERS;
    }else if(gesture==APP_GESTURE_DOUBLE){
        if(slot==1)t->page=t->page==TEXT_INPUT_CHARACTERS ? TEXT_INPUT_EDIT : TEXT_INPUT_CHARACTERS;
        else if(t->page==TEXT_INPUT_CHARACTERS && !t->config.numeric_only){
            if(slot==0){if(t->mode==TEXT_INPUT_ABC)t->uppercase=!t->uppercase;else t->mode=TEXT_INPUT_ABC;}
            else t->mode=(t->mode+1)%TEXT_INPUT_MODE_COUNT;
        }
    }
}
void text_input_window(const text_input_t *t,uint64_t now,char *out,unsigned capacity){
    if(!out || capacity<4)return;
    unsigned available=capacity-2;
    unsigned start=t->cursor>available/2 ? t->cursor-available/2 : 0;
    if(start+available>t->length)start=t->length>available ? t->length-available : 0;
    unsigned end=start+available;if(end>t->length)end=t->length;unsigned n=0;
    for(unsigned i=start;i<=end;++i){if(i==t->cursor)out[n++]='|';if(i==end)break;
        bool mask=t->config.secret && !t->revealed && !(i+1==t->cursor && now<t->reveal_until_ms);
        out[n++]=mask ? '*' : t->text[i];}out[n]=0;
}
void text_input_dispose(text_input_t *t){volatile unsigned char *p=(volatile unsigned char *)t;for(unsigned i=0;i<sizeof(*t);++i)p[i]=0;}
enum {INPUT_LEFT=300,INPUT_SELECT,INPUT_RIGHT,INPUT_UP,INPUT_ERASE,INPUT_DOWN,INPUT_CASE,INPUT_MENU,INPUT_MODE};
void text_input_bindings(const text_input_t *t,app_bindings_t *b){
    app_actions_clear(b);if(!active(t))return;
    bool confirm=t->page==TEXT_INPUT_CLEAR || t->page==TEXT_INPUT_CANCEL;
    bool keyboard=t->page==TEXT_INPUT_CHARACTERS,cursor=t->page==TEXT_INPUT_CURSOR;
    app_actions_register(b,0,APP_GESTURE_SHORT,INPUT_LEFT,confirm ? TXT_INPUT_CANCEL : TXT_INPUT_LEFT,true,confirm ? APP_ACTION_CANCEL : APP_ACTION_NAVIGATE);
    app_actions_register(b,1,APP_GESTURE_SHORT,INPUT_SELECT,confirm ? TXT_INPUT_CONFIRM : cursor ? TXT_INPUT_BACK : keyboard ? TXT_INPUT_INSERT : TXT_INPUT_SELECT,true,APP_ACTION_CONFIRM);
    app_actions_register(b,2,APP_GESTURE_SHORT,INPUT_RIGHT,confirm ? TXT_INPUT_BACK : TXT_INPUT_RIGHT,true,confirm ? APP_ACTION_BACK : APP_ACTION_NAVIGATE);
    app_actions_register(b,1,APP_GESTURE_LONG,INPUT_ERASE,keyboard || cursor ? TXT_INPUT_DELETE : TXT_INPUT_BACK,true,keyboard || cursor ? APP_ACTION_DECREASE : APP_ACTION_BACK);
    if(confirm)return;
    if(!cursor){
        app_actions_register(b,0,APP_GESTURE_LONG,INPUT_UP,TXT_INPUT_UP,true,APP_ACTION_NAVIGATE);
        app_actions_register(b,2,APP_GESTURE_LONG,INPUT_DOWN,TXT_INPUT_DOWN,true,APP_ACTION_NAVIGATE);
    }
    app_actions_register(b,1,APP_GESTURE_DOUBLE,INPUT_MENU,keyboard ? TXT_INPUT_MENU : TXT_INPUT_BACK,true,APP_ACTION_BACK);
    if(keyboard && !t->config.numeric_only){
        app_actions_register(b,0,APP_GESTURE_DOUBLE,INPUT_CASE,t->mode!=TEXT_INPUT_ABC ? "ABC" : t->uppercase ? TXT_INPUT_LOWER : TXT_INPUT_UPPER,true,APP_ACTION_NEUTRAL);
        app_actions_register(b,2,APP_GESTURE_DOUBLE,INPUT_MODE,TXT_INPUT_MODE,true,APP_ACTION_NAVIGATE);
    }
}
