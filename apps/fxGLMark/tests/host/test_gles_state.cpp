#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>

#include <GLES2/gl2.h>
#include <fxgl/platform.h>
#include "gles/context_internal.hpp"

namespace
{
struct TestFragmentColor
{
    float rgba[4];
};

bool test_fragment_shader(const fxgl::FragmentInput&, float rgba[4], void *user_data)
{
    const TestFragmentColor& color =
        *static_cast<const TestFragmentColor *>(user_data);
    for (int i = 0; i < 4; ++i)
        rgba[i] = color.rgba[i];
    return true;
}

constexpr uint16_t RED = 0xf800;
constexpr uint16_t GREEN = 0x07e0;
constexpr uint16_t BLUE = 0x001f;
constexpr uint16_t SENTINEL = 0x5a5a;

void reset_buffers(uint16_t* color, uint16_t* depth, uint8_t* stencil, size_t count)
{
    for (size_t i = 0; i < count; ++i)
    {
        color[i] = SENTINEL;
        depth[i] = 0x1234;
        stencil[i] = 0xf0;
    }
}

void test_surface_binding_and_default_state()
{
    uint16_t color[8] = {};
    fxgl_surface_t invalid = {nullptr, nullptr, nullptr, 3, 2, 4};
    fxglReleaseCurrent();
    assert(fxglMakeCurrent(&invalid) == GL_FALSE);
    assert(fxglHasCurrentContext() == GL_FALSE);

    fxgl_surface_t surface = {color, nullptr, nullptr, 3, 2, 4};
    assert(fxglMakeCurrent(&surface) == GL_TRUE);
    assert(fxglHasCurrentContext() == GL_TRUE);

    GLint viewport[4] = {};
    GLint scissor[4] = {};
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetIntegerv(GL_SCISSOR_BOX, scissor);
    assert(viewport[0] == 0 && viewport[1] == 0);
    assert(viewport[2] == 3 && viewport[3] == 2);
    assert(scissor[0] == 0 && scissor[1] == 0);
    assert(scissor[2] == 3 && scissor[3] == 2);

    assert(glIsEnabled(GL_DITHER) == GL_TRUE);
    assert(glIsEnabled(GL_BLEND) == GL_FALSE);
    assert(glIsEnabled(GL_DEPTH_TEST) == GL_FALSE);
    assert(glGetError() == GL_NO_ERROR);

    GLboolean color_mask[4] = {};
    glGetBooleanv(GL_COLOR_WRITEMASK, color_mask);
    assert(color_mask[0] == GL_TRUE && color_mask[1] == GL_TRUE);
    assert(color_mask[2] == GL_TRUE && color_mask[3] == GL_TRUE);

    GLfloat clear_depth = 0.0f;
    glClearDepthf(0.375f);
    glGetFloatv(GL_DEPTH_CLEAR_VALUE, &clear_depth);
    assert(clear_depth > 0.374f && clear_depth < 0.376f);

    GLfloat point_size_range[2] = {};
    glGetFloatv(GL_ALIASED_POINT_SIZE_RANGE, point_size_range);
    assert(point_size_range[0] == 1.0f);
    assert(point_size_range[1] == 16.0f);
    GLint integer_point_size_range[2] = {};
    glGetIntegerv(GL_ALIASED_POINT_SIZE_RANGE, integer_point_size_range);
    assert(integer_point_size_range[0] == 1);
    assert(integer_point_size_range[1] == 16);

    assert(glGetString(GL_VENDOR) != nullptr);
    assert(glGetString(GL_RENDERER) != nullptr);
    assert(glGetString(GL_VERSION) != nullptr);
    assert(glGetString(GL_EXTENSIONS) != nullptr);

    fxglReleaseCurrent();
    assert(fxglHasCurrentContext() == GL_FALSE);
}

void test_rgb565_clear_and_stride_padding()
{
    uint16_t color[8];
    uint16_t depth[8];
    uint8_t stencil[8];
    reset_buffers(color, depth, stencil, 8);

    fxgl_surface_t surface = {color, depth, stencil, 3, 2, 4};
    assert(fxglMakeCurrent(&surface) == GL_TRUE);

    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    for (int y = 0; y < 2; ++y)
    {
        for (int x = 0; x < 3; ++x)
            assert(color[y * 4 + x] == RED);
        assert(color[y * 4 + 3] == SENTINEL);
    }
    for (size_t i = 0; i < 8; ++i)
    {
        assert(depth[i] == 0x1234);
        assert(stencil[i] == 0xf0);
    }

    fxglReleaseCurrent();
}

void test_scissor_color_mask_and_depth_stencil_clear()
{
    uint16_t color[8];
    uint16_t depth[8];
    uint8_t stencil[8];
    reset_buffers(color, depth, stencil, 8);

    fxgl_surface_t surface = {color, depth, stencil, 3, 2, 4};
    assert(fxglMakeCurrent(&surface) == GL_TRUE);

    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glClearDepthf(0.5f);
    glClearStencil(0x12);
    glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_TRUE);
    glStencilMask(0x0f);
    glScissor(1, 0, 1, 1);
    glEnable(GL_SCISSOR_TEST);
    glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // Only one pixel is in the scissor rectangle. The color mask preserves
    // red/blue bits; the stencil mask preserves the high nibble.
    assert(color[0] == SENTINEL);
    assert(color[1] == static_cast<uint16_t>((SENTINEL & 0xf81f) | GREEN));
    assert(color[2] == SENTINEL);
    assert(depth[1] == 32768);
    assert(stencil[1] == 0xf2);

    const size_t untouched[] = {0, 2, 4, 5, 6, 7};
    for (size_t i : untouched)
    {
        assert(depth[i] == 0x1234);
        assert(stencil[i] == 0xf0);
    }

    glDepthMask(GL_FALSE);
    glClearDepthf(0.0f);
    glClear(GL_DEPTH_BUFFER_BIT);
    assert(depth[1] == 32768);

    fxglReleaseCurrent();
}

void test_outside_scissor_is_safe_noop()
{
    uint16_t color[8];
    uint16_t depth[8];
    uint8_t stencil[8];
    reset_buffers(color, depth, stencil, 8);

    fxgl_surface_t surface = {color, depth, stencil, 3, 2, 4};
    assert(fxglMakeCurrent(&surface) == GL_TRUE);

    glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
    glScissor(100, 100, 10, 10);
    glEnable(GL_SCISSOR_TEST);
    glClear(GL_COLOR_BUFFER_BIT);

    for (size_t i = 0; i < 8; ++i)
        assert(color[i] == SENTINEL);

    glScissor(-100, -100, 10, 10);
    glClear(GL_COLOR_BUFFER_BIT);
    for (size_t i = 0; i < 8; ++i)
        assert(color[i] == SENTINEL);

    fxglReleaseCurrent();
}

void test_buffer_objects_and_vertex_attributes()
{
    uint16_t color[4] = {};
    fxgl_surface_t surface = {color, nullptr, nullptr, 2, 2, 2};
    assert(fxglMakeCurrent(&surface) == GL_TRUE);

    GLuint names[2] = {};
    glGenBuffers(2, names);
    assert(names[0] != 0 && names[1] != 0 && names[0] != names[1]);
    assert(glIsBuffer(names[0]) == GL_FALSE);

    const GLfloat vertices[6] = {0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
    glBindBuffer(GL_ARRAY_BUFFER, names[0]);
    assert(glIsBuffer(names[0]) == GL_TRUE);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

    GLint value = 0;
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &value);
    assert(value == static_cast<GLint>(sizeof(vertices)));
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_USAGE, &value);
    assert(value == GL_DYNAMIC_DRAW);

    const GLfloat replacement[2] = {0.25f, 0.75f};
    glBufferSubData(GL_ARRAY_BUFFER, sizeof(GLfloat), sizeof(replacement), replacement);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &value);
    assert(value == static_cast<GLint>(names[0]));

    const GLvoid *offset = reinterpret_cast<const GLvoid *>(
        static_cast<std::uintptr_t>(sizeof(GLfloat)));
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), offset);
    glEnableVertexAttribArray(2);

    glGetVertexAttribiv(2, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &value);
    assert(value == GL_TRUE);
    glGetVertexAttribiv(2, GL_VERTEX_ATTRIB_ARRAY_SIZE, &value);
    assert(value == 3);
    glGetVertexAttribiv(2, GL_VERTEX_ATTRIB_ARRAY_TYPE, &value);
    assert(value == GL_FLOAT);
    glGetVertexAttribiv(2, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &value);
    assert(value == 3 * static_cast<GLint>(sizeof(GLfloat)));
    glGetVertexAttribiv(2, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &value);
    assert(value == static_cast<GLint>(names[0]));

    GLvoid *stored_pointer = nullptr;
    glGetVertexAttribPointerv(2, GL_VERTEX_ATTRIB_ARRAY_POINTER, &stored_pointer);
    assert(stored_pointer == offset);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, names[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 12, nullptr, GL_STATIC_DRAW);
    glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &value);
    assert(value == 12);
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &value);
    assert(value == static_cast<GLint>(names[1]));

    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 10, 4, replacement);
    assert(glGetError() == GL_INVALID_VALUE);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, 1, nullptr);
    assert(glGetError() == GL_INVALID_VALUE);

    glVertexAttribPointer(2, 3, 0xdead, GL_FALSE, 0, nullptr);
    assert(glGetError() == GL_INVALID_ENUM);
    glVertexAttribPointer(2, 5, GL_FLOAT, GL_FALSE, 0, nullptr);
    assert(glGetError() == GL_INVALID_VALUE);
    glEnableVertexAttribArray(8);
    assert(glGetError() == GL_INVALID_VALUE);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &names[0]);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &value);
    assert(value == 0);
    glGetVertexAttribiv(2, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &value);
    assert(value == 0);
    glGetVertexAttribPointerv(2, GL_VERTEX_ATTRIB_ARRAY_POINTER, &stored_pointer);
    assert(stored_pointer == nullptr);

    glBindBuffer(GL_ARRAY_BUFFER, 999);
    assert(glGetError() == GL_INVALID_OPERATION);

    glBindBuffer(GL_ARRAY_BUFFER, names[1]);
    glBufferData(GL_ARRAY_BUFFER, 128 * 1024 + 1, nullptr, GL_STATIC_DRAW);
    assert(glGetError() == GL_OUT_OF_MEMORY);

    glDeleteBuffers(2, names);
    assert(glIsBuffer(names[0]) == GL_FALSE);
    assert(glIsBuffer(names[1]) == GL_FALSE);
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &value);
    assert(value == 8);
    assert(glGetError() == GL_NO_ERROR);

    fxglReleaseCurrent();
}

