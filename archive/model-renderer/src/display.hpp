#ifndef DISPLAY_H
#define DISPLAY_H
#include "dout.hpp"
#include <gint/gint.h>
#include <gint/display.h>
#include <gint/keyboard.h>
#include <gint/rtc.h>
#include <gint/keyboard.h>
#include <gint/timer.h>
#include <gint/clock.h>
#include <gint/dma.h>

#define SCREEN_WIDTH 396
#define SCREEN_HEIGHT 224

#define rgb(r, g, b) ((r & 0xf8) << 8) | ((g & 0xfc) << 3) | ((b & 0xf8) >> 3)

void gint_dhline(int x1, int x2, int y, color_t color);

void DrawPixel(int x, int y, int color);
void DrawLine(int x1, int y1, int x2, int y2, int color);
void set_uv_map(int x, int y, int color);

int get_uv_map(int x, int y);

inline int GetPixel_CG_P8(bopti_image_t* img, int x, int y){
    uint8_t *data_u8 = (uint8_t *)((uint8_t *)img->data + y * img->stride);
    return (int8_t)data_u8[x];
}

inline int DecodePixel_CG_P8(bopti_image_t* img, int pixel){
    return img->palette[pixel+128];
}

int get_uv_map_img(int x, int y);

extern bopti_image_t IMG_Texture;

const int alpha_texture = image_alpha(IMG_Texture.format);
inline void DrawPixelTexture(int x, int y, int dx, int dy)
{
    /*int i = GetPixel_CG_P8(&IMG_Texture, x, y);
    if (i != alpha_texture)
        gint_vram[DWIDTH * (dy) + (dx)] = DecodePixel_CG_P8(&IMG_Texture, i);*/
    const uint8_t *data_u8 = (uint8_t *)((uint8_t *)IMG_Texture.data + y * IMG_Texture.stride);
    int i = (int8_t)data_u8[x];
    if (i != alpha_texture)
        gint_vram[DWIDTH * (dy) + (dx)] = IMG_Texture.palette[i+128];
}

void init_uv_map();

#endif