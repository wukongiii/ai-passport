#include "app/app_carousel_layout.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    app_card_layout_t center = app_carousel_layout(0);
    assert(center.x == 60 && center.width == 200 && center.height == 138);
    app_card_layout_t left = app_carousel_layout(-1024), right = app_carousel_layout(1024);
    assert(left.x + left.width / 2 == -6 && right.x + right.width / 2 == 326);
    assert(left.width == right.width && left.width < center.width && left.opacity < center.opacity);
    app_card_layout_t half = app_carousel_layout(512);
    assert(half.width > right.width && half.width < center.width);
    assert(app_carousel_layout(4096).opacity == app_carousel_layout(2048).opacity);
    assert(app_carousel_direction(4,0,5) == 1 && app_carousel_direction(0,4,5) == -1);
    assert(app_carousel_direction(0,0,1) == 0 && app_carousel_direction(0,3,5) == 0);
    puts("Carousel center/side/interpolation layout and wrap direction: PASS");
}
