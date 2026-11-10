#pragma once

#include <stddef.h>
#include <stdint.h>

#include <GLES2/gl2.h>
#include "math.hpp"

namespace fxgl
{

/* Bounded software point rasterization range, also exposed by glGetFloatv. */
constexpr float MAX_POINT_SIZE = 16.0f;

struct RenderSurface
{
    uint16_t *color;
    uint16_t *depth;
    uint8_t *stencil;
    GLsizei width;
    GLsizei height;
    GLsizei stride;
};

struct StencilFaceState
{
    GLenum function;
    GLint reference;
    GLuint value_mask;
    GLuint write_mask;
    GLenum stencil_fail;
    GLenum depth_fail;
    GLenum depth_pass;
};

struct PipelineState
{
    GLint viewport[4];
    GLint scissor[4];
    bool scissor_enabled;
    /* Compute perspective-correct varying gradients only when a shader path
       consumes implicit texture derivatives (e.g. mipmapped sampling). */
    bool varying_derivatives;
    /* Test depth before running the fragment shader when no stencil test is
       active. Buffer results are unchanged; the shader is simply not run
       for fragments that would fail the depth test, so a shader's side
       effects (including failure reporting) are skipped for them. Off by
       default; the GLES layer enables it because its shaders cannot write
       depth and their only side effect is failure reporting. */
    bool early_depth_test;

    bool cull_enabled;
    GLenum cull_face;
    GLenum front_face;

    bool depth_enabled;
    GLenum depth_function;
    bool depth_write;
    float depth_near;
    float depth_far;
    /* glPolygonOffset for filled triangles: window depth is offset by
       factor * max depth slope + units * (1 / 65535) before clamping and
       the depth test. Points and lines are not offset. */
    bool polygon_offset_fill;
    float polygon_offset_factor;
    float polygon_offset_units;

    bool stencil_enabled;
    StencilFaceState stencil_front;
    StencilFaceState stencil_back;

    bool blend_enabled;
    GLenum blend_src_rgb;
    GLenum blend_dst_rgb;
    GLenum blend_src_alpha;
    GLenum blend_dst_alpha;
    GLenum blend_equation_rgb;
    GLenum blend_equation_alpha;
    float blend_color[4];

    bool color_write[4];
};

struct RasterStats
{
    size_t input_triangles;
    size_t input_points;
    size_t input_lines;
    size_t clipped_polygons;
    size_t generated_triangles;
    size_t generated_lines;
    size_t tested_fragments;
    size_t shaded_fragments;
    size_t depth_passed_fragments;
    size_t written_fragments;
};

struct FragmentInput
{
    const float *varyings;
    const float *varying_ddx;
    const float *varying_ddy;
    size_t varying_count;
};

using FragmentShader = bool (*)(
    const FragmentInput& input,
    float rgba[4],
    void *user_data);

template <unsigned Levels>
struct UnitFloatTable
{
    float values[Levels + 1];
};

template <unsigned Levels>
constexpr UnitFloatTable<Levels> make_unit_float_table()
{
    UnitFloatTable<Levels> table = {};
    for (unsigned i = 0; i <= Levels; ++i)
        table.values[i] = static_cast<float>(i) / static_cast<float>(Levels);
    return table;
}

/* static_cast<float>(i) / 31.0f and / 63.0f, folded at compile time with the
   same correctly rounded IEEE division the device performs in software. */
inline constexpr UnitFloatTable<31> UNIT_FROM_FIVE_BIT = make_unit_float_table<31>();
inline constexpr UnitFloatTable<63> UNIT_FROM_SIX_BIT = make_unit_float_table<63>();

/* Expand an RGB565 pixel to normalized RGBA with alpha 1.0. */
inline void rgb565_to_unit_float(uint16_t pixel, float rgba[4])
{
    rgba[0] = UNIT_FROM_FIVE_BIT.values[(pixel >> 11) & 0x1f];
    rgba[1] = UNIT_FROM_SIX_BIT.values[(pixel >> 5) & 0x3f];
    rgba[2] = UNIT_FROM_FIVE_BIT.values[pixel & 0x1f];
    rgba[3] = 1.0f;
}

void pipeline_default_state(PipelineState& state, GLsizei width, GLsizei height);

/* Render one GLES homogeneous clip-space triangle into a caller-owned surface.
   Input varyings are linear shader outputs; interpolation after projection is
   perspective-correct. The fragment callback returns false to discard a
   fragment. No Gint symbols or dynamic allocations are used. */
bool draw_triangle(
    const RenderSurface& surface,
    const PipelineState& state,
    const ClipVertex input[3],
    size_t varying_count,
    FragmentShader fragment_shader,
    void *user_data,
    RasterStats *stats = nullptr);


/* Render one homogeneous point or line segment through the same clip,
   perspective interpolation, fragment, depth, stencil and blend stages.
   Point coverage is a single pixel; line coverage uses a one-pixel DDA rule. */
bool draw_point(
    const RenderSurface& surface,
    const PipelineState& state,
    const ClipVertex& input,
    size_t varying_count,
    FragmentShader fragment_shader,
    void *user_data,
    RasterStats *stats = nullptr);

bool draw_line(
    const RenderSurface& surface,
    const PipelineState& state,
    const ClipVertex input[2],
    size_t varying_count,
    FragmentShader fragment_shader,
    void *user_data,
    RasterStats *stats = nullptr);

} // namespace fxgl
