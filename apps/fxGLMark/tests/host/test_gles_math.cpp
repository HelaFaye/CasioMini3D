#include <cassert>
#include <cmath>
#include <cstdio>

#include "gles/math.hpp"

namespace
{
using fxgl::ClipVertex;
using fxgl::Mat4;
using fxgl::Vec4;

static bool near(float a, float b, float epsilon = 0.0001f)
{
    return std::fabs(a - b) <= epsilon;
}

static void test_column_major_identity_and_multiplication()
{
    Mat4 identity;
    fxgl::mat4_identity(identity);

    const Vec4 v = {1.0f, -2.0f, 3.0f, 1.0f};
    const Vec4 transformed = fxgl::mat4_transform(identity, v);
    assert(near(transformed.x, v.x));
    assert(near(transformed.y, v.y));
    assert(near(transformed.z, v.z));
    assert(near(transformed.w, v.w));

    Mat4 translate = {};
    fxgl::mat4_identity(translate);
    translate.m[12] = 4.0f;
    translate.m[13] = -3.0f;
    translate.m[14] = 2.0f;

    Mat4 scale = {};
    fxgl::mat4_identity(scale);
    scale.m[0] = 2.0f;
    scale.m[5] = 3.0f;
    scale.m[10] = 4.0f;

    Mat4 combined = {};
    fxgl::mat4_multiply(combined, translate, scale);
    const Vec4 result = fxgl::mat4_transform(combined, v);
    assert(near(result.x, 6.0f));
    assert(near(result.y, -9.0f));
    assert(near(result.z, 14.0f));
    assert(near(result.w, 1.0f));

    // The API permits output aliasing either input.
    fxgl::mat4_multiply(translate, translate, scale);
    const Vec4 alias_result = fxgl::mat4_transform(translate, v);
    assert(near(alias_result.x, result.x));
    assert(near(alias_result.y, result.y));
    assert(near(alias_result.z, result.z));
}

static void test_perspective_projection_clip_depth()
{
    Mat4 projection = {};
    constexpr float pi = 3.14159265358979323846f;
    assert(fxgl::mat4_perspective(projection, pi / 2.0f, 2.0f, 1.0f, 10.0f));

    const Vec4 near_point = fxgl::mat4_transform(
        projection, Vec4{0.0f, 0.0f, -1.0f, 1.0f});
    const Vec4 far_point = fxgl::mat4_transform(
        projection, Vec4{0.0f, 0.0f, -10.0f, 1.0f});
    assert(near(near_point.z / near_point.w, -1.0f));
    assert(near(far_point.z / far_point.w, 1.0f));
    assert(near(projection.m[0], 0.5f));
    assert(near(projection.m[5], 1.0f));

    assert(!fxgl::mat4_perspective(projection, 0.0f, 1.0f, 1.0f, 10.0f));
    for (float value : projection.m)
        assert(value == 0.0f);

    assert(!fxgl::mat4_perspective(projection, pi / 2.0f, 1.0f, 10.0f, 1.0f));
    assert(!fxgl::mat4_perspective(projection, pi / 2.0f, 0.0f, 1.0f, 10.0f));
}

static ClipVertex vertex(float x, float y, float z, float w, float varying)
{
    ClipVertex result = {};
    result.position = {x, y, z, w};
    result.varying[0] = varying;
    return result;
}

static void assert_inside_clip_volume(const ClipVertex& v)
{
    const float epsilon = 0.0001f;
    assert(v.position.x + v.position.w >= -epsilon);
    assert(v.position.w - v.position.x >= -epsilon);
    assert(v.position.y + v.position.w >= -epsilon);
    assert(v.position.w - v.position.y >= -epsilon);
    assert(v.position.z + v.position.w >= -epsilon);
    assert(v.position.w - v.position.z >= -epsilon);
}

static void test_fully_inside_triangle_is_unchanged()
{
    const ClipVertex input[3] = {
        vertex(-0.5f, -0.5f, 0.0f, 1.0f, 1.0f),
        vertex(0.5f, -0.5f, 0.0f, 1.0f, 2.0f),
        vertex(0.0f, 0.5f, 0.0f, 1.0f, 3.0f)
    };
    ClipVertex output[fxgl::MAX_CLIPPED_POLYGON_VERTICES] = {};
    size_t count = 0;
    assert(fxgl::clip_triangle(input, 1, output, count));
    assert(count == 3);
    for (size_t i = 0; i < count; ++i)
    {
        assert_inside_clip_volume(output[i]);
        assert(near(output[i].varying[0], input[i].varying[0]));
    }
}

static void test_crossing_left_plane_preserves_varying_interpolation()
{
    const ClipVertex input[3] = {
        vertex(-2.0f, 0.0f, 0.0f, 1.0f, 0.0f),
        vertex(0.0f, -0.5f, 0.0f, 1.0f, 10.0f),
        vertex(0.0f, 0.5f, 0.0f, 1.0f, 20.0f)
    };
    ClipVertex output[fxgl::MAX_CLIPPED_POLYGON_VERTICES] = {};
    size_t count = 0;
    assert(fxgl::clip_triangle(input, 1, output, count));
    assert(count == 4);

    bool found_low = false;
    bool found_high = false;
    for (size_t i = 0; i < count; ++i)
    {
        assert_inside_clip_volume(output[i]);
        if (near(output[i].position.y, -0.25f) &&
            near(output[i].varying[0], 5.0f))
            found_low = true;
        if (near(output[i].position.y, 0.25f) &&
            near(output[i].varying[0], 10.0f))
            found_high = true;
    }
    assert(found_low && found_high);
}

static void test_each_clip_plane()
{
    for (unsigned plane = 0; plane < 6; ++plane)
    {
        ClipVertex input[3] = {
            vertex(0.0f, 0.0f, 0.0f, 1.0f, 0.0f),
            vertex(0.5f, -0.5f, 0.0f, 1.0f, 1.0f),
            vertex(-0.5f, 0.5f, 0.0f, 1.0f, 2.0f)
        };

        switch (plane)
        {
            case 0: input[0].position.x = -2.0f; break;
            case 1: input[0].position.x = 2.0f; break;
            case 2: input[0].position.y = -2.0f; break;
            case 3: input[0].position.y = 2.0f; break;
            case 4: input[0].position.z = -2.0f; break;
            default: input[0].position.z = 2.0f; break;
        }

        ClipVertex output[fxgl::MAX_CLIPPED_POLYGON_VERTICES] = {};
        size_t count = 0;
        assert(fxgl::clip_triangle(input, 1, output, count));
        assert(count >= 3 && count <= fxgl::MAX_CLIPPED_POLYGON_VERTICES);
        for (size_t i = 0; i < count; ++i)
            assert_inside_clip_volume(output[i]);
    }
}

static void test_rejected_and_fully_clipped_triangles()
{
    const ClipVertex outside[3] = {
        vertex(-3.0f, 0.0f, 0.0f, 1.0f, 0.0f),
        vertex(-2.0f, 0.5f, 0.0f, 1.0f, 1.0f),
        vertex(-2.0f, -0.5f, 0.0f, 1.0f, 2.0f)
    };
    ClipVertex output[fxgl::MAX_CLIPPED_POLYGON_VERTICES] = {};
    size_t count = 123;
    assert(fxgl::clip_triangle(outside, 1, output, count));
    assert(count == 0);

    ClipVertex invalid[3] = {outside[0], outside[1], outside[2]};
    invalid[0].position.w = std::nanf("");
    assert(!fxgl::clip_triangle(invalid, 1, output, count));
    assert(count == 0);

    assert(!fxgl::clip_triangle(outside, fxgl::MAX_VARYINGS + 1, output, count));
    assert(count == 0);
}
} // namespace

int main()
{
    test_column_major_identity_and_multiplication();
    test_perspective_projection_clip_depth();
    test_fully_inside_triangle_is_unchanged();
    test_crossing_left_plane_preserves_varying_interpolation();
    test_each_clip_plane();
    test_rejected_and_fully_clipped_triangles();

    std::puts("GLES matrix and clipper tests: PASS");
    return 0;
}