void test_texture_objects_and_uploads()
{
    uint16_t color[4] = {};
    fxgl_surface_t surface = {color, nullptr, nullptr, 2, 2, 2};
    assert(fxglMakeCurrent(&surface) == GL_TRUE);

    GLuint textures[2] = {};
    glGenTextures(2, textures);
    assert(textures[0] != 0 && textures[1] != 0 && textures[0] != textures[1]);
    assert(glIsTexture(textures[0]) == GL_FALSE);

    GLint value = 0;
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, textures[1]);
    assert(glIsTexture(textures[1]) == GL_TRUE);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &value);
    assert(value == GL_TEXTURE1);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &value);
    assert(value == static_cast<GLint>(textures[1]));

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);

    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &value);
    assert(value == GL_LINEAR);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &value);
    assert(value == GL_LINEAR);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &value);
    assert(value == GL_REPEAT);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &value);
    assert(value == GL_MIRRORED_REPEAT);

    const uint16_t source[4] = {RED, 0x001f, GREEN, 0xffff};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2, 2, 0, GL_RGB,
        GL_UNSIGNED_SHORT_5_6_5, source);

    gfx::Texture2DView view = {};
    gfx::TextureSampler sampler = {};
    assert(fxgl::get_texture_binding(1, view, sampler));
    assert(view.width == 2 && view.height == 2 && view.stride == 2);
    assert(sampler.filter == gfx::TextureFilter::Bilinear);
    assert(sampler.address_u == gfx::TextureAddressMode::Repeat);
    assert(sampler.address_v == gfx::TextureAddressMode::MirroredRepeat);
    assert(gfx::sample_texture(view, 0, 0, sampler) == RED);

    const uint16_t update = BLUE;
    glTexSubImage2D(GL_TEXTURE_2D, 0, 1, 0, 1, 1, GL_RGB,
        GL_UNSIGNED_SHORT_5_6_5, &update);
    assert(fxgl::get_texture_binding(1, view, sampler));
    const gfx::TextureSampler nearest = {
        gfx::TextureFilter::Nearest,
        gfx::TextureAddressMode::Clamp,
        gfx::TextureAddressMode::Clamp
    };
    assert(gfx::sample_texture(view, 1LL << 16, 0, nearest) == BLUE);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB,
        GL_UNSIGNED_SHORT_5_6_5, nullptr);
    assert(fxgl::get_texture_binding(0, view, sampler));
    assert(view.width == 1 && view.height == 1);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
        GL_LINEAR_MIPMAP_LINEAR);
    assert(glGetError() == GL_NO_ERROR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
        GL_NEAREST_MIPMAP_NEAREST);
    assert(glGetError() == GL_INVALID_ENUM);
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGB, 1, 1, 0, GL_RGB,
        GL_UNSIGNED_SHORT_5_6_5, nullptr);
    assert(glGetError() == GL_INVALID_VALUE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 129, 1, 0, GL_RGB,
        GL_UNSIGNED_SHORT_5_6_5, nullptr);
    assert(glGetError() == GL_INVALID_VALUE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB,
        GL_UNSIGNED_BYTE, nullptr);
    assert(glGetError() == GL_INVALID_ENUM);

    glTexSubImage2D(GL_TEXTURE_2D, 0, 1, 0, 1, 1, GL_RGB,
        GL_UNSIGNED_SHORT_5_6_5, &update);
    assert(glGetError() == GL_INVALID_VALUE);

    glDeleteTextures(2, textures);
    assert(glIsTexture(textures[0]) == GL_FALSE);
    assert(glIsTexture(textures[1]) == GL_FALSE);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &value);
    assert(value == 0);
    assert(!fxgl::get_texture_binding(0, view, sampler));

    glActiveTexture(GL_TEXTURE1 + 1);
    assert(glGetError() == GL_INVALID_ENUM);

    fxglReleaseCurrent();
}

void test_rgb565_unpack_alignment()
{
    uint16_t color[1] = {};
    fxgl_surface_t surface = {color, nullptr, nullptr, 1, 1, 1};
    assert(fxglMakeCurrent(&surface) == GL_TRUE);

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    // Default unpack alignment is four bytes. Two RGB565 pixels occupy four
    // bytes per row; an odd-width row occupies two bytes but starts each next
    // row at a four-byte boundary.
    const uint16_t aligned_rows[4] = {
        RED, 0xaaaa,
        GREEN, 0xbbbb
    };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 2, 0, GL_RGB,
        GL_UNSIGNED_SHORT_5_6_5, aligned_rows);

    gfx::Texture2DView view = {};
    gfx::TextureSampler sampler = {};
    assert(fxgl::get_texture_binding(0, view, sampler));
    assert(view.width == 1 && view.height == 2);
    assert(gfx::sample_texture(view, 0, 0, sampler) == RED);
    assert(gfx::sample_texture(view, 0, 1LL << 16, sampler) == GREEN);

    GLint alignment = 0;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
    assert(alignment == 4);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
    assert(alignment == 1);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 3);
    assert(glGetError() == GL_INVALID_VALUE);

    const uint16_t tight_rows[2] = {BLUE, 0xffff};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 2, 0, GL_RGB,
        GL_UNSIGNED_SHORT_5_6_5, tight_rows);
    assert(fxgl::get_texture_binding(0, view, sampler));
    assert(gfx::sample_texture(view, 0, 0, sampler) == BLUE);
    assert(gfx::sample_texture(view, 0, 1LL << 16, sampler) == 0xffff);

    glDeleteTextures(1, &texture);
    fxglReleaseCurrent();
}

void test_context_drives_software_pipeline()
{
    uint16_t color[16] = {};
    uint16_t depth[16];
    uint8_t stencil[16] = {};
    for (size_t i = 0; i < 16; ++i)
        depth[i] = 0xffff;

    fxgl_surface_t surface = {color, depth, stencil, 4, 4, 4};
    assert(fxglMakeCurrent(&surface) == GL_TRUE);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    fxgl::ClipVertex triangle[3] = {};
    triangle[0].position = {-1.0f, -1.0f, 0.0f, 1.0f};
    triangle[1].position = {1.0f, -1.0f, 0.0f, 1.0f};
    triangle[2].position = {-1.0f, 1.0f, 0.0f, 1.0f};

    TestFragmentColor red = {{1.0f, 0.0f, 0.0f, 1.0f}};
    TestFragmentColor blue = {{0.0f, 0.0f, 1.0f, 1.0f}};
    TestFragmentColor green = {{0.0f, 1.0f, 0.0f, 1.0f}};
    fxgl::RasterStats stats = {};

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    assert(fxgl::draw_clip_triangle(triangle, 0, test_fragment_shader, &red, &stats));
    assert(color[0] == RED);
    assert(depth[0] == 32768);

    // Equal depth fails LESS, leaving the first fragment in place.
    assert(!fxgl::draw_clip_triangle(triangle, 0, test_fragment_shader, &blue));
    assert(color[0] == RED);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    glScissor(1, 1, 1, 1);
    assert(fxgl::draw_clip_triangle(triangle, 0, test_fragment_shader, &green));
    assert(color[1 * 4 + 1] == GREEN);
    assert(color[0] == RED);

    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    TestFragmentColor half_blue = {{0.0f, 0.0f, 1.0f, 0.5f}};
    assert(fxgl::draw_clip_triangle(triangle, 0, test_fragment_shader, &half_blue));
    const uint16_t blended = color[0];
    assert(((blended >> 11) & 0x1f) >= 14);
    assert((blended & 0x1f) >= 14);

    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    std::swap(triangle[1], triangle[2]);
    const uint16_t before_cull = color[0];
    assert(!fxgl::draw_clip_triangle(triangle, 0, test_fragment_shader, &red));
    assert(color[0] == before_cull);

    fxglReleaseCurrent();
}

