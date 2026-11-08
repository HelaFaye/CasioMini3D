#include <GLES2/gl2.h>
#include <fxgl/platform.h>
#include "../gfx/texture.hpp"
#include "context_internal.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
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
constexpr size_t FXGL_MAX_TEXTURE_LEVELS = 8;
constexpr size_t FXGL_MAX_FRAMEBUFFERS = 4;
constexpr size_t FXGL_MAX_RENDERBUFFERS = 8;
constexpr size_t FXGL_MAX_RENDERBUFFER_BYTES = 128u * 1024u;
constexpr GLint FXGL_MAX_RENDERBUFFER_SIZE = 128;

struct BufferObject
{
    uint8_t *data = nullptr;
    size_t size = 0;
    GLenum usage = GL_STATIC_DRAW;
    bool reserved = false;
    bool alive = false;
};

struct TextureMipObject
{
    uint16_t *pixels = nullptr;
    uint16_t width = 0;
    uint16_t height = 0;
    size_t bytes = 0;
};

struct TextureObject
{
    uint16_t *pixels = nullptr;
    TextureMipObject mip_levels[FXGL_MAX_TEXTURE_LEVELS - 1] = {};
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

struct FramebufferAttachment
{
    GLenum object_type = GL_NONE;
    GLuint object_name = 0;
    GLint texture_level = 0;
};

struct FramebufferObject
{
    bool reserved = false;
    bool alive = false;
    FramebufferAttachment color;
    FramebufferAttachment depth;
    FramebufferAttachment stencil;
};

struct RenderbufferObject
{
    uint8_t *storage = nullptr;
    size_t bytes = 0;
    GLsizei width = 0;
    GLsizei height = 0;
    GLenum internal_format = 0;
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
    GLfloat current[4] = {0.0f, 0.0f, 0.0f, 1.0f};
};

struct StencilFace
{
    GLenum function = GL_ALWAYS;
    GLint reference = 0;
    GLuint value_mask = 0xff;
    GLuint write_mask = 0xff;
    GLenum stencil_fail = GL_KEEP;
    GLenum depth_fail = GL_KEEP;
    GLenum depth_pass = GL_KEEP;
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
    FramebufferObject framebuffers[FXGL_MAX_FRAMEBUFFERS] = {};
    RenderbufferObject renderbuffers[FXGL_MAX_RENDERBUFFERS] = {};
    size_t renderbuffer_bytes = 0;
    GLuint framebuffer_binding = 0;
    GLuint renderbuffer_binding = 0;
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
    GLfloat depth_near = 0.0f;
    GLfloat depth_far = 1.0f;
    GLenum cull_mode = GL_BACK;
    GLenum front_face = GL_CCW;
    GLenum blend_src = GL_ONE;
    GLenum blend_dst = GL_ZERO;
    GLenum blend_equation = GL_FUNC_ADD;
    GLenum blend_src_alpha = GL_ONE;
    GLenum blend_dst_alpha = GL_ZERO;
    GLenum blend_equation_alpha = GL_FUNC_ADD;
    GLfloat blend_color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    GLboolean color_mask[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
    GLuint stencil_mask = 0xff;
    StencilFace stencil_front = {};
    StencilFace stencil_back = {};

    GLenum error = GL_NO_ERROR;
};

Context g_context;
static gfx::TextureMipLevel g_texture_mip_views[
    FXGL_MAX_TEXTURES][FXGL_MAX_TEXTURE_LEVELS - 1] = {};

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
    sampler.filter = object.mag_filter == GL_LINEAR
        ? gfx::TextureFilter::Bilinear : gfx::TextureFilter::Nearest;
    switch (object.min_filter)
    {
        case GL_NEAREST:
            sampler.min_filter = gfx::TextureMinFilter::Nearest;
            break;
        case GL_LINEAR:
            sampler.min_filter = gfx::TextureMinFilter::Linear;
            break;
        case GL_NEAREST_MIPMAP_NEAREST:
            sampler.min_filter = gfx::TextureMinFilter::NearestMipmapNearest;
            break;
        case GL_LINEAR_MIPMAP_NEAREST:
            sampler.min_filter = gfx::TextureMinFilter::LinearMipmapNearest;
            break;
        case GL_NEAREST_MIPMAP_LINEAR:
            sampler.min_filter = gfx::TextureMinFilter::NearestMipmapLinear;
            break;
        case GL_LINEAR_MIPMAP_LINEAR:
            sampler.min_filter = gfx::TextureMinFilter::LinearMipmapLinear;
            break;
        default:
            sampler.min_filter = gfx::TextureMinFilter::Nearest;
            break;
    }
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

static void release_texture_mips(TextureObject& object)
{
    for (TextureMipObject& level : object.mip_levels)
    {
        std::free(level.pixels);
        level = {};
    }
}

static size_t texture_level_bytes(uint16_t width, uint16_t height)
{
    return static_cast<size_t>(width) * height * sizeof(uint16_t);
}

static GLint texture_max_level(uint16_t width, uint16_t height)
{
    uint16_t extent = std::max(width, height);
    GLint level = 0;
    while (extent > 1)
    {
        extent = static_cast<uint16_t>(extent / 2);
        ++level;
    }
    return level;
}

static void destroy_textures(Context& context)
{
    for (TextureObject& object : context.textures)
    {
        std::free(object.pixels);
        release_texture_mips(object);
        object = {};
    }
    context.texture_bytes = 0;
    for (GLuint& binding : context.texture_binding_2d)
        binding = 0;
    context.active_texture_unit = 0;
}

static FramebufferObject *framebuffer_slot(GLuint name)
{
    if (name == 0 || name > FXGL_MAX_FRAMEBUFFERS)
        return nullptr;
    FramebufferObject& object = g_context.framebuffers[name - 1];
    return object.reserved ? &object : nullptr;
}

static FramebufferObject *find_framebuffer(GLuint name)
{
    FramebufferObject *object = framebuffer_slot(name);
    return object && object->alive ? object : nullptr;
}

static RenderbufferObject *renderbuffer_slot(GLuint name)
{
    if (name == 0 || name > FXGL_MAX_RENDERBUFFERS)
        return nullptr;
    RenderbufferObject& object = g_context.renderbuffers[name - 1];
    return object.reserved ? &object : nullptr;
}

static RenderbufferObject *find_renderbuffer(GLuint name)
{
    RenderbufferObject *object = renderbuffer_slot(name);
    return object && object->alive ? object : nullptr;
}

static FramebufferAttachment *bound_attachment(GLenum attachment)
{
    FramebufferObject *framebuffer = find_framebuffer(
        g_context.framebuffer_binding);
    if (!framebuffer)
    {
        set_error(GL_INVALID_OPERATION);
        return nullptr;
    }

    switch (attachment)
    {
        case GL_COLOR_ATTACHMENT0: return &framebuffer->color;
        case GL_DEPTH_ATTACHMENT: return &framebuffer->depth;
        case GL_STENCIL_ATTACHMENT: return &framebuffer->stencil;
        default:
            set_error(GL_INVALID_ENUM);
            return nullptr;
    }
}

static bool attachment_dimensions(
    const FramebufferAttachment& attachment,
    GLsizei& width,
    GLsizei& height,
    GLenum expected_format)
{
    if (attachment.object_type == GL_TEXTURE)
    {
        if (attachment.texture_level != 0 || expected_format != GL_RGB565)
            return false;
        TextureObject *texture = find_texture(attachment.object_name);
        if (!texture || !texture->pixels || texture->width == 0 ||
            texture->height == 0)
            return false;
        width = texture->width;
        height = texture->height;
        return true;
    }

    if (attachment.object_type == GL_RENDERBUFFER)
    {
        RenderbufferObject *renderbuffer =
            find_renderbuffer(attachment.object_name);
        if (!renderbuffer || !renderbuffer->storage ||
            renderbuffer->width <= 0 || renderbuffer->height <= 0 ||
            renderbuffer->internal_format != expected_format)
            return false;
        width = renderbuffer->width;
        height = renderbuffer->height;
        return true;
    }

    return false;
}

static GLenum framebuffer_status()
{
    if (g_context.framebuffer_binding == 0)
        return g_context.surface.color ? GL_FRAMEBUFFER_COMPLETE
                                       : GL_FRAMEBUFFER_UNSUPPORTED;

    FramebufferObject *framebuffer =
        find_framebuffer(g_context.framebuffer_binding);
    if (!framebuffer)
        return GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT;

    const FramebufferAttachment *attachments[3] = {
        &framebuffer->color, &framebuffer->depth, &framebuffer->stencil
    };
    const GLenum formats[3] = {
        GL_RGB565, GL_DEPTH_COMPONENT16, GL_STENCIL_INDEX8
    };

    bool has_attachment = false;
    GLsizei reference_width = 0;
    GLsizei reference_height = 0;

    for (size_t i = 0; i < 3; ++i)
    {
        const FramebufferAttachment& attachment = *attachments[i];
        if (attachment.object_type == GL_NONE)
            continue;
        has_attachment = true;

        GLsizei width = 0;
        GLsizei height = 0;
        if (!attachment_dimensions(attachment, width, height, formats[i]))
            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;

        if (width > FXGL_MAX_RENDERBUFFER_SIZE ||
            height > FXGL_MAX_RENDERBUFFER_SIZE)
            return GL_FRAMEBUFFER_UNSUPPORTED;

        if (reference_width == 0)
        {
            reference_width = width;
            reference_height = height;
        }
        else if (width != reference_width || height != reference_height)
        {
            return GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS;
        }
    }

    return has_attachment ? GL_FRAMEBUFFER_COMPLETE
                          : GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT;
}

static bool resolve_draw_surface(fxgl_surface_t& surface)
{
    if (g_context.framebuffer_binding == 0)
    {
        surface = g_context.surface;
        return surface.color != nullptr;
    }
    if (framebuffer_status() != GL_FRAMEBUFFER_COMPLETE)
        return false;

    FramebufferObject *framebuffer =
        find_framebuffer(g_context.framebuffer_binding);
    if (!framebuffer)
        return false;

    surface = {};
    GLsizei width = 0;
    GLsizei height = 0;

    if (framebuffer->color.object_type == GL_TEXTURE)
    {
        TextureObject *texture = find_texture(framebuffer->color.object_name);
        if (!texture)
            return false;
        surface.color = texture->pixels;
        width = texture->width;
        height = texture->height;
    }
    else if (framebuffer->color.object_type == GL_RENDERBUFFER)
    {
        RenderbufferObject *renderbuffer =
            find_renderbuffer(framebuffer->color.object_name);
        if (!renderbuffer || renderbuffer->internal_format != GL_RGB565)
            return false;
        surface.color = reinterpret_cast<uint16_t *>(renderbuffer->storage);
        width = renderbuffer->width;
        height = renderbuffer->height;
    }

    if (width == 0 || height == 0)
    {
        const FramebufferAttachment *fallbacks[2] = {
            &framebuffer->depth, &framebuffer->stencil
        };
        for (const FramebufferAttachment *attachment : fallbacks)
        {
            if (attachment->object_type != GL_RENDERBUFFER)
                continue;
            RenderbufferObject *renderbuffer =
                find_renderbuffer(attachment->object_name);
            if (!renderbuffer)
                return false;
            width = renderbuffer->width;
            height = renderbuffer->height;
            break;
        }
    }
    if (width <= 0 || height <= 0)
        return false;

    if (framebuffer->depth.object_type == GL_RENDERBUFFER)
    {
        RenderbufferObject *renderbuffer =
            find_renderbuffer(framebuffer->depth.object_name);
        if (!renderbuffer)
            return false;
        surface.depth = reinterpret_cast<uint16_t *>(renderbuffer->storage);
    }
    if (framebuffer->stencil.object_type == GL_RENDERBUFFER)
    {
        RenderbufferObject *renderbuffer =
            find_renderbuffer(framebuffer->stencil.object_name);
        if (!renderbuffer)
            return false;
        surface.stencil = renderbuffer->storage;
    }

    surface.width = width;
    surface.height = height;
    surface.stride = width;
    return true;
}

static void destroy_framebuffer_resources(Context& context)
{
    for (RenderbufferObject& renderbuffer : context.renderbuffers)
    {
        std::free(renderbuffer.storage);
        renderbuffer = {};
    }
    for (FramebufferObject& framebuffer : context.framebuffers)
        framebuffer = {};
    context.renderbuffer_bytes = 0;
    context.framebuffer_binding = 0;
    context.renderbuffer_binding = 0;
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

static bool valid_blend_pair(GLenum source, GLenum destination,
                              bool alpha_channel)
{
    if (!valid_blend_factor(source) || !valid_blend_factor(destination) ||
        destination == GL_SRC_ALPHA_SATURATE)
        return false;
    if (alpha_channel && source == GL_SRC_ALPHA_SATURATE)
        return false;
    return true;
}

static bool valid_stencil_operation(GLenum value)
{
    return value == GL_ZERO || value == GL_KEEP || value == GL_REPLACE ||
           value == GL_INCR || value == GL_DECR || value == GL_INVERT ||
           value == GL_INCR_WRAP || value == GL_DECR_WRAP;
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

static void clear_rect(const fxgl_surface_t& surface, GLint rect[4], GLint bounds[4])
{
    const int64_t raw_x0 = rect[0];
    const int64_t raw_y0 = rect[1];
    const int64_t raw_x1 = raw_x0 + rect[2];
    const int64_t raw_y1 = raw_y0 + rect[3];

    const int64_t x0 = std::max<int64_t>(
        0, std::min<int64_t>(surface.width, raw_x0));
    const int64_t y0 = std::max<int64_t>(
        0, std::min<int64_t>(surface.height, raw_y0));
    const int64_t x1 = std::max<int64_t>(
        0, std::min<int64_t>(surface.width, raw_x1));
    const int64_t y1 = std::max<int64_t>(
        0, std::min<int64_t>(surface.height, raw_y1));

    bounds[0] = static_cast<GLint>(std::min(x0, x1));
    bounds[1] = static_cast<GLint>(std::min(y0, y1));
    bounds[2] = static_cast<GLint>(std::max(x0, x1));
    bounds[3] = static_cast<GLint>(std::max(y0, y1));
}

static size_t shader_components(fxgl::ShaderType type)
{
    switch (type)
    {
        case fxgl::ShaderType::Float: return 1;
        case fxgl::ShaderType::Vec2: return 2;
        case fxgl::ShaderType::Vec3: return 3;
        case fxgl::ShaderType::Vec4: return 4;
        default: return 0;
    }
}

static size_t attribute_component_bytes(GLenum type)
{
    switch (type)
    {
        case GL_BYTE:
        case GL_UNSIGNED_BYTE: return 1;
        case GL_SHORT:
        case GL_UNSIGNED_SHORT: return 2;
        case GL_FIXED:
        case GL_FLOAT: return 4;
        default: return 0;
    }
}

static float decode_attribute_value(
    const uint8_t *source,
    GLenum type,
    bool normalized)
{
    switch (type)
    {
        case GL_BYTE:
        {
            int8_t value = 0;
            std::memcpy(&value, source, sizeof(value));
            if (!normalized) return static_cast<float>(value);
            return std::max(-1.0f, static_cast<float>(value) / 127.0f);
        }
        case GL_UNSIGNED_BYTE:
        {
            uint8_t value = 0;
            std::memcpy(&value, source, sizeof(value));
            return normalized ? static_cast<float>(value) / 255.0f
                              : static_cast<float>(value);
        }
        case GL_SHORT:
        {
            int16_t value = 0;
            std::memcpy(&value, source, sizeof(value));
            if (!normalized) return static_cast<float>(value);
            return std::max(-1.0f, static_cast<float>(value) / 32767.0f);
        }
        case GL_UNSIGNED_SHORT:
        {
            uint16_t value = 0;
            std::memcpy(&value, source, sizeof(value));
            return normalized ? static_cast<float>(value) / 65535.0f
                              : static_cast<float>(value);
        }
        case GL_FIXED:
        {
            int32_t value = 0;
            std::memcpy(&value, source, sizeof(value));
            return static_cast<float>(value) / 65536.0f;
        }
        case GL_FLOAT:
        {
            float value = 0.0f;
            std::memcpy(&value, source, sizeof(value));
            return value;
        }
        default:
            return 0.0f;
    }
}

static bool fetch_vertex_attribute(
    GLuint location,
    uint32_t vertex_index,
    fxgl::ShaderType shader_type,
    fxgl::ShaderValue& value)
{
    if (location >= FXGL_MAX_VERTEX_ATTRIBS)
        return false;

    const VertexAttribute& attribute = g_context.attributes[location];
    const size_t components = shader_components(shader_type);
    if (components == 0)
        return false;

    value = {};
    value.type = shader_type;
    value.data[0] = attribute.current[0];
    value.data[1] = attribute.current[1];
    value.data[2] = attribute.current[2];
    value.data[3] = attribute.current[3];

    if (!attribute.enabled)
        return true;

    const size_t component_bytes = attribute_component_bytes(attribute.type);
    if (component_bytes == 0 || attribute.size < 1 || attribute.size > 4)
        return false;

    const size_t element_bytes =
        static_cast<size_t>(attribute.size) * component_bytes;
    const size_t stride = attribute.stride == 0
        ? element_bytes : static_cast<size_t>(attribute.stride);

    const uint8_t *element = nullptr;
    if (attribute.buffer_binding != 0)
    {
        BufferObject *buffer = find_buffer(attribute.buffer_binding);
        if (!buffer)
            return false;

        const uintptr_t base_offset =
            reinterpret_cast<uintptr_t>(attribute.pointer);
        if (vertex_index > (std::numeric_limits<size_t>::max() - base_offset) /
                           std::max<size_t>(stride, 1))
            return false;
        const size_t offset = static_cast<size_t>(base_offset) +
                              static_cast<size_t>(vertex_index) * stride;
        if (offset > buffer->size || element_bytes > buffer->size - offset)
            return false;
        element = buffer->data + offset;
    }
    else
    {
        if (!attribute.pointer)
            return false;
        if (vertex_index > std::numeric_limits<size_t>::max() /
                           std::max<size_t>(stride, 1))
            return false;
        const size_t offset = static_cast<size_t>(vertex_index) * stride;
        element = static_cast<const uint8_t *>(attribute.pointer) + offset;
    }

    const size_t fetch_components =
        std::min(components, static_cast<size_t>(attribute.size));
    for (size_t i = 0; i < fetch_components; ++i)
    {
        value.data[i] = decode_attribute_value(
            element + i * component_bytes,
            attribute.type,
            attribute.normalized);
        if (!std::isfinite(value.data[i]))
            return false;
    }
    return true;
}

struct CachedTextureBinding
{
    gfx::Texture2DView view;
    gfx::TextureSampler sampler;
    bool valid;
    bool nearest_repeat_power_of_two;
    bool requires_derivatives;
};

struct DrawInvocation
{
    fxgl::LinkedProgramView program;
    int passthrough_varying_offset;
    bool fragment_failed;
    bool varying_derivatives;
    CachedTextureBinding textures[FXGL_MAX_TEXTURE_UNITS];
    fxgl::ShaderRuntime fragment_runtime;
};

static int find_passthrough_varying_offset(
    const fxgl::LinkedProgramView& program)
{
    const fxgl::ShaderModule& module = *program.fragment_shader;
    if (module.stage != fxgl::ShaderStage::Fragment ||
        module.instruction_count != 2 ||
        module.varying_count == 0)
        return -1;

    const fxgl::ShaderInstruction& load = module.instructions[0];
    const fxgl::ShaderInstruction& store = module.instructions[1];
    if (load.op != fxgl::ShaderOp::LoadVarying ||
        store.op != fxgl::ShaderOp::StoreFragmentColor ||
        load.type != fxgl::ShaderType::Vec4 ||
        store.type != fxgl::ShaderType::Vec4 ||
        load.index >= module.varying_count ||
        module.varyings[load.index].type != fxgl::ShaderType::Vec4)
        return -1;

    const size_t offset = program.fragment_varying_offsets[load.index];
    if (offset + 4 > program.varying_scalar_count)
        return -1;
    return static_cast<int>(offset);
}

static void initialize_draw_invocation(
    DrawInvocation& invocation,
    const fxgl::LinkedProgramView& program)
{
    invocation = {};
    invocation.program = program;
    invocation.passthrough_varying_offset =
        find_passthrough_varying_offset(program);

    if (!program.fragment_shader)
        return;

    const fxgl::ShaderModule& module = *program.fragment_shader;

    // Fragment uniforms are immutable for the duration of a synchronous GL
    // draw. Resolve and copy them once into the per-draw runtime rather than
    // repeating the lookup and ShaderValue copy for every covered fragment.
    for (size_t i = 0; i < module.uniform_count; ++i)
    {
        const uint8_t location = program.fragment_uniform_map[i];
        if (location >= program.uniform_count)
        {
            invocation.fragment_failed = true;
            return;
        }
        invocation.fragment_runtime.uniforms[i] =
            program.uniform_values[location];
    }

    bool uses_texture = false;
    for (size_t i = 0; i < module.instruction_count; ++i)
    {
        if (module.instructions[i].op == fxgl::ShaderOp::Texture2D)
        {
            uses_texture = true;
            break;
        }
    }
    if (!uses_texture)
        return;

    // A GL draw call cannot mutate texture bindings or sampler uniforms while
    // its fragments execute. Resolve each supported unit once and reuse the
    // validated non-owning views for every fragment and triangle in this draw.
    for (size_t unit = 0; unit < FXGL_MAX_TEXTURE_UNITS; ++unit)
    {
        CachedTextureBinding& cached = invocation.textures[unit];
        cached.valid = fxgl::get_texture_binding(
            unit, cached.view, cached.sampler);
        cached.nearest_repeat_power_of_two = cached.valid &&
            cached.sampler.filter == gfx::TextureFilter::Nearest &&
            cached.sampler.min_filter == gfx::TextureMinFilter::Nearest &&
            cached.sampler.address_u == gfx::TextureAddressMode::Repeat &&
            cached.sampler.address_v == gfx::TextureAddressMode::Repeat &&
            (cached.view.width &
             static_cast<uint16_t>(cached.view.width - 1)) == 0 &&
            (cached.view.height &
             static_cast<uint16_t>(cached.view.height - 1)) == 0;
        cached.requires_derivatives = !cached.valid ||
            gfx::sampler_requires_implicit_derivatives(cached.sampler);
    }

    bool found_sampler = false;
    for (size_t i = 0; i < module.uniform_count; ++i)
    {
        if (module.uniforms[i].type != fxgl::ShaderType::Sampler2D)
            continue;
        found_sampler = true;

        const uint8_t location = program.fragment_uniform_map[i];
        if (location >= program.uniform_count)
        {
            invocation.varying_derivatives = true;
            continue;
        }

        const float unit_value = program.uniform_values[location].data[0];
        if (!std::isfinite(unit_value) || unit_value < 0.0f ||
            unit_value >= static_cast<float>(FXGL_MAX_TEXTURE_UNITS) ||
            std::floor(unit_value) != unit_value)
        {
            invocation.varying_derivatives = true;
            continue;
        }

        const CachedTextureBinding& cached =
            invocation.textures[static_cast<size_t>(unit_value)];
        if (!cached.valid || cached.requires_derivatives)
            invocation.varying_derivatives = true;
    }

    // An unaccounted-for texture opcode stays on the conservative path.
    if (!found_sampler)
        invocation.varying_derivatives = true;
}

static bool sample_bound_texture(
    int unit,
    float u,
    float v,
    float dudx,
    float dvdx,
    float dudy,
    float dvdy,
    float rgba[4],
    void *user_data)
{
    if (unit < 0 || unit >= static_cast<int>(FXGL_MAX_TEXTURE_UNITS))
        return false;

    DrawInvocation& invocation = *static_cast<DrawInvocation *>(user_data);
    const CachedTextureBinding& cached =
        invocation.textures[static_cast<size_t>(unit)];
    if (!cached.valid)
        return false;
    const gfx::Texture2DView& texture = cached.view;
    const gfx::TextureSampler& sampler = cached.sampler;

    // Texture and sampler properties are invariant throughout this draw.
    // The common 32x32 Repeat/Repeat case uses its preselected sampler before
    // entering the generic per-fragment validation and address-mode branches.
    if (cached.nearest_repeat_power_of_two)
    {
        uint16_t pixel = 0;
        if (gfx::sample_texture_nearest_repeat_pow2_validated(
                texture, u, v, pixel))
        {
            rgba[0] = static_cast<float>((pixel >> 11) & 0x1f) / 31.0f;
            rgba[1] = static_cast<float>((pixel >> 5) & 0x3f) / 63.0f;
            rgba[2] = static_cast<float>(pixel & 0x1f) / 31.0f;
            rgba[3] = 1.0f;
            return true;
        }
    }

    if (!std::isfinite(u) || !std::isfinite(v))
        return false;
    // Non-mipmapped nearest/linear filtering does not consume derivatives.
    // Avoid four finite checks on the common nearest-texture path.
    if (cached.requires_derivatives &&
        (!std::isfinite(dudx) || !std::isfinite(dvdx) ||
         !std::isfinite(dudy) || !std::isfinite(dvdy)))
        return false;

    if (sampler.filter == gfx::TextureFilter::Nearest &&
        sampler.min_filter == gfx::TextureMinFilter::Nearest)
    {
        uint16_t pixel = 0;
        if (!gfx::sample_texture_nearest_validated(
                texture, u, v, sampler, pixel))
            return false;
        rgba[0] = static_cast<float>((pixel >> 11) & 0x1f) / 31.0f;
        rgba[1] = static_cast<float>((pixel >> 5) & 0x3f) / 63.0f;
        rgba[2] = static_cast<float>(pixel & 0x1f) / 31.0f;
        rgba[3] = 1.0f;
        return true;
    }

    float lod = 0.0f;
    if (cached.requires_derivatives)
    {
        const float rho_x = std::sqrt(
            (dudx * texture.width) * (dudx * texture.width) +
            (dvdx * texture.height) * (dvdx * texture.height));
        const float rho_y = std::sqrt(
            (dudy * texture.width) * (dudy * texture.width) +
            (dvdy * texture.height) * (dvdy * texture.height));
        const float rho = std::max(rho_x, rho_y);
        lod = rho > 0.0f ? std::log2(rho) : 0.0f;
    }

    const bool minifying = lod > 0.0f;
    const bool linear_filter = minifying
        ? (sampler.min_filter == gfx::TextureMinFilter::Linear ||
           sampler.min_filter == gfx::TextureMinFilter::LinearMipmapNearest ||
           sampler.min_filter == gfx::TextureMinFilter::LinearMipmapLinear)
        : sampler.filter == gfx::TextureFilter::Bilinear;

    /* Normalized GLES coordinates are converted to texel-space coordinates.
       Linear filtering is centered half a texel between neighboring samples. */
    double texel_u = static_cast<double>(u) * texture.width;
    double texel_v = static_cast<double>(v) * texture.height;
    if (linear_filter)
    {
        texel_u -= 0.5;
        texel_v -= 0.5;
    }

    const double scale = 65536.0;
    const double scaled_u = std::floor(texel_u * scale);
    const double scaled_v = std::floor(texel_v * scale);
    const double minimum = static_cast<double>(
        std::numeric_limits<int64_t>::min());
    const double maximum = static_cast<double>(
        std::numeric_limits<int64_t>::max());
    if (scaled_u < minimum || scaled_u >= maximum ||
        scaled_v < minimum || scaled_v >= maximum)
        return false;

    const uint16_t pixel = gfx::sample_texture_validated(
        texture,
        static_cast<int64_t>(scaled_u),
        static_cast<int64_t>(scaled_v),
        sampler,
        lod);
    rgba[0] = static_cast<float>((pixel >> 11) & 0x1f) / 31.0f;
    rgba[1] = static_cast<float>((pixel >> 5) & 0x3f) / 63.0f;
    rgba[2] = static_cast<float>(pixel & 0x1f) / 31.0f;
    rgba[3] = 1.0f;
    return true;
}

static bool execute_fragment(
    const fxgl::FragmentInput& input,
    float rgba[4],
    void *user_data)
{
    const float *varyings = input.varyings;
    const size_t varying_count = input.varying_count;
    DrawInvocation& invocation = *static_cast<DrawInvocation *>(user_data);
    const fxgl::ShaderModule& module = *invocation.program.fragment_shader;

    // The common Gouraud/unlit fragment form is just
    // "gl_FragColor = varying_vec4;". Bypass per-fragment VM setup and
    // interpretation for this exact, side-effect-free two-op program.
    if (invocation.passthrough_varying_offset >= 0)
    {
        const size_t offset =
            static_cast<size_t>(invocation.passthrough_varying_offset);
        if (!varyings || offset + 4 > varying_count)
        {
            invocation.fragment_failed = true;
            return false;
        }
        for (size_t i = 0; i < 4; ++i)
            rgba[i] = varyings[offset + i];
        return true;
    }

    // Keep one runtime per draw instead of clearing its full maximum-sized
    // storage for every covered fragment. Locals and output validity are
    // reset here; all declared uniforms and varyings are refreshed below.
    fxgl::ShaderRuntime& runtime = invocation.fragment_runtime;
    fxgl::prepare_shader_runtime(module, runtime);

    for (size_t i = 0; i < module.varying_count; ++i)
    {
        const fxgl::ShaderVariable& variable = module.varyings[i];
        const size_t components = shader_components(variable.type);
        const size_t offset = invocation.program.fragment_varying_offsets[i];
        if (components == 0 || offset + components > varying_count)
        {
            invocation.fragment_failed = true;
            return false;
        }
        // Every component consumed by this declared type is overwritten
        // below, including its derivatives. Do not clear the full mat4-sized
        // ShaderValue for each fragment when this is usually only a vec2.
        runtime.varyings[i].type = variable.type;
        for (size_t component = 0; component < components; ++component)
        {
            runtime.varyings[i].data[component] = varyings[offset + component];
            runtime.varyings[i].ddx[component] =
                input.varying_ddx ? input.varying_ddx[offset + component] : 0.0f;
            runtime.varyings[i].ddy[component] =
                input.varying_ddy ? input.varying_ddy[offset + component] : 0.0f;
        }
    }

    if (!fxgl::execute_glsl(module, runtime, sample_bound_texture, &invocation) ||
        runtime.fragment_color.type != fxgl::ShaderType::Vec4)
    {
        invocation.fragment_failed = true;
        return false;
    }

    for (size_t i = 0; i < 4; ++i)
        rgba[i] = runtime.fragment_color.data[i];
    return true;
}

static bool execute_vertex(
    const fxgl::LinkedProgramView& program,
    uint32_t vertex_index,
    fxgl::ClipVertex& output)
{
    const fxgl::ShaderModule& module = *program.vertex_shader;
    fxgl::ShaderRuntime runtime = {};

    for (size_t i = 0; i < module.attribute_count; ++i)
    {
        const uint8_t location = program.attribute_locations[i];
        if (!fetch_vertex_attribute(
                location, vertex_index, module.attributes[i].type,
                runtime.attributes[i]))
        {
            set_error(GL_INVALID_OPERATION);
            return false;
        }
    }

    for (size_t i = 0; i < module.uniform_count; ++i)
    {
        const uint8_t location = program.vertex_uniform_map[i];
        if (location >= program.uniform_count)
        {
            set_error(GL_INVALID_OPERATION);
            return false;
        }
        runtime.uniforms[i] = program.uniform_values[location];
    }

    if (!fxgl::execute_glsl(module, runtime) ||
        runtime.position.type != fxgl::ShaderType::Vec4)
    {
        set_error(GL_INVALID_OPERATION);
        return false;
    }

    output = {};
    output.position = {
        runtime.position.data[0],
        runtime.position.data[1],
        runtime.position.data[2],
        runtime.position.data[3]
    };

    for (size_t i = 0; i < module.varying_count; ++i)
    {
        const fxgl::ShaderVariable& variable = module.varyings[i];
        const size_t components = shader_components(variable.type);
        const size_t offset = program.vertex_varying_offsets[i];
        if (components == 0 ||
            offset + components > fxgl::GLSL_MAX_VARYINGS)
        {
            set_error(GL_INVALID_OPERATION);
            return false;
        }
        for (size_t component = 0; component < components; ++component)
            output.varying[offset + component] =
                runtime.varyings[i].data[component];
    }
    return true;
}

static bool draw_vertex_triangle(
    DrawInvocation& invocation,
    uint32_t first,
    uint32_t second,
    uint32_t third)
{
    const fxgl::LinkedProgramView& program = invocation.program;
    fxgl::ClipVertex vertices[3] = {};
    if (!execute_vertex(program, first, vertices[0]) ||
        !execute_vertex(program, second, vertices[1]) ||
        !execute_vertex(program, third, vertices[2]))
        return false;

    fxgl::draw_clip_triangle(
        vertices,
        program.varying_scalar_count,
        execute_fragment,
        &invocation,
        nullptr,
        invocation.varying_derivatives);
    if (invocation.fragment_failed)
    {
        set_error(GL_INVALID_OPERATION);
        return false;
    }
    return true;
}

static bool draw_vertex_point(
    DrawInvocation& invocation,
    uint32_t index)
{
    const fxgl::LinkedProgramView& program = invocation.program;
    fxgl::ClipVertex vertex = {};
    if (!execute_vertex(program, index, vertex))
        return false;

    fxgl::draw_clip_point(
        vertex,
        program.varying_scalar_count,
        execute_fragment,
        &invocation,
        nullptr,
        invocation.varying_derivatives);
    if (invocation.fragment_failed)
    {
        set_error(GL_INVALID_OPERATION);
        return false;
    }
    return true;
}

static bool draw_vertex_line(
    DrawInvocation& invocation,
    uint32_t first,
    uint32_t second)
{
    const fxgl::LinkedProgramView& program = invocation.program;
    fxgl::ClipVertex vertices[2] = {};
    if (!execute_vertex(program, first, vertices[0]) ||
        !execute_vertex(program, second, vertices[1]))
        return false;

    fxgl::draw_clip_line(
        vertices,
        program.varying_scalar_count,
        execute_fragment,
        &invocation,
        nullptr,
        invocation.varying_derivatives);
    if (invocation.fragment_failed)
    {
        set_error(GL_INVALID_OPERATION);
        return false;
    }
    return true;
}

static bool valid_draw_mode(GLenum mode)
{
    return mode == GL_POINTS || mode == GL_LINES ||
           mode == GL_LINE_LOOP || mode == GL_LINE_STRIP ||
           mode == GL_TRIANGLES || mode == GL_TRIANGLE_STRIP ||
           mode == GL_TRIANGLE_FAN;
}

static void clear_color_buffer(const fxgl_surface_t& surface, GLint x0, GLint y0, GLint x1, GLint y1)
{
    const uint16_t clear_pixel = pack_rgb565(g_context.clear_color);
    const uint16_t red_mask = 0xf800;
    const uint16_t green_mask = 0x07e0;
    const uint16_t blue_mask = 0x001f;

    for (GLint y = y0; y < y1; ++y)
    {
        uint16_t *row = surface.color +
            static_cast<size_t>(y) * surface.stride;
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
        destroy_framebuffer_resources(g_context);
    }
    fxgl::program_context_reset();

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

GLboolean fxglUpdateSurface(const fxgl_surface_t *surface)
{
    if (!g_context.current || !surface || !surface->color ||
        surface->width != g_context.surface.width ||
        surface->height != g_context.surface.height ||
        surface->stride != g_context.surface.stride)
        return GL_FALSE;

    // Retain the new caller-owned addresses without destroying resources,
    // object names, uniforms, vertex attributes, or render state.
    g_context.surface = *surface;
    return GL_TRUE;
}

void fxglReleaseCurrent(void)
{
    destroy_buffers(g_context);
    destroy_textures(g_context);
    destroy_framebuffer_resources(g_context);
    fxgl::program_context_reset();
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

void glVertexAttrib1f(GLuint index, GLfloat x)
{
    if (!has_context()) return;
    if (index >= FXGL_MAX_VERTEX_ATTRIBS)
    {
        set_error(GL_INVALID_VALUE);
        return;
    }
    VertexAttribute& attribute = g_context.attributes[index];
    attribute.current[0] = x;
    attribute.current[1] = 0.0f;
    attribute.current[2] = 0.0f;
    attribute.current[3] = 1.0f;
}

void glVertexAttrib2f(GLuint index, GLfloat x, GLfloat y)
{
    if (!has_context()) return;
    if (index >= FXGL_MAX_VERTEX_ATTRIBS)
    {
        set_error(GL_INVALID_VALUE);
        return;
    }
    VertexAttribute& attribute = g_context.attributes[index];
    attribute.current[0] = x;
    attribute.current[1] = y;
    attribute.current[2] = 0.0f;
    attribute.current[3] = 1.0f;
}

void glVertexAttrib3f(GLuint index, GLfloat x, GLfloat y, GLfloat z)
{
    if (!has_context()) return;
    if (index >= FXGL_MAX_VERTEX_ATTRIBS)
    {
        set_error(GL_INVALID_VALUE);
        return;
    }
    VertexAttribute& attribute = g_context.attributes[index];
    attribute.current[0] = x;
    attribute.current[1] = y;
    attribute.current[2] = z;
    attribute.current[3] = 1.0f;
}

void glVertexAttrib4f(
    GLuint index,
    GLfloat x,
    GLfloat y,
    GLfloat z,
    GLfloat w)
{
    if (!has_context()) return;
    if (index >= FXGL_MAX_VERTEX_ATTRIBS)
    {
        set_error(GL_INVALID_VALUE);
        return;
    }
    VertexAttribute& attribute = g_context.attributes[index];
    attribute.current[0] = x;
    attribute.current[1] = y;
    attribute.current[2] = z;
    attribute.current[3] = w;
}

void glVertexAttrib4fv(GLuint index, const GLfloat *values)
{
    if (!has_context()) return;
    if (index >= FXGL_MAX_VERTEX_ATTRIBS)
    {
        set_error(GL_INVALID_VALUE);
        return;
    }
    if (!values)
    {
        set_error(GL_INVALID_VALUE);
        return;
    }
    VertexAttribute& attribute = g_context.attributes[index];
    for (int i = 0; i < 4; ++i)
        attribute.current[i] = values[i];
}

void glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
    if (!has_context()) return;
    if (!valid_draw_mode(mode))
    {
        set_error(GL_INVALID_ENUM);
        return;
    }
    if (first < 0 || count < 0)
    {
        set_error(GL_INVALID_VALUE);
        return;
    }
    if (count == 0)
        return;

    fxgl::LinkedProgramView program = {};
    if (!fxgl::get_current_program_view(program))
    {
        set_error(GL_INVALID_OPERATION);
        return;
    }

    const uint64_t first_wide = static_cast<uint32_t>(first);
    const uint64_t final_vertex = first_wide +
        static_cast<uint64_t>(static_cast<uint32_t>(count - 1));
    if (final_vertex > std::numeric_limits<uint32_t>::max())
    {
        set_error(GL_INVALID_VALUE);
        return;
    }
    const uint32_t base = static_cast<uint32_t>(first);

    DrawInvocation invocation = {};
    initialize_draw_invocation(invocation, program);

    if (mode == GL_POINTS)
    {
        for (GLsizei i = 0; i < count; ++i)
            if (!draw_vertex_point(invocation, base + static_cast<uint32_t>(i)))
                return;
    }
    else if (mode == GL_LINES)
    {
        for (GLsizei i = 0; i < count - 1; i += 2)
        {
            if (!draw_vertex_line(invocation,
                    base + static_cast<uint32_t>(i),
                    base + static_cast<uint32_t>(i + 1)))
                return;
        }
    }
    else if (mode == GL_LINE_STRIP || mode == GL_LINE_LOOP)
    {
        for (GLsizei i = 0; i < count - 1; ++i)
        {
            if (!draw_vertex_line(invocation,
                    base + static_cast<uint32_t>(i),
                    base + static_cast<uint32_t>(i + 1)))
                return;
        }
        if (mode == GL_LINE_LOOP && count > 1)
        {
            if (!draw_vertex_line(invocation,
                    base + static_cast<uint32_t>(count - 1), base))
                return;
        }
    }
    else if (mode == GL_TRIANGLES)
    {
        for (GLsizei i = 0; i < count - 2; i += 3)
        {
            if (!draw_vertex_triangle(invocation,
                    base + static_cast<uint32_t>(i),
                    base + static_cast<uint32_t>(i + 1),
                    base + static_cast<uint32_t>(i + 2)))
                return;
        }
    }
    else if (mode == GL_TRIANGLE_STRIP)
    {
        for (GLsizei i = 0; i < count - 2; ++i)
        {
            const uint32_t a = base + static_cast<uint32_t>(i);
            const uint32_t b = base + static_cast<uint32_t>(i + 1);
            const uint32_t d = base + static_cast<uint32_t>(i + 2);
            if ((i & 1) == 0)
            {
                if (!draw_vertex_triangle(invocation, a, b, d))
                    return;
            }
            else if (!draw_vertex_triangle(invocation, b, a, d))
                return;
        }
    }
    else /* GL_TRIANGLE_FAN */
    {
        for (GLsizei i = 1; i < count - 1; ++i)
        {
            if (!draw_vertex_triangle(invocation,
                    base,
                    base + static_cast<uint32_t>(i),
                    base + static_cast<uint32_t>(i + 1)))
                return;
        }
    }
}

void glDrawElements(
    GLenum mode,
    GLsizei count,
    GLenum type,
    const GLvoid *indices)
{
    if (!has_context()) return;
    if (!valid_draw_mode(mode))
    {
        set_error(GL_INVALID_ENUM);
        return;
    }
    if (count < 0)
    {
        set_error(GL_INVALID_VALUE);
        return;
    }
    const size_t index_bytes =
        type == GL_UNSIGNED_BYTE ? 1 :
        type == GL_UNSIGNED_SHORT ? 2 : 0;
    if (index_bytes == 0)
    {
        set_error(GL_INVALID_ENUM);
        return;
    }
    if (count == 0)
        return;

    fxgl::LinkedProgramView program = {};
    if (!fxgl::get_current_program_view(program))
    {
        set_error(GL_INVALID_OPERATION);
        return;
    }

    const uint64_t byte_count =
        static_cast<uint64_t>(static_cast<uint32_t>(count)) * index_bytes;
    if (byte_count > std::numeric_limits<size_t>::max())
    {
        set_error(GL_INVALID_VALUE);
        return;
    }

    const uint8_t *index_data = nullptr;
    if (g_context.element_buffer_binding != 0)
    {
        BufferObject *buffer = find_buffer(g_context.element_buffer_binding);
        if (!buffer)
        {
            set_error(GL_INVALID_OPERATION);
            return;
        }
        const uintptr_t offset = reinterpret_cast<uintptr_t>(indices);
        if (offset > buffer->size ||
            static_cast<size_t>(byte_count) > buffer->size - offset)
        {
            set_error(GL_INVALID_OPERATION);
            return;
        }
        index_data = buffer->data + offset;
    }
    else
    {
        if (!indices)
        {
            set_error(GL_INVALID_VALUE);
            return;
        }
        index_data = static_cast<const uint8_t *>(indices);
    }

    const auto get_index = [index_data, index_bytes, type](GLsizei i) -> uint32_t
    {
        if (type == GL_UNSIGNED_BYTE)
            return index_data[static_cast<size_t>(i)];
        uint16_t value = 0;
        std::memcpy(&value, index_data + static_cast<size_t>(i) * index_bytes,
                    sizeof(value));
        return value;
    };

    DrawInvocation invocation = {};
    initialize_draw_invocation(invocation, program);

    if (mode == GL_POINTS)
    {
        for (GLsizei i = 0; i < count; ++i)
            if (!draw_vertex_point(invocation, get_index(i)))
                return;
    }
    else if (mode == GL_LINES)
    {
        for (GLsizei i = 0; i < count - 1; i += 2)
        {
            if (!draw_vertex_line(invocation, get_index(i), get_index(i + 1)))
                return;
        }
    }
    else if (mode == GL_LINE_STRIP || mode == GL_LINE_LOOP)
    {
        for (GLsizei i = 0; i < count - 1; ++i)
            if (!draw_vertex_line(invocation, get_index(i), get_index(i + 1)))
                return;
        if (mode == GL_LINE_LOOP && count > 1)
            if (!draw_vertex_line(invocation, get_index(count - 1), get_index(0)))
                return;
    }
    else if (mode == GL_TRIANGLES)
    {
        for (GLsizei i = 0; i < count - 2; i += 3)
        {
            if (!draw_vertex_triangle(
                    invocation, get_index(i), get_index(i + 1), get_index(i + 2)))
                return;
        }
    }
    else if (mode == GL_TRIANGLE_STRIP)
    {
        for (GLsizei i = 0; i < count - 2; ++i)
        {
            const uint32_t a = get_index(i);
            const uint32_t b = get_index(i + 1);
            const uint32_t d = get_index(i + 2);
            if ((i & 1) == 0)
            {
                if (!draw_vertex_triangle(invocation, a, b, d))
                    return;
            }
            else if (!draw_vertex_triangle(invocation, b, a, d))
                return;
        }
    }
    else /* GL_TRIANGLE_FAN */
    {
        for (GLsizei i = 1; i < count - 1; ++i)
            if (!draw_vertex_triangle(
                    invocation, get_index(0), get_index(i), get_index(i + 1)))
                return;
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

        for (FramebufferObject& framebuffer : g_context.framebuffers)
        {
            FramebufferAttachment *attachments[3] = {
                &framebuffer.color, &framebuffer.depth, &framebuffer.stencil
            };
            for (FramebufferAttachment *attachment : attachments)
                if (attachment->object_type == GL_TEXTURE &&
                    attachment->object_name == name)
                    *attachment = {};
        }

        if (object->alive)
        {
            g_context.texture_bytes -= object->bytes;
            std::free(object->pixels);
            release_texture_mips(*object);
        }
        *object = {};
    }
}

GLboolean glIsFramebuffer(GLuint framebuffer)
{
    if (!has_context()) return GL_FALSE;
    return find_framebuffer(framebuffer) ? GL_TRUE : GL_FALSE;
}

void glGenFramebuffers(GLsizei n, GLuint *framebuffers)
{
    if (!has_context()) return;
    if (n < 0 || (n > 0 && !framebuffers))
    {
        set_error(GL_INVALID_VALUE);
        return;
    }

    size_t available = 0;
    for (const FramebufferObject& object : g_context.framebuffers)
        if (!object.reserved) ++available;
    if (available < static_cast<size_t>(n))
    {
        set_error(GL_OUT_OF_MEMORY);
        return;
    }

    GLsizei generated = 0;
    for (size_t i = 0; i < FXGL_MAX_FRAMEBUFFERS && generated < n; ++i)
    {
        if (!g_context.framebuffers[i].reserved)
        {
            g_context.framebuffers[i] = {};
            g_context.framebuffers[i].reserved = true;
            framebuffers[generated++] = static_cast<GLuint>(i + 1);
        }
    }
}

void glDeleteFramebuffers(GLsizei n, const GLuint *framebuffers)
{
    if (!has_context()) return;
    if (n < 0 || (n > 0 && !framebuffers))
    {
        set_error(GL_INVALID_VALUE);
        return;
    }

    for (GLsizei i = 0; i < n; ++i)
    {
        FramebufferObject *object = framebuffer_slot(framebuffers[i]);
        if (!object)
            continue;
        if (g_context.framebuffer_binding == framebuffers[i])
            g_context.framebuffer_binding = 0;
        *object = {};
    }
}

void glBindFramebuffer(GLenum target, GLuint framebuffer)
{
    if (!has_context()) return;
    if (target != GL_FRAMEBUFFER)
    {
        set_error(GL_INVALID_ENUM);
        return;
    }
    if (framebuffer != 0)
    {
        FramebufferObject *object = framebuffer_slot(framebuffer);
        if (!object)
        {
            set_error(GL_INVALID_OPERATION);
            return;
        }
        object->alive = true;
    }
    g_context.framebuffer_binding = framebuffer;
}

void glFramebufferTexture2D(
    GLenum target,
    GLenum attachment,
    GLenum textarget,
    GLuint texture,
    GLint level)
{
    if (!has_context()) return;
    if (target != GL_FRAMEBUFFER || textarget != GL_TEXTURE_2D)
    {
        set_error(GL_INVALID_ENUM);
        return;
    }
    if (level < 0)
    {
        set_error(GL_INVALID_VALUE);
        return;
    }
    if (attachment != GL_COLOR_ATTACHMENT0 &&
        attachment != GL_DEPTH_ATTACHMENT &&
        attachment != GL_STENCIL_ATTACHMENT)
    {
        set_error(GL_INVALID_ENUM);
        return;
    }
    if (g_context.framebuffer_binding == 0)
    {
        set_error(GL_INVALID_OPERATION);
        return;
    }
    if (texture != 0 && !find_texture(texture))
    {
        set_error(GL_INVALID_OPERATION);
        return;
    }

    FramebufferAttachment *destination = bound_attachment(attachment);
    if (!destination) return;
    if (texture == 0)
        *destination = {};
    else
        *destination = {GL_TEXTURE, texture, level};
}

void glFramebufferRenderbuffer(
    GLenum target,
    GLenum attachment,
    GLenum renderbuffertarget,
    GLuint renderbuffer)
{
    if (!has_context()) return;
    if (target != GL_FRAMEBUFFER || renderbuffertarget != GL_RENDERBUFFER)
    {
        set_error(GL_INVALID_ENUM);
        return;
    }
    if (attachment != GL_COLOR_ATTACHMENT0 &&
        attachment != GL_DEPTH_ATTACHMENT &&
        attachment != GL_STENCIL_ATTACHMENT)
    {
        set_error(GL_INVALID_ENUM);
        return;
    }
    if (g_context.framebuffer_binding == 0)
    {
        set_error(GL_INVALID_OPERATION);
        return;
    }
    if (renderbuffer != 0 && !find_renderbuffer(renderbuffer))
    {
        set_error(GL_INVALID_OPERATION);
        return;
    }

    FramebufferAttachment *destination = bound_attachment(attachment);
    if (!destination) return;
    if (renderbuffer == 0)
        *destination = {};
    else
        *destination = {GL_RENDERBUFFER, renderbuffer, 0};
}

GLenum glCheckFramebufferStatus(GLenum target)
{
    if (!has_context()) return 0;
    if (target != GL_FRAMEBUFFER)
    {
        set_error(GL_INVALID_ENUM);
        return 0;
    }
    return framebuffer_status();
}

void glGetFramebufferAttachmentParameteriv(
    GLenum target,
    GLenum attachment,
    GLenum pname,
    GLint *params)
{
    if (!has_context()) return;
    if (target != GL_FRAMEBUFFER)
    {
        set_error(GL_INVALID_ENUM);
        return;
    }
    if (!params)
    {
        set_error(GL_INVALID_VALUE);
        return;
    }
    if (attachment != GL_COLOR_ATTACHMENT0 &&
        attachment != GL_DEPTH_ATTACHMENT &&
        attachment != GL_STENCIL_ATTACHMENT)
    {
        set_error(GL_INVALID_ENUM);
        return;
    }
    if (g_context.framebuffer_binding == 0)
    {
        set_error(GL_INVALID_OPERATION);
        return;
    }

    FramebufferAttachment *source = bound_attachment(attachment);
    if (!source) return;
    switch (pname)
    {
        case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
            *params = static_cast<GLint>(source->object_type);
            break;
        case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
            *params = static_cast<GLint>(source->object_name);
            break;
        case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
            if (source->object_type != GL_TEXTURE)
            {
                set_error(GL_INVALID_ENUM);
                return;
            }
            *params = source->texture_level;
            break;
        case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE:
            if (source->object_type != GL_TEXTURE)
            {
                set_error(GL_INVALID_ENUM);
                return;
            }
            *params = 0;
            break;
        default:
            set_error(GL_INVALID_ENUM);
            break;
    }
}

GLboolean glIsRenderbuffer(GLuint renderbuffer)
{
    if (!has_context()) return GL_FALSE;
    return find_renderbuffer(renderbuffer) ? GL_TRUE : GL_FALSE;
}

void glGenRenderbuffers(GLsizei n, GLuint *renderbuffers)
{
    if (!has_context()) return;
    if (n < 0 || (n > 0 && !renderbuffers))
    {
        set_error(GL_INVALID_VALUE);
        return;
    }

    size_t available = 0;
    for (const RenderbufferObject& object : g_context.renderbuffers)
        if (!object.reserved) ++available;
    if (available < static_cast<size_t>(n))
    {
        set_error(GL_OUT_OF_MEMORY);
        return;
    }

    GLsizei generated = 0;
    for (size_t i = 0; i < FXGL_MAX_RENDERBUFFERS && generated < n; ++i)
    {
        if (!g_context.renderbuffers[i].reserved)
        {
            g_context.renderbuffers[i] = {};
            g_context.renderbuffers[i].reserved = true;
            renderbuffers[generated++] = static_cast<GLuint>(i + 1);
        }
    }
}

void glDeleteRenderbuffers(GLsizei n, const GLuint *renderbuffers)
{
    if (!has_context()) return;
    if (n < 0 || (n > 0 && !renderbuffers))
    {
        set_error(GL_INVALID_VALUE);
        return;
    }

    for (GLsizei i = 0; i < n; ++i)
    {
        RenderbufferObject *object = renderbuffer_slot(renderbuffers[i]);
        if (!object)
            continue;

        if (g_context.renderbuffer_binding == renderbuffers[i])
            g_context.renderbuffer_binding = 0;

        for (FramebufferObject& framebuffer : g_context.framebuffers)
        {
            FramebufferAttachment *attachments[3] = {
                &framebuffer.color, &framebuffer.depth, &framebuffer.stencil
            };
            for (FramebufferAttachment *attachment : attachments)
            {
                if (attachment->object_type == GL_RENDERBUFFER &&
                    attachment->object_name == renderbuffers[i])
                    *attachment = {};
            }
        }

        if (object->alive)
        {
            g_context.renderbuffer_bytes -= object->bytes;
            std::free(object->storage);
        }
        *object = {};
    }
}

void glBindRenderbuffer(GLenum target, GLuint renderbuffer)
{
    if (!has_context()) return;
    if (target != GL_RENDERBUFFER)
    {
        set_error(GL_INVALID_ENUM);
        return;
    }
    if (renderbuffer != 0)
    {
        RenderbufferObject *object = renderbuffer_slot(renderbuffer);
        if (!object)
        {
            set_error(GL_INVALID_OPERATION);
            return;
        }
        object->alive = true;
    }
    g_context.renderbuffer_binding = renderbuffer;
}

void glRenderbufferStorage(
    GLenum target,
    GLenum internalformat,
    GLsizei width,
    GLsizei height)
{
    if (!has_context()) return;
    if (target != GL_RENDERBUFFER)
    {
        set_error(GL_INVALID_ENUM);
        return;
    }
    if (width < 0 || height < 0 ||
        width > FXGL_MAX_RENDERBUFFER_SIZE ||
        height > FXGL_MAX_RENDERBUFFER_SIZE)
    {
        set_error(GL_INVALID_VALUE);
        return;
    }

    size_t bytes_per_pixel = 0;
    if (internalformat == GL_RGB565 ||
        internalformat == GL_DEPTH_COMPONENT16)
        bytes_per_pixel = sizeof(uint16_t);
    else if (internalformat == GL_STENCIL_INDEX8)
        bytes_per_pixel = sizeof(uint8_t);
    else
    {
        set_error(GL_INVALID_ENUM);
        return;
    }

    RenderbufferObject *object =
        find_renderbuffer(g_context.renderbuffer_binding);
    if (!object)
    {
        set_error(GL_INVALID_OPERATION);
        return;
    }

    const size_t pixels =
        static_cast<size_t>(width) * static_cast<size_t>(height);
    const size_t new_bytes = pixels * bytes_per_pixel;
    const size_t retained = g_context.renderbuffer_bytes - object->bytes;
    if (new_bytes > FXGL_MAX_RENDERBUFFER_BYTES - retained)
    {
        set_error(GL_OUT_OF_MEMORY);
        return;
    }

    uint8_t *storage = nullptr;
    if (new_bytes > 0)
    {
        storage = static_cast<uint8_t *>(std::malloc(new_bytes));
        if (!storage)
        {
            set_error(GL_OUT_OF_MEMORY);
            return;
        }
        std::memset(storage, 0, new_bytes);
    }

    std::free(object->storage);
    object->storage = storage;
    object->bytes = new_bytes;
    object->width = width;
    object->height = height;
    object->internal_format = internalformat;
    g_context.renderbuffer_bytes = retained + new_bytes;
}

void glGetRenderbufferParameteriv(
    GLenum target,
    GLenum pname,
    GLint *params)
{
    if (!has_context()) return;
    if (target != GL_RENDERBUFFER)
    {
        set_error(GL_INVALID_ENUM);
        return;
    }
    if (!params)
    {
        set_error(GL_INVALID_VALUE);
        return;
    }
    RenderbufferObject *object =
        find_renderbuffer(g_context.renderbuffer_binding);
    if (!object)
    {
        set_error(GL_INVALID_OPERATION);
        return;
    }

    switch (pname)
    {
        case GL_RENDERBUFFER_WIDTH:
            *params = object->width;
            break;
        case GL_RENDERBUFFER_HEIGHT:
            *params = object->height;
            break;
        case GL_RENDERBUFFER_INTERNAL_FORMAT:
            *params = static_cast<GLint>(object->internal_format);
            break;
        case GL_RENDERBUFFER_RED_SIZE:
            *params = object->internal_format == GL_RGB565 ? 5 : 0;
            break;
        case GL_RENDERBUFFER_GREEN_SIZE:
            *params = object->internal_format == GL_RGB565 ? 6 : 0;
            break;
        case GL_RENDERBUFFER_BLUE_SIZE:
            *params = object->internal_format == GL_RGB565 ? 5 : 0;
            break;
        case GL_RENDERBUFFER_ALPHA_SIZE:
            *params = 0;
            break;
        case GL_RENDERBUFFER_DEPTH_SIZE:
            *params = object->internal_format == GL_DEPTH_COMPONENT16 ? 16 : 0;
            break;
        case GL_RENDERBUFFER_STENCIL_SIZE:
            *params = object->internal_format == GL_STENCIL_INDEX8 ? 8 : 0;
            break;
        default:
            set_error(GL_INVALID_ENUM);
            break;
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
            switch (param)
            {
                case GL_NEAREST:
                case GL_LINEAR:
                case GL_NEAREST_MIPMAP_NEAREST:
                case GL_LINEAR_MIPMAP_NEAREST:
                case GL_NEAREST_MIPMAP_LINEAR:
                case GL_LINEAR_MIPMAP_LINEAR:
                    object->min_filter = static_cast<GLenum>(param);
                    break;
                default:
                    set_error(GL_INVALID_ENUM);
                    break;
            }
            break;

        case GL_TEXTURE_MAG_FILTER:
            if (param != GL_NEAREST && param != GL_LINEAR)
            {
                set_error(GL_INVALID_ENUM);
                return;
            }
            object->mag_filter = static_cast<GLenum>(param);
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
    if (level < 0 || level >= static_cast<GLint>(FXGL_MAX_TEXTURE_LEVELS) ||
        width < 0 || height < 0 || border != 0 ||
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

    if (level > 0)
    {
        if (!object->pixels || object->width == 0 || object->height == 0)
        {
            set_error(GL_INVALID_OPERATION);
            return;
        }
        if (level > texture_max_level(object->width, object->height))
        {
            set_error(GL_INVALID_VALUE);
            return;
        }
        GLsizei expected_width = object->width;
        GLsizei expected_height = object->height;
        for (GLint i = 0; i < level; ++i)
        {
            expected_width = std::max<GLsizei>(1, expected_width / 2);
            expected_height = std::max<GLsizei>(1, expected_height / 2);
        }
        if (width != expected_width || height != expected_height)
        {
            set_error(GL_INVALID_VALUE);
            return;
        }
    }

    const size_t new_bytes = texture_level_bytes(
        static_cast<uint16_t>(width), static_cast<uint16_t>(height));
    TextureMipObject *mip = level > 0
        ? &object->mip_levels[level - 1] : nullptr;
    const size_t old_bytes = level > 0 ? mip->bytes : object->bytes;
    const size_t retained = g_context.texture_bytes - old_bytes;
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
                std::memcpy(new_pixels + static_cast<size_t>(row) * width,
                    source + static_cast<size_t>(row) * source_stride, row_bytes);
        }
        else
        {
            std::memset(new_pixels, 0, new_bytes);
        }
    }

    if (level == 0)
    {
        // Replacing the base image invalidates the entire lower mip chain.
        std::free(object->pixels);
        release_texture_mips(*object);
        object->pixels = new_pixels;
        object->width = static_cast<uint16_t>(width);
        object->height = static_cast<uint16_t>(height);
        object->bytes = new_bytes;
        g_context.texture_bytes = retained + new_bytes;
    }
    else
    {
        std::free(mip->pixels);
        mip->pixels = new_pixels;
        mip->width = static_cast<uint16_t>(width);
        mip->height = static_cast<uint16_t>(height);
        mip->bytes = new_bytes;
        object->bytes = object->bytes - old_bytes + new_bytes;
        g_context.texture_bytes = retained + new_bytes;
    }
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
    if (level < 0 || level >= static_cast<GLint>(FXGL_MAX_TEXTURE_LEVELS) ||
        xoffset < 0 || yoffset < 0 || width < 0 || height < 0)
    {
        set_error(GL_INVALID_VALUE);
        return;
    }

    TextureObject *object = bound_texture(target);
    if (!object) return;

    uint16_t *destination_pixels = object->pixels;
    GLsizei level_width = object->width;
    GLsizei level_height = object->height;
    if (level > 0)
    {
        const TextureMipObject& mip = object->mip_levels[level - 1];
        destination_pixels = mip.pixels;
        level_width = mip.width;
        level_height = mip.height;
    }

    const int64_t right = static_cast<int64_t>(xoffset) + width;
    const int64_t bottom = static_cast<int64_t>(yoffset) + height;
    if (!destination_pixels || right > level_width || bottom > level_height)
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
        uint16_t *destination = destination_pixels +
            static_cast<size_t>(yoffset + row) * level_width + xoffset;
        std::memcpy(destination,
            source + static_cast<size_t>(row) * source_stride, row_bytes);
    }
}

void glGenerateMipmap(GLenum target)
{
    if (!has_context()) return;
    if (target != GL_TEXTURE_2D)
    {
        set_error(GL_INVALID_ENUM);
        return;
    }

    TextureObject *object = bound_texture(target);
    if (!object) return;
    if (!object->pixels || object->width == 0 || object->height == 0)
    {
        set_error(GL_INVALID_OPERATION);
        return;
    }

    TextureMipObject generated[FXGL_MAX_TEXTURE_LEVELS - 1] = {};
    const size_t retained = g_context.texture_bytes - object->bytes;
    size_t generated_bytes = 0;
    const uint16_t *source_pixels = object->pixels;
    uint16_t source_width = object->width;
    uint16_t source_height = object->height;

    for (size_t level = 1; level < FXGL_MAX_TEXTURE_LEVELS; ++level)
    {
        if (source_width == 1 && source_height == 1)
            break;
        const uint16_t width = source_width > 1
            ? static_cast<uint16_t>(source_width / 2) : 1;
        const uint16_t height = source_height > 1
            ? static_cast<uint16_t>(source_height / 2) : 1;
        const size_t bytes = texture_level_bytes(width, height);
        if (bytes > FXGL_MAX_TEXTURE_BYTES - retained - generated_bytes)
        {
            for (TextureMipObject& mip : generated)
                std::free(mip.pixels);
            set_error(GL_OUT_OF_MEMORY);
            return;
        }

        uint16_t *destination = static_cast<uint16_t *>(std::malloc(bytes));
        if (!destination)
        {
            for (TextureMipObject& mip : generated)
                std::free(mip.pixels);
            set_error(GL_OUT_OF_MEMORY);
            return;
        }

        for (uint16_t y = 0; y < height; ++y)
        {
            for (uint16_t x = 0; x < width; ++x)
            {
                unsigned red = 0;
                unsigned green = 0;
                unsigned blue = 0;
                for (unsigned dy = 0; dy < 2; ++dy)
                {
                    for (unsigned dx = 0; dx < 2; ++dx)
                    {
                        const uint16_t sx = std::min<uint16_t>(
                            static_cast<uint16_t>(x * 2 + dx),
                            static_cast<uint16_t>(source_width - 1));
                        const uint16_t sy = std::min<uint16_t>(
                            static_cast<uint16_t>(y * 2 + dy),
                            static_cast<uint16_t>(source_height - 1));
                        const uint16_t pixel =
                            source_pixels[static_cast<size_t>(sy) * source_width + sx];
                        red += (pixel >> 11) & 0x1f;
                        green += (pixel >> 5) & 0x3f;
                        blue += pixel & 0x1f;
                    }
                }
                destination[static_cast<size_t>(y) * width + x] =
                    static_cast<uint16_t>(
                        (((red + 2) / 4) << 11) |
                        (((green + 2) / 4) << 5) |
                        ((blue + 2) / 4));
            }
        }

        generated[level - 1] = {destination, width, height, bytes};
        generated_bytes += bytes;
        source_pixels = destination;
        source_width = width;
        source_height = height;
    }

    release_texture_mips(*object);
    for (size_t i = 0; i < FXGL_MAX_TEXTURE_LEVELS - 1; ++i)
        object->mip_levels[i] = generated[i];
    object->bytes = texture_level_bytes(object->width, object->height) +
                    generated_bytes;
    g_context.texture_bytes = retained + object->bytes;
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

    fxgl_surface_t surface = {};
    if (!resolve_draw_surface(surface))
    {
        set_error(GL_INVALID_FRAMEBUFFER_OPERATION);
        return;
    }

    GLint rect[4] = {0, 0, surface.width, surface.height};
    GLint bounds[4] = {0, 0, surface.width, surface.height};
    if (g_context.scissor_test)
        clear_rect(surface, g_context.scissor, bounds);
    else
        clear_rect(surface, rect, bounds);

    if (bounds[0] >= bounds[2] || bounds[1] >= bounds[3])
        return;

    if ((mask & GL_COLOR_BUFFER_BIT) && surface.color)
        clear_color_buffer(surface, bounds[0], bounds[1], bounds[2], bounds[3]);

    if ((mask & GL_DEPTH_BUFFER_BIT) && surface.depth && g_context.depth_mask)
    {
        const uint16_t depth = static_cast<uint16_t>(
            g_context.clear_depth * 65535.0f + 0.5f);
        for (GLint y = bounds[1]; y < bounds[3]; ++y)
        {
            uint16_t *row = surface.depth +
                static_cast<size_t>(y) * surface.stride;
            for (GLint x = bounds[0]; x < bounds[2]; ++x)
                row[x] = depth;
        }
    }

    if ((mask & GL_STENCIL_BUFFER_BIT) && surface.stencil)
    {
        const uint8_t clear_value = static_cast<uint8_t>(g_context.clear_stencil);
        const uint8_t write_mask = static_cast<uint8_t>(g_context.stencil_mask);
        for (GLint y = bounds[1]; y < bounds[3]; ++y)
        {
            uint8_t *row = surface.stencil +
                static_cast<size_t>(y) * surface.stride;
            for (GLint x = bounds[0]; x < bounds[2]; ++x)
            {
                row[x] = static_cast<uint8_t>(
                    (row[x] & static_cast<uint8_t>(~write_mask)) |
                    (clear_value & write_mask));
            }
        }
    }
}

void glReadPixels(
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLenum type,
    GLvoid *pixels)
{
    if (!has_context()) return;
    if (width < 0 || height < 0)
    {
        set_error(GL_INVALID_VALUE);
        return;
    }
    if (format != GL_RGBA && format != GL_RGB)
    {
        set_error(GL_INVALID_ENUM);
        return;
    }

    const bool packed_565 =
        format == GL_RGB && type == GL_UNSIGNED_SHORT_5_6_5;
    if (!packed_565 && type != GL_UNSIGNED_BYTE)
    {
        set_error(GL_INVALID_ENUM);
        return;
    }

    if (width == 0 || height == 0)
        return;
    if (!pixels)
    {
        set_error(GL_INVALID_VALUE);
        return;
    }

    fxgl_surface_t surface = {};
    if (!resolve_draw_surface(surface) || !surface.color)
    {
        set_error(GL_INVALID_FRAMEBUFFER_OPERATION);
        return;
    }

    const size_t bytes_per_pixel = packed_565 ? 2 :
        (format == GL_RGBA ? 4 : 3);
    const size_t row_bytes = static_cast<size_t>(width) * bytes_per_pixel;
    if (row_bytes > std::numeric_limits<size_t>::max() - 7)
    {
        set_error(GL_INVALID_VALUE);
        return;
    }
    const size_t alignment = static_cast<size_t>(g_context.pack_alignment);
    const size_t row_stride =
        (row_bytes + alignment - 1) & ~(alignment - 1);
    if (static_cast<size_t>(height) >
        std::numeric_limits<size_t>::max() / row_stride)
    {
        set_error(GL_INVALID_VALUE);
        return;
    }

    uint8_t *destination = static_cast<uint8_t *>(pixels);
    for (GLsizei row_index = 0; row_index < height; ++row_index)
    {
        uint8_t *destination_row =
            destination + static_cast<size_t>(row_index) * row_stride;
        const int64_t source_y = static_cast<int64_t>(y) + row_index;

        for (GLsizei column = 0; column < width; ++column)
        {
            const int64_t source_x = static_cast<int64_t>(x) + column;
            uint16_t source_pixel = 0;
            if (source_x >= 0 && source_y >= 0 &&
                source_x < surface.width && source_y < surface.height)
            {
                source_pixel = surface.color[
                    static_cast<size_t>(source_y) * surface.stride +
                    static_cast<size_t>(source_x)];
            }

            if (packed_565)
            {
                std::memcpy(destination_row +
                    static_cast<size_t>(column) * sizeof(uint16_t),
                    &source_pixel, sizeof(source_pixel));
                continue;
            }

            const uint8_t red = static_cast<uint8_t>(
                (((source_pixel >> 11) & 0x1f) * 255 + 15) / 31);
            const uint8_t green = static_cast<uint8_t>(
                (((source_pixel >> 5) & 0x3f) * 255 + 31) / 63);
            const uint8_t blue = static_cast<uint8_t>(
                ((source_pixel & 0x1f) * 255 + 15) / 31);
            const size_t offset = static_cast<size_t>(column) * bytes_per_pixel;
            destination_row[offset + 0] = red;
            destination_row[offset + 1] = green;
            destination_row[offset + 2] = blue;
            if (format == GL_RGBA)
                destination_row[offset + 3] = 255;
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

void glDepthRangef(GLclampf near_value, GLclampf far_value)
{
    if (!has_context()) return;
    g_context.depth_near = clamp01(near_value);
    g_context.depth_far = clamp01(far_value);
}

void glBlendColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
    if (!has_context()) return;
    g_context.blend_color[0] = clamp01(red);
    g_context.blend_color[1] = clamp01(green);
    g_context.blend_color[2] = clamp01(blue);
    g_context.blend_color[3] = clamp01(alpha);
}

void glBlendFuncSeparate(
    GLenum src_rgb,
    GLenum dst_rgb,
    GLenum src_alpha,
    GLenum dst_alpha)
{
    if (!has_context()) return;
    if (!valid_blend_pair(src_rgb, dst_rgb, false) ||
        !valid_blend_pair(src_alpha, dst_alpha, true))
    {
        set_error(GL_INVALID_ENUM);
        return;
    }
    g_context.blend_src = src_rgb;
    g_context.blend_dst = dst_rgb;
    g_context.blend_src_alpha = src_alpha;
    g_context.blend_dst_alpha = dst_alpha;
}

void glBlendEquationSeparate(GLenum mode_rgb, GLenum mode_alpha)
{
    if (!has_context()) return;
    if (!valid_blend_equation(mode_rgb) ||
        !valid_blend_equation(mode_alpha))
    {
        set_error(GL_INVALID_ENUM);
        return;
    }
    g_context.blend_equation = mode_rgb;
    g_context.blend_equation_alpha = mode_alpha;
}

void glStencilFunc(GLenum func, GLint ref, GLuint mask)
{
    glStencilFuncSeparate(GL_FRONT_AND_BACK, func, ref, mask);
}

void glStencilOp(GLenum stencil_fail, GLenum depth_fail, GLenum depth_pass)
{
    glStencilOpSeparate(GL_FRONT_AND_BACK, stencil_fail, depth_fail, depth_pass);
}

void glStencilFuncSeparate(
    GLenum face,
    GLenum func,
    GLint ref,
    GLuint mask)
{
    if (!has_context()) return;
    if (face != GL_FRONT && face != GL_BACK && face != GL_FRONT_AND_BACK)
    {
        set_error(GL_INVALID_ENUM);
        return;
    }
    if (!valid_depth_func(func))
    {
        set_error(GL_INVALID_ENUM);
        return;
    }

    if (face == GL_FRONT || face == GL_FRONT_AND_BACK)
    {
        g_context.stencil_front.function = func;
        g_context.stencil_front.reference = ref;
        g_context.stencil_front.value_mask = mask;
    }
    if (face == GL_BACK || face == GL_FRONT_AND_BACK)
    {
        g_context.stencil_back.function = func;
        g_context.stencil_back.reference = ref;
        g_context.stencil_back.value_mask = mask;
    }
}

void glStencilOpSeparate(
    GLenum face,
    GLenum stencil_fail,
    GLenum depth_fail,
    GLenum depth_pass)
{
    if (!has_context()) return;
    if (face != GL_FRONT && face != GL_BACK && face != GL_FRONT_AND_BACK)
    {
        set_error(GL_INVALID_ENUM);
        return;
    }
    if (!valid_stencil_operation(stencil_fail) ||
        !valid_stencil_operation(depth_fail) ||
        !valid_stencil_operation(depth_pass))
    {
        set_error(GL_INVALID_ENUM);
        return;
    }

    if (face == GL_FRONT || face == GL_FRONT_AND_BACK)
    {
        g_context.stencil_front.stencil_fail = stencil_fail;
        g_context.stencil_front.depth_fail = depth_fail;
        g_context.stencil_front.depth_pass = depth_pass;
    }
    if (face == GL_BACK || face == GL_FRONT_AND_BACK)
    {
        g_context.stencil_back.stencil_fail = stencil_fail;
        g_context.stencil_back.depth_fail = depth_fail;
        g_context.stencil_back.depth_pass = depth_pass;
    }
}

void glStencilMaskSeparate(GLenum face, GLuint mask)
{
    if (!has_context()) return;
    if (face != GL_FRONT && face != GL_BACK && face != GL_FRONT_AND_BACK)
    {
        set_error(GL_INVALID_ENUM);
        return;
    }

    if (face == GL_FRONT || face == GL_FRONT_AND_BACK)
    {
        g_context.stencil_front.write_mask = mask;
        g_context.stencil_mask = mask;
    }
    if (face == GL_BACK || face == GL_FRONT_AND_BACK)
        g_context.stencil_back.write_mask = mask;
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
    if (!valid_blend_pair(sfactor, dfactor, false) ||
        !valid_blend_pair(sfactor, dfactor, true))
    {
        set_error(GL_INVALID_ENUM);
        return;
    }
    g_context.blend_src = sfactor;
    g_context.blend_dst = dfactor;
    g_context.blend_src_alpha = sfactor;
    g_context.blend_dst_alpha = dfactor;
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
    g_context.blend_equation_alpha = mode;
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
    g_context.stencil_front.write_mask = mask;
    g_context.stencil_back.write_mask = mask;
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
        case GL_BLEND_SRC_ALPHA:
            data[0] = static_cast<GLint>(g_context.blend_src_alpha);
            break;
        case GL_BLEND_DST_ALPHA:
            data[0] = static_cast<GLint>(g_context.blend_dst_alpha);
            break;
        case GL_BLEND_EQUATION_ALPHA:
            data[0] = static_cast<GLint>(g_context.blend_equation_alpha);
            break;
        case GL_STENCIL_WRITEMASK:
            data[0] = static_cast<GLint>(g_context.stencil_front.write_mask);
            break;
        case GL_STENCIL_FUNC:
            data[0] = static_cast<GLint>(g_context.stencil_front.function);
            break;
        case GL_STENCIL_REF:
            data[0] = g_context.stencil_front.reference;
            break;
        case GL_STENCIL_VALUE_MASK:
            data[0] = static_cast<GLint>(g_context.stencil_front.value_mask);
            break;
        case GL_STENCIL_FAIL:
            data[0] = static_cast<GLint>(g_context.stencil_front.stencil_fail);
            break;
        case GL_STENCIL_PASS_DEPTH_FAIL:
            data[0] = static_cast<GLint>(g_context.stencil_front.depth_fail);
            break;
        case GL_STENCIL_PASS_DEPTH_PASS:
            data[0] = static_cast<GLint>(g_context.stencil_front.depth_pass);
            break;
        case GL_STENCIL_BACK_FUNC:
            data[0] = static_cast<GLint>(g_context.stencil_back.function);
            break;
        case GL_STENCIL_BACK_REF:
            data[0] = g_context.stencil_back.reference;
            break;
        case GL_STENCIL_BACK_VALUE_MASK:
            data[0] = static_cast<GLint>(g_context.stencil_back.value_mask);
            break;
        case GL_STENCIL_BACK_WRITEMASK:
            data[0] = static_cast<GLint>(g_context.stencil_back.write_mask);
            break;
        case GL_STENCIL_BACK_FAIL:
            data[0] = static_cast<GLint>(g_context.stencil_back.stencil_fail);
            break;
        case GL_STENCIL_BACK_PASS_DEPTH_FAIL:
            data[0] = static_cast<GLint>(g_context.stencil_back.depth_fail);
            break;
        case GL_STENCIL_BACK_PASS_DEPTH_PASS:
            data[0] = static_cast<GLint>(g_context.stencil_back.depth_pass);
            break;
        case GL_FRAMEBUFFER_BINDING:
            data[0] = static_cast<GLint>(g_context.framebuffer_binding);
            break;
        case GL_RENDERBUFFER_BINDING:
            data[0] = static_cast<GLint>(g_context.renderbuffer_binding);
            break;
        case GL_MAX_RENDERBUFFER_SIZE:
            data[0] = FXGL_MAX_RENDERBUFFER_SIZE;
            break;
        case GL_ARRAY_BUFFER_BINDING:
            data[0] = static_cast<GLint>(g_context.array_buffer_binding);
            break;
        case GL_CURRENT_PROGRAM:
            data[0] = static_cast<GLint>(fxgl::current_program_name());
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
    if (pname == GL_DEPTH_RANGE)
    {
        data[0] = g_context.depth_near;
        data[1] = g_context.depth_far;
        return;
    }
    if (pname == GL_BLEND_COLOR)
    {
        for (int i = 0; i < 4; ++i)
            data[i] = g_context.blend_color[i];
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
void report_context_error(unsigned int error)
{
    set_error(static_cast<GLenum>(error));
}

bool get_texture_binding(
    size_t unit,
    gfx::Texture2DView& view,
    gfx::TextureSampler& sampler)
{
    if (!g_context.current || unit >= FXGL_MAX_TEXTURE_UNITS)
        return false;

    const GLuint name = g_context.texture_binding_2d[unit];
    TextureObject *object = find_texture(name);
    if (!object || !object->pixels || object->width == 0 || object->height == 0)
        return false;

    view = {};
    view.pixels = object->pixels;
    view.width = object->width;
    view.height = object->height;
    view.stride = object->width;
    const size_t object_index = static_cast<size_t>(name - 1);

    size_t level_count = 0;
    for (size_t i = 0; i < FXGL_MAX_TEXTURE_LEVELS - 1; ++i)
    {
        const TextureMipObject& level = object->mip_levels[i];
        if (!level.pixels)
            break;
        g_texture_mip_views[object_index][i] = {
            level.pixels, level.width, level.height, level.width
        };
        ++level_count;
    }
    view.mip_levels = g_texture_mip_views[object_index];
    view.mip_level_count = static_cast<uint8_t>(level_count);

    sampler = make_sampler(*object);
    return gfx::texture_valid(view) && gfx::sampler_valid(sampler);
}

static StencilFaceState convert_stencil_face(const StencilFace& face)
{
    StencilFaceState result = {};
    result.function = face.function;
    result.reference = face.reference;
    result.value_mask = face.value_mask;
    result.write_mask = face.write_mask;
    result.stencil_fail = face.stencil_fail;
    result.depth_fail = face.depth_fail;
    result.depth_pass = face.depth_pass;
    return result;
}

static void build_pipeline_context(
    RenderSurface& surface,
    PipelineState& state,
    bool varying_derivatives)
{
    fxgl_surface_t resolved = {};
    if (!resolve_draw_surface(resolved))
    {
        set_error(GL_INVALID_FRAMEBUFFER_OPERATION);
        surface = {};
        state = {};
        return;
    }
    surface.color = resolved.color;
    surface.depth = resolved.depth;
    surface.stencil = resolved.stencil;
    surface.width = resolved.width;
    surface.height = resolved.height;
    surface.stride = resolved.stride;

    pipeline_default_state(state, surface.width, surface.height);
    state.varying_derivatives = varying_derivatives;
    for (int i = 0; i < 4; ++i)
    {
        state.viewport[i] = g_context.viewport[i];
        state.scissor[i] = g_context.scissor[i];
        state.color_write[i] = g_context.color_mask[i] != GL_FALSE;
        state.blend_color[i] = g_context.blend_color[i];
    }

    state.scissor_enabled = g_context.scissor_test;
    state.cull_enabled = g_context.cull_face;
    state.cull_face = g_context.cull_mode;
    state.front_face = g_context.front_face;
    state.depth_enabled = g_context.depth_test;
    state.depth_function = g_context.depth_func;
    state.depth_write = g_context.depth_mask;
    state.depth_near = g_context.depth_near;
    state.depth_far = g_context.depth_far;
    state.stencil_enabled = g_context.stencil_test;
    state.stencil_front = convert_stencil_face(g_context.stencil_front);
    state.stencil_back = convert_stencil_face(g_context.stencil_back);
    state.blend_enabled = g_context.blend;
    state.blend_src_rgb = g_context.blend_src;
    state.blend_dst_rgb = g_context.blend_dst;
    state.blend_src_alpha = g_context.blend_src_alpha;
    state.blend_dst_alpha = g_context.blend_dst_alpha;
    state.blend_equation_rgb = g_context.blend_equation;
    state.blend_equation_alpha = g_context.blend_equation_alpha;
}

bool draw_clip_point(
    const ClipVertex& input,
    size_t varying_count,
    FragmentShader fragment_shader,
    void *user_data,
    RasterStats *stats,
    bool varying_derivatives)
{
    if (!g_context.current)
        return false;
    RenderSurface surface = {};
    PipelineState state = {};
    build_pipeline_context(surface, state, varying_derivatives);
    return fxgl::draw_point(surface, state, input, varying_count,
                            fragment_shader, user_data, stats);
}

bool draw_clip_line(
    const ClipVertex input[2],
    size_t varying_count,
    FragmentShader fragment_shader,
    void *user_data,
    RasterStats *stats,
    bool varying_derivatives)
{
    if (!g_context.current)
        return false;
    RenderSurface surface = {};
    PipelineState state = {};
    build_pipeline_context(surface, state, varying_derivatives);
    return fxgl::draw_line(surface, state, input, varying_count,
                           fragment_shader, user_data, stats);
}

bool draw_clip_triangle(
    const ClipVertex input[3],
    size_t varying_count,
    FragmentShader fragment_shader,
    void *user_data,
    RasterStats *stats,
    bool varying_derivatives)
{
    if (!g_context.current)
        return false;

    RenderSurface surface = {};
    PipelineState state = {};
    build_pipeline_context(surface, state, varying_derivatives);
    if (surface.width <= 0 || surface.height <= 0)
        return false;

    return fxgl::draw_triangle(surface, state, input, varying_count,
                               fragment_shader, user_data, stats);
}
} // namespace fxgl
