#include <array>
#include <cassert>
#include <climits>
#include <cstddef>
#include <cstdio>

#include "../../src/shapes/shapes.hpp"

namespace
{
constexpr int EMPTY_PIXEL = -1;
constexpr int SOLID_COLOR = 0x1234;

std::array<int, SCREEN_WIDTH * SCREEN_HEIGHT> framebuffer;
size_t pixel_writes = 0;
size_t uv_reads = 0;
size_t invalid_pixel_writes = 0;
size_t invalid_uv_reads = 0;

void reset_framebuffer()
{
    framebuffer.fill(EMPTY_PIXEL);
    pixel_writes = 0;
    uv_reads = 0;
    invalid_pixel_writes = 0;
    invalid_uv_reads = 0;
}

bool same_framebuffer(
    const std::array<int, SCREEN_WIDTH * SCREEN_HEIGHT>& a,
    const std::array<int, SCREEN_WIDTH * SCREEN_HEIGHT>& b)
{
    return a == b;
}
}

void DrawPixel(int x, int y, int color)
{
    if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT)
    {
        ++invalid_pixel_writes;
        return;
    }

    framebuffer[static_cast<size_t>(y) * SCREEN_WIDTH + x] = color;
    ++pixel_writes;
}

int get_uv_map(int x, int y)
{
    ++uv_reads;
    if (x < 0 || x >= 40 || y < 0 || y >= 40)
    {
        ++invalid_uv_reads;
        return C_BLACK;
    }
    return 0x2000 + y * 40 + x;
}

static void test_solid_triangle_coverage_and_color()
{
    reset_framebuffer();

    const VertexLocal a = {10, 10, 0, 0};
    const VertexLocal b = {30, 10, 0, 0};
    const VertexLocal c = {10, 30, 0, 0};

    DrawTriangleSolid(a, b, c, SOLID_COLOR);

    // Preserve the textured rasterizer's current inclusive-edge convention.
    assert(pixel_writes == 230);
    assert(uv_reads == 0);
    assert(invalid_pixel_writes == 0);
    for (int pixel : framebuffer)
        assert(pixel == EMPTY_PIXEL || pixel == SOLID_COLOR);
}

static void test_winding_and_vertex_order_invariance()
{
    const VertexLocal vertices[3] = {
        {10, 10, 0, 0},
        {30, 10, 0, 0},
        {10, 30, 0, 0}
    };

    reset_framebuffer();
    DrawTriangleSolid(vertices[0], vertices[1], vertices[2], SOLID_COLOR);
    const auto reference = framebuffer;

    const int permutations[6][3] = {
        {0, 1, 2}, {0, 2, 1}, {1, 0, 2},
        {1, 2, 0}, {2, 0, 1}, {2, 1, 0}
    };

    for (const auto& permutation : permutations)
    {
        reset_framebuffer();
        DrawTriangleSolid(
            vertices[permutation[0]],
            vertices[permutation[1]],
            vertices[permutation[2]],
            SOLID_COLOR);
        assert(same_framebuffer(reference, framebuffer));
        assert(uv_reads == 0);
        assert(invalid_pixel_writes == 0);
    }
}

static void test_flat_top_and_flat_bottom()
{
    reset_framebuffer();
    DrawTriangleSolid(
        VertexLocal{10, 10, 0, 0},
        VertexLocal{30, 10, 0, 0},
        VertexLocal{20, 30, 0, 0},
        SOLID_COLOR);
    assert(pixel_writes > 0);
    assert(invalid_pixel_writes == 0);
    assert(uv_reads == 0);

    reset_framebuffer();
    DrawTriangleSolid(
        VertexLocal{20, 10, 0, 0},
        VertexLocal{10, 30, 0, 0},
        VertexLocal{30, 30, 0, 0},
        SOLID_COLOR);
    assert(pixel_writes > 0);
    assert(invalid_pixel_writes == 0);
    assert(uv_reads == 0);
}

static void test_degenerate_triangles_draw_nothing()
{
    reset_framebuffer();
    DrawTriangleSolid(
        VertexLocal{5, 10, 0, 0},
        VertexLocal{20, 10, 0, 0},
        VertexLocal{35, 10, 0, 0},
        SOLID_COLOR);
    assert(pixel_writes == 0);

    DrawTriangleSolid(
        VertexLocal{10, 5, 0, 0},
        VertexLocal{10, 20, 0, 0},
        VertexLocal{10, 35, 0, 0},
        SOLID_COLOR);
    assert(pixel_writes == 0);

    DrawTriangleSolid(
        VertexLocal{10, 10, 0, 0},
        VertexLocal{20, 20, 0, 0},
        VertexLocal{30, 30, 0, 0},
        SOLID_COLOR);
    assert(pixel_writes == 0);
    assert(invalid_pixel_writes == 0);
}

static void test_off_screen_and_clipped_triangles()
{
    reset_framebuffer();
    DrawTriangleSolid(
        VertexLocal{-100, -100, 0, 0},
        VertexLocal{-20, -100, 0, 0},
        VertexLocal{-50, -10, 0, 0},
        SOLID_COLOR);
    assert(pixel_writes == 0);

    DrawTriangleSolid(
        VertexLocal{-30, 30, 0, 0},
        VertexLocal{40, -20, 0, 0},
        VertexLocal{50, 80, 0, 0},
        SOLID_COLOR);
    assert(pixel_writes > 0);
    assert(invalid_pixel_writes == 0);

    DrawTriangleSolid(
        VertexLocal{INT_MIN, 112, 0, 0},
        VertexLocal{INT_MAX, 0, 0, 0},
        VertexLocal{INT_MAX, 223, 0, 0},
        SOLID_COLOR);
    assert(invalid_pixel_writes == 0);
    assert(uv_reads == 0);

    DrawTriangleSolid(
        VertexLocal{20, 200, 0, 0},
        VertexLocal{40, 250, 0, 0},
        VertexLocal{0, 250, 0, 0},
        SOLID_COLOR);
    assert(pixel_writes > 0);
    assert(invalid_pixel_writes == 0);

    reset_framebuffer();
    DrawTriangleSolid(
        VertexLocal{20, 230, 0, 0},
        VertexLocal{40, 250, 0, 0},
        VertexLocal{0, 250, 0, 0},
        SOLID_COLOR);
    assert(pixel_writes == 0);
    assert(invalid_pixel_writes == 0);
}

static void test_textured_path_still_samples_uv_map()
{
    reset_framebuffer();

    DrawTriangle(
        VertexLocal{10, 10, 0, 0},
        VertexLocal{30, 10, FLOAT_TO_FIXED(39), 0},
        VertexLocal{10, 30, 0, FLOAT_TO_FIXED(39)});

    assert(pixel_writes == 230);
    assert(uv_reads == pixel_writes);
    assert(invalid_pixel_writes == 0);
    assert(invalid_uv_reads == 0);
}

int main()
{
    test_solid_triangle_coverage_and_color();
    test_winding_and_vertex_order_invariance();
    test_flat_top_and_flat_bottom();
    test_degenerate_triangles_draw_nothing();
    test_off_screen_and_clipped_triangles();
    test_textured_path_still_samples_uv_map();

    std::puts("triangle rasterizer tests: PASS");
    return 0;
}
