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

enum class TextureMinFilter : uint8_t
{
    Nearest = 0,
    Linear = 1,
    NearestMipmapNearest = 2,
    LinearMipmapNearest = 3,
    NearestMipmapLinear = 4,
    LinearMipmapLinear = 5
};

struct TextureMipLevel
{
    const uint16_t *pixels;
    uint16_t width;
    uint16_t height;
    uint16_t stride;
};

struct Texture2DView
{
    const uint16_t* pixels;
    uint16_t width;
    uint16_t height;
    uint16_t stride; /* Pixels between the start of adjacent rows. */
    const TextureMipLevel *mip_levels = nullptr; /* Levels 1..N-1. */
    uint8_t mip_level_count = 0;
};

struct TextureSampler
{
    TextureFilter filter;
    TextureAddressMode address_u;
    TextureAddressMode address_v;
    TextureMinFilter min_filter = TextureMinFilter::Nearest;
};

/* Validate a non-owning RGB565 texture view and sampler configuration. */
bool texture_valid(const Texture2DView& texture);
bool sampler_valid(const TextureSampler& sampler);
/* True when implicit LOD can change the selected minification/magnification filter. */
bool sampler_requires_implicit_derivatives(const TextureSampler& sampler);
/* Fast path for a texture/sampler pair already validated and kept unchanged
   for the duration of a draw call. The caller must satisfy both preconditions. */
uint16_t sample_texture_validated(
    const Texture2DView& texture,
    int64_t u_16_16,
    int64_t v_16_16,
    const TextureSampler& sampler,
    float lod = 0.0f);

/* Sample an RGB565 texture using signed 16.16 texel-space coordinates.
   Nearest uses floor(u), floor(v). Bilinear interpolates the four surrounding
   texels. Clamp and Repeat addressing are applied independently per axis.
   Invalid texture or sampler input returns black (0). */
uint16_t sample_texture(
    const Texture2DView& texture,
    int64_t u_16_16,
    int64_t v_16_16,
    const TextureSampler& sampler,
    float lod = 0.0f);

} // namespace gfx
