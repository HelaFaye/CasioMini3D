#include <GLES2/gl2.h>
#include <fxgl/platform.h>
#include "../gfx/texture.hpp"
#include "context_internal.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace
{
constexpr size_t FXGL_MAX_BUFFERS = 16;
constexpr size_t FXGL_MAX_BUFFER_BYTES = 128u * 1024u;
constexpr GLuint FXGL_MAX_VERTEX_ATTRIBS = 8;
constexpr size_t FXGL_MAX_TEXTURES = 8;
constexpr size_t FXGL_MAX_TEXTURE_UNITS = 2;
constexpr size_t FXGL_MAX_TEXTURE_BYTES = 64u * 1024u;
constexpr GLint FXGL_MAX_TEXTURE_SIZE = 128;

struct BufferObject
{
    uint8_t *data = nullptr;
    size_t size = 0;
    GLenum usage = GL_STATIC_DRAW;
    bool reserved = false;
    bool alive = false;
};

struct TextureObject
{
    uint16_t *pixels = nullptr;
    uint16_t width = 0;
    uint16_t height = 0;
    size_t bytes = 0;
    GLenum min_filter = GL_NEAREST;
    GLenum mag_filter = GL_NEAREST;
    GLenum wrap_s = GL_CLAMP_TO_EDGE;
    GLenum wrap_t = GL_CLAMP_TO_EDGE;
    bool reserved = false;
    bool alive = false;
};

struct VertexAttribute
{
    bool enabled = false;
    GLint size = 4;
    GLenum type = GL_FLOAT;
    bool normalized = false;
    GLsizei stride = 0;
    const GLvoid *pointer = nullptr;
    GLuint buffer_binding = 0;
};

struct Context
{
    bool current = false;
    fxgl_surface_t surface = {};

    BufferObject buffers[FXGL_MAX_BUFFERS] = {};
    size_t buffer_bytes = 0;
    GLuint array_buffer_binding = 0;
    GLuint element_buffer_binding = 0;
    VertexAttribute attributes[FXGL_MAX_VERTEX_ATTRIBS] = {};

    TextureObject textures[FXGL_MAX_TEXTURES] = {};
    size_t texture_bytes = 0;
    GLuint texture_binding_2d[FXGL_MAX_TEXTURE_UNITS] = {};
    size_t active_texture_unit = 0;
    GLint unpack_alignment = 4;
    GLint pack_alignment = 4;

    GLint viewport[4] = {};
    GLint scissor[4] = {};

    GLfloat clear_color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    GLclampf clear_depth = 1.0f;
    GLint clear_stencil = 0;

    bool blend = false;
    bool cull_face = false;
    bool depth_test = false;
    bool dither = true;
    bool scissor_test = false;
    bool stencil_test = false;

    GLenum depth_func = GL_LESS;
    bool depth_mask = true;
    GLenum cull_mode = GL_BACK;
    GLenum front_face = GL_CCW;
    GLenum blend_src = GL_ONE;
    GLenum blend_dst = GL_ZERO;
    GLenum blend_equation = GL_FUNC_ADD;
    GLboolean color_mask[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
    GLuint stencil_mask = 0xff;

    GLenum error = GL_NO_ERROR;
};

Context g_context;

static void set_error(GLenum error)
{
    if (g_context.current && g_context.error == GL_NO_ERROR)
        g_context.error = error;
}

static bool has_context()
{
    if (!g_context.current)
        return false;
    return true;
}

static BufferObject *buffer_slot(GLuint name)
{
    if (name == 0 || name > FXGL_MAX_BUFFERS)
        return nullptr;
    BufferObject& object = g_context.buffers[name - 1];
    return object.reserved ? &object : nullptr;
}

static BufferObject *find_buffer(GLuint name)
{
    BufferObject *object = buffer_slot(name);
    return object && object->alive ? object : nullptr;
}

static BufferObject *bound_buffer(GLenum target)
{
    GLuint name = 0;
    if (target == GL_ARRAY_BUFFER)
        name = g_context.array_buffer_binding;
    else if (target == GL_ELEMENT_ARRAY_BUFFER)
        name = g_context.element_buffer_binding;
    else
    {
        set_error(GL_INVALID_ENUM);
        return nullptr;
    }

    if (name == 0)
    {
        set_error(GL_INVALID_OPERATION);
        return nullptr;
    }

    BufferObject *object = find_buffer(name);
    if (!object)
        set_error(GL_INVALID_OPERATION);
    return object;
}

static TextureObject *texture_slot(GLuint name)
{
    if (name == 0 || name > FXGL_MAX_TEXTURES)
        return nullptr;
    TextureObject& object = g_context.textures[name - 1];
    return object.reserved ? &object : nullptr;
}

static TextureObject *find_texture(GLuint name)
{
    TextureObject *object = texture_slot(name);
    return object && object->alive ? object : nullptr;
}

static TextureObject *bound_texture(GLenum target)
{
    if (target != GL_TEXTURE_2D)
    {
        set_error(GL_INVALID_ENUM);
        return nullptr;
    }

    const GLuint name =
        g_context.texture_binding_2d[g_context.active_texture_unit];
    if (name == 0)
    {
        set_error(GL_INVALID_OPERATION);
        return nullptr;
    }

    TextureObject *object = find_texture(name);
    if (!object)
        set_error(GL_INVALID_OPERATION);
    return object;
}

static gfx::TextureSampler make_sampler(const TextureObject& object)
{
    gfx::TextureSampler sampler = {};
    sampler.filter = object.min_filter == GL_LINEAR
        ? gfx::TextureFilter::Bilinear : gfx::TextureFilter::Nearest;
    sampler.address_u = object.wrap_s == GL_REPEAT
        ? gfx::TextureAddressMode::Repeat :
        (object.wrap_s == GL_MIRRORED_REPEAT
            ? gfx::TextureAddressMode::MirroredRepeat
            : gfx::TextureAddressMode::Clamp);
    sampler.address_v = object.wrap_t == GL_REPEAT
        ? gfx::TextureAddressMode::Repeat :
        (object.wrap_t == GL_MIRRORED_REPEAT
            ? gfx::TextureAddressMode::MirroredRepeat
            : gfx::TextureAddressMode::Clamp);
    return sampler;
}

static void destroy_textures(Context& context)
{
    for (TextureObject& object : context.textures)
    {
        std::free(object.pixels);
        object = {};
    }
    context.texture_bytes = 0;
    for (GLuint& binding : context.texture_binding_2d)
        binding = 0;
    context.active_texture_unit = 0;
}

static bool valid_buffer_usage(GLenum usage)
{
    return usage == GL_STATIC_DRAW ||
           usage == GL_DYNAMIC_DRAW ||
           usage == GL_STREAM_DRAW;
}

static size_t buffer_size_budget_used_without(const BufferObject& object)
{
    return g_context.buffer_bytes - object.size;
}

static void destroy_buffers(Context& context)
{
    for (BufferObject& object : context.buffers)
    {
        std::free(object.data);
        object = {};
    }
    context.buffer_bytes = 0;
    context.array_buffer_binding = 0;
    context.element_buffer_binding = 0;
    for (VertexAttribute& attribute : context.attributes)
        attribute = {};
}

static bool valid_depth_func(GLenum value)
{
    switch (value)
    {
        case GL_NEVER:
        case GL_LESS:
        case GL_EQUAL:
        case GL_LEQUAL:
        case GL_GREATER:
        case GL_NOTEQUAL:
        case GL_GEQUAL:
        case GL_ALWAYS:
            return true;
        default:
            return false;
    }
}

static bool valid_blend_factor(GLenum value)
{
    switch (value)
    {
        case GL_ZERO:
        case GL_ONE:
        case GL_SRC_COLOR:
        case GL_ONE_MINUS_SRC_COLOR:
        case GL_SRC_ALPHA:
        case GL_ONE_MINUS_SRC_ALPHA:
        case GL_DST_ALPHA:
        case GL_ONE_MINUS_DST_ALPHA:
        case GL_DST_COLOR:
        case GL_ONE_MINUS_DST_COLOR:
        case GL_SRC_ALPHA_SATURATE:
        case GL_CONSTANT_COLOR:
        case GL_ONE_MINUS_CONSTANT_COLOR:
        case GL_CONSTANT_ALPHA:
        case GL_ONE_MINUS_CONSTANT_ALPHA:
            return true;
        default:
            return false;
    }
}

static bool valid_blend_equation(GLenum value)
{
    return value == GL_FUNC_ADD || value == GL_FUNC_SUBTRACT ||
           value == GL_FUNC_REVERSE_SUBTRACT;
}

static bool *capability(GLenum cap)
{
    switch (cap)
    {
        case GL_BLEND: return &g_context.blend;
        case GL_CULL_FACE: return &g_context.cull_face;
        case GL_DEPTH_TEST: return &g_context.depth_test;
        case GL_DITHER: return &g_context.dither;
        case GL_SCISSOR_TEST: return &g_context.scissor_test;
        case GL_STENCIL_TEST: return &g_context.stencil_test;
        default: return nullptr;
    }
}

static GLfloat clamp01(GLfloat value)
{
    if (!std::isfinite(value))
        return 0.0f;
    return std::max(0.0f, std::min(1.0f, value));
}

static uint16_t pack_rgb565(const GLfloat rgba[4])
{
    const uint16_t red = static_cast<uint16_t>(clamp01(rgba[0]) * 31.0f + 0.5f);
    const uint16_t green = static_cast<uint16_t>(clamp01(rgba[1]) * 63.0f + 0.5f);
    const uint16_t blue = static_cast<uint16_t>(clamp01(rgba[2]) * 31.0f + 0.5f);
    return static_cast<uint16_t>((red << 11) | (green << 5) | blue);
}

static void clear_rect(GLint rect[4], GLint bounds[4])
{
    const int64_t raw_x0 = rect[0];
    const int64_t raw_y0 = rect[1];
    const int64_t raw_x1 = raw_x0 + rect[2];
    const int64_t raw_y1 = raw_y0 + rect[3];

    const int64_t x0 = std::max<int64_t>(
        0, std::min<int64_t>(g_context.surface.width, raw_x0));
    const int64_t y0 = std::max<int64_t>(
        0, std::min<int64_t>(g_context.surface.height, raw_y0));
    const int64_t x1 = std::max<int64_t>(
        0, std::min<int64_t>(g_context.surface.width, raw_x1));
    const int64_t y1 = std::max<int64_t>(
        0, std::min<int64_t>(g_context.surface.height, raw_y1));

    bounds[0] = static_cast<GLint>(std::min(x0, x1));
    bounds[1] = static_cast<GLint>(std::min(y0, y1));
    bounds[2] = static_cast<GLint>(std::max(x0, x1));
    bounds[3] = static_cast<GLint>(std::max(y0, y1));
}

static void clear_color_buffer(GLint x0, GLint y0, GLint x1, GLint y1)
{
    const uint16_t clear_pixel = pack_rgb565(g_context.clear_color);
    const uint16_t red_mask = 0xf800;
    const uint16_t green_mask = 0x07e0;
    const uint16_t blue_mask = 0x001f;

    for (GLint y = y0; y < y1; ++y)
    {
        uint16_t *row = g_context.surface.color +
            static_cast<size_t>(y) * g_context.surface.stride;
        for (GLint x = x0; x < x1; ++x)
        {
            if (g_context.color_mask[0] && g_context.color_mask[1] &&
                g_context.color_mask[2])
            {
                row[x] = clear_pixel;
                continue;
            }

            uint16_t value = row[x];
            if (g_context.color_mask[0])
                value = static_cast<uint16_t>((value & ~red_mask) |
                                              (clear_pixel & red_mask));
            if (g_context.color_mask[1])
                value = static_cast<uint16_t>((value & ~green_mask) |
                                              (clear_pixel & green_mask));
            if (g_context.color_mask[2])
                value = static_cast<uint16_t>((value & ~blue_mask) |
                                              (clear_pixel & blue_mask));
            row[x] = value;
        }
    }
}
} // namespace

extern "C" {

GLboolean fxglMakeCurrent(const fxgl_surface_t *surface)
{
    if (!surface || !surface->color ||
        surface->width <= 0 || surface->height <= 0 ||
        surface->stride < surface->width)
        return GL_FALSE;

    if (g_context.current)
    {
        destroy_buffers(g_context);
        destroy_textures(g_context);
    }

    Context next = {};
    next.current = true;
    next.surface = *surface;
    next.viewport[2] = surface->width;
    next.viewport[3] = surface->height;
    next.scissor[2] = surface->width;
    next.scissor[3] = surface->height;
    next.clear_depth = 1.0f;
    next.dither = true;
    next.depth_func = GL_LESS;
    next.depth_mask = true;
    next.cull_mode = GL_BACK;
    next.front_face = GL_CCW;
    next.blend_src = GL_ONE;
    next.blend_dst = GL_ZERO;
    next.blend_equation = GL_FUNC_ADD;
    next.color_mask[0] = next.color_mask[1] =
        next.color_mask[2] = next.color_mask[3] = GL_TRUE;
    next.stencil_mask = 0xff;
    g_context = next;
    return GL_TRUE;
}

void fxglReleaseCurrent(void)
{
    destroy_buffers(g_context);
    destroy_textures(g_context);
    g_context = {};
}

GLboolean fxglHasCurrentContext(void)
{
    return g_context.current ? GL_TRUE : GL_FALSE;
}

GLboolean glIsBuffer(GLuint buffer)
{
    if (!has_context()) return GL_FALSE;
    return find_buffer(buffer) ? GL_TRUE : GL_FALSE;
}

void glGenBuffers(GLsizei n, GLuint *buffers)
{
    if (!has_context()) return;
    if (n < 0 || (n > 0 && !buffers))
    {
        set_error(GL_INVALID_VALUE);
        return;
    }

    size_t available = 0;
    for (const BufferObject& object : g_context.buffers)
        if (!object.reserved) ++available;
    if (available < static_cast<size_t>(n))
    {
        set_error(GL_OUT_OF_MEMORY);
        return;
    }

    GLsizei generated = 0;
    for (size_t i = 0; i < FXGL_MAX_BUFFERS && generated < n; ++i)
    {
        if (!g_context.buffers[i].reserved)
        {
            g_context.buffers[i] = {};
            g_context.buffers[i].reserved = true;
            buffers[generated++] = static_cast<GLuint>(i + 1);
        }
    }
}

void glDeleteBuffers(GLsizei n, const GLuint *buffers)
{
    if (!has_context()) return;
    if (n < 0 || (n > 0 && !buffers))
    {
        set_error(GL_INVALID_VALUE);
        return;
    }

    for (GLsizei i = 0; i < n; ++i)
    {
        BufferObject *object = buffer_slot(buffers[i]);
        if (!object)
            continue;

        const GLuint name = buffers[i];
        if (g_context.array_buffer_binding == name)
            g_context.array_buffer_binding = 0;
        if (g_context.element_buffer_binding == name)
            g_context.element_buffer_binding = 0;

        for (VertexAttribute& attribute : g_context.attributes)
        {
            if (attribute.buffer_binding == name)
            {
                attribute.buffer_binding = 0;
                attribute.pointer = nullptr;
            }
        }

        if (object->alive)
        {
            g_context.buffer_bytes -= object->size;
            std::free(object->data);
        }
        *object = {};
    }
}

void glBindBuffer(GLenum target, GLuint buffer)
{
    if (!has_context()) return;
    GLuint *binding = nullptr;
    if (target == GL_ARRAY_BUFFER)
        binding = &g_context.array_buffer_binding;
    else if (target == GL_ELEMENT_ARRAY_BUFFER)
        binding = &g_context.element_buffer_binding;
    else
    {
        set_error(GL_INVALID_ENUM);
        return;
    }

    if (buffer != 0)
    {
        BufferObject *object = buffer_slot(buffer);
        if (!object)
        {
            set_error(GL_INVALID_OPERATION);
            return;
        }
        object->alive = true;
    }
    *binding = buffer;
}

void glBufferData(GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage)
{
    if (!has_context()) return;
    if (size < 0)
    {
        set_error(GL_INVALID_VALUE);
        return;
    }
    if (!valid_buffer_usage(usage))
    {
        set_error(GL_INVALID_ENUM);
        return;
    }

    BufferObject *object = bound_buffer(target);
    if (!object) return;

    const size_t new_size = static_cast<size_t>(size);
    const size_t retained = buffer_size_budget_used_without(*object);
    if (new_size > FXGL_MAX_BUFFER_BYTES - retained)
    {
        set_error(GL_OUT_OF_MEMORY);
        return;
    }

    uint8_t *new_data = nullptr;
    if (new_size > 0)
    {
        new_data = static_cast<uint8_t *>(std::malloc(new_size));
        if (!new_data)
        {
            set_error(GL_OUT_OF_MEMORY);
            return;
        }
        if (data)
            std::memcpy(new_data, data, new_size);
        else
            std::memset(new_data, 0, new_size);
    }

    std::free(object->data);
    object->data = new_data;
    object->size = new_size;
    object->usage = usage;
    g_context.buffer_bytes = retained + new_size;
}

void glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid *data)
{
    if (!has_context()) return;
    if (offset < 0 || size < 0 || (size > 0 && !data))
    {
        set_error(GL_INVALID_VALUE);
        return;
    }

    BufferObject *object = bound_buffer(target);
    if (!object) return;

    const size_t start = static_cast<size_t>(offset);
    const size_t count = static_cast<size_t>(size);
    if (start > object->size || count > object->size - start)
    {
        set_error(GL_INVALID_VALUE);
        return;
    }
    if (count > 0)
        std::memcpy(object->data + start, data, count);
}

void glGetBufferParameteriv(GLenum target, GLenum pname, GLint *params)
{
    if (!has_context()) return;
    if (!params)
    {
        set_error(GL_INVALID_VALUE);
        return;
    }
    BufferObject *object = bound_buffer(target);
    if (!object) return;

    switch (pname)
    {
        case GL_BUFFER_SIZE:
            *params = static_cast<GLint>(object->size);
            break;
        case GL_BUFFER_USAGE:
            *params = static_cast<GLint>(object->usage);
            break;
        default:
            set_error(GL_INVALID_ENUM);
            break;
    }
}

void glEnableVertexAttribArray(GLuint index)
{
    if (!has_context()) return;
    if (index >= FXGL_MAX_VERTEX_ATTRIBS)
    {
        set_error(GL_INVALID_VALUE);
        return;
    }
    g_context.attributes[index].enabled = true;
}

void glDisableVertexAttribArray(GLuint index)
{
    if (!has_context()) return;
    if (index >= FXGL_MAX_VERTEX_ATTRIBS)
    {
        set_error(GL_INVALID_VALUE);
        return;
    }
    g_context.attributes[index].enabled = false;
}

static bool valid_vertex_attrib_type(GLenum type)
{
    return type == GL_BYTE || type == GL_UNSIGNED_BYTE ||
           type == GL_SHORT || type == GL_UNSIGNED_SHORT ||
           type == GL_FIXED || type == GL_FLOAT;
}

void glVertexAttribPointer(
    GLuint index,
    GLint size,
    GLenum type,
    GLboolean normalized,
    GLsizei stride,
    const GLvoid *pointer)
{
    if (!has_context()) return;
    if (index >= FXGL_MAX_VERTEX_ATTRIBS || size < 1 || size > 4 || stride < 0)
    {
        set_error(GL_INVALID_VALUE);
        return;
    }
    if (!valid_vertex_attrib_type(type))
    {
        set_error(GL_INVALID_ENUM);
        return;
    }

    VertexAttribute& attribute = g_context.attributes[index];
    attribute.size = size;
    attribute.type = type;
    attribute.normalized = normalized != GL_FALSE;
    attribute.stride = stride;
    attribute.pointer = pointer;
    attribute.buffer_binding = g_context.array_buffer_binding;
}

void glGetVertexAttribiv(GLuint index, GLenum pname, GLint *params)
{
    if (!has_context()) return;
    if (index >= FXGL_MAX_VERTEX_ATTRIBS)
    {
        set_error(GL_INVALID_VALUE);
        return;
    }
    if (!params)
    {
        set_error(GL_INVALID_VALUE);
        return;
    }

    const VertexAttribute& attribute = g_context.attributes[index];
    switch (pname)
    {
        case GL_VERTEX_ATTRIB_ARRAY_ENABLED:
            *params = attribute.enabled ? GL_TRUE : GL_FALSE;
            break;
        case GL_VERTEX_ATTRIB_ARRAY_SIZE:
            *params = attribute.size;
            break;
        case GL_VERTEX_ATTRIB_ARRAY_STRIDE:
            *params = attribute.stride;
            break;
        case GL_VERTEX_ATTRIB_ARRAY_TYPE:
            *params = static_cast<GLint>(attribute.type);
            break;
        case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED:
            *params = attribute.normalized ? GL_TRUE : GL_FALSE;
            break;
        case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
            *params = static_cast<GLint>(attribute.buffer_binding);
            break;
        default:
            set_error(GL_INVALID_ENUM);
            break;
    }
}

void glGetVertexAttribPointerv(GLuint index, GLenum pname, GLvoid **pointer)
{
    if (!has_context()) return;
    if (index >= FXGL_MAX_VERTEX_ATTRIBS)
    {
        set_error(GL_INVALID_VALUE);
        return;
    }
    if (!pointer)
    {
        set_error(GL_INVALID_VALUE);
        return;
    }
    if (pname != GL_VERTEX_ATTRIB_ARRAY_POINTER)
    {
        set_error(GL_INVALID_ENUM);
        return;
    }
    *pointer = const_cast<GLvoid *>(g_context.attributes[index].pointer);
}

void glPixelStorei(GLenum pname, GLint param)
{
    if (!has_context()) return;
    if (pname != GL_UNPACK_ALIGNMENT && pname != GL_PACK_ALIGNMENT)
    {
        set_error(GL_INVALID_ENUM);
        return;
    }
    if (param != 1 && param != 2 && param != 4 && param != 8)
    {
        set_error(GL_INVALID_VALUE);
        return;
    }

    if (pname == GL_UNPACK_ALIGNMENT)
        g_context.unpack_alignment = param;
    else
        g_context.pack_alignment = param;
}

GLboolean glIsTexture(GLuint texture)
{
    if (!has_context()) return GL_FALSE;
    return find_texture(texture) ? GL_TRUE : GL_FALSE;
}

void glGenTextures(GLsizei n, GLuint *textures)
{
    if (!has_context()) return;
    if (n < 0 || (n > 0 && !textures))
    {
        set_error(GL_INVALID_VALUE);
        return;
    }

    size_t available = 0;
    for (const TextureObject& object : g_context.textures)
        if (!object.reserved) ++available;
    if (available < static_cast<size_t>(n))
    {
        set_error(GL_OUT_OF_MEMORY);
        return;
    }

    GLsizei generated = 0;
    for (size_t i = 0; i < FXGL_MAX_TEXTURES && generated < n; ++i)
    {
        if (!g_context.textures[i].reserved)
        {
            g_context.textures[i] = {};
            g_context.textures[i].reserved = true;
            textures[generated++] = static_cast<GLuint>(i + 1);
        }
    }
}

void glDeleteTextures(GLsizei n, const GLuint *textures)
{
    if (!has_context()) return;
    if (n < 0 || (n > 0 && !textures))
    {
        set_error(GL_INVALID_VALUE);
        return;
    }

    for (GLsizei i = 0; i < n; ++i)
    {
        TextureObject *object = texture_slot(textures[i]);
        if (!object)
            continue;

        const GLuint name = textures[i];
        for (GLuint& binding : g_context.texture_binding_2d)
            if (binding == name)
                binding = 0;

        if (object->alive)
        {
            g_context.texture_bytes -= object->bytes;
            std::free(object->pixels);
        }
        *object = {};
    }
}

void glActiveTexture(GLenum texture)
{
    if (!has_context()) return;
    if (texture < GL_TEXTURE0 ||
        texture >= GL_TEXTURE0 + FXGL_MAX_TEXTURE_UNITS)
    {
        set_error(GL_INVALID_ENUM);
        return;
    }
    g_context.active_texture_unit =
        static_cast<size_t>(texture - GL_TEXTURE0);
}

void glBindTexture(GLenum target, GLuint texture)
{
    if (!has_context()) return;
    if (target != GL_TEXTURE_2D)
    {
        set_error(GL_INVALID_ENUM);
        return;
    }
    if (texture != 0)
    {
        TextureObject *object = texture_slot(texture);
        if (!object)
        {
            set_error(GL_INVALID_OPERATION);
            return;
        }
        object->alive = true;
    }
    g_context.texture_binding_2d[g_context.active_texture_unit] = texture;
}

void glTexParameteri(GLenum target, GLenum pname, GLint param)
{
    if (!has_context()) return;
    if (target != GL_TEXTURE_2D)
    {
        set_error(GL_INVALID_ENUM);
        return;
    }

    TextureObject *object = bound_texture(target);
    if (!object) return;

    switch (pname)
    {
        case GL_TEXTURE_MIN_FILTER:
        case GL_TEXTURE_MAG_FILTER:
            if (param == GL_NEAREST || param == GL_LINEAR)
            {
                if (pname == GL_TEXTURE_MIN_FILTER)
                    object->min_filter = static_cast<GLenum>(param);
                else
                    object->mag_filter = static_cast<GLenum>(param);
            }
            else if (param == GL_NEAREST_MIPMAP_NEAREST ||
                     param == GL_LINEAR_MIPMAP_NEAREST ||
                     param == GL_NEAREST_MIPMAP_LINEAR ||
                     param == GL_LINEAR_MIPMAP_LINEAR)
            {
                /* Valid GLES enums, but mip chains are not implemented yet. */
                set_error(GL_INVALID_OPERATION);
            }
            else
            {
                set_error(GL_INVALID_ENUM);
            }
            break;

        case GL_TEXTURE_WRAP_S:
        case GL_TEXTURE_WRAP_T:
            if (param != GL_CLAMP_TO_EDGE && param != GL_REPEAT &&
                param != GL_MIRRORED_REPEAT)
            {
                set_error(GL_INVALID_ENUM);
                return;
            }
            if (pname == GL_TEXTURE_WRAP_S)
                object->wrap_s = static_cast<GLenum>(param);
            else
                object->wrap_t = static_cast<GLenum>(param);
            break;

        default:
            set_error(GL_INVALID_ENUM);
            break;
    }
}

void glGetTexParameteriv(GLenum target, GLenum pname, GLint *params)
{
    if (!has_context()) return;
    if (!params)
    {
        set_error(GL_INVALID_VALUE);
        return;
    }
    TextureObject *object = bound_texture(target);
    if (!object) return;

    switch (pname)
    {
        case GL_TEXTURE_MIN_FILTER:
            *params = static_cast<GLint>(object->min_filter);
            break;
        case GL_TEXTURE_MAG_FILTER:
            *params = static_cast<GLint>(object->mag_filter);
            break;
        case GL_TEXTURE_WRAP_S:
            *params = static_cast<GLint>(object->wrap_s);
            break;
        case GL_TEXTURE_WRAP_T:
            *params = static_cast<GLint>(object->wrap_t);
            break;
        default:
            set_error(GL_INVALID_ENUM);
            break;
    }
}

void glTexImage2D(
    GLenum target,
    GLint level,
    GLint internalformat,
    GLsizei width,
    GLsizei height,
    GLint border,
    GLenum format,
    GLenum type,
    const GLvoid *pixels)
{
    if (!has_context()) return;
    if (target != GL_TEXTURE_2D || format != GL_RGB ||
        type != GL_UNSIGNED_SHORT_5_6_5)
    {
        set_error(GL_INVALID_ENUM);
        return;
    }
    if (level != 0 || width < 0 || height < 0 || border != 0 ||
        width > FXGL_MAX_TEXTURE_SIZE || height > FXGL_MAX_TEXTURE_SIZE)
    {
        set_error(GL_INVALID_VALUE);
        return;
    }
    if (internalformat != static_cast<GLint>(format))
    {
        set_error(GL_INVALID_OPERATION);
        return;
    }

    TextureObject *object = bound_texture(target);
    if (!object) return;

    const size_t texels = static_cast<size_t>(width) *
                          static_cast<size_t>(height);
    const size_t new_bytes = texels * sizeof(uint16_t);
    const size_t retained = g_context.texture_bytes - object->bytes;
    if (new_bytes > FXGL_MAX_TEXTURE_BYTES - retained)
    {
        set_error(GL_OUT_OF_MEMORY);
        return;
    }

    uint16_t *new_pixels = nullptr;
    if (new_bytes > 0)
    {
        new_pixels = static_cast<uint16_t *>(std::malloc(new_bytes));
        if (!new_pixels)
        {
            set_error(GL_OUT_OF_MEMORY);
            return;
        }
        if (pixels)
        {
            const size_t row_bytes = static_cast<size_t>(width) * sizeof(uint16_t);
            const size_t alignment =
                static_cast<size_t>(g_context.unpack_alignment);
            const size_t source_stride =
                (row_bytes + alignment - 1) & ~(alignment - 1);
            const uint8_t *source = static_cast<const uint8_t *>(pixels);
            for (GLsizei row = 0; row < height; ++row)
            {
                std::memcpy(new_pixels + static_cast<size_t>(row) * width,
                    source + static_cast<size_t>(row) * source_stride,
                    row_bytes);
            }
        }
        else
        {
            std::memset(new_pixels, 0, new_bytes);
        }
    }

    std::free(object->pixels);
    object->pixels = new_pixels;
    object->width = static_cast<uint16_t>(width);
    object->height = static_cast<uint16_t>(height);
    object->bytes = new_bytes;
    g_context.texture_bytes = retained + new_bytes;
}

void glTexSubImage2D(
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLenum type,
    const GLvoid *pixels)
{
    if (!has_context()) return;
    if (target != GL_TEXTURE_2D || format != GL_RGB ||
        type != GL_UNSIGNED_SHORT_5_6_5)
    {
        set_error(GL_INVALID_ENUM);
        return;
    }
    if (level != 0 || xoffset < 0 || yoffset < 0 || width < 0 || height < 0)
    {
        set_error(GL_INVALID_VALUE);
        return;
    }

    TextureObject *object = bound_texture(target);
    if (!object) return;

    const int64_t right = static_cast<int64_t>(xoffset) + width;
    const int64_t bottom = static_cast<int64_t>(yoffset) + height;
    if (!object->pixels || right > object->width || bottom > object->height)
    {
        set_error(GL_INVALID_VALUE);
        return;
    }
    if (width > 0 && height > 0 && !pixels)
    {
        set_error(GL_INVALID_VALUE);
        return;
    }

    const size_t row_bytes = static_cast<size_t>(width) * sizeof(uint16_t);
    const size_t alignment = static_cast<size_t>(g_context.unpack_alignment);
    const size_t source_stride =
        (row_bytes + alignment - 1) & ~(alignment - 1);
    const uint8_t *source = static_cast<const uint8_t *>(pixels);
    for (GLsizei row = 0; row < height; ++row)
    {
        uint16_t *destination = object->pixels +
            static_cast<size_t>(yoffset + row) * object->width + xoffset;
        std::memcpy(destination,
            source + static_cast<size_t>(row) * source_stride,
            row_bytes);
    }
}

void glViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
    if (!has_context()) return;
    if (width < 0 || height < 0)
    {
        set_error(GL_INVALID_VALUE);
        return;
    }
    g_context.viewport[0] = x;
    g_context.viewport[1] = y;
    g_context.viewport[2] = width;
    g_context.viewport[3] = height;
}

void glScissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
    if (!has_context()) return;
    if (width < 0 || height < 0)
    {
        set_error(GL_INVALID_VALUE);
        return;
    }
    g_context.scissor[0] = x;
    g_context.scissor[1] = y;
    g_context.scissor[2] = width;
    g_context.scissor[3] = height;
}

void glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    if (!has_context()) return;
    g_context.clear_color[0] = clamp01(red);
    g_context.clear_color[1] = clamp01(green);
    g_context.clear_color[2] = clamp01(blue);
    g_context.clear_color[3] = clamp01(alpha);
}

void glClearDepthf(GLclampf depth)
{
    if (!has_context()) return;
    g_context.clear_depth = clamp01(depth);
}

void glClearStencil(GLint s)
{
    if (!has_context()) return;
    g_context.clear_stencil = s;
}

void glClear(GLbitfield mask)
{
    if (!has_context()) return;
    const GLbitfield allowed =
        GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
    if (mask & ~allowed)
    {
        set_error(GL_INVALID_VALUE);
        return;
    }

    GLint rect[4] = {0, 0, g_context.surface.width, g_context.surface.height};
    GLint bounds[4] = {0, 0, g_context.surface.width, g_context.surface.height};
    if (g_context.scissor_test)
    {
        clear_rect(g_context.scissor, bounds);
    }
    else
    {
        clear_rect(rect, bounds);
    }

    if (bounds[0] >= bounds[2] || bounds[1] >= bounds[3])
        return;

    if ((mask & GL_COLOR_BUFFER_BIT) && g_context.surface.color)
        clear_color_buffer(bounds[0], bounds[1], bounds[2], bounds[3]);

    if ((mask & GL_DEPTH_BUFFER_BIT) && g_context.surface.depth &&
        g_context.depth_mask)
    {
        const uint16_t depth = static_cast<uint16_t>(
            g_context.clear_depth * 65535.0f + 0.5f);
        for (GLint y = bounds[1]; y < bounds[3]; ++y)
        {
            uint16_t *row = g_context.surface.depth +
                static_cast<size_t>(y) * g_context.surface.stride;
            for (GLint x = bounds[0]; x < bounds[2]; ++x)
                row[x] = depth;
        }
    }

    if ((mask & GL_STENCIL_BUFFER_BIT) && g_context.surface.stencil)
    {
        const uint8_t clear_value = static_cast<uint8_t>(g_context.clear_stencil);
        const uint8_t write_mask = static_cast<uint8_t>(g_context.stencil_mask);
        for (GLint y = bounds[1]; y < bounds[3]; ++y)
        {
            uint8_t *row = g_context.surface.stencil +
                static_cast<size_t>(y) * g_context.surface.stride;
            for (GLint x = bounds[0]; x < bounds[2]; ++x)
            {
                row[x] = static_cast<uint8_t>(
                    (row[x] & static_cast<uint8_t>(~write_mask)) |
                    (clear_value & write_mask));
            }
        }
    }
}

