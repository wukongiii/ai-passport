#include "app_module.h"
#include "app_strings.h"
#include "../games/eight_module.h"

const app_module_t *app_registry(unsigned *count) {
    // One central list. New games supply a descriptor; placeholders have no ops.
    static app_module_t modules[5];
    if (!modules[0].id) {
        modules[0] = app_module_eight;
        modules[1] = (app_module_t){2, TXT_APP_CHASE, TXT_APP_CHASE_DESC, APP_ENTRY_GAME, NULL, NULL, APP_ART_CHASE};
        modules[2] = (app_module_t){3, TXT_APP_SNOOKER, TXT_APP_SNOOKER_DESC, APP_ENTRY_GAME, NULL, NULL, APP_ART_SNOOKER};
        modules[3] = (app_module_t){4, TXT_APP_SETTINGS, TXT_APP_SETTINGS_DESC, APP_ENTRY_SETTINGS, NULL, NULL, APP_ART_SETTINGS};
        modules[4] = (app_module_t){5, TXT_APP_VOICE, TXT_APP_VOICE_DESC, APP_ENTRY_SERVICE, NULL, NULL, APP_ART_VOICE};
    }
    *count = sizeof(modules) / sizeof(modules[0]);
    return modules;
}
