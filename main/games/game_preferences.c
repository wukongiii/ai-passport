#include "game_preferences.h"
#include "../network/app_network_model.h"
#include "../score/score_strings.h"
#include "../app/app_strings.h"
#include <limits.h>
#include <string.h>

const game_preferences_entry_t *game_preferences_entry(unsigned index) {
    static const game_preferences_entry_t entries[] = {
        {"eight", TXT_APP_EIGHT, true}, {"chase", TXT_APP_CHASE, false},
        {"snooker", TXT_APP_SNOOKER, false}
    };
    return index < sizeof(entries) / sizeof(entries[0]) ? &entries[index] : NULL;
}
void game_preferences_default(game_eight_preferences_t *out) {
    *out = (game_eight_preferences_t){ .players = {TXT_PLAYER_ONE, TXT_PLAYER_TWO} };
}
static bool supported(uint32_t cp) {
    return (cp >= 32 && cp <= 126) || (cp >= 0x3000 && cp <= 0x303f) ||
           (cp >= 0x4e00 && cp <= 0x9fef) || (cp >= 0xff01 && cp <= 0xff60);
}
bool game_player_name(const char *input, char out[GAME_PLAYER_NAME_CAPACITY]) {
    if (!input || !out) return false;
    size_t length=0;
    while(length<GAME_PLAYER_NAME_CAPACITY && input[length])++length;
    if(length==GAME_PLAYER_NAME_CAPACITY)return false;
    const char *end=input+length;
    const unsigned char *p = (const unsigned char *)input;
    const unsigned char *limit = (const unsigned char *)end;
    char result[GAME_PLAYER_NAME_CAPACITY] = {0};
    unsigned used = 0, count = 0, trimmed_bytes = 0, trimmed_chars = 0;
    while (p < limit) {
        unsigned bytes = 1; uint32_t cp = *p;
        if (*p >= 0xe0 && *p <= 0xef) {
            bytes = 3;
            if (limit - p < 3 || (p[1] & 0xc0) != 0x80 || (p[2] & 0xc0) != 0x80) return false;
            cp = ((p[0] & 15) << 12) | ((p[1] & 63) << 6) | (p[2] & 63);
            if (cp < 0x800 || (cp >= 0xd800 && cp <= 0xdfff)) return false;
        } else if (*p >= 128) return false;
        if (!supported(cp)) return false;
        bool space = cp == 32 || cp == 0x3000;
        if (count || !space) {
            memcpy(result + used, p, bytes); used += bytes; ++count;
            if (!space) { trimmed_bytes = used; trimmed_chars = count; }
        }
        p += bytes;
    }
    if (!trimmed_chars || trimmed_chars > GAME_PLAYER_NAME_CHARS) return false;
    result[trimmed_bytes] = 0;
    memcpy(out, result, sizeof(result));
    return true;
}
static bool same_name(const char *a, const char *b) {
    for (; *a && *b; ++a, ++b) {
        unsigned char x = (unsigned char)*a, y = (unsigned char)*b;
        if (x >= 'A' && x <= 'Z') x += 'a' - 'A';
        if (y >= 'A' && y <= 'Z') y += 'a' - 'A';
        if (x != y) return false;
    }
    return *a == *b;
}
bool game_preferences_valid(const game_eight_preferences_t *p) {
    if (!p) return false;
    for (unsigned i = 0; i < 2; ++i) {
        char normalized[GAME_PLAYER_NAME_CAPACITY];
        if (!game_player_name(p->players[i], normalized) || strcmp(normalized, p->players[i])) return false;
    }
    return !same_name(p->players[0], p->players[1]);
}
bool game_preferences_parse(const char *form, game_preferences_request_t *out) {
    char game[16], first[GAME_PLAYER_NAME_CAPACITY], second[GAME_PLAYER_NAME_CAPACITY], revision[12];
    game_preferences_request_t r = {0};
    if (!out || !app_net_form_value(form, "game", game, sizeof(game)) || strcmp(game, "eight") ||
        !app_net_form_value(form, "player1", first, sizeof(first)) ||
        !app_net_form_value(form, "player2", second, sizeof(second)) ||
        !app_net_form_value(form, "revision", revision, sizeof(revision)) || !*revision ||
        !game_player_name(first, r.eight.players[0]) || !game_player_name(second, r.eight.players[1]) ||
        !game_preferences_valid(&r.eight)) return false;
    for (const char *p = revision; *p; ++p) {
        if (*p < '0' || *p > '9' || r.expected_revision > (UINT_MAX - (unsigned)(*p - '0')) / 10) return false;
        r.expected_revision = r.expected_revision * 10 + (unsigned)(*p - '0');
    }
    *out = r; return true;
}
