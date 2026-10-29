#include "input/text_input.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint64_t now=1000;
static void press(text_input_t *t,unsigned slot,app_gesture_t g){
    text_input_gesture(t,slot,g,now+100);now+=200;
}
static void pick(text_input_t *t,char c){
    for(unsigned i=0;i<text_input_choice_count(t);++i)if(text_input_candidate(t,i).character==c){
        t->focus[t->mode]=i;press(t,1,APP_GESTURE_SHORT);return;
    }assert(false);
}
int main(void){
    text_input_t t; text_input_config_t cfg={.maximum_length=64};
    assert(text_input_begin(&t,&cfg,now));assert(text_input_choice_count(&t)==31);
    assert(text_input_row_size(&t,0)==10 && text_input_row_size(&t,1)==9 && text_input_row_size(&t,2)==7);
    bool coverage[127]={0};
    for(unsigned m=0;m<TEXT_INPUT_MODE_COUNT;++m){t.mode=m;
        for(unsigned i=0;i<text_input_choice_count(&t);++i){text_input_candidate_t k=text_input_candidate(&t,i);
            if(k.action==TEXT_KEY_CHARACTER){assert(!coverage[(unsigned char)k.character]);coverage[(unsigned char)k.character]=true;}}
    }
    for(unsigned ch=33;ch<127;++ch)if(ch<'A' || ch>'Z')assert(coverage[ch]);
    t.mode=TEXT_INPUT_ABC;
    // Every candidate reachable with horizontal/vertical gestures, without teleporting.
    bool reached[TEXT_INPUT_MAX_KEYS]={0};unsigned start=0;
    for(unsigned r=0;r<text_input_row_count(&t);++r){
        unsigned n=text_input_row_size(&t,r),first=text_input_choice_focus(&t);
        for(unsigned c=0;c<n;++c){reached[text_input_choice_focus(&t)]=true;press(&t,2,APP_GESTURE_SHORT);}
        assert(text_input_choice_focus(&t)==first);press(&t,2,APP_GESTURE_LONG);start+=n;
    }
    for(unsigned i=0;i<start;++i)assert(reached[i]);
    // Positional row mapping, including short footer rows and every mode edge.
    for(unsigned m=0;m<TEXT_INPUT_MODE_COUNT;++m){t.mode=m;
        for(unsigned i=0;i<text_input_choice_count(&t);++i){
            t.focus[m]=i;press(&t,0,APP_GESTURE_LONG);assert(t.focus[m]<text_input_choice_count(&t));
            t.focus[m]=i;press(&t,2,APP_GESTURE_LONG);assert(t.focus[m]<text_input_choice_count(&t));
        }
    }
    t.mode=TEXT_INPUT_ABC;t.focus[0]=4;press(&t,2,APP_GESTURE_LONG);assert(t.focus[0]==14);
    press(&t,2,APP_GESTURE_LONG);assert(t.focus[0]==22);
    // Visible footer routes to other modes, inserts space and completes directly.
    text_input_t footer;assert(text_input_begin(&footer,&cfg,now));
    footer.focus[0]=26;press(&footer,1,APP_GESTURE_SHORT);assert(footer.mode==TEXT_INPUT_DIGITS);
    footer.focus[1]=11;press(&footer,1,APP_GESTURE_SHORT);assert(footer.mode==TEXT_INPUT_SYMBOLS);
    footer.focus[2]=33;press(&footer,1,APP_GESTURE_SHORT);assert(footer.mode==TEXT_INPUT_DIGITS);
    footer.focus[1]=12;press(&footer,1,APP_GESTURE_SHORT);assert(!strcmp(footer.text," "));
    footer.focus[1]=14;press(&footer,1,APP_GESTURE_SHORT);assert(footer.page==TEXT_INPUT_DONE);
    t.focus[0]=0;press(&t,0,APP_GESTURE_SHORT);assert(t.focus[0]==9);
    press(&t,2,APP_GESTURE_SHORT);assert(t.focus[0]==0);
    press(&t,0,APP_GESTURE_DOUBLE);assert(t.uppercase && !t.length);
    pick(&t,'T');press(&t,0,APP_GESTURE_DOUBLE);pick(&t,'h');pick(&t,'e');pick(&t,'r');
    press(&t,2,APP_GESTURE_DOUBLE);assert(t.mode==TEXT_INPUT_DIGITS);pick(&t,'2');
    press(&t,2,APP_GESTURE_DOUBLE);assert(t.mode==TEXT_INPUT_SYMBOLS);pick(&t,'!');
    assert(!strcmp(t.text,"Ther2!"));press(&t,1,APP_GESTURE_LONG);assert(!strcmp(t.text,"Ther2"));
    press(&t,1,APP_GESTURE_DOUBLE);assert(t.page==TEXT_INPUT_EDIT && t.length==5);
    t.edit_focus=0;press(&t,1,APP_GESTURE_SHORT);assert(t.page==TEXT_INPUT_CURSOR);
    press(&t,0,APP_GESTURE_SHORT);assert(t.cursor==4);press(&t,1,APP_GESTURE_SHORT);
    pick(&t,'_');assert(!strcmp(t.text,"Ther_2"));
    t.config.secret=true;char out[20];text_input_window(&t,now+2000,out,sizeof(out));assert(!strcmp(out,"*****|*"));
    press(&t,1,APP_GESTURE_DOUBLE);t.edit_focus=1;press(&t,1,APP_GESTURE_SHORT);
    text_input_window(&t,now,out,sizeof(out));assert(!strcmp(out,"Ther_|2"));
    t.edit_focus=2;press(&t,1,APP_GESTURE_SHORT);press(&t,0,APP_GESTURE_SHORT);assert(t.length==6);
    press(&t,1,APP_GESTURE_SHORT);press(&t,1,APP_GESTURE_SHORT);assert(!t.length && t.page==TEXT_INPUT_EDIT);
    t.edit_focus=4;press(&t,1,APP_GESTURE_SHORT);press(&t,2,APP_GESTURE_SHORT);assert(t.page==TEXT_INPUT_EDIT);
    t.edit_focus=4;press(&t,1,APP_GESTURE_SHORT);press(&t,1,APP_GESTURE_SHORT);assert(t.page==TEXT_INPUT_CANCELLED);
    text_input_dispose(&t);for(unsigned i=0;i<sizeof(t);++i)assert(((unsigned char *)&t)[i]==0);
    cfg=(text_input_config_t){.maximum_length=2,.numeric_only=true};assert(text_input_begin(&t,&cfg,now));
    press(&t,2,APP_GESTURE_DOUBLE);press(&t,0,APP_GESTURE_DOUBLE);assert(t.mode==TEXT_INPUT_DIGITS && !t.uppercase);
    pick(&t,'1');pick(&t,'2');pick(&t,'3');assert(t.full && !strcmp(t.text,"12"));
    app_bindings_t b;text_input_bindings(&t,&b);assert(!b.actions[APP_GESTURE_DOUBLE][0].id);
    t.focus[t.mode]=text_input_choice_count(&t)-1;press(&t,1,APP_GESTURE_SHORT);assert(t.page==TEXT_INPUT_DONE);
    cfg.initial="a";assert(!text_input_begin(&t,&cfg,now));
    cfg.numeric_only=false;cfg.initial=NULL;assert(text_input_begin(&t,&cfg,now));
    text_input_bindings(&t,&b);for(unsigned g=0;g<3;++g)for(unsigned k=0;k<3;++k)assert(b.actions[g][k].id);
    puts("QWERTY ASCII coverage, row navigation/reachability, nine gestures, edit/mask/bounds/cancel: PASS");
}
