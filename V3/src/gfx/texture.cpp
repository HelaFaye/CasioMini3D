#include "texture.hpp"

#include <stddef.h>

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
    int64_t wrapped = index % extent;
    if (wrapped < 0)
        wrapped += extent;
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
    return texture.pixels != nullptr &&
           texture.width > 0 &&
           texture.height > 0 &&
           texture.stride >= texture.width;
}

bool sampler_valid(const TextureSampler& sampler)
{
    const bool filter_ok =
        sampler.filter == TextureFilter::Nearest ||
        sampler.filter == TextureFilter::Bilinear;
    const bool address_u_ok =
        sampler.address_u == TextureAddressMode::Clamp ||
        sampler.address_u == TextureAddressMode::Repeat;
    const bool address_v_ok =
        sampler.address_v == TextureAddressMode::Clamp ||
        sampler.address_v == TextureAddressMode::Repeat;
    return filter_ok && address_u_ok && address_v_ok;
}

uint16_t sample_texture(
    const Texture2DView& texture,
    int64_t u_16_16,
    int64_t v_16_16,
    const TextureSampler& sampler)
{
    if (!texture_valid(texture) || !sampler_valid(sampler))
        return 0;

    const FixedCoordinate u = split_coordinate(u_16_16);
    const FixedCoordinate v = split_coordinate(v_16_16);

    if (sampler.filter == TextureFilter::Nearest)
        return texel(texture, u.whole, v.whole, sampler);

    const uint16_t top_left = texel(texture, u.whole, v.whole, sampler);
    const uint16_t top_right = texel(texture, u.whole + 1, v.whole, sampler);
    const uint16_t bottom_left = texel(texture, u.whole, v.whole + 1, sampler);
    const uint16_t bottom_right = texel(
        texture, u.whole + 1, v.whole + 1, sampler);

    const uint16_t top = interpolate_rgb565(top_left, top_right, u.fraction);
    const uint16_t bottom =
        interpolate_rgb565(bottom_left, bottom_right, u.fraction);
    return interpolate_rgb565(top, bottom, v.fraction);
}

} // namespace gfx
