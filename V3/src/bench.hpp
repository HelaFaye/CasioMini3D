#pragma once

#include <stdint.h>

enum class BenchId : uint8_t
{
    ClearOnly = 0,
    TriangleThroughput = 1,
    TexturedGeometry = 2,
    SolidFill = 3
};

struct BenchConfig
{
    uint32_t warmup_frames;
    uint32_t measure_frames;
    uint32_t triangle_count;
    int width;
    int height;
};

struct BenchResult
{
    const char* name;
    uint32_t warmup_frames;
    uint32_t measure_frames;
    uint32_t elapsed_ms;
    double fps;
    double avg_ms_per_frame;
};

const char* bench_name(BenchId id);
bool run_benchmark(BenchId id, const BenchConfig& cfg, BenchResult* out);
