#pragma once

#include <stddef.h>

#include "../gfx/texture.hpp"

namespace fxgl
{

/* Internal renderer bridge: obtain a non-owning view of a bound texture unit.
   The texture storage remains owned by the GLES context and is invalidated by
   reallocation, deletion, context rebinding or context release. */
bool get_texture_binding(
    size_t unit,
    gfx::Texture2DView& view,
    gfx::TextureSampler& sampler);

} // namespace fxgl
