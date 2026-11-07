#include "display.hpp"
#include "bench.hpp"

#include <errno.h>
#include <stdio.h>
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
    const BenchConfig* config;
    CsvErrorStage error;
    int error_number;
};

// Gint's POSIX/C99 filesystem API uses normalized paths. "/" is the
// Fugue storage root; the raw "\\\\fls0\\..." prefix belongs to BFile.
// File operations must run in the OS world on fx-CG targets.
static int append_csv_in_os_world(void* opaque)
{
    CsvWriteArgs* args = static_cast<CsvWriteArgs*>(opaque);
    args->error = CsvErrorStage::None;
    args->error_number = 0;

    static const char path[] = "/benchmarks.csv";
    static const char header[] = "name,warmup_frames,measure_frames,elapsed_ms,fps,avg_ms_per_frame\n";

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

        // Avoid relying on append-mode behavior: create only if absent,
        // then explicitly seek to the end before writing.
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

    if (failure == CsvErrorStage::None)
    {
        errno = 0;
        if (fprintf(file, "%s,%lu,%lu,%lu,%.2f,%.3f\n",
                args->result->name,
                static_cast<unsigned long>(args->config->warmup_frames),
                static_cast<unsigned long>(args->config->measure_frames),
                static_cast<unsigned long>(args->result->elapsed_ms),
                args->result->fps,
                args->result->avg_ms_per_frame) < 0)
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
    const BenchConfig& cfg,
    CsvErrorStage* error,
    int* error_number)
{
    CsvWriteArgs args = {
        &result,
        &cfg,
        CsvErrorStage::WorldSwitch,
        0
    };

    const int switch_result = gint_world_switch(
        GINT_CALL(append_csv_in_os_world, &args));

    // If the callback did not run, retain the world-switch failure as the
    // stage and preserve the switch result as its diagnostic code.
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
        result, cfg, &benchmark_csv_error[index], &benchmark_csv_errno[index]);
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

static void draw_results()
{
    dclear(C_BLACK);
    dtext(8, 8, C_WHITE, "Benchmark results");

    for (size_t i = 0; i < BENCHMARK_COUNT; ++i)
    {
        const int y = 24 + static_cast<int>(i) * 39;
        dtext(8, y, C_WHITE, bench_name(BENCHMARKS[i]));
        if (benchmark_valid[i])
        {
            char fps[32];
            char ms[32];
            char error_number[20];
            snprintf(fps, sizeof(fps), "FPS %.2f", benchmark_results[i].fps);
            snprintf(ms, sizeof(ms), "%.2f ms/frame", benchmark_results[i].avg_ms_per_frame);
            dtext(8, y + 15, C_WHITE, fps);
            dtext(145, y + 15, C_WHITE, ms);
            dtext(300, y + 15, C_WHITE,
                benchmark_logged[i] ? "CSV OK" : csv_error_name(benchmark_csv_error[i]));
            if (!benchmark_logged[i])
            {
                if (benchmark_csv_error[i] == CsvErrorStage::WorldSwitch)
                    snprintf(error_number, sizeof(error_number), "code %d", benchmark_csv_errno[i]);
                else
                    snprintf(error_number, sizeof(error_number), "errno %d", benchmark_csv_errno[i]);
                dtext(300, y + 27, C_WHITE, error_number);
            }
        }
        else
        {
            dtext(8, y + 15, C_WHITE, "FAILED (timer or scene)");
        }
    }

    dtext(8, 190, C_WHITE, "F6 clear / F5 solid / EXE triangles / DEL UV");
    dtext(8, 207, C_WHITE, "MENU: quit");
}

int main()
{
    init_uv_map();

    BenchConfig cfg;
    cfg.warmup_frames = 5;
    cfg.measure_frames = 60;
    cfg.triangle_count = 64;
    cfg.width = SCREEN_WIDTH;
    cfg.height = SCREEN_HEIGHT;

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
        else if (keydown(KEY_F5))
            selected = 1;
        else if (keydown(KEY_EXE))
            selected = 2;
        else if (keydown(KEY_DEL))
            selected = 3;

        if (selected < BENCHMARK_COUNT)
        {
            dclear(C_BLACK);
            dtext(8, 8, C_WHITE, "Running selected benchmark...");
            dtext(8, 28, C_WHITE, bench_name(BENCHMARKS[selected]));
            dupdate();
            run_and_store(selected, cfg);
            while (keydown(KEY_F6) || keydown(KEY_F5) || keydown(KEY_EXE) || keydown(KEY_DEL))
                clearevents();
        }
    }

    return 0;
}
