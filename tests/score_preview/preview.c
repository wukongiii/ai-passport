#include <string.h>
#include "lvgl.h"
#include "ai/app_game_voice.h"
#include "score/score.h"
#include "score/score_ui.h"
#include "score/score_strings.h"
#include "ui/app_shell.h"
#include "ui/app_header.h"
#include "app/app_ui.h"
#include "input/text_input.h"
#include "app/app_settings.h"
#include "app/app_input_lab.h"
#include "network/app_network.h"
#include "speech/app_speech.h"
#include "ai/ai_config.h"
#include "app/app_preferences_store.h"
#include "app/app_strings.h"
#include "games/eight_module.h"
#include "games/game_preferences.h"
#include "score/score_store.h"
#include "bsp_display_rounding.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
// In-memory storage adapter for the actual game module in host rendering.
score_store_status_t score_store_open(score_session_t *s) { (void)s; return SCORE_STORE_EMPTY; }
esp_err_t score_store_save(const score_session_t *s) { (void)s; return ESP_OK; }
app_pref_store_status_t app_preferences_load(app_preferences_t *p) { *p=app_preferences_default();return APP_PREF_DEFAULT; }
esp_err_t app_preferences_save(const app_preferences_t *p) { (void)p; return ESP_OK; }
// Network snapshot adapter: exercise real settings UI without radio hardware.
static app_network_snapshot_t net_preview={.ready=true,.revision=1};
void app_network_snapshot(app_network_snapshot_t *s){*s=net_preview;}
bool app_network_portal(bool on){net_preview.portal=on;net_preview.seconds_left=300;net_preview.revision++;return true;}
bool app_network_forget(void){net_preview.saved=false;net_preview.revision++;return true;}
bool app_network_retry(void){return true;}
static app_net_scan_t scan_preview;
void app_network_scan_snapshot(app_net_scan_t *out){*out=scan_preview;}
bool app_network_scan(void){scan_preview.scanning=false;scan_preview.attempted=true;scan_preview.count=2;scan_preview.revision++;
    snprintf(scan_preview.entries[0].ssid,33,"台球室网络");scan_preview.entries[0].secured=true;scan_preview.entries[0].rssi=-42;
    snprintf(scan_preview.entries[1].ssid,33,"Guest");scan_preview.entries[1].secured=false;scan_preview.entries[1].rssi=-65;return true;}
bool app_network_connect(const app_net_credentials_t *c){(void)c;return true;}

