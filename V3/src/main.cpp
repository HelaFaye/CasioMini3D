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

static void reset_benchmark_state()
{
    for (size_t i = 0; i < BENCHMARK_COUNT; ++i)
    {
        benchmark_valid[i] = false;
        benchmark_results[i].name = "";
        benchmark_results[i].warmup_frames = 0;
        benchmark_results[i].measure_frames = 0;
        benchmark_results[i].elapsed_ms = 0;
        benchmark_results[i].fps = 0.0;
        benchmark_results[i].avg_ms_per_frame = 0.0;
    }
}

static void run_benchmark_suite(const BenchConfig& cfg)
{
    reset_benchmark_state();
    for (size_t i = 0; i < BENCHMARK_COUNT; ++i)
    {
        benchmark_valid[i] = run_benchmark(BENCHMARKS[i], cfg, &benchmark_results[i]);
    }
}

static bool append_benchmark_result_csv(const BenchResult& result, const BenchConfig& cfg)
{
    static const char kLogPath[] = "fls0:benchmarks.csv";
    static const char kCsvHeader[] = "name,warmup_frames,measure_frames,elapsed_ms,fps,avg_ms_per_frame\n";

    FILE* file = fopen(kLogPath, "a");
    if (!file)
    {
        file = fopen(kLogPath, "w");
        if (!file)
            return false;

        if (fprintf(file, "%s", kCsvHeader) < 0)
        {
            fclose(file);
            return false;
        }
    }
    else
    {
        if (fseek(file, 0, SEEK_END) == 0)
        {
            const long size = ftell(file);
            if (size == 0)
            {
                if (fprintf(file, "%s", kCsvHeader) < 0)
                {
                    fclose(file);
                    return false;
                }
            }
        }
    }

    const int written = fprintf(file,
        "%s,%lu,%lu,%lu,%.2f,%.3f\n",
        result.name,
        (unsigned long)cfg.warmup_frames,
        (unsigned long)cfg.measure_frames,
        (unsigned long)result.elapsed_ms,
        result.fps,
        result.avg_ms_per_frame);

    if (written < 0)
    {
        fclose(file);
        return false;
    }

    fclose(file);
    return true;
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
    dtext(UI_TEXT_X, 8, C_WHITE, "Benchmark suite results");

    for (size_t i = 0; i < BENCHMARK_COUNT; ++i)
    {
        const int y = 32 + (int)i * 26;
        dtext(UI_TEXT_X, y, C_WHITE, bench_name(BENCHMARKS[i]));
        if (benchmark_valid[i])
        {
            char fps_buffer[32];
            char frame_buffer[32];
            snprintf(fps_buffer, sizeof(fps_buffer), "FPS %.2f", benchmark_results[i].fps);
            snprintf(frame_buffer, sizeof(frame_buffer), "%.3f ms/frame", benchmark_results[i].avg_ms_per_frame);
            dtext(UI_TEXT_X + 120, y, C_WHITE, fps_buffer);
            dtext(UI_TEXT_X + 120, y + 12, C_WHITE, frame_buffer);
        }
        else
        {
            dtext(UI_TEXT_X + 120, y, C_WHITE, "FAILED");
        }
    }

    dtext(UI_TEXT_X, 150, C_WHITE, "F6 / EXE / DEL: rerun");
    dtext(UI_TEXT_X, 166, C_WHITE, "MENU: quit");
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

int main()
{
    srand(time(NULL));
    init_uv_map();

    BenchConfig cfg;
    cfg.warmup_frames = 5;
    cfg.measure_frames = 30;
    cfg.triangle_count = 64;
    cfg.width = SCREEN_WIDTH;
    cfg.height = SCREEN_HEIGHT;

    Camera* camera = new Camera(Vector3<int>(0, 100, -300), 0);
    World* world = new World(16, 16, 16, new Block*[2]{nullptr, new Cube()});
    world->set_id_unsafe(0, 0, 0, 1);
    world->set_id_unsafe(1, 0, 0, 1);
    for (size_t i = 0; i < 200; ++i)
    {
        world->set_id_unsafe(rand() % 16, rand() % 16, rand() % 16, 1);
    }

    run_benchmark_suite(cfg);
    for (size_t i = 0; i < BENCHMARK_COUNT; ++i)
    {
        if (benchmark_valid[i])
        {
            append_benchmark_result_csv(benchmark_results[i], cfg);
        }
    }

    AppMode mode = AppMode::Results;
    BenchId selected_bench = BenchId::ClearOnly;

    while (!keydown(KEY_MENU))
    {
        dclear(C_BLACK);
        clearevents();

        if (keydown(KEY_F1))
            clock_set_speed(CLOCK_SPEED_F1);
        if (keydown(KEY_F5))
            clock_set_speed(CLOCK_SPEED_F5);
        if (keydown(KEY_F6))
        {
            selected_bench = BenchId::ClearOnly;
            mode = AppMode::Benchmark;
        }
        if (keydown(KEY_EXE))
        {
            selected_bench = BenchId::TriangleThroughput;
            mode = AppMode::Benchmark;
        }
        if (keydown(KEY_DEL))
        {
            selected_bench = BenchId::TexturedGeometry;
            mode = AppMode::Benchmark;
        }

        if (mode == AppMode::Benchmark)
        {
            BenchResult result;
            if (run_benchmark(selected_bench, cfg, &result))
            {
                append_benchmark_result_csv(result, cfg);
                dclear(C_BLACK);
                print_bench_result(result);
                dupdate();
                while (!keydown(KEY_MENU))
                    clearevents();
                mode = AppMode::Results;
            }
        }
        else if (mode == AppMode::Results)
        {
            print_benchmark_results();
            dupdate();
        }
        else
        {
            if (keydown(KEY_LEFT))
                camera->position.x -= 10;
            if (keydown(KEY_RIGHT))
                camera->position.x += 10;
            if (keydown(KEY_UP))
                camera->position.z += 10;
            if (keydown(KEY_DOWN))
                camera->position.z -= 10;

            if (keydown(KEY_8))
                camera->position.y += 10;
            if (keydown(KEY_2))
                camera->position.y -= 10;

            if (keydown(KEY_7))
                camera->angle += 3;
            if (keydown(KEY_1))
                camera->angle -= 3;

            if (keydown(KEY_9))
                camera->offset.y -= 10;
            if (keydown(KEY_3))
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
