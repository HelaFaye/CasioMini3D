#include "display.hpp"
#include "bench.hpp"

#include <errno.h>
#include <stdio.h>
#include <fxlibc/printf.h>
#include <libprof.h>
#include <gint/gint.h>

static const BenchId BENCHMARKS[] = {
    BenchId::ClearOnly,
    BenchId::SolidFill,
    BenchId::TriangleThroughput,
    BenchId::TexturedGeometry
};
static const size_t BENCHMARK_COUNT = sizeof(BENCHMARKS) / sizeof(BENCHMARKS[0]);
static BenchResult benchmark_results[BENCHMARK_COUNT];
static bool benchmark_valid[BENCHMARK_COUNT];
static bool benchmark_logged[BENCHMARK_COUNT];

enum class CsvErrorStage : uint8_t
{
    None = 0,
    WorldSwitch,
    Open,
    Create,
    Seek,
    Size,
    HeaderWrite,
    RowWrite,
    Flush,
    Stream,
    Close
};

static CsvErrorStage benchmark_csv_error[BENCHMARK_COUNT];
static int benchmark_csv_errno[BENCHMARK_COUNT];

struct CsvWriteArgs
{
    const BenchResult* result;
    CsvErrorStage error;
    int error_number;
};

// Schema v2 is deliberately written to a new file. Existing v1 logs remain
// intact rather than receiving rows with a different column layout.
static int append_csv_in_os_world(void* opaque)
{
    CsvWriteArgs* args = static_cast<CsvWriteArgs*>(opaque);
    args->error = CsvErrorStage::None;
    args->error_number = 0;

    static const char path[] = "/benchmarks-v2.csv";
    static const char header[] =
        "schema_version,benchmark_version,scene,timer_source,timer_resolution_us,"
        "warmup_frames,measure_frames,repeat_count,triangles_per_frame,screen_width,screen_height,"
        "clear_mean_us_per_frame,clear_median_us_per_frame,clear_min_us_per_frame,clear_max_us_per_frame,clear_spread_us_per_frame,"
        "raster_mean_us_per_frame,raster_median_us_per_frame,raster_min_us_per_frame,raster_max_us_per_frame,raster_spread_us_per_frame,"
        "present_mean_us_per_frame,present_median_us_per_frame,present_min_us_per_frame,present_max_us_per_frame,present_spread_us_per_frame,"
        "frame_mean_us_per_frame,frame_median_us_per_frame,frame_min_us_per_frame,frame_max_us_per_frame,frame_spread_us_per_frame\n";

    errno = 0;
    FILE* file = fopen(path, "r+");
    if (!file)
    {
        const int open_errno = errno;
        if (open_errno != ENOENT)
        {
            args->error = CsvErrorStage::Open;
            args->error_number = open_errno;
            return static_cast<int>(args->error);
        }

        errno = 0;
        file = fopen(path, "w+");
        if (!file)
        {
            args->error = CsvErrorStage::Create;
            args->error_number = errno;
            return static_cast<int>(args->error);
        }
    }

    CsvErrorStage failure = CsvErrorStage::None;
    int failure_errno = 0;

    errno = 0;
    if (fseek(file, 0, SEEK_END) != 0)
    {
        failure = CsvErrorStage::Seek;
        failure_errno = errno;
    }

    long size = -1;
    if (failure == CsvErrorStage::None)
    {
        errno = 0;
        size = ftell(file);
        if (size < 0)
        {
            failure = CsvErrorStage::Size;
            failure_errno = errno;
        }
    }

    if (failure == CsvErrorStage::None && size == 0)
    {
        errno = 0;
        if (fprintf(file, "%s", header) < 0)
        {
            failure = CsvErrorStage::HeaderWrite;
            failure_errno = errno;
        }
    }

    const BenchResult& r = *args->result;
    if (failure == CsvErrorStage::None)
    {
        errno = 0;
        const int written = fprintf(file,
            "2,%s,%s,%s,%lu,%lu,%lu,%lu,%lu,%u,%u,"
            "%.3f,%.3f,%.3f,%.3f,%.3f,"
            "%.3f,%.3f,%.3f,%.3f,%.3f,"
            "%.3f,%.3f,%.3f,%.3f,%.3f,"
            "%.3f,%.3f,%.3f,%.3f,%.3f\n",
            BENCH_VERSION,
            r.name,
            BENCH_TIMER_SOURCE,
            static_cast<unsigned long>(BENCH_TIMER_RESOLUTION_US),
            static_cast<unsigned long>(r.warmup_frames),
            static_cast<unsigned long>(r.measure_frames),
            static_cast<unsigned long>(r.repeat_count),
            static_cast<unsigned long>(r.triangles_per_frame),
            static_cast<unsigned int>(r.screen_width),
            static_cast<unsigned int>(r.screen_height),
            r.clear.mean_us_per_frame,
            r.clear.median_us_per_frame,
            r.clear.min_us_per_frame,
            r.clear.max_us_per_frame,
            r.clear.spread_us_per_frame,
            r.raster.mean_us_per_frame,
            r.raster.median_us_per_frame,
            r.raster.min_us_per_frame,
            r.raster.max_us_per_frame,
            r.raster.spread_us_per_frame,
            r.present.mean_us_per_frame,
            r.present.median_us_per_frame,
            r.present.min_us_per_frame,
            r.present.max_us_per_frame,
            r.present.spread_us_per_frame,
            r.frame_total.mean_us_per_frame,
            r.frame_total.median_us_per_frame,
            r.frame_total.min_us_per_frame,
            r.frame_total.max_us_per_frame,
            r.frame_total.spread_us_per_frame);
        if (written < 0)
        {
            failure = CsvErrorStage::RowWrite;
            failure_errno = errno;
        }
    }

    if (failure == CsvErrorStage::None)
    {
        errno = 0;
        if (fflush(file) != 0)
        {
            failure = CsvErrorStage::Flush;
            failure_errno = errno;
        }
    }

    if (failure == CsvErrorStage::None && ferror(file))
    {
        failure = CsvErrorStage::Stream;
        failure_errno = errno;
    }

    errno = 0;
    if (fclose(file) != 0 && failure == CsvErrorStage::None)
    {
        failure = CsvErrorStage::Close;
        failure_errno = errno;
    }

    args->error = failure;
    args->error_number = failure_errno;
    return static_cast<int>(failure);
}

