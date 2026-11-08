#include "glmark2.hpp"

#include <GLES2/gl2.h>
#include <libprof.h>
#include "../display.hpp"
#include "../gles/math.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>

namespace fxglmark2
{
namespace
{
constexpr unsigned SPHERE_ROWS = 6;
constexpr unsigned SPHERE_SLICES = 12;
constexpr size_t VERTEX_COUNT =
    static_cast<size_t>(SPHERE_ROWS) * SPHERE_SLICES * 6;
constexpr unsigned TEXTURE_SIZE = 32;

struct Vertex
{
    float position[3];
    float normal[3];
    float texcoord[2];
};

static Vertex g_vertices[VERTEX_COUNT];
static uint16_t g_texture_pixels[TEXTURE_SIZE * TEXTURE_SIZE];
static bool g_mesh_ready = false;
static bool g_texture_ready = false;

static const SceneInfo g_scenes[] = {
    {"clear", "Full-surface clear throughput", true},
    {"build", "Geometry submission: client arrays or VBO", true},
    {"texture", "Textured geometry: nearest or linear", true},
    {"shading", "GLSL Gouraud lighting", true},
    {"buffer", "Dynamic buffer update and map/subdata", false},
    {"bump", "Bump mapping and normal/height maps", false},
    {"effect2d", "Off-screen 2D convolution", false},
    {"pulsar", "Animated textured-quad workload", false},
    {"desktop", "Multi-window blur and shadow effects", false},
    {"ideas", "Animated shader effects", false},
    {"jellyfish", "Animated multi-object geometry", false},
    {"terrain", "Terrain rendering workload", false},
    {"shadow", "Shadow-map workload", false},
    {"refract", "Environment/refraction workload", false},
    {"conditionals", "Shader conditional stress", false},
    {"function", "Shader function-call stress", false},
    {"loop", "Shader loop stress", false}
};

static const char *const g_default_benchmarks[] = {
    "build:use-vbo=false",
    "build:use-vbo=true",
    "texture:texture-filter=nearest",
    "texture:texture-filter=linear",
    "shading:shading=gouraud"
};

static const char *const g_quick_smoke[] = {
    "build:use-vbo=true",
    "texture:texture-filter=nearest"
};

static const char *const g_all_implemented[] = {
    "clear",
    "build:use-vbo=false",
    "build:use-vbo=true",
    "texture:texture-filter=nearest",
    "texture:texture-filter=linear",
    "shading:shading=gouraud"
};

static void set_error_text(char *destination, size_t capacity, const char *message)
{
    if (destination && capacity)
        std::snprintf(destination, capacity, "%s", message ? message : "");
}

static bool parse_bool(const char *value, bool& result)
{
    if (std::strcmp(value, "true") == 0)
    {
        result = true;
        return true;
    }
    if (std::strcmp(value, "false") == 0)
    {
        result = false;
        return true;
    }
    return false;
}

static SceneId scene_from_name(const char *name, bool& found, bool& implemented)
{
    found = false;
    implemented = false;
    for (size_t i = 0; i < sizeof(g_scenes) / sizeof(g_scenes[0]); ++i)
    {
        if (std::strcmp(g_scenes[i].name, name) == 0)
        {
            found = true;
            implemented = g_scenes[i].implemented;
            return static_cast<SceneId>(i);
        }
    }
    return SceneId::Clear;
}

static uint16_t pack_rgb565(unsigned red, unsigned green, unsigned blue)
{
    return static_cast<uint16_t>(
        ((red & 0xf8u) << 8) |
        ((green & 0xfcu) << 3) |
        ((blue & 0xf8u) >> 3));
}

static void make_vertex(unsigned row, unsigned column, Vertex& vertex)
{
    const float v = static_cast<float>(row) / SPHERE_ROWS;
    const float u = static_cast<float>(column) / SPHERE_SLICES;
    const float latitude = v * 3.14159265358979323846f;
    const float longitude = u * 6.28318530717958647692f;
    const float ring = std::sin(latitude);
    const float x = ring * std::cos(longitude);
    const float y = std::cos(latitude);
    const float z = ring * std::sin(longitude);

    vertex.position[0] = x;
    vertex.position[1] = y;
    vertex.position[2] = z;
    vertex.normal[0] = x;
    vertex.normal[1] = y;
    vertex.normal[2] = z;
    vertex.texcoord[0] = u;
    vertex.texcoord[1] = v;
}

static void generate_mesh()
{
    if (g_mesh_ready)
        return;

    size_t output = 0;
    for (unsigned row = 0; row < SPHERE_ROWS; ++row)
    {
        for (unsigned column = 0; column < SPHERE_SLICES; ++column)
        {
            Vertex top_left = {};
            Vertex top_right = {};
            Vertex bottom_left = {};
            Vertex bottom_right = {};
            make_vertex(row, column, top_left);
            make_vertex(row, (column + 1) % SPHERE_SLICES, top_right);
            make_vertex(row + 1, column, bottom_left);
            make_vertex(row + 1, (column + 1) % SPHERE_SLICES, bottom_right);

            g_vertices[output++] = top_left;
            g_vertices[output++] = bottom_left;
            g_vertices[output++] = top_right;
            g_vertices[output++] = top_right;
            g_vertices[output++] = bottom_left;
            g_vertices[output++] = bottom_right;
        }
    }
    g_mesh_ready = output == VERTEX_COUNT;
}

static void generate_texture()
{
    if (g_texture_ready)
        return;
    for (unsigned y = 0; y < TEXTURE_SIZE; ++y)
    {
        for (unsigned x = 0; x < TEXTURE_SIZE; ++x)
        {
            const unsigned tile = ((x / 4u) ^ (y / 4u)) & 1u;
            const unsigned red = tile ? 240u : 24u;
            const unsigned green = (x * 7u + y * 3u) & 0xffu;
            const unsigned blue = tile ? 40u : 224u;
            g_texture_pixels[y * TEXTURE_SIZE + x] =
                pack_rgb565(red, green, blue);
        }
    }
    g_texture_ready = true;
}

static const char *const gouraud_vertex_shader =
    "#version 100\n"
    "attribute vec3 a_position;\n"
    "attribute vec3 a_normal;\n"
    "uniform mat4 u_mvp;\n"
    "uniform mat4 u_model;\n"
    "uniform vec3 u_light_dir;\n"
    "varying vec4 v_color;\n"
    "void main() {\n"
    "  vec3 n = normalize((u_model * vec4(a_normal, 0.0)).xyz);\n"
    "  float diffuse = max(dot(n, normalize(u_light_dir)), 0.0);\n"
    "  float light = 0.18 + 0.82 * diffuse;\n"
    "  vec3 base = vec3(0.12, 0.48, 0.92);\n"
    "  v_color = vec4(base * light, 1.0);\n"
    "  gl_Position = u_mvp * vec4(a_position, 1.0);\n"
    "}\n";

static const char *const gouraud_fragment_shader =
    "#version 100\n"
    "precision mediump float;\n"
    "varying vec4 v_color;\n"
    "void main() { gl_FragColor = v_color; }\n";

static const char *const texture_vertex_shader =
    "#version 100\n"
    "attribute vec3 a_position;\n"
    "attribute vec2 a_texcoord;\n"
    "uniform mat4 u_mvp;\n"
    "varying vec2 v_uv;\n"
    "void main() {\n"
    "  gl_Position = u_mvp * vec4(a_position, 1.0);\n"
    "  v_uv = a_texcoord * 6.0;\n"
    "}\n";

static const char *const texture_fragment_shader =
    "#version 100\n"
    "precision mediump float;\n"
    "uniform sampler2D u_texture;\n"
    "varying vec2 v_uv;\n"
    "void main() { gl_FragColor = texture2D(u_texture, v_uv); }\n";

static bool compile_shader(
    GLenum type,
    const char *source,
    GLuint& output,
    char *error,
    size_t error_capacity)
{
    const GLuint shader = glCreateShader(type);
    if (!shader)
    {
        set_error_text(error, error_capacity, "glCreateShader failed");
        return false;
    }

    const GLchar *strings[1] = {source};
    glShaderSource(shader, 1, strings, nullptr);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE)
    {
        char log[160] = {};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        set_error_text(error, error_capacity,
            log[0] ? log : "GLSL compile failed");
        glDeleteShader(shader);
        return false;
    }
    output = shader;
    return true;
}

static bool link_program(
    SceneInstance& instance,
    const char *vertex_source,
    const char *fragment_source,
    bool textured,
    char *error,
    size_t error_capacity)
{
    if (!compile_shader(GL_VERTEX_SHADER, vertex_source,
                        instance.vertex_shader, error, error_capacity))
        return false;
    if (!compile_shader(GL_FRAGMENT_SHADER, fragment_source,
                        instance.fragment_shader, error, error_capacity))
        return false;

    const GLuint program = glCreateProgram();
    if (!program)
    {
        set_error_text(error, error_capacity, "glCreateProgram failed");
        return false;
    }

    glBindAttribLocation(program, 0, "a_position");
    glBindAttribLocation(program, 1,
        textured ? "a_texcoord" : "a_normal");
    glAttachShader(program, instance.vertex_shader);
    glAttachShader(program, instance.fragment_shader);
    glLinkProgram(program);

    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE)
    {
        char log[160] = {};
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        set_error_text(error, error_capacity, log[0] ? log : "program link failed");
        glDeleteProgram(program);
        return false;
    }

