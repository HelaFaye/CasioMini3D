#pragma once

#include <stddef.h>
#include <stdint.h>

namespace fxglmark2
{

constexpr size_t MAX_OPTIONS = 4;
constexpr size_t OPTION_NAME_BYTES = 24;
constexpr size_t OPTION_VALUE_BYTES = 40;

/* Scene identifiers follow the corresponding upstream glmark2 scene names.
   This is a native fx-CG50 port, not the Linux/EGL window-system executable. */
enum class SceneId : uint8_t
{
    Clear,
    Build,
    Texture,
    Shading,
    Buffer,
    Bump,
    Effect2D,
    Pulsar,
    Desktop,
    Ideas,
    Jellyfish,
    Terrain,
    Shadow,
    Refract,
    Conditionals,
    Function,
    Loop
};

struct Option
{
    char name[OPTION_NAME_BYTES];
    char value[OPTION_VALUE_BYTES];
};

struct BenchmarkRequest
{
    SceneId scene;
    bool use_vbo;
    uint8_t texture_filter; /* 0 = nearest, 1 = linear */
    uint8_t shading_model;  /* 0 = Gouraud */
    Option options[MAX_OPTIONS];
    size_t option_count;
};

struct SceneInfo
{
    const char *name;
    const char *description;
    bool implemented;
};

struct SceneInstance
{
    BenchmarkRequest request;
    uint32_t vertex_count;
    uint32_t frame_number;
    uint32_t triangle_count;
    unsigned int vertex_shader;
    unsigned int fragment_shader;
    unsigned int program;
    unsigned int vertex_buffer;
    unsigned int texture;
    int position_location;
    int normal_or_uv_location;
    int mvp_location;
    int model_location;
    int light_location;
    int texture_location;
    bool prepared;
};

struct RunConfig
{
    uint32_t warmup_frames;
    uint32_t measure_frames;
    uint32_t repeat_count;
};

struct BenchmarkResult
{
    char description[96];
    uint32_t frames;
    uint32_t triangles_per_frame;
    double min_frame_us;
    double max_frame_us;
    double mean_frame_us;
    double median_frame_us;
    double fps;
};

const SceneInfo *scene_info(size_t index);
size_t scene_count();
size_t implemented_scene_count();
const char *scene_name(SceneId scene);

/* Parse glmark2-style descriptions, e.g.
   "build:use-vbo=true" or "texture:texture-filter=linear".
   Unsupported scenes/options fail explicitly with a diagnostic. */
bool parse_benchmark(
    const char *description,
    BenchmarkRequest& request,
    char *error,
    size_t error_capacity);

/* Execute a scene against the already-current fxgl GLES context. */
bool prepare_scene(
    const BenchmarkRequest& request,
    SceneInstance& instance,
    char *error,
    size_t error_capacity);
bool render_scene_frame(SceneInstance& instance, uint32_t frame_number);
void finish_scene(SceneInstance& instance);

/* The initial default set maps directly to supported glmark2 workloads:
   client arrays, VBOs, nearest/linear texturing, and Gouraud shading. */
enum class BatteryId : uint8_t
{
    QuickSmoke,
    Glmark2SupportedDefault,
    AllImplemented
};

const char *battery_name(BatteryId battery);
size_t battery_size(BatteryId battery);
bool battery_request(BatteryId battery, size_t index, BenchmarkRequest& request,
    char *error, size_t error_capacity);

bool run_benchmark(const BenchmarkRequest& request, const RunConfig& config,
    BenchmarkResult& result, char *error, size_t error_capacity);

const char *default_benchmark_description(size_t index);
size_t default_benchmark_count();

} // namespace fxglmark2
