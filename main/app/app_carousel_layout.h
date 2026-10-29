#pragma once
#include <stdint.h>
/* Pure layout, positions measured in 1/1024 of one card step. */
typedef struct { int x, y, width, height, image_scale; uint8_t opacity; } app_card_layout_t;
app_card_layout_t app_carousel_layout(int position);
/* Signed shortest step; wraparound still travels one card. */
int app_carousel_direction(unsigned previous, unsigned next, unsigned count);