void glEnable(GLenum cap)
{
    if (!has_context()) return;
    bool *state = capability(cap);
    if (!state)
    {
        set_error(GL_INVALID_ENUM);
        return;
    }
    *state = true;
}

void glDisable(GLenum cap)
{
    if (!has_context()) return;
    bool *state = capability(cap);
    if (!state)
    {
        set_error(GL_INVALID_ENUM);
        return;
    }
    *state = false;
}

GLboolean glIsEnabled(GLenum cap)
{
    if (!has_context()) return GL_FALSE;
    bool *state = capability(cap);
    if (!state)
    {
        set_error(GL_INVALID_ENUM);
        return GL_FALSE;
    }
    return *state ? GL_TRUE : GL_FALSE;
}

void glDepthFunc(GLenum func)
{
    if (!has_context()) return;
    if (!valid_depth_func(func))
    {
        set_error(GL_INVALID_ENUM);
        return;
    }
    g_context.depth_func = func;
}

void glDepthMask(GLboolean flag)
{
    if (!has_context()) return;
    g_context.depth_mask = flag != GL_FALSE;
}

void glCullFace(GLenum mode)
{
    if (!has_context()) return;
    if (mode != GL_FRONT && mode != GL_BACK && mode != GL_FRONT_AND_BACK)
    {
        set_error(GL_INVALID_ENUM);
        return;
    }
    g_context.cull_mode = mode;
}