    instance.program = program;
    instance.position_location = glGetAttribLocation(program, "a_position");
    instance.normal_or_uv_location = glGetAttribLocation(
        program, textured ? "a_texcoord" : "a_normal");
    instance.mvp_location = glGetUniformLocation(program, "u_mvp");
    if (!textured)
    {
        instance.model_location = glGetUniformLocation(program, "u_model");
        instance.light_location = glGetUniformLocation(program, "u_light_dir");
    }
    else
    {
        instance.texture_location = glGetUniformLocation(program, "u_texture");
    }

    if (instance.position_location < 0 ||
        instance.normal_or_uv_location < 0 ||
        instance.mvp_location < 0 ||
        (!textured && (instance.model_location < 0 ||
                       instance.light_location < 0)) ||
        (textured && instance.texture_location < 0))
    {
        set_error_text(error, error_capacity,
            "required shader attribute or uniform is missing");
        return false;
    }

    glUseProgram(program);
    return glGetError() == GL_NO_ERROR;
}

static void setup_vertex_input(SceneInstance& instance, bool textured)
{
    const GLsizei stride = static_cast<GLsizei>(sizeof(Vertex));
    glBindBuffer(GL_ARRAY_BUFFER, instance.vertex_buffer);

    const void *position_pointer = instance.vertex_buffer
        ? reinterpret_cast<const void *>(offsetof(Vertex, position))
        : static_cast<const void *>(g_vertices[0].position);
    const void *secondary_pointer = instance.vertex_buffer
        ? reinterpret_cast<const void *>(textured
            ? offsetof(Vertex, texcoord) : offsetof(Vertex, normal))
        : static_cast<const void *>(textured
            ? g_vertices[0].texcoord : g_vertices[0].normal);

    glVertexAttribPointer(
        static_cast<GLuint>(instance.position_location), 3, GL_FLOAT,
        GL_FALSE, stride, position_pointer);
    glEnableVertexAttribArray(
        static_cast<GLuint>(instance.position_location));

    glVertexAttribPointer(
        static_cast<GLuint>(instance.normal_or_uv_location),
        textured ? 2 : 3, GL_FLOAT, GL_FALSE,
        stride, secondary_pointer);
    glEnableVertexAttribArray(
        static_cast<GLuint>(instance.normal_or_uv_location));
}

