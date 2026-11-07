#include "bench.hpp"

#include "display.hpp"
#include "fixed.hpp"
#include "shapes/shapes.hpp"

#include <time.h>

static bool measure_frames(BenchId id, const BenchConfig& cfg, uint32_t frames)
{
    switch (id)
    {
        case BenchId::ClearOnly:
            for (uint32_t i = 0; i < frames; ++i)
                dclear(C_BLACK);
            return true;

        case BenchId::TriangleThroughput:
        {
            for (uint32_t frame = 0; frame < frames; ++frame)
            {
                dclear(C_BLACK);
                for (uint32_t i = 0; i < cfg.triangle_count; ++i)
                {
                    const int x = (int)((i * 37u) % (SCREEN_WIDTH - 32));
                    const int y = (int)((i * 23u) % (SCREEN_HEIGHT - 32));
                    const int w = 12 + (int)(i % 20u);
                    const int h = 12 + (int)((i * 3u) % 20u);

                    VertexLocal a = {x, y, 0, 0};
                    VertexLocal b = {x + w, y + h / 2, FLOAT_TO_FIXED(39), 0};
                    VertexLocal c = {x, y + h, 0, FLOAT_TO_FIXED(39)};
                    DrawTriangle(a, b, c);
                }
            }
            return true;
        }
    }

    return false;
}

const char* bench_name(BenchId id)
{
    switch (id)
    {
        case BenchId::ClearOnly:
            return "clear-only";
        case BenchId::TriangleThroughput:
            return "triangle-throughput";
        default:
            return "unknown";
    }
}

bool run_benchmark(BenchId id, const BenchConfig& cfg, BenchResult* out)
{
    if (!out || cfg.measure_frames == 0 || cfg.width <= 0 || cfg.height <= 0)
        return false;
    if (id == BenchId::TriangleThroughput && cfg.triangle_count == 0)
        return false;

    if (!measure_frames(id, cfg, cfg.warmup_frames))
        return false;

    const clock_t start = clock();
    if (!measure_frames(id, cfg, cfg.measure_frames))
        return false;
    const clock_t elapsed_ticks = clock() - start;
    if (elapsed_ticks <= 0 || CLOCKS_PER_SEC <= 0)
        return false;

    const double elapsed_ms = (double)elapsed_ticks * 1000.0 / (double)CLOCKS_PER_SEC;
    out->name = bench_name(id);
    out->warmup_frames = cfg.warmup_frames;
    out->measure_frames = cfg.measure_frames;
    out->elapsed_ms = (uint32_t)(elapsed_ms + 0.5);
    out->fps = (double)cfg.measure_frames * 1000.0 / elapsed_ms;
    out->avg_ms_per_frame = elapsed_ms / (double)cfg.measure_frames;
    return true;
}