void test_shader_and_program_object_lifecycle()
{
    uint16_t color[4] = {};
    fxgl_surface_t surface = {color, nullptr, nullptr, 2, 2, 2};
    assert(fxglMakeCurrent(&surface) == GL_TRUE);

    static const GLchar vertex_source[] =
        "#version 100\n"
        "attribute vec4 a_position;\n"
        "attribute vec2 a_uv;\n"
        "uniform mat4 u_mvp;\n"
        "uniform float u_gain;\n"
        "varying vec2 v_uv;\n"
        "void main() {\n"
        "  gl_Position = u_mvp * a_position;\n"
        "  v_uv = a_uv;\n"
        "}\n";
    static const GLchar fragment_source[] =
        "#version 100\n"
        "precision mediump float;\n"
        "uniform sampler2D u_texture;\n"
        "uniform float u_gain;\n"
        "varying vec2 v_uv;\n"
        "void main() {\n"
        "  gl_FragColor = texture2D(u_texture, v_uv) * u_gain;\n"
        "}\n";

    const GLuint vertex = glCreateShader(GL_VERTEX_SHADER);
    const GLuint fragment = glCreateShader(GL_FRAGMENT_SHADER);
    assert(vertex != 0 && fragment != 0 && vertex != fragment);
    assert(glIsShader(vertex) == GL_TRUE);
    assert(glIsShader(fragment) == GL_TRUE);

    const GLchar *vertex_parts[] = {vertex_source};
    glShaderSource(vertex, 1, vertex_parts, nullptr);
    glCompileShader(vertex);
    GLint value = 0;
    glGetShaderiv(vertex, GL_COMPILE_STATUS, &value);
    assert(value == GL_TRUE);
    glGetShaderiv(vertex, GL_SHADER_TYPE, &value);
    assert(value == GL_VERTEX_SHADER);
    glGetShaderiv(vertex, GL_SHADER_SOURCE_LENGTH, &value);
    assert(value == static_cast<GLint>(sizeof(vertex_source)));

    const GLchar *fragment_parts[] = {fragment_source};
    const GLint fragment_length[] = {static_cast<GLint>(sizeof(fragment_source) - 1)};
    glShaderSource(fragment, 1, fragment_parts, fragment_length);
    glCompileShader(fragment);
    glGetShaderiv(fragment, GL_COMPILE_STATUS, &value);
    assert(value == GL_TRUE);

    const GLuint program = glCreateProgram();
    assert(program != 0 && glIsProgram(program) == GL_TRUE);
    glBindAttribLocation(program, 3, "a_uv");
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glGetProgramiv(program, GL_ATTACHED_SHADERS, &value);
    assert(value == 2);

    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &value);
    assert(value == GL_TRUE);
    glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &value);
    assert(value == 2);
    glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &value);
    assert(value == 3);
    glValidateProgram(program);
    glGetProgramiv(program, GL_VALIDATE_STATUS, &value);
    assert(value == GL_TRUE);

    GLuint attached[2] = {};
    GLsizei attached_count = 0;
    glGetAttachedShaders(program, 2, &attached_count, attached);
    assert(attached_count == 2);
    assert(attached[0] == vertex && attached[1] == fragment);

    GLchar active_name[32] = {};
    GLsizei active_length = 0;
    GLint active_size = 0;
    GLenum active_type = 0;
    glGetActiveAttrib(program, 0, sizeof(active_name),
        &active_length, &active_size, &active_type, active_name);
    assert(std::strcmp(active_name, "a_position") == 0);
    assert(active_size == 1 && active_type == GL_FLOAT_VEC4);
    glGetActiveUniform(program, 0, sizeof(active_name),
        &active_length, &active_size, &active_type, active_name);
    assert(std::strcmp(active_name, "u_mvp") == 0);
    assert(active_size == 1 && active_type == GL_FLOAT_MAT4);

    GLchar retrieved_source[256] = {};
    glGetShaderSource(vertex, sizeof(retrieved_source),
        &active_length, retrieved_source);
    assert(active_length == static_cast<GLsizei>(sizeof(vertex_source) - 1));
    assert(std::strcmp(retrieved_source, vertex_source) == 0);

    assert(glGetAttribLocation(program, "a_position") == 0);
    assert(glGetAttribLocation(program, "a_uv") == 3);
    assert(glGetAttribLocation(program, "not_an_attribute") == -1);

    const GLint mvp_location = glGetUniformLocation(program, "u_mvp");
    const GLint gain_location = glGetUniformLocation(program, "u_gain");
    const GLint texture_location = glGetUniformLocation(program, "u_texture");
    assert(mvp_location >= 0);
    assert(gain_location >= 0);
    assert(texture_location >= 0);
    assert(glGetUniformLocation(program, "missing_uniform") == -1);

    glUseProgram(program);
    glGetIntegerv(GL_CURRENT_PROGRAM, &value);
    assert(value == static_cast<GLint>(program));

    const GLfloat identity[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
    glUniformMatrix4fv(mvp_location, 1, GL_FALSE, identity);
    const GLfloat gain_value[1] = {0.75f};
    glUniform1fv(gain_location, 1, gain_value);
    glUniform1i(texture_location, 1);
    GLfloat retrieved_gain = 0.0f;
    GLint retrieved_texture_unit = -1;
    glGetUniformfv(program, gain_location, &retrieved_gain);
    glGetUniformiv(program, texture_location, &retrieved_texture_unit);
    assert(retrieved_gain == 0.75f);
    assert(retrieved_texture_unit == 1);

    fxgl::LinkedProgramView view = {};
    assert(fxgl::get_current_program_view(view));
    assert(view.vertex_shader && view.fragment_shader);
    assert(view.uniform_count == 3);
    assert(view.vertex_shader->uniform_count == 2);
    assert(view.fragment_shader->uniform_count == 2);
    assert(view.vertex_uniform_map[1] == view.fragment_uniform_map[1]);
    assert(view.vertex_varying_offsets[0] == 0);
    assert(view.fragment_varying_offsets[0] == 0);
    assert(view.varying_scalar_count == 2);
    assert(view.attribute_locations[0] == 0);
    assert(view.attribute_locations[1] == 3);
    assert(view.uniform_values[static_cast<size_t>(mvp_location)].type ==
        fxgl::ShaderType::Mat4);
    assert(view.uniform_values[static_cast<size_t>(mvp_location)].data[0] == 1.0f);
    assert(view.uniform_values[static_cast<size_t>(gain_location)].type ==
        fxgl::ShaderType::Float);
    assert(view.uniform_values[static_cast<size_t>(gain_location)].data[0] == 0.75f);
    assert(view.uniform_values[static_cast<size_t>(texture_location)].type ==
        fxgl::ShaderType::Sampler2D);
    assert(view.uniform_values[static_cast<size_t>(texture_location)].data[0] == 1.0f);

    glUniform1f(texture_location, 1.0f);
    assert(glGetError() == GL_INVALID_OPERATION);
    glUniform1f(-1, 1.0f);
    assert(glGetError() == GL_NO_ERROR);

    // Deleting attached shaders marks them pending; the attached program
    // keeps them alive until it is deleted or detached.
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    glGetShaderiv(vertex, GL_DELETE_STATUS, &value);
    assert(value == GL_TRUE);
    assert(glIsShader(vertex) == GL_TRUE);

    // A current program remains current while deletion is pending. It is
    // released when another program (or zero) becomes current.
    glDeleteProgram(program);
    glGetProgramiv(program, GL_DELETE_STATUS, &value);
    assert(value == GL_TRUE);
    glGetIntegerv(GL_CURRENT_PROGRAM, &value);
    assert(value == static_cast<GLint>(program));
    assert(glIsProgram(program) == GL_TRUE);

    glUniform1f(gain_location, 0.5f);
    assert(glGetError() == GL_NO_ERROR);
    glUseProgram(0);
    glGetIntegerv(GL_CURRENT_PROGRAM, &value);
    assert(value == 0);
    assert(!fxgl::get_current_program_view(view));
    assert(glIsProgram(program) == GL_FALSE);
    assert(glIsShader(vertex) == GL_FALSE);
    assert(glIsShader(fragment) == GL_FALSE);

    fxglReleaseCurrent();
}

void test_gles_draw_arrays_and_elements()
{
    uint16_t color[64] = {};
    fxgl_surface_t surface = {color, nullptr, nullptr, 8, 8, 8};
    assert(fxglMakeCurrent(&surface) == GL_TRUE);

    static const GLchar vertex_source[] =
        "#version 100\n"
        "attribute vec4 a_position;\n"
        "uniform mat4 u_mvp;\n"
        "void set_point_size(out float size);\n"
        "void enlarge_point_size(inout float size);\n"
        "void main() {\n"
        "  gl_Position = u_mvp * a_position;\n"
        "  set_point_size(gl_PointSize);\n"
        "  enlarge_point_size(gl_PointSize);\n"
        "}\n"
        "void set_point_size(out float size) { size = 2.5; }\n"
        "void enlarge_point_size(inout float size) { size = size + 0.5; }";
    static const GLchar fragment_source[] =
        "#version 100\n"
        "precision mediump float;\n"
        "uniform vec4 u_color;\n"
        "void main() { gl_FragColor = u_color; }";

    const GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    const GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    const GLchar *vertex_strings[] = {vertex_source};
    const GLchar *fragment_strings[] = {fragment_source};
    glShaderSource(vertex_shader, 1, vertex_strings, nullptr);
    glShaderSource(fragment_shader, 1, fragment_strings, nullptr);
    glCompileShader(vertex_shader);
    glCompileShader(fragment_shader);

    GLint status = GL_FALSE;
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &status);
    assert(status == GL_TRUE);
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &status);
    assert(status == GL_TRUE);

    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    assert(status == GL_TRUE);
    glUseProgram(program);

    const GLint position_location = glGetAttribLocation(program, "a_position");
    const GLint matrix_location = glGetUniformLocation(program, "u_mvp");
    const GLint color_location = glGetUniformLocation(program, "u_color");
    assert(position_location >= 0 && matrix_location >= 0 && color_location >= 0);

    const GLfloat identity[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
    glUniformMatrix4fv(matrix_location, 1, GL_FALSE, identity);
    glUniform4f(color_location, 1.0f, 0.0f, 0.0f, 1.0f);

    const GLfloat vertices[12] = {
        -1.0f, -1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 0.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 1.0f
    };
    glVertexAttribPointer(
        static_cast<GLuint>(position_location), 4, GL_FLOAT, GL_FALSE,
        0, vertices);
    glEnableVertexAttribArray(static_cast<GLuint>(position_location));

    glDrawArrays(GL_TRIANGLES, 0, 3);
    assert(color[0] == RED);
    size_t red_pixels = 0;
    for (uint16_t pixel : color)
        if (pixel == RED) ++red_pixels;
    assert(red_pixels > 0);

    for (uint16_t& pixel : color)
        pixel = 0;
    glUniform4f(color_location, 0.0f, 1.0f, 0.0f, 1.0f);

    GLuint buffers[2] = {};
    glGenBuffers(2, buffers);
    glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(
        static_cast<GLuint>(position_location), 4, GL_FLOAT, GL_FALSE,
        0, nullptr);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[1]);
    const uint8_t indices[3] = {0, 1, 2};
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_BYTE, nullptr);
    assert(color[0] == GREEN);

    // Element offsets are bounds-checked against the bound index buffer.
    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, nullptr);
    assert(glGetError() == GL_INVALID_OPERATION);

    // Independent line pairs and points now use their own raster paths.
    for (uint16_t& pixel : color)
        pixel = 0;
    glUniform4f(color_location, 0.0f, 0.0f, 1.0f, 1.0f);
    glDrawArrays(GL_LINES, 0, 2);
    assert(color[0] == BLUE);
    size_t blue_line_pixels = 0;
    for (size_t x = 0; x < 8; ++x)
        if (color[x] == BLUE) ++blue_line_pixels;
    assert(blue_line_pixels >= 7);

    for (uint16_t& pixel : color)
        pixel = 0;
    glDrawArrays(GL_POINTS, 0, 1);
    assert(color[0] == BLUE);
    assert(color[1] == BLUE);
    assert(color[8] == BLUE);
    assert(color[9] == BLUE);
    size_t blue_point_pixels = 0;
    for (uint16_t pixel : color)
        if (pixel == BLUE) ++blue_point_pixels;
    assert(blue_point_pixels == 4);

    // Unknown primitive enums are rejected, not treated as a known mode.
    glDrawArrays(0x0007, 0, 3);
    assert(glGetError() == GL_INVALID_ENUM);

    // The current program and all bound object names remain queryable.
    GLint current = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &current);
    assert(current == static_cast<GLint>(program));

    glDeleteBuffers(2, buffers);
    glDeleteProgram(program);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    fxglReleaseCurrent();
}

