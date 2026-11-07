#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>

#include "../../src/bench.hpp"
#include "../../src/display.hpp"

namespace
{
uint32_t rtc_value = 0;
size_t clear_calls = 0;
size_t update_calls = 0;
size_t pixel_writes = 0;
size_t uv_reads = 0;

void reset_counters()
{
    clear_calls = 0;
    update_calls = 0;
    pixel_writes = 0;
    uv_reads = 0;
}
}

uint32_t rtc_ticks(void)
{
    return ++rtc_value;
}

void dclear(int color)
{
    (void)color;
    ++clear_calls;
}

void dupdate(void)
{
    ++update_calls;
}

void DrawPixel(int x, int y, int color)
{
    (void)color;
    assert(x >= 0 && x < SCREEN_WIDTH);
    assert(y >= 0 && y < SCREEN_HEIGHT);
    ++pixel_writes;
}

int get_uv_map(int x, int y)
{
    assert(x >= 0 && x < 40);
    assert(y >= 0 && y < 40);
    ++uv_reads;
    return C_WHITE;
}

static BenchConfig test_config()
{
    BenchConfig cfg = {};
    cfg.warmup_frames = 0;
    cfg.measure_frames = 1;
    cfg.triangle_count = 4;
    cfg.width = SCREEN_WIDTH;
    cfg.height = SCREEN_HEIGHT;
    return cfg;
}

static void test_registry_names()
{
    assert(std::strcmp(bench_name(BenchId::ClearOnly), "clear-only") == 0);
    assert(std::strcmp(bench_name(BenchId::SolidFill), "solid-fill") == 0);
    assert(std::strcmp(bench_name(BenchId::TriangleThroughput), "triangle-throughput") == 0);
    assert(std::strcmp(bench_name(BenchId::TexturedGeometry), "textured-geometry") == 0);
}

static void test_solid_fill_uses_no_uv_map()
{
    reset_counters();
    const BenchConfig cfg = test_config();
    BenchResult result = {};

    assert(run_benchmark(BenchId::SolidFill, cfg, &result));
    assert(std::strcmp(result.name, "solid-fill") == 0);
    assert(clear_calls == 1);
    assert(update_calls == 1);
    assert(pixel_writes > 0);
    assert(uv_reads == 0);
}

static void test_existing_triangle_workloads_still_use_uv_map()
{
    reset_counters();
    const BenchConfig cfg = test_config();
    BenchResult result = {};

    assert(run_benchmark(BenchId::TriangleThroughput, cfg, &result));
    assert(std::strcmp(result.name, "triangle-throughput") == 0);
    assert(pixel_writes > 0);
    assert(uv_reads == pixel_writes);

    reset_counters();
    assert(run_benchmark(BenchId::TexturedGeometry, cfg, &result));
    assert(std::strcmp(result.name, "textured-geometry") == 0);
    assert(pixel_writes > 0);
    assert(uv_reads == pixel_writes);
}

static void test_clear_and_invalid_id()
{
    reset_counters();
    const BenchConfig cfg = test_config();
    BenchResult result = {};

    assert(run_benchmark(BenchId::ClearOnly, cfg, &result));
    assert(std::strcmp(result.name, "clear-only") == 0);
    assert(clear_calls == 1);
    assert(update_calls == 1);
    assert(pixel_writes == 0);
    assert(uv_reads == 0);

    assert(std::strcmp(bench_name(static_cast<BenchId>(255)), "unknown") == 0);
    assert(!run_benchmark(static_cast<BenchId>(255), cfg, &result));
}

int main()
{
    test_registry_names();
    test_solid_fill_uses_no_uv_map();
    test_existing_triangle_workloads_still_use_uv_map();
    test_clear_and_invalid_id();

    std::puts("benchmark registry tests: PASS");
    return 0;
}
