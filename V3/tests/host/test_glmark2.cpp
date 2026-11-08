#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>

#include <GLES2/gl2.h>
#include <fxgl/platform.h>
#include "../../src/glmark2/glmark2.hpp"

uint32_t fxmark_test_prof_counter = 0;
static uint32_t presentation_count = 0;

void dupdate(void)
{
    ++presentation_count;
    fxmark_test_prof_counter += 100;
}

namespace
{
static uint16_t color[396 * 224];
static uint16_t depth[396 * 224];

static void reset_surface()
{
    std::memset(color, 0, sizeof(color));
    std::memset(depth, 0xffff, sizeof(depth));
    presentation_count = 0;
    fxmark_test_prof_counter = 0;
}

static void test_scene_catalog_and_batteries()
{
    using namespace fxglmark2;
    assert(scene_count() == 17);
    assert(implemented_scene_count() == 4);
    assert(std::strcmp(scene_name(SceneId::Build), "build") == 0);
    assert(std::strcmp(battery_name(BatteryId::QuickSmoke), "Quick smoke (2)") == 0);
    assert(battery_size(BatteryId::QuickSmoke) == 2);
    assert(battery_size(BatteryId::Glmark2SupportedDefault) == 5);
    assert(battery_size(BatteryId::AllImplemented) == 6);
    assert(default_benchmark_count() == 5);

    BenchmarkRequest request = {};
    char error[160] = {};
    assert(battery_request(BatteryId::Glmark2SupportedDefault, 0,
        request, error, sizeof(error)));
    assert(request.scene == SceneId::Build && !request.use_vbo);
    assert(battery_request(BatteryId::Glmark2SupportedDefault, 1,
        request, error, sizeof(error)));
    assert(request.scene == SceneId::Build && request.use_vbo);
    assert(battery_request(BatteryId::AllImplemented, 0,
        request, error, sizeof(error)));
    assert(request.scene == SceneId::Clear);
    assert(!battery_request(BatteryId::QuickSmoke, 10,
        request, error, sizeof(error)));
}

static void test_glmark_description_parser()
{
    using namespace fxglmark2;
    BenchmarkRequest request = {};
    char error[160] = {};

    assert(parse_benchmark("build:use-vbo=false", request,
        error, sizeof(error)));
    assert(request.scene == SceneId::Build && !request.use_vbo);

    assert(parse_benchmark("build:use-vbo=true", request,
        error, sizeof(error)));
    assert(request.scene == SceneId::Build && request.use_vbo);

    assert(parse_benchmark("texture:texture-filter=nearest", request,
        error, sizeof(error)));
    assert(request.scene == SceneId::Texture && request.texture_filter == 0);

    assert(parse_benchmark("texture:texture-filter=linear", request,
        error, sizeof(error)));
    assert(request.scene == SceneId::Texture && request.texture_filter == 1);

    assert(parse_benchmark("shading:shading=gouraud", request,
        error, sizeof(error)));
    assert(request.scene == SceneId::Shading);

    assert(parse_benchmark("clear", request, error, sizeof(error)));
    assert(request.scene == SceneId::Clear);

    assert(!parse_benchmark("bump:bump-render=normals", request,
        error, sizeof(error)));
    assert(std::strstr(error, "not implemented") != nullptr);
    assert(!parse_benchmark("texture:texture-filter=mipmap", request,
        error, sizeof(error)));
    assert(!parse_benchmark("build:use-vbo=maybe", request,
        error, sizeof(error)));
    assert(!parse_benchmark("unknown", request, error, sizeof(error)));
    assert(!parse_benchmark("build:use-vbo=true:use-vbo=false",
        request, error, sizeof(error)));
}

static void test_scene_preparation_and_render()
{
    using namespace fxglmark2;
    reset_surface();
    const fxgl_surface_t surface = {
        color, depth, nullptr, 396, 224, 396
    };
    assert(fxglMakeCurrent(&surface) == GL_TRUE);

    char error[160] = {};
    BenchmarkRequest request = {};
    SceneInstance scene = {};

    assert(parse_benchmark("build:use-vbo=false", request,
        error, sizeof(error)));
    assert(prepare_scene(request, scene, error, sizeof(error)));
    assert(scene.prepared);
    assert(scene.vertex_buffer == 0);
    assert(scene.triangle_count > 100);
    assert(render_scene_frame(scene, 0));
    finish_scene(scene);

    assert(parse_benchmark("build:use-vbo=true", request,
        error, sizeof(error)));
    assert(prepare_scene(request, scene, error, sizeof(error)));
    assert(scene.vertex_buffer != 0);
    assert(render_scene_frame(scene, 1));
    finish_scene(scene);

    assert(parse_benchmark("texture:texture-filter=nearest", request,
        error, sizeof(error)));
    assert(prepare_scene(request, scene, error, sizeof(error)));
    assert(scene.texture != 0);
    assert(render_scene_frame(scene, 2));
    finish_scene(scene);

    assert(parse_benchmark("texture:texture-filter=linear", request,
        error, sizeof(error)));
    assert(prepare_scene(request, scene, error, sizeof(error)));
    assert(render_scene_frame(scene, 3));
    finish_scene(scene);

    assert(parse_benchmark("shading:shading=gouraud", request,
        error, sizeof(error)));
    assert(prepare_scene(request, scene, error, sizeof(error)));
    assert(render_scene_frame(scene, 4));
    finish_scene(scene);

    assert(parse_benchmark("clear", request, error, sizeof(error)));
    assert(prepare_scene(request, scene, error, sizeof(error)));
    assert(render_scene_frame(scene, 5));
    finish_scene(scene);

    fxglReleaseCurrent();
}

static void test_benchmark_runner_and_timing()
{
    using namespace fxglmark2;
    reset_surface();
    const fxgl_surface_t surface = {
        color, depth, nullptr, 396, 224, 396
    };
    assert(fxglMakeCurrent(&surface) == GL_TRUE);

    BenchmarkRequest request = {};
    char error[160] = {};
    assert(parse_benchmark("clear", request, error, sizeof(error)));

    RunConfig config = {1, 3, 2};
    BenchmarkResult result = {};
    assert(run_benchmark(request, config, result, error, sizeof(error)));
    assert(result.frames == 6);
    assert(result.triangles_per_frame == 0);
    assert(result.mean_frame_us == 100.0);
    assert(result.median_frame_us == 100.0);
    assert(result.fps == 10000.0);
    assert(presentation_count == 7);

    config.repeat_count = 0;
    assert(!run_benchmark(request, config, result, error, sizeof(error)));
    fxglReleaseCurrent();
}
} // namespace

int main()
{
    test_scene_catalog_and_batteries();
    test_glmark_description_parser();
    test_scene_preparation_and_render();
    test_benchmark_runner_and_timing();
    std::puts("fx-CG50 glmark2 port tests: PASS");
    return 0;
}
