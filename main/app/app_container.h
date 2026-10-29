#pragma once
#include "app_module.h"

typedef enum { APP_PAGE_HOME, APP_PAGE_MODULE, APP_PAGE_PLACEHOLDER } app_page_t;
typedef struct {
    const app_module_t *modules;
    unsigned count, focus;
    app_page_t page;
    app_rotation_t rotation;
} app_container_t;

bool app_container_init(app_container_t *app, const app_module_t *modules,
                         unsigned count, app_rotation_t rotation);
const app_module_t *app_container_active(const app_container_t *app);
void app_container_bindings(const app_container_t *app, app_bindings_t *bindings);
void app_container_input(app_container_t *app, unsigned slot, app_gesture_t gesture);
/* Tick every registered module, including inactive modules with pending saves. */
bool app_container_tick(app_container_t *app, int64_t now);
void app_modules_init(const app_module_t *modules, unsigned count);
