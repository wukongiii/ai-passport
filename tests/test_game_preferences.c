#include "games/game_preferences.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    game_eight_preferences_t p; game_preferences_default(&p); assert(game_preferences_valid(&p));
    char name[GAME_PLAYER_NAME_CAPACITY];
    assert(game_player_name("  张三  ", name) && !strcmp(name, "张三"));
    assert(game_player_name("一二三四五六七八", name));
    assert(!game_player_name("123456789", name));
    assert(!game_player_name("　 ", name));
    assert(!game_player_name("😀", name));
    assert(!game_player_name("\xc0\xaf", name));
    assert(!game_player_name("\xed\xa0\x80", name));
    assert(!game_player_name("\xe4\xb8", name));
    char missing[GAME_PLAYER_NAME_CAPACITY]; memset(missing, 'a', sizeof(missing));
    assert(!game_player_name(missing, name));
    game_preferences_request_t r = {0};
    assert(game_preferences_parse("game=eight&player1=%20%E5%BC%A0%E4%B8%89%20&player2=Li&revision=7", &r));
    assert(!strcmp(r.eight.players[0], "张三") && r.expected_revision == 7);
    assert(!game_preferences_parse("game=chase&player1=A&player2=B&revision=7", &r));
    assert(!game_preferences_parse("game=eight&player1=A&player2=a&revision=7", &r));
    assert(!game_preferences_parse("game=eight&player1=A&player2=B&revision=4294967296", &r));
    assert(!game_preferences_parse("game=eight&player1=A&player2=B&revision=7&player1=C", &r));
    assert(!game_preferences_parse("game=eight&player1=%00&player2=B&revision=7", &r));
    assert(!game_preferences_parse("game=eight&player1=%ZZ&player2=B&revision=7", &r));
    assert(!game_preferences_parse("game=eight&player1=A&player2=B", &r));
    assert(game_preferences_entry(0)->available && !game_preferences_entry(1)->available && !game_preferences_entry(3));
    puts("Game preferences UTF-8, font ranges, trim, identity uniqueness and strict forms: PASS");
}
