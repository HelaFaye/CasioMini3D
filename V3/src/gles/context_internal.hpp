#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../gfx/texture.hpp"
#include "glsl.hpp"
#include "pipeline.hpp"

namespace fxgl
{

bool get_texture_binding(
    size_t unit,
    gfx::Texture2DView& view,
    gfx::TextureSampler& sampler);

bool draw_clip_triangle(
    const ClipVertex input[3],
    size_t varying_count,
    FragmentShader fragment_shader,
    void *user_data,
    RasterStats *stats = nullptr);

/* The GLES-style API is intentionally single-context on the calculator. */
void program_context_reset();
void report_error(unsigned int error);
unsigned int current_program_name();

struct LinkedProgramView
{
    const ShaderModule *vertex_shader;
    const ShaderModule *fragment_shader;
    const ShaderValue *uniform_values;
    size_t uniform_count;
    uint8_t vertex_uniform_map[GLSL_MAX_UNIFORMS];
    uint8_t fragment_uniform_map[GLSL_MAX_UNIFORMS];
    uint8_t attribute_locations[GLSL_MAX_ATTRIBUTES];
    uint8_t vertex_varying_offsets[GLSL_MAX_VARYINGS];
    uint8_t fragment_varying_offsets[GLSL_MAX_VARYINGS];
    size_t varying_scalar_count;
};

bool get_current_program_view(LinkedProgramView& view);

} // namespace fxgl