void test_draw_call_texture_cache_refreshes_between_draws()
{
    uint16_t color[16] = {};
    fxgl_surface_t surface = {color, nullptr, nullptr, 4, 4, 4};
    assert(fxglMakeCurrent(&surface) == GL_TRUE);

    static const GLchar vertex_source[] =
        "#version 100\n"
        "attribute vec4 a_position;\n"
        "attribute vec2 a_uv;\n"
        "varying vec2 v_uv;\n"
        "void main() { gl_Position = a_position; v_uv = a_uv; }";
    static const GLchar fragment_source[] =
        "#version 100\n"
        "precision mediump float;\n"
        "uniform sampler2D u_texture;\n"
        "varying vec2 v_uv;\n"
        "void main() { gl_FragColor = texture2D(u_texture, v_uv); }";

    const GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    const GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    const GLchar *vertex_parts[] = {vertex_source};
    const GLchar *fragment_parts[] = {fragment_source};
    glShaderSource(vertex_shader, 1, vertex_parts, nullptr);
    glShaderSource(fragment_shader, 1, fragment_parts, nullptr);
    glCompileShader(vertex_shader);
    glCompileShader(fragment_shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &compiled);
    assert(compiled == GL_TRUE);
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &compiled);
    assert(compiled == GL_TRUE);

    const GLuint program = glCreateProgram();
    glBindAttribLocation(program, 0, "a_position");
    glBindAttribLocation(program, 1, "a_uv");
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    assert(linked == GL_TRUE);
    glUseProgram(program);

    const GLint sampler_location =
        glGetUniformLocation(program, "u_texture");
    assert(sampler_location >= 0);
    glUniform1i(sampler_location, 0);

    const uint16_t red_texel = RED;
    const uint16_t green_texel = GREEN;
    GLuint textures[2] = {};
    glGenTextures(2, textures);

    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0,
        GL_RGB, GL_UNSIGNED_SHORT_5_6_5, &red_texel);
    glBindTexture(GL_TEXTURE_2D, textures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0,
        GL_RGB, GL_UNSIGNED_SHORT_5_6_5, &green_texel);
    assert(glGetError() == GL_NO_ERROR);

    const GLfloat vertices[] = {
        -1.0f, -1.0f, 0.0f, 1.0f, 0.5f, 0.5f,
         1.0f, -1.0f, 0.0f, 1.0f, 0.5f, 0.5f,
        -1.0f,  1.0f, 0.0f, 1.0f, 0.5f, 0.5f
    };
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE,
        6 * sizeof(GLfloat), vertices);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
        6 * sizeof(GLfloat), vertices + 4);
    glEnableVertexAttribArray(1);

    // The first draw caches texture zero and samples red.
    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    assert(color[0] == RED);

    // A subsequent draw must resolve the newly bound texture, not reuse the
    // previous draw's cached view or sampler.
    for (uint16_t& pixel : color)
        pixel = 0;
    glBindTexture(GL_TEXTURE_2D, textures[1]);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    assert(color[0] == GREEN);
    assert(glGetError() == GL_NO_ERROR);

    glDeleteTextures(2, textures);
    glDeleteProgram(program);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    fxglReleaseCurrent();
}

