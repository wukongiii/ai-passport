#include "app_container.h"
#include "app_strings.h"

enum { ACTION_PREVIOUS = 65000, ACTION_OPEN, ACTION_NEXT, ACTION_HOME };

bool app_container_init(app_container_t *app, const app_module_t *modules,
                         unsigned count, app_rotation_t rotation) {
    if (!app || !modules || !count || !app_rotation_valid(rotation)) return false;
    for (unsigned i = 0; i < count; ++i) {
        const app_module_t *m = &modules[i];
        if (!m->id || m->id >= ACTION_PREVIOUS || !m->title || !m->description) return false;
        if (m->ops && (!m->ops->enter || !m->ops->leave || !m->ops->bindings ||
            !m->ops->input || !m->ops->ui_create || !m->ops->ui_render)) return false;
        for (unsigned j = 0; j < i; ++j) if (modules[j].id == m->id) return false;
    }
    *app = (app_container_t){ .modules = modules, .count = count, .rotation = rotation };
    return true;
}

const app_module_t *app_container_active(const app_container_t *app) {
    return app->page == APP_PAGE_HOME ? NULL : &app->modules[app->focus];
}

void app_container_bindings(const app_container_t *app, app_bindings_t *b) {
    app_actions_clear(b);
    if (app->page == APP_PAGE_HOME) {
        app_actions_register(b, 0, APP_GESTURE_SHORT, ACTION_PREVIOUS, TXT_APP_PREVIOUS, true, APP_ACTION_NAVIGATE);
        app_actions_register(b, 1, APP_GESTURE_SHORT, ACTION_OPEN, TXT_APP_ENTER, true, APP_ACTION_CONFIRM);
        app_actions_register(b, 2, APP_GESTURE_SHORT, ACTION_NEXT, TXT_APP_NEXT, true, APP_ACTION_NAVIGATE);
    } else if (app->page == APP_PAGE_PLACEHOLDER) {
        app_actions_register(b, 1, APP_GESTURE_SHORT, ACTION_HOME, TXT_APP_BACK, true, APP_ACTION_BACK);
        app_actions_register(b, 1, APP_GESTURE_LONG, ACTION_HOME, TXT_APP_BACK, true, APP_ACTION_BACK);
    } else {
        const app_module_t *m = app_container_active(app);
        m->ops->bindings(m->context, b);
        // Games retain their subpage Back. A free middle hold returns home.
        if (!b->actions[APP_GESTURE_LONG][1].id)
            app_actions_register(b, 1, APP_GESTURE_LONG, ACTION_HOME, TXT_APP_HOME, true, APP_ACTION_BACK);
    }
}

void app_container_input(app_container_t *app, unsigned slot, app_gesture_t gesture) {
    app_bindings_t b;
    app_container_bindings(app, &b);
    const unsigned action = app_actions_resolve(&b, slot, gesture);
    if (!action) return;
    if (app->page == APP_PAGE_HOME) {
        if (action == ACTION_PREVIOUS) app->focus = (app->focus + app->count - 1) % app->count;
        else if (action == ACTION_NEXT) app->focus = (app->focus + 1) % app->count;
        else if (action == ACTION_OPEN) {
            const app_module_t *m = &app->modules[app->focus];
            if (!m->ops) app->page = APP_PAGE_PLACEHOLDER;
            else if (m->ops->enter(m->context, app->rotation)) { app->page = APP_PAGE_MODULE; ++app->activation; }
        }
    } else if (action == ACTION_HOME) {
        const app_module_t *m = app_container_active(app);
        if (!m->ops || m->ops->leave(m->context)) { app->page = APP_PAGE_HOME; ++app->activation; }
    } else if (app->page == APP_PAGE_MODULE) {
        const app_module_t *m = app_container_active(app);
        const app_module_effect_t effect = m->ops->input(m->context, app->rotation, slot, gesture);
        if (effect.rotation_changed && app_rotation_valid(effect.rotation)) app->rotation = effect.rotation;
    }
}

bool app_container_tick(app_container_t *app, int64_t now) {
    bool changed = false;
    for (unsigned i = 0; i < app->count; ++i) {
        const app_module_t *m = &app->modules[i];
        if (m->ops && m->ops->tick && m->ops->tick(m->context, now)) changed = true;
    }
    return changed;
}

void app_modules_init(const app_module_t *modules, unsigned count) {
    for (unsigned i = 0; i < count; ++i)
        if (modules[i].ops && modules[i].ops->init) modules[i].ops->init(modules[i].context);
}

ai_game_target_t app_container_ai_target(const app_container_t *app) {
    if(!app || app->page!=APP_PAGE_MODULE)return (ai_game_target_t){0};
    const app_module_t *m=app_container_active(app);
    if(!m || m->kind!=APP_ENTRY_GAME || !m->ai)return (ai_game_target_t){0};
    return (ai_game_target_t){m->ai,m->context,m->id,app->activation};
}
