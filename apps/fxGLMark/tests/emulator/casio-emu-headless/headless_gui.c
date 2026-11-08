#include "gui/gui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCREEN_WIDTH 396
#define SCREEN_HEIGHT 224
#define EXPECTED_POINT_SIZE_PIXELS 9
#define EXPECTED_POINT_SIZE_MARKER 0x07e0
#define EXPECTED_POINT_RED 0xf800

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

static int count_center_red_pixels(void)
{
    int count = 0;
    const int center_x = SCREEN_WIDTH / 2;
    const int center_y = SCREEN_HEIGHT / 2;
    for (int y = center_y - 2; y <= center_y + 2; ++y)
        for (int x = center_x - 2; x <= center_x + 2; ++x)
            if (last_frame[y * SCREEN_WIDTH + x] == EXPECTED_POINT_RED)
                ++count;
    return count;
}

void runMainLoop(void (*callback)(void))
{
    /* The application is a non-interactive smoke test. Advancing a bounded
       number of display frames lets its DMA/display writes settle. */
    for (int frame = 0; frame < 12; ++frame)
        callback();

    save_ppm();
    const unsigned marker = have_frame ? last_frame[0] : 0;
    const int red_pixels = have_frame ? count_center_red_pixels() : 0;
    printf("CASIO_EMU_POINT_SIZE_MARKER=0x%04x\n", marker);
    printf("CASIO_EMU_POINT_SIZE_CENTER_RED_PIXELS=%d\n", red_pixels);
    printf("CASIO_EMU_DISPLAY_CAPTURED=%s\n", have_frame ? "yes" : "no");

    if (!have_frame || marker != EXPECTED_POINT_SIZE_MARKER ||
        red_pixels != EXPECTED_POINT_SIZE_PIXELS)
    {
        fprintf(stderr, "Casio emulator point-size smoke test failed\n");
        exit(1);
    }
    puts("CASIO_EMU_POINT_SIZE_RESULT=PASS");
}