void glFrontFace(GLenum mode)
{
    if (!has_context()) return;
    if (mode != GL_CW && mode != GL_CCW)
    {
        set_error(GL_INVALID_ENUM);
        return;
    }
    g_context.front_face = mode;
}

void glBlendFunc(GLenum sfactor, GLenum dfactor)
{
    if (!has_context()) return;
    if (!valid_blend_factor(sfactor) || !valid_blend_factor(dfactor))
    {
        set_error(GL_INVALID_ENUM);
        return;
    }
    g_context.blend_src = sfactor;
    g_context.blend_dst = dfactor;
}

void glBlendEquation(GLenum mode)
{
    if (!has_context()) return;
    if (!valid_blend_equation(mode))
    {
        set_error(GL_INVALID_ENUM);
        return;
    }
    g_context.blend_equation = mode;
}

void glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
    if (!has_context()) return;
    g_context.color_mask[0] = red != GL_FALSE ? GL_TRUE : GL_FALSE;
    g_context.color_mask[1] = green != GL_FALSE ? GL_TRUE : GL_FALSE;
    g_context.color_mask[2] = blue != GL_FALSE ? GL_TRUE : GL_FALSE;
    g_context.color_mask[3] = alpha != GL_FALSE ? GL_TRUE : GL_FALSE;
}

