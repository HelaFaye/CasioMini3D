#include "display.hpp"
#include "glmark2/glmark2.hpp"

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
    dtext(8, 44, C_WHITE, "F3 All Implemented (6)   F4 Pending");
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

static void show_pending_scenes()
{
    dclear(C_BLACK);
    dtext(8, 8, C_WHITE, "PENDING glmark2 SCENE FAMILIES");
    dtext(8, 30, C_WHITE, "buffer       - dynamic/static update and map/subdata");
    dtext(8, 46, C_WHITE, "bump         - normal and height-map shading");
    dtext(8, 62, C_WHITE, "effect2d     - convolution and post-processing");
    dtext(8, 78, C_WHITE, "pulsar       - animated textured quad workloads");
    dtext(8, 94, C_WHITE, "desktop      - multi-window blur and shadows");
    dtext(8, 110, C_WHITE, "ideas        - animated shader effects");
    dtext(8, 126, C_WHITE, "jellyfish    - animated multi-object geometry");
    dtext(8, 142, C_WHITE, "terrain      - height-field rendering");
    dtext(8, 158, C_WHITE, "shadow       - shadow-map rendering");
    dtext(8, 174, C_WHITE, "refract      - environment/refraction");
    dtext(8, 190, C_WHITE, "conditionals / function / loop - shader stress");
    dtext(8, 214, C_WHITE, "Not selectable yet | EXE or MENU to return");
    dupdate();
    while (!keydown(KEY_EXE) && !keydown(KEY_MENU))
        clearevents();
    while (keydown(KEY_EXE) || keydown(KEY_MENU))
        clearevents();
}

static void wait_for_key_release()
{
    while (keydown(KEY_UP) || keydown(KEY_DOWN) ||
           keydown(KEY_EXE) || keydown(KEY_EXIT) || keydown(KEY_F1) ||
           keydown(KEY_F2) || keydown(KEY_F3) ||
           keydown(KEY_F5) || keydown(KEY_F6))
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
            show_pending_scenes();
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
