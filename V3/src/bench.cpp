#include "bench.hpp"

#include "display.hpp"
#include "fixed.hpp"
#include "shapes/shapes.hpp"

#include <libprof.h>
#include <stdint.h>

using BenchSceneFn = void (*)(const BenchConfig& cfg, uint32_t frame);

struct BenchSceneEntry
{
    BenchId id;
    const char* name;
    BenchSceneFn draw;
};

static void draw_solid_fill_frame(const BenchConfig& cfg, uint32_t frame)
{
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
}

static void draw_triangle_throughput_frame(const BenchConfig& cfg, uint32_t frame)
{
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
}

static void draw_textured_geometry_frame(const BenchConfig& cfg, uint32_t frame)
{
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
}

static const BenchSceneEntry g_bench_scenes[] =
{
    {BenchId::ClearOnly, "clear-only", nullptr},
    {BenchId::SolidFill, "solid-fill", draw_solid_fill_frame},
    {BenchId::TriangleThroughput, "triangle-throughput", draw_triangle_throughput_frame},
    {BenchId::TexturedGeometry, "textured-geometry", draw_textured_geometry_frame}
};

static const uint32_t g_bench_scene_count =
    sizeof(g_bench_scenes) / sizeof(g_bench_scenes[0]);

static const BenchSceneEntry* find_scene(BenchId id)
{
    for (uint32_t i = 0; i < g_bench_scene_count; ++i)
    {
        if (g_bench_scenes[i].id == id)
            return &g_bench_scenes[i];
    }
    return nullptr;
}

const char* bench_name(BenchId id)
{
    const BenchSceneEntry* scene = find_scene(id);
    return scene ? scene->name : "unknown";
}

static void run_unmeasured_frame(
    const BenchSceneEntry& scene,
    const BenchConfig& cfg,
    uint32_t frame)
{
    dclear(C_BLACK);
    if (scene.draw)
        scene.draw(cfg, frame);
    dupdate();
}

static BenchMetricStats summarize_samples(double samples[BENCH_MAX_REPEATS], uint32_t count)
{
    // Insertion sort is sufficient for the deliberately small repeat cap.
    for (uint32_t i = 1; i < count; ++i)
    {
        const double value = samples[i];
        uint32_t j = i;
        while (j > 0 && samples[j - 1] > value)
        {
            samples[j] = samples[j - 1];
            --j;
        }
        samples[j] = value;
    }

    double sum = 0.0;
    for (uint32_t i = 0; i < count; ++i)
        sum += samples[i];

    BenchMetricStats stats = {};
    stats.min_us_per_frame = samples[0];
    stats.max_us_per_frame = samples[count - 1];
    stats.mean_us_per_frame = sum / count;
    stats.median_us_per_frame = (count & 1u)
        ? samples[count / 2]
        : (samples[count / 2 - 1] + samples[count / 2]) / 2.0;
    stats.spread_us_per_frame = stats.max_us_per_frame - stats.min_us_per_frame;
    return stats;
}

static uint32_t triangles_per_frame(BenchId id, const BenchConfig& cfg)
{
    switch (id)
    {
        case BenchId::ClearOnly:
            return 0;
        case BenchId::SolidFill:
        case BenchId::TriangleThroughput:
            return cfg.triangle_count;
        case BenchId::TexturedGeometry:
            return cfg.triangle_count * 2u;
    }
    return 0;
}

bool run_benchmark(BenchId id, const BenchConfig& cfg, BenchResult* out)
{
    if (!out ||
        cfg.measure_frames == 0 ||
        cfg.repeat_count == 0 ||
        cfg.repeat_count > BENCH_MAX_REPEATS ||
        cfg.width <= 0 ||
        cfg.height <= 0)
        return false;

    if ((id == BenchId::SolidFill ||
         id == BenchId::TriangleThroughput ||
         id == BenchId::TexturedGeometry) &&
        (cfg.triangle_count == 0 || cfg.triangle_count > UINT32_MAX / 2u))
        return false;

    const BenchSceneEntry* scene = find_scene(id);
    if (!scene)
        return false;

    // Warm-up uses the same clear/draw/present path but is not included in
    // any sample. Each measured repetition then starts at frame index zero.
    for (uint32_t frame = 0; frame < cfg.warmup_frames; ++frame)
        run_unmeasured_frame(*scene, cfg, frame);

    double clear_samples[BENCH_MAX_REPEATS] = {};
    double raster_samples[BENCH_MAX_REPEATS] = {};
    double present_samples[BENCH_MAX_REPEATS] = {};
    double total_samples[BENCH_MAX_REPEATS] = {};

    for (uint32_t repeat = 0; repeat < cfg.repeat_count; ++repeat)
    {
        prof_t clear_prof = prof_make();
        prof_t raster_prof = prof_make();
        prof_t present_prof = prof_make();
        prof_t total_prof = prof_make();

        prof_enter(total_prof);
        for (uint32_t frame = 0; frame < cfg.measure_frames; ++frame)
        {
            prof_enter(clear_prof);
            dclear(C_BLACK);
            prof_leave(clear_prof);

            if (scene->draw)
            {
                prof_enter(raster_prof);
                scene->draw(cfg, frame);
                prof_leave(raster_prof);
            }

            prof_enter(present_prof);
            dupdate();
            prof_leave(present_prof);
        }
        prof_leave(total_prof);

        const double frame_count = static_cast<double>(cfg.measure_frames);
        clear_samples[repeat] = prof_time(clear_prof) / frame_count;
        raster_samples[repeat] = scene->draw
            ? prof_time(raster_prof) / frame_count
            : 0.0;
        present_samples[repeat] = prof_time(present_prof) / frame_count;
        total_samples[repeat] = prof_time(total_prof) / frame_count;
    }

    out->name = scene->name;
    out->warmup_frames = cfg.warmup_frames;
    out->measure_frames = cfg.measure_frames;
    out->repeat_count = cfg.repeat_count;
    out->triangles_per_frame = triangles_per_frame(id, cfg);
    out->screen_width = SCREEN_WIDTH;
    out->screen_height = SCREEN_HEIGHT;
    out->clear = summarize_samples(clear_samples, cfg.repeat_count);
    out->raster = summarize_samples(raster_samples, cfg.repeat_count);
    out->present = summarize_samples(present_samples, cfg.repeat_count);
    out->frame_total = summarize_samples(total_samples, cfg.repeat_count);
    return true;
}
