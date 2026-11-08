#include "texture.hpp"

#include <stddef.h>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace gfx
{
namespace
{
constexpr int64_t FIXED_ONE_16 = 65536;

struct FixedCoordinate
{
    int64_t whole;
    uint32_t fraction;
};

/* C++ signed division truncates toward zero. Convert it to mathematical floor
   so negative texture coordinates wrap and interpolate consistently. */
static FixedCoordinate split_coordinate(int64_t value)
{
    int64_t whole = value / FIXED_ONE_16;
    int64_t remainder = value % FIXED_ONE_16;

    if (remainder < 0)
    {
        --whole;
        remainder += FIXED_ONE_16;
    }

    return {whole, static_cast<uint32_t>(remainder)};
}

static uint16_t address_index(
    int64_t index,
    uint16_t size,
    TextureAddressMode mode)
{
    if (mode == TextureAddressMode::Clamp)
    {
        if (index < 0)
            return 0;
        if (index >= static_cast<int64_t>(size))
            return static_cast<uint16_t>(size - 1);
        return static_cast<uint16_t>(index);
    }

    const int64_t extent = static_cast<int64_t>(size);
    if (mode == TextureAddressMode::Repeat)
    {
        int64_t wrapped = index % extent;
        if (wrapped < 0)
            wrapped += extent;
        return static_cast<uint16_t>(wrapped);
    }

    const int64_t period = extent * 2;
    int64_t wrapped = index % period;
    if (wrapped < 0)
        wrapped += period;
    if (wrapped >= extent)
        wrapped = period - wrapped - 1;
    return static_cast<uint16_t>(wrapped);
}

static uint16_t texel(
    const Texture2DView& texture,
    int64_t x,
    int64_t y,
    const TextureSampler& sampler)
{
    const uint16_t addressed_x =
        address_index(x, texture.width, sampler.address_u);
    const uint16_t addressed_y =
        address_index(y, texture.height, sampler.address_v);
    const size_t offset =
        static_cast<size_t>(addressed_y) * texture.stride + addressed_x;
    return texture.pixels[offset];
}

static uint32_t interpolate_channel(uint32_t a, uint32_t b, uint32_t t)
{
    const uint32_t inverse = 65536u - t;
    return (a * inverse + b * t + 32768u) >> 16;
}

static uint16_t interpolate_rgb565(uint16_t a, uint16_t b, uint32_t t)
{
    const uint32_t red = interpolate_channel((a >> 11) & 0x1fu,
                                              (b >> 11) & 0x1fu, t);
    const uint32_t green = interpolate_channel((a >> 5) & 0x3fu,
                                                (b >> 5) & 0x3fu, t);
    const uint32_t blue = interpolate_channel(a & 0x1fu,
                                               b & 0x1fu, t);

    return static_cast<uint16_t>((red << 11) | (green << 5) | blue);
}
} // namespace

bool texture_valid(const Texture2DView& texture)
{
    if (!texture.pixels || texture.width == 0 || texture.height == 0 ||
        texture.stride < texture.width ||
        texture.mip_level_count > 7 ||
        (texture.mip_level_count != 0 && !texture.mip_levels))
        return false;

    uint16_t expected_width = texture.width;
    uint16_t expected_height = texture.height;
    for (size_t i = 0; i < texture.mip_level_count; ++i)
    {
        expected_width = expected_width > 1
            ? static_cast<uint16_t>(expected_width / 2) : 1;
        expected_height = expected_height > 1
            ? static_cast<uint16_t>(expected_height / 2) : 1;
        const TextureMipLevel& level = texture.mip_levels[i];
        if (!level.pixels || level.width != expected_width ||
            level.height != expected_height || level.stride < level.width)
            return false;
    }
    return true;
}

bool sampler_valid(const TextureSampler& sampler)
{
    const bool filter_ok =
        sampler.filter == TextureFilter::Nearest ||
        sampler.filter == TextureFilter::Bilinear;
    const bool address_u_ok =
        sampler.address_u == TextureAddressMode::Clamp ||
        sampler.address_u == TextureAddressMode::Repeat ||
        sampler.address_u == TextureAddressMode::MirroredRepeat;
    const bool address_v_ok =
        sampler.address_v == TextureAddressMode::Clamp ||
        sampler.address_v == TextureAddressMode::Repeat ||
        sampler.address_v == TextureAddressMode::MirroredRepeat;
    const bool min_filter_ok =
        sampler.min_filter == TextureMinFilter::Nearest ||
        sampler.min_filter == TextureMinFilter::Linear ||
        sampler.min_filter == TextureMinFilter::NearestMipmapNearest ||
        sampler.min_filter == TextureMinFilter::LinearMipmapNearest ||
        sampler.min_filter == TextureMinFilter::NearestMipmapLinear ||
        sampler.min_filter == TextureMinFilter::LinearMipmapLinear;
    return filter_ok && address_u_ok && address_v_ok && min_filter_ok;
}

bool sampler_requires_implicit_derivatives(const TextureSampler& sampler)
{
    if (!sampler_valid(sampler))
        return true;

    // With identical non-mipmapped minification and magnification filters,
    // implicit LOD cannot affect the selected texel or filtering operation.
    return !(
        (sampler.min_filter == TextureMinFilter::Nearest &&
         sampler.filter == TextureFilter::Nearest) ||
        (sampler.min_filter == TextureMinFilter::Linear &&
         sampler.filter == TextureFilter::Bilinear));
}

static Texture2DView level_view(
    const Texture2DView& texture,
    size_t level)
{
    if (level == 0)
        return {texture.pixels, texture.width, texture.height,
                texture.stride, nullptr, 0};
    const TextureMipLevel& mip = texture.mip_levels[level - 1];
    return {mip.pixels, mip.width, mip.height, mip.stride, nullptr, 0};
}

static int64_t scale_coordinate(
    int64_t coordinate,
    uint16_t base_size,
    uint16_t level_size)
{
    const int64_t divisor = base_size;
    const int64_t quotient = coordinate / divisor;
    const int64_t remainder = coordinate % divisor;
    if (quotient > 0 &&
        quotient > INT64_MAX / static_cast<int64_t>(level_size))
        return INT64_MAX;
    if (quotient < 0 &&
        quotient < INT64_MIN / static_cast<int64_t>(level_size))
        return INT64_MIN;
    return quotient * level_size +
        remainder * static_cast<int64_t>(level_size) / divisor;
}

static uint16_t sample_level(
    const Texture2DView& level,
    int64_t u,
    int64_t v,
    TextureFilter filter,
    TextureAddressMode address_u,
    TextureAddressMode address_v)
{
    TextureSampler sampler = {filter, address_u, address_v};
    if (filter == TextureFilter::Nearest)
    {
        const FixedCoordinate fixed_u = split_coordinate(u);
        const FixedCoordinate fixed_v = split_coordinate(v);
        return texel(level, fixed_u.whole, fixed_v.whole, sampler);
    }

    const FixedCoordinate fixed_u = split_coordinate(u);
    const FixedCoordinate fixed_v = split_coordinate(v);
    const uint16_t top_left = texel(level, fixed_u.whole, fixed_v.whole, sampler);
    const uint16_t top_right = texel(level, fixed_u.whole + 1, fixed_v.whole, sampler);
    const uint16_t bottom_left = texel(level, fixed_u.whole, fixed_v.whole + 1, sampler);
    const uint16_t bottom_right = texel(
        level, fixed_u.whole + 1, fixed_v.whole + 1, sampler);
    const uint16_t top = interpolate_rgb565(
        top_left, top_right, fixed_u.fraction);
    const uint16_t bottom = interpolate_rgb565(
        bottom_left, bottom_right, fixed_u.fraction);
    return interpolate_rgb565(top, bottom, fixed_v.fraction);
}

static uint16_t sample_texture_impl(
    const Texture2DView& texture,
    int64_t u_16_16,
    int64_t v_16_16,
    const TextureSampler& sampler,
    float lod)
{
    if (!std::isfinite(lod))
        return 0;

    if (lod <= 0.0f)
        return sample_level(level_view(texture, 0), u_16_16, v_16_16,
            sampler.filter, sampler.address_u, sampler.address_v);

    const bool linear_texel =
        sampler.min_filter == TextureMinFilter::Linear ||
        sampler.min_filter == TextureMinFilter::LinearMipmapNearest ||
        sampler.min_filter == TextureMinFilter::LinearMipmapLinear;
    const bool mip_linear =
        sampler.min_filter == TextureMinFilter::NearestMipmapLinear ||
        sampler.min_filter == TextureMinFilter::LinearMipmapLinear;
    const bool uses_mipmaps =
        sampler.min_filter == TextureMinFilter::NearestMipmapNearest ||
        sampler.min_filter == TextureMinFilter::LinearMipmapNearest ||
        sampler.min_filter == TextureMinFilter::NearestMipmapLinear ||
        sampler.min_filter == TextureMinFilter::LinearMipmapLinear;

    if (!uses_mipmaps)
        return sample_level(level_view(texture, 0), u_16_16, v_16_16,
            linear_texel ? TextureFilter::Bilinear : TextureFilter::Nearest,
            sampler.address_u, sampler.address_v);

    size_t required_levels = 0;
    uint16_t required_width = texture.width;
    uint16_t required_height = texture.height;
    while (required_width > 1 || required_height > 1)
    {
        required_width = required_width > 1
            ? static_cast<uint16_t>(required_width / 2) : 1;
        required_height = required_height > 1
            ? static_cast<uint16_t>(required_height / 2) : 1;
        ++required_levels;
    }
    if (texture.mip_level_count != required_levels)
        return 0; /* GLES texture is incomplete for its mipmapped min filter. */

    const float maximum_lod = static_cast<float>(required_levels);
    const float clamped_lod = std::max(0.0f, std::min(lod, maximum_lod));
    const size_t lower_level = static_cast<size_t>(std::floor(clamped_lod));
    const bool mip_nearest =
        sampler.min_filter == TextureMinFilter::NearestMipmapNearest ||
        sampler.min_filter == TextureMinFilter::LinearMipmapNearest;
    const size_t selected_level = std::min(
        mip_nearest
            ? static_cast<size_t>(std::floor(clamped_lod + 0.5f))
            : lower_level,
        required_levels);
    const TextureFilter filter = linear_texel
        ? TextureFilter::Bilinear : TextureFilter::Nearest;

    const Texture2DView lower = level_view(texture, selected_level);
    const int64_t lower_u = scale_coordinate(
        u_16_16, texture.width, lower.width);
    const int64_t lower_v = scale_coordinate(
        v_16_16, texture.height, lower.height);
    const uint16_t first = sample_level(lower, lower_u, lower_v, filter,
        sampler.address_u, sampler.address_v);

    if (!mip_linear || selected_level >= texture.mip_level_count)
        return first;

    const size_t upper_level = selected_level + 1;
    const Texture2DView upper = level_view(texture, upper_level);
    const int64_t upper_u = scale_coordinate(
        u_16_16, texture.width, upper.width);
    const int64_t upper_v = scale_coordinate(
        v_16_16, texture.height, upper.height);
    const uint16_t second = sample_level(upper, upper_u, upper_v, filter,
        sampler.address_u, sampler.address_v);
    const uint32_t fraction = static_cast<uint32_t>(
        (clamped_lod - static_cast<float>(selected_level)) * 65536.0f + 0.5f);
    return interpolate_rgb565(first, second, std::min<uint32_t>(fraction, 65536u));
}

uint16_t sample_texture_validated(
    const Texture2DView& texture,
    int64_t u_16_16,
    int64_t v_16_16,
    const TextureSampler& sampler,
    float lod)
{
    return sample_texture_impl(texture, u_16_16, v_16_16, sampler, lod);
}

bool sample_texture_nearest_validated(
    const Texture2DView& texture,
    float u,
    float v,
    const TextureSampler& sampler,
    uint16_t& pixel)
{
    if (sampler.filter != TextureFilter::Nearest ||
        sampler.min_filter != TextureMinFilter::Nearest ||
        !std::isfinite(u) || !std::isfinite(v))
        return false;

    // Ordinary in-range fractional coordinates are common in GLES draws.
    // Their float products already identify the correct texel unless rounding
    // lands exactly on an integer boundary. Treat that ambiguous case as a
    // miss and let the precise double/fixed-point-compatible path below handle
    // it. This keeps double and 64-bit conversion off the common path.
    const float fast_u = u * static_cast<float>(texture.width);
    const float fast_v = v * static_cast<float>(texture.height);
    if (fast_u >= 0.0f && fast_u < static_cast<float>(texture.width) &&
        fast_v >= 0.0f && fast_v < static_cast<float>(texture.height))
    {
        const uint16_t fast_x = static_cast<uint16_t>(fast_u);
        const uint16_t fast_y = static_cast<uint16_t>(fast_v);
        if (fast_u != static_cast<float>(fast_x) &&
            fast_v != static_cast<float>(fast_y))
        {
            pixel = texture.pixels[
                static_cast<size_t>(fast_y) * texture.stride + fast_x];
            return true;
        }
    }

    // Preserve the full-range behavior and exact boundary decisions of the
    // original GLES conversion for negative, out-of-range or ambiguous values.
    constexpr double coordinate_limit = 140737488355328.0; // 2^47
    const double texel_u = static_cast<double>(u) * texture.width;
    const double texel_v = static_cast<double>(v) * texture.height;
    if (texel_u < -coordinate_limit || texel_u >= coordinate_limit ||
        texel_v < -coordinate_limit || texel_v >= coordinate_limit)
        return false;

    // Integer conversion truncates toward zero; correct negative fractions
    // to mathematical floor, matching split_coordinate() in the old path.
    int64_t x = static_cast<int64_t>(texel_u);
    int64_t y = static_cast<int64_t>(texel_v);
    if (static_cast<double>(x) > texel_u) --x;
    if (static_cast<double>(y) > texel_v) --y;

    // In-range results need no clamp/repeat/mirror address arithmetic.
    if (x >= 0 && x < texture.width &&
        y >= 0 && y < texture.height)
    {
        pixel = texture.pixels[
            static_cast<size_t>(y) * texture.stride +
            static_cast<size_t>(x)];
        return true;
    }

    const uint16_t addressed_x =
        address_index(x, texture.width, sampler.address_u);
    const uint16_t addressed_y =
        address_index(y, texture.height, sampler.address_v);
    pixel = texture.pixels[
        static_cast<size_t>(addressed_y) * texture.stride + addressed_x];
    return true;
}

uint16_t sample_texture(
    const Texture2DView& texture,
    int64_t u_16_16,
    int64_t v_16_16,
    const TextureSampler& sampler,
    float lod)
{
    if (!texture_valid(texture) || !sampler_valid(sampler))
        return 0;
    return sample_texture_validated(texture, u_16_16, v_16_16, sampler, lod);
}

} // namespace gfx