static const char* csv_error_name(CsvErrorStage error)
{
    switch (error)
    {
        case CsvErrorStage::None:        return "CSV OK";
        case CsvErrorStage::WorldSwitch: return "CSV SWITCH";
        case CsvErrorStage::Open:        return "CSV OPEN";
        case CsvErrorStage::Create:      return "CSV CREATE";
        case CsvErrorStage::Seek:        return "CSV SEEK";
        case CsvErrorStage::Size:        return "CSV SIZE";
        case CsvErrorStage::HeaderWrite: return "CSV HEADER";
        case CsvErrorStage::RowWrite:    return "CSV WRITE";
        case CsvErrorStage::Flush:       return "CSV FLUSH";
        case CsvErrorStage::Stream:      return "CSV STREAM";
        case CsvErrorStage::Close:       return "CSV CLOSE";
    }
    return "CSV UNKNOWN";
}

static bool append_benchmark_result_csv(
    const BenchResult& result,
    CsvErrorStage* error,
    int* error_number)
{
    CsvWriteArgs args = {
        &result,
        CsvErrorStage::WorldSwitch,
        0
    };

    const int switch_result = gint_world_switch(
        GINT_CALL(append_csv_in_os_world, &args));

    if (args.error == CsvErrorStage::WorldSwitch)
        args.error_number = switch_result;

    if (error)
        *error = args.error;
    if (error_number)
        *error_number = args.error_number;

    return switch_result == 0 && args.error == CsvErrorStage::None;
}

static bool run_and_store(size_t index, const BenchConfig& cfg)
{
    BenchResult result = {};
    benchmark_valid[index] = false;
    benchmark_logged[index] = false;
    benchmark_csv_error[index] = CsvErrorStage::None;
    benchmark_csv_errno[index] = 0;

    if (!run_benchmark(BENCHMARKS[index], cfg, &result))
        return false;

    benchmark_results[index] = result;
    benchmark_valid[index] = true;
    benchmark_logged[index] = append_benchmark_result_csv(
        result, &benchmark_csv_error[index], &benchmark_csv_errno[index]);
    return true;
}

