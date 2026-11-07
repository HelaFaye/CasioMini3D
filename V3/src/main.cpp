#include "display.hpp"
#include "fixed.hpp"
#include "types.hpp"
#include "world.hpp"
#include "bench.hpp"
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <gint/bfile.h>

clock_t lastTime = 0;

enum class AppMode
{
    Demo = 0,
    Benchmark = 1
};

static const int UI_TEXT_X = 8;
static const int UI_TEXT_TOP = 8;
static const int LEGEND_TOP = 148;

static void utf16_from_cstr(const char* src, uint16_t* out, size_t max_chars)
{
    size_t i = 0;
    while (src[i] != '\0' && i + 1 < max_chars)
    {
        out[i] = (uint16_t)(unsigned char)src[i];
        ++i;
    }
    out[i] = 0;
}

static bool append_benchmark_result_csv(const BenchResult& result, const BenchConfig& cfg)
{
    static const char kLogPath[] = "fls0:benchmarks.csv";
    static const char kCsvHeader[] = "name,warmup_frames,measure_frames,elapsed_ms,fps,avg_ms_per_frame\n";

    uint16_t filename[32];
    utf16_from_cstr(kLogPath, filename, sizeof(filename) / sizeof(filename[0]));

    int handle = Bfile_OpenFile(filename, BFile_ReadWrite);
    if (handle < 0)
    {
        if (Bfile_CreateFile(filename, 4096) < 0)
            return false;
        handle = Bfile_OpenFile(filename, BFile_ReadWrite);
        if (handle < 0)
            return false;

        const size_t header_len = strlen(kCsvHeader);
        if (Bfile_WriteFile(handle, kCsvHeader, (int)header_len) < 0)
        {
            Bfile_CloseFile(handle);
            return false;
        }
    }
    else
    {
        long file_size = Bfile_SeekFile(handle, 0, BFile_SeekEnd);
        if (file_size < 0)
        {
            Bfile_CloseFile(handle);
            return false;
        }

        if (file_size == 0)
        {
            const size_t header_len = strlen(kCsvHeader);
            if (Bfile_WriteFile(handle, kCsvHeader, (int)header_len) < 0)
            {
                Bfile_CloseFile(handle);
                return false;
            }
        }

        if (Bfile_SeekFile(handle, 0, BFile_SeekEnd) < 0)
        {
            Bfile_CloseFile(handle);
            return false;
        }
    }

    char line[256];
    int written = snprintf(line, sizeof(line),
        "%s,%u,%u,%u,%.2f,%.3f\n",
        result.name,
        cfg.warmup_frames,
        cfg.measure_frames,
        result.elapsed_ms,
        result.fps,
        result.avg_ms_per_frame);

    if (written <= 0)
    {
        Bfile_CloseFile(handle);
        return false;
    }

    const bool ok = Bfile_WriteFile(handle, line, written) >= 0;
    Bfile_CloseFile(handle);
    return ok;
}

void dclock()
{
    clock_t deltaTime = clock() - lastTime;
    int fps = 1000;
    if (deltaTime != 0)
        fps = CLOCKS_PER_SEC / deltaTime;

    char buffer[20];
    sprintf(buffer, "FPS %d", fps);
    dtext(UI_TEXT_X, UI_TEXT_TOP, C_WHITE, buffer);
    lastTime = clock();
}

static void print_bench_result(const BenchResult& result)
{
    char fps_buffer[32];
    char frame_buffer[32];
    char elapsed_buffer[32];
    snprintf(fps_buffer, sizeof(fps_buffer), "FPS %.2f", result.fps);
    snprintf(frame_buffer, sizeof(frame_buffer), "ms/frame %.3f", result.avg_ms_per_frame);
    snprintf(elapsed_buffer, sizeof(elapsed_buffer), "Elapsed %lu ms", (unsigned long)result.elapsed_ms);

    dtext(UI_TEXT_X, 12, C_WHITE, "Benchmark");
    dtext(UI_TEXT_X, 28, C_WHITE, result.name);
    dtext(UI_TEXT_X, 44, C_WHITE, fps_buffer);
    dtext(UI_TEXT_X, 60, C_WHITE, frame_buffer);
    dtext(UI_TEXT_X, 76, C_WHITE, elapsed_buffer);
    dtext(UI_TEXT_X, 96, C_WHITE, "MENU: return");
}

