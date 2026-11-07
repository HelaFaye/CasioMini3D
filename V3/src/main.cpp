#include "display.hpp"
#include "fixed.hpp"
#include "types.hpp"
#include "world.hpp"
#include "bench.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <gint/gint.h>

static const BenchId BENCHMARKS[] = {
    BenchId::ClearOnly,
    BenchId::TriangleThroughput,
    BenchId::TexturedGeometry
};
static const size_t BENCHMARK_COUNT = sizeof(BENCHMARKS) / sizeof(BENCHMARKS[0]);
static BenchResult benchmark_results[BENCHMARK_COUNT];
static bool benchmark_valid[BENCHMARK_COUNT];
static bool benchmark_logged[BENCHMARK_COUNT];

typedef struct CsvWriteArgs
{
    const BenchResult* result;
    const BenchConfig* config;
} CsvWriteArgs;

// Fugue filesystem operations run in the OS world, not in Gint's world.
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

    long size = ok ? ftell(file) : -1;
    if (size < 0)
        ok = false;
    if (ok && size == 0 && fprintf(file, "%s", header) < 0)
        ok = false;

    if (ok && fprintf(file, "%s,%lu,%lu,%lu,%.2f,%.3f\n",
            args->result->name,
            (unsigned long)args->config->warmup_frames,
            (unsigned long)args->config->measure_frames,
            (unsigned long)args->result->elapsed_ms,
            args->result->fps,
            args->result->avg_ms_per_frame) < 0)
        ok = false;

    if (fclose(file) != 0)
        ok = false;
    return ok ? 0 : -1;
}

static bool append_benchmark_result_csv(const BenchResult& result, const BenchConfig& cfg)
{
    CsvWriteArgs args = {&result, &cfg};
    return gint_world_switch(GINT_CALL(append_csv_in_os_world, &args)) == 0;
}

static bool run_and_store(size_t index, const BenchConfig& cfg)
{
    BenchResult result = {};
    benchmark_valid[index] = false;
    benchmark_logged[index] = false;

    if (!run_benchmark(BENCHMARKS[index], cfg, &result))
        return false;

    benchmark_results[index] = result;
    benchmark_valid[index] = true;
    benchmark_logged[index] = append_benchmark_result_csv(result, cfg);
    return true;
}

static void run_suite(const BenchConfig& cfg)
{
    for (size_t i = 0; i < BENCHMARK_COUNT; ++i)
    {
        benchmark_valid[i] = false;
        benchmark_logged[i] = false;
        benchmark_results[i] = {};
    }

    for (size_t i = 0; i < BENCHMARK_COUNT; ++i)
        run_and_store(i, cfg);
}

static void draw_results()
{
    dclear(C_BLACK);
    dtext(8, 8, C_WHITE, "Benchmark results");

    for (size_t i = 0; i < BENCHMARK_COUNT; ++i)
    {
        const int y = 34 + (int)i * 38;
        dtext(8, y, C_WHITE, bench_name(BENCHMARKS[i]));
        if (benchmark_valid[i])
        {
            char fps[32];
            char ms[32];
            snprintf(fps, sizeof(fps), "FPS %.2f", benchmark_results[i].fps);
            snprintf(ms, sizeof(ms), "%.2f ms/frame", benchmark_results[i].avg_ms_per_frame);
            dtext(8, y + 15, C_WHITE, fps);
            dtext(145, y + 15, C_WHITE, ms);
            dtext(300, y + 15, C_WHITE, benchmark_logged[i] ? "CSV OK" : "CSV FAIL");
        }
        else
        {
            dtext(8, y + 15, C_WHITE, "FAILED (timer or scene)");
        }
    }

    dtext(8, 164, C_WHITE, "F6 / EXE / DEL: rerun test");
    dtext(8, 182, C_WHITE, "MENU: quit");
}

int main()
{
    srand(time(NULL));
    init_uv_map();

    BenchConfig cfg;
    cfg.warmup_frames = 5;
    cfg.measure_frames = 60;
    cfg.triangle_count = 64;
    cfg.width = SCREEN_WIDTH;
    cfg.height = SCREEN_HEIGHT;

    // Give visible feedback before the blocking automatic startup suite.
    dclear(C_BLACK);
    dtext(8, 8, C_WHITE, "Running benchmark suite...");
    dupdate();
    run_suite(cfg);

    while (!keydown(KEY_MENU))
    {
        clearevents();
        draw_results();
        dupdate();

        size_t selected = BENCHMARK_COUNT;
        if (keydown(KEY_F6))
            selected = 0;
        else if (keydown(KEY_EXE))
            selected = 1;
        else if (keydown(KEY_DEL))
            selected = 2;

        if (selected < BENCHMARK_COUNT)
        {
            dclear(C_BLACK);
            dtext(8, 8, C_WHITE, "Running selected benchmark...");
            dtext(8, 28, C_WHITE, bench_name(BENCHMARKS[selected]));
            dupdate();
            run_and_store(selected, cfg);
            while (keydown(KEY_F6) || keydown(KEY_EXE) || keydown(KEY_DEL))
                clearevents();
        }
    }

    return 0;
}
