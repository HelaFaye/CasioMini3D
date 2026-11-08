#pragma once

#include <stddef.h>
#include <stdint.h>

#include <GLES2/gl2.h>
#include "math.hpp"

namespace fxgl
{

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

    bool cull_enabled;
    GLenum cull_face;
    GLenum front_face;

    bool depth_enabled;
    GLenum depth_function;
    bool depth_write;
    float depth_near;
    float depth_far;

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