void test_shader_compile_and_program_link_failures()
{
    uint16_t color[4] = {};
    fxgl_surface_t surface = {color, nullptr, nullptr, 2, 2, 2};
    assert(fxglMakeCurrent(&surface) == GL_TRUE);

    static const GLchar bad_vertex_source[] =
        "attribute vec4 a_position; void main() { gl_Position = unknown; }";
    static const GLchar valid_fragment_source[] =
        "precision mediump float; void main() { gl_FragColor = vec4(1.0); }";

    const GLuint bad_vertex = glCreateShader(GL_VERTEX_SHADER);
    const GLuint fragment = glCreateShader(GL_FRAGMENT_SHADER);
    const GLchar *bad_parts[] = {bad_vertex_source};
    const GLchar *fragment_parts[] = {valid_fragment_source};
    glShaderSource(bad_vertex, 1, bad_parts, nullptr);
    glShaderSource(fragment, 1, fragment_parts, nullptr);
    glCompileShader(bad_vertex);
    glCompileShader(fragment);

    GLint value = GL_TRUE;
    glGetShaderiv(bad_vertex, GL_COMPILE_STATUS, &value);
    assert(value == GL_FALSE);
    glGetShaderiv(bad_vertex, GL_INFO_LOG_LENGTH, &value);
    assert(value > 1);
    GLchar log[160] = {};
    GLsizei log_length = 0;
    glGetShaderInfoLog(bad_vertex, sizeof(log), &log_length, log);
    assert(log_length > 0 && log[0] != '\0');

    const GLuint failed_program = glCreateProgram();
    glAttachShader(failed_program, bad_vertex);
    glAttachShader(failed_program, fragment);
    glLinkProgram(failed_program);
    glGetProgramiv(failed_program, GL_LINK_STATUS, &value);
    assert(value == GL_FALSE);
    glGetProgramiv(failed_program, GL_INFO_LOG_LENGTH, &value);
    assert(value > 1);
    glGetProgramInfoLog(failed_program, sizeof(log), &log_length, log);
    assert(log_length > 0 && log[0] != '\0');
    glUseProgram(failed_program);
    assert(glGetError() == GL_INVALID_OPERATION);

    glDeleteProgram(failed_program);
    glDeleteShader(bad_vertex);
    glDeleteShader(fragment);

    // A syntactically valid pair still fails linking when fragment varyings
    // are not produced by the vertex stage.
    static const GLchar vertex_no_varying[] =
        "void main() { gl_Position = vec4(0.0); }";
    static const GLchar fragment_missing_varying[] =
        "varying vec2 v_missing; void main() { gl_FragColor = vec4(v_missing, 0.0, 1.0); }";
    const GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    const GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    const GLchar *vs_parts[] = {vertex_no_varying};
    const GLchar *fs_parts[] = {fragment_missing_varying};
    glShaderSource(vs, 1, vs_parts, nullptr);
    glShaderSource(fs, 1, fs_parts, nullptr);
    glCompileShader(vs);
    glCompileShader(fs);

    const GLuint link_error_program = glCreateProgram();
    glAttachShader(link_error_program, vs);
    glAttachShader(link_error_program, fs);
    glLinkProgram(link_error_program);
    glGetProgramiv(link_error_program, GL_LINK_STATUS, &value);
    assert(value == GL_FALSE);

    glDeleteProgram(link_error_program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    fxglReleaseCurrent();
}

void test_gles_framebuffer_and_renderbuffer_objects()
{
    uint16_t default_color[16] = {};
    fxgl_surface_t surface = {default_color, nullptr, nullptr, 4, 4, 4};
    assert(fxglMakeCurrent(&surface) == GL_TRUE);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    GLuint texture = 0;
    glGenTextures(1, &texture);
    assert(texture != 0 && glIsTexture(texture) == GL_FALSE);
    glBindTexture(GL_TEXTURE_2D, texture);
    const uint16_t initial_pixels[16] = {};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 4, 4, 0, GL_RGB,
        GL_UNSIGNED_SHORT_5_6_5, initial_pixels);

    GLuint framebuffer = 0;
    glGenFramebuffers(1, &framebuffer);
    assert(framebuffer != 0 && glIsFramebuffer(framebuffer) == GL_FALSE);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    assert(glIsFramebuffer(framebuffer) == GL_TRUE);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) ==
        GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D, texture, 0);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    GLint value = -1;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &value);
    assert(value == static_cast<GLint>(framebuffer));
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &value);
    assert(value == GL_TEXTURE);
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &value);
    assert(value == static_cast<GLint>(texture));
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, &value);
    assert(value == 0);

    GLuint renderbuffers[3] = {};
    glGenRenderbuffers(3, renderbuffers);
    assert(glIsRenderbuffer(renderbuffers[0]) == GL_FALSE);
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffers[0]);
    assert(glIsRenderbuffer(renderbuffers[0]) == GL_TRUE);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, 4, 4);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
        GL_RENDERBUFFER, renderbuffers[0]);

    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffers[1]);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, 4, 4);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
        GL_RENDERBUFFER, renderbuffers[1]);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    glGetIntegerv(GL_RENDERBUFFER_BINDING, &value);
    assert(value == static_cast<GLint>(renderbuffers[1]));
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &value);
    assert(value == 4);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &value);
    assert(value == 4);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER,
        GL_RENDERBUFFER_INTERNAL_FORMAT, &value);
    assert(value == GL_STENCIL_INDEX8);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER,
        GL_RENDERBUFFER_STENCIL_SIZE, &value);
    assert(value == 8);

    // Mismatched attachment dimensions make the framebuffer incomplete.
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffers[2]);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, 2, 2);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
        GL_RENDERBUFFER, renderbuffers[2]);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) ==
        GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS);

    // Restore matching dimensions, then clear the texture-backed FBO.
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, 4, 4);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    gfx::Texture2DView view = {};
    gfx::TextureSampler sampler = {};
    assert(fxgl::get_texture_binding(0, view, sampler));
    assert(view.width == 4 && view.height == 4);
    assert(view.pixels[0] == BLUE);
    assert(view.pixels[5] == BLUE);
    assert(view.pixels[15] == BLUE);

    GLubyte rgba[2 * 2 * 4] = {};
    glReadPixels(1, 1, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    assert(rgba[0] == 0 && rgba[1] == 0 && rgba[2] == 255 && rgba[3] == 255);
    assert(rgba[4] == 0 && rgba[5] == 0 && rgba[6] == 255 && rgba[7] == 255);

    // RGB rows are padded to the current pack alignment (four bytes by default).
    GLubyte aligned_rgb[8] = {0xaa, 0xaa, 0xaa, 0xaa, 0xbb, 0xbb, 0xbb, 0xbb};
    glReadPixels(0, 0, 1, 2, GL_RGB, GL_UNSIGNED_BYTE, aligned_rgb);
    assert(aligned_rgb[0] == 0 && aligned_rgb[1] == 0 && aligned_rgb[2] == 255);
    assert(aligned_rgb[3] == 0xaa);
    assert(aligned_rgb[4] == 0 && aligned_rgb[5] == 0 && aligned_rgb[6] == 255);
    assert(aligned_rgb[7] == 0xbb);

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    GLubyte tight_rgb[6] = {};
    glReadPixels(0, 0, 1, 2, GL_RGB, GL_UNSIGNED_BYTE, tight_rgb);
    assert(tight_rgb[0] == 0 && tight_rgb[1] == 0 && tight_rgb[2] == 255);
    assert(tight_rgb[3] == 0 && tight_rgb[4] == 0 && tight_rgb[5] == 255);

    uint16_t packed[4] = {};
    glReadPixels(0, 0, 2, 2, GL_RGB,
        GL_UNSIGNED_SHORT_5_6_5, packed);
    for (uint16_t pixel : packed)
        assert(pixel == BLUE);

    // Reads outside the framebuffer are safely initialized to black.
    GLubyte outside[4] = {0xff, 0xff, 0xff, 0xff};
    glReadPixels(-1, -1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, outside);
    assert(outside[0] == 0 && outside[1] == 0 &&
           outside[2] == 0 && outside[3] == 255);

    glPixelStorei(GL_PACK_ALIGNMENT, 4);

    // An invalid attachment point is an enum error and does not mutate the FBO.
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, 0x8ce1,
        GL_RENDERBUFFER, renderbuffers[0]);
    assert(glGetError() == GL_INVALID_ENUM);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
        GL_RENDERBUFFER, 0);
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
        GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &value);
    assert(value == GL_NONE);

    // Depth-only and stencil-only targets are complete and clearable.
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D, 0, 0);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    glClearStencil(0x35);
    glClear(GL_STENCIL_BUFFER_BIT);

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
        GL_RENDERBUFFER, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
        GL_RENDERBUFFER, renderbuffers[0]);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    glClearDepthf(0.25f);
    glClear(GL_DEPTH_BUFFER_BIT);

    // Restore the texture attachment, then deleting it must detach it.
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D, texture, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
        GL_RENDERBUFFER, 0);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    glDeleteTextures(1, &texture);
    assert(glIsTexture(texture) == GL_FALSE);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) ==
        GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT);

    glDeleteRenderbuffers(3, renderbuffers);
    assert(glIsRenderbuffer(renderbuffers[0]) == GL_FALSE);
    assert(glIsRenderbuffer(renderbuffers[1]) == GL_FALSE);
    assert(glIsRenderbuffer(renderbuffers[2]) == GL_FALSE);
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &value);
    assert(value == 0);

    glDeleteFramebuffers(1, &framebuffer);
    assert(glIsFramebuffer(framebuffer) == GL_FALSE);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &value);
    assert(value == 0);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    assert(rgba[0] == 0 && rgba[1] == 0 && rgba[2] == 0 && rgba[3] == 255);

    // Bad targets, formats and dimensions are rejected with GLES errors.
    glBindFramebuffer(GL_RENDERBUFFER, 0);
    assert(glGetError() == GL_INVALID_ENUM);
    GLuint rb = 0;
    glGenRenderbuffers(1, &rb);
    glBindRenderbuffer(GL_RENDERBUFFER, rb);
    glRenderbufferStorage(GL_RENDERBUFFER, 0xdead, 4, 4);
    assert(glGetError() == GL_INVALID_ENUM);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB565, 129, 4);
    assert(glGetError() == GL_INVALID_VALUE);
    glDeleteRenderbuffers(1, &rb);
    glReadPixels(0, 0, 1, 1, 0xdead, GL_UNSIGNED_BYTE, rgba);
    assert(glGetError() == GL_INVALID_ENUM);
    glReadPixels(0, 0, -1, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    assert(glGetError() == GL_INVALID_VALUE);

    fxglReleaseCurrent();
}

void test_state_validation_and_error_latching()
{
    uint16_t color[4] = {};
    fxgl_surface_t surface = {color, nullptr, nullptr, 2, 2, 2};
    assert(fxglMakeCurrent(&surface) == GL_TRUE);

    glDepthFunc(0xdead);
    glViewport(0, 0, -1, 2);
    assert(glGetError() == GL_INVALID_ENUM);
    assert(glGetError() == GL_NO_ERROR);

    glScissor(0, 0, -1, 2);
    assert(glGetError() == GL_INVALID_VALUE);

    glEnable(0xdead);
    assert(glGetError() == GL_INVALID_ENUM);

    glCullFace(GL_FRONT_AND_BACK);
    glFrontFace(GL_CW);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
    assert(glGetError() == GL_NO_ERROR);

    GLint value = 0;
    glGetIntegerv(GL_CULL_FACE_MODE, &value);
    assert(value == GL_FRONT_AND_BACK);
    glGetIntegerv(GL_FRONT_FACE, &value);
    assert(value == GL_CW);
    glGetIntegerv(GL_BLEND_SRC_RGB, &value);
    assert(value == GL_SRC_ALPHA);
    glGetIntegerv(GL_BLEND_DST_RGB, &value);
    assert(value == GL_ONE_MINUS_SRC_ALPHA);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &value);
    assert(value == GL_FUNC_REVERSE_SUBTRACT);

    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
    glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_REVERSE_SUBTRACT);
    glBlendColor(0.25f, 0.5f, 0.75f, 0.125f);
    glDepthRangef(0.2f, 0.8f);

    glStencilFuncSeparate(GL_FRONT, GL_GEQUAL, 5, 0x3f);
    glStencilOpSeparate(GL_FRONT, GL_REPLACE, GL_INCR, GL_DECR);
    glStencilMaskSeparate(GL_FRONT, 0x0f);
    glStencilFuncSeparate(GL_BACK, GL_LESS, 2, 0xf0);
    glStencilOpSeparate(GL_BACK, GL_ZERO, GL_KEEP, GL_INVERT);
    glStencilMaskSeparate(GL_BACK, 0xf0);

    glGetIntegerv(GL_BLEND_SRC_ALPHA, &value);
    assert(value == GL_ONE);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &value);
    assert(value == GL_ZERO);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &value);
    assert(value == GL_FUNC_REVERSE_SUBTRACT);

    glGetIntegerv(GL_STENCIL_FUNC, &value);
    assert(value == GL_GEQUAL);
    glGetIntegerv(GL_STENCIL_REF, &value);
    assert(value == 5);
    glGetIntegerv(GL_STENCIL_VALUE_MASK, &value);
    assert(value == 0x3f);
    glGetIntegerv(GL_STENCIL_FAIL, &value);
    assert(value == GL_REPLACE);
    glGetIntegerv(GL_STENCIL_PASS_DEPTH_FAIL, &value);
    assert(value == GL_INCR);
    glGetIntegerv(GL_STENCIL_PASS_DEPTH_PASS, &value);
    assert(value == GL_DECR);
    glGetIntegerv(GL_STENCIL_WRITEMASK, &value);
    assert(value == 0x0f);
    glGetIntegerv(GL_STENCIL_BACK_FUNC, &value);
    assert(value == GL_LESS);
    glGetIntegerv(GL_STENCIL_BACK_REF, &value);
    assert(value == 2);
    glGetIntegerv(GL_STENCIL_BACK_VALUE_MASK, &value);
    assert(value == 0xf0);
    glGetIntegerv(GL_STENCIL_BACK_WRITEMASK, &value);
    assert(value == 0xf0);
    glGetIntegerv(GL_STENCIL_BACK_FAIL, &value);
    assert(value == GL_ZERO);
    glGetIntegerv(GL_STENCIL_BACK_PASS_DEPTH_FAIL, &value);
    assert(value == GL_KEEP);
    glGetIntegerv(GL_STENCIL_BACK_PASS_DEPTH_PASS, &value);
    assert(value == GL_INVERT);

    GLfloat depth_range[2] = {};
    glGetFloatv(GL_DEPTH_RANGE, depth_range);
    assert(depth_range[0] > 0.19f && depth_range[0] < 0.21f);
    assert(depth_range[1] > 0.79f && depth_range[1] < 0.81f);
    GLfloat blend_color[4] = {};
    glGetFloatv(GL_BLEND_COLOR, blend_color);
    assert(blend_color[0] > 0.24f && blend_color[0] < 0.26f);
    assert(blend_color[3] > 0.12f && blend_color[3] < 0.13f);

    glStencilFunc(GL_EQUAL, 9, 0xff);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glGetIntegerv(GL_STENCIL_REF, &value);
    assert(value == 9);
    glGetIntegerv(GL_STENCIL_BACK_REF, &value);
    assert(value == 9);

    glBlendFuncSeparate(GL_ZERO, GL_SRC_ALPHA_SATURATE, GL_ONE, GL_ZERO);
    assert(glGetError() == GL_INVALID_ENUM);
    glStencilOpSeparate(GL_FRONT, 0xdead, GL_KEEP, GL_KEEP);
    assert(glGetError() == GL_INVALID_ENUM);

    fxglReleaseCurrent();
}
} // namespace

