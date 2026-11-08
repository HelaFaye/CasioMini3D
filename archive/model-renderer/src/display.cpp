#include "display.hpp"

void DrawPixel(int x, int y, int color)
{
    gint_vram[DWIDTH * y + x] = color;
}

void DrawLine(int x1, int y1, int x2, int y2, int color)
{
    dline(x1, y1, x2, y2, color);
}

int uv_map[40*40];

void set_uv_map(int x, int y, int color)
{
    uv_map[x + y * 40] = color;
}

int get_uv_map(int x, int y)
{
    return uv_map[x + y * 40];
}

int get_uv_map_img(int x, int y, bool* alpha)
{
    int color = GetPixel_CG_P8(&IMG_Texture, x, y);
    *alpha = color == image_alpha(IMG_Texture.format);
    if (*alpha)
        return 0;
    return DecodePixel_CG_P8(&IMG_Texture, color);
}

void init_uv_map()
{
    for (int i = 0; i < 40; i++)
    {
        for (int j = 0; j < 40; j++)
        {
            float u = i / 40.0;
            float v = j / 40.0;
            set_uv_map(i, j, rgb((int)(u * 255), (int)(v * 255), 0));
        }
    }
}