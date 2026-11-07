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
#include <gint/gint.h>

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
static bool benchmark_log_ok = true;

struct CsvWriteArgs
{
    const BenchResult* result;
    const BenchConfig* config;
};

// Fugue-backed filesystem calls need to run in the OS world on Gint.
static int append_csv_in_os_world(void* opaque)
{
    const CsvWriteArgs* args = (const CsvWriteArgs*)opaque;
    static const char path[] = "\\fls0\\benchmarks.csv";
    static const char header[] = "name,warmup_frames,measure_frames,elapsed_ms,fps,avg_ms_per_frame\n";

    FILE* file = fopen(path, "a+");
    if (!file)
        return -1;

    bool ok = true;
    if (fseek(file, 0, SEEK_END) != 0)
        ok = false;

    const long size = ok ? ftell(file) : -1;
    if (size < 0)
        ok = false;

    if (ok && size == 0 && fprintf(file, "%s", header) < 0)
        ok = false;

    int written = -1;
    if (ok)
    {
        written = fprintf(file,
            "%s,%lu,%lu,%lu,%.2f,%.3f\n",
            args->result->name,
            (unsigned long)args->config->warmup_frames,
            (unsigned long)args->config->measure_frames,
            (unsigned long)args->result->elapsed_ms,
            args->result->fps,
            args->result->avg_ms_per_frame);
        if (written < 0)
            ok = false;
    }

    if (fclose(file) != 0)
        ok = false;
    return ok ? 0 : -1;
}

static bool append_benchmark_result_csv(const BenchResult& result, const BenchConfig& cfg)
{
    CsvWriteArgs args = {&result, &cfg};
    return gint_world_switch(GINT_CALL(append_csv_in_os_world, &args)) == 0;
}

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
    benchmark_log_ok = true;
}

static void run_benchmark_suite(const BenchConfig& cfg)
{
    reset_benchmark_state();
    for (size_t i = 0; i < BENCHMARK_COUNT; ++i)
    {
        benchmark_valid[i] = run_benchmark(BENCHMARKS[i], cfg, &benchmark_results[i]);
        if (benchmark_valid[i] && !append_benchmark_result_csv(benchmark_results[i], cfg))
            benchmark_log_ok = false;
    }
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

    dtext(UI_TEXT_X, 132, C_WHITE, benchmark_log_ok ? "CSV log: OK" : "CSV log: FAILED");
    dtext(UI_TEXT_X, 150, C_WHITE, "F6 / EXE / DEL: rerun");
    dtext(UI_TEXT_X, 166, C_WHITE, "MENU: quit");
}

static void print_bench_result(const BenchResult& result, bool logged)
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
    dtext(UI_TEXT_X, 92, C_WHITE, logged ? "CSV log: OK" : "CSV log: FAILED");
    dtext(UI_TEXT_X, 108, C_WHITE, "MENU: return");
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
        world->set_id_unsafe(rand() % 16, rand() % 16, rand() % 16, 1);

    run_benchmark_suite(cfg);

    AppMode mode = AppMode::Results;
    BenchId selected_bench = BenchId::ClearOnly;

    while (!keydown(KEY_MENU))
    {
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
            const bool ran = run_benchmark(selected_bench, cfg, &result);
            const size_t index = (size_t)selected_bench;
            bool logged = false;
            if (ran)
            {
                logged = append_benchmark_result_csv(result, cfg);
                if (!logged)
                    benchmark_log_ok = false;
                if (index < BENCHMARK_COUNT)
                {
                    benchmark_valid[index] = true;
                    benchmark_results[index] = result;
                }
            }
            else if (index < BENCHMARK_COUNT)
            {
                benchmark_valid[index] = false;
            }

            dclear(C_BLACK);
            if (ran)
                print_bench_result(result, logged);
            else
                dtext(UI_TEXT_X, 12, C_WHITE, "Benchmark failed");
            dupdate();
            while (!keydown(KEY_MENU))
                clearevents();
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
