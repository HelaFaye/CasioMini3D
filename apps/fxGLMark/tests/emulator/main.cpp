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

struct ExpectedRect
{
    int x;
    int y;
    int width;
    int height;
};

/* Pixel rectangles produced by the point tests below. Keep this list in sync
   with the independent framebuffer checks in casio-emu-headless. */
constexpr ExpectedRect kExpectedRedRects[] = {
    {50, 40, 1, 1},       // Point size 1.
    {119, 39, 3, 3},      // Point size 3, assigned through out/inout calls.
    {212, 32, 16, 16},    // Maximum advertised size.
    {312, 32, 16, 16},    // Oversize request clamps to 16.
    {50, 100, 1, 1},      // Zero size clamps to 1.
    {90, 100, 1, 1},      // Fractional size below 1 clamps to 1.
    {130, 100, 1, 1},     // Negative size clamps to 1.
    {97, 54, 5, 5},       // 5x5 point clipped by a half-size viewport.
    {168, 148, 5, 5},     // 7x7 point clipped by a 5x5 scissor.
    {391, 219, 5, 5},     // 7x7 point clipped by the framebuffer edge.
};

static const GLchar kVertexSource[] =
    "attribute vec4 a_position;\n"
    "void set_size(out float size, float requested);\n"
    "void preserve_size(inout float size);\n"
    "void main() {\n"
    "  gl_Position = vec4(a_position.x, a_position.y, 0.0, 1.0);\n"
    "  set_size(gl_PointSize, a_position.z);\n"
    "  preserve_size(gl_PointSize);\n"
    "}\n"
    "void set_size(out float size, float requested) { size = requested; }\n"
    "void preserve_size(inout float size) { size = size; }\n";

static const GLchar kFragmentSource[] =
    "precision mediump float;\n"
    "void main() { gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0); }\n";

float clip_coordinate_for_pixel(int pixel, int extent)
{
    /* Keep integer screen centers just to the positive side of the exact
       boundary so float conversion cannot round a coverage edge down. */
    return (2.0f * static_cast<float>(pixel) /
            static_cast<float>(extent)) - 1.0f + 0.00001f;
}

bool issue_point_draw(GLuint position_location,
                      GLfloat clip_x, GLfloat clip_y, GLfloat point_size)
{
    const GLfloat position[4] = {clip_x, clip_y, point_size, 1.0f};
    glVertexAttribPointer(position_location, 4, GL_FLOAT, GL_FALSE, 0, position);
    glDrawArrays(GL_POINTS, 0, 1);
    return glGetError() == GL_NO_ERROR;
}

bool draw_point_at_pixel(GLuint position_location,
                         int x, int y, GLfloat point_size)
{
    return issue_point_draw(position_location,
        clip_coordinate_for_pixel(x, DWIDTH),
        clip_coordinate_for_pixel(y, DHEIGHT),
        point_size);
}

bool expected_red_pixel(int x, int y)
{
    for (const ExpectedRect& rect : kExpectedRedRects)
    {
        if (x >= rect.x && x < rect.x + rect.width &&
            y >= rect.y && y < rect.y + rect.height)
            return true;
    }
    return false;
}

bool framebuffer_matches_expected(const uint16_t *pixels)
{
    for (int y = 0; y < DHEIGHT; ++y)
    {
        for (int x = 0; x < DWIDTH; ++x)
        {
            const bool expected = expected_red_pixel(x, y);
            const bool actual =
                pixels[static_cast<size_t>(y) * DWIDTH + x] == kRed;
            if (actual != expected)
                return false;
        }
    }
    return true;
}

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

    GLint position_location = -1;
    if (passed)
    {
        glUseProgram(program);
        position_location = glGetAttribLocation(program, "a_position");
        passed = position_location >= 0;
    }

    if (passed)
    {
        glViewport(0, 0, DWIDTH, DHEIGHT);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glEnableVertexAttribArray(static_cast<GLuint>(position_location));
        passed = glGetError() == GL_NO_ERROR;
    }

    if (passed)
    {
        GLfloat float_range[2] = {};
        GLint integer_range[2] = {};
        glGetFloatv(GL_ALIASED_POINT_SIZE_RANGE, float_range);
        glGetIntegerv(GL_ALIASED_POINT_SIZE_RANGE, integer_range);
        passed = float_range[0] == 1.0f && float_range[1] == 16.0f &&
                 integer_range[0] == 1 && integer_range[1] == 16 &&
                 glGetError() == GL_NO_ERROR;
    }

    if (passed)
    {
        /* Core sizes and the implementation's 1..16 clamp. */
        passed = draw_point_at_pixel(position_location, 50, 40, 1.0f) &&
                 draw_point_at_pixel(position_location, 120, 40, 3.0f) &&
                 draw_point_at_pixel(position_location, 220, 40, 16.0f) &&
                 draw_point_at_pixel(position_location, 320, 40, 32.0f);
    }

    if (passed)
    {
        /* Non-positive and sub-pixel requests clamp to the minimum size. */
        passed = draw_point_at_pixel(position_location, 50, 100, 0.0f) &&
                 draw_point_at_pixel(position_location, 90, 100, 0.25f) &&
                 draw_point_at_pixel(position_location, 130, 100, -2.0f);
    }

    if (passed)
    {
        /* The viewport clips point coverage after point-size expansion. */
        glViewport(0, 0, DWIDTH / 2, DHEIGHT / 2);
        passed = issue_point_draw(position_location, 0.0f, 0.0f, 5.0f);
        glViewport(0, 0, DWIDTH, DHEIGHT);
        passed = passed && glGetError() == GL_NO_ERROR;
    }

    if (passed)
    {
        /* Scissor clips the expanded square, not just its center. */
        glEnable(GL_SCISSOR_TEST);
        glScissor(168, 148, 5, 5);
        passed = draw_point_at_pixel(position_location, 170, 150, 7.0f);
        glDisable(GL_SCISSOR_TEST);
        passed = passed && glGetError() == GL_NO_ERROR;
    }

    if (passed)
    {
        /* A partially off-screen square is clipped to the color surface. */
        passed = draw_point_at_pixel(
            position_location, DWIDTH - 2, DHEIGHT - 2, 7.0f);
    }

    if (passed)
    {
        /* The center outside homogeneous clip space must emit no fragments. */
        passed = issue_point_draw(position_location, 1.2f, 0.0f, 7.0f);
    }

    if (passed)
        passed = framebuffer_matches_expected(pixels);

    publish_result(passed);
    for (;;)
        __asm__ volatile("nop");
}
