#pragma once
#include <stddef.h>
#include "../ui/app_actions.h"
#include "../ui/app_layout.h"

/* Owner-task interface; no ESP-IDF or LVGL dependency. UI calls require the
 * LVGL lock; enter/leave/input/tick must not access widgets. leave must stop
 * producers before returning true, otherwise navigation is refused.
 * Module action IDs must be below 65000 (higher IDs are container-reserved). */
typedef struct {
    app_rotation_t rotation;
    int content_height, battery;
    const char *storage_overlay;
} app_module_view_t;
typedef struct { bool rotation_changed; app_rotation_t rotation; } app_module_effect_t;
typedef struct {
    bool (*enter)(void *context, app_rotation_t rotation);
    bool (*leave)(void *context);
    void (*bindings)(void *context, app_bindings_t *bindings);
    app_module_effect_t (*input)(void *context, app_rotation_t rotation,
                                 unsigned slot, app_gesture_t gesture);
    bool (*tick)(void *context, int64_t now); // true when visible state changed
    bool (*ui_create)(void *context, void *content);
    void (*ui_render)(void *context, const app_module_view_t *view);
    void (*init)(void *context); // optional owner startup, outside UI lock
    void (*key)(void *context, unsigned slot, bool pressed, int64_t now); // optional raw edges, owner task
} app_module_ops_t;
typedef enum { APP_ENTRY_GAME, APP_ENTRY_SETTINGS, APP_ENTRY_SERVICE } app_entry_kind_t;
typedef enum { APP_ART_EIGHT, APP_ART_CHASE, APP_ART_SNOOKER, APP_ART_SETTINGS, APP_ART_VOICE } app_art_t;
typedef struct {
    uint16_t id;
    const char *title, *description;
    app_entry_kind_t kind;
    const app_module_ops_t *ops; // NULL: explicit UI-only placeholder.
    void *context;
    app_art_t artwork; // Presentation metadata, independent of LVGL.
} app_module_t;

const app_module_t *app_registry(unsigned *count);
