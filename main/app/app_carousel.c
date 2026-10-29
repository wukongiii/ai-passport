#include "app_carousel.h"
#include "app_carousel_layout.h"

LV_FONT_DECLARE(score_font_16);
LV_IMAGE_DECLARE(launcher_eight);
LV_IMAGE_DECLARE(launcher_chase);
LV_IMAGE_DECLARE(launcher_snooker);
LV_IMAGE_DECLARE(launcher_settings);
LV_IMAGE_DECLARE(launcher_voice);

typedef struct { lv_obj_t *box, *image, *title; unsigned index; } card_t;
static struct {
    const app_module_t *modules;
    unsigned count, center, target;
    bool mounted;
    int32_t pan;
    card_t cards[5];
} carousel;

static const lv_image_dsc_t *art(const app_module_t *m) {
    switch (m->artwork) {
        case APP_ART_CHASE: return &launcher_chase;
        case APP_ART_SNOOKER: return &launcher_snooker;
        case APP_ART_SETTINGS: return &launcher_settings;
        case APP_ART_VOICE: return &launcher_voice;
        default: return &launcher_eight;
    }
}

static void position(void *unused, int32_t pan) {
    (void)unused;
    carousel.pan = pan;
    for (int i = 0; i < 5; ++i) {
        card_t *c = &carousel.cards[i];
        const app_card_layout_t p = app_carousel_layout((i - 2) * 1024 - pan);
        lv_obj_set_pos(c->box, p.x, p.y);
        lv_obj_set_size(c->box, p.width, p.height);
        lv_obj_set_style_opa(c->box, p.opacity, 0);
        lv_image_set_scale(c->image, p.image_scale);
        lv_obj_set_pos(c->image, (p.width - 176 * p.image_scale / 256) / 2, 7);
        lv_obj_set_pos(c->title, 2, 11 + 96 * p.image_scale / 256);
        lv_obj_set_width(c->title, p.width - 4);
        const int distance = (i - 2) * 1024 - pan;
        const bool focused = distance > -512 && distance < 512;
        lv_obj_set_style_border_color(c->box, lv_color_hex(focused ? 0xB8D391 : 0x304752), 0);
        lv_obj_set_style_bg_color(c->box, lv_color_hex(focused ? 0x233D42 : 0x192B37), 0);
    }
}

static void bind(unsigned focus) {
    carousel.center = focus;
    carousel.target = focus;
    for (int i = 0; i < 5; ++i) {
        card_t *c = &carousel.cards[i];
        c->index = (unsigned)(((int64_t)focus + 2 * carousel.count + i - 2) % carousel.count);
        lv_image_set_src(c->image, art(&carousel.modules[c->index]));
        lv_label_set_text(c->title, carousel.modules[c->index].title);
        // Small registries must not duplicate the same entry in adjacent slots.
        const bool show = i == 2 || (carousel.count > 1 && i == 3) ||
            (carousel.count > 2 && i == 1) || (carousel.count > 3 && i == 4) ||
            (carousel.count > 4 && i == 0);
        if (show) lv_obj_remove_flag(c->box, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(c->box, LV_OBJ_FLAG_HIDDEN);
    }
    position(NULL, 0);
    lv_obj_move_foreground(carousel.cards[2].box);
}

static void completed(lv_anim_t *animation) {
    (void)animation;
    bind(carousel.target);
}

void app_carousel_destroy(void) {
    lv_anim_delete(&carousel, position);
    carousel.mounted = false;
}

void app_carousel_create(lv_obj_t *parent, const app_module_t *modules, unsigned count, unsigned focus) {
    app_carousel_destroy();
    carousel.modules = modules; carousel.count = count; carousel.mounted = true;
    for (unsigned i = 0; i < 5; ++i) {
        card_t *c = &carousel.cards[i];
        c->box = lv_obj_create(parent);
        lv_obj_remove_style_all(c->box);
        lv_obj_remove_flag(c->box, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(c->box, 12, 0);
        lv_obj_set_style_bg_opa(c->box, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(c->box, 1, 0);
        c->image = lv_image_create(c->box);
        lv_image_set_pivot(c->image, 0, 0);
        lv_image_set_antialias(c->image, true);
        c->title = lv_label_create(c->box);
        lv_obj_set_style_text_font(c->title, &score_font_16, 0);
        lv_obj_set_style_text_color(c->title, lv_color_hex(0xF2F3E9), 0);
        lv_obj_set_style_text_align(c->title, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(c->title, LV_LABEL_LONG_CLIP);
    }
    bind(focus);
}

void app_carousel_select(unsigned focus) {
    if (!carousel.mounted || focus >= carousel.count || focus == carousel.target) return;
    // Rebase the current visual offset onto the previous target, then retarget.
    // This preserves motion on rapid reversals without an animation queue.
    lv_anim_delete(&carousel, position);
    const unsigned previous = carousel.target;
    const int32_t start = carousel.pan - 1024 *
        app_carousel_direction(carousel.center, previous, carousel.count);
    bind(previous);
    const int direction = app_carousel_direction(previous, focus, carousel.count);
    carousel.target = focus;
    if (!direction || carousel.count < 3) { bind(focus); return; }
    lv_obj_move_foreground(carousel.cards[2 + direction].box);
    lv_anim_t animation; lv_anim_init(&animation);
    lv_anim_set_var(&animation, &carousel);
    lv_anim_set_exec_cb(&animation, position);
    lv_anim_set_values(&animation, start, direction * 1024);
    lv_anim_set_duration(&animation, 220);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_in_out);
    lv_anim_set_completed_cb(&animation, completed);
    lv_anim_start(&animation);
}
