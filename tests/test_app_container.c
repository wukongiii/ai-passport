#include "app/app_container.h"
#include <assert.h>
#include <stdio.h>

typedef struct { unsigned enters, leaves, inputs, ticks; bool allow_enter, allow_leave, subpage; } fake_t;
static bool enter(void *c, app_rotation_t rotation) {
    fake_t *f = c; ++f->enters; assert(app_rotation_valid(rotation)); return f->allow_enter;
}
static bool leave(void *c) { fake_t *f = c; ++f->leaves; return f->allow_leave; }
static void bindings(void *c, app_bindings_t *b) {
    fake_t *f = c; app_actions_clear(b);
    app_actions_register(b, 0, APP_GESTURE_SHORT, 1, "A +1", true, APP_ACTION_INCREASE);
    if (f->subpage) app_actions_register(b, 1, APP_GESTURE_LONG, 2, "Back", true, APP_ACTION_BACK);
}
static app_module_effect_t input(void *c, app_rotation_t r, unsigned slot, app_gesture_t g) {
    fake_t *f = c; ++f->inputs;
    if (g == APP_GESTURE_LONG) f->subpage = false;
    assert(slot < 3);
    return (app_module_effect_t){true, r == APP_ROTATION_270 ? APP_ROTATION_90 : APP_ROTATION_270};
}
static bool tick(void *c, int64_t now) { fake_t *f = c; ++f->ticks; assert(now == 42); return true; }
static bool create(void *c, void *p) { (void)c; (void)p; return true; }
static void render(void *c, const app_module_view_t *v) { (void)c; (void)v; }
static const app_module_ops_t ops = {enter, leave, bindings, input, tick, create, render, NULL};
int main(void) {
    fake_t f = {.allow_enter = true, .allow_leave = true};
    const app_module_t modules[] = {
        {1, "Eight", "real game", APP_ENTRY_GAME, &ops, &f, APP_ART_EIGHT},
        {2, "Chase", "mock", APP_ENTRY_GAME, NULL, NULL, APP_ART_CHASE},
        {3, "Settings", "mock", APP_ENTRY_SETTINGS, NULL, NULL, APP_ART_SETTINGS},
        {4, "Voice", "mock", APP_ENTRY_SERVICE, NULL, NULL, APP_ART_VOICE},
    };
    app_container_t app;
    assert(!app_container_init(&app, modules, 0, APP_ROTATION_270));
    app_module_t duplicates[] = {modules[0], modules[0]};
    assert(!app_container_init(&app, duplicates, 2, APP_ROTATION_270));
    assert(app_container_init(&app, modules, 4, APP_ROTATION_270));
    assert(app.page == APP_PAGE_HOME && !app_container_active(&app) && !f.enters);
    app_container_input(&app, 0, APP_GESTURE_SHORT);
    assert(app.focus == 3);
    app_container_input(&app, 2, APP_GESTURE_SHORT);
    assert(app.focus == 0);
    app_container_input(&app, 1, APP_GESTURE_DOUBLE);
    assert(!f.enters);
    f.allow_enter = false;
    app_container_input(&app, 1, APP_GESTURE_SHORT);
    assert(app.page == APP_PAGE_HOME);
    f.allow_enter = true;
    app_container_input(&app, 1, APP_GESTURE_SHORT);
    assert(app.page == APP_PAGE_MODULE && app_container_active(&app) == &modules[0]);
    app_container_input(&app, 0, APP_GESTURE_SHORT);
    assert(f.inputs == 1 && app.rotation == APP_ROTATION_90);
    f.subpage = true;
    app_container_input(&app, 1, APP_GESTURE_LONG);
    assert(!f.subpage && app.page == APP_PAGE_MODULE && f.inputs == 2);
    f.allow_leave = false;
    app_container_input(&app, 1, APP_GESTURE_LONG);
    assert(app.page == APP_PAGE_MODULE);
    f.allow_leave = true;
    app_container_input(&app, 1, APP_GESTURE_LONG);
    assert(app.page == APP_PAGE_HOME);
    // Placeholder navigation must never enter/call the game or alter settings.
    const unsigned inputs = f.inputs, enters = f.enters;
    const app_rotation_t rotation = app.rotation;
    for (unsigned i = 1; i < 4; ++i) {
        app_container_input(&app, 2, APP_GESTURE_SHORT);
        assert(app.focus == i);
        app_container_input(&app, 1, APP_GESTURE_SHORT);
        assert(app.page == APP_PAGE_PLACEHOLDER);
        app_container_input(&app, 0, APP_GESTURE_SHORT);
        assert(app.page == APP_PAGE_PLACEHOLDER);
        app_container_input(&app, 1, APP_GESTURE_SHORT);
        assert(app.page == APP_PAGE_HOME && app.focus == i);
    }
    assert(f.inputs == inputs && f.enters == enters && app.rotation == rotation);
    app_container_tick(&app, 42);
    assert(f.ticks == 1); // Inactive modules still finish saves.
    puts("Container registration, routing, lifecycle, mock isolation and inactive tick: PASS");
}