void glStencilMask(GLuint mask)
{
    if (!has_context()) return;
    g_context.stencil_mask = mask;
}

void glGetIntegerv(GLenum pname, GLint *data)
{
    if (!has_context()) return;
    if (!data)
    {
        set_error(GL_INVALID_VALUE);
        return;
    }

    switch (pname)
    {
        case GL_VIEWPORT:
            std::memcpy(data, g_context.viewport, sizeof(g_context.viewport));
            break;
        case GL_SCISSOR_BOX:
            std::memcpy(data, g_context.scissor, sizeof(g_context.scissor));
            break;
        case GL_DEPTH_FUNC:
            data[0] = static_cast<GLint>(g_context.depth_func);
            break;
        case GL_DEPTH_WRITEMASK:
            data[0] = g_context.depth_mask ? GL_TRUE : GL_FALSE;
            break;
        case GL_COLOR_WRITEMASK:
            for (int i = 0; i < 4; ++i)
                data[i] = g_context.color_mask[i] ? GL_TRUE : GL_FALSE;
            break;
        case GL_STENCIL_CLEAR_VALUE:
            data[0] = g_context.clear_stencil;
            break;
        case GL_CULL_FACE_MODE:
            data[0] = static_cast<GLint>(g_context.cull_mode);
            break;
        case GL_FRONT_FACE:
            data[0] = static_cast<GLint>(g_context.front_face);
            break;
        case GL_BLEND_SRC_RGB:
            data[0] = static_cast<GLint>(g_context.blend_src);
            break;
        case GL_BLEND_DST_RGB:
            data[0] = static_cast<GLint>(g_context.blend_dst);
            break;
        case GL_BLEND_EQUATION_RGB:
            data[0] = static_cast<GLint>(g_context.blend_equation);
            break;
        case GL_STENCIL_WRITEMASK:
            data[0] = static_cast<GLint>(g_context.stencil_mask);
            break;
        case GL_ARRAY_BUFFER_BINDING:
            data[0] = static_cast<GLint>(g_context.array_buffer_binding);
            break;
        case GL_ELEMENT_ARRAY_BUFFER_BINDING:
            data[0] = static_cast<GLint>(g_context.element_buffer_binding);
            break;
        case GL_MAX_VERTEX_ATTRIBS:
            data[0] = static_cast<GLint>(FXGL_MAX_VERTEX_ATTRIBS);
            break;
        case GL_ACTIVE_TEXTURE:
            data[0] = static_cast<GLint>(
                GL_TEXTURE0 + g_context.active_texture_unit);
            break;
        case GL_TEXTURE_BINDING_2D:
            data[0] = static_cast<GLint>(
                g_context.texture_binding_2d[g_context.active_texture_unit]);
            break;
        case GL_MAX_TEXTURE_IMAGE_UNITS:
            data[0] = static_cast<GLint>(FXGL_MAX_TEXTURE_UNITS);
            break;
        case GL_UNPACK_ALIGNMENT:
            data[0] = g_context.unpack_alignment;
            break;
        case GL_PACK_ALIGNMENT:
            data[0] = g_context.pack_alignment;
            break;
        case GL_MAX_TEXTURE_SIZE:
            data[0] = FXGL_MAX_TEXTURE_SIZE;
            break;
        case GL_MAX_VIEWPORT_DIMS:
            data[0] = g_context.surface.width;
            data[1] = g_context.surface.height;
            break;
        default:
            set_error(GL_INVALID_ENUM);
            break;
    }
}

