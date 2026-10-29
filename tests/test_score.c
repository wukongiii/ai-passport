#include "score/score.h"
#include "score/score_config.h"
#include "score/score_controller.h"
#include "score/score_input.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static score_command_t command(score_command_type_t type) {
    return (score_command_t){ .type = type, .amount = 1, .expected_revision = SCORE_ANY_REVISION };
}

static void rules_and_history(void) {
    score_session_t s;
    score_session_init(&s);
    s.state.target = 5; // Explicit small race for winner/history boundary checks.
    assert(score_state_valid(&s.state) && score_winner(&s.state) == -1);
    score_command_t c = command(SCORE_CMD_AWARD_FRAME);
    for (int i = 0; i < 4; ++i) assert(score_dispatch(&s, &c) == SCORE_OK);
    c.player = 1;
    assert(score_dispatch(&s, &c) == SCORE_OK);
    c.player = 0;
    assert(score_dispatch(&s, &c) == SCORE_OK && score_winner(&s.state) == 0);
    const score_session_t finished = s;
    assert(score_dispatch(&s, &c) == SCORE_FINISHED);
    assert(!memcmp(&s, &finished, sizeof(s)));
    c = command(SCORE_CMD_UNDO);
    assert(score_dispatch(&s, &c) == SCORE_OK && score_winner(&s.state) == -1);
    assert(s.state.points[0] == 4 && s.state.points[1] == 1);
    c = (score_command_t){ .type = SCORE_CMD_NEW_MATCH, .rule_id = SCORE_RULE_CHINESE_EIGHT,
        .player_count = 2, .target = 3, .expected_revision = s.revision };
    assert(score_dispatch(&s, &c) == SCORE_OK && s.state.points[0] == 0);
    assert(score_dispatch(&s, &c) == SCORE_STALE); // Delayed voice command cannot apply twice.
    c = command(SCORE_CMD_UNDO);
    assert(score_dispatch(&s, &c) == SCORE_OK && s.state.target == 5 && s.state.points[0] == 4);
    while (s.history_count) assert(score_dispatch(&s, &c) == SCORE_OK);
    assert(s.state.points[0] == 0 && s.state.points[1] == 0);
    assert(score_dispatch(&s, &c) == SCORE_NO_HISTORY);
    c = command(SCORE_CMD_ADJUST_POINTS);
    c.amount = -1;
    assert(score_dispatch(&s, &c) == SCORE_INVALID); // zero cannot go negative
    c.amount = 2;
    assert(score_dispatch(&s, &c) == SCORE_INVALID);
    c = command(SCORE_CMD_AWARD_FRAME);
    c.player = 2;
    assert(score_dispatch(&s, &c) == SCORE_INVALID);
    c.player = 0; c.amount = -1;
    assert(score_dispatch(&s, &c) == SCORE_INVALID);
    c = (score_command_t){ .type = SCORE_CMD_NEW_MATCH, .rule_id = 1,
        .player_count = 2, .target = 0, .expected_revision = SCORE_ANY_REVISION };
    assert(score_dispatch(&s, &c) == SCORE_INVALID);
    c.target = 36;
    assert(score_dispatch(&s, &c) == SCORE_INVALID);
    c.target = 5; c.player_count = 3;
    assert(score_dispatch(&s, &c) == SCORE_INVALID);
    c.player_count = 2; c.rule_id = 99;
    assert(score_dispatch(&s, &c) == SCORE_INVALID);
    for (unsigned i = 0; i < 40; ++i) {
        c = (score_command_t){ .type = SCORE_CMD_NEW_MATCH, .rule_id = 1,
            .player_count = 2, .target = (uint8_t)(i % 35 + 1), .expected_revision = SCORE_ANY_REVISION };
        assert(score_dispatch(&s, &c) == SCORE_OK);
    }
    assert(s.history_count == SCORE_HISTORY_CAPACITY);
    c = command(SCORE_CMD_UNDO);
    for (unsigned i = 0; i < SCORE_HISTORY_CAPACITY; ++i) assert(score_dispatch(&s, &c) == SCORE_OK);
    assert(score_dispatch(&s, &c) == SCORE_NO_HISTORY);
}

