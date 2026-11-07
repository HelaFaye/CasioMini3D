#pragma once

#include <stdint.h>

namespace gfx
{

/* Texture coordinates are signed 16.16 texel-space values: integer (u,v)
   addresses the texel at that coordinate, and the fractional part controls
   bilinear interpolation. The view does not own its pixel storage. */
enum class TextureFilter : uint8_t
{
    Nearest = 0,
    Bilinear = 1
};

enum class TextureAddressMode : uint8_t
{
    Clamp = 0,
    Repeat = 1,
    MirroredRepeat = 2
};

struct Texture2DView
{
    const uint16_t* pixels;
    uint16_t width;
    uint16_t height;
    uint16_t stride; /* Pixels between the start of adjacent rows. */
};

struct TextureSampler
{
    TextureFilter filter;
    TextureAddressMode address_u;
    TextureAddressMode address_v;
};

/* Validate a non-owning RGB565 texture view and sampler configuration. */
bool texture_valid(const Texture2DView& texture);
bool sampler_valid(const TextureSampler& sampler);

/* Sample an RGB565 texture using signed 16.16 texel-space coordinates.
   Nearest uses floor(u), floor(v). Bilinear interpolates the four surrounding
   texels. Clamp and Repeat addressing are applied independently per axis.
   Invalid texture or sampler input returns black (0). */
uint16_t sample_texture(
    const Texture2DView& texture,
    int64_t u_16_16,
    int64_t v_16_16,
    const TextureSampler& sampler);

} // namespace gfx