static void test_surface_refresh_preserves_gles_state()
{
    uint16_t first[16] = {};
    uint16_t second[16] = {};
    const fxgl_surface_t initial = {first, nullptr, nullptr, 4, 4, 4};
    assert(fxglMakeCurrent(&initial) == GL_TRUE);

    fxgl_surface_t updated = {second, nullptr, nullptr, 4, 4, 4};
    assert(fxglUpdateSurface(&updated) == GL_TRUE);

    // Updating the pointer must not reset state or resource namespaces.
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    for (size_t i = 0; i < 16; ++i)
    {
        assert(first[i] == 0);
        assert(second[i] == 0xf800);
    }

    fxgl_surface_t invalid = {first, nullptr, nullptr, 8, 2, 8};
    assert(fxglUpdateSurface(&invalid) == GL_FALSE);
    fxglReleaseCurrent();
}

void test_mat2_mat3_uniform_api_and_introspection()
{
    uint16_t color[4] = {};
    fxgl_surface_t surface = {color, nullptr, nullptr, 2, 2, 2};
    assert(fxglMakeCurrent(&surface) == GL_TRUE);

    static const GLchar vertex_source[] =
        "#version 100\n"
        "attribute vec4 a_position;\n"
        "uniform mat2 u_transform2;\n"
        "uniform mat3 u_transform3;\n"
        "void main() {\n"
        "  vec2 p = u_transform2 * a_position.xy;\n"
        "  vec3 q = u_transform3 * vec3(p, 1.0);\n"
        "  gl_Position = vec4(q, 1.0);\n"
        "}\n";
    static const GLchar fragment_source[] =
        "#version 100\n"
        "precision mediump float;\n"
        "void main() { gl_FragColor = vec4(1.0); }\n";

    const GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    const GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    const GLchar *vs_parts[] = {vertex_source};
    const GLchar *fs_parts[] = {fragment_source};
    glShaderSource(vs, 1, vs_parts, nullptr);
    glShaderSource(fs, 1, fs_parts, nullptr);
    glCompileShader(vs);
    glCompileShader(fs);
    GLint value = GL_FALSE;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &value);
    assert(value == GL_TRUE);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &value);
    assert(value == GL_TRUE);

    const GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &value);
    assert(value == GL_TRUE);
    glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &value);
    assert(value == 2);

    bool saw_mat2 = false;
    bool saw_mat3 = false;
    for (GLuint i = 0; i < 2; ++i)
    {
        GLchar name[32] = {};
        GLsizei length = 0;
        GLint size = 0;
        GLenum type = 0;
        glGetActiveUniform(program, i, sizeof(name),
            &length, &size, &type, name);
        if (std::strcmp(name, "u_transform2") == 0)
            saw_mat2 = type == GL_FLOAT_MAT2;
        if (std::strcmp(name, "u_transform3") == 0)
            saw_mat3 = type == GL_FLOAT_MAT3;
    }
    assert(saw_mat2 && saw_mat3);

    const GLint mat2_location =
        glGetUniformLocation(program, "u_transform2");
    const GLint mat3_location =
        glGetUniformLocation(program, "u_transform3");
    assert(mat2_location >= 0 && mat3_location >= 0);
    glUseProgram(program);

    const GLfloat mat2[4] = {0.0f, -1.0f, 1.0f, 0.0f};
    const GLfloat mat3[9] = {
        2.0f, 0.0f, 0.0f,
        0.0f, 3.0f, 0.0f,
        0.0f, 0.0f, 4.0f
    };
    glUniformMatrix2fv(mat2_location, 1, GL_FALSE, mat2);
    glUniformMatrix3fv(mat3_location, 1, GL_FALSE, mat3);

    GLfloat retrieved2[4] = {};
    GLfloat retrieved3[9] = {};
    glGetUniformfv(program, mat2_location, retrieved2);
    glGetUniformfv(program, mat3_location, retrieved3);
    for (size_t i = 0; i < 4; ++i) assert(retrieved2[i] == mat2[i]);
    for (size_t i = 0; i < 9; ++i) assert(retrieved3[i] == mat3[i]);

    glUniformMatrix2fv(mat2_location, 1, GL_TRUE, mat2);
    assert(glGetError() == GL_INVALID_OPERATION);
    glUniformMatrix3fv(mat3_location, -1, GL_FALSE, mat3);
    assert(glGetError() == GL_INVALID_VALUE);

    glDeleteProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    fxglReleaseCurrent();
}

void test_integer_and_boolean_vector_uniform_api()
{
    uint16_t color[4] = {};
    fxgl_surface_t surface = {color, nullptr, nullptr, 2, 2, 2};
    assert(fxglMakeCurrent(&surface) == GL_TRUE);

    static const GLchar vertex_source[] =
        "#version 100\n"
        "attribute vec4 a_position;\n"
        "uniform ivec2 u_integer_pair;\n"
        "uniform bvec3 u_boolean_triplet;\n"
        "void main() {\n"
        "  vec2 p = vec2(u_integer_pair);\n"
        "  bvec2 b = greaterThan(p, vec2(0.0));\n"
        "  gl_Position = vec4(p, float(any(b)), 1.0);\n"
        "}\n";
    static const GLchar fragment_source[] =
        "#version 100\n"
        "precision mediump float;\n"
        "uniform bvec3 u_boolean_triplet;\n"
        "void main() { gl_FragColor = vec4(float(all(u_boolean_triplet))); }\n";

    const GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    const GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    const GLchar *vs_source[] = {vertex_source};
    const GLchar *fs_source[] = {fragment_source};
    glShaderSource(vs, 1, vs_source, nullptr);
    glShaderSource(fs, 1, fs_source, nullptr);
    glCompileShader(vs);
    glCompileShader(fs);

    GLint status = GL_FALSE;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &status);
    assert(status == GL_TRUE);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &status);
    assert(status == GL_TRUE);

    const GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    assert(status == GL_TRUE);
    glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &status);
    assert(status == 2);

    bool saw_ivec2 = false;
    bool saw_bvec3 = false;
    for (GLuint i = 0; i < 2; ++i)
    {
        GLchar name[32] = {};
        GLsizei length = 0;
        GLint size = 0;
        GLenum type = 0;
        glGetActiveUniform(program, i, sizeof(name), &length,
            &size, &type, name);
        if (std::strcmp(name, "u_integer_pair") == 0)
            saw_ivec2 = type == GL_INT_VEC2;
        if (std::strcmp(name, "u_boolean_triplet") == 0)
            saw_bvec3 = type == GL_BOOL_VEC3;
    }
    assert(saw_ivec2 && saw_bvec3);

    const GLint ivec_location = glGetUniformLocation(program, "u_integer_pair");
    const GLint bvec_location = glGetUniformLocation(program, "u_boolean_triplet");
    assert(ivec_location >= 0 && bvec_location >= 0);
    glUseProgram(program);

    const GLint integer_values[2] = {4, -2};
    glUniform2iv(ivec_location, 1, integer_values);
    glUniform3i(bvec_location, 0, 9, -1);

    GLint retrieved_integer[2] = {};
    GLint retrieved_boolean[3] = {};
    glGetUniformiv(program, ivec_location, retrieved_integer);
    glGetUniformiv(program, bvec_location, retrieved_boolean);
    assert(retrieved_integer[0] == 4 && retrieved_integer[1] == -2);
    assert(retrieved_boolean[0] == 0 &&
           retrieved_boolean[1] == 1 &&
           retrieved_boolean[2] == 1);

    glUniform2iv(bvec_location, 1, integer_values);
    assert(glGetError() == GL_INVALID_OPERATION);

    glDeleteProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    fxglReleaseCurrent();
}

