#pragma once

#include <stdint.h>

typedef int color_t;

typedef struct bopti_image_t
{
    const uint8_t *data;
    int width;
    int height;
    int stride;
    const color_t *palette;
    int color_count;
} bopti_image_t;

#define C_BLACK 0
#define C_WHITE 0x7fff
#define C_RED 0x7c00

void dclear(int color);
void dupdate(void);
