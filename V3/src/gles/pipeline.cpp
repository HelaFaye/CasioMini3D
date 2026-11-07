#include "pipeline.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace fxgl
{
namespace
{
struct ScreenVertex
{
    float x;
    float y;
    float inverse_w;
    float z_over_w;
    float varying_over_w[MAX_VARYINGS];
};

static bool valid_compare_function(GLenum function)
{
    return function == GL_NEVER || function == GL_LESS ||
           function == GL_EQUAL || function == GL_LEQUAL ||
           function == GL_GREATER || function == GL_NOTEQUAL ||
           function == GL_GEQUAL || function == GL_ALWAYS;
}

static bool compare_values(GLenum function, uint32_t reference, uint32_t value)
{
    switch (function)
    {
        case GL_NEVER: return false;
        case GL_LESS: return reference < value;
        case GL_EQUAL: return reference == value;
        case GL_LEQUAL: return reference <= value;
        case GL_GREATER: return reference > value;
        case GL_NOTEQUAL: return reference != value;
        case GL_GEQUAL: return reference >= value;
        case GL_ALWAYS: return true;
        default: return false;
    }
}

static bool valid_blend_factor(GLenum factor)
{
    return factor == GL_ZERO || factor == GL_ONE ||
           factor == GL_SRC_COLOR || factor == GL_ONE_MINUS_SRC_COLOR ||
           factor == GL_SRC_ALPHA || factor == GL_ONE_MINUS_SRC_ALPHA ||
           factor == GL_DST_ALPHA || factor == GL_ONE_MINUS_DST_ALPHA ||
           factor == GL_DST_COLOR || factor == GL_ONE_MINUS_DST_COLOR ||
           factor == GL_SRC_ALPHA_SATURATE ||
           factor == GL_CONSTANT_COLOR ||
           factor == GL_ONE_MINUS_CONSTANT_COLOR ||
           factor == GL_CONSTANT_ALPHA ||
           factor == GL_ONE_MINUS_CONSTANT_ALPHA;
}

static bool valid_stencil_operation(GLenum operation)
{
    return operation == GL_ZERO || operation == GL_KEEP ||
           operation == GL_REPLACE || operation == GL_INCR ||
           operation == GL_DECR || operation == GL_INVERT ||
           operation == GL_INCR_WRAP || operation == GL_DECR_WRAP;
}

static bool valid_face_state(const StencilFaceState& face)
{
    return valid_compare_function(face.function) &&
           valid_stencil_operation(face.stencil_fail) &&
           valid_stencil_operation(face.depth_fail) &&
           valid_stencil_operation(face.depth_pass);
}

static bool valid_state(const PipelineState& state)
{
    const bool cull_face_ok =
        state.cull_face == GL_FRONT || state.cull_face == GL_BACK ||
        state.cull_face == GL_FRONT_AND_BACK;
    const bool front_face_ok =
        state.front_face == GL_CW || state.front_face == GL_CCW;
    const bool blend_equation_ok =
        (state.blend_equation_rgb == GL_FUNC_ADD ||
         state.blend_equation_rgb == GL_FUNC_SUBTRACT ||
         state.blend_equation_rgb == GL_FUNC_REVERSE_SUBTRACT) &&
        (state.blend_equation_alpha == GL_FUNC_ADD ||
         state.blend_equation_alpha == GL_FUNC_SUBTRACT ||
         state.blend_equation_alpha == GL_FUNC_REVERSE_SUBTRACT);

    if (!valid_compare_function(state.depth_function) ||
        !cull_face_ok || !front_face_ok || !blend_equation_ok ||
        !valid_face_state(state.stencil_front) ||
        !valid_face_state(state.stencil_back) ||
        !valid_blend_factor(state.blend_src_rgb) ||
        !valid_blend_factor(state.blend_dst_rgb) ||
        !valid_blend_factor(state.blend_src_alpha) ||
        !valid_blend_factor(state.blend_dst_alpha))
        return false;

    if (state.viewport[2] < 0 || state.viewport[3] < 0 ||
        state.scissor[2] < 0 || state.scissor[3] < 0 ||
        !std::isfinite(state.depth_near) ||
        !std::isfinite(state.depth_far))
        return false;

    return true;
}

static bool valid_surface(const RenderSurface& surface)
{
    return surface.color && surface.width > 0 && surface.height > 0 &&
           surface.stride >= surface.width;
}

static float edge(
    const ScreenVertex& a,
    const ScreenVertex& b,
    float x,
    float y)
{
    return (b.x - a.x) * (y - a.y) -
           (b.y - a.y) * (x - a.x);
}

static bool top_left_edge(const ScreenVertex& a, const ScreenVertex& b)
{
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    return dy > 0.0f || (dy == 0.0f && dx < 0.0f);
}

static bool covered(float value, bool top_left)
{
    return value > 0.0f || (value == 0.0f && top_left);
}

static bool to_screen(
    const ClipVertex& input,
    size_t varying_count,
    const PipelineState& state,
    ScreenVertex& output)
{
    const float w = input.position.w;
    if (!std::isfinite(w) || w <= 0.0f)
        return false;

    const float inverse_w = 1.0f / w;
    const float ndc_x = input.position.x * inverse_w;
    const float ndc_y = input.position.y * inverse_w;
    const float ndc_z = input.position.z * inverse_w;

    output.x = static_cast<float>(state.viewport[0]) +
        (ndc_x + 1.0f) * static_cast<float>(state.viewport[2]) * 0.5f;
    output.y = static_cast<float>(state.viewport[1]) +
        (ndc_y + 1.0f) * static_cast<float>(state.viewport[3]) * 0.5f;
    output.inverse_w = inverse_w;
    output.z_over_w = ndc_z;

    if (!std::isfinite(output.x) || !std::isfinite(output.y) ||
        !std::isfinite(output.z_over_w))
        return false;

    for (size_t i = 0; i < varying_count; ++i)
        output.varying_over_w[i] = input.varying[i] * inverse_w;
    return true;
}

static uint8_t apply_stencil_operation(
    GLenum operation,
    uint8_t current,
    GLint reference)
{
    switch (operation)
    {
        case GL_ZERO: return 0;
        case GL_KEEP: return current;
        case GL_REPLACE: return static_cast<uint8_t>(reference);
        case GL_INCR: return current == 0xff ? 0xff : current + 1;
        case GL_DECR: return current == 0 ? 0 : current - 1;
        case GL_INVERT: return static_cast<uint8_t>(~current);
        case GL_INCR_WRAP: return static_cast<uint8_t>(current + 1);
        case GL_DECR_WRAP: return static_cast<uint8_t>(current - 1);
        default: return current;
    }
}

static void write_stencil(
    uint8_t& destination,
    const StencilFaceState& face,
    GLenum operation)
{
    const uint8_t changed = apply_stencil_operation(
        operation, destination, face.reference);
    const uint8_t mask = static_cast<uint8_t>(face.write_mask);
    destination = static_cast<uint8_t>(
        (destination & static_cast<uint8_t>(~mask)) | (changed & mask));
}

static float clamp01(float value)
{
    if (!std::isfinite(value))
        return 0.0f;
    return std::max(0.0f, std::min(1.0f, value));
}

static float blend_factor(
    GLenum factor,
    unsigned channel,
    const float source[4],
    const float destination[4],
    const float constant[4])
{
    switch (factor)
    {
        case GL_ZERO: return 0.0f;
        case GL_ONE: return 1.0f;
        case GL_SRC_COLOR: return source[channel];
        case GL_ONE_MINUS_SRC_COLOR: return 1.0f - source[channel];
        case GL_SRC_ALPHA: return source[3];
        case GL_ONE_MINUS_SRC_ALPHA: return 1.0f - source[3];
        case GL_DST_ALPHA: return destination[3];
        case GL_ONE_MINUS_DST_ALPHA: return 1.0f - destination[3];
        case GL_DST_COLOR: return destination[channel];
        case GL_ONE_MINUS_DST_COLOR: return 1.0f - destination[channel];
        case GL_CONSTANT_COLOR: return constant[channel];
        case GL_ONE_MINUS_CONSTANT_COLOR: return 1.0f - constant[channel];
        case GL_CONSTANT_ALPHA: return constant[3];
        case GL_ONE_MINUS_CONSTANT_ALPHA: return 1.0f - constant[3];
        case GL_SRC_ALPHA_SATURATE:
            return channel == 3 ? 1.0f :
                std::min(source[3], 1.0f - destination[3]);
        default: return 0.0f;
    }
}

static float blend_channel(
    GLenum equation,
    float source,
    float destination,
    float source_factor,
    float destination_factor)
{
    const float a = source * source_factor;
    const float b = destination * destination_factor;
    switch (equation)
    {
        case GL_FUNC_ADD: return a + b;
        case GL_FUNC_SUBTRACT: return a - b;
        case GL_FUNC_REVERSE_SUBTRACT: return b - a;
        default: return source;
    }
}

static void unpack_rgb565(uint16_t pixel, float rgba[4])
{
    rgba[0] = static_cast<float>((pixel >> 11) & 0x1f) / 31.0f;
    rgba[1] = static_cast<float>((pixel >> 5) & 0x3f) / 63.0f;
    rgba[2] = static_cast<float>(pixel & 0x1f) / 31.0f;
    rgba[3] = 1.0f;
}

static uint16_t pack_rgb565(const float rgba[4])
{
    const uint16_t r = static_cast<uint16_t>(clamp01(rgba[0]) * 31.0f + 0.5f);
    const uint16_t g = static_cast<uint16_t>(clamp01(rgba[1]) * 63.0f + 0.5f);
    const uint16_t b = static_cast<uint16_t>(clamp01(rgba[2]) * 31.0f + 0.5f);
    return static_cast<uint16_t>((r << 11) | (g << 5) | b);
}

static void blend_pixel(
    uint16_t& destination_pixel,
    const float source[4],
    const PipelineState& state)
{
    float destination[4] = {};
    unpack_rgb565(destination_pixel, destination);

    float output[4] = {};
    for (unsigned channel = 0; channel < 4; ++channel)
    {
        const bool alpha_channel = channel == 3;
        const GLenum source_factor = alpha_channel
            ? state.blend_src_alpha : state.blend_src_rgb;
        const GLenum destination_factor = alpha_channel
            ? state.blend_dst_alpha : state.blend_dst_rgb;
        const GLenum equation = alpha_channel
            ? state.blend_equation_alpha : state.blend_equation_rgb;
        const float sf = blend_factor(
            source_factor, channel, source, destination, state.blend_color);
        const float df = blend_factor(
            destination_factor, channel, source, destination, state.blend_color);
        output[channel] = state.color_write[channel]
            ? clamp01(blend_channel(
                equation, source[channel], destination[channel], sf, df))
            : destination[channel];
    }
    destination_pixel = pack_rgb565(output);
}

static bool passes_scissor(
    int x,
    int y,
    const PipelineState& state)
{
    if (!state.scissor_enabled)
        return true;
    const int64_t right =
        static_cast<int64_t>(state.scissor[0]) + state.scissor[2];
    const int64_t top =
        static_cast<int64_t>(state.scissor[1]) + state.scissor[3];
    return x >= state.scissor[0] && y >= state.scissor[1] &&
           static_cast<int64_t>(x) < right &&
           static_cast<int64_t>(y) < top;
}

static bool process_fragment(
    const RenderSurface& surface,
    const PipelineState& state,
    int x,
    int y,
    const float *varyings,
    size_t varying_count,
    float ndc_depth,
    bool front,
    FragmentShader fragment_shader,
    void *user_data,
    RasterStats& stats)
{
    if (!passes_scissor(x, y, state))
        return false;

    ++stats.tested_fragments;
    float source[4] = {};
    if (!fragment_shader(varyings, varying_count, source, user_data))
        return false;
    ++stats.shaded_fragments;

    for (float& component : source)
        component = clamp01(component);

    const float mapped_depth = clamp01(
        state.depth_near + (ndc_depth * 0.5f + 0.5f) *
        (state.depth_far - state.depth_near));
    const uint16_t incoming_depth = static_cast<uint16_t>(
        mapped_depth * 65535.0f + 0.5f);
    const size_t offset = static_cast<size_t>(y) * surface.stride +
                          static_cast<size_t>(x);

    const StencilFaceState& stencil_face = front
        ? state.stencil_front : state.stencil_back;
    if (state.stencil_enabled && surface.stencil)
    {
        const uint8_t stored = surface.stencil[offset];
        const uint32_t masked_reference =
            static_cast<uint32_t>(stencil_face.reference) &
            stencil_face.value_mask;
        const uint32_t masked_stored =
            static_cast<uint32_t>(stored) & stencil_face.value_mask;
        const bool stencil_pass = compare_values(
            stencil_face.function, masked_reference, masked_stored);
        if (!stencil_pass)
        {
            write_stencil(surface.stencil[offset], stencil_face,
                          stencil_face.stencil_fail);
            return false;
        }
    }

    const bool depth_pass = !state.depth_enabled || !surface.depth ||
        compare_values(state.depth_function, incoming_depth, surface.depth[offset]);
    if (!depth_pass)
    {
        if (state.stencil_enabled && surface.stencil)
            write_stencil(surface.stencil[offset], stencil_face,
                          stencil_face.depth_fail);
        return false;
    }

    ++stats.depth_passed_fragments;
    if (state.stencil_enabled && surface.stencil)
        write_stencil(surface.stencil[offset], stencil_face,
                      stencil_face.depth_pass);
    if (state.depth_enabled && surface.depth && state.depth_write)
        surface.depth[offset] = incoming_depth;

    if (state.blend_enabled)
    {
        blend_pixel(surface.color[offset], source, state);
    }
    else if (state.color_write[0] && state.color_write[1] &&
             state.color_write[2])
    {
        surface.color[offset] = pack_rgb565(source);
    }
    else
    {
        float destination[4] = {};
        unpack_rgb565(surface.color[offset], destination);
        for (unsigned channel = 0; channel < 3; ++channel)
            if (state.color_write[channel])
                destination[channel] = source[channel];
        surface.color[offset] = pack_rgb565(destination);
    }

    ++stats.written_fragments;
    return true;
}

static bool rasterize_one(
    const RenderSurface& surface,
    const PipelineState& state,
    const ClipVertex input[3],
    size_t varying_count,
    FragmentShader fragment_shader,
    void *user_data,
    RasterStats& stats)
{
    ScreenVertex vertices[3] = {};
    for (size_t i = 0; i < 3; ++i)
    {
        if (!to_screen(input[i], varying_count, state, vertices[i]))
            return false;
    }

    float area = edge(vertices[0], vertices[1],
                      vertices[2].x, vertices[2].y);
    if (!std::isfinite(area) || area == 0.0f)
        return false;

    const bool ccw = area > 0.0f;
    const bool front = state.front_face == GL_CCW ? ccw : !ccw;
    if (state.cull_enabled &&
        (state.cull_face == GL_FRONT_AND_BACK ||
         (state.cull_face == GL_FRONT && front) ||
         (state.cull_face == GL_BACK && !front)))
        return false;

    if (area < 0.0f)
    {
        std::swap(vertices[1], vertices[2]);
        area = -area;
    }

    const float min_x = std::min(vertices[0].x,
        std::min(vertices[1].x, vertices[2].x));
    const float max_x = std::max(vertices[0].x,
        std::max(vertices[1].x, vertices[2].x));
    const float min_y = std::min(vertices[0].y,
        std::min(vertices[1].y, vertices[2].y));
    const float max_y = std::max(vertices[0].y,
        std::max(vertices[1].y, vertices[2].y));

    int64_t x0 = static_cast<int64_t>(std::ceil(min_x - 0.5f));
    int64_t x1 = static_cast<int64_t>(std::floor(max_x - 0.5f));
    int64_t y0 = static_cast<int64_t>(std::ceil(min_y - 0.5f));
    int64_t y1 = static_cast<int64_t>(std::floor(max_y - 0.5f));

    x0 = std::max<int64_t>(x0, 0);
    y0 = std::max<int64_t>(y0, 0);
    x1 = std::min<int64_t>(x1, surface.width - 1);
    y1 = std::min<int64_t>(y1, surface.height - 1);
    x0 = std::max<int64_t>(x0, state.viewport[0]);
    y0 = std::max<int64_t>(y0, state.viewport[1]);
    x1 = std::min<int64_t>(x1,
        static_cast<int64_t>(state.viewport[0]) + state.viewport[2] - 1);
    y1 = std::min<int64_t>(y1,
        static_cast<int64_t>(state.viewport[1]) + state.viewport[3] - 1);
    if (state.scissor_enabled)
    {
        x0 = std::max<int64_t>(x0, state.scissor[0]);
        y0 = std::max<int64_t>(y0, state.scissor[1]);
        x1 = std::min<int64_t>(x1,
            static_cast<int64_t>(state.scissor[0]) + state.scissor[2] - 1);
        y1 = std::min<int64_t>(y1,
            static_cast<int64_t>(state.scissor[1]) + state.scissor[3] - 1);
    }
    if (x0 > x1 || y0 > y1)
        return false;

    const bool top_left_0 = top_left_edge(vertices[1], vertices[2]);
    const bool top_left_1 = top_left_edge(vertices[2], vertices[0]);
    const bool top_left_2 = top_left_edge(vertices[0], vertices[1]);

    const StencilFaceState& stencil_face = front
        ? state.stencil_front : state.stencil_back;

    bool wrote_any = false;
    float varyings[MAX_VARYINGS] = {};
    for (int64_t y = y0; y <= y1; ++y)
    {
        for (int64_t x = x0; x <= x1; ++x)
        {
            const float px = static_cast<float>(x) + 0.5f;
            const float py = static_cast<float>(y) + 0.5f;
            const float e0 = edge(vertices[1], vertices[2], px, py);
            const float e1 = edge(vertices[2], vertices[0], px, py);
            const float e2 = edge(vertices[0], vertices[1], px, py);
            if (!covered(e0, top_left_0) ||
                !covered(e1, top_left_1) ||
                !covered(e2, top_left_2))
                continue;
            if (!passes_scissor(static_cast<int>(x), static_cast<int>(y), state))
                continue;

            const float w0 = e0 / area;
            const float w1 = e1 / area;
            const float w2 = e2 / area;

            const float reciprocal_w =
                w0 * vertices[0].inverse_w +
                w1 * vertices[1].inverse_w +
                w2 * vertices[2].inverse_w;
            if (!std::isfinite(reciprocal_w) || reciprocal_w <= 0.0f)
                continue;

            for (size_t i = 0; i < varying_count; ++i)
            {
                const float interpolated =
                    w0 * vertices[0].varying_over_w[i] +
                    w1 * vertices[1].varying_over_w[i] +
                    w2 * vertices[2].varying_over_w[i];
                varyings[i] = interpolated / reciprocal_w;
            }

            const float ndc_depth =
                w0 * vertices[0].z_over_w +
                w1 * vertices[1].z_over_w +
                w2 * vertices[2].z_over_w;
            if (process_fragment(surface, state,
                    static_cast<int>(x), static_cast<int>(y),
                    varyings, varying_count, ndc_depth, front,
                    fragment_shader, user_data, stats))
                wrote_any = true;
        }
    }

    return wrote_any;
}

static float clip_plane_distance(const Vec4& p, unsigned plane)
{
    switch (plane)
    {
        case 0: return p.x + p.w;
        case 1: return p.w - p.x;
        case 2: return p.y + p.w;
        case 3: return p.w - p.y;
        case 4: return p.z + p.w;
        default: return p.w - p.z;
    }
}

static ClipVertex interpolate_clip_vertex(
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

static bool clip_point(const ClipVertex& point)
{
    for (unsigned plane = 0; plane < 6; ++plane)
        if (clip_plane_distance(point.position, plane) < 0.0f)
            return false;
    return true;
}

static bool clip_line_segment(
    const ClipVertex input[2],
    size_t varying_count,
    ClipVertex output[2])
{
    float enter = 0.0f;
    float leave = 1.0f;

    for (unsigned plane = 0; plane < 6; ++plane)
    {
        const float a = clip_plane_distance(input[0].position, plane);
        const float b = clip_plane_distance(input[1].position, plane);
        if (!std::isfinite(a) || !std::isfinite(b))
            return false;
        if (a < 0.0f && b < 0.0f)
            return false;
        if (a < 0.0f || b < 0.0f)
        {
            const float denominator = a - b;
            if (denominator == 0.0f)
                return false;
            const float t = a / denominator;
            if (a < 0.0f)
                enter = std::max(enter, t);
            else
                leave = std::min(leave, t);
            if (enter > leave)
                return false;
        }
    }

    output[0] = interpolate_clip_vertex(input[0], input[1], enter, varying_count);
    output[1] = interpolate_clip_vertex(input[0], input[1], leave, varying_count);
    return true;
}

/* Clip screen-space line parameters to a bounded integer pixel rectangle.
   This ensures raster steps are at most max(surface.width,height), even if a
   caller supplies a very large viewport partially overlapping the surface. */
static bool clip_screen_line(
    const ScreenVertex& a,
    const ScreenVertex& b,
    float minimum_x,
    float minimum_y,
    float maximum_x,
    float maximum_y,
    float& enter,
    float& leave)
{
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    enter = 0.0f;
    leave = 1.0f;

    const float p[4] = {-dx, dx, -dy, dy};
    const float q[4] = {
        a.x - minimum_x,
        maximum_x - a.x,
        a.y - minimum_y,
        maximum_y - a.y
    };

    for (int i = 0; i < 4; ++i)
    {
        if (p[i] == 0.0f)
        {
            if (q[i] < 0.0f)
                return false;
            continue;
        }

        const float t = q[i] / p[i];
        if (p[i] < 0.0f)
            enter = std::max(enter, t);
        else
            leave = std::min(leave, t);
        if (enter > leave)
            return false;
    }
    return true;
}

} // namespace

void pipeline_default_state(PipelineState& state, GLsizei width, GLsizei height)
{
    state = {};
    state.viewport[2] = width;
    state.viewport[3] = height;
    state.scissor[2] = width;
    state.scissor[3] = height;
    state.cull_face = GL_BACK;
    state.front_face = GL_CCW;
    state.depth_function = GL_LESS;
    state.depth_write = true;
    state.depth_near = 0.0f;
    state.depth_far = 1.0f;
    state.stencil_front = {
        GL_ALWAYS, 0, 0xff, 0xff, GL_KEEP, GL_KEEP, GL_KEEP
    };
    state.stencil_back = state.stencil_front;
    state.blend_src_rgb = GL_ONE;
    state.blend_dst_rgb = GL_ZERO;
    state.blend_src_alpha = GL_ONE;
    state.blend_dst_alpha = GL_ZERO;
    state.blend_equation_rgb = GL_FUNC_ADD;
    state.blend_equation_alpha = GL_FUNC_ADD;
    state.color_write[0] = true;
    state.color_write[1] = true;
    state.color_write[2] = true;
    state.color_write[3] = true;
}

bool draw_triangle(
    const RenderSurface& surface,
    const PipelineState& state,
    const ClipVertex input[3],
    size_t varying_count,
    FragmentShader fragment_shader,
    void *user_data,
    RasterStats *stats_out)
{
    RasterStats local_stats = {};
    RasterStats& stats = stats_out ? *stats_out : local_stats;
    stats = {};

    if (!valid_surface(surface) || !valid_state(state) ||
        !input || !fragment_shader || varying_count > MAX_VARYINGS)
        return false;

    ++stats.input_triangles;
    ClipVertex clipped[MAX_CLIPPED_POLYGON_VERTICES] = {};
    size_t clipped_count = 0;
    if (!clip_triangle(input, varying_count, clipped, clipped_count))
        return false;
    if (clipped_count < 3)
        return true;

    ++stats.clipped_polygons;
    bool wrote_any = false;
    for (size_t i = 1; i + 1 < clipped_count; ++i)
    {
        ClipVertex fan[3] = {clipped[0], clipped[i], clipped[i + 1]};
        ++stats.generated_triangles;
        if (rasterize_one(surface, state, fan, varying_count,
                          fragment_shader, user_data, stats))
            wrote_any = true;
    }
    return wrote_any;
}

bool draw_point(
    const RenderSurface& surface,
    const PipelineState& state,
    const ClipVertex& input,
    size_t varying_count,
    FragmentShader fragment_shader,
    void *user_data,
    RasterStats *stats_out)
{
    RasterStats local_stats = {};
    RasterStats& stats = stats_out ? *stats_out : local_stats;
    stats = {};

    if (!valid_surface(surface) || !valid_state(state) ||
        !fragment_shader || varying_count > MAX_VARYINGS)
        return false;

    ++stats.input_points;
    for (size_t i = 0; i < varying_count; ++i)
        if (!std::isfinite(input.varying[i]))
            return false;
    if (!clip_point(input))
        return false;

    ScreenVertex screen = {};
    if (!to_screen(input, varying_count, state, screen))
        return false;

    const float min_x = static_cast<float>(std::max(0, state.viewport[0]));
    const float min_y = static_cast<float>(std::max(0, state.viewport[1]));
    const float max_x = static_cast<float>(std::min(
        surface.width, static_cast<int64_t>(state.viewport[0]) +
        state.viewport[2]));
    const float max_y = static_cast<float>(std::min(
        surface.height, static_cast<int64_t>(state.viewport[1]) +
        state.viewport[3]));
    if (state.scissor_enabled)
    {
        if (min_x >= max_x || min_y >= max_y)
            return false;
        if (screen.x < std::max(min_x, static_cast<float>(state.scissor[0])) ||
            screen.y < std::max(min_y, static_cast<float>(state.scissor[1])) ||
            screen.x >= std::min(max_x,
                static_cast<float>(static_cast<int64_t>(state.scissor[0]) +
                                   state.scissor[2])) ||
            screen.y >= std::min(max_y,
                static_cast<float>(static_cast<int64_t>(state.scissor[1]) +
                                   state.scissor[3])))
            return false;
    }

    if (screen.x < min_x || screen.y < min_y ||
        screen.x >= max_x || screen.y >= max_y)
        return false;

    const int x = static_cast<int>(std::floor(screen.x));
    const int y = static_cast<int>(std::floor(screen.y));
    float varyings[MAX_VARYINGS] = {};
    for (size_t i = 0; i < varying_count; ++i)
        varyings[i] = input.varying[i];

    return process_fragment(surface, state, x, y, varyings,
        varying_count, screen.z_over_w, true,
        fragment_shader, user_data, stats);
}

bool draw_line(
    const RenderSurface& surface,
    const PipelineState& state,
    const ClipVertex input[2],
    size_t varying_count,
    FragmentShader fragment_shader,
    void *user_data,
    RasterStats *stats_out)
{
    RasterStats local_stats = {};
    RasterStats& stats = stats_out ? *stats_out : local_stats;
    stats = {};

    if (!valid_surface(surface) || !valid_state(state) ||
        !input || !fragment_shader || varying_count > MAX_VARYINGS)
        return false;

    ++stats.input_lines;
    for (size_t endpoint = 0; endpoint < 2; ++endpoint)
    {
        const Vec4& p = input[endpoint].position;
        if (!std::isfinite(p.x) || !std::isfinite(p.y) ||
            !std::isfinite(p.z) || !std::isfinite(p.w))
            return false;
        for (size_t i = 0; i < varying_count; ++i)
            if (!std::isfinite(input[endpoint].varying[i]))
                return false;
    }

    ClipVertex clipped[2] = {};
    if (!clip_line_segment(input, varying_count, clipped))
        return false;

    ScreenVertex a = {};
    ScreenVertex b = {};
    if (!to_screen(clipped[0], varying_count, state, a) ||
        !to_screen(clipped[1], varying_count, state, b))
        return false;

    int64_t left = std::max<int64_t>(0, state.viewport[0]);
    int64_t bottom = std::max<int64_t>(0, state.viewport[1]);
    int64_t right = std::min<int64_t>(
        surface.width,
        static_cast<int64_t>(state.viewport[0]) + state.viewport[2]);
    int64_t top = std::min<int64_t>(
        surface.height,
        static_cast<int64_t>(state.viewport[1]) + state.viewport[3]);

    if (state.scissor_enabled)
    {
        left = std::max<int64_t>(left, state.scissor[0]);
        bottom = std::max<int64_t>(bottom, state.scissor[1]);
        right = std::min<int64_t>(
            right, static_cast<int64_t>(state.scissor[0]) + state.scissor[2]);
        top = std::min<int64_t>(
            top, static_cast<int64_t>(state.scissor[1]) + state.scissor[3]);
    }
    if (left >= right || bottom >= top)
        return false;

    float enter = 0.0f;
    float leave = 1.0f;
    if (!clip_screen_line(a, b,
            static_cast<float>(left), static_cast<float>(bottom),
            static_cast<float>(right) - 0.0001f,
            static_cast<float>(top) - 0.0001f, enter, leave))
        return false;

    const float original_dx = b.x - a.x;
    const float original_dy = b.y - a.y;
    const ScreenVertex original_a = a;
    const ScreenVertex original_b = b;

    a.x = original_a.x + original_dx * enter;
    a.y = original_a.y + original_dy * enter;
    a.inverse_w = original_a.inverse_w +
        (original_b.inverse_w - original_a.inverse_w) * enter;
    a.z_over_w = original_a.z_over_w +
        (original_b.z_over_w - original_a.z_over_w) * enter;
    for (size_t i = 0; i < varying_count; ++i)
        a.varying_over_w[i] = original_a.varying_over_w[i] +
            (original_b.varying_over_w[i] - original_a.varying_over_w[i]) * enter;

    b.x = original_a.x + original_dx * leave;
    b.y = original_a.y + original_dy * leave;
    b.inverse_w = original_a.inverse_w +
        (original_b.inverse_w - original_a.inverse_w) * leave;
    b.z_over_w = original_a.z_over_w +
        (original_b.z_over_w - original_a.z_over_w) * leave;
    for (size_t i = 0; i < varying_count; ++i)
        b.varying_over_w[i] = original_a.varying_over_w[i] +
            (original_b.varying_over_w[i] - original_a.varying_over_w[i]) * leave;

    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const int steps = static_cast<int>(
        std::ceil(std::max(std::fabs(dx), std::fabs(dy))));
    const int total_steps = std::max(steps, 1);

    bool wrote_any = false;
    int previous_x = std::numeric_limits<int>::min();
    int previous_y = std::numeric_limits<int>::min();
    float varyings[MAX_VARYINGS] = {};

    for (int step = 0; step <= total_steps; ++step)
    {
        const float t = static_cast<float>(step) / total_steps;
        const float screen_x = a.x + dx * t;
        const float screen_y = a.y + dy * t;
        const int x = static_cast<int>(std::floor(screen_x));
        const int y = static_cast<int>(std::floor(screen_y));
        if (x == previous_x && y == previous_y)
            continue;
        previous_x = x;
        previous_y = y;

        const float reciprocal_w =
            a.inverse_w + (b.inverse_w - a.inverse_w) * t;
        if (!std::isfinite(reciprocal_w) || reciprocal_w <= 0.0f)
            continue;
        for (size_t i = 0; i < varying_count; ++i)
        {
            const float interpolated = a.varying_over_w[i] +
                (b.varying_over_w[i] - a.varying_over_w[i]) * t;
            varyings[i] = interpolated / reciprocal_w;
        }
        const float ndc_depth =
            a.z_over_w + (b.z_over_w - a.z_over_w) * t;

        if (process_fragment(surface, state, x, y,
                varyings, varying_count, ndc_depth, true,
                fragment_shader, user_data, stats))
            wrote_any = true;
    }
    return wrote_any;
}

} // namespace fxgl