void glGetBooleanv(GLenum pname, GLboolean *data)
{
    if (!has_context()) return;
    if (!data)
    {
        set_error(GL_INVALID_VALUE);
        return;
    }

    GLint values[4] = {};
    glGetIntegerv(pname, values);
    if (g_context.error != GL_NO_ERROR)
        return;

    const int count = (pname == GL_VIEWPORT || pname == GL_SCISSOR_BOX ||
                       pname == GL_COLOR_WRITEMASK) ? 4 :
                      (pname == GL_MAX_VIEWPORT_DIMS ? 2 : 1);
    for (int i = 0; i < count; ++i)
        data[i] = values[i] ? GL_TRUE : GL_FALSE;
}

void glGetFloatv(GLenum pname, GLfloat *data)
{
    if (!has_context()) return;
    if (!data)
    {
        set_error(GL_INVALID_VALUE);
        return;
    }

    if (pname == GL_DEPTH_CLEAR_VALUE)
    {
        data[0] = g_context.clear_depth;
        return;
    }

    GLint values[4] = {};
    glGetIntegerv(pname, values);
    if (g_context.error != GL_NO_ERROR)
        return;

    const int count = (pname == GL_VIEWPORT || pname == GL_SCISSOR_BOX ||
                       pname == GL_COLOR_WRITEMASK) ? 4 :
                      (pname == GL_MAX_VIEWPORT_DIMS ? 2 : 1);
    for (int i = 0; i < count; ++i)
        data[i] = static_cast<GLfloat>(values[i]);
}

