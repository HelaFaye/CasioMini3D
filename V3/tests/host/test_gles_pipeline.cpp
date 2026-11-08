#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cmath>

#include "../../src/gles/pipeline.hpp"

namespace
{
using fxgl::ClipVertex;
using fxgl::PipelineState;
using fxgl::RasterStats;
using fxgl::RenderSurface;

struct SolidColor
{
    float rgba[4];
};

bool solid_fragment(const fxgl::FragmentInput&, float rgba[4], void *user_data)
{
    const SolidColor& color = *static_cast<const SolidColor *>(user_data);
    for (int i = 0; i < 4; ++i)
        rgba[i] = color.rgba[i];
    return true;
}

bool red_from_varying(const fxgl::FragmentInput& input,
                      float rgba[4], void *)
{
    if (input.varying_count < 1)
        return false;
    rgba[0] = input.varyings[0];
    rgba[1] = 0.0f;
    rgba[2] = 0.0f;
    rgba[3] = 1.0f;
    return true;
}

ClipVertex vertex(float x, float y, float z, float w)
{
    ClipVertex result = {};
    result.position = {x, y, z, w};
    return result;
}

void full_screen_triangle(ClipVertex triangle[3], float z = 0.0f)
{
    triangle[0] = vertex(-1.0f, -1.0f, z, 1.0f);
    triangle[1] = vertex(1.0f, -1.0f, z, 1.0f);
    triangle[2] = vertex(-1.0f, 1.0f, z, 1.0f);
}

void reset_surface(uint16_t *color, uint16_t *depth, uint8_t *stencil)
{
    for (size_t i = 0; i < 16; ++i)
    {
        color[i] = 0x0000;
        depth[i] = 0xffff;
        stencil[i] = 0;
    }
}

void test_solid_triangle_coverage_and_top_left_fill()
{
    uint16_t color[16];
    uint16_t depth[16];
    uint8_t stencil[16];
    reset_surface(color, depth, stencil);

    const RenderSurface surface = {color, depth, stencil, 4, 4, 4};
    PipelineState state = {};
    fxgl::pipeline_default_state(state, 4, 4);
    SolidColor red = {{1.0f, 0.0f, 0.0f, 1.0f}};
    ClipVertex triangle[3];
    full_screen_triangle(triangle);

    RasterStats stats = {};
    assert(fxgl::draw_triangle(surface, state, triangle, 0,
        solid_fragment, &red, &stats));
    assert(stats.input_triangles == 1);
    assert(stats.clipped_polygons == 1);
    assert(stats.generated_triangles == 1);
    assert(stats.tested_fragments > 0);
    assert(stats.written_fragments > 0);
    assert(color[0] == 0xf800);
    assert(color[3 * 4 + 3] == 0x0000);
}

void test_back_face_culling_and_front_face_winding()
{
    uint16_t color[16];
    uint16_t depth[16];
    uint8_t stencil[16];
    reset_surface(color, depth, stencil);

    const RenderSurface surface = {color, depth, stencil, 4, 4, 4};
    PipelineState state = {};
    fxgl::pipeline_default_state(state, 4, 4);
    state.cull_enabled = true;
    state.cull_face = GL_BACK;

    SolidColor green = {{0.0f, 1.0f, 0.0f, 1.0f}};
    ClipVertex triangle[3];
    full_screen_triangle(triangle);
    std::swap(triangle[1], triangle[2]);

    RasterStats stats = {};
    assert(!fxgl::draw_triangle(surface, state, triangle, 0,
        solid_fragment, &green, &stats));
    assert(stats.tested_fragments == 0);
    for (uint16_t pixel : color)
        assert(pixel == 0x0000);

    state.front_face = GL_CW;
    assert(fxgl::draw_triangle(surface, state, triangle, 0,
        solid_fragment, &green, &stats));
    assert(color[0] == 0x07e0);
}

void test_homogeneous_clipping_renders_visible_portion()
{
    uint16_t color[16];
    uint16_t depth[16];
    uint8_t stencil[16];
    reset_surface(color, depth, stencil);

    const RenderSurface surface = {color, depth, stencil, 4, 4, 4};
    PipelineState state = {};
    fxgl::pipeline_default_state(state, 4, 4);
    SolidColor blue = {{0.0f, 0.0f, 1.0f, 1.0f}};

    ClipVertex triangle[3] = {
        vertex(-2.0f, 0.0f, 0.0f, 1.0f),
        vertex(0.5f, -0.75f, 0.0f, 1.0f),
        vertex(0.5f, 0.75f, 0.0f, 1.0f)
    };
    RasterStats stats = {};
    assert(fxgl::draw_triangle(surface, state, triangle, 0,
        solid_fragment, &blue, &stats));
    assert(stats.clipped_polygons == 1);
    assert(stats.generated_triangles >= 1);
    assert(stats.written_fragments > 0);
}

void test_depth_comparison_and_write_mask()
{
    uint16_t color[16];
    uint16_t depth[16];
    uint8_t stencil[16];
    reset_surface(color, depth, stencil);

    const RenderSurface surface = {color, depth, stencil, 4, 4, 4};
    PipelineState state = {};
    fxgl::pipeline_default_state(state, 4, 4);
    state.depth_enabled = true;

    ClipVertex triangle[3];
    full_screen_triangle(triangle);
    SolidColor red = {{1.0f, 0.0f, 0.0f, 1.0f}};
    SolidColor green = {{0.0f, 1.0f, 0.0f, 1.0f}};

    assert(fxgl::draw_triangle(surface, state, triangle, 0,
        solid_fragment, &red));
    assert(color[0] == 0xf800);
    assert(depth[0] == 32768);

    assert(!fxgl::draw_triangle(surface, state, triangle, 0,
        solid_fragment, &green));
    assert(color[0] == 0xf800);

    state.depth_function = GL_ALWAYS;
    state.depth_write = false;
    assert(fxgl::draw_triangle(surface, state, triangle, 0,
        solid_fragment, &green));
    assert(color[0] == 0x07e0);
    assert(depth[0] == 32768);
}

void test_alpha_blending_and_color_mask()
{
    uint16_t color[16];
    uint16_t depth[16];
    uint8_t stencil[16];
    reset_surface(color, depth, stencil);
    for (uint16_t& pixel : color)
        pixel = 0xf800;

    const RenderSurface surface = {color, depth, stencil, 4, 4, 4};
    PipelineState state = {};
    fxgl::pipeline_default_state(state, 4, 4);
    state.blend_enabled = true;
    state.blend_src_rgb = GL_SRC_ALPHA;
    state.blend_dst_rgb = GL_ONE_MINUS_SRC_ALPHA;
    state.blend_src_alpha = GL_ONE;
    state.blend_dst_alpha = GL_ZERO;

    SolidColor half_blue = {{0.0f, 0.0f, 1.0f, 0.5f}};
    ClipVertex triangle[3];
    full_screen_triangle(triangle);
    assert(fxgl::draw_triangle(surface, state, triangle, 0,
        solid_fragment, &half_blue));

    const uint16_t blended = color[0];
    const int red = (blended >> 11) & 0x1f;
    const int blue = blended & 0x1f;
    assert(red >= 14 && red <= 17);
    assert(blue >= 14 && blue <= 17);

    state.color_write[0] = false;
    state.color_write[1] = false;
    SolidColor green = {{0.0f, 1.0f, 0.0f, 1.0f}};
    assert(fxgl::draw_triangle(surface, state, triangle, 0,
        solid_fragment, &green));
    assert(((color[0] >> 11) & 0x1f) == red);
    assert((color[0] & 0x1f) == 0);
}

void test_scissor_and_stencil_operations()
{
    uint16_t color[16];
    uint16_t depth[16];
    uint8_t stencil[16];
    reset_surface(color, depth, stencil);

    const RenderSurface surface = {color, depth, stencil, 4, 4, 4};
    PipelineState state = {};
    fxgl::pipeline_default_state(state, 4, 4);
    state.scissor_enabled = true;
    state.scissor[0] = 1;
    state.scissor[1] = 1;
    state.scissor[2] = 1;
    state.scissor[3] = 1;
    state.stencil_enabled = true;
    state.stencil_front.function = GL_ALWAYS;
    state.stencil_front.reference = 7;
    state.stencil_front.depth_pass = GL_REPLACE;

    SolidColor white = {{1.0f, 1.0f, 1.0f, 1.0f}};
    ClipVertex triangle[3];
    full_screen_triangle(triangle);
    assert(fxgl::draw_triangle(surface, state, triangle, 0,
        solid_fragment, &white));
    assert(color[1 * 4 + 1] == 0xffff);
    assert(color[0] == 0x0000);
    assert(stencil[1 * 4 + 1] == 7);
    assert(stencil[0] == 0);

    state.stencil_front.function = GL_EQUAL;
    state.stencil_front.reference = 6;
    state.stencil_front.stencil_fail = GL_INCR;
    SolidColor red = {{1.0f, 0.0f, 0.0f, 1.0f}};
    assert(!fxgl::draw_triangle(surface, state, triangle, 0,
        solid_fragment, &red));
    assert(color[1 * 4 + 1] == 0xffff);
    assert(stencil[1 * 4 + 1] == 8);
}

void test_point_rasterization_and_clip_rejection()
{
    uint16_t color[16];
    uint16_t depth[16];
    uint8_t stencil[16];
    reset_surface(color, depth, stencil);

    const RenderSurface surface = {color, depth, stencil, 4, 4, 4};
    PipelineState state = {};
    fxgl::pipeline_default_state(state, 4, 4);
    SolidColor yellow = {{1.0f, 1.0f, 0.0f, 1.0f}};

    const ClipVertex center = vertex(0.0f, 0.0f, 0.0f, 1.0f);
    RasterStats stats = {};
    assert(fxgl::draw_point(surface, state, center, 0,
        solid_fragment, &yellow, &stats));
    assert(stats.input_points == 1);
    assert(stats.tested_fragments == 1);
    assert(stats.written_fragments == 1);
    assert(color[2 * 4 + 2] == 0xffe0);

    const ClipVertex outside = vertex(2.0f, 0.0f, 0.0f, 1.0f);
    assert(!fxgl::draw_point(surface, state, outside, 0,
        solid_fragment, &yellow, &stats));
    assert(stats.input_points == 1);
    assert(stats.written_fragments == 0);
}

void test_line_clipping_and_scissor_fragment_bounds()
{
    uint16_t color[16];
    uint16_t depth[16];
    uint8_t stencil[16];
    reset_surface(color, depth, stencil);

    const RenderSurface surface = {color, depth, stencil, 4, 4, 4};
    PipelineState state = {};
    fxgl::pipeline_default_state(state, 4, 4);
    state.scissor_enabled = true;
    state.scissor[0] = 1;
    state.scissor[1] = 2;
    state.scissor[2] = 2;
    state.scissor[3] = 1;
    SolidColor cyan = {{0.0f, 1.0f, 1.0f, 1.0f}};

    const ClipVertex line[2] = {
        vertex(-2.0f, 0.0f, 0.0f, 1.0f),
        vertex(0.5f, 0.0f, 0.0f, 1.0f)
    };
    RasterStats stats = {};
    assert(fxgl::draw_line(surface, state, line, 0,
        solid_fragment, &cyan, &stats));
    assert(stats.input_lines == 1);
    assert(stats.written_fragments == 2);
    assert(color[2 * 4 + 1] == 0x07ff);
    assert(color[2 * 4 + 2] == 0x07ff);
    assert(color[2 * 4 + 0] == 0x0000);
    assert(color[2 * 4 + 3] == 0x0000);
}

struct GradientCapture
{
    size_t calls;
    float dudx;
    float dvdx;
    float dudy;
    float dvdy;
};

bool capture_gradients(const fxgl::FragmentInput& input,
                       float rgba[4], void *user_data)
{
    GradientCapture& capture = *static_cast<GradientCapture *>(user_data);
    ++capture.calls;
    if (input.varying_count < 2)
        return false;
    capture.dudx = input.varying_ddx[0];
    capture.dvdx = input.varying_ddx[1];
    capture.dudy = input.varying_ddy[0];
    capture.dvdy = input.varying_ddy[1];
    rgba[0] = rgba[1] = rgba[2] = 1.0f;
    rgba[3] = 1.0f;
    return true;
}

void test_perspective_correct_screen_space_varying_gradients()
{
    uint16_t color[16];
    uint16_t depth[16];
    uint8_t stencil[16];
    reset_surface(color, depth, stencil);

    const RenderSurface surface = {color, depth, stencil, 4, 4, 4};
    PipelineState state = {};
    fxgl::pipeline_default_state(state, 4, 4);

    ClipVertex triangle[3];
    full_screen_triangle(triangle);
    triangle[0].varying[0] = 0.0f;
    triangle[0].varying[1] = 0.0f;
    triangle[1].varying[0] = 1.0f;
    triangle[1].varying[1] = 0.0f;
    triangle[2].varying[0] = 0.0f;
    triangle[2].varying[1] = 1.0f;

    // Default renderer state does not pay for derivatives when a fragment
    // program has no implicit texture sampling requirement.
    GradientCapture capture = {};
    assert(fxgl::draw_triangle(surface, state, triangle, 2,
        capture_gradients, &capture));
    assert(capture.calls > 0);
    assert(capture.dudx == 0.0f && capture.dvdx == 0.0f);
    assert(capture.dudy == 0.0f && capture.dvdy == 0.0f);

    // Derivatives remain available to shader paths which explicitly request
    // them (e.g. mipmapped/LOD texture sampling).
    state.varying_derivatives = true;
    capture = {};
    assert(fxgl::draw_triangle(surface, state, triangle, 2,
        capture_gradients, &capture));
    assert(capture.calls > 0);
    // The 4-pixel viewport maps each normalized coordinate axis over four
    // screen pixels, so adjacent-pixel derivatives are 1/4.
    assert(std::fabs(capture.dudx - 0.25f) < 0.001f);
    assert(std::fabs(capture.dvdx) < 0.001f);
    assert(std::fabs(capture.dudy) < 0.001f);
    assert(std::fabs(capture.dvdy - 0.25f) < 0.001f);
}

void test_perspective_correct_varying_interpolation()
{
    uint16_t color[16];
    uint16_t depth[16];
    uint8_t stencil[16];
    reset_surface(color, depth, stencil);

    const RenderSurface surface = {color, depth, stencil, 4, 4, 4};
    PipelineState state = {};
    fxgl::pipeline_default_state(state, 4, 4);

    ClipVertex triangle[3] = {
        vertex(-1.0f, -1.0f, 0.0f, 1.0f),
        vertex(2.0f, -2.0f, 0.0f, 2.0f),
        vertex(0.0f, 2.0f, 0.0f, 2.0f)
    };
    triangle[0].varying[0] = 0.0f;
    triangle[1].varying[0] = 1.0f;
    triangle[2].varying[0] = 0.0f;

    assert(fxgl::draw_triangle(surface, state, triangle, 1,
        red_from_varying, nullptr));
    const int red = (color[0 * 4 + 2] >> 11) & 0x1f;
    // At pixel center (2.5, 0.5), perspective correction yields 3/7,
    // while affine interpolation would yield 9/16.
    assert(red >= 12 && red <= 14);
}

struct PerspectiveCapture
{
    size_t calls;
    float value[2];
    float ddx[2];
    float ddy[2];
};

bool capture_perspective_sample(const fxgl::FragmentInput& input,
                                float rgba[4], void *user_data)
{
    PerspectiveCapture& capture = *static_cast<PerspectiveCapture *>(user_data);
    ++capture.calls;
    if (capture.calls == 3 && input.varying_count >= 2)
    {
        for (size_t i = 0; i < 2; ++i)
        {
            capture.value[i] = input.varyings[i];
            capture.ddx[i] = input.varying_ddx[i];
            capture.ddy[i] = input.varying_ddy[i];
        }
    }
    rgba[0] = rgba[1] = rgba[2] = rgba[3] = 1.0f;
    return true;
}

void test_perspective_planes_preserve_varyings_and_finite_difference_gradients()
{
    uint16_t color[16];
    uint16_t depth[16];
    uint8_t stencil[16];
    reset_surface(color, depth, stencil);

    const RenderSurface surface = {color, depth, stencil, 4, 4, 4};
    PipelineState state = {};
    fxgl::pipeline_default_state(state, 4, 4);
    state.varying_derivatives = true;

    ClipVertex triangle[3] = {
        vertex(-1.0f, -1.0f, 0.0f, 1.0f),
        vertex(2.0f, -2.0f, 0.0f, 2.0f),
        vertex(0.0f, 2.0f, 0.0f, 2.0f)
    };
    triangle[0].varying[0] = 0.0f;
    triangle[0].varying[1] = 0.0f;
    triangle[1].varying[0] = 1.0f;
    triangle[1].varying[1] = 0.0f;
    triangle[2].varying[0] = 0.0f;
    triangle[2].varying[1] = 1.0f;

    PerspectiveCapture capture = {};
    assert(fxgl::draw_triangle(surface, state, triangle, 2,
        capture_perspective_sample, &capture));
    assert(capture.calls >= 3);

    // The third covered sample on the first scanline is pixel (2, 0).
    // Its screen-space barycentrics are (5/16, 9/16, 2/16). The neighboring
    // pixel values below use a double-precision reference rational plane.
    assert(std::fabs(capture.value[0] - (3.0f / 7.0f)) < 0.0005f);
    assert(std::fabs(capture.value[1] - (2.0f / 21.0f)) < 0.0005f);
    assert(std::fabs(capture.ddx[0] - (40.0f / 119.0f)) < 0.0005f);
    assert(std::fabs(capture.ddx[1] - (8.0f / 357.0f)) < 0.0005f);
    assert(std::fabs(capture.ddy[0] - (-26.0f / 175.0f)) < 0.0005f);
    assert(std::fabs(capture.ddy[1] - (76.0f / 525.0f)) < 0.0005f);
}

} // namespace

int main()
{
    test_solid_triangle_coverage_and_top_left_fill();
    test_back_face_culling_and_front_face_winding();
    test_homogeneous_clipping_renders_visible_portion();
    test_depth_comparison_and_write_mask();
    test_alpha_blending_and_color_mask();
    test_scissor_and_stencil_operations();
    test_point_rasterization_and_clip_rejection();
    test_line_clipping_and_scissor_fragment_bounds();
    test_perspective_correct_varying_interpolation();
    test_perspective_correct_screen_space_varying_gradients();
    test_perspective_planes_preserve_varyings_and_finite_difference_gradients();

    std::puts("GLES software pipeline tests: PASS");
    return 0;
}