static bool setup_vertex_buffer(SceneInstance& instance)
{
    if (!instance.request.use_vbo)
        return true;

    glGenBuffers(1, &instance.vertex_buffer);
    if (!instance.vertex_buffer)
        return false;

    glBindBuffer(GL_ARRAY_BUFFER, instance.vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(sizeof(g_vertices)),
        g_vertices, GL_STATIC_DRAW);
    return glGetError() == GL_NO_ERROR;
}

static void update_matrices(SceneInstance& instance, uint32_t frame_number)
{
    fxgl::Mat4 projection = {};
    fxgl::Mat4 model = {};
    fxgl::Mat4 mvp = {};

    const float angle = static_cast<float>(frame_number) * 0.035f;
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);

    fxgl::mat4_identity(model);
    model.m[0] = cosine;
    model.m[2] = -sine;
    model.m[8] = sine;
    model.m[10] = cosine;
    model.m[14] = -3.2f;

    if (!fxgl::mat4_perspective(
            projection, 0.95f, 396.0f / 224.0f, 0.1f, 20.0f))
        return;

    fxgl::mat4_multiply(mvp, projection, model);
    glUniformMatrix4fv(instance.mvp_location, 1, GL_FALSE, mvp.m);

    if (instance.model_location >= 0)
    {
        model.m[14] = 0.0f;
        glUniformMatrix4fv(instance.model_location, 1, GL_FALSE, model.m);
    }
}