static void storage(void) {
    score_session_t original, loaded;
    score_session_init(&original);
    score_command_t c = command(SCORE_CMD_AWARD_FRAME);
    assert(score_dispatch(&original, &c) == SCORE_OK);
    uint8_t record[SCORE_RECORD_SIZE];
    assert(score_encode(&original, record, sizeof(record)));
    assert(!memcmp(record, "SCOR", 4) && record[4] == 1 && record[16] == 1);
    assert(score_decode(&loaded, record, sizeof(record)));
    assert(loaded.state.points[0] == 1 && loaded.revision == 1 && loaded.history_count == 1);
    c = command(SCORE_CMD_UNDO);
    assert(score_dispatch(&loaded, &c) == SCORE_OK && loaded.state.points[0] == 0);
    for (size_t i = 0; i < sizeof(record); ++i) {
        record[i] ^= 0x40;
        const score_session_t before = loaded;
        assert(!score_decode(&loaded, record, sizeof(record)));
        assert(!memcmp(&loaded, &before, sizeof(loaded))); // Failed restore is atomic.
        record[i] ^= 0x40;
    }
    assert(!score_decode(&loaded, record, sizeof(record) - 1));
    original.state.player_count = 3;
    assert(!score_encode(&original, record, sizeof(record)));
    original.state.player_count = 2;
    original.state.points[0] = -1;
    assert(!score_encode(&original, record, sizeof(record)));
    for (unsigned target = 1; target <= 35; ++target) {
        score_session_init(&original);
        original.state.target = (uint8_t)target;
        c = command(SCORE_CMD_AWARD_FRAME);
        for (unsigned i = 0; i < target; ++i) assert(score_dispatch(&original, &c) == SCORE_OK);
        assert(score_encode(&original, record, sizeof(record)));
        assert(score_decode(&loaded, record, sizeof(record)));
        assert(score_winner(&loaded.state) == 0);
    }
}

static bool press(score_controller_t *ui, score_session_t *s, unsigned slot, app_gesture_t g) {
    score_command_t c;
    if (!score_controller_input(ui, s, slot, g, &c)) return false;
    assert(c.expected_revision == s->revision);
    assert(score_dispatch(s, &c) == SCORE_OK);
    return true;
}

static void controls(void) {
    score_session_t s; score_session_init(&s);
    score_controller_t ui; score_controller_init(&ui, &s);
    assert(press(&ui, &s, 0, APP_GESTURE_SHORT));
    assert(s.state.points[0] == 1 && ui.page == SCORE_PAGE_BOARD);
    assert(press(&ui, &s, 2, APP_GESTURE_SHORT));
    assert(s.state.points[1] == 1 && ui.page == SCORE_PAGE_BOARD);
    assert(press(&ui, &s, 0, APP_GESTURE_LONG));
    assert(s.state.points[0] == 0 && s.state.points[1] == 1);
    assert(!press(&ui, &s, 0, APP_GESTURE_LONG));
    assert(press(&ui, &s, 2, APP_GESTURE_LONG));
    assert(s.state.points[1] == 0);
    const score_session_t before = s;
    assert(!press(&ui, &s, 0, APP_GESTURE_DOUBLE));
    assert(!press(&ui, &s, 1, APP_GESTURE_LONG));
    assert(!memcmp(&s, &before, sizeof(s)));
    // Middle opens settings; left/right navigate; middle executes selection.
    assert(!press(&ui, &s, 1, APP_GESTURE_SHORT) && ui.page == SCORE_PAGE_SETTINGS);
    assert(!press(&ui, &s, 1, APP_GESTURE_SHORT) && ui.page == SCORE_PAGE_TARGET);
    ui.target = 1;
    assert(!press(&ui, &s, 0, APP_GESTURE_SHORT) && ui.target == 1);
    ui.target = 35;
    assert(!press(&ui, &s, 2, APP_GESTURE_SHORT) && ui.target == 35);
    assert(!press(&ui, &s, 1, APP_GESTURE_SHORT) && ui.page == SCORE_PAGE_NEW);
    assert(!press(&ui, &s, 0, APP_GESTURE_SHORT) && ui.page == SCORE_PAGE_SETTINGS);
    assert(!memcmp(&s, &before, sizeof(s))); // Cancel never clears the match.
    press(&ui, &s, 1, APP_GESTURE_SHORT);
    ui.target = 3;
    press(&ui, &s, 1, APP_GESTURE_SHORT);
    assert(press(&ui, &s, 1, APP_GESTURE_SHORT));
    assert(s.state.target == 3 && ui.page == SCORE_PAGE_BOARD);
    // Every +1 is immediate; reaching target disables further adds, but -1
    // still corrects a mistaken winning point and reopens scoring.
    for (unsigned i = 0; i < 3; ++i) assert(press(&ui, &s, 0, APP_GESTURE_SHORT));
    assert(score_winner(&s.state) == 0);
    assert(!press(&ui, &s, 2, APP_GESTURE_SHORT));
    assert(press(&ui, &s, 0, APP_GESTURE_LONG));
    assert(score_winner(&s.state) < 0 && s.state.points[0] == 2);
    assert(press(&ui, &s, 2, APP_GESTURE_SHORT));
    uint8_t record[SCORE_RECORD_SIZE];
    assert(score_encode(&s, record, sizeof(record)));
    score_session_t restored; assert(score_decode(&restored, record, sizeof(record)));
    assert(restored.state.points[0] == 2 && restored.state.points[1] == 1);
    // Undo remains available in settings and includes decrements/new matches.
    press(&ui, &s, 1, APP_GESTURE_SHORT);
    press(&ui, &s, 2, APP_GESTURE_SHORT);
    assert(ui.settings_focus == 1);
    assert(press(&ui, &s, 1, APP_GESTURE_SHORT));
    assert(s.state.points[1] == 0 && ui.page == SCORE_PAGE_BOARD);
}