static void run_suite(const BenchConfig& cfg)
{
    for (size_t i = 0; i < BENCHMARK_COUNT; ++i)
    {
        benchmark_valid[i] = false;
        benchmark_logged[i] = false;
        benchmark_results[i] = {};
        benchmark_csv_error[i] = CsvErrorStage::None;
        benchmark_csv_errno[i] = 0;
    }

    for (size_t i = 0; i < BENCHMARK_COUNT; ++i)
        run_and_store(i, cfg);
}

static void draw_results(const BenchConfig& cfg)
{
    dclear(C_BLACK);
    dtext(8, 8, C_WHITE, "fxmark 2.0 | mean/median us per frame");

    for (size_t i = 0; i < BENCHMARK_COUNT; ++i)
    {
        const int y = 22 + static_cast<int>(i) * 40;
        dtext(8, y, C_WHITE, bench_name(BENCHMARKS[i]));

        if (!benchmark_valid[i])
        {
            dtext(8, y + 13, C_WHITE, "FAILED (profile or scene)");
            continue;
        }

        const BenchResult& r = benchmark_results[i];
        char clear_raster[64];
        char present_total[64];
        char csv_number[20];

        snprintf(clear_raster, sizeof(clear_raster),
            "C %.1f/%.1f R %.1f/%.1f us",
            r.clear.mean_us_per_frame,
            r.clear.median_us_per_frame,
            r.raster.mean_us_per_frame,
            r.raster.median_us_per_frame);
        snprintf(present_total, sizeof(present_total),
            "P %.1f/%.1f F %.1f +/- %.1f us %.1f FPS",
            r.present.mean_us_per_frame,
            r.present.median_us_per_frame,
            r.frame_total.mean_us_per_frame,
            r.frame_total.spread_us_per_frame,
            r.frame_total.mean_us_per_frame > 0.0
                ? 1000000.0 / r.frame_total.mean_us_per_frame
                : 0.0);

        dtext(8, y + 12, C_WHITE, clear_raster);
        dtext(8, y + 24, C_WHITE, present_total);
        dtext(300, y, C_WHITE,
            benchmark_logged[i] ? "CSV OK" : csv_error_name(benchmark_csv_error[i]));

        if (!benchmark_logged[i])
        {
            if (benchmark_csv_error[i] == CsvErrorStage::WorldSwitch)
                snprintf(csv_number, sizeof(csv_number), "code %d", benchmark_csv_errno[i]);
            else
                snprintf(csv_number, sizeof(csv_number), "errno %d", benchmark_csv_errno[i]);
            dtext(300, y + 12, C_WHITE, csv_number);
        }
    }

    dtext(8, 188, C_WHITE, "C clear R raster P dupdate F full-frame");
    dtext(8, 205, C_WHITE, "5 runs x60 frames | F6 clear F5 solid EXE tri DEL UV");
    (void)cfg;
}

int main()
{
    __printf_enable_fp();
    prof_init();
    init_uv_map();

    BenchConfig cfg;
    cfg.warmup_frames = 5;
    cfg.measure_frames = 60;
    cfg.repeat_count = 5;
    cfg.triangle_count = 64;
    cfg.width = SCREEN_WIDTH;
    cfg.height = SCREEN_HEIGHT;

    dclear(C_BLACK);
    dtext(8, 8, C_WHITE, "Running fxmark 2.0...");
    dupdate();
    run_suite(cfg);

    while (!keydown(KEY_MENU))
    {
        clearevents();
        draw_results(cfg);
        dupdate();

        size_t selected = BENCHMARK_COUNT;
        if (keydown(KEY_F6))
            selected = 0;
        else if (keydown(KEY_F5))
            selected = 1;
        else if (keydown(KEY_EXE))
            selected = 2;
        else if (keydown(KEY_DEL))
            selected = 3;

        if (selected < BENCHMARK_COUNT)
        {
            dclear(C_BLACK);
            dtext(8, 8, C_WHITE, "Running repeated benchmark...");
            dtext(8, 28, C_WHITE, bench_name(BENCHMARKS[selected]));
            dupdate();
            run_and_store(selected, cfg);
            while (keydown(KEY_F6) || keydown(KEY_F5) || keydown(KEY_EXE) || keydown(KEY_DEL))
                clearevents();
        }
    }

    prof_quit();
    return 0;
}
