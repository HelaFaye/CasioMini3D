#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>

#include "../../src/bench.hpp"
#include "../../src/display.hpp"
#include <libprof.h>

uint32_t fxmark_test_prof_counter = 0;

namespace
{
size_t clear_calls = 0;
size_t update_calls = 0;
size_t pixel_writes = 0;
size_t uv_reads = 0;

void reset_counters()
{
    fxmark_test_prof_counter = 0;
    clear_calls = 0;
    update_calls = 0;
    pixel_writes = 0;
    uv_reads = 0;
}

static BenchConfig test_config()
{
    BenchConfig cfg = {};
    cfg.warmup_frames = 0;
    cfg.measure_frames = 2;
    cfg.repeat_count = 3;
    cfg.triangle_count = 4;
    cfg.width = SCREEN_WIDTH;
    cfg.height = SCREEN_HEIGHT;
    return cfg;
}
}

void dclear(int color)
{
    (void)color;
    // Two frames per repetition: 10, 20, then 30 us/frame across runs.
    const uint32_t duration = static_cast<uint32_t>((clear_calls / 2) + 1) * 10u;
    ++clear_calls;
    fxmark_test_prof_counter += duration;
}

void dupdate(void)
{
    ++update_calls;
    fxmark_test_prof_counter += 25u;
}

void DrawPixel(int x, int y, int color)
{
    (void)color;
    assert(x >= 0 && x < SCREEN_WIDTH);
    assert(y >= 0 && y < SCREEN_HEIGHT);
    ++pixel_writes;
    ++fxmark_test_prof_counter;
}

int get_uv_map(int x, int y)
{
    assert(x >= 0 && x < 40);
    assert(y >= 0 && y < 40);
    ++uv_reads;
    fxmark_test_prof_counter += 2u;
    return C_WHITE;
}

static void test_registry_names()
{
    assert(std::strcmp(bench_name(BenchId::ClearOnly), "clear-only") == 0);
    assert(std::strcmp(bench_name(BenchId::SolidFill), "solid-fill") == 0);
    assert(std::strcmp(bench_name(BenchId::TriangleThroughput), "triangle-throughput") == 0);
    assert(std::strcmp(bench_name(BenchId::TexturedGeometry), "textured-geometry") == 0);
}

static void test_clear_only_separates_clear_present_and_total()
{
    reset_counters();
    const BenchConfig cfg = test_config();
    BenchResult result = {};

    assert(run_benchmark(BenchId::ClearOnly, cfg, &result));
    assert(std::strcmp(result.name, "clear-only") == 0);
    assert(result.repeat_count == 3);
    assert(result.triangles_per_frame == 0);
    assert(result.screen_width == SCREEN_WIDTH);
    assert(result.screen_height == SCREEN_HEIGHT);
    assert(clear_calls == 6);
    assert(update_calls == 6);
    assert(pixel_writes == 0);
    assert(uv_reads == 0);

    assert(result.clear.min_us_per_frame == 10.0);
    assert(result.clear.median_us_per_frame == 20.0);
    assert(result.clear.mean_us_per_frame == 20.0);
    assert(result.clear.max_us_per_frame == 30.0);
    assert(result.clear.spread_us_per_frame == 20.0);

    assert(result.raster.mean_us_per_frame == 0.0);
    assert(result.present.mean_us_per_frame == 25.0);
    assert(result.frame_total.min_us_per_frame == 35.0);
    assert(result.frame_total.median_us_per_frame == 45.0);
    assert(result.frame_total.mean_us_per_frame == 45.0);
    assert(result.frame_total.max_us_per_frame == 55.0);
    assert(result.frame_total.spread_us_per_frame == 20.0);
}

static void test_solid_fill_uses_no_uv_map_and_reports_primitive_count()
{
    reset_counters();
    const BenchConfig cfg = test_config();
    BenchResult result = {};

    assert(run_benchmark(BenchId::SolidFill, cfg, &result));
    assert(std::strcmp(result.name, "solid-fill") == 0);
    assert(result.triangles_per_frame == 4);
    assert(clear_calls == 6);
    assert(update_calls == 6);
    assert(pixel_writes > 0);
    assert(uv_reads == 0);
    assert(result.raster.mean_us_per_frame > 0.0);
    assert(result.present.mean_us_per_frame == 25.0);
}

static void test_existing_triangle_workloads_still_sample_uv_map()
{
    reset_counters();
    const BenchConfig cfg = test_config();
    BenchResult result = {};

    assert(run_benchmark(BenchId::TriangleThroughput, cfg, &result));
    assert(std::strcmp(result.name, "triangle-throughput") == 0);
    assert(result.triangles_per_frame == 4);
    assert(pixel_writes > 0);
    assert(uv_reads == pixel_writes);
    assert(result.raster.mean_us_per_frame > 0.0);
    assert(result.present.mean_us_per_frame == 25.0);

    reset_counters();
    assert(run_benchmark(BenchId::TexturedGeometry, cfg, &result));
    assert(std::strcmp(result.name, "textured-geometry") == 0);
    assert(result.triangles_per_frame == 8);
    assert(pixel_writes > 0);
    assert(uv_reads == pixel_writes);
    assert(result.raster.mean_us_per_frame > 0.0);
}

static void test_repeat_limit_and_invalid_id()
{
    reset_counters();
    BenchConfig cfg = test_config();
    BenchResult result = {};

    cfg.repeat_count = 0;
    assert(!run_benchmark(BenchId::ClearOnly, cfg, &result));

    cfg.repeat_count = BENCH_MAX_REPEATS + 1;
    assert(!run_benchmark(BenchId::ClearOnly, cfg, &result));

    cfg = test_config();
    assert(std::strcmp(bench_name(static_cast<BenchId>(255)), "unknown") == 0);
    assert(!run_benchmark(static_cast<BenchId>(255), cfg, &result));
}

int main()
{
    prof_init();
    test_registry_names();
    test_clear_only_separates_clear_present_and_total();
    test_solid_fill_uses_no_uv_map_and_reports_primitive_count();
    test_existing_triangle_workloads_still_sample_uv_map();
    test_repeat_limit_and_invalid_id();
    prof_quit();

    std::puts("benchmark timing tests: PASS");
    return 0;
}
