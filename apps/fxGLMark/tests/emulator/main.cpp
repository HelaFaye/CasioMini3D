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
constexpr uint16_t kBlue = 0x001f;
constexpr uint16_t kBlendedRedBlue = 0x8010;
constexpr uint16_t kGreenOnBlue = 0x07ff;
constexpr uint16_t kTextureGreen = 0x07e0;
constexpr uint16_t kTextureWhite = 0xffff;
constexpr uint16_t kTriangleCyan = 0x07ff;
constexpr uint16_t kIndexedTriangleMagenta = 0xf81f;
constexpr uint16_t kArrayYellow = 0xffe0;
static uint16_t depth_pixels[DWIDTH * DHEIGHT];
static uint8_t stencil_pixels[DWIDTH * DHEIGHT];

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
    "attribute vec4 a_color;\n"
    "attribute vec2 a_uv;\n"
    "varying vec4 v_color;\n"
    "varying vec2 v_uv;\n"
    "void set_size(out float size, float requested);\n"
    "void preserve_size(inout float size);\n"
    "void main() {\n"
    "  gl_Position = vec4(a_position.x, a_position.y, a_position.w, 1.0);\n"
    "  v_color = a_color;\n"
    "  v_uv = a_uv;\n"
    "  set_size(gl_PointSize, a_position.z);\n"
    "  preserve_size(gl_PointSize);\n"
    "}\n"
    "void set_size(out float size, float requested) { size = requested; }\n"
    "void preserve_size(inout float size) { size = size; }\n";

static const GLchar kFragmentSource[] =
    "precision mediump float;\n"
    "uniform sampler2D u_texture;\n"
    "varying vec4 v_color;\n"
    "varying vec2 v_uv;\n"
    "void main() { gl_FragColor = v_color * texture2D(u_texture, v_uv); }\n";

/* A second program for GLSL local arrays. gate[] = {-1, 0, 1} is written
   through a loop (dynamic) index; k = gate[2] - gate[0] - 1 is 1 only when
   dynamic writes, the dynamic read and constant reads all address their own
   slots. palette[0] is subtracted, so constant indices that alias the same
   slot cancel to black. Every mis-addressed variant clamps to black. */
static const GLchar kArrayVertexSource[] =
    "attribute vec4 a_position;\n"
    "void main() {\n"
    "  gl_Position = vec4(a_position.x, a_position.y, a_position.w, 1.0);\n"
    "  gl_PointSize = a_position.z;\n"
    "}\n";

static const GLchar kArrayFragmentSource[] =
    "precision mediump float;\n"
    "void main() {\n"
    "  vec4 palette[2];\n"
    "  float gate[3];\n"
    "  palette[0] = vec4(0.0, 0.0, 1.0, 0.0);\n"
    "  palette[1] = vec4(0.0);\n"
    "  palette[1].rg = vec2(1.0, 1.0);\n"
    "  palette[1].a = 1.0;\n"
    "  for (int i = 0; i < 3; i++) { gate[i] = float(i) - 1.0; }\n"
    "  int pick = 2;\n"
    "  float k = gate[pick] - gate[0] - 1.0;\n"
    "  gl_FragColor = palette[1] * k - palette[0];\n"
    "}\n";

GLuint build_program(const GLchar *vertex_source,
                     const GLchar *fragment_source)
{
    const GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    const GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    if (vertex_shader == 0 || fragment_shader == 0)
        return 0;
    glShaderSource(vertex_shader, 1, &vertex_source, nullptr);
    glShaderSource(fragment_shader, 1, &fragment_source, nullptr);
    glCompileShader(vertex_shader);
    glCompileShader(fragment_shader);
    GLint vertex_compiled = GL_FALSE;
    GLint fragment_compiled = GL_FALSE;
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &vertex_compiled);
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &fragment_compiled);
    if (vertex_compiled != GL_TRUE || fragment_compiled != GL_TRUE)
        return 0;

    const GLuint program = glCreateProgram();
    if (program == 0)
        return 0;
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    return linked == GL_TRUE ? program : 0;
}

