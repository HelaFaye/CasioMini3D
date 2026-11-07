#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>

#include <GLES2/gl2.h>
#include <fxgl/platform.h>

namespace
{
constexpr uint16_t RED = 0xf800;
constexpr uint16_t GREEN = 0x07e0;
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

    const GLfloat vertices[6] = {0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
    glBindBuffer(GL_ARRAY_BUFFER, names[0]);
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
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &value);
    assert(value == 8);
    assert(glGetError() == GL_NO_ERROR);

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

    fxglReleaseCurrent();
}
} // namespace

int main()
{
    test_surface_binding_and_default_state();
    test_rgb565_clear_and_stride_padding();
    test_scissor_color_mask_and_depth_stencil_clear();
    test_outside_scissor_is_safe_noop();
    test_buffer_objects_and_vertex_attributes();
    test_state_validation_and_error_latching();

    std::puts("GLES-style context/state tests: PASS");
    return 0;
}
