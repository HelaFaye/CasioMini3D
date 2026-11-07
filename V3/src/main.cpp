#include "display.hpp"
#include "fixed.hpp"
#include "types.hpp"
#include "world.hpp"
#include "bench.hpp"
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

clock_t lastTime = 0;

enum class AppMode
{
    Demo = 0,
    Benchmark = 1,
    Results = 2
};

static const int UI_TEXT_X = 8;
static const int UI_TEXT_TOP = 8;
static const int LEGEND_TOP = 148;
static const BenchId BENCHMARKS[] = {
    BenchId::ClearOnly,
    BenchId::TriangleThroughput,
    BenchId::TexturedGeometry
};
static const size_t BENCHMARK_COUNT = sizeof(BENCHMARKS) / sizeof(BENCHMARKS[0]);
static BenchResult benchmark_results[BENCHMARK_COUNT];
static bool benchmark_valid[BENCHMARK_COUNT];

static size_t benchmark_index(BenchId id)
{
    for (size_t i = 0; i < BENCHMARK_COUNT; ++i)
        if (BENCHMARKS[i] == id)
            return i;
    return BENCHMARK_COUNT;
}

static void run_benchmark_suite(const BenchConfig& cfg)
{
    for (size_t i = 0; i < BENCHMARK_COUNT; ++i)
        benchmark_valid[i] = run_benchmark(BENCHMARKS[i], cfg, &benchmark_results[i]);
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

static void print_benchmark_results()
{
    dclear(C_BLACK);
    dtext(UI_TEXT_X, 12, C_WHITE, "Benchmark suite results");

    for (size_t i = 0; i < BENCHMARK_COUNT; ++i)
    {
        const int y = 38 + (int)i * 32;
        dtext(UI_TEXT_X, y, C_WHITE, bench_name(BENCHMARKS[i]));
        if (benchmark_valid[i])
        {
            char fps_buffer[32];
            char frame_buffer[32];
            snprintf(fps_buffer, sizeof(fps_buffer), "FPS %.2f", benchmark_results[i].fps);
            snprintf(frame_buffer, sizeof(frame_buffer), "%.3f ms/frame", benchmark_results[i].avg_ms_per_frame);
            dtext(UI_TEXT_X + 190, y, C_WHITE, fps_buffer);
            dtext(UI_TEXT_X + 190, y + 12, C_WHITE, frame_buffer);
        }
        else
        {
            dtext(UI_TEXT_X + 190, y, C_WHITE, "FAILED");
        }
    }

    dtext(UI_TEXT_X, 150, C_WHITE, "F6 / EXE / DEL: rerun test");
    dtext(UI_TEXT_X, 166, C_WHITE, "MENU: quit");
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

    // Run the full suite automatically on launch; no keypress is required.
    run_benchmark_suite(cfg);
    AppMode mode = AppMode::Results;
    BenchId selected_bench = BenchId::ClearOnly;

    while (!keydown(KEY_MENU))
    {
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
            const size_t index = benchmark_index(selected_bench);
            const bool ok = run_benchmark(selected_bench, cfg, &result);
            if (index < BENCHMARK_COUNT)
            {
                benchmark_valid[index] = ok;
                if (ok)
                    benchmark_results[index] = result;
            }
            mode = AppMode::Results;
        }
        else if (mode == AppMode::Results)
        {
            print_benchmark_results();
            dupdate();
        }
        else
        {
            dclear(C_BLACK);
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