static bool setup_texture(SceneInstance& instance, char *error, size_t capacity)
{
    generate_texture();
    glGenTextures(1, &instance.texture);
    if (!instance.texture)
    {
        set_error_text(error, capacity, "glGenTextures failed");
        return false;
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, instance.texture);
    const GLint filter = instance.request.texture_filter == 0
        ? GL_NEAREST : GL_LINEAR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
        TEXTURE_SIZE, TEXTURE_SIZE, 0, GL_RGB,
        GL_UNSIGNED_SHORT_5_6_5, g_texture_pixels);
    glUniform1i(instance.texture_location, 0);

    if (glGetError() != GL_NO_ERROR)
    {
        set_error_text(error, capacity, "texture allocation/upload failed");
        return false;
    }
    return true;
}

static bool render_prepared_scene(SceneInstance& instance, uint32_t frame_number)
{
    if (!instance.prepared)
        return false;

    glViewport(0, 0, 396, 224);
    glClearColor(0.035f, 0.045f, 0.075f, 1.0f);
    glClearDepthf(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (instance.request.scene == SceneId::Clear)
        return glGetError() == GL_NO_ERROR;

    glUseProgram(instance.program);
    update_matrices(instance, frame_number);

    if (instance.texture)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, instance.texture);
    }

    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(instance.vertex_count));
    return glGetError() == GL_NO_ERROR;
}

static void sort_samples(double *samples, size_t count)
{
    for (size_t i = 1; i < count; ++i)
    {
        const double value = samples[i];
        size_t j = i;
        while (j > 0 && samples[j - 1] > value)
        {
            samples[j] = samples[j - 1];
            --j;
        }
        samples[j] = value;
    }
}

static bool battery_description(
    BatteryId battery,
    size_t index,
    const char *&description)
{
    switch (battery)
    {
        case BatteryId::QuickSmoke:
            if (index >= sizeof(g_quick_smoke) / sizeof(g_quick_smoke[0]))
                return false;
            description = g_quick_smoke[index];
            return true;
        case BatteryId::Glmark2SupportedDefault:
            if (index >= sizeof(g_default_benchmarks) / sizeof(g_default_benchmarks[0]))
                return false;
            description = g_default_benchmarks[index];
            return true;
        case BatteryId::AllImplemented:
            if (index >= sizeof(g_all_implemented) / sizeof(g_all_implemented[0]))
                return false;
            description = g_all_implemented[index];
            return true;
    }
    return false;
}

} // namespace

const SceneInfo *scene_info(size_t index)
{
    return index < sizeof(g_scenes) / sizeof(g_scenes[0])
        ? &g_scenes[index] : nullptr;
}

size_t scene_count()
{
    return sizeof(g_scenes) / sizeof(g_scenes[0]);
}

size_t implemented_scene_count()
{
    size_t count = 0;
    for (const SceneInfo& scene : g_scenes)
        if (scene.implemented)
            ++count;
    return count;
}

const char *scene_name(SceneId scene)
{
    const SceneInfo *info = scene_info(static_cast<size_t>(scene));
    return info ? info->name : "unknown";
}

