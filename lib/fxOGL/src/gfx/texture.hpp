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
/* Convert a finite normalized coordinate to 16.16 texel space:
   floor(coordinate * size * 65536), minus half a texel (32768) when
   half_texel_offset is set for linear filtering. Exactly equal to the
   double-precision formulation it replaces, using integer arithmetic only
   (the fx-CG50 has no FPU). Returns false for non-finite input or when the
   result is outside [-2^63, 2^63). */
bool texture_coordinate_16_16(
    float coordinate,
    uint16_t size,
    bool half_texel_offset,
    int64_t& result);

/* Fast path for a texture/sampler pair already validated and kept unchanged
   for the duration of a draw call. The caller must satisfy both preconditions. */
uint16_t sample_texture_validated(
    const Texture2DView& texture,
    int64_t u_16_16,
    int64_t v_16_16,
    const TextureSampler& sampler,
    float lod = 0.0f);

/* Precomputed state for a tightly packed, power-of-two Repeat/Repeat texture.
   The pixel storage must outlive the prepared sampler. */
struct TextureNearestRepeatPow2Sampler
{
    const uint16_t *pixels;
    float scale_u;
    float scale_v;
    uint16_t mask_u;
    uint16_t mask_v;
    uint8_t row_shift;    /* log2(width) */
    uint8_t height_shift; /* log2(height) */
};

/* Prepare once per draw; returns false unless the texture/sampler is exactly
   nearest/nearest, Repeat/Repeat, power-of-two, and tightly packed. */
bool prepare_texture_nearest_repeat_pow2_sampler(
    const Texture2DView& texture,
    const TextureSampler& sampler,
    TextureNearestRepeatPow2Sampler& prepared);

/* Sample using state prepared once per draw. Returns false outside [-64, 64)
   or for non-finite coordinates. */
bool sample_texture_nearest_repeat_pow2_cached(
    const TextureNearestRepeatPow2Sampler& sampler,
    float u,
    float v,
    uint16_t& pixel);

/* Direct two-axis Repeat sampler for validated nearest textures whose
   nonzero width and height are powers of two. Returns false outside [-64, 64)
   so the caller can use the general full-range sampler. */
bool sample_texture_nearest_repeat_pow2_validated(
    const Texture2DView& texture,
    float u,
    float v,
    uint16_t& pixel);

/* Direct normalized-coordinate path for validated nearest/nearest sampling.
   Returns false for non-finite or out-of-range coordinates. The texture view
   and sampler must already be valid and stable for the call. */
bool sample_texture_nearest_validated(
    const Texture2DView& texture,
    float u,
    float v,
    const TextureSampler& sampler,
    uint16_t& pixel);

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
