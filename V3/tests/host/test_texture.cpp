#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>

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

static void test_sampler_derivative_requirements()
{
    TextureSampler sampler = nearest_clamp();
    assert(!gfx::sampler_requires_implicit_derivatives(sampler));

    sampler.min_filter = gfx::TextureMinFilter::Linear;
    sampler.filter = TextureFilter::Bilinear;
    assert(!gfx::sampler_requires_implicit_derivatives(sampler));

    // Different minification/magnification filters need LOD selection.
    sampler.filter = TextureFilter::Nearest;
    assert(gfx::sampler_requires_implicit_derivatives(sampler));

    sampler.min_filter = gfx::TextureMinFilter::Nearest;
    sampler.filter = TextureFilter::Bilinear;
    assert(gfx::sampler_requires_implicit_derivatives(sampler));

    // Every mipmapped minification mode requires implicit LOD.
    sampler.filter = TextureFilter::Nearest;
    sampler.min_filter = gfx::TextureMinFilter::NearestMipmapNearest;
    assert(gfx::sampler_requires_implicit_derivatives(sampler));
    sampler.min_filter = gfx::TextureMinFilter::LinearMipmapNearest;
    assert(gfx::sampler_requires_implicit_derivatives(sampler));
    sampler.min_filter = gfx::TextureMinFilter::NearestMipmapLinear;
    assert(gfx::sampler_requires_implicit_derivatives(sampler));
    sampler.min_filter = gfx::TextureMinFilter::LinearMipmapLinear;
    assert(gfx::sampler_requires_implicit_derivatives(sampler));

    sampler.min_filter = static_cast<gfx::TextureMinFilter>(99);
    assert(gfx::sampler_requires_implicit_derivatives(sampler));
}

static void test_validated_sampler_matches_checked_sampler()
{
    const uint16_t base[16] = {
        0xf800, 0xf800, 0x07e0, 0x07e0,
        0xf800, 0xf800, 0x07e0, 0x07e0,
        0x001f, 0x001f, 0xffff, 0xffff,
        0x001f, 0x001f, 0xffff, 0xffff
    };
    const uint16_t mip1[4] = {0xf800, 0x07e0, 0x001f, 0xffff};
    const uint16_t mip2[1] = {0x8410};
    const TextureMipLevel levels[2] = {
        {mip1, 2, 2, 2}, {mip2, 1, 1, 1}
    };
    const Texture2DView texture = {base, 4, 4, 4, levels, 2};
    TextureSampler samplers[6] = {
        nearest_clamp(),
        {TextureFilter::Bilinear, TextureAddressMode::Clamp,
         TextureAddressMode::Clamp, gfx::TextureMinFilter::Linear},
        {TextureFilter::Nearest, TextureAddressMode::Repeat,
         TextureAddressMode::MirroredRepeat, gfx::TextureMinFilter::NearestMipmapNearest},
        {TextureFilter::Bilinear, TextureAddressMode::Repeat,
         TextureAddressMode::Clamp, gfx::TextureMinFilter::LinearMipmapNearest},
        {TextureFilter::Nearest, TextureAddressMode::MirroredRepeat,
         TextureAddressMode::Repeat, gfx::TextureMinFilter::NearestMipmapLinear},
        {TextureFilter::Bilinear, TextureAddressMode::Clamp,
         TextureAddressMode::MirroredRepeat, gfx::TextureMinFilter::LinearMipmapLinear}
    };
    const int64_t coordinates[][2] = {
        {0, 0}, {ONE / 2, ONE / 2}, {ONE, ONE},
        {-ONE / 2, 3 * ONE / 2}, {3 * ONE, 2 * ONE}
    };
    const float lods[] = {0.0f, 0.25f, 0.5f, 1.0f, 1.4f, 1.6f, 2.0f};

    assert(gfx::texture_valid(texture));
    for (const TextureSampler& sampler : samplers)
    {
        assert(gfx::sampler_valid(sampler));
        for (const auto& coordinate : coordinates)
        {
            for (float lod : lods)
            {
                const uint16_t checked = gfx::sample_texture(
                    texture, coordinate[0], coordinate[1], sampler, lod);
                const uint16_t fast = gfx::sample_texture_validated(
                    texture, coordinate[0], coordinate[1], sampler, lod);
                assert(checked == fast);
            }
        }
    }
}

