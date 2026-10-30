#include "app_preferences.h"
app_preferences_t app_preferences_default(void) {
    return (app_preferences_t){APP_ROTATION_270, 80, 30, 120, APP_VOLUME_DEFAULT};
}
bool app_preferences_valid(const app_preferences_t *p) {
    if (!p || !app_rotation_valid(p->rotation) || p->brightness < 20 || p->brightness > 100 || p->brightness % 10 || p->volume>100 || p->volume%APP_VOLUME_STEP) return false;
    const bool dim = p->dim_seconds == 0 || p->dim_seconds == 15 || p->dim_seconds == 30 || p->dim_seconds == 60 || p->dim_seconds == 120;
    const bool blank = p->blank_seconds == 0 || p->blank_seconds == 60 || p->blank_seconds == 120 || p->blank_seconds == 300 || p->blank_seconds == 600;
    return dim && blank;
}
void app_preferences_encode(const app_preferences_t *p, uint8_t b[APP_PREFERENCES_SIZE]) {
    for (unsigned i=0;i<APP_PREFERENCES_SIZE;++i) b[i]=0;
    b[0]='A'; b[1]='P'; b[2]=2; b[3]=p->rotation == APP_ROTATION_90 ? 1 : 3;
    b[4]=p->brightness; b[5]=p->dim_seconds; b[6]=p->dim_seconds>>8;
    b[9]=p->volume;
    b[7]=p->blank_seconds; b[8]=p->blank_seconds>>8;
    for (unsigned i=0;i<15;++i) b[15]^=b[i];
}
bool app_preferences_decode(app_preferences_t *p, const uint8_t *b, unsigned size) {
    if (!p || !b || size != APP_PREFERENCES_SIZE || b[0]!='A' || b[1]!='P' || (b[2]!=1 && b[2]!=2) || (b[3]!=1 && b[3]!=3)) return false;
    uint8_t sum=0; for(unsigned i=0;i<16;++i) sum^=b[i];
    for(unsigned i=b[2]==1 ? 9 : 10;i<15;++i) if(b[i]) return false;
    app_preferences_t v={b[3]==1 ? APP_ROTATION_90 : APP_ROTATION_270,b[4],b[5]|b[6]<<8,b[7]|b[8]<<8,b[2]==1 ? APP_VOLUME_DEFAULT : b[9]};
    if(sum || !app_preferences_valid(&v)) return false;
    *p=v; return true;
}
uint8_t app_preferences_backlight(const app_preferences_t *p, int64_t idle) {
    if(p->blank_seconds && idle >= (int64_t)p->blank_seconds*1000000) return 0;
    if(p->dim_seconds && idle >= (int64_t)p->dim_seconds*1000000) return 15;
    return p->brightness;
}
