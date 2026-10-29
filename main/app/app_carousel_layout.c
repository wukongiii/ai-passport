#include "app_carousel_layout.h"

app_card_layout_t app_carousel_layout(int position) {
    int distance = position < 0 ? -position : position;
    if (distance > 2048) distance = 2048;
    const int near = distance > 1024 ? 1024 : distance;
    const int scale = 256 - near * 62 / 1024;
    const int width = 200 * scale / 256;
    const int height = 138 * scale / 256;
    return (app_card_layout_t){160 + position * 166 / 1024 - width / 2,
        34 + (138 - height) / 2, width, height, scale,
        (uint8_t)(255 - distance * 95 / 1024)};
}

int app_carousel_direction(unsigned previous, unsigned next, unsigned count) {
    if (count < 2 || previous == next) return 0;
    if ((previous + 1) % count == next) return 1;
    if ((previous + count - 1) % count == next) return -1;
    return 0; // Programmatic jump: settle directly.
}
