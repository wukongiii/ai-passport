#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "../ui/app_actions.h"
/* Allocation-free ASCII editor. Layouts are rows of keys, independent of UI and
 * hardware. A future nine-key layout can reuse the editor/result boundary. */
enum { TEXT_INPUT_CAPACITY=64, TEXT_INPUT_MAX_KEYS=37 };
typedef enum { TEXT_INPUT_ABC, TEXT_INPUT_DIGITS, TEXT_INPUT_SYMBOLS, TEXT_INPUT_MODE_COUNT } text_input_mode_t;
typedef enum { TEXT_INPUT_CHARACTERS, TEXT_INPUT_EDIT, TEXT_INPUT_CURSOR,
    TEXT_INPUT_CLEAR, TEXT_INPUT_CANCEL, TEXT_INPUT_DONE, TEXT_INPUT_CANCELLED } text_input_page_t;
typedef enum { TEXT_KEY_CHARACTER, TEXT_KEY_ABC, TEXT_KEY_DIGITS, TEXT_KEY_SYMBOLS,
    TEXT_KEY_SPACE, TEXT_KEY_MENU, TEXT_KEY_FINISH } text_input_key_action_t;
typedef struct { const char *rows[3]; unsigned row_count; } text_input_layout_t;
extern const text_input_layout_t text_input_qwerty_layout;
typedef struct { unsigned maximum_length; bool numeric_only, secret; const char *initial; } text_input_config_t;
typedef struct { char character; text_input_key_action_t action; } text_input_candidate_t;
typedef struct {
    text_input_config_t config;
    char text[TEXT_INPUT_CAPACITY+1];
    unsigned length,cursor,focus[TEXT_INPUT_MODE_COUNT],edit_focus;
    text_input_mode_t mode;
    text_input_page_t page;
    bool uppercase,revealed,full;
    uint64_t reveal_until_ms;
} text_input_t;
bool text_input_begin(text_input_t *,const text_input_config_t *,uint64_t now_ms);
void text_input_gesture(text_input_t *,unsigned slot,app_gesture_t,uint64_t now_ms);
void text_input_bindings(const text_input_t *,app_bindings_t *);
unsigned text_input_choice_count(const text_input_t *);
unsigned text_input_choice_focus(const text_input_t *);
unsigned text_input_row_count(const text_input_t *);
unsigned text_input_row_size(const text_input_t *,unsigned row);
text_input_candidate_t text_input_candidate(const text_input_t *,unsigned index);
void text_input_choice(const text_input_t *,unsigned index,char *out,unsigned capacity);
void text_input_window(const text_input_t *,uint64_t now_ms,char *out,unsigned capacity);
void text_input_dispose(text_input_t *);
