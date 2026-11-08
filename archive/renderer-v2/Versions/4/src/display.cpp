#include "display.hpp"

void DrawPixel(int x, int y, int color)
{
    gint_vram[DWIDTH * y + x] = color;
}

extern bopti_image_t IMG_ASSET_blocks;

int uv_map[40*40];

void set_uv_map(int x, int y, int color)
{
    uv_map[x + y * 40] = color;
}

int get_uv_map(int x, int y)
{
    return uv_map[x + y * 40];
}

int GetPixel_CG_P8(bopti_image_t* img, int x, int y){
    uint8_t *data_u8 = (uint8_t *)((uint8_t *)img->data + y * img->stride);
    return (int8_t)data_u8[x];
}

int DecodePixel_CG_P8(bopti_image_t* img, int pixel){
    return img->palette[pixel+128];
}

int get_uv_map_img(int x, int y)
{
    return DecodePixel_CG_P8(&IMG_ASSET_blocks, GetPixel_CG_P8(&IMG_ASSET_blocks, x, y));
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