static void test_normalized_nearest_fast_path_matches_fixed_point()
{
    const uint16_t pixels[12] = {
        0xf800, 0x07e0, 0x001f, 0xaaaa,
        0xffe0, 0xf81f, 0x07ff, 0xbbbb,
        0x8410, 0x4208, 0xffff, 0xcccc
    };
    const Texture2DView texture = {pixels, 3, 3, 4};

    TextureSampler samplers[] = {
        nearest_clamp(),
        nearest_repeat(),
        {TextureFilter::Nearest, TextureAddressMode::MirroredRepeat,
         TextureAddressMode::Repeat}
    };
    const float boundaries[] = {
        -1000.5f, -4.25f, -1.0f, -0.5001f, -0.001f, -0.0f,
        0.0f, 0.0001f, 0.25f, 0.33333334f, 0.5f, 0.9999f,
        1.0f, 1.0001f, 1.25f, 2.75f, 1000.5f
    };

    assert(gfx::texture_valid(texture));
    for (const TextureSampler& sampler : samplers)
    {
        assert(gfx::sampler_valid(sampler));
        for (float u : boundaries)
        {
            for (float v : boundaries)
            {
                const double texel_u =
                    static_cast<double>(u) * texture.width;
                const double texel_v =
                    static_cast<double>(v) * texture.height;
                const int64_t fixed_u = static_cast<int64_t>(
                    std::floor(texel_u * 65536.0));
                const int64_t fixed_v = static_cast<int64_t>(
                    std::floor(texel_v * 65536.0));
                const uint16_t expected = gfx::sample_texture_validated(
                    texture, fixed_u, fixed_v, sampler, 0.0f);

                uint16_t actual = 0;
                assert(gfx::sample_texture_nearest_validated(
                    texture, u, v, sampler, actual));
                assert(actual == expected);
            }
        }

        // Cover a larger set of fractional and negative coordinates.
        for (int i = -300; i <= 300; ++i)
        {
            const float u = static_cast<float>(i) / 37.0f;
            const float v = static_cast<float>(i) / 53.0f;
            const int64_t fixed_u = static_cast<int64_t>(std::floor(
                static_cast<double>(u) * texture.width * 65536.0));
            const int64_t fixed_v = static_cast<int64_t>(std::floor(
                static_cast<double>(v) * texture.height * 65536.0));
            const uint16_t expected = gfx::sample_texture_validated(
                texture, fixed_u, fixed_v, sampler, 0.0f);
            uint16_t actual = 0;
            assert(gfx::sample_texture_nearest_validated(
                texture, u, v, sampler, actual));
            assert(actual == expected);
        }
    }

    // Exercise all texel edges for non-power-of-two dimensions, including
    // adjacent representable float values and padded row strides.
    {
        constexpr uint16_t width = 127;
        constexpr uint16_t height = 63;
        constexpr size_t stride = 129;
        static uint16_t boundary_pixels[height * stride] = {};
        for (uint16_t y = 0; y < height; ++y)
        {
            for (uint16_t x = 0; x < stride; ++x)
            {
                boundary_pixels[static_cast<size_t>(y) * stride + x] =
                    static_cast<uint16_t>(
                        (static_cast<uint32_t>(x) * 131u +
                         static_cast<uint32_t>(y) * 71u) ^
                        (static_cast<uint32_t>(x) << 7));
            }
        }

        const Texture2DView boundary_texture = {
            boundary_pixels, width, height, stride
        };
        const TextureSampler boundary_samplers[] = {
            nearest_clamp(),
            nearest_repeat(),
            {TextureFilter::Nearest, TextureAddressMode::MirroredRepeat,
             TextureAddressMode::Repeat}
        };

        for (const TextureSampler& boundary_sampler : boundary_samplers)
        {
            for (int xi = -1; xi <= static_cast<int>(width) + 1; ++xi)
            {
                const float boundary_u =
                    static_cast<float>(xi) / static_cast<float>(width);
                const float us[4] = {
                    std::nextafter(boundary_u,
                        -std::numeric_limits<float>::infinity()),
                    boundary_u,
                    std::nextafter(boundary_u,
                        std::numeric_limits<float>::infinity()),
                    (static_cast<float>(xi) + 0.371f) /
                        static_cast<float>(width)
                };

                for (int yi = -1; yi <= static_cast<int>(height) + 1; ++yi)
                {
                    const float boundary_v =
                        static_cast<float>(yi) / static_cast<float>(height);
                    const float vs[4] = {
                        std::nextafter(boundary_v,
                            -std::numeric_limits<float>::infinity()),
                        boundary_v,
                        std::nextafter(boundary_v,
                            std::numeric_limits<float>::infinity()),
                        (static_cast<float>(yi) + 0.619f) /
                            static_cast<float>(height)
                    };

                    for (float u : us)
                    {
                        for (float v : vs)
                        {
                            const int64_t fixed_u = static_cast<int64_t>(
                                std::floor(static_cast<double>(u) *
                                    width * 65536.0));
                            const int64_t fixed_v = static_cast<int64_t>(
                                std::floor(static_cast<double>(v) *
                                    height * 65536.0));
                            const uint16_t expected =
                                gfx::sample_texture_validated(
                                    boundary_texture, fixed_u, fixed_v,
                                    boundary_sampler, 0.0f);
                            uint16_t actual = 0;
                            assert(gfx::sample_texture_nearest_validated(
                                boundary_texture, u, v,
                                boundary_sampler, actual));
                            assert(actual == expected);
                        }
                    }
                }
            }
        }
    }

    uint16_t unchanged = 0x1234;
    assert(!gfx::sample_texture_nearest_validated(
        texture, 0.5f, 0.5f,
        {TextureFilter::Bilinear, TextureAddressMode::Clamp,
         TextureAddressMode::Clamp},
        unchanged));
    assert(!gfx::sample_texture_nearest_validated(
        texture, 1.0e20f, 0.5f, nearest_clamp(), unchanged));
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
    test_sampler_derivative_requirements();
    test_validated_sampler_matches_checked_sampler();
    test_normalized_nearest_fast_path_matches_fixed_point();
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
