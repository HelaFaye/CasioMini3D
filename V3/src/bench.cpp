#include "bench.hpp"

#include "display.hpp"
#include "fixed.hpp"
#include "shapes/shapes.hpp"

#include <gint/rtc.h>
#include <stdint.h>

using BenchSceneFn = bool (*)(const BenchConfig& cfg, uint32_t frames);

struct BenchSceneEntry
{
    BenchId id;
    const char* name;
    BenchSceneFn run;
};

static bool run_clear_only_scene(const BenchConfig& cfg, uint32_t frames)
{
    (void)cfg;
    for (uint32_t frame = 0; frame < frames; ++frame)
    {
        dclear(C_BLACK);
        dupdate();
    }
    return true;
}

// Solid-color fill baseline. This path deliberately avoids UV work.
static bool run_solid_fill_scene(const BenchConfig& cfg, uint32_t frames)
{
    for (uint32_t frame = 0; frame < frames; ++frame)
    {
        dclear(C_BLACK);
        for (uint32_t i = 0; i < cfg.triangle_count; ++i)
        {
            const int x = static_cast<int>((i * 37u + frame * 3u) % (SCREEN_WIDTH - 32));
            const int y = static_cast<int>((i * 23u + frame * 2u) % (SCREEN_HEIGHT - 32));
            const int w = 12 + static_cast<int>(i % 20u);
            const int h = 12 + static_cast<int>((i * 3u) % 20u);

            VertexLocal a = {x, y, 0, 0};
            VertexLocal b = {x + w, y + h / 2, 0, 0};
            VertexLocal c = {x, y + h, 0, 0};
            DrawTriangleSolid(a, b, c, C_WHITE);
        }
        dupdate();
    }
    return true;
}

static bool run_triangle_throughput_scene(const BenchConfig& cfg, uint32_t frames)
{
    for (uint32_t frame = 0; frame < frames; ++frame)
    {
        dclear(C_BLACK);
        for (uint32_t i = 0; i < cfg.triangle_count; ++i)
        {
            const int x = static_cast<int>((i * 37u + frame * 3u) % (SCREEN_WIDTH - 32));
            const int y = static_cast<int>((i * 23u + frame * 2u) % (SCREEN_HEIGHT - 32));
            const int w = 12 + static_cast<int>(i % 20u);
            const int h = 12 + static_cast<int>((i * 3u) % 20u);

            VertexLocal a = {x, y, 0, 0};
            VertexLocal b = {x + w, y + h / 2, FLOAT_TO_FIXED(39), 0};
            VertexLocal c = {x, y + h, 0, FLOAT_TO_FIXED(39)};
            DrawTriangle(a, b, c);
        }
        dupdate();
    }
    return true;
}

static bool run_textured_geometry_scene(const BenchConfig& cfg, uint32_t frames)
{
    for (uint32_t frame = 0; frame < frames; ++frame)
    {
        dclear(C_BLACK);
        for (uint32_t i = 0; i < cfg.triangle_count; ++i)
        {
            const int x0 = static_cast<int>((i * 17u + frame * 2u) % (SCREEN_WIDTH - 48));
            const int y0 = static_cast<int>((i * 19u + frame * 3u) % (SCREEN_HEIGHT - 48));
            const int w = 16 + static_cast<int>(i % 16u);
            const int h = 16 + static_cast<int>((i * 5u) % 16u);

            VertexLocal a = {x0, y0, 0, 0};
            VertexLocal b = {x0 + w, y0, FLOAT_TO_FIXED(39), 0};
            VertexLocal c = {x0 + w, y0 + h, FLOAT_TO_FIXED(39), FLOAT_TO_FIXED(39)};
            VertexLocal d = {x0, y0 + h, 0, FLOAT_TO_FIXED(39)};

            DrawTriangle(a, b, c);
            DrawTriangle(a, c, d);
        }
        dupdate();
    }
    return true;
}

static const BenchSceneEntry g_bench_scenes[] =
{
    {BenchId::ClearOnly, "clear-only", run_clear_only_scene},
    {BenchId::SolidFill, "solid-fill", run_solid_fill_scene},
    {BenchId::TriangleThroughput, "triangle-throughput", run_triangle_throughput_scene},
    {BenchId::TexturedGeometry, "textured-geometry", run_textured_geometry_scene}
};

static const int g_bench_scene_count = sizeof(g_bench_scenes) / sizeof(g_bench_scenes[0]);

static bool execute_scene(BenchId id, const BenchConfig& cfg, uint32_t frames)
{
    for (int i = 0; i < g_bench_scene_count; ++i)
    {
        if (g_bench_scenes[i].id == id)
            return g_bench_scenes[i].run(cfg, frames);
    }
    return false;
}

const char* bench_name(BenchId id)
{
    for (int i = 0; i < g_bench_scene_count; ++i)
    {
        if (g_bench_scenes[i].id == id)
            return g_bench_scenes[i].name;
    }
    return "unknown";
}

bool run_benchmark(BenchId id, const BenchConfig& cfg, BenchResult* out)
{
    if (!out || cfg.measure_frames == 0 || cfg.width <= 0 || cfg.height <= 0)
        return false;
    if ((id == BenchId::SolidFill || id == BenchId::TriangleThroughput || id == BenchId::TexturedGeometry) && cfg.triangle_count == 0)
        return false;

    if (!execute_scene(id, cfg, cfg.warmup_frames))
        return false;

    const uint32_t start_ticks = rtc_ticks();
    if (!execute_scene(id, cfg, cfg.measure_frames))
        return false;
    const uint32_t end_ticks = rtc_ticks();

    // rtc_ticks() is the number of 128-Hz ticks since midnight. It resets
    // at midnight, so use a one-day modulus rather than uint32_t wraparound.
    constexpr uint32_t RTC_TICKS_PER_DAY = 24u * 60u * 60u * 128u;
    const uint32_t elapsed_ticks = (end_ticks >= start_ticks)
        ? (end_ticks - start_ticks)
        : (RTC_TICKS_PER_DAY - start_ticks + end_ticks);

    if (elapsed_ticks == 0)
        return false;

    const double elapsed_ms = static_cast<double>(elapsed_ticks) * 1000.0 / 128.0;
    if (elapsed_ms <= 0.0)
        return false;

    out->name = bench_name(id);
    out->warmup_frames = cfg.warmup_frames;
    out->measure_frames = cfg.measure_frames;
    out->elapsed_ms = static_cast<uint32_t>(elapsed_ms + 0.5);
    out->fps = static_cast<double>(cfg.measure_frames) * 1000.0 / elapsed_ms;
    out->avg_ms_per_frame = elapsed_ms / static_cast<double>(cfg.measure_frames);
    return true;
}
