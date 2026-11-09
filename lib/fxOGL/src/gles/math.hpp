#pragma once

#include <stddef.h>

namespace fxgl
{
constexpr size_t MAX_VARYINGS = 8;
constexpr size_t MAX_CLIPPED_POLYGON_VERTICES = 9;

struct Vec4
{
    float x, y, z, w;
};

struct Mat4
{
    /* OpenGL column-major storage: element(row, column) = m[column*4+row]. */
    float m[16];
};

struct ClipVertex
{
    Vec4 position;
    /* Vertex-shader gl_PointSize; the 1-pixel default preserves legacy callers. */
    float point_size = 1.0f;
    float varying[MAX_VARYINGS];
};

/* OpenGL/GLES column-major matrix operations. Inputs may alias output. */
void mat4_identity(Mat4& out);
void mat4_multiply(Mat4& out, const Mat4& left, const Mat4& right);
Vec4 mat4_transform(const Mat4& matrix, Vec4 vector);

/* Right-handed perspective projection with the OpenGL clip-depth range [-w,+w].
   Invalid parameters produce an all-zero matrix and return false. */
bool mat4_perspective(
    Mat4& out,
    float vertical_fov_radians,
    float aspect,
    float near_plane,
    float far_plane);

/* Clip a triangle against all six homogeneous clip planes.
   Each input vertex carries varying_count scalar attributes. Output contains
   the clipped convex polygon in boundary order, ready for a triangle fan.
   Returns false for invalid inputs or output-capacity failure. */
bool clip_triangle(
    const ClipVertex input[3],
    size_t varying_count,
    ClipVertex output[MAX_CLIPPED_POLYGON_VERTICES],
    size_t& output_count);

} // namespace fxgl
