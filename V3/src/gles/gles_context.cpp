#include <GLES2/gl2.h>
#include <fxgl/platform.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

namespace
{
struct Context
{
    bool current = false;
    fxgl_surface_t surface = {};

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
    g_context = {};
}

GLboolean fxglHasCurrentContext(void)
{
    return g_context.current ? GL_TRUE : GL_FALSE;
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
        case GL_MAX_TEXTURE_SIZE:
            data[0] = 128;
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
