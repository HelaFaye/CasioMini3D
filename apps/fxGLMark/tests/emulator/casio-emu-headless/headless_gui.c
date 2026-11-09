#include "gui/gui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCREEN_WIDTH 396
#define SCREEN_HEIGHT 224

#define EXPECTED_POINT_SIZE_MARKER 0x07e0
#define EXPECTED_POINT_RED 0xf800
#define EXPECTED_POINT_SIZE_RED_PIXELS 600
#define EXPECTED_AUXILIARY_PIXELS 3
#define EXPECTED_DEPTH_PIXEL 0x001f
#define EXPECTED_BLEND_PIXEL 0x8010
#define EXPECTED_COLOR_MASK_PIXEL 0x07ff

struct ExpectedRect
{
    int x;
    int y;
    int width;
    int height;
};

/* Independent framebuffer oracle for main.cpp's point-size test cases. */
static const struct ExpectedRect expected_red_rects[] = {
    {50, 40, 1, 1},
    {119, 39, 3, 3},
    {212, 32, 16, 16},
    {312, 32, 16, 16},
    {50, 100, 1, 1},
    {90, 100, 1, 1},
    {130, 100, 1, 1},
    {97, 54, 5, 5},
    {168, 148, 5, 5},
    {391, 219, 5, 5},
};

static u16 last_frame[SCREEN_WIDTH * SCREEN_HEIGHT];
static int have_frame;

void initGui(void)
{
    memset(last_frame, 0, sizeof(last_frame));
    have_frame = 0;
}

void updateDisplay(u16 *vram)
{
    if (!vram)
        return;
    memcpy(last_frame, vram, sizeof(last_frame));
    have_frame = 1;
}

static void save_ppm(void)
{
    FILE *file = fopen("emulated-screen.ppm", "wb");
    if (!file)
    {
        perror("emulated-screen.ppm");
        return;
    }
    fprintf(file, "P6\n%d %d\n255\n", SCREEN_WIDTH, SCREEN_HEIGHT);
    for (size_t i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; ++i)
    {
        const u16 pixel = last_frame[i];
        const unsigned char rgb[3] = {
            (unsigned char)((((pixel >> 11) & 0x1f) * 255) / 31),
            (unsigned char)((((pixel >> 5) & 0x3f) * 255) / 63),
            (unsigned char)(((pixel & 0x1f) * 255) / 31)
        };
        fwrite(rgb, sizeof(rgb), 1, file);
    }
    fclose(file);
}

static u16 expected_pixel_color(int x, int y)
{
    for (size_t i = 0;
         i < sizeof(expected_red_rects) / sizeof(expected_red_rects[0]);
         ++i)
    {
        const struct ExpectedRect *rect = &expected_red_rects[i];
        if (x >= rect->x && x < rect->x + rect->width &&
            y >= rect->y && y < rect->y + rect->height)
            return EXPECTED_POINT_RED;
    }
    if (x == 240 && y == 150) return EXPECTED_DEPTH_PIXEL;
    if (x == 280 && y == 150) return EXPECTED_BLEND_PIXEL;
    if (x == 310 && y == 150) return EXPECTED_COLOR_MASK_PIXEL;
    return 0;
}

static int validate_framebuffer(int *red_pixel_count, int *auxiliary_pixel_count)
{
    int red_pixels = 0;
    int auxiliary_pixels = 0;
    for (int y = 0; y < SCREEN_HEIGHT; ++y)
    {
        for (int x = 0; x < SCREEN_WIDTH; ++x)
        {
            const u16 pixel = last_frame[y * SCREEN_WIDTH + x];

            /* Pixel zero is reserved for the add-in's pass/fail marker. */
            if (x == 0 && y == 0)
                continue;

            const u16 expected = expected_pixel_color(x, y);
            if (pixel == EXPECTED_POINT_RED)
                ++red_pixels;
            else if (expected != 0)
                ++auxiliary_pixels;

            if (pixel != expected)
                return 0;
        }
    }

    *red_pixel_count = red_pixels;
    *auxiliary_pixel_count = auxiliary_pixels;
    return red_pixels == EXPECTED_POINT_SIZE_RED_PIXELS &&
           auxiliary_pixels == EXPECTED_AUXILIARY_PIXELS;
}

void runMainLoop(void (*callback)(void))
{
    /* The application is non-interactive. Advance a bounded number of frames
       so display/DMA writes have time to reach the emulator's LCD callback. */
    for (int frame = 0; frame < 12; ++frame)
        callback();

    save_ppm();
    const unsigned marker = have_frame ? last_frame[0] : 0;
    int red_pixels = 0;
    int auxiliary_pixels = 0;
    const int framebuffer_valid =
        have_frame && validate_framebuffer(&red_pixels, &auxiliary_pixels);

    printf("CASIO_EMU_GLES_MARKER=0x%04x\n", marker);
    printf("CASIO_EMU_GLES_RED_PIXELS=%d\n", red_pixels);
    printf("CASIO_EMU_GLES_AUXILIARY_PIXELS=%d\n", auxiliary_pixels);
    printf("CASIO_EMU_GLES_CASES=13\n");
    printf("CASIO_EMU_DISPLAY_CAPTURED=%s\n", have_frame ? "yes" : "no");

    if (!have_frame || marker != EXPECTED_POINT_SIZE_MARKER ||
        !framebuffer_valid)
    {
        fprintf(stderr, "Casio emulator GLES regression test failed\n");
        exit(1);
    }
    puts("CASIO_EMU_GLES_RESULT=PASS");
}