float clip_coordinate_for_pixel(int pixel, int extent)
{
    /* Keep integer screen centers just to the positive side of the exact
       boundary so float conversion cannot round a coverage edge down. */
    return (2.0f * static_cast<float>(pixel) /
            static_cast<float>(extent)) - 1.0f + 0.00001f;
}

float clip_coordinate_for_screen(float screen_coordinate, int extent)
{
    return (2.0f * screen_coordinate / static_cast<float>(extent)) - 1.0f;
}

bool issue_point_draw(GLuint position_location,
                      GLfloat clip_x, GLfloat clip_y, GLfloat point_size,
                      GLfloat clip_z = 0.0f)
{
    const GLfloat position[4] = {clip_x, clip_y, point_size, clip_z};
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

bool draw_textured_point_at_pixel(
    GLuint position_location, GLuint uv_location,
    int x, int y, GLfloat u, GLfloat v)
{
    glVertexAttrib2f(uv_location, u, v);
    return draw_point_at_pixel(position_location, x, y, 1.0f);
}

bool framebuffer_matches_expected(const uint16_t *pixels)
{
    /* Check only the pixels this add-in draws. The independent host-side
       emulator oracle scans the complete LCD framebuffer and rejects any
       unexpected non-black pixel, so duplicating that 88,704-pixel scan on
       the SH-4 only burns emulation time without adding coverage. */
    for (const ExpectedRect& rect : kExpectedRedRects)
    {
        for (int y = rect.y; y < rect.y + rect.height; ++y)
        {
            for (int x = rect.x; x < rect.x + rect.width; ++x)
            {
                if (pixels[static_cast<size_t>(y) * DWIDTH + x] != kRed)
                    return false;
            }
        }
    }

    return pixels[static_cast<size_t>(150) * DWIDTH + 240] == kBlue &&
           pixels[static_cast<size_t>(150) * DWIDTH + 280] ==
               kBlendedRedBlue &&
           pixels[static_cast<size_t>(150) * DWIDTH + 310] == kGreenOnBlue &&
           pixels[static_cast<size_t>(170) * DWIDTH + 240] == kRed &&
           pixels[static_cast<size_t>(170) * DWIDTH + 260] == kTextureGreen &&
           pixels[static_cast<size_t>(170) * DWIDTH + 280] == kBlue &&
           pixels[static_cast<size_t>(170) * DWIDTH + 300] == kTextureWhite &&
           pixels[static_cast<size_t>(170) * DWIDTH + 320] == kRed &&
           pixels[static_cast<size_t>(170) * DWIDTH + 340] == kTextureGreen &&
           pixels[static_cast<size_t>(170) * DWIDTH + 360] == kTextureGreen &&
           pixels[static_cast<size_t>(170) * DWIDTH + 380] == kRed &&
           pixels[static_cast<size_t>(190) * DWIDTH + 240] == kTextureGreen &&
           pixels[static_cast<size_t>(190) * DWIDTH + 260] == 0 &&
           pixels[static_cast<size_t>(190) * DWIDTH + 280] == kTriangleCyan &&
           pixels[static_cast<size_t>(190) * DWIDTH + 320] ==
               kIndexedTriangleMagenta &&
           pixels[static_cast<size_t>(190) * DWIDTH + 360] == kArrayYellow;
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
        pixels, depth_pixels, stencil_pixels, DWIDTH, DHEIGHT, DWIDTH
    };
    bool passed = fxglMakeCurrent(&surface) == GL_TRUE;

    GLuint vertex_shader = 0;
    GLuint fragment_shader = 0;
    GLuint program = 0;
    GLuint texture = 0;
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
    GLint color_location = -1;
    GLint uv_location = -1;
    GLint texture_location = -1;
    if (passed)
    {
        glUseProgram(program);
        position_location = glGetAttribLocation(program, "a_position");
        color_location = glGetAttribLocation(program, "a_color");
        uv_location = glGetAttribLocation(program, "a_uv");
        texture_location = glGetUniformLocation(program, "u_texture");
        passed = position_location >= 0 && color_location >= 0 &&
                 uv_location >= 0 && texture_location >= 0;
    }

    if (passed)
    {
        glViewport(0, 0, DWIDTH, DHEIGHT);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClearDepthf(1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnableVertexAttribArray(static_cast<GLuint>(position_location));
        glDisable(GL_DITHER);
        glVertexAttrib4f(static_cast<GLuint>(color_location),
                         1.0f, 0.0f, 0.0f, 1.0f);

        /* Bind a white 1x1 texture so the existing color/depth/blend tests
           exercise the same public texture2D shader path without changing
           their expected output. */
        const uint16_t white_pixel = kTextureWhite;
        glGenTextures(1, &texture);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0,
                     GL_RGB, GL_UNSIGNED_SHORT_5_6_5, &white_pixel);
        glUniform1i(texture_location, 0);
        glVertexAttrib2f(static_cast<GLuint>(uv_location), 0.5f, 0.5f);
        passed = texture != 0 && glGetError() == GL_NO_ERROR;
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
    {
        /* Depth rejects farther fragments and accepts a nearer replacement
           at the same pixel. The final blue fragment must win. */
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);
        glClearDepthf(1.0f);
        glClear(GL_DEPTH_BUFFER_BIT);
        glVertexAttrib4f(static_cast<GLuint>(color_location),
                         1.0f, 0.0f, 0.0f, 1.0f);
        passed = issue_point_draw(
            static_cast<GLuint>(position_location),
            clip_coordinate_for_pixel(240, DWIDTH),
            clip_coordinate_for_pixel(150, DHEIGHT), 1.0f, -0.5f);
        glVertexAttrib4f(static_cast<GLuint>(color_location),
                         0.0f, 1.0f, 0.0f, 1.0f);
        passed = passed && issue_point_draw(
            static_cast<GLuint>(position_location),
            clip_coordinate_for_pixel(240, DWIDTH),
            clip_coordinate_for_pixel(150, DHEIGHT), 1.0f, 0.5f);
        glVertexAttrib4f(static_cast<GLuint>(color_location),
                         0.0f, 0.0f, 1.0f, 1.0f);
        passed = passed && issue_point_draw(
            static_cast<GLuint>(position_location),
            clip_coordinate_for_pixel(240, DWIDTH),
            clip_coordinate_for_pixel(150, DHEIGHT), 1.0f, -0.75f);
        glDisable(GL_DEPTH_TEST);
        passed = passed && glGetError() == GL_NO_ERROR;
    }

    if (passed)
    {
        /* Alpha blend half-red over a blue clear at one scissored pixel.
           RGB565 rounding gives 16/31 red and 16/31 blue (0x8010). */
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glEnable(GL_SCISSOR_TEST);
        glScissor(280, 150, 1, 1);
        glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_SCISSOR_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glVertexAttrib4f(static_cast<GLuint>(color_location),
                         1.0f, 0.0f, 0.0f, 0.5f);
        passed = draw_point_at_pixel(
            static_cast<GLuint>(position_location), 280, 150, 1.0f);
        glDisable(GL_BLEND);
        passed = passed && glGetError() == GL_NO_ERROR;
    }

    if (passed)
    {
        /* A green-only write over blue preserves the destination blue. */
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glEnable(GL_SCISSOR_TEST);
        glScissor(310, 150, 1, 1);
        glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_SCISSOR_TEST);
        glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_FALSE);
        glVertexAttrib4f(static_cast<GLuint>(color_location),
                         0.0f, 1.0f, 0.0f, 1.0f);
        passed = draw_point_at_pixel(
            static_cast<GLuint>(position_location), 310, 150, 1.0f);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        passed = passed && glGetError() == GL_NO_ERROR;
    }

    if (passed)
    {
        /* Exercise nearest texture sampling at all four texels of a 2x2
           RGB565 image, then repeat and clamp behavior on the S axis. */
        const uint16_t texture_pixels[4] = {
            kRed, kTextureGreen,
            kBlue, kTextureWhite
        };
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glVertexAttrib4f(static_cast<GLuint>(color_location),
                         1.0f, 1.0f, 1.0f, 1.0f);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2, 2, 0,
                     GL_RGB, GL_UNSIGNED_SHORT_5_6_5, texture_pixels);
        passed = glGetError() == GL_NO_ERROR;

        passed = passed && draw_textured_point_at_pixel(
            static_cast<GLuint>(position_location),
            static_cast<GLuint>(uv_location), 240, 170, 0.25f, 0.25f);
        passed = passed && draw_textured_point_at_pixel(
            static_cast<GLuint>(position_location),
            static_cast<GLuint>(uv_location), 260, 170, 0.75f, 0.25f);
        passed = passed && draw_textured_point_at_pixel(
            static_cast<GLuint>(position_location),
            static_cast<GLuint>(uv_location), 280, 170, 0.25f, 0.75f);
        passed = passed && draw_textured_point_at_pixel(
            static_cast<GLuint>(position_location),
            static_cast<GLuint>(uv_location), 300, 170, 0.75f, 0.75f);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        passed = passed && glGetError() == GL_NO_ERROR;
        passed = passed && draw_textured_point_at_pixel(
            static_cast<GLuint>(position_location),
            static_cast<GLuint>(uv_location), 320, 170, 1.25f, 0.25f);
        passed = passed && draw_textured_point_at_pixel(
            static_cast<GLuint>(position_location),
            static_cast<GLuint>(uv_location), 340, 170, -0.25f, 0.25f);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        passed = passed && glGetError() == GL_NO_ERROR;
        passed = passed && draw_textured_point_at_pixel(
            static_cast<GLuint>(position_location),
            static_cast<GLuint>(uv_location), 360, 170, 1.25f, 0.25f);
        passed = passed && draw_textured_point_at_pixel(
            static_cast<GLuint>(position_location),
            static_cast<GLuint>(uv_location), 380, 170, -0.25f, 0.25f);
        passed = passed && glGetError() == GL_NO_ERROR;
    }

    if (passed)
    {
        /* Stencil replace writes reference 1 at the first pixel. GL_EQUAL
           then permits an overwrite there but rejects a draw at the untouched
           neighboring pixel, which must remain black. */
        glVertexAttrib2f(static_cast<GLuint>(uv_location), 0.75f, 0.75f);
        glClearStencil(0);
        glStencilMask(0xff);
        glClear(GL_STENCIL_BUFFER_BIT);
        glEnable(GL_STENCIL_TEST);
        glStencilFunc(GL_ALWAYS, 1, 0xff);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glVertexAttrib4f(static_cast<GLuint>(color_location),
                         1.0f, 0.0f, 0.0f, 1.0f);
        passed = draw_point_at_pixel(
            static_cast<GLuint>(position_location), 240, 190, 1.0f);

        glStencilFunc(GL_EQUAL, 1, 0xff);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        glVertexAttrib4f(static_cast<GLuint>(color_location),
                         0.0f, 1.0f, 0.0f, 1.0f);
        passed = passed && draw_point_at_pixel(
            static_cast<GLuint>(position_location), 240, 190, 1.0f);

        glVertexAttrib4f(static_cast<GLuint>(color_location),
                         0.0f, 0.0f, 1.0f, 1.0f);
        passed = passed && draw_point_at_pixel(
            static_cast<GLuint>(position_location), 260, 190, 1.0f);
        glDisable(GL_STENCIL_TEST);
        passed = passed && glGetError() == GL_NO_ERROR;
    }

    if (passed)
    {
        /* Render a deliberately sub-pixel-sized triangle whose only covered
           sample is the center of (280,190). This exercises GL_TRIANGLES,
           three-vertex attribute fetch, varying interpolation, and the
           fragment shader's texture lookup through the public GLES path. */
        const GLfloat triangle_positions[12] = {
            clip_coordinate_for_screen(280.1f, DWIDTH),
            clip_coordinate_for_screen(190.1f, DHEIGHT), 1.0f, 0.0f,
            clip_coordinate_for_screen(280.9f, DWIDTH),
            clip_coordinate_for_screen(190.1f, DHEIGHT), 1.0f, 0.0f,
            clip_coordinate_for_screen(280.5f, DWIDTH),
            clip_coordinate_for_screen(190.9f, DHEIGHT), 1.0f, 0.0f
        };
        glVertexAttribPointer(static_cast<GLuint>(position_location),
                              4, GL_FLOAT, GL_FALSE, 0, triangle_positions);
        glVertexAttrib4f(static_cast<GLuint>(color_location),
                         0.0f, 1.0f, 1.0f, 1.0f);
        glVertexAttrib2f(static_cast<GLuint>(uv_location), 0.75f, 0.75f);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        passed = glGetError() == GL_NO_ERROR;
    }

    if (passed)
    {
        /* Repeat the one-pixel triangle through GL_ARRAY_BUFFER,
           GL_ELEMENT_ARRAY_BUFFER, and glDrawElements. The two buffer stores
           are copied before drawing; the null pointer arguments are offsets
           into their respective bound buffers. */
        const GLfloat indexed_triangle_positions[12] = {
            clip_coordinate_for_screen(320.1f, DWIDTH),
            clip_coordinate_for_screen(190.1f, DHEIGHT), 1.0f, 0.0f,
            clip_coordinate_for_screen(320.9f, DWIDTH),
            clip_coordinate_for_screen(190.1f, DHEIGHT), 1.0f, 0.0f,
            clip_coordinate_for_screen(320.5f, DWIDTH),
            clip_coordinate_for_screen(190.9f, DHEIGHT), 1.0f, 0.0f
        };
        const GLushort indices[3] = {0, 1, 2};
        GLuint vertex_buffer = 0;
        GLuint index_buffer = 0;
        glGenBuffers(1, &vertex_buffer);
        glGenBuffers(1, &index_buffer);
        passed = vertex_buffer != 0 && index_buffer != 0 &&
                 glGetError() == GL_NO_ERROR;

        if (passed)
        {
            glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
            glBufferData(GL_ARRAY_BUFFER,
                         static_cast<GLsizeiptr>(
                             sizeof(indexed_triangle_positions)),
                         indexed_triangle_positions, GL_STATIC_DRAW);
            glVertexAttribPointer(
                static_cast<GLuint>(position_location),
                4, GL_FLOAT, GL_FALSE, 0, nullptr);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                         static_cast<GLsizeiptr>(sizeof(indices)),
                         indices, GL_STATIC_DRAW);
            glVertexAttrib4f(static_cast<GLuint>(color_location),
                             1.0f, 0.0f, 1.0f, 1.0f);
            glVertexAttrib2f(static_cast<GLuint>(uv_location), 0.75f, 0.75f);
            passed = glGetError() == GL_NO_ERROR;
        }

        if (passed)
        {
            glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, nullptr);
            passed = glGetError() == GL_NO_ERROR;
        }

        if (vertex_buffer != 0)
            glDeleteBuffers(1, &vertex_buffer);
        if (index_buffer != 0)
            glDeleteBuffers(1, &index_buffer);
        passed = passed && glGetError() == GL_NO_ERROR;
    }

    if (passed)
    {
        /* Switch to the array program and draw one point at (360,190). The
           indexed draw above deleted its buffers, which also unbinds them,
           so the position pointer below is client memory again. */
        const GLuint array_program =
            build_program(kArrayVertexSource, kArrayFragmentSource);
        passed = array_program != 0;
        GLint array_position = -1;
        if (passed)
        {
            glUseProgram(array_program);
            array_position = glGetAttribLocation(array_program, "a_position");
            passed = array_position >= 0 && glGetError() == GL_NO_ERROR;
        }
        if (passed)
        {
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glEnableVertexAttribArray(static_cast<GLuint>(array_position));
            passed = draw_point_at_pixel(
                static_cast<GLuint>(array_position), 360, 190, 1.0f);
        }
    }

    if (passed)
        passed = framebuffer_matches_expected(pixels);

    publish_result(passed);
    for (;;)
        __asm__ volatile("nop");
}
