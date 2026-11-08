#include "display.hpp"
#include "glmark2/glmark2.hpp"
#include "gfx/texture.hpp"
#include "gles/glsl.hpp"
#include "gles/pipeline.hpp"

#include <GLES2/gl2.h>
#include <fxgl/platform.h>

#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>

#include <fxlibc/printf.h>
#include <libprof.h>
#include <gint/gint.h>

namespace
{
constexpr size_t TEST_COUNT = 6;
static const char *const TEST_DESCRIPTIONS[TEST_COUNT] = {
    "clear",
    "build:use-vbo=false",
    "build:use-vbo=true",
    "texture:texture-filter=nearest",
    "texture:texture-filter=linear",
    "shading:shading=gouraud"
};
static const char *const TEST_LABELS[TEST_COUNT] = {
    "Clear baseline",
    "Build / client arrays",
    "Build / VBO",
    "Texture / nearest",
    "Texture / linear",
    "Shading / Gouraud"
};

static bool g_selected[TEST_COUNT] = {};
static size_t g_cursor = 0;
static char g_status[64] = "Choose individual tests or a battery";
static uint16_t g_depth_buffer[DWIDTH * DHEIGHT];

constexpr uint32_t MICROBENCH_REPEATS = 3;
constexpr size_t MICROBENCH_COUNT = 6;
constexpr uint32_t MICROBENCH_TEXTURE_SAMPLES = 16384;
constexpr uint32_t MICROBENCH_RASTER_TRIANGLES = 8;
constexpr uint32_t MICROBENCH_SHADER_EXECUTIONS = 1024;
constexpr uint32_t MICROBENCH_PRESENT_CALLS = 8;

struct RendererMicrobenchResult
{
    const char *stage;
    uint32_t iterations;
    double min_us_per_iteration;
    double max_us_per_iteration;
    double mean_us_per_iteration;
    double operations_per_second;
};

static uint16_t g_microbench_texture_pixels[32 * 32];
static volatile uint16_t g_microbench_texture_sink = 0;
static volatile float g_microbench_shader_sink = 0.0f;
static fxgl::RenderSurface g_microbench_surface = {};
static fxgl::PipelineState g_microbench_pipeline_state = {};
static fxgl::ClipVertex g_microbench_triangle[3] = {};
static fxgl::ShaderModule g_microbench_shader = {};
static fxgl::ShaderRuntime g_microbench_runtime = {};
static bool g_microbench_raster_ready = false;
static bool g_microbench_shader_ready = false;

struct CsvWriteArgs
{
    const fxglmark2::BenchmarkResult *result;
    int error_number;
};

static int append_result_csv_os_world(void *opaque)
{
    CsvWriteArgs *args = static_cast<CsvWriteArgs *>(opaque);
    args->error_number = 0;
    static const char path[] = "/glmark2-fx-cg50-v1.csv";
    static const char header[] =
        "schema_version,benchmark,frames,triangles_per_frame,"
        "min_frame_us,max_frame_us,mean_frame_us,median_frame_us,fps\n";

    struct stat before = {};
    long long expected = 0;
    errno = 0;
    if (stat(path, &before) == 0)
    {
        if (before.st_size < 0)
        {
            args->error_number = EIO;
            return -1;
        }
        expected = static_cast<long long>(before.st_size);
    }
    else if (errno != ENOENT)
    {
        args->error_number = errno;
        return -1;
    }

    errno = 0;
    FILE *file = fopen(path, "a");
    if (!file)
    {
        args->error_number = errno;
        return -1;
    }

    bool failed = false;
    if (expected == 0)
    {
        const int count = fprintf(file, "%s", header);
        if (count < 0)
            failed = true;
        else
            expected += static_cast<long long>(sizeof(header) - 1);
    }

    const fxglmark2::BenchmarkResult& r = *args->result;
    if (!failed)
    {
        const int count = fprintf(file,
            "1,%s,%lu,%lu,%.3f,%.3f,%.3f,%.3f,%.3f\n",
            r.description,
            static_cast<unsigned long>(r.frames),
            static_cast<unsigned long>(r.triangles_per_frame),
            r.min_frame_us, r.max_frame_us, r.mean_frame_us,
            r.median_frame_us, r.fps);
        if (count < 0)
            failed = true;
        else
            expected += count;
    }

    if (!failed && fflush(file) != 0)
        failed = true;
    if (ferror(file))
        failed = true;
    if (fclose(file) != 0)
        failed = true;

    if (!failed)
    {
        struct stat after = {};
        if (stat(path, &after) != 0 ||
            static_cast<long long>(after.st_size) != expected)
        {
            failed = true;
            args->error_number = errno ? errno : EIO;
        }
    }

    if (failed && args->error_number == 0)
        args->error_number = errno ? errno : EIO;
    return failed ? -1 : 0;
}

static bool append_result_csv(const fxglmark2::BenchmarkResult& result)
{
    CsvWriteArgs args = {&result, 0};
    const int status = gint_world_switch(
        GINT_CALL(append_result_csv_os_world, &args));
    if (status != 0 && args.error_number == 0)
        args.error_number = status;
    return status == 0 && args.error_number == 0;
}

struct MicrobenchCsvWriteArgs
{
    const RendererMicrobenchResult *result;
    int error_number;
};

static int append_microbench_csv_os_world(void *opaque)
{
    MicrobenchCsvWriteArgs *args = static_cast<MicrobenchCsvWriteArgs *>(opaque);
    args->error_number = 0;
    static const char path[] = "/glmark2-renderer-microbench-v1.csv";
    static const char header[] =
        "schema_version,stage,iterations,min_us_per_iteration,"
        "max_us_per_iteration,mean_us_per_iteration,operations_per_second\n";

    struct stat before = {};
    long long expected = 0;
    errno = 0;
    if (stat(path, &before) == 0)
    {
        if (before.st_size < 0)
        {
            args->error_number = EIO;
            return -1;
        }
        expected = static_cast<long long>(before.st_size);
    }
    else if (errno != ENOENT)
    {
        args->error_number = errno;
        return -1;
    }

    errno = 0;
    FILE *file = fopen(path, "a");
    if (!file)
    {
        args->error_number = errno;
        return -1;
    }

    bool failed = false;
    if (expected == 0)
    {
        const int count = fprintf(file, "%s", header);
        if (count < 0)
            failed = true;
        else
            expected += static_cast<long long>(sizeof(header) - 1);
    }

    const RendererMicrobenchResult& result = *args->result;
    if (!failed)
    {
        const int count = fprintf(file,
            "1,%s,%lu,%.3f,%.3f,%.3f,%.3f\n",
            result.stage,
            static_cast<unsigned long>(result.iterations),
            result.min_us_per_iteration,
            result.max_us_per_iteration,
            result.mean_us_per_iteration,
            result.operations_per_second);
        if (count < 0)
            failed = true;
        else
            expected += count;
    }

    if (!failed && fflush(file) != 0)
        failed = true;
    if (ferror(file))
        failed = true;
    if (fclose(file) != 0)
        failed = true;

    if (!failed)
    {
        struct stat after = {};
        if (stat(path, &after) != 0 ||
            static_cast<long long>(after.st_size) != expected)
        {
            failed = true;
            args->error_number = errno ? errno : EIO;
        }
    }

    if (failed && args->error_number == 0)
        args->error_number = errno ? errno : EIO;
    return failed ? -1 : 0;
}

static bool append_microbench_csv(const RendererMicrobenchResult& result)
{
    MicrobenchCsvWriteArgs args = {&result, 0};
    const int status = gint_world_switch(
        GINT_CALL(append_microbench_csv_os_world, &args));
    if (status != 0 && args.error_number == 0)
        args.error_number = status;
    return status == 0 && args.error_number == 0;
}

static bool microbench_fragment(
    const fxgl::FragmentInput&,
    float rgba[4],
    void *)
{
    rgba[0] = 0.22f;
    rgba[1] = 0.58f;
    rgba[2] = 0.91f;
    rgba[3] = 1.0f;
    return true;
}

static bool prepare_renderer_microbenchmarks()
{
    for (size_t i = 0; i < sizeof(g_microbench_texture_pixels) /
                            sizeof(g_microbench_texture_pixels[0]); ++i)
    {
        const unsigned x = static_cast<unsigned>(i % 32);
        const unsigned y = static_cast<unsigned>(i / 32);
        const unsigned red = (x * 7u + y * 3u) & 0xffu;
        const unsigned green = (x * 5u + y * 11u) & 0xffu;
        const unsigned blue = (x * 13u + y * 2u) & 0xffu;
        g_microbench_texture_pixels[i] = static_cast<uint16_t>(
            ((red & 0xf8u) << 8) |
            ((green & 0xfcu) << 3) |
            ((blue & 0xf8u) >> 3));
    }

    const char *fragment_source =
        "#version 100\n"
        "precision mediump float;\n"
        "varying vec4 v_color;\n"
        "void main() {\n"
        "  vec3 lit = v_color.rgb * 0.75 + vec3(0.125);\n"
        "  gl_FragColor = vec4(lit, v_color.a);\n"
        "}\n";
    g_microbench_shader_ready = fxgl::compile_glsl(
        fxgl::ShaderStage::Fragment, fragment_source, g_microbench_shader);
    if (!g_microbench_shader_ready)
        return false;

    g_microbench_runtime = {};
    g_microbench_runtime.varyings[0].type = fxgl::ShaderType::Vec4;
    g_microbench_runtime.varyings[0].data[0] = 0.25f;
    g_microbench_runtime.varyings[0].data[1] = 0.50f;
    g_microbench_runtime.varyings[0].data[2] = 0.75f;
    g_microbench_runtime.varyings[0].data[3] = 1.0f;

    fxgl::pipeline_default_state(
        g_microbench_pipeline_state, SCREEN_WIDTH, SCREEN_HEIGHT);
    g_microbench_triangle[0].position = {-0.72f, -0.72f, 0.0f, 1.0f};
    g_microbench_triangle[1].position = {0.18f, -0.72f, 0.0f, 1.0f};
    g_microbench_triangle[2].position = {-0.27f, 0.20f, 0.0f, 1.0f};
    g_microbench_raster_ready = true;
    return true;
}

static bool benchmark_texture_sampling(uint32_t iterations)
{
    const gfx::Texture2DView texture = {
        g_microbench_texture_pixels, 32, 32, 32
    };
    const gfx::TextureSampler sampler = {
        gfx::TextureFilter::Nearest,
        gfx::TextureAddressMode::Clamp,
        gfx::TextureAddressMode::Clamp
    };

    uint16_t checksum = 0;
    for (uint32_t i = 0; i < iterations; ++i)
    {
        const int64_t u = static_cast<int64_t>(
            ((i * 977u) & 0x001fffffu));
        const int64_t v = static_cast<int64_t>(
            ((i * 3253u + 7919u) & 0x001fffffu));
        checksum = static_cast<uint16_t>(checksum ^
            gfx::sample_texture_validated(texture, u, v, sampler));
    }
    g_microbench_texture_sink = checksum;
    return true;
}

static bool benchmark_texture_nearest_clamp(uint32_t iterations)
{
    const gfx::Texture2DView texture = {
        g_microbench_texture_pixels, 32, 32, 32
    };
    const gfx::TextureSampler sampler = {
        gfx::TextureFilter::Nearest,
        gfx::TextureAddressMode::Clamp,
        gfx::TextureAddressMode::Clamp
    };

    uint16_t checksum = 0;
    for (uint32_t i = 0; i < iterations; ++i)
    {
        // Generate stable normalized coordinates strictly inside [0, 1).
        // This exercises the same nearest-normalized sampler entry point
        // called by the GLES fragment callback, without fixed-point setup.
        const uint32_t u_bits = (i * 977u + 12345u) & 0x00ffffffu;
        const uint32_t v_bits = (i * 3253u + 7919u) & 0x00ffffffu;
        const float u = static_cast<float>(u_bits) * (1.0f / 16777216.0f);
        const float v = static_cast<float>(v_bits) * (1.0f / 16777216.0f);

        uint16_t pixel = 0;
        if (!gfx::sample_texture_nearest_validated(
                texture, u, v, sampler, pixel))
            return false;
        checksum = static_cast<uint16_t>(checksum ^ pixel);
    }
    g_microbench_texture_sink = checksum;
    return true;
}

static bool benchmark_texture_nearest_repeat(uint32_t iterations)
{
    const gfx::Texture2DView texture = {
        g_microbench_texture_pixels, 32, 32, 32
    };
    const gfx::TextureSampler sampler = {
        gfx::TextureFilter::Nearest,
        gfx::TextureAddressMode::Repeat,
        gfx::TextureAddressMode::Repeat
    };

    uint16_t checksum = 0;
    for (uint32_t i = 0; i < iterations; ++i)
    {
        // Match the [0, 6) repeated UV domain of the textured glmark2 sphere.
        const uint32_t u_bits = (i * 977u + 12345u) & 0x00ffffffu;
        const uint32_t v_bits = (i * 3253u + 7919u) & 0x00ffffffu;
        const float u = static_cast<float>(u_bits) *
            (6.0f / 16777216.0f);
        const float v = static_cast<float>(v_bits) *
            (6.0f / 16777216.0f);

        uint16_t pixel = 0;
        if (!gfx::sample_texture_nearest_validated(
                texture, u, v, sampler, pixel))
            return false;
        checksum = static_cast<uint16_t>(checksum ^ pixel);
    }
    g_microbench_texture_sink = checksum;
    return true;
}

static bool benchmark_triangle_rasterization(uint32_t iterations)
{
    if (!g_microbench_raster_ready || !g_microbench_surface.color)
        return false;

    for (uint32_t i = 0; i < iterations; ++i)
    {
        if (!fxgl::draw_triangle(g_microbench_surface,
                g_microbench_pipeline_state, g_microbench_triangle, 0,
                microbench_fragment, nullptr))
            return false;
    }
    return true;
}

static bool benchmark_shader_execution(uint32_t iterations)
{
    if (!g_microbench_shader_ready)
        return false;

    float checksum = 0.0f;
    for (uint32_t i = 0; i < iterations; ++i)
    {
        if (!fxgl::execute_glsl(g_microbench_shader, g_microbench_runtime))
            return false;
        checksum += g_microbench_runtime.fragment_color.data[0];
    }
    g_microbench_shader_sink = checksum;
    return true;
}

static bool benchmark_framebuffer_presentation(uint32_t iterations)
{
    for (uint32_t i = 0; i < iterations; ++i)
        dupdate();
    return true;
}

using MicrobenchWork = bool (*)(uint32_t iterations);

static bool measure_microbench_stage(
    const char *name,
    uint32_t iterations,
    MicrobenchWork work,
    RendererMicrobenchResult& result)
{
    if (!name || !work || iterations == 0 || !work(1))
        return false;

    double samples[MICROBENCH_REPEATS] = {};
    for (uint32_t repeat = 0; repeat < MICROBENCH_REPEATS; ++repeat)
    {
        prof_t timer = prof_make();
        prof_enter(timer);
        const bool completed = work(iterations);
        prof_leave(timer);
        if (!completed)
            return false;
        samples[repeat] = prof_time(timer) /
            static_cast<double>(iterations);
    }

    double sum = 0.0;
    double minimum = samples[0];
    double maximum = samples[0];
    for (uint32_t i = 0; i < MICROBENCH_REPEATS; ++i)
    {
        sum += samples[i];
        if (samples[i] < minimum) minimum = samples[i];
        if (samples[i] > maximum) maximum = samples[i];
    }

    result.stage = name;
    result.iterations = iterations;
    result.min_us_per_iteration = minimum;
    result.max_us_per_iteration = maximum;
    result.mean_us_per_iteration =
        sum / static_cast<double>(MICROBENCH_REPEATS);
    result.operations_per_second =
        result.mean_us_per_iteration > 0.0
            ? 1000000.0 / result.mean_us_per_iteration : 0.0;
    return true;
}

static void show_renderer_microbench_results(
    const RendererMicrobenchResult *results,
    const bool *valid,
    const bool *logged)
{
    dclear(C_BLACK);
    dtext(8, 5, C_WHITE, "Renderer component microbenchmarks");
    for (size_t i = 0; i < MICROBENCH_COUNT; ++i)
    {
        // Use two lines per stage. The previous three-line rows overlapped
        // because the final metric line was only four pixels from the next
        // stage label on the 224px display.
        const int y = 19 + static_cast<int>(i) * 28;
        if (!valid[i])
        {
            dtext(8, y, C_WHITE, "FAILED: component benchmark");
            continue;
        }

        char timing[72];
        char throughput[24];
        snprintf(timing, sizeof(timing),
            "min/max/mean %.2f/%.2f/%.2f us",
            results[i].min_us_per_iteration,
            results[i].max_us_per_iteration,
            results[i].mean_us_per_iteration);
        if (results[i].operations_per_second >= 1000.0)
            snprintf(throughput, sizeof(throughput), "%.1fk/s",
                results[i].operations_per_second / 1000.0);
        else
            snprintf(throughput, sizeof(throughput), "%.1f/s",
                results[i].operations_per_second);

        dtext(8, y, C_WHITE, results[i].stage);
        dtext(248, y, C_WHITE, throughput);
        dtext(340, y, C_WHITE, logged[i] ? "CSV OK" : "CSV ERR");
        dtext(8, y + 12, C_WHITE, timing);
    }
    dtext(8, 190, C_WHITE, "Saved to /glmark2-renderer-microbench-v1.csv");
    dtext(8, 207, C_WHITE, "EXE or MENU to return");
    dupdate();
    while (!keydown(KEY_EXE) && !keydown(KEY_MENU))
        clearevents();
    while (keydown(KEY_EXE) || keydown(KEY_MENU))
        clearevents();
}

static void run_renderer_microbenchmarks()
{
    static const char *const names[MICROBENCH_COUNT] = {
        "texture-sample-fixed",
        "texture-nearest-clamp",
        "texture-nearest-repeat",
        "triangle-raster",
        "shader-vm",
        "framebuffer-present"
    };
    static const uint32_t iterations[MICROBENCH_COUNT] = {
        MICROBENCH_TEXTURE_SAMPLES,
        MICROBENCH_TEXTURE_SAMPLES,
        MICROBENCH_TEXTURE_SAMPLES,
        MICROBENCH_RASTER_TRIANGLES,
        MICROBENCH_SHADER_EXECUTIONS,
        MICROBENCH_PRESENT_CALLS
    };
    static const MicrobenchWork work[MICROBENCH_COUNT] = {
        benchmark_texture_sampling,
        benchmark_texture_nearest_clamp,
        benchmark_texture_nearest_repeat,
        benchmark_triangle_rasterization,
        benchmark_shader_execution,
        benchmark_framebuffer_presentation
    };

    RendererMicrobenchResult results[MICROBENCH_COUNT] = {};
    bool valid[MICROBENCH_COUNT] = {};
    bool logged[MICROBENCH_COUNT] = {};

    if (!prepare_renderer_microbenchmarks())
    {
        dclear(C_BLACK);
        dtext(8, 16, C_WHITE, "Could not prepare renderer microbenchmarks");
        dtext(8, 205, C_WHITE, "EXE or MENU to return");
        dupdate();
        while (!keydown(KEY_EXE) && !keydown(KEY_MENU))
            clearevents();
        while (keydown(KEY_EXE) || keydown(KEY_MENU))
            clearevents();
        return;
    }

    for (size_t i = 0; i < MICROBENCH_COUNT; ++i)
    {
        dclear(C_BLACK);
        dtext(8, 8, C_WHITE, "Running renderer microbenchmarks...");
        dtext(8, 27, C_WHITE, names[i]);
        dupdate();

        if (i == 3)
        {
            g_microbench_surface.color =
                reinterpret_cast<uint16_t *>(gint_vram);
            g_microbench_surface.depth = nullptr;
            g_microbench_surface.stencil = nullptr;
            g_microbench_surface.width = SCREEN_WIDTH;
            g_microbench_surface.height = SCREEN_HEIGHT;
            g_microbench_surface.stride = DWIDTH;
        }

        valid[i] = measure_microbench_stage(
            names[i], iterations[i], work[i], results[i]);
        if (valid[i])
            logged[i] = append_microbench_csv(results[i]);
    }

    show_renderer_microbench_results(results, valid, logged);
    snprintf(g_status, sizeof(g_status), "Renderer microbenchmarks complete");
}

static void release_all_selection()
{
    for (bool& selected : g_selected)
        selected = false;
}

static void select_battery(fxglmark2::BatteryId battery)
{
    release_all_selection();
    const size_t count = fxglmark2::battery_size(battery);
    for (size_t item = 0; item < count; ++item)
    {
        fxglmark2::BenchmarkRequest request = {};
        char error[96] = {};
        if (!fxglmark2::battery_request(
                battery, item, request, error, sizeof(error)))
            continue;

        for (size_t test = 0; test < TEST_COUNT; ++test)
        {
            fxglmark2::BenchmarkRequest candidate = {};
            if (fxglmark2::parse_benchmark(
                    TEST_DESCRIPTIONS[test], candidate, error, sizeof(error)) &&
                candidate.scene == request.scene &&
                candidate.use_vbo == request.use_vbo &&
                candidate.texture_filter == request.texture_filter)
                g_selected[test] = true;
        }
    }

    snprintf(g_status, sizeof(g_status), "Selected: %s",
             fxglmark2::battery_name(battery));
}

static size_t selected_count()
{
    size_t count = 0;
    for (bool selected : g_selected)
        if (selected) ++count;
    return count;
}

static void draw_menu()
{
    dclear(C_BLACK);
    dtext(8, 5, C_WHITE, "fxGLMark2 | fx-CG50 graphics benchmark");
    dtext(8, 19, C_WHITE, "STANDARD BATTERIES");
    dtext(8, 32, C_WHITE, "F1 Quick Smoke (2)   F2 glmark2 Supported Default (5)");
    dtext(8, 44, C_WHITE, "F3 All Implemented (6)   F4 Renderer Microbench");
    dtext(8, 61, C_WHITE, "SELECT TESTS  [EXE/F6 toggle]  [UP/DOWN move]");

    for (size_t i = 0; i < TEST_COUNT; ++i)
    {
        char line[64];
        snprintf(line, sizeof(line), "%c [%c] %s",
            i == g_cursor ? '>' : ' ',
            g_selected[i] ? 'X' : ' ',
            TEST_LABELS[i]);
        dtext(8, 78 + static_cast<int>(i) * 17,
              i == g_cursor ? C_WHITE : C_WHITE, line);
    }

    char selection[48];
    snprintf(selection, sizeof(selection), "Selected: %lu / %lu",
        static_cast<unsigned long>(selected_count()),
        static_cast<unsigned long>(TEST_COUNT));
    dtext(8, 185, C_WHITE, selection);
    dtext(8, 198, C_WHITE, "F5 RUN SELECTED    EXIT cancels run");
    dtext(8, 211, C_WHITE, g_status);
    dupdate();
}

static void show_message(const char *title, const char *line1, const char *line2)
{
    dclear(C_BLACK);
    dtext(8, 12, C_WHITE, title);
    if (line1) dtext(8, 38, C_WHITE, line1);
    if (line2) dtext(8, 54, C_WHITE, line2);
    dtext(8, 205, C_WHITE, "EXE or MENU to return");
    dupdate();
    while (!keydown(KEY_EXE) && !keydown(KEY_MENU))
        clearevents();
    while (keydown(KEY_EXE) || keydown(KEY_MENU))
        clearevents();
}

static void show_results(
    const fxglmark2::BenchmarkResult *results,
    const bool *valid,
    const bool *logged,
    size_t count)
{
    dclear(C_BLACK);
    dtext(8, 5, C_WHITE, "fxGLMark2 results | mean frame time and FPS");

    for (size_t i = 0; i < count; ++i)
    {
        const int y = 23 + static_cast<int>(i) * 29;
        if (!valid[i])
        {
            dtext(8, y, C_WHITE, "FAILED: scene setup or draw");
            continue;
        }

        char name[48];
        char metrics[72];
        snprintf(name, sizeof(name), "%.47s", results[i].description);
        snprintf(metrics, sizeof(metrics), "%.1f us/frame  %.2f FPS  %s",
            results[i].mean_frame_us, results[i].fps,
            logged[i] ? "CSV OK" : "CSV ERROR");
        dtext(8, y, C_WHITE, name);
        dtext(8, y + 12, C_WHITE, metrics);
    }

    dtext(8, 203, C_WHITE, "Saved to /glmark2-fx-cg50-v1.csv");
    dtext(8, 215, C_WHITE, "EXE or MENU to return");
    dupdate();
    while (!keydown(KEY_EXE) && !keydown(KEY_MENU))
        clearevents();
    while (keydown(KEY_EXE) || keydown(KEY_MENU))
        clearevents();
}

struct ProgressContext
{
    fxgl_surface_t *surface;
    const char *label;
    bool cancelled;
    bool surface_error;
};

static bool present_benchmark_frame(
    uint32_t frame_index,
    uint32_t total_frames,
    void *user_data)
{
    ProgressContext *progress = static_cast<ProgressContext *>(user_data);
    if (!progress || !progress->surface)
        return false;

    clearevents();
    const bool cancel = keydown(KEY_EXIT) != 0;

    char line[64];
    snprintf(line, sizeof(line), "Frame %lu / %lu",
        static_cast<unsigned long>(frame_index + 1),
        static_cast<unsigned long>(total_frames));

    dtext(8, 4, C_WHITE, "fxGLMark2 running - EXIT cancels");
    dtext(8, 17, C_WHITE, progress->label);
    dtext(8, 30, C_WHITE, line);
    if (cancel)
    {
        dtext(8, 43, C_WHITE, "Cancelling after this frame...");
        progress->cancelled = true;
    }

    dupdate();

    // Gint may switch the active VRAM pointer during dupdate(). Refresh only
    // the default surface address; keep shader, buffer, texture and GL state.
    progress->surface->color = gint_vram;
    if (!fxglUpdateSurface(progress->surface))
    {
        progress->surface_error = true;
        return false;
    }
    return !cancel;
}

static void run_selected_benchmarks()
{
    const size_t total_selected = selected_count();
    if (total_selected == 0)
    {
        snprintf(g_status, sizeof(g_status), "Select at least one test first");
        return;
    }

    fxglmark2::BenchmarkResult results[TEST_COUNT] = {};
    bool valid[TEST_COUNT] = {};
    bool logged[TEST_COUNT] = {};
    size_t completed = 0;

    fxgl_surface_t surface = {};
    surface.color = reinterpret_cast<uint16_t *>(gint_vram);
    surface.depth = g_depth_buffer;
    surface.stencil = nullptr;
    surface.width = SCREEN_WIDTH;
    surface.height = SCREEN_HEIGHT;
    surface.stride = DWIDTH;

    if (!fxglMakeCurrent(&surface))
    {
        show_message("GLES context error",
            "Could not bind calculator framebuffer",
            "No benchmarks were started");
        return;
    }

    const fxglmark2::RunConfig config = {1, 12, 2};
    for (size_t i = 0; i < TEST_COUNT; ++i)
    {
        if (!g_selected[i])
            continue;

        dclear(C_BLACK);
        dtext(8, 8, C_WHITE, "Running selected benchmark...");
        dtext(8, 28, C_WHITE, TEST_LABELS[i]);
        dupdate();

        fxglmark2::BenchmarkRequest request = {};
        char error[160] = {};
        ProgressContext progress = {
            &surface, TEST_LABELS[i], false, false
        };
        const bool parsed = fxglmark2::parse_benchmark(
            TEST_DESCRIPTIONS[i], request, error, sizeof(error));
        const bool ran = parsed && fxglmark2::run_benchmark(
            request, config, results[completed], error, sizeof(error),
            present_benchmark_frame, &progress);

        if (ran)
        {
            valid[completed] = true;
            logged[completed] = append_result_csv(results[completed]);
            ++completed;
        }
        else
        {
            const char *failure = progress.surface_error
                ? "VRAM surface refresh failed"
                : progress.cancelled ? "Cancelled by user" : error;
            snprintf(results[completed].description,
                sizeof(results[completed].description), "%.95s",
                progress.cancelled || progress.surface_error
                    ? failure : TEST_LABELS[i]);
            valid[completed] = false;
            ++completed;
            snprintf(g_status, sizeof(g_status), "%.63s", failure);
            if (progress.cancelled || progress.surface_error)
                break;
        }
    }

    fxglReleaseCurrent();
    show_results(results, valid, logged, completed);
    snprintf(g_status, sizeof(g_status), "Last run: %lu benchmark(s)",
             static_cast<unsigned long>(completed));
}

static void wait_for_key_release()
{
    while (keydown(KEY_UP) || keydown(KEY_DOWN) ||
           keydown(KEY_EXE) || keydown(KEY_EXIT) || keydown(KEY_F1) ||
           keydown(KEY_F2) || keydown(KEY_F3) ||
           keydown(KEY_F4) || keydown(KEY_F5) || keydown(KEY_F6))
        clearevents();
}

} // namespace