const char *default_benchmark_description(size_t index)
{
    return index < default_benchmark_count() ? g_default_benchmarks[index] : nullptr;
}

size_t default_benchmark_count()
{
    return sizeof(g_default_benchmarks) / sizeof(g_default_benchmarks[0]);
}

const char *battery_name(BatteryId battery)
{
    switch (battery)
    {
        case BatteryId::QuickSmoke: return "Quick smoke (2)";
        case BatteryId::Glmark2SupportedDefault:
            return "glmark2 supported default (5)";
        case BatteryId::AllImplemented: return "All implemented (6)";
    }
    return "Unknown battery";
}

size_t battery_size(BatteryId battery)
{
    switch (battery)
    {
        case BatteryId::QuickSmoke:
            return sizeof(g_quick_smoke) / sizeof(g_quick_smoke[0]);
        case BatteryId::Glmark2SupportedDefault:
            return default_benchmark_count();
        case BatteryId::AllImplemented:
            return sizeof(g_all_implemented) / sizeof(g_all_implemented[0]);
    }
    return 0;
}

bool battery_request(
    BatteryId battery,
    size_t index,
    BenchmarkRequest& request,
    char *error,
    size_t error_capacity)
{
    const char *description = nullptr;
    if (!battery_description(battery, index, description))
    {
        set_error_text(error, error_capacity, "battery index out of range");
        return false;
    }
    return parse_benchmark(description, request, error, error_capacity);
}

bool parse_benchmark(
    const char *description,
    BenchmarkRequest& request,
    char *error,
    size_t error_capacity)
{
    request = {};
    request.use_vbo = true;
    if (!description || !*description)
    {
        set_error_text(error, error_capacity, "empty benchmark description");
        return false;
    }

    char scene[OPTION_NAME_BYTES] = {};
    const char *separator = std::strchr(description, ':');
    const size_t scene_length = separator
        ? static_cast<size_t>(separator - description)
        : std::strlen(description);
    if (scene_length == 0 || scene_length >= sizeof(scene))
    {
        set_error_text(error, error_capacity, "invalid scene name");
        return false;
    }
    std::memcpy(scene, description, scene_length);
    scene[scene_length] = '\0';

    bool found = false;
    bool implemented = false;
    request.scene = scene_from_name(scene, found, implemented);
    if (!found)
    {
        set_error_text(error, error_capacity, "unknown scene name");
        return false;
    }
    if (!implemented)
    {
        set_error_text(error, error_capacity,
            "scene recognized but not implemented on fx-CG50 yet");
        return false;
    }

    const char *cursor = separator ? separator + 1 : nullptr;
    while (cursor && *cursor)
    {
        const char *next = std::strchr(cursor, ':');
        const size_t token_length = next
            ? static_cast<size_t>(next - cursor)
            : std::strlen(cursor);
        const char *equals = static_cast<const char *>(
            std::memchr(cursor, '=', token_length));
        if (!equals || equals == cursor ||
            static_cast<size_t>(equals - cursor) >= OPTION_NAME_BYTES)
        {
            set_error_text(error, error_capacity, "option must be name=value");
            return false;
        }

        const size_t key_length = static_cast<size_t>(equals - cursor);
        const size_t value_length = token_length - key_length - 1;
        if (value_length == 0 || value_length >= OPTION_VALUE_BYTES ||
            request.option_count >= MAX_OPTIONS)
        {
            set_error_text(error, error_capacity,
                "empty, oversized or excessive option");
            return false;
        }

        Option& option = request.options[request.option_count];
        std::memcpy(option.name, cursor, key_length);
        option.name[key_length] = '\0';
        std::memcpy(option.value, equals + 1, value_length);
        option.value[value_length] = '\0';

        for (size_t i = 0; i < request.option_count; ++i)
        {
            if (std::strcmp(request.options[i].name, option.name) == 0)
            {
                set_error_text(error, error_capacity, "duplicate benchmark option");
                return false;
            }
        }

        if (request.scene == SceneId::Build)
        {
            if (std::strcmp(option.name, "use-vbo") == 0)
            {
                if (!parse_bool(option.value, request.use_vbo))
                {
                    set_error_text(error, error_capacity, "use-vbo expects true or false");
                    return false;
                }
            }
            else if (std::strcmp(option.name, "interleave") == 0)
            {
                bool interleave = false;
                if (!parse_bool(option.value, interleave) || interleave)
                {
                    set_error_text(error, error_capacity,
                        "interleave=true is not supported by this first scene port");
                    return false;
                }
            }
            else
            {
                set_error_text(error, error_capacity, "unsupported build option");
                return false;
            }
        }
        else if (request.scene == SceneId::Texture)
        {
            if (std::strcmp(option.name, "texture-filter") == 0)
            {
                if (std::strcmp(option.value, "nearest") == 0)
                    request.texture_filter = 0;
                else if (std::strcmp(option.value, "linear") == 0)
                    request.texture_filter = 1;
                else
                {
                    set_error_text(error, error_capacity,
                        "texture-filter supports nearest/linear; mipmaps are pending");
                    return false;
                }
            }
            else
            {
                set_error_text(error, error_capacity, "unsupported texture option");
                return false;
            }
        }
        else if (request.scene == SceneId::Shading)
        {
            if (std::strcmp(option.name, "shading") == 0)
            {
                if (std::strcmp(option.value, "gouraud") != 0)
                {
                    set_error_text(error, error_capacity,
                        "Gouraud is the only implemented shading model");
                    return false;
                }
            }
            else if (std::strcmp(option.name, "num-lights") == 0)
            {
                if (std::strcmp(option.value, "1") != 0)
                {
                    set_error_text(error, error_capacity,
                        "Gouraud scene currently uses one light");
                    return false;
                }
            }
            else
            {
                set_error_text(error, error_capacity, "unsupported shading option");
                return false;
            }
        }
        else if (request.scene == SceneId::Clear)
        {
            set_error_text(error, error_capacity, "clear scene accepts no options");
            return false;
        }

        ++request.option_count;
        if (!next)
            break;
        cursor = next + 1;
        if (!*cursor)
        {
            set_error_text(error, error_capacity, "trailing empty option");
            return false;
        }
    }

    set_error_text(error, error_capacity, "");
    return true;
}