void test_uniform_array_api_and_draw()
{
    uint16_t color[1] = {};
    fxgl_surface_t surface = {color, nullptr, nullptr, 1, 1, 1};
    assert(fxglMakeCurrent(&surface) == GL_TRUE);

    static const GLchar vertex_source[] =
        "attribute vec4 a_position;\n"
        "uniform vec2 u_offsets[2];\n"
        "void main() {\n"
        "  gl_Position = vec4(a_position.xy + u_offsets[1], 0.0, 1.0);\n"
        "  gl_PointSize = 1.0;\n"
        "}\n";
    static const GLchar fragment_source[] =
        "precision mediump float;\n"
        "uniform vec4 u_colors[3];\n"
        "uniform int u_pick;\n"
        "uniform mat2 u_basis[2];\n"
        "uniform float u_gain;\n"
        "void main() {\n"
        "  gl_FragColor = u_colors[u_pick] * u_basis[1][1].y * u_gain;\n"
        "}\n";
    const GLchar *vertex_parts[] = {vertex_source};
    const GLchar *fragment_parts[] = {fragment_source};
    const GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    const GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(vs, 1, vertex_parts, nullptr);
    glShaderSource(fs, 1, fragment_parts, nullptr);
    glCompileShader(vs);
    glCompileShader(fs);
    GLint status = GL_FALSE;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &status);
    assert(status == GL_TRUE);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &status);
    assert(status == GL_TRUE);

    const GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    assert(status == GL_TRUE);

    /* Each array is one active uniform reported as name[0] with its size. */
    glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &status);
    assert(status == 5);
    bool saw_colors = false;
    bool saw_basis = false;
    bool saw_pick = false;
    for (GLuint i = 0; i < 5; ++i)
    {
        GLchar name[40] = {};
        GLsizei length = 0;
        GLint size = 0;
        GLenum type = 0;
        glGetActiveUniform(program, i, sizeof(name), &length, &size, &type, name);
        assert(length == static_cast<GLsizei>(std::strlen(name)));
        if (std::strcmp(name, "u_colors[0]") == 0)
            saw_colors = size == 3 && type == GL_FLOAT_VEC4;
        if (std::strcmp(name, "u_basis[0]") == 0)
            saw_basis = size == 2 && type == GL_FLOAT_MAT2;
        if (std::strcmp(name, "u_pick") == 0)
            saw_pick = size == 1 && type == GL_INT;
    }
    assert(saw_colors && saw_basis && saw_pick);
    GLchar unused[8];
    glGetActiveUniform(program, 5, sizeof(unused), nullptr, nullptr, nullptr, unused);
    assert(glGetError() == GL_INVALID_VALUE);

    const GLint colors = glGetUniformLocation(program, "u_colors");
    assert(colors >= 0);
    assert(glGetUniformLocation(program, "u_colors[0]") == colors);
    const GLint colors1 = glGetUniformLocation(program, "u_colors[1]");
    assert(colors1 == colors + 1);
    assert(glGetUniformLocation(program, "u_colors[2]") == colors + 2);
    const char *bad_names[] = {
        "u_colors[3]", "u_colors[01]", "u_colors[]", "u_colors[x]",
        "u_colors[1]x", "u_pick[0]", "[0]", "", "u_colors[-1]"
    };
    for (const char *bad : bad_names)
        assert(glGetUniformLocation(program, bad) == -1);
    assert(glGetError() == GL_NO_ERROR);

    const GLint offsets = glGetUniformLocation(program, "u_offsets");
    const GLint pick = glGetUniformLocation(program, "u_pick");
    const GLint basis = glGetUniformLocation(program, "u_basis");
    const GLint basis1 = glGetUniformLocation(program, "u_basis[1]");
    const GLint gain = glGetUniformLocation(program, "u_gain");
    assert(offsets >= 0 && pick >= 0 && basis >= 0 && basis1 == basis + 1 &&
           gain >= 0);
    glUseProgram(program);

    /* count spans consecutive elements; values past the end are ignored. */
    const GLfloat palette[12] = {
        1.0f, 0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    glUniform4fv(colors, 3, palette);
    const GLfloat tail[12] = {
        0.0f, 1.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f, 1.0f,
        9.0f, 9.0f, 9.0f, 9.0f
    };
    glUniform4fv(colors1, 3, tail);
    assert(glGetError() == GL_NO_ERROR);
    GLfloat retrieved[4] = {};
    glGetUniformfv(program, colors, retrieved);
    assert(retrieved[0] == 1.0f && retrieved[1] == 0.0f);
    glGetUniformfv(program, colors1, retrieved);
    assert(retrieved[1] == 1.0f && retrieved[2] == 0.0f);
    glGetUniformfv(program, colors + 2, retrieved);
    assert(retrieved[1] == 0.0f && retrieved[2] == 1.0f);
    /* The excess value did not spill into the next uniform. */
    glGetUniformfv(program, colors + 3, retrieved);
    assert(retrieved[0] != 9.0f);

    const GLfloat offset_values[4] = {5.0f, 5.0f, 0.0f, 0.0f};
    glUniform2fv(offsets, 2, offset_values);
    const GLfloat bases[8] = {
        0.0f, 0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f, 1.0f
    };
    glUniformMatrix2fv(basis, 2, GL_FALSE, bases);
    GLfloat retrieved_matrix[4] = {};
    glGetUniformfv(program, basis1, retrieved_matrix);
    assert(retrieved_matrix[0] == 1.0f && retrieved_matrix[3] == 1.0f);
    const GLfloat one = 1.0f;
    glUniform1fv(gain, 1, &one);
    assert(glGetError() == GL_NO_ERROR);

    /* count > 1 needs an array; a negative count is invalid. */
    const GLfloat two[2] = {1.0f, 1.0f};
    glUniform1fv(gain, 2, two);
    assert(glGetError() == GL_INVALID_OPERATION);
    glUniform4fv(colors, -1, palette);
    assert(glGetError() == GL_INVALID_VALUE);
    const GLint picks[2] = {0, 0};
    glUniform1iv(pick, 2, picks);
    assert(glGetError() == GL_INVALID_OPERATION);

    /* Draw one point; the fragment colour is u_colors[u_pick]. The vertex
       stage reads u_offsets[1] (zero), so a wrong element would move the
       point off the 1x1 surface. */
    const GLint position = glGetAttribLocation(program, "a_position");
    assert(position >= 0);
    const GLfloat center[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    glVertexAttribPointer(static_cast<GLuint>(position), 4, GL_FLOAT,
                          GL_FALSE, 0, center);
    glEnableVertexAttribArray(static_cast<GLuint>(position));
    glViewport(0, 0, 1, 1);
    const uint16_t expected[3] = {RED, GREEN, BLUE};
    for (GLint index = 0; index < 3; ++index)
    {
        color[0] = SENTINEL;
        glUniform1i(pick, index);
        glDrawArrays(GL_POINTS, 0, 1);
        assert(glGetError() == GL_NO_ERROR);
        assert(color[0] == expected[index]);
    }

    /* An out-of-range dynamic index fails the fragment, not memory. */
    color[0] = SENTINEL;
    glUniform1i(pick, 3);
    glDrawArrays(GL_POINTS, 0, 1);
    assert(glGetError() == GL_INVALID_OPERATION);
    assert(color[0] == SENTINEL);

    glDeleteProgram(program);
    glDeleteShader(fs);

    /* Stages must agree on a shared uniform array's size. */
    static const GLchar mismatched_fragment[] =
        "precision mediump float;\n"
        "uniform vec2 u_offsets[3];\n"
        "void main() { gl_FragColor = vec4(u_offsets[2], 0.0, 1.0); }\n";
    const GLchar *mismatched_parts[] = {mismatched_fragment};
    const GLuint mismatched_fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(mismatched_fs, 1, mismatched_parts, nullptr);
    glCompileShader(mismatched_fs);
    glGetShaderiv(mismatched_fs, GL_COMPILE_STATUS, &status);
    assert(status == GL_TRUE);
    const GLuint mismatched = glCreateProgram();
    glAttachShader(mismatched, vs);
    glAttachShader(mismatched, mismatched_fs);
    glLinkProgram(mismatched);
    glGetProgramiv(mismatched, GL_LINK_STATUS, &status);
    assert(status == GL_FALSE);

    glDeleteProgram(mismatched);
    glDeleteShader(mismatched_fs);
    glDeleteShader(vs);
    fxglReleaseCurrent();
}

void test_remaining_gles2_entry_points()
{
    uint16_t color[4] = {};
    fxgl_surface_t surface = {color, nullptr, nullptr, 2, 2, 2};
    assert(fxglMakeCurrent(&surface) == GL_TRUE);

    glFlush();
    glFinish();
    glReleaseShaderCompiler();
    assert(glGetError() == GL_NO_ERROR);

    // glHint: only GL_GENERATE_MIPMAP_HINT with a valid mode.
    GLint value = -1;
    glGetIntegerv(GL_GENERATE_MIPMAP_HINT, &value);
    assert(value == GL_DONT_CARE);
    glHint(GL_GENERATE_MIPMAP_HINT, GL_NICEST);
    glGetIntegerv(GL_GENERATE_MIPMAP_HINT, &value);
    assert(value == GL_NICEST && glGetError() == GL_NO_ERROR);
    glHint(GL_GENERATE_MIPMAP_HINT, GL_ZERO);
    assert(glGetError() == GL_INVALID_ENUM);
    glHint(GL_DEPTH_TEST, GL_FASTEST);
    assert(glGetError() == GL_INVALID_ENUM);

    // glLineWidth: stored as given, lines rasterize within [1, 1].
    GLfloat floats[4] = {};
    glGetFloatv(GL_LINE_WIDTH, floats);
    assert(floats[0] == 1.0f);
    glLineWidth(2.5f);
    glGetFloatv(GL_LINE_WIDTH, floats);
    assert(floats[0] == 2.5f && glGetError() == GL_NO_ERROR);
    glLineWidth(0.0f);
    assert(glGetError() == GL_INVALID_VALUE);
    glLineWidth(-1.0f);
    assert(glGetError() == GL_INVALID_VALUE);
    glGetFloatv(GL_ALIASED_LINE_WIDTH_RANGE, floats);
    assert(floats[0] == 1.0f && floats[1] == 1.0f);

    // Sample coverage is clamped and stored; there are no sample buffers.
    glSampleCoverage(1.5f, GL_TRUE);
    glGetFloatv(GL_SAMPLE_COVERAGE_VALUE, floats);
    assert(floats[0] == 1.0f);
    GLboolean flag = GL_FALSE;
    glGetBooleanv(GL_SAMPLE_COVERAGE_INVERT, &flag);
    assert(flag == GL_TRUE);
    glGetIntegerv(GL_SAMPLE_BUFFERS, &value);
    assert(value == 0);
    glEnable(GL_SAMPLE_COVERAGE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    assert(glIsEnabled(GL_SAMPLE_COVERAGE) == GL_TRUE);
    assert(glIsEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE) == GL_FALSE);
    assert(glIsEnabled(GL_POLYGON_OFFSET_FILL) == GL_TRUE);
    assert(glGetError() == GL_NO_ERROR);

    // No compressed or binary shader formats exist.
    glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &value);
    assert(value == 0);
    glGetIntegerv(GL_NUM_SHADER_BINARY_FORMATS, &value);
    assert(value == 0);
    glGetIntegerv(GL_SHADER_COMPILER, &value);
    assert(value == GL_TRUE);
    GLint sentinel = 1234;
    glGetIntegerv(GL_COMPRESSED_TEXTURE_FORMATS, &sentinel);
    assert(sentinel == 1234 && glGetError() == GL_NO_ERROR);
    const uint8_t blob[4] = {};
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, 4, blob);
    assert(glGetError() == GL_INVALID_ENUM);
    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGB, 4, blob);
    assert(glGetError() == GL_INVALID_ENUM);
    const GLuint shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderBinary(1, &shader, 0, blob, 4);
    assert(glGetError() == GL_INVALID_ENUM);
    glShaderBinary(-1, &shader, 0, blob, 4);
    assert(glGetError() == GL_INVALID_VALUE);
    glDeleteShader(shader);

    // Shader precision: IEEE single floats, integers exact to 2^24.
    GLint range[2] = {};
    GLint precision = -1;
    glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_HIGH_FLOAT, range, &precision);
    assert(range[0] == 127 && range[1] == 127 && precision == 23);
    glGetShaderPrecisionFormat(GL_VERTEX_SHADER, GL_LOW_INT, range, &precision);
    assert(range[0] == 24 && range[1] == 24 && precision == 0);
    glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_FLOAT, range, &precision);
    assert(glGetError() == GL_INVALID_ENUM);
    glGetShaderPrecisionFormat(GL_TEXTURE_2D, GL_HIGH_FLOAT, range, &precision);
    assert(glGetError() == GL_INVALID_ENUM);

    // Float/vector texture parameter forms route through glTexParameteri.
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<GLfloat>(GL_LINEAR));
    const GLint wrap = GL_MIRRORED_REPEAT;
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &wrap);
    const GLfloat clamp = static_cast<GLfloat>(GL_CLAMP_TO_EDGE);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &clamp);
    assert(glGetError() == GL_NO_ERROR);
    glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, floats);
    assert(floats[0] == static_cast<GLfloat>(GL_LINEAR));
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &value);
    assert(value == GL_MIRRORED_REPEAT);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &value);
    assert(value == GL_CLAMP_TO_EDGE);
    floats[0] = 42.0f;
    glGetTexParameterfv(GL_TEXTURE_2D, GL_DEPTH_TEST, floats);
    assert(glGetError() == GL_INVALID_ENUM && floats[0] == 42.0f);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, nullptr);
    assert(glGetError() == GL_INVALID_VALUE);
    glDeleteTextures(1, &texture);

    // Vertex attribute vector setters and float queries.
    const GLfloat v3[3] = {1.0f, 2.0f, 3.0f};
    glVertexAttrib1fv(1, v3);
    glGetVertexAttribfv(1, GL_CURRENT_VERTEX_ATTRIB, floats);
    assert(floats[0] == 1.0f && floats[1] == 0.0f && floats[2] == 0.0f && floats[3] == 1.0f);
    glVertexAttrib2fv(1, v3);
    glGetVertexAttribfv(1, GL_CURRENT_VERTEX_ATTRIB, floats);
    assert(floats[0] == 1.0f && floats[1] == 2.0f && floats[2] == 0.0f);
    glVertexAttrib3fv(1, v3);
    glGetVertexAttribfv(1, GL_CURRENT_VERTEX_ATTRIB, floats);
    assert(floats[2] == 3.0f && floats[3] == 1.0f);
    GLint ints[4] = {};
    glGetVertexAttribiv(1, GL_CURRENT_VERTEX_ATTRIB, ints);
    assert(ints[0] == 1 && ints[1] == 2 && ints[2] == 3 && ints[3] == 1);
    glGetVertexAttribfv(1, GL_VERTEX_ATTRIB_ARRAY_ENABLED, floats);
    assert(floats[0] == 0.0f);
    glVertexAttrib3fv(1, nullptr);
    assert(glGetError() == GL_INVALID_VALUE);
    glGetVertexAttribfv(99, GL_CURRENT_VERTEX_ATTRIB, floats);
    assert(glGetError() == GL_INVALID_VALUE);
    assert(glGetError() == GL_NO_ERROR);

    // With an earlier error still pending, valid float/boolean queries
    // still answer, and the first error is what glGetError reports.
    glLineWidth(-3.0f);
    floats[0] = 0.0f;
    glGetFloatv(GL_DEPTH_FUNC, floats);
    assert(floats[0] == static_cast<GLfloat>(GL_LESS));
    flag = GL_FALSE;
    glGetBooleanv(GL_SHADER_COMPILER, &flag);
    assert(flag == GL_TRUE);
    glGetTexParameterfv(GL_TEXTURE_2D, GL_DEPTH_TEST, floats);
    assert(glGetError() == GL_INVALID_VALUE);
    assert(glGetError() == GL_NO_ERROR);

    fxglReleaseCurrent();
}

