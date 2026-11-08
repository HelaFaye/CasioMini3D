#ifndef DISPLAY_H
#define DISPLAY_H

#include <gint/display.h>
#include <gint/keyboard.h>
#include <gint/display.h>
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
void set_uv_map(int x, int y, int color);

int get_uv_map(int x, int y);

int GetPixel_CG_P8(bopti_image_t* img, int x, int y);
int DecodePixel_CG_P8(bopti_image_t* img, int pixel);

int get_uv_map_img(int x, int y);
void init_uv_map();

#endif