bool prepare_scene(
    const BenchmarkRequest& request,
    SceneInstance& instance,
    char *error,
    size_t error_capacity)
{
    instance = {};
    instance.request = request;
    instance.position_location = -1;
    instance.normal_or_uv_location = -1;
    instance.mvp_location = -1;
    instance.model_location = -1;
    instance.light_location = -1;
    instance.texture_location = -1;

    if (request.scene == SceneId::Clear)
    {
        instance.prepared = true;
        set_error_text(error, error_capacity, "");
        return true;
    }

    generate_mesh();
    if (!g_mesh_ready)
    {
        set_error_text(error, error_capacity, "sphere mesh generation failed");
        return false;
    }
    instance.vertex_count = static_cast<uint32_t>(VERTEX_COUNT);
    instance.triangle_count = static_cast<uint32_t>(VERTEX_COUNT / 3);

    const bool textured = request.scene == SceneId::Texture;
    if (!link_program(instance,
            textured ? texture_vertex_shader : gouraud_vertex_shader,
            textured ? texture_fragment_shader : gouraud_fragment_shader,
            textured, error, error_capacity))
    {
        finish_scene(instance);
        return false;
    }

    if (!setup_vertex_buffer(instance))
    {
        set_error_text(error, error_capacity, "vertex buffer allocation/upload failed");
        finish_scene(instance);
        return false;
    }

    setup_vertex_input(instance, textured);

    if (textured && !setup_texture(instance, error, error_capacity))
    {
        finish_scene(instance);
        return false;
    }

    if (!textured)
        glUniform3f(instance.light_location, 0.35f, 0.75f, 0.55f);

    if (glGetError() != GL_NO_ERROR)
    {
        set_error_text(error, error_capacity, "GLES scene preparation failed");
        finish_scene(instance);
        return false;
    }

    instance.prepared = true;
    set_error_text(error, error_capacity, "");
    return true;
}

