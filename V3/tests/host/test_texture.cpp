#include <cassert>
#include <cstdint>
#include <cstdio>

#include "../../src/gfx/texture.hpp"

using gfx::Texture2DView;
using gfx::TextureAddressMode;
using gfx::TextureFilter;
using gfx::TextureSampler;
using gfx::TextureMipLevel;

namespace
{
constexpr int64_t ONE = 65536;

static TextureSampler nearest_clamp()
{
    return {TextureFilter::Nearest,
            TextureAddressMode::Clamp,
            TextureAddressMode::Clamp};
}

static TextureSampler nearest_repeat()
{
    return {TextureFilter::Nearest,
            TextureAddressMode::Repeat,
            TextureAddressMode::Repeat};
}

static void test_view_and_sampler_validation()
{
    const uint16_t pixels[4] = {0xf800, 0x07e0, 0x001f, 0xffff};
    assert(gfx::texture_valid({pixels, 2, 2, 2}));
    assert(!gfx::texture_valid({nullptr, 2, 2, 2}));
    assert(!gfx::texture_valid({pixels, 0, 2, 2}));
    assert(!gfx::texture_valid({pixels, 2, 0, 2}));
    assert(!gfx::texture_valid({pixels, 2, 2, 1}));

    assert(gfx::sampler_valid(nearest_clamp()));
    assert(gfx::sampler_valid({TextureFilter::Bilinear,
                               TextureAddressMode::Repeat,
                               TextureAddressMode::Clamp}));
    assert(!gfx::sampler_valid({static_cast<TextureFilter>(99),
                                TextureAddressMode::Clamp,
                                TextureAddressMode::Clamp}));
    assert(!gfx::sampler_valid({TextureFilter::Nearest,
                                static_cast<TextureAddressMode>(99),
                                TextureAddressMode::Clamp}));
}

static void test_nearest_texel_centres_and_stride()
{
    const uint16_t pixels[8] = {
        0xf800, 0x07e0, 0x001f, 0xdead,
        0x07ff, 0xf81f, 0xffe0, 0xbeef
    };
    const Texture2DView texture = {pixels, 3, 2, 4};
    const TextureSampler sampler = nearest_clamp();

    assert(gfx::sample_texture(texture, 0, 0, sampler) == 0xf800);
    assert(gfx::sample_texture(texture, ONE, 0, sampler) == 0x07e0);
    assert(gfx::sample_texture(texture, 2 * ONE, ONE, sampler) == 0xffe0);
    // The fourth element in each row is padding and must never be sampled.
    assert(gfx::sample_texture(texture, 3 * ONE, 0, sampler) == 0x001f);
}

static void test_clamp_and_repeat_addressing()
{
    const uint16_t pixels[4] = {0xf800, 0x07e0, 0x001f, 0xffe0};
    const Texture2DView texture = {pixels, 2, 2, 2};

    assert(gfx::sample_texture(texture, -ONE, -ONE, nearest_clamp()) == 0xf800);
    assert(gfx::sample_texture(texture, 2 * ONE, 0, nearest_clamp()) == 0x07e0);
    assert(gfx::sample_texture(texture, 0, 2 * ONE, nearest_clamp()) == 0x001f);

    const TextureSampler repeat = nearest_repeat();
    assert(gfx::sample_texture(texture, -ONE, -ONE, repeat) == 0xffe0);
    assert(gfx::sample_texture(texture, 2 * ONE, 0, repeat) == 0xf800);
    assert(gfx::sample_texture(texture, 0, 2 * ONE, repeat) == 0xf800);
}

static void test_negative_fractional_coordinates_use_floor()
{
    const uint16_t pixels[4] = {0xf800, 0x07e0, 0x001f, 0xffe0};
    const Texture2DView texture = {pixels, 2, 2, 2};

    // floor(-0.25) = -1, then repeat maps -1 to the last texel.
    assert(gfx::sample_texture(texture, -(ONE / 4), 0, nearest_repeat()) == 0x07e0);
    assert(gfx::sample_texture(texture, -(ONE / 4), 0, nearest_clamp()) == 0xf800);
}

static void test_bilinear_interpolation_rgb565()
{
    const uint16_t pixels[4] = {
        0xf800, 0x001f,
        0x07e0, 0xffff
    };
    const Texture2DView texture = {pixels, 2, 2, 2};
    const TextureSampler bilinear = {
        TextureFilter::Bilinear,
        TextureAddressMode::Clamp,
        TextureAddressMode::Clamp
    };

    assert(gfx::sample_texture(texture, 0, 0, bilinear) == 0xf800);
    assert(gfx::sample_texture(texture, ONE, 0, bilinear) == 0x001f);
    assert(gfx::sample_texture(texture, 0, ONE, bilinear) == 0x07e0);
    assert(gfx::sample_texture(texture, ONE, ONE, bilinear) == 0xffff);
    assert(gfx::sample_texture(texture, ONE / 2, ONE / 2, bilinear) == 0x8410);

    // Clamp at the left edge: the negative neighbor resolves to column zero.
    assert(gfx::sample_texture(texture, -(ONE / 2), ONE / 2, bilinear) == 0x8400);
}

static void test_mirrored_repeat_addressing()
{
    const uint16_t pixels[4] = {0xf800, 0x07e0, 0x001f, 0xffff};
    const Texture2DView texture = {pixels, 2, 2, 2};
    const TextureSampler mirrored = {
        TextureFilter::Nearest,
        TextureAddressMode::MirroredRepeat,
        TextureAddressMode::MirroredRepeat
    };

    assert(gfx::sample_texture(texture, -ONE, 0, mirrored) == 0xf800);
    assert(gfx::sample_texture(texture, 2 * ONE, 0, mirrored) == 0x07e0);
    assert(gfx::sample_texture(texture, 3 * ONE, 0, mirrored) == 0xf800);
    assert(gfx::sample_texture(texture, 4 * ONE, 0, mirrored) == 0xf800);
    assert(gfx::sample_texture(texture, -2 * ONE, ONE, mirrored) == 0xffff);
}

static void test_mipmap_nearest_and_trilinear_filtering()
{
    const uint16_t base_pixels[16] = {
        0xf800, 0xf800, 0xf800, 0xf800,
        0xf800, 0xf800, 0xf800, 0xf800,
        0xf800, 0xf800, 0xf800, 0xf800,
        0xf800, 0xf800, 0xf800, 0xf800
    };
    const uint16_t level1_pixels[4] = {
        0x07e0, 0x07e0,
        0x07e0, 0x07e0
    };
    const uint16_t level2_pixels[1] = {0x001f};
    const TextureMipLevel levels[2] = {
        {level1_pixels, 2, 2, 2},
        {level2_pixels, 1, 1, 1}
    };
    const Texture2DView texture = {
        base_pixels, 4, 4, 4, levels, 2
    };

    TextureSampler sampler = nearest_clamp();
    sampler.min_filter = gfx::TextureMinFilter::NearestMipmapNearest;
    assert(gfx::texture_valid(texture));
    assert(gfx::sample_texture(texture, ONE, ONE, sampler, 1.0f) == 0x07e0);
    assert(gfx::sample_texture(texture, ONE, ONE, sampler, 2.0f) == 0x001f);
    // Nearest-mipmap filters round LOD to the closest integer level.
    assert(gfx::sample_texture(texture, ONE, ONE, sampler, 1.4f) == 0x07e0);
    assert(gfx::sample_texture(texture, ONE, ONE, sampler, 1.6f) == 0x001f);

    sampler.min_filter = gfx::TextureMinFilter::LinearMipmapNearest;
    assert(gfx::sample_texture(texture, ONE, ONE, sampler, 1.0f) == 0x07e0);

    sampler.min_filter = gfx::TextureMinFilter::NearestMipmapLinear;
    assert(gfx::sample_texture(texture, ONE, ONE, sampler, 0.5f) == 0x8400);

    sampler.min_filter = gfx::TextureMinFilter::LinearMipmapLinear;
    assert(gfx::sample_texture(texture, ONE, ONE, sampler, 0.5f) == 0x8400);

    sampler.min_filter = gfx::TextureMinFilter::Linear;
    assert(gfx::sample_texture(texture, ONE, ONE, sampler, 2.0f) == 0xf800);

    const TextureMipLevel partial_levels[1] = {
        {level1_pixels, 2, 2, 2}
    };
    const Texture2DView incomplete = {
        base_pixels, 4, 4, 4, partial_levels, 1
    };
    sampler.min_filter = gfx::TextureMinFilter::NearestMipmapNearest;
    assert(gfx::sample_texture(incomplete, ONE, ONE, sampler, 1.0f) == 0);
}

static void test_mipmap_view_and_sampler_validation()
{
    const uint16_t base[16] = {};
    const uint16_t bad_level[1] = {};
    const TextureMipLevel bad_levels[1] = {
        {bad_level, 1, 1, 1}
    };
    // A 4x4 base must have a 2x2 first mip, not a 1x1 level.
    assert(!gfx::texture_valid({base, 4, 4, 4, bad_levels, 1}));

    TextureSampler sampler = nearest_clamp();
    sampler.min_filter = static_cast<gfx::TextureMinFilter>(99);
    assert(!gfx::sampler_valid(sampler));
}

static void test_invalid_inputs_fail_closed()
{
    const uint16_t pixels[4] = {0xf800, 0x07e0, 0x001f, 0xffff};
    const Texture2DView invalid = {pixels, 2, 2, 1};
    assert(gfx::sample_texture(invalid, 0, 0, nearest_clamp()) == 0);

    const Texture2DView valid = {pixels, 2, 2, 2};
    const TextureSampler invalid_sampler = {
        static_cast<TextureFilter>(99),
        TextureAddressMode::Clamp,
        TextureAddressMode::Clamp
    };
    assert(gfx::sample_texture(valid, 0, 0, invalid_sampler) == 0);
}
} // namespace

int main()
{
    test_view_and_sampler_validation();
    test_nearest_texel_centres_and_stride();
    test_clamp_and_repeat_addressing();
    test_negative_fractional_coordinates_use_floor();
    test_bilinear_interpolation_rgb565();
    test_mirrored_repeat_addressing();
    test_mipmap_nearest_and_trilinear_filtering();
    test_mipmap_view_and_sampler_validation();
    test_invalid_inputs_fail_closed();

    std::puts("texture sampler tests: PASS");
    return 0;
}