const GLubyte *glGetString(GLenum name)
{
    static const GLubyte vendor[] = "HelaFaye / CasioMini3D";
    static const GLubyte renderer[] = "fxgl SH-4A software renderer";
    static const GLubyte version[] = "OpenGL ES 2.0 fxgl partial software implementation";
    static const GLubyte extensions[] = "";
    if (!has_context()) return nullptr;

    switch (name)
    {
        case GL_VENDOR: return vendor;
        case GL_RENDERER: return renderer;
        case GL_VERSION: return version;
        case GL_EXTENSIONS: return extensions;
        default:
            set_error(GL_INVALID_ENUM);
            return nullptr;
    }
}

GLenum glGetError(void)
{
    if (!g_context.current)
        return GL_NO_ERROR;
    const GLenum error = g_context.error;
    g_context.error = GL_NO_ERROR;
    return error;
}

} // extern "C"

namespace fxgl
{
bool get_texture_binding(
    size_t unit,
    gfx::Texture2DView& view,
    gfx::TextureSampler& sampler)
{
    if (!g_context.current || unit >= FXGL_MAX_TEXTURE_UNITS)
        return false;

    TextureObject *object = find_texture(
        g_context.texture_binding_2d[unit]);
    if (!object || !object->pixels || object->width == 0 || object->height == 0)
        return false;

    view.pixels = object->pixels;
    view.width = object->width;
    view.height = object->height;
    view.stride = object->width;
    sampler = make_sampler(*object);
    return gfx::texture_valid(view) && gfx::sampler_valid(sampler);
}
} // namespace fxgl