bool render_scene_frame(SceneInstance& instance, uint32_t frame_number)
{
    if (!instance.prepared)
        return false;
    instance.frame_number = frame_number;

    glViewport(0, 0, 396, 224);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glClearColor(0.035f, 0.045f, 0.075f, 1.0f);
    glClearDepthf(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (instance.request.scene == SceneId::Clear)
        return glGetError() == GL_NO_ERROR;

    glUseProgram(instance.program);
    update_matrices(instance, frame_number);
    if (instance.texture)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, instance.texture);
    }

    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(instance.vertex_count));
    return glGetError() == GL_NO_ERROR;
}

void finish_scene(SceneInstance& instance)
{
    if (instance.program)
    {
        glUseProgram(0);
        glDeleteProgram(instance.program);
    }
    if (instance.vertex_shader)
        glDeleteShader(instance.vertex_shader);
    if (instance.fragment_shader)
        glDeleteShader(instance.fragment_shader);
    if (instance.vertex_buffer)
        glDeleteBuffers(1, &instance.vertex_buffer);
    if (instance.texture)
        glDeleteTextures(1, &instance.texture);

    instance.program = 0;
    instance.vertex_shader = 0;
    instance.fragment_shader = 0;
    instance.vertex_buffer = 0;
    instance.texture = 0;
    instance.prepared = false;
}

bool run_benchmark(
    const BenchmarkRequest& request,
    const RunConfig& config,
    BenchmarkResult& result,
    char *error,
    size_t error_capacity)
{
    if (config.measure_frames == 0 || config.repeat_count == 0 ||
        config.repeat_count > 9)
    {
        set_error_text(error, error_capacity,
            "measure_frames must be positive and repeat_count must be 1..9");
        return false;
    }

    SceneInstance instance = {};
    if (!prepare_scene(request, instance, error, error_capacity))
        return false;

    for (uint32_t i = 0; i < config.warmup_frames; ++i)
    {
        if (!render_scene_frame(instance, i))
        {
            finish_scene(instance);
            set_error_text(error, error_capacity, "warm-up render failed");
            return false;
        }
        dupdate();
    }

    double samples[9] = {};
    for (uint32_t repeat = 0; repeat < config.repeat_count; ++repeat)
    {
        prof_t timer = prof_make();
        prof_enter(timer);
        for (uint32_t frame = 0; frame < config.measure_frames; ++frame)
        {
            if (!render_scene_frame(instance, frame))
            {
                prof_leave(timer);
                finish_scene(instance);
                set_error_text(error, error_capacity, "measured render failed");
                return false;
            }
            dupdate();
        }
        prof_leave(timer);
        samples[repeat] = static_cast<double>(prof_time(timer)) /
                          config.measure_frames;
    }

    sort_samples(samples, config.repeat_count);
    double sum = 0.0;
    for (uint32_t i = 0; i < config.repeat_count; ++i)
        sum += samples[i];

    result = {};
    std::snprintf(result.description, sizeof(result.description), "%s",
        request.scene == SceneId::Build
            ? (request.use_vbo ? "build:use-vbo=true" : "build:use-vbo=false")
            : request.scene == SceneId::Texture
                ? (request.texture_filter ? "texture:texture-filter=linear"
                                          : "texture:texture-filter=nearest")
                : request.scene == SceneId::Shading
                    ? "shading:shading=gouraud" : "clear");
    result.frames = config.measure_frames * config.repeat_count;
    result.triangles_per_frame = instance.triangle_count;
    result.min_frame_us = samples[0];
    result.max_frame_us = samples[config.repeat_count - 1];
    result.mean_frame_us = sum / config.repeat_count;
    result.median_frame_us = (config.repeat_count & 1u)
        ? samples[config.repeat_count / 2]
        : (samples[config.repeat_count / 2 - 1] +
           samples[config.repeat_count / 2]) / 2.0;
    result.fps = result.mean_frame_us > 0.0
        ? 1000000.0 / result.mean_frame_us : 0.0;

    finish_scene(instance);
    set_error_text(error, error_capacity, "");
    return true;
}

} // namespace fxglmark2