void app_network_publish_preferences(const app_preferences_t *p){(void)p;}
bool app_network_take_preferences(app_preferences_t *p){(void)p;return false;}
static app_speech_snapshot_t speech_preview={.volume=65};
bool app_speech_start(void){return true;}
bool app_speech_say(const char *text){(void)text;speech_preview.state=SPEECH_WAITING;speech_preview.pending=true;++speech_preview.revision;return true;}
void app_speech_stop(void){speech_preview.state=SPEECH_IDLE;speech_preview.pending=false;++speech_preview.revision;}
void app_speech_snapshot(app_speech_snapshot_t *out){*out=speech_preview;}
static ai_config_snapshot_t ai_preview={.ready=true,.active=AI_PROVIDER_NONE};
static ai_check_snapshot_t check_preview={.provider=AI_PROVIDER_NONE};
void ai_check_snapshot(ai_check_snapshot_t *out){*out=check_preview;}
bool ai_check_request(unsigned p){check_preview.provider=p;check_preview.state=AI_CHECK_QUEUED;++check_preview.revision;return true;}
void ai_config_snapshot(ai_config_snapshot_t *out){*out=ai_preview;}
bool ai_config_submit(const ai_config_request_t *r,unsigned *id){(void)id;ai_preview.active=r->provider;ai_preview.revision++;return true;}
static game_preferences_snapshot_t game_names={.ready=true,.revision=1};
void game_preferences_snapshot(game_preferences_snapshot_t *out){*out=game_names;}
static game_voice_snapshot_t game_voice_preview;
void app_game_voice_snapshot(game_voice_snapshot_t *out){*out=game_voice_preview;}
void app_game_voice_bindings(ai_game_target_t target,app_bindings_t *b){
 if(target.ops && game_voice_preview.state>=GAME_VOICE_STARTING && game_voice_preview.state<=GAME_VOICE_WAITING){
  b->actions[APP_GESTURE_LONG][1]=(app_action_hint_t){0};
  b->actions[APP_GESTURE_DOUBLE][1]=(app_action_hint_t){0};b->actions[APP_GESTURE_SHORT][1]=(app_action_hint_t){0};
  if(game_voice_preview.state==GAME_VOICE_RECORDING)app_actions_register(b,1,APP_GESTURE_SHORT,64001,TXT_GAME_VOICE_SEND,true,APP_ACTION_CONFIRM);
  app_actions_register(b,1,APP_GESTURE_LONG,64002,TXT_GAME_VOICE_CANCEL,true,APP_ACTION_CANCEL);
 }else if(target.ops){app_actions_register(b,1,APP_GESTURE_DOUBLE,64000,TXT_GAME_VOICE,true,APP_ACTION_NAVIGATE);}
}
static unsigned char frame[320*240*3];
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *data) {
    const uint16_t *p=(const uint16_t *)data;
    for(int y=a->y1;y<=a->y2;y++) for(int x=a->x1;x<=a->x2;x++) {
        uint16_t v=*p++; size_t at=(y*320+x)*3;
        if (bsp_display_pixel_outside_rounded_rect(x,y,320,240,30)) v=0;
        frame[at]=((v>>11)&31)*255/31; frame[at+1]=((v>>5)&63)*255/63; frame[at+2]=(v&31)*255/31;
    }
    lv_display_flush_ready(d);
}
static void capture(const char *name) {
    lv_obj_invalidate(lv_screen_active()); lv_refr_now(NULL);
    char path[256];snprintf(path,sizeof(path),"%s.ppm",name);
    FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n320 240\n255\n");
    fwrite(frame,1,sizeof(frame),f);fclose(f);
}
static void advance(unsigned ms) {
    for (unsigned t=0; t<ms; t+=20) { lv_tick_inc(20); lv_timer_handler(); }
}
int main(void) {
    game_preferences_default(&game_names.eight);
    lv_init();lv_display_t *d=lv_display_create(240,320);
    static uint16_t draw[320*40];
    lv_display_set_buffers(d,draw,NULL,sizeof(draw),LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d,flush);
    lv_display_set_rotation(d,LV_DISPLAY_ROTATION_270);
    extern const lv_font_t score_font_16;
    app_shell_t game_shell; app_shell_create(&game_shell, &score_font_16);
    lv_screen_load(game_shell.screen);
    app_header_create(game_shell.screen);
    assert(score_ui_create(game_shell.content));
    score_session_t s;score_session_init(&s);score_controller_t ui;score_controller_init(&ui,&s);
    for (unsigned r = 0; r < 2; ++r) {
        ui.rotation = r ? APP_ROTATION_90 : APP_ROTATION_270;
        const char *suffix = r ? "90" : "270";
        for (unsigned page = 0; page < SCORE_PAGE_COUNT; ++page) {
            ui.page = (score_page_t)page;
            ui.target = 35; ui.settings_focus = 1;
            s.state.target = 35; s.state.points[0] = 10; s.state.points[1] = 9;
            app_bindings_t b; score_controller_bindings(&ui, &s, &b);
            app_shell_render(&game_shell, ui.rotation, &b);
            score_ui_render(&s,&ui,app_content_rect(ui.rotation,app_binding_rows(&b)).height,
                            85,TXT_RESTORED,TXT_SAVING_FAILED,&game_names.eight,false,0,0);
            app_header_render(app_content_rect(ui.rotation,app_binding_rows(&b)).y,TXT_APP_EIGHT,85,false);
            char name[64]; snprintf(name,sizeof(name),"page-%u-%s",page,suffix); capture(name);
        }
    }
    // Long Chinese names use the existing broad font in both landscape layouts.
    snprintf(game_names.eight.players[0],GAME_PLAYER_NAME_CAPACITY,"一二三四五六七八");
    snprintf(game_names.eight.players[1],GAME_PLAYER_NAME_CAPACITY,"张三");
    extern const lv_font_t network_font_14;
    for(uint32_t cp=0x4e00;cp<=0x9fef;++cp){lv_font_glyph_dsc_t glyph={0};assert(lv_font_get_glyph_dsc(&network_font_14,&glyph,cp,0)&&!glyph.is_placeholder);}
    for(unsigned r=0;r<2;++r){
        ui.page=SCORE_PAGE_BOARD;ui.rotation=r?APP_ROTATION_90:APP_ROTATION_270;
        app_bindings_t b;score_controller_bindings(&ui,&s,&b);app_shell_render(&game_shell,ui.rotation,&b);
        score_ui_render(&s,&ui,192,85,TXT_RESTORED,"",&game_names.eight,true,0,1);
        app_header_render(app_content_rect(ui.rotation,app_binding_rows(&b)).y,TXT_APP_EIGHT,85,false);
        capture(r?"named-eight-90":"named-eight-270");
        s.state.points[1]=s.state.target;
        score_ui_render(&s,&ui,192,85,TXT_RESTORED,"",&game_names.eight,false,0,0);
        capture(r?"named-winner-90":"named-winner-270");s.state.points[1]=9;
    }
    // Exercise all nine shared-shell labels with a synthetic future module.
    lv_obj_t *old = lv_screen_active();
    extern const lv_font_t score_font_16;
    app_shell_t shell; app_shell_create(&shell, &score_font_16);
    lv_screen_load(shell.screen); lv_obj_delete(old);
    app_bindings_t bindings = {0};
    for (unsigned g=0; g<APP_GESTURE_COUNT; ++g)
        for (unsigned k=0; k<APP_KEY_COUNT; ++k)
            bindings.actions[g][k] = (app_action_hint_t){TXT_TARGET_BACK, true, 1, APP_ACTION_BACK};
    app_shell_render(&shell, APP_ROTATION_270, &bindings); capture("nine-actions-270");
    app_shell_render(&shell, APP_ROTATION_90, &bindings); capture("nine-actions-90");
    // Render each key container page once per orientation, without stress loops.
    old = lv_screen_active();
    assert(app_ui_create()); lv_obj_delete(old);
    unsigned count; const app_module_t *modules = app_registry(&count);
    assert(count==5);
    for(unsigned i=0;i<count;++i)assert(modules[i].id!=app_module_input_lab.id);
    app_modules_init(modules,count);
    app_container_t app; assert(app_container_init(&app, modules, count, APP_ROTATION_270));
    for (unsigned r=0; r<2; ++r) {
        app.rotation=r ? APP_ROTATION_90 : APP_ROTATION_270;
        app.page=APP_PAGE_HOME; app.focus=0; app_ui_render(&app,85,"");
        capture(r ? "launcher-90" : "launcher-270");
        app.focus=4; app_ui_render(&app,85,""); advance(260); capture(r ? "services-90" : "services-270");
        app.page=APP_PAGE_PLACEHOLDER; app_ui_render(&app,85,"");
        capture(r ? "mock-90" : "mock-270");
    }
    // Real model configuration, QR and listening pages in both orientations.
    for(unsigned r=0;r<2;++r){
        app.rotation=r ? APP_ROTATION_90 : APP_ROTATION_270;app.page=APP_PAGE_HOME;app.focus=4;
        ai_preview=(ai_config_snapshot_t){.ready=true,.active=AI_PROVIDER_NONE,.revision=ai_preview.revision+1};
        strcpy(ai_preview.model,"test-audio-model");strcpy(ai_preview.url,"https://example.com/v1/chat/completions");
        app_container_input(&app,1,APP_GESTURE_SHORT);app_ui_render(&app,91,"");capture(r ? "ai-empty-90" : "ai-empty-270");
        app_container_input(&app,1,APP_GESTURE_SHORT);app_ui_render(&app,91,"");capture(r ? "ai-provider-90" : "ai-provider-270");
        app_container_input(&app,0,APP_GESTURE_SHORT);snprintf(net_preview.ap_name,sizeof(net_preview.ap_name),"Billiards-862C");
        app_container_tick(&app,100);app_ui_render(&app,91,"");capture(r ? "ai-hotspot-90" : "ai-hotspot-270");
        app_container_input(&app,1,APP_GESTURE_SHORT);app_ui_render(&app,91,"");capture(r ? "ai-phone-90" : "ai-phone-270");
        ai_preview.configured[0]=true;ai_preview.active=0;++ai_preview.revision;app_container_tick(&app,200);
        app_container_input(&app,1,APP_GESTURE_LONG);app_ui_render(&app,91,"");capture(r ? "ai-saved-90" : "ai-saved-270");
        check_preview=(ai_check_snapshot_t){.provider=0,.state=AI_CHECK_PASS,.text=true,.tools=true,.audio=true,.revision=check_preview.revision+1};
        app_container_tick(&app,210);app_ui_render(&app,91,"");capture(r ? "ai-check-pass-90" : "ai-check-pass-270");
        check_preview.state=AI_CHECK_FAILED;check_preview.audio=false;check_preview.error=401;++check_preview.revision;app_container_tick(&app,220);app_ui_render(&app,91,"");capture(r ? "ai-check-fail-90" : "ai-check-fail-270");
        check_preview=(ai_check_snapshot_t){.provider=AI_PROVIDER_NONE,.revision=check_preview.revision+1};
        app_container_input(&app,1,APP_GESTURE_LONG);app_ui_render(&app,91,"");
        app_container_input(&app,2,APP_GESTURE_SHORT);app_container_input(&app,1,APP_GESTURE_SHORT);
        app_ui_render(&app,91,"");capture(r ? "voice-ready-90" : "voice-ready-270");
        app_container_input(&app,1,APP_GESTURE_SHORT);assert(speech_preview.state==SPEECH_WAITING);
        app_container_input(&app,1,APP_GESTURE_LONG);app_ui_render(&app,91,"");
        assert(speech_preview.state==SPEECH_IDLE);app_container_input(&app,1,APP_GESTURE_LONG);assert(app.page==APP_PAGE_HOME);
    }
    // One wrap, reversal during motion, then exit mid-animation.
    app.page=APP_PAGE_HOME; app.focus=0; app_ui_render(&app,85,"");
    app_container_input(&app,2,APP_GESTURE_SHORT); app_ui_render(&app,85,"");
    advance(100); capture("carousel-mid");
    advance(160); capture("carousel-chase");
    app_container_input(&app,0,APP_GESTURE_SHORT); app_ui_render(&app,85,"");
    advance(60);
    app_container_input(&app,0,APP_GESTURE_SHORT); app_ui_render(&app,85,"");
    advance(260); capture("carousel-wrap");
    app_container_input(&app,2,APP_GESTURE_SHORT); app_ui_render(&app,85,"");
    app_container_input(&app,1,APP_GESTURE_SHORT); app_ui_render(&app,85,"");
    advance(260); // Deleted cards must not be touched by the old callback.
    app_container_input(&app,1,APP_GESTURE_LONG); app_ui_render(&app,85,"");
    // One real entry/scoring/exit path; no default repetition loop.
    app.page=APP_PAGE_HOME; app.focus=0;
    app_container_input(&app,1,APP_GESTURE_SHORT); app_ui_render(&app,85,"");
    app_container_input(&app,0,APP_GESTURE_SHORT); app_ui_render(&app,85,""); capture("container-eight");
    for(unsigned r=0;r<2;++r){
        app.rotation=r ? APP_ROTATION_90 : APP_ROTATION_270;
        game_voice_preview.state=GAME_VOICE_RECORDING;game_voice_preview.milliseconds=3000;
        app_ui_render(&app,85,"");capture(r ? "game-record-90" : "game-record-270");
        game_voice_preview.state=GAME_VOICE_WAITING;strcpy(game_voice_preview.text,TXT_GAME_VOICE_WAIT);
        app_ui_render(&app,85,"");capture(r ? "game-wait-90" : "game-wait-270");
        game_voice_preview.state=GAME_VOICE_ERROR;strcpy(game_voice_preview.text,TXT_GAME_VOICE_NEED_WIFI);
        app_ui_render(&app,85,"");capture(r ? "game-error-90" : "game-error-270");
    }
    game_voice_preview=(game_voice_snapshot_t){0};
    app_container_input(&app,1,APP_GESTURE_LONG); app_ui_render(&app,85,""); capture("return-home");
    // Settings: finite key-screen coverage and confirmed direction through container.
    app_settings_device_info(91,4093,"cdd94cd-dirty");
    app.page=APP_PAGE_HOME; app.focus=3;
    app_container_input(&app,1,APP_GESTURE_SHORT); app_ui_render(&app,91,"");
    capture("settings-direction");
    app_container_input(&app,1,APP_GESTURE_SHORT);
    app_container_input(&app,2,APP_GESTURE_SHORT);
    app_container_input(&app,1,APP_GESTURE_SHORT); app_ui_render(&app,91,"");
    assert(app.rotation==APP_ROTATION_90);
    app_container_input(&app,2,APP_GESTURE_SHORT);
    app_container_input(&app,1,APP_GESTURE_SHORT); app_ui_render(&app,91,"");capture("settings-brightness-90");
    app_container_input(&app,0,APP_GESTURE_SHORT);
    app_container_input(&app,1,APP_GESTURE_LONG);
    assert(app_settings_preferences()->brightness==80 && app_settings_preview_brightness()==80);
    app_container_input(&app,2,APP_GESTURE_SHORT);
    app_container_input(&app,1,APP_GESTURE_SHORT);
    for(unsigned i=0;i<3;++i)app_container_input(&app,2,APP_GESTURE_SHORT);
    app_ui_render(&app,91,"");capture("settings-infinity-90");
    app_container_input(&app,1,APP_GESTURE_SHORT);
    assert(app_settings_preferences()->dim_seconds==0);
    app_container_input(&app,2,APP_GESTURE_SHORT);app_ui_render(&app,91,"");capture("settings-blank-90");
    app_container_input(&app,2,APP_GESTURE_SHORT);app_ui_render(&app,91,"");capture("settings-info-90");
    app_container_input(&app,1,APP_GESTURE_LONG);
    app.rotation=APP_ROTATION_270; app.focus=3;
    app_container_input(&app,1,APP_GESTURE_SHORT);
    app_container_input(&app,2,APP_GESTURE_SHORT);
    app_container_input(&app,1,APP_GESTURE_SHORT);app_ui_render(&app,91,"");capture("settings-brightness-270");
    app_container_input(&app,1,APP_GESTURE_LONG);
    app_container_input(&app,1,APP_GESTURE_LONG); app_ui_render(&app,91,"");
    // Network settings entry, hotspot details, clear-confirm and both orientations.
    for(unsigned r=0;r<2;++r){
        app.rotation=r ? APP_ROTATION_90 : APP_ROTATION_270;app.page=APP_PAGE_HOME;app.focus=3;
        app_container_input(&app,1,APP_GESTURE_SHORT);
        if(app_settings_preferences()->rotation!=app.rotation){
            app_container_input(&app,1,APP_GESTURE_SHORT);
            app_container_input(&app,2,APP_GESTURE_SHORT);
            app_container_input(&app,1,APP_GESTURE_SHORT);
        }
        assert(app.rotation==(r ? APP_ROTATION_90 : APP_ROTATION_270));
        for(unsigned i=0;i<5;++i)app_container_input(&app,2,APP_GESTURE_SHORT);
        app_ui_render(&app,91,"");capture(r ? "network-summary-offline-90" : "network-summary-offline-270");
        net_preview.state=APP_NET_CONNECTED;
        snprintf(net_preview.ssid,sizeof(net_preview.ssid),"台球室网络");
        snprintf(net_preview.ip,sizeof(net_preview.ip),"192.168.100.125");net_preview.revision++;
        app_container_tick(&app,1800000);app_ui_render(&app,91,"");capture(r ? "network-summary-connected-90" : "network-summary-connected-270");
        snprintf(net_preview.ssid,sizeof(net_preview.ssid),"ABCDEFGHIJKLMNOPQRSTUVWXYZ123456");net_preview.revision++;
        app_container_tick(&app,1900000);app_ui_render(&app,91,"");capture(r ? "network-summary-long-90" : "network-summary-long-270");
        app_container_input(&app,0,APP_GESTURE_SHORT);app_ui_render(&app,91,"");capture(r ? "network-summary-to-info-90" : "network-summary-to-info-270");
        app_container_input(&app,2,APP_GESTURE_SHORT);app_ui_render(&app,91,"");capture(r ? "network-summary-return-90" : "network-summary-return-270");
        net_preview.state=APP_NET_OFFLINE;net_preview.ssid[0]=0;net_preview.ip[0]=0;net_preview.revision++;
        app_container_tick(&app,1950000);
        app_container_input(&app,1,APP_GESTURE_SHORT);app_ui_render(&app,91,"");
        capture(r ? "network-offline-90" : "network-offline-270");
        net_preview.portal=true;net_preview.saved=true;net_preview.seconds_left=280;
        snprintf(net_preview.ap_name,sizeof(net_preview.ap_name),"Billiards-DEMO");
        net_preview.revision++;
        app_container_tick(&app,2000000);app_ui_render(&app,91,"");capture(r ? "network-hotspot-90" : "network-hotspot-270");
        net_preview.state=APP_NET_CONNECTING;net_preview.busy=true;net_preview.attempt=2;net_preview.attempt_seconds=18;net_preview.revision++;
        app_container_tick(&app,2100000);app_ui_render(&app,91,"");capture(r ? "network-connecting-90" : "network-connecting-270");
        net_preview.associated=true;net_preview.revision++;
        app_container_tick(&app,2200000);app_ui_render(&app,91,"");capture(r ? "network-ip-wait-90" : "network-ip-wait-270");
        net_preview.state=APP_NET_CONNECTED;net_preview.busy=false;snprintf(net_preview.ssid,sizeof(net_preview.ssid),"台球室网络");snprintf(net_preview.ip,sizeof(net_preview.ip),"192.168.100.125");net_preview.revision++;
        app_container_tick(&app,2300000);app_ui_render(&app,91,"");capture(r ? "network-success-90" : "network-success-270");
        net_preview.save_error=1;net_preview.revision++;
        app_container_tick(&app,2400000);app_ui_render(&app,91,"");capture(r ? "network-save-failed-90" : "network-save-failed-270");
        net_preview.state=APP_NET_FAILED;net_preview.reason=202;net_preview.error=1;net_preview.save_error=0;net_preview.ip[0]=0;net_preview.revision++;
        app_container_tick(&app,2500000);app_ui_render(&app,91,"");capture(r ? "network-auth-failed-90" : "network-auth-failed-270");
        app_container_input(&app,1,APP_GESTURE_SHORT);app_container_tick(&app,2600000);app_ui_render(&app,91,"");capture(r ? "network-list-90" : "network-list-270");
        net_preview.state=APP_NET_OFFLINE;net_preview.revision++;app_container_tick(&app,2700000);
        app_container_input(&app,1,APP_GESTURE_SHORT);app_ui_render(&app,91,"");capture(r ? "network-keyboard-90" : "network-keyboard-270");
        // Use the editor's visible menu to cancel, preserving keyboard behavior.
        app_container_input(&app,1,APP_GESTURE_DOUBLE);
        app_container_input(&app,2,APP_GESTURE_LONG);app_container_input(&app,2,APP_GESTURE_SHORT);
        app_container_input(&app,1,APP_GESTURE_SHORT);app_container_input(&app,1,APP_GESTURE_SHORT);
        app_ui_render(&app,91,"");
        app_container_input(&app,2,APP_GESTURE_SHORT);app_container_input(&app,1,APP_GESTURE_SHORT);
        app_ui_render(&app,91,"");capture(r ? "network-connect-confirm-90" : "network-connect-confirm-270");
        app_container_input(&app,0,APP_GESTURE_SHORT);
        app_container_input(&app,1,APP_GESTURE_LONG);
        for(unsigned i=0;i<3;++i)app_container_input(&app,2,APP_GESTURE_SHORT);
        app_container_input(&app,1,APP_GESTURE_SHORT);app_ui_render(&app,91,"");capture(r ? "network-forget-90" : "network-forget-270");
        app_container_input(&app,0,APP_GESTURE_SHORT);app_container_input(&app,1,APP_GESTURE_LONG);
        app_container_input(&app,1,APP_GESTURE_LONG);net_preview.portal=false;net_preview.state=APP_NET_OFFLINE;net_preview.associated=false;net_preview.reason=0;net_preview.error=0;net_preview.revision++;
    }
    // The lab is a host-only fixture, never registered in the production hall.
    const app_module_t lab_modules[]={app_module_input_lab};
    app_modules_init(lab_modules,1);
    assert(app_container_init(&app,lab_modules,1,APP_ROTATION_270));
    // Bounded QWERTY sessions: all modes/menu/confirmation, both rotations.
    for(unsigned r=0;r<2;++r){
        app.rotation=r ? APP_ROTATION_90 : APP_ROTATION_270;
        app.page=APP_PAGE_HOME;app.focus=0;
        app_container_input(&app,1,APP_GESTURE_SHORT);app_ui_render(&app,91,"");
        app_container_tick(&app,1000000);
        app_container_input(&app,1,APP_GESTURE_SHORT);app_ui_render(&app,91,"");
        assert(lv_display_get_rotation(d)==(r ? LV_DISPLAY_ROTATION_90 : LV_DISPLAY_ROTATION_270));
        capture(r ? "qwerty-90" : "qwerty-270");
        app_container_input(&app,0,APP_GESTURE_DOUBLE);app_ui_render(&app,91,"");
        capture(r ? "qwerty-upper-90" : "qwerty-upper-270");
        app_container_input(&app,1,APP_GESTURE_SHORT);
        app_container_input(&app,2,APP_GESTURE_DOUBLE);app_ui_render(&app,91,"");
        capture(r ? "qwerty-digits-90" : "qwerty-digits-270");
        app_container_input(&app,2,APP_GESTURE_DOUBLE);app_ui_render(&app,91,"");
        capture(r ? "qwerty-symbols-90" : "qwerty-symbols-270");
        app_container_input(&app,1,APP_GESTURE_DOUBLE);app_ui_render(&app,91,"");
        capture(r ? "qwerty-menu-90" : "qwerty-menu-270");
        app_container_input(&app,1,APP_GESTURE_SHORT);app_ui_render(&app,91,"");
        capture(r ? "qwerty-cursor-90" : "qwerty-cursor-270");
        app_container_input(&app,1,APP_GESTURE_SHORT);
        app_container_input(&app,1,APP_GESTURE_DOUBLE);
        app_container_input(&app,2,APP_GESTURE_SHORT);app_container_input(&app,2,APP_GESTURE_SHORT);
        app_container_input(&app,1,APP_GESTURE_SHORT);app_ui_render(&app,91,"");
        capture(r ? "qwerty-clear-90" : "qwerty-clear-270");
        app_container_input(&app,0,APP_GESTURE_SHORT);
        app_container_input(&app,2,APP_GESTURE_LONG); // Clear -> Exit (nearest column).
        app_container_input(&app,1,APP_GESTURE_SHORT);app_ui_render(&app,91,"");
        capture(r ? "qwerty-exit-90" : "qwerty-exit-270");
        app_container_input(&app,1,APP_GESTURE_SHORT);app_ui_render(&app,91,"");
        capture(r ? "qwerty-result-90" : "qwerty-result-270");
        app_container_input(&app,1,APP_GESTURE_LONG);assert(app.page==APP_PAGE_HOME);
    }
    lv_mem_monitor_t memory; lv_mem_monitor(&memory);
    printf("LVGL max used: %u, remaining: %u, key screens rendered\n",
           (unsigned)memory.max_used,(unsigned)memory.free_size);
}