void test_gles_mipmap_generation_and_level_uploads()
{
    uint16_t color[16] = {};
    fxgl_surface_t surface = {color, nullptr, nullptr, 4, 4, 4};
    assert(fxglMakeCurrent(&surface) == GL_TRUE);

    const uint16_t base_pixels[16] = {
        0xf800, 0xf800, 0x07e0, 0x07e0,
        0xf800, 0xf800, 0x07e0, 0x07e0,
        0x001f, 0x001f, 0xffff, 0xffff,
        0x001f, 0x001f, 0xffff, 0xffff
    };

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 4, 4, 0,
        GL_RGB, GL_UNSIGNED_SHORT_5_6_5, base_pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
        GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    gfx::Texture2DView view = {};
    gfx::TextureSampler sampler = {};
    assert(fxgl::get_texture_binding(0, view, sampler));
    assert(view.mip_level_count == 0);
    assert(glGetError() == GL_NO_ERROR);
    // A mipmapped sampler with an incomplete chain is an incomplete texture,
    // not an API error. Sampling it returns black.
    assert(gfx::sample_texture(view, 0, 0, sampler, 1.0f) == 0);

    glGenerateMipmap(GL_TEXTURE_2D);
    assert(glGetError() == GL_NO_ERROR);
    assert(fxgl::get_texture_binding(0, view, sampler));
    assert(view.mip_level_count == 2);
    assert(view.mip_levels[0].width == 2 &&
           view.mip_levels[0].height == 2);
    assert(view.mip_levels[1].width == 1 &&
           view.mip_levels[1].height == 1);
    assert(sampler.min_filter == gfx::TextureMinFilter::LinearMipmapLinear);

    // The four 2x2 base-image quadrants are solid colors, so the first mip
    // level preserves each quadrant's RGB565 value exactly.
    assert(view.mip_levels[0].pixels[0] == 0xf800);
    assert(view.mip_levels[0].pixels[1] == 0x07e0);
    assert(view.mip_levels[0].pixels[2] == 0x001f);
    assert(view.mip_levels[0].pixels[3] == 0xffff);

    // Updating an explicit mip level must address that level, not level zero.
    const uint16_t green = 0x07e0;
    glTexSubImage2D(GL_TEXTURE_2D, 1, 0, 0, 1, 1,
        GL_RGB, GL_UNSIGNED_SHORT_5_6_5, &green);
    assert(glGetError() == GL_NO_ERROR);
    assert(fxgl::get_texture_binding(0, view, sampler));
    assert(view.mip_levels[0].pixels[0] == green);

    // Replacing level zero invalidates the lower mip chain. The object
    // remains bindable, but mipmapped sampling returns black until levels
    // are regenerated or uploaded again.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 4, 4, 0,
        GL_RGB, GL_UNSIGNED_SHORT_5_6_5, base_pixels);
    assert(glGetError() == GL_NO_ERROR);
    assert(fxgl::get_texture_binding(0, view, sampler));
    assert(view.mip_level_count == 0);
    assert(gfx::sample_texture(view, 0, 0, sampler, 1.0f) == 0);

    // An explicit mip level with the wrong expected dimensions is rejected.
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGB, 1, 1, 0,
        GL_RGB, GL_UNSIGNED_SHORT_5_6_5, &green);
    assert(glGetError() == GL_INVALID_VALUE);

    // Correct explicit mip dimensions restore completeness when all levels
    // are supplied.
    const uint16_t level1[4] = {0xf800, 0x07e0, 0x001f, 0xffff};
    const uint16_t level2 = 0x8410;
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGB, 2, 2, 0,
        GL_RGB, GL_UNSIGNED_SHORT_5_6_5, level1);
    glTexImage2D(GL_TEXTURE_2D, 2, GL_RGB, 1, 1, 0,
        GL_RGB, GL_UNSIGNED_SHORT_5_6_5, &level2);
    assert(glGetError() == GL_NO_ERROR);
    assert(fxgl::get_texture_binding(0, view, sampler));
    assert(view.mip_level_count == 2);
    assert(view.mip_levels[1].pixels[0] == level2);

    GLint filter = 0;
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &filter);
    assert(filter == GL_LINEAR_MIPMAP_LINEAR);
    glDeleteTextures(1, &texture);
    fxglReleaseCurrent();
}

int main()
{
    test_surface_binding_and_default_state();
    test_surface_refresh_preserves_gles_state();
    test_rgb565_clear_and_stride_padding();
    test_scissor_color_mask_and_depth_stencil_clear();
    test_outside_scissor_is_safe_noop();
    test_buffer_objects_and_vertex_attributes();
    test_texture_objects_and_uploads();
    test_rgb565_unpack_alignment();
    test_state_validation_and_error_latching();
    test_gles_framebuffer_and_renderbuffer_objects();
    test_shader_and_program_object_lifecycle();
    test_mat2_mat3_uniform_api_and_introspection();
    test_integer_and_boolean_vector_uniform_api();
    test_uniform_array_api_and_draw();
    test_remaining_gles2_entry_points();
    test_gles_mipmap_generation_and_level_uploads();
    test_gles_draw_arrays_and_elements();
    test_draw_call_texture_cache_refreshes_between_draws();
    test_shader_compile_and_program_link_failures();
    test_context_drives_software_pipeline();

    std::puts("GLES-style context/state tests: PASS");
    return 0;
}
