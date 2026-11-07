#pragma once

#include <stdint.h>

constexpr uint32_t BENCH_MAX_REPEATS = 9;
constexpr uint32_t BENCH_TIMER_RESOLUTION_US = 1;
constexpr const char* BENCH_VERSION = "fxmark-2.0";
constexpr const char* BENCH_TIMER_SOURCE = "libprof>=2.4/TMU";

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
    uint32_t repeat_count;
    uint32_t triangle_count;
    int width;
    int height;
};

struct BenchMetricStats
{
    double min_us_per_frame;
    double max_us_per_frame;
    double mean_us_per_frame;
    double median_us_per_frame;
    double spread_us_per_frame;
};

struct BenchResult
{
    const char* name;
    uint32_t warmup_frames;
    uint32_t measure_frames;
    uint32_t repeat_count;
    uint32_t triangles_per_frame;
    uint32_t screen_width;
    uint32_t screen_height;
    BenchMetricStats clear;
    BenchMetricStats raster;
    BenchMetricStats present;
    BenchMetricStats frame_total;
};

const char* bench_name(BenchId id);
bool run_benchmark(BenchId id, const BenchConfig& cfg, BenchResult* out);
