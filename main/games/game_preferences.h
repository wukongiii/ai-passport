#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { GAME_PLAYER_NAME_CHARS = 8, GAME_PLAYER_NAME_CAPACITY = 25 };
typedef struct { char players[2][GAME_PLAYER_NAME_CAPACITY]; } game_eight_preferences_t;
typedef struct { const char *id, *name; bool available; } game_preferences_entry_t;
const game_preferences_entry_t *game_preferences_entry(unsigned index);
void game_preferences_default(game_eight_preferences_t *out);
/* Strict UTF-8, bounded to the existing 14px font: printable ASCII, CJK
 * U+4E00..U+9FEF, CJK punctuation and fullwidth punctuation. Edge spaces trim.
 * IDs remain the stable player slots; names are display labels/voice aliases. */
bool game_player_name(const char *input, char out[GAME_PLAYER_NAME_CAPACITY]);
bool game_preferences_valid(const game_eight_preferences_t *p);
typedef struct { game_eight_preferences_t eight; unsigned expected_revision; } game_preferences_request_t;
bool game_preferences_parse(const char *form, game_preferences_request_t *out);

/* Network worker owns NVS. UI/HTTP only copy bounded snapshots/requests.
 * A submitted request is not durable until completed with error == 0. */
typedef struct {
    game_eight_preferences_t eight;
    bool ready, busy;
    unsigned revision, submitted, completed;
    int error;
} game_preferences_snapshot_t;
void game_preferences_snapshot(game_preferences_snapshot_t *out);
bool game_preferences_submit(const game_preferences_request_t *request, unsigned *id);
void game_preferences_service_init(void);
void game_preferences_service_poll(void);
