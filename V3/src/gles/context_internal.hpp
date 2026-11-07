#pragma once

#include <stddef.h>

#include "../gfx/texture.hpp"
#include "pipeline.hpp"

namespace fxgl
{

/* Internal renderer bridge: obtain a non-owning view of a bound texture unit.
   The texture storage remains owned by the GLES context and is invalidated by
   reallocation, deletion, context rebinding or context release. */
bool get_texture_binding(
    size_t unit,
    gfx::Texture2DView& view,
    gfx::TextureSampler& sampler);


/* Executes one clip-space triangle through the active context's GLES render
   state and bound surface. Used by the shader/draw-call backend. */
bool draw_clip_triangle(
    const ClipVertex input[3],
    size_t varying_count,
    FragmentShader fragment_shader,
    void *user_data,
    RasterStats *stats = nullptr);

} // namespace fxgl
