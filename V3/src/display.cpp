#include "display.hpp"

void DrawPixel(int x, int y, int color)
{
    if (static_cast<unsigned>(x) >= SCREEN_WIDTH ||
        static_cast<unsigned>(y) >= SCREEN_HEIGHT)
        return;

    gint_vram[DWIDTH * y + x] = static_cast<color_t>(color);
}

extern bopti_image_t IMG_ASSET_blocks;

static int uv_map[40 * 40];

void set_uv_map(int x, int y, int color)
{
    if (static_cast<unsigned>(x) >= 40u || static_cast<unsigned>(y) >= 40u)
        return;
    uv_map[x + y * 40] = color;
}

int get_uv_map(int x, int y)
{
    // Define out-of-range texture coordinates as clamp-to-edge sampling.
    if (x < 0) x = 0;
    else if (x >= 40) x = 39;
    if (y < 0) y = 0;
    else if (y >= 40) y = 39;
    return uv_map[x + y * 40];
}

int GetPixel_CG_P8(bopti_image_t* img, int x, int y)
{
    if (!img || !img->data || x < 0 || y < 0 ||
        x >= img->width || y >= img->height)
        return -128;

    const uint8_t* data_u8 = static_cast<const uint8_t*>(img->data) + y * img->stride;
    return static_cast<int8_t>(data_u8[x]);
}

int DecodePixel_CG_P8(bopti_image_t* img, int pixel)
{
    if (!img || !img->palette)
        return C_BLACK;

    const int palette_index = pixel + 128;
    if (palette_index < 0 || palette_index >= img->color_count)
        return C_BLACK;

    return img->palette[palette_index];
}

int get_uv_map_img(int x, int y)
{
    return DecodePixel_CG_P8(&IMG_ASSET_blocks, GetPixel_CG_P8(&IMG_ASSET_blocks, x, y));
}

void init_uv_map()
{
    for (int i = 0; i < 40; ++i)
    {
        for (int j = 0; j < 40; ++j)
        {
            const float u = i / 40.0f;
            const float v = j / 40.0f;
            set_uv_map(i, j, rgb(static_cast<int>(u * 255), static_cast<int>(v * 255), 0));
        }
    }
}