static void layout_and_settings(void) {
    for (unsigned r = 0; r < 2; ++r) {
        app_rotation_t rotation = r ? APP_ROTATION_90 : APP_ROTATION_270;
        for (unsigned rows = 1; rows <= 3; ++rows) {
            app_rect_t content = app_content_rect(rotation, rows), bar = app_keybar_rect(rotation, rows);
            assert(content.height + bar.height == APP_HEIGHT);
            assert(content.width == APP_WIDTH && bar.width == APP_WIDTH);
            assert(r ? content.y + content.height == bar.y : bar.y + bar.height == content.y);
            assert(rotation == APP_ROTATION_270 ? bar.y == 0 : bar.y + bar.height == APP_HEIGHT);
        }
        assert(app_key_from_hardware(rotation, 0) == (r ? 2 : 0));
        assert(app_key_from_hardware(rotation, 1) == 1);
        assert(app_key_from_hardware(rotation, 2) == (r ? 0 : 2));
        assert(app_key_from_hardware(rotation, 3) == APP_KEY_COUNT);
        uint8_t record[APP_SETTINGS_SIZE]; app_settings_encode(rotation, record);
        app_rotation_t restored = APP_ROTATION_270;
        assert(app_settings_decode(&restored, record, sizeof(record)) && restored == rotation);
        assert(!app_settings_decode(&restored, record, sizeof(record)-1));
        for (unsigned i = 0; i < sizeof(record); ++i) {
            record[i] ^= 0x40;
            assert(!app_settings_decode(&restored, record, sizeof(record)));
            assert(restored == rotation);
            record[i] ^= 0x40;
        }
    }
    assert(!app_rotation_valid(0) && !app_rotation_valid(180));
    app_bindings_t b; app_actions_clear(&b);
    assert(app_actions_register(&b, 0, APP_GESTURE_SHORT, 1, "Add", true, APP_ACTION_NEUTRAL));
    assert(app_binding_rows(&b) == 1);
    assert(app_actions_register(&b, 0, APP_GESTURE_DOUBLE, 2, "Undo", true, APP_ACTION_NEUTRAL));
    assert(app_binding_rows(&b) == 2);
    assert(app_actions_register(&b, 0, APP_GESTURE_LONG, 3, "Subtract", false, APP_ACTION_NEUTRAL));
    assert(app_binding_rows(&b) == 3 && !app_actions_resolve(&b, 0, APP_GESTURE_LONG));
    assert(!app_actions_register(&b, 0, APP_GESTURE_SHORT, 4, "Duplicate", true, APP_ACTION_NEUTRAL));
    assert(!app_actions_register(&b, 3, APP_GESTURE_SHORT, 4, "Bad", true, APP_ACTION_NEUTRAL));
}

static void physical_key_actions(void) {
    const unsigned hardware[2][3] = {{0, 1, 2}, {2, 1, 0}};
    const app_rotation_t rotations[2] = {APP_ROTATION_270, APP_ROTATION_90};
    for (unsigned r = 0; r < 2; ++r) {
        score_session_t s; score_session_init(&s);
        score_controller_t ui; score_controller_init(&ui, &s);
        ui.rotation = rotations[r];
        assert(press(&ui, &s, app_key_from_hardware(ui.rotation, hardware[r][0]), APP_GESTURE_SHORT));
        assert(s.state.points[0] == 1 && ui.page == SCORE_PAGE_BOARD);
        assert(press(&ui, &s, app_key_from_hardware(ui.rotation, hardware[r][2]), APP_GESTURE_SHORT));
        assert(s.state.points[1] == 1);
        assert(press(&ui, &s, app_key_from_hardware(ui.rotation, hardware[r][0]), APP_GESTURE_LONG));
        assert(s.state.points[0] == 0 && s.state.points[1] == 1);
        press(&ui, &s, 1, APP_GESTURE_SHORT);
        ui.settings_focus = 2;
        press(&ui, &s, 1, APP_GESTURE_SHORT);
        assert(ui.page == SCORE_PAGE_DIRECTION);
        press(&ui, &s, r == 0 ? 2 : 0, APP_GESTURE_SHORT);
        assert(ui.rotation == rotations[r]); // Selection alone does not apply.
        press(&ui, &s, 1, APP_GESTURE_SHORT); // Middle always confirms.
        assert(ui.rotation == rotations[1-r]);
        press(&ui, &s, 1, APP_GESTURE_LONG);
        assert(ui.page == SCORE_PAGE_BOARD);
        assert(press(&ui, &s, app_key_from_hardware(ui.rotation, hardware[1-r][2]), APP_GESTURE_LONG));
        assert(s.state.points[1] == 0);
    }
}