static void print_benchmark_legend()
{
    dtext(UI_TEXT_X, LEGEND_TOP, C_WHITE, "F6: clear-only");
    dtext(UI_TEXT_X, LEGEND_TOP + 14, C_WHITE, "EXE: triangles");
    dtext(UI_TEXT_X, LEGEND_TOP + 28, C_WHITE, "DEL: textured");
    dtext(UI_TEXT_X, LEGEND_TOP + 42, C_WHITE, "MENU: quit");
}

int main(){
    //clock_set_speed(CLOCK_SPEED_F5);
    srand(time(NULL));
    init_uv_map();

    AppMode mode = AppMode::Demo;
    BenchId selected_bench = BenchId::ClearOnly;

    BenchConfig cfg;
    cfg.warmup_frames = 5;
    cfg.measure_frames = 30;
    cfg.triangle_count = 64;
    cfg.width = SCREEN_WIDTH;
    cfg.height = SCREEN_HEIGHT;

    Camera *camera = new Camera(Vector3<int>(0, 100, -300), 0);
    World *world = new World(16, 16, 16, new Block*[2]{nullptr, new Cube()});
    world->set_id_unsafe(0, 0, 0, 1);
    world->set_id_unsafe(1, 0, 0, 1);
    for (size_t i = 0; i < 200; i++)
    {
        world->set_id_unsafe(rand() % 16, rand() % 16, rand() % 16, 1);
    }
    while (!keydown(KEY_MENU))
    {
        dclear(C_BLACK);
        clearevents();

        if(keydown(KEY_F1))
            clock_set_speed(CLOCK_SPEED_F1);
        if(keydown(KEY_F5))
            clock_set_speed(CLOCK_SPEED_F5);
        if(keydown(KEY_F6))
        {
            selected_bench = BenchId::ClearOnly;
            mode = AppMode::Benchmark;
        }
        if(keydown(KEY_EXE))
        {
            selected_bench = BenchId::TriangleThroughput;
            mode = AppMode::Benchmark;
        }
        if(keydown(KEY_DEL))
        {
            selected_bench = BenchId::TexturedGeometry;
            mode = AppMode::Benchmark;
        }

        if (mode == AppMode::Benchmark)
        {
            BenchResult result;
            if (run_benchmark(selected_bench, cfg, &result))
            {
                dclear(C_BLACK);
                print_bench_result(result);
                append_benchmark_result_csv(result, cfg);
                dupdate();
                while (!keydown(KEY_MENU))
                    clearevents();
                mode = AppMode::Demo;
            }
        }
        else
        {
            if(keydown(KEY_LEFT))
                camera->position.x -= 10;
            if(keydown(KEY_RIGHT))
                camera->position.x += 10;
            if(keydown(KEY_UP))
                camera->position.z += 10;
            if(keydown(KEY_DOWN))
                camera->position.z -= 10;

            if(keydown(KEY_8))
                camera->position.y += 10;
            if(keydown(KEY_2))
                camera->position.y -= 10;

            if(keydown(KEY_7))
                camera->angle += 3;
            if(keydown(KEY_1))
                camera->angle -= 3;

            if(keydown(KEY_9))
                camera->offset.y -= 10;
            if(keydown(KEY_3))
                camera->offset.y += 10;

            world->Render();
            camera->Render();
            dclock();
            print_benchmark_legend();
            dupdate();
        }
    }
    return 0;
}
