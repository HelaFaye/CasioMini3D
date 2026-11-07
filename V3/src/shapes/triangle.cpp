#include "shapes.hpp"

#include <stdint.h>

template<bool Textured>
struct RasterEdge
{
    int x;
};

template<>
struct RasterEdge<true>
{
    int x;
    fixed_t u;
    fixed_t v;
};

template<bool Textured>
static RasterEdge<Textured> CalcEdge(VertexLocal a, VertexLocal b, int y)
{
    const int64_t delta_y = static_cast<int64_t>(b.y) - a.y;
    if (delta_y == 0)
    {
        if constexpr (Textured)
            return {a.x, a.u, a.v};
        else
            return {a.x};
    }

    // Keep coordinate differences wider than int. This matters when a
    // triangle crosses the screen from very large off-screen coordinates.
    const int64_t factor =
        ((static_cast<int64_t>(y) - a.y) * FIXED_ONE) / delta_y;
    const int64_t delta_x = static_cast<int64_t>(b.x) - a.x;
    const int64_t x_offset = (delta_x * factor) >> PRECISION;
    const int64_t x = static_cast<int64_t>(a.x) + x_offset;

    if constexpr (Textured)
    {
        const fixed_t u = a.u + (((b.u - a.u) * factor) >> PRECISION);
        const fixed_t v = a.v + (((b.v - a.v) * factor) >> PRECISION);
        return {static_cast<int>(x), u, v};
    }
    else
    {
        return {static_cast<int>(x)};
    }
}

template<bool Textured>
static void ProcessScanline(
    int y,
    RasterEdge<Textured> a,
    RasterEdge<Textured> b,
    int solid_color,
    const gfx::Texture2DView* texture,
    const gfx::TextureSampler* sampler)
{
    if (a.x == b.x)
        return;

    if (a.x > b.x)
    {
        const int old_x = a.x;
        a.x = b.x;
        b.x = old_x;

        if constexpr (Textured)
        {
            const fixed_t old_u = a.u;
            const fixed_t old_v = a.v;
            a.u = b.u;
            a.v = b.v;
            b.u = old_u;
            b.v = old_v;
        }
    }

    const int start_x = (a.x < 0) ? 0 : a.x;
    const int end_x = (b.x >= SCREEN_WIDTH) ? SCREEN_WIDTH - 1 : b.x;
    if (start_x > end_x)
        return;

    if constexpr (Textured)
    {
        const fixed_t length = static_cast<fixed_t>(b.x) - a.x;
        const fixed_t du = (b.u - a.u) / length;
        const fixed_t dv = (b.v - a.v) / length;

        fixed_t u = a.u + du * (static_cast<fixed_t>(start_x) - a.x);
        fixed_t v = a.v + dv * (static_cast<fixed_t>(start_x) - a.x);

        for (int x = start_x; x <= end_x; ++x)
        {
            const int color = (texture && sampler)
                ? gfx::sample_texture(*texture, u, v, *sampler)
                : get_uv_map(TO_INT(u), TO_INT(v));
            DrawPixel(x, y, color);
            u += du;
            v += dv;
        }
    }
    else
    {
        // No UV interpolation, texture lookup, or per-pixel shader dispatch.
        for (int x = start_x; x <= end_x; ++x)
            DrawPixel(x, y, solid_color);
    }
}

template<bool Textured>
static void RasterizeTriangle(
    VertexLocal v0,
    VertexLocal v1,
    VertexLocal v2,
    int solid_color,
    const gfx::Texture2DView* texture,
    const gfx::TextureSampler* sampler)
{
    if (v1.y < v0.y) std::swap(v0, v1);
    if (v2.y < v0.y) std::swap(v0, v2);
    if (v2.y < v1.y) std::swap(v1, v2);

    // Degenerate horizontal triangles have no covered scanlines.
    if (v2.y == v0.y)
        return;

    const int start_y = (v0.y < 0) ? 0 : v0.y;
    const int end_y = (v2.y >= SCREEN_HEIGHT) ? SCREEN_HEIGHT - 1 : v2.y;
    if (start_y > end_y)
        return;

    for (int y = start_y; y <= end_y; ++y)
    {
        const bool second_half = (y > v1.y) || (v1.y == v0.y);
        const RasterEdge<Textured> a = CalcEdge<Textured>(v0, v2, y);
        const RasterEdge<Textured> b = second_half
            ? CalcEdge<Textured>(v1, v2, y)
            : CalcEdge<Textured>(v0, v1, y);

        ProcessScanline<Textured>(y, a, b, solid_color, texture, sampler);
    }
}

void DrawTriangle(VertexLocal v0, VertexLocal v1, VertexLocal v2)
{
    RasterizeTriangle<true>(v0, v1, v2, C_WHITE, nullptr, nullptr);
}

void DrawTriangleTextured(
    VertexLocal v0,
    VertexLocal v1,
    VertexLocal v2,
    const gfx::Texture2DView& texture,
    const gfx::TextureSampler& sampler)
{
    if (!gfx::texture_valid(texture) || !gfx::sampler_valid(sampler))
        return;
    RasterizeTriangle<true>(v0, v1, v2, C_WHITE, &texture, &sampler);
}

void DrawTriangleSolid(VertexLocal v0, VertexLocal v1, VertexLocal v2, int color)
{
    RasterizeTriangle<false>(v0, v1, v2, color, nullptr, nullptr);
}