static void configuration(void) {
    const score_config_t *config = score_config_default();
    assert(config == score_config_find(SCORE_RULE_CHINESE_EIGHT));
    assert(config->default_target == 10);
    assert(config->min_target <= config->default_target && config->default_target <= config->max_target);
    assert(!score_config_find(999));
    score_session_t s; score_session_init(&s);
    assert(s.state.target == config->default_target && s.state.player_count == config->player_count);
    // An old race-to-five record is restored exactly, without applying defaults.
    s.state.target = 5; s.state.points[0] = 2;
    uint8_t record[SCORE_RECORD_SIZE]; assert(score_encode(&s, record, sizeof(record)));
    score_session_t loaded; score_session_init(&loaded);
    assert(score_decode(&loaded, record, sizeof(record)));
    assert(loaded.state.target == 5 && loaded.state.points[0] == 2);
    score_controller_t ui; score_controller_init(&ui, &loaded);
    press(&ui, &loaded, 1, APP_GESTURE_SHORT);
    press(&ui, &loaded, 1, APP_GESTURE_SHORT);
    assert(ui.page == SCORE_PAGE_TARGET && ui.target == config->default_target);
    assert(loaded.state.target == 5); // Opening setup does not modify the match.
    press(&ui, &loaded, 1, APP_GESTURE_SHORT);
    assert(press(&ui, &loaded, 1, APP_GESTURE_SHORT));
    assert(loaded.state.target == 10 && loaded.state.points[0] == 0);
}

static void semantic_continuity(void) {
    score_session_t s; score_session_init(&s);
    score_controller_t ui; score_controller_init(&ui, &s);
    app_bindings_t b;
    for (unsigned page = SCORE_PAGE_SETTINGS; page < SCORE_PAGE_COUNT; ++page) {
        ui.page = (score_page_t)page;
        score_controller_bindings(&ui, &s, &b);
        assert(b.actions[APP_GESTURE_SHORT][1].role == APP_ACTION_CONFIRM);
        assert(b.actions[APP_GESTURE_LONG][1].role == APP_ACTION_BACK);
    }
    ui.page = SCORE_PAGE_NEW;
    score_controller_bindings(&ui, &s, &b);
    assert(b.actions[APP_GESTURE_SHORT][0].role == APP_ACTION_CANCEL);
    assert(b.actions[APP_GESTURE_SHORT][2].role == APP_ACTION_BACK);
    const score_session_t before = s;
    assert(!press(&ui, &s, 2, APP_GESTURE_SHORT));
    assert(ui.page == SCORE_PAGE_SETTINGS && !memcmp(&before, &s, sizeof(s)));
    app_actions_clear(&b);
    assert(!app_actions_register(&b, 0, APP_GESTURE_SHORT, 1, "bad", true, APP_ACTION_ROLE_COUNT));
}

int main(void) {
    score_input_guard_t guard = { .swallowing = true };
    assert(score_input_consume(&guard, SCORE_INPUT_PRESS));
    assert(score_input_consume(&guard, SCORE_INPUT_RELEASE));
    assert(guard.swallowing);
    assert(score_input_consume(&guard, SCORE_INPUT_CLICK));
    assert(!score_input_consume(&guard, SCORE_INPUT_CLICK));
    guard = (score_input_guard_t){ .swallowing = true };
    assert(score_input_consume(&guard, SCORE_INPUT_LONG));
    assert(score_input_consume(&guard, SCORE_INPUT_RELEASE));
    assert(!guard.swallowing);
    semantic_continuity(); configuration(); rules_and_history(); storage(); controls(); layout_and_settings(); physical_key_actions();
    puts("Score rules, bounded undo, delayed commands, persistent codec and controls: PASS");
}
