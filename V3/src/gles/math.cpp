#include "math.hpp"

#include <cmath>
#include <cstring>

namespace fxgl
{
namespace
{
static bool finite_vertex(const ClipVertex& vertex, size_t varying_count)
{
    if (!std::isfinite(vertex.position.x) ||
        !std::isfinite(vertex.position.y) ||
        !std::isfinite(vertex.position.z) ||
        !std::isfinite(vertex.position.w))
        return false;

    for (size_t i = 0; i < varying_count; ++i)
        if (!std::isfinite(vertex.varying[i]))
            return false;
    return true;
}

/* Signed distance to the six OpenGL homogeneous clip planes.
   A point is inside a plane when the distance is >= 0. */
static float plane_distance(const Vec4& p, unsigned plane)
{
    switch (plane)
    {
        case 0: return p.x + p.w; // left
        case 1: return p.w - p.x; // right
        case 2: return p.y + p.w; // bottom
        case 3: return p.w - p.y; // top
        case 4: return p.z + p.w; // near
        default: return p.w - p.z; // far
    }
}

static ClipVertex interpolate(
    const ClipVertex& a,
    const ClipVertex& b,
    float t,
    size_t varying_count)
{
    const float inverse = 1.0f - t;
    ClipVertex result = {};
    result.position.x = a.position.x * inverse + b.position.x * t;
    result.position.y = a.position.y * inverse + b.position.y * t;
    result.position.z = a.position.z * inverse + b.position.z * t;
    result.position.w = a.position.w * inverse + b.position.w * t;
    for (size_t i = 0; i < varying_count; ++i)
        result.varying[i] = a.varying[i] * inverse + b.varying[i] * t;
    return result;
}
} // namespace

void mat4_identity(Mat4& out)
{
    std::memset(out.m, 0, sizeof(out.m));
    out.m[0] = 1.0f;
    out.m[5] = 1.0f;
    out.m[10] = 1.0f;
    out.m[15] = 1.0f;
}

void mat4_multiply(Mat4& out, const Mat4& left, const Mat4& right)
{
    Mat4 result = {};
    for (int column = 0; column < 4; ++column)
    {
        for (int row = 0; row < 4; ++row)
        {
            float value = 0.0f;
            for (int k = 0; k < 4; ++k)
                value += left.m[k * 4 + row] * right.m[column * 4 + k];
            result.m[column * 4 + row] = value;
        }
    }
    out = result;
}

Vec4 mat4_transform(const Mat4& matrix, Vec4 vector)
{
    return {
        matrix.m[0] * vector.x + matrix.m[4] * vector.y +
            matrix.m[8] * vector.z + matrix.m[12] * vector.w,
        matrix.m[1] * vector.x + matrix.m[5] * vector.y +
            matrix.m[9] * vector.z + matrix.m[13] * vector.w,
        matrix.m[2] * vector.x + matrix.m[6] * vector.y +
            matrix.m[10] * vector.z + matrix.m[14] * vector.w,
        matrix.m[3] * vector.x + matrix.m[7] * vector.y +
            matrix.m[11] * vector.z + matrix.m[15] * vector.w
    };
}

bool mat4_perspective(
    Mat4& out,
    float vertical_fov_radians,
    float aspect,
    float near_plane,
    float far_plane)
{
    mat4_identity(out);
    if (!std::isfinite(vertical_fov_radians) ||
        !std::isfinite(aspect) ||
        !std::isfinite(near_plane) ||
        !std::isfinite(far_plane) ||
        vertical_fov_radians <= 0.0f ||
        vertical_fov_radians >= 3.14159265358979323846f ||
        aspect <= 0.0f || near_plane <= 0.0f ||
        far_plane <= near_plane)
    {
        std::memset(out.m, 0, sizeof(out.m));
        return false;
    }

    const float tangent = std::tan(vertical_fov_radians * 0.5f);
    if (!std::isfinite(tangent) || tangent <= 0.0f)
    {
        std::memset(out.m, 0, sizeof(out.m));
        return false;
    }

    std::memset(out.m, 0, sizeof(out.m));
    const float focal = 1.0f / tangent;
    out.m[0] = focal / aspect;
    out.m[5] = focal;
    out.m[10] = -(far_plane + near_plane) / (far_plane - near_plane);
    out.m[11] = -1.0f;
    out.m[14] = -(2.0f * far_plane * near_plane) /
                (far_plane - near_plane);
    return true;
}

bool clip_triangle(
    const ClipVertex input[3],
    size_t varying_count,
    ClipVertex output[MAX_CLIPPED_POLYGON_VERTICES],
    size_t& output_count)
{
    output_count = 0;
    if (!input || !output || varying_count > MAX_VARYINGS)
        return false;

    ClipVertex first[MAX_CLIPPED_POLYGON_VERTICES] = {};
    ClipVertex second[MAX_CLIPPED_POLYGON_VERTICES] = {};
    size_t count = 3;

    for (size_t i = 0; i < 3; ++i)
    {
        if (!finite_vertex(input[i], varying_count))
            return false;
        first[i] = input[i];
    }

    ClipVertex *source = first;
    ClipVertex *destination = second;

    for (unsigned plane = 0; plane < 6; ++plane)
    {
        if (count == 0)
            break;

        size_t next_count = 0;
        ClipVertex previous = source[count - 1];
        float previous_distance = plane_distance(previous.position, plane);
        bool previous_inside = previous_distance >= 0.0f;

        for (size_t i = 0; i < count; ++i)
        {
            const ClipVertex current = source[i];
            const float current_distance = plane_distance(current.position, plane);
            const bool current_inside = current_distance >= 0.0f;

            if (previous_inside != current_inside)
            {
                const float denominator = previous_distance - current_distance;
                if (denominator == 0.0f || !std::isfinite(denominator))
                    return false;
                const float t = previous_distance / denominator;
                if (next_count >= MAX_CLIPPED_POLYGON_VERTICES)
                    return false;
                destination[next_count++] =
                    interpolate(previous, current, t, varying_count);
            }

            if (current_inside)
            {
                if (next_count >= MAX_CLIPPED_POLYGON_VERTICES)
                    return false;
                destination[next_count++] = current;
            }

            previous = current;
            previous_distance = current_distance;
            previous_inside = current_inside;
        }

        count = next_count;
        ClipVertex *swap = source;
        source = destination;
        destination = swap;
    }

    output_count = count;
    for (size_t i = 0; i < count; ++i)
        output[i] = source[i];
    return true;
}

} // namespace fxgl