int main()
{
    __printf_enable_fp();
    prof_init();

    dclear(C_BLACK);
    dtext(8, 8, C_WHITE, "fxGLMark2 initializing...");
    dupdate();

    // Start with a short, representative smoke selection; no benchmark runs
    // automatically. The user explicitly starts a battery or selected tests.
    select_battery(fxglmark2::BatteryId::QuickSmoke);

    while (!keydown(KEY_MENU))
    {
        clearevents();
        draw_menu();

        if (keydown(KEY_UP))
        {
            if (g_cursor > 0) --g_cursor;
            wait_for_key_release();
        }
        else if (keydown(KEY_DOWN))
        {
            if (g_cursor + 1 < TEST_COUNT) ++g_cursor;
            wait_for_key_release();
        }
        else if (keydown(KEY_EXE) || keydown(KEY_F6))
        {
            g_selected[g_cursor] = !g_selected[g_cursor];
            snprintf(g_status, sizeof(g_status), "%s: %s",
                TEST_LABELS[g_cursor],
                g_selected[g_cursor] ? "selected" : "deselected");
            wait_for_key_release();
        }
        else if (keydown(KEY_F1))
        {
            select_battery(fxglmark2::BatteryId::QuickSmoke);
            wait_for_key_release();
        }
        else if (keydown(KEY_F2))
        {
            select_battery(fxglmark2::BatteryId::Glmark2SupportedDefault);
            wait_for_key_release();
        }
        else if (keydown(KEY_F3))
        {
            select_battery(fxglmark2::BatteryId::AllImplemented);
            wait_for_key_release();
        }
        else if (keydown(KEY_F4))
        {
            run_renderer_microbenchmarks();
            wait_for_key_release();
        }
        else if (keydown(KEY_F5))
        {
            run_selected_benchmarks();
            wait_for_key_release();
        }
    }

    prof_quit();
    return 0;
}
