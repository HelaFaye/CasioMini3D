#include <GLES2/gl2.h>
#include <fxgl/platform.h>
#include <gint/display.h>
#include <gint/gint.h>

#include <stddef.h>
#include <stdint.h>

namespace
{
constexpr uint16_t kRed = 0xf800;
constexpr uint16_t kPassMarker = 0x07e0;
constexpr uint16_t kFailMarker = 0xf800;

static const GLchar kVertexSource[] =
    "attribute vec4 a_position;\n"
    "void set_size(out float size);\n"
    "void enlarge(inout float size);\n"
    "void main() {\n"
    "  gl_Position = a_position;\n"
    "  set_size(gl_PointSize);\n"
    "  enlarge(gl_PointSize);\n"
    "}\n"
    "void set_size(out float size) { size = 2.5; }\n"
    "void enlarge(inout float size) { size = size + 0.5; }\n";

static const GLchar kFragmentSource[] =
    "precision mediump float;\n"
    "void main() { gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0); }\n";

void publish_result(bool passed)
{
    fxglReleaseCurrent();
    reinterpret_cast<uint16_t *>(gint_vram)[0] =
        passed ? kPassMarker : kFailMarker;
    dupdate();
}
} // namespace

int main()
{
    uint16_t *const pixels = reinterpret_cast<uint16_t *>(gint_vram);
    for (size_t i = 0; i < static_cast<size_t>(DWIDTH) * DHEIGHT; ++i)
        pixels[i] = 0;

    const fxgl_surface_t surface = {
        pixels, nullptr, nullptr, DWIDTH, DHEIGHT, DWIDTH
    };
    bool passed = fxglMakeCurrent(&surface) == GL_TRUE;

    GLuint vertex_shader = 0;
    GLuint fragment_shader = 0;
    GLuint program = 0;
    if (passed)
    {
        vertex_shader = glCreateShader(GL_VERTEX_SHADER);
        fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
        passed = vertex_shader != 0 && fragment_shader != 0;
    }

    if (passed)
    {
        const GLchar *const vertex_parts[] = {kVertexSource};
        const GLchar *const fragment_parts[] = {kFragmentSource};
        glShaderSource(vertex_shader, 1, vertex_parts, nullptr);
        glShaderSource(fragment_shader, 1, fragment_parts, nullptr);
        glCompileShader(vertex_shader);
        glCompileShader(fragment_shader);

        GLint vertex_compiled = GL_FALSE;
        GLint fragment_compiled = GL_FALSE;
        glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &vertex_compiled);
        glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &fragment_compiled);
        passed = vertex_compiled == GL_TRUE && fragment_compiled == GL_TRUE;
    }

    if (passed)
    {
        program = glCreateProgram();
        passed = program != 0;
    }
    if (passed)
    {
        glAttachShader(program, vertex_shader);
        glAttachShader(program, fragment_shader);
        glLinkProgram(program);
        GLint linked = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        passed = linked == GL_TRUE;
    }

    if (passed)
    {
        glUseProgram(program);
        const GLint position_location = glGetAttribLocation(program, "a_position");
        passed = position_location >= 0;

        if (passed)
        {
            const GLfloat point_position[4] = {0.0f, 0.0f, 0.0f, 1.0f};
            glViewport(0, 0, DWIDTH, DHEIGHT);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            glVertexAttribPointer(
                static_cast<GLuint>(position_location), 4, GL_FLOAT,
                GL_FALSE, 0, point_position);
            glEnableVertexAttribArray(static_cast<GLuint>(position_location));
            glDrawArrays(GL_POINTS, 0, 1);
            passed = glGetError() == GL_NO_ERROR;
        }
    }

    if (passed)
    {
        GLfloat point_size_range[2] = {};
        glGetFloatv(GL_ALIASED_POINT_SIZE_RANGE, point_size_range);
        passed = point_size_range[0] == 1.0f &&
                 point_size_range[1] == 16.0f;
    }

    if (passed)
    {
        size_t red_pixels = 0;
        const int center_x = DWIDTH / 2;
        const int center_y = DHEIGHT / 2;
        for (int y = center_y - 2; y <= center_y + 2; ++y)
        {
            for (int x = center_x - 2; x <= center_x + 2; ++x)
            {
                const bool in_expected_square =
                    x >= center_x - 1 && x <= center_x + 1 &&
                    y >= center_y - 1 && y <= center_y + 1;
                const bool is_red =
                    pixels[static_cast<size_t>(y) * DWIDTH + x] == kRed;
                if (is_red)
                    ++red_pixels;
                if (is_red != in_expected_square)
                    passed = false;
            }
        }
        passed = passed && red_pixels == 9;
    }

    publish_result(passed);
    for (;;)
        __asm__ volatile("nop");
}
