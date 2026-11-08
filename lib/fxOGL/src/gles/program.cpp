#include <GLES2/gl2.h>
#include <fxgl/platform.h>
#include "context_internal.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace
{
constexpr size_t FXGL_MAX_SHADER_OBJECTS = 4;
constexpr size_t FXGL_MAX_PROGRAM_OBJECTS = 2;
constexpr size_t FXGL_SHADER_SOURCE_BYTES = 2048;

struct ShaderObject
{
    bool alive = false;
    bool delete_pending = false;
    size_t attached_count = 0;
    GLenum type = 0;
    size_t source_length = 0;
    char source[FXGL_SHADER_SOURCE_BYTES] = {};
    fxgl::ShaderModule module = {};
};

struct AttributeBinding
{
    char name[fxgl::GLSL_NAME_BYTES] = {};
    GLuint location = 0;
};

struct ProgramObject
{
    bool alive = false;
    bool delete_pending = false;
    bool linked = false;
    bool validated = false;
    GLuint vertex_shader = 0;
    GLuint fragment_shader = 0;
    fxgl::ShaderModule vertex_module = {};
    fxgl::ShaderModule fragment_module = {};
    char info_log[160] = {};

    AttributeBinding attribute_bindings[fxgl::GLSL_MAX_ATTRIBUTES] = {};
    size_t attribute_binding_count = 0;

    fxgl::ShaderVariable uniforms[fxgl::GLSL_MAX_UNIFORMS] = {};
    size_t uniform_count = 0;
    fxgl::ShaderValue uniform_values[fxgl::GLSL_MAX_UNIFORMS] = {};
    uint8_t vertex_uniform_map[fxgl::GLSL_MAX_UNIFORMS] = {};
    uint8_t fragment_uniform_map[fxgl::GLSL_MAX_UNIFORMS] = {};

    uint8_t attribute_locations[fxgl::GLSL_MAX_ATTRIBUTES] = {};
    uint8_t vertex_varying_offsets[fxgl::GLSL_MAX_VARYINGS] = {};
    uint8_t fragment_varying_offsets[fxgl::GLSL_MAX_VARYINGS] = {};
    size_t varying_scalar_count = 0;
};

ShaderObject g_shaders[FXGL_MAX_SHADER_OBJECTS];
ProgramObject g_programs[FXGL_MAX_PROGRAM_OBJECTS];
GLuint g_current_program = 0;

static ShaderObject *find_shader(GLuint name)
{
    if (name == 0 || name > FXGL_MAX_SHADER_OBJECTS)
        return nullptr;
    ShaderObject& shader = g_shaders[name - 1];
    return shader.alive ? &shader : nullptr;
}

static ProgramObject *find_program(GLuint name)
{
    if (name == 0 || name > FXGL_MAX_PROGRAM_OBJECTS)
        return nullptr;
    ProgramObject& program = g_programs[name - 1];
    return program.alive ? &program : nullptr;
}

static void release_shader_if_pending(ShaderObject& shader)
{
    if (shader.alive && shader.delete_pending && shader.attached_count == 0)
        shader = {};
}

static void set_log(char *destination, size_t capacity, const char *message)
{
    if (capacity == 0)
        return;
    std::snprintf(destination, capacity, "%s", message ? message : "");
}

static void copy_log(
    const char *source,
    GLsizei max_length,
    GLsizei *length,
    GLchar *destination)
{
    const size_t source_length = source ? std::strlen(source) : 0;
    if (length)
        *length = 0;
    if (!destination || max_length <= 0)
        return;

    const size_t copy_length = std::min(
        source_length, static_cast<size_t>(max_length - 1));
    if (copy_length > 0)
        std::memcpy(destination, source, copy_length);
    destination[copy_length] = '\0';
    if (length)
        *length = static_cast<GLsizei>(copy_length);
}

static GLenum gl_type_for_shader_type(fxgl::ShaderType type)
{
    switch (type)
    {
        case fxgl::ShaderType::Float: return GL_FLOAT;
        case fxgl::ShaderType::Int: return GL_INT;
        case fxgl::ShaderType::Bool: return GL_BOOL;
        case fxgl::ShaderType::Vec2: return GL_FLOAT_VEC2;
        case fxgl::ShaderType::Vec3: return GL_FLOAT_VEC3;
        case fxgl::ShaderType::Vec4: return GL_FLOAT_VEC4;
        case fxgl::ShaderType::IVec2: return GL_INT_VEC2;
        case fxgl::ShaderType::IVec3: return GL_INT_VEC3;
        case fxgl::ShaderType::IVec4: return GL_INT_VEC4;
        case fxgl::ShaderType::BVec2: return GL_BOOL_VEC2;
        case fxgl::ShaderType::BVec3: return GL_BOOL_VEC3;
        case fxgl::ShaderType::BVec4: return GL_BOOL_VEC4;
        case fxgl::ShaderType::Mat2: return GL_FLOAT_MAT2;
        case fxgl::ShaderType::Mat3: return GL_FLOAT_MAT3;
        case fxgl::ShaderType::Mat4: return GL_FLOAT_MAT4;
        case fxgl::ShaderType::Sampler2D: return GL_SAMPLER_2D;
        default: return 0;
    }
}

static void copy_name(
    const char *source,
    GLsizei bufsize,
    GLsizei *length,
    GLchar *destination)
{
    if (length)
        *length = 0;
    if (!destination || bufsize <= 0)
        return;
    const size_t source_length = source ? std::strlen(source) : 0;
    const size_t copied = std::min(
        source_length, static_cast<size_t>(bufsize - 1));
    if (copied > 0)
        std::memcpy(destination, source, copied);
    destination[copied] = '\0';
    if (length)
        *length = static_cast<GLsizei>(copied);
}

static int find_uniform(const ProgramObject& program, const char *name)
{
    for (size_t i = 0; i < program.uniform_count; ++i)
        if (std::strcmp(program.uniforms[i].name, name) == 0)
            return static_cast<int>(i);
    return -1;
}

static int find_attribute(const ProgramObject& program, const char *name)
{
    for (size_t i = 0; i < program.vertex_module.attribute_count; ++i)
        if (std::strcmp(program.vertex_module.attributes[i].name, name) == 0)
            return static_cast<int>(i);
    return -1;
}

static bool add_uniform(
    ProgramObject& program,
    const fxgl::ShaderVariable& variable,
    uint8_t& location)
{
    const int existing = find_uniform(program, variable.name);
    if (existing >= 0)
    {
        if (program.uniforms[existing].type != variable.type)
            return false;
        location = static_cast<uint8_t>(existing);
        return true;
    }

    if (program.uniform_count >= fxgl::GLSL_MAX_UNIFORMS)
        return false;

    const size_t index = program.uniform_count++;
    program.uniforms[index] = variable;
    program.uniforms[index].index = static_cast<uint8_t>(index);
    program.uniform_values[index] = {};
    program.uniform_values[index].type = variable.type;
    location = static_cast<uint8_t>(index);
    return true;
}

static int find_bound_attribute(
    const ProgramObject& program,
    const char *name)
{
    for (size_t i = 0; i < program.attribute_binding_count; ++i)
        if (std::strcmp(program.attribute_bindings[i].name, name) == 0)
            return static_cast<int>(program.attribute_bindings[i].location);
    return -1;
}

static size_t shader_component_count(fxgl::ShaderType type)
{
    switch (type)
    {
        case fxgl::ShaderType::Float:
        case fxgl::ShaderType::Int:
        case fxgl::ShaderType::Bool:
        case fxgl::ShaderType::Sampler2D:
            return 1;
        case fxgl::ShaderType::Vec2: return 2;
        case fxgl::ShaderType::Vec3: return 3;
        case fxgl::ShaderType::Vec4: return 4;
        case fxgl::ShaderType::IVec2:
        case fxgl::ShaderType::BVec2: return 2;
        case fxgl::ShaderType::IVec3:
        case fxgl::ShaderType::BVec3: return 3;
        case fxgl::ShaderType::IVec4:
        case fxgl::ShaderType::BVec4: return 4;
        case fxgl::ShaderType::Mat2: return 4;
        case fxgl::ShaderType::Mat3: return 9;
        case fxgl::ShaderType::Mat4: return 16;
        default: return 0;
    }
}

static bool link_program(ProgramObject& program)
{
    program.linked = false;
    program.validated = false;
    program.info_log[0] = '\0';
    program.uniform_count = 0;
    program.varying_scalar_count = 0;
    std::memset(program.vertex_uniform_map, 0xff,
                sizeof(program.vertex_uniform_map));
    std::memset(program.fragment_uniform_map, 0xff,
                sizeof(program.fragment_uniform_map));
    std::memset(program.attribute_locations, 0xff,
                sizeof(program.attribute_locations));
    std::memset(program.vertex_varying_offsets, 0xff,
                sizeof(program.vertex_varying_offsets));
    std::memset(program.fragment_varying_offsets, 0xff,
                sizeof(program.fragment_varying_offsets));

    ShaderObject *vertex = find_shader(program.vertex_shader);
    ShaderObject *fragment = find_shader(program.fragment_shader);
    if (!vertex || !fragment)
    {
        set_log(program.info_log, sizeof(program.info_log),
                "Link failed: one vertex shader and one fragment shader must be attached");
        return false;
    }
    if (!vertex->module.compiled || !fragment->module.compiled)
    {
        set_log(program.info_log, sizeof(program.info_log),
                "Link failed: attached shaders must compile successfully");
        return false;
    }

    program.vertex_module = vertex->module;
    program.fragment_module = fragment->module;

    for (size_t i = 0; i < program.vertex_module.uniform_count; ++i)
    {
        uint8_t location = 0;
        if (!add_uniform(program, program.vertex_module.uniforms[i], location))
        {
            set_log(program.info_log, sizeof(program.info_log),
                    "Link failed: uniform type mismatch or uniform limit exceeded");
            return false;
        }
        program.vertex_uniform_map[i] = location;
    }

    for (size_t i = 0; i < program.fragment_module.uniform_count; ++i)
    {
        uint8_t location = 0;
        if (!add_uniform(program, program.fragment_module.uniforms[i], location))
        {
            set_log(program.info_log, sizeof(program.info_log),
                    "Link failed: uniform type mismatch or uniform limit exceeded");
            return false;
        }
        program.fragment_uniform_map[i] = location;
    }

    for (size_t i = 0; i < program.vertex_module.attribute_count; ++i)
    {
        const fxgl::ShaderType type =
            program.vertex_module.attributes[i].type;
        if (type != fxgl::ShaderType::Float &&
            type != fxgl::ShaderType::Vec2 &&
            type != fxgl::ShaderType::Vec3 &&
            type != fxgl::ShaderType::Vec4)
        {
            set_log(program.info_log, sizeof(program.info_log),
                    "Link failed: vertex attributes must be float or vec2/vec3/vec4");
            return false;
        }
    }

    bool occupied[fxgl::GLSL_MAX_ATTRIBUTES] = {};
    /* Reserve every explicitly bound location before assigning defaults;
       otherwise an earlier unbound attribute could steal a later binding. */
    for (size_t i = 0; i < program.vertex_module.attribute_count; ++i)
    {
        const fxgl::ShaderVariable& attribute =
            program.vertex_module.attributes[i];
        const int location = find_bound_attribute(program, attribute.name);
        if (location < 0)
            continue;
        if (location >= static_cast<int>(fxgl::GLSL_MAX_ATTRIBUTES) ||
            occupied[static_cast<size_t>(location)])
        {
            set_log(program.info_log, sizeof(program.info_log),
                    "Link failed: duplicate or out-of-range attribute binding");
            return false;
        }
        occupied[static_cast<size_t>(location)] = true;
        program.attribute_locations[i] = static_cast<uint8_t>(location);
    }

    for (size_t i = 0; i < program.vertex_module.attribute_count; ++i)
    {
        if (program.attribute_locations[i] != 0xff)
            continue;
        size_t location = 0;
        while (location < fxgl::GLSL_MAX_ATTRIBUTES && occupied[location])
            ++location;
        if (location >= fxgl::GLSL_MAX_ATTRIBUTES)
        {
            set_log(program.info_log, sizeof(program.info_log),
                    "Link failed: vertex attribute limit exceeded");
            return false;
        }
        occupied[location] = true;
        program.attribute_locations[i] = static_cast<uint8_t>(location);
    }

    size_t scalar_offset = 0;
    for (size_t i = 0; i < program.vertex_module.varying_count; ++i)
    {
        const fxgl::ShaderVariable& varying =
            program.vertex_module.varyings[i];
        const size_t components = shader_component_count(varying.type);
        if (components == 0 ||
            scalar_offset + components > fxgl::GLSL_MAX_VARYINGS)
        {
            set_log(program.info_log, sizeof(program.info_log),
                    "Link failed: varying scalar budget exceeded");
            return false;
        }
        program.vertex_varying_offsets[i] =
            static_cast<uint8_t>(scalar_offset);
        scalar_offset += components;
    }
    program.varying_scalar_count = scalar_offset;

    for (size_t i = 0; i < program.fragment_module.varying_count; ++i)
    {
        const fxgl::ShaderVariable& fragment_varying =
            program.fragment_module.varyings[i];
        bool matched = false;
        for (size_t j = 0; j < program.vertex_module.varying_count; ++j)
        {
            const fxgl::ShaderVariable& vertex_varying =
                program.vertex_module.varyings[j];
            if (std::strcmp(fragment_varying.name, vertex_varying.name) == 0)
            {
                if (fragment_varying.type != vertex_varying.type)
                {
                    set_log(program.info_log, sizeof(program.info_log),
                            "Link failed: varying type mismatch");
                    return false;
                }
                program.fragment_varying_offsets[i] =
                    program.vertex_varying_offsets[j];
                matched = true;
                break;
            }
        }
        if (!matched)
        {
            set_log(program.info_log, sizeof(program.info_log),
                    "Link failed: fragment varying has no vertex output");
            return false;
        }
    }

    program.linked = true;
    return true;
}

static void release_program(ProgramObject& program)
{
    ShaderObject *vertex = find_shader(program.vertex_shader);
    ShaderObject *fragment = find_shader(program.fragment_shader);
    if (vertex && vertex->attached_count > 0)
    {
        --vertex->attached_count;
        release_shader_if_pending(*vertex);
    }
    if (fragment && fragment != vertex && fragment->attached_count > 0)
    {
        --fragment->attached_count;
        release_shader_if_pending(*fragment);
    }
    program = {};
}

static void release_program_if_pending(ProgramObject& program)
{
    if (program.alive && program.delete_pending &&
        g_current_program != static_cast<GLuint>(&program - g_programs + 1))
        release_program(program);
}

static ProgramObject *current_program()
{
    if (g_current_program == 0)
        return nullptr;
    ProgramObject *program = find_program(g_current_program);
    return program && program->linked ? program : nullptr;
}

static bool valid_uniform_location(
    ProgramObject& program,
    GLint location,
    fxgl::ShaderType type,
    size_t components)
{
    if (location == -1)
        return true;
    if (location < 0 || static_cast<size_t>(location) >= program.uniform_count)
        return false;
    const fxgl::ShaderVariable& uniform = program.uniforms[location];
    return uniform.type == type &&
           shader_component_count(type) == components;
}

static void set_uniform_integer(
    GLint location,
    const GLint *values,
    size_t components)
{
    if (!fxglHasCurrentContext())
        return;
    if (location == -1)
        return;
    ProgramObject *program = current_program();
    if (!program)
    {
        fxgl::report_error(GL_INVALID_OPERATION);
        return;
    }
    if (!values || location < 0 ||
        static_cast<size_t>(location) >= program->uniform_count)
    {
        fxgl::report_error(GL_INVALID_OPERATION);
        return;
    }

    const fxgl::ShaderType declared = program->uniforms[location].type;
    fxgl::ShaderType expected = fxgl::ShaderType::Invalid;
    switch (components)
    {
        case 1:
            if (declared == fxgl::ShaderType::Int ||
                declared == fxgl::ShaderType::Bool ||
                declared == fxgl::ShaderType::Sampler2D)
                expected = declared;
            break;
        case 2:
            if (declared == fxgl::ShaderType::IVec2 ||
                declared == fxgl::ShaderType::BVec2)
                expected = declared;
            break;
        case 3:
            if (declared == fxgl::ShaderType::IVec3 ||
                declared == fxgl::ShaderType::BVec3)
                expected = declared;
            break;
        case 4:
            if (declared == fxgl::ShaderType::IVec4 ||
                declared == fxgl::ShaderType::BVec4)
                expected = declared;
            break;
        default:
            break;
    }
    if (expected == fxgl::ShaderType::Invalid)
    {
        fxgl::report_error(GL_INVALID_OPERATION);
        return;
    }

    fxgl::ShaderValue& destination = program->uniform_values[location];
    destination = {};
    destination.type = expected;
    for (size_t i = 0; i < components; ++i)
    {
        const float value = static_cast<float>(values[i]);
        destination.data[i] =
            (expected == fxgl::ShaderType::Bool ||
             expected == fxgl::ShaderType::BVec2 ||
             expected == fxgl::ShaderType::BVec3 ||
             expected == fxgl::ShaderType::BVec4)
                ? (value != 0.0f ? 1.0f : 0.0f)
                : value;
    }
}

static void set_uniform(
    GLint location,
    fxgl::ShaderType type,
    const GLfloat *values,
    size_t components)
{
    if (!fxglHasCurrentContext())
        return;
    if (location == -1)
        return;
    ProgramObject *program = current_program();
    if (!program)
    {
        fxgl::report_error(GL_INVALID_OPERATION);
        return;
    }
    if (!values ||
        !valid_uniform_location(*program, location, type, components))
    {
        fxgl::report_error(GL_INVALID_OPERATION);
        return;
    }

    fxgl::ShaderValue& destination = program->uniform_values[location];
    destination = {};
    destination.type = type;
    for (size_t i = 0; i < components; ++i)
        destination.data[i] = values[i];
}

} // namespace

namespace fxgl
{
void program_context_reset()
{
    for (ShaderObject& shader : g_shaders)
        shader = {};
    for (ProgramObject& program : g_programs)
        program = {};
    g_current_program = 0;
}

void report_error(unsigned int error)
{
    report_context_error(error);
}

unsigned int current_program_name()
{
    return g_current_program;
}

bool get_current_program_view(LinkedProgramView& view)
{
    ProgramObject *program = current_program();
    if (!program)
        return false;

    view = {};
    view.vertex_shader = &program->vertex_module;
    view.fragment_shader = &program->fragment_module;
    view.uniform_values = program->uniform_values;
    view.uniform_count = program->uniform_count;
    std::memcpy(view.vertex_uniform_map, program->vertex_uniform_map,
                sizeof(view.vertex_uniform_map));
    std::memcpy(view.fragment_uniform_map, program->fragment_uniform_map,
                sizeof(view.fragment_uniform_map));
    std::memcpy(view.attribute_locations, program->attribute_locations,
                sizeof(view.attribute_locations));
    std::memcpy(view.vertex_varying_offsets, program->vertex_varying_offsets,
                sizeof(view.vertex_varying_offsets));
    std::memcpy(view.fragment_varying_offsets, program->fragment_varying_offsets,
                sizeof(view.fragment_varying_offsets));
    view.varying_scalar_count = program->varying_scalar_count;
    return true;
}
} // namespace fxgl

extern "C"
{

GLuint glCreateShader(GLenum type)
{
    if (!fxglHasCurrentContext())
        return 0;
    if (type != GL_VERTEX_SHADER && type != GL_FRAGMENT_SHADER)
    {
        fxgl::report_error(GL_INVALID_ENUM);
        return 0;
    }

    for (size_t i = 0; i < FXGL_MAX_SHADER_OBJECTS; ++i)
    {
        if (!g_shaders[i].alive)
        {
            g_shaders[i] = {};
            g_shaders[i].alive = true;
            g_shaders[i].type = type;
            return static_cast<GLuint>(i + 1);
        }
    }

    fxgl::report_error(GL_OUT_OF_MEMORY);
    return 0;
}

void glDeleteShader(GLuint shader_name)
{
    if (!fxglHasCurrentContext())
        return;
    ShaderObject *shader = find_shader(shader_name);
    if (!shader)
        return;
    shader->delete_pending = true;
    release_shader_if_pending(*shader);
}

void glShaderSource(
    GLuint shader_name,
    GLsizei count,
    const GLchar *const *strings,
    const GLint *lengths)
{
    if (!fxglHasCurrentContext())
        return;
    ShaderObject *shader = find_shader(shader_name);
    if (!shader)
    {
        fxgl::report_error(GL_INVALID_VALUE);
        return;
    }
    if (count < 0 || (count > 0 && !strings))
    {
        fxgl::report_error(GL_INVALID_VALUE);
        return;
    }

    char combined[FXGL_SHADER_SOURCE_BYTES] = {};
    size_t combined_length = 0;
    for (GLsizei i = 0; i < count; ++i)
    {
        if (!strings[i])
        {
            fxgl::report_error(GL_INVALID_VALUE);
            return;
        }
        size_t length = 0;
        if (lengths && lengths[i] >= 0)
            length = static_cast<size_t>(lengths[i]);
        else if (lengths && lengths[i] < -1)
        {
            fxgl::report_error(GL_INVALID_VALUE);
            return;
        }
        else
            length = std::strlen(strings[i]);

        if (length > FXGL_SHADER_SOURCE_BYTES - 1 - combined_length)
        {
            fxgl::report_error(GL_OUT_OF_MEMORY);
            return;
        }
        std::memcpy(combined + combined_length, strings[i], length);
        combined_length += length;
    }
    combined[combined_length] = '\0';
    std::memcpy(shader->source, combined, combined_length + 1);
    shader->source_length = combined_length;
    shader->module = {};
}

void glCompileShader(GLuint shader_name)
{
    if (!fxglHasCurrentContext())
        return;
    ShaderObject *shader = find_shader(shader_name);
    if (!shader)
    {
        fxgl::report_error(GL_INVALID_VALUE);
        return;
    }

    const fxgl::ShaderStage stage = shader->type == GL_VERTEX_SHADER
        ? fxgl::ShaderStage::Vertex : fxgl::ShaderStage::Fragment;
    fxgl::compile_glsl(stage, shader->source, shader->module);
}

void glGetShaderiv(GLuint shader_name, GLenum pname, GLint *params)
{
    if (!fxglHasCurrentContext())
        return;
    ShaderObject *shader = find_shader(shader_name);
    if (!shader)
    {
        fxgl::report_error(GL_INVALID_VALUE);
        return;
    }
    if (!params)
    {
        fxgl::report_error(GL_INVALID_VALUE);
        return;
    }

    switch (pname)
    {
        case GL_SHADER_TYPE:
            *params = static_cast<GLint>(shader->type);
            break;
        case GL_DELETE_STATUS:
            *params = shader->delete_pending ? GL_TRUE : GL_FALSE;
            break;
        case GL_COMPILE_STATUS:
            *params = shader->module.compiled ? GL_TRUE : GL_FALSE;
            break;
        case GL_INFO_LOG_LENGTH:
            *params = static_cast<GLint>(std::strlen(shader->module.info_log) + 1);
            break;
        case GL_SHADER_SOURCE_LENGTH:
            *params = static_cast<GLint>(shader->source_length + 1);
            break;
        default:
            fxgl::report_error(GL_INVALID_ENUM);
            break;
    }
}

void glGetShaderInfoLog(
    GLuint shader_name,
    GLsizei max_length,
    GLsizei *length,
    GLchar *info_log)
{
    if (!fxglHasCurrentContext())
        return;
    ShaderObject *shader = find_shader(shader_name);
    if (!shader)
    {
        fxgl::report_error(GL_INVALID_VALUE);
        return;
    }
    if (max_length < 0)
    {
        fxgl::report_error(GL_INVALID_VALUE);
        return;
    }
    copy_log(shader->module.info_log, max_length, length, info_log);
}

void glGetShaderSource(
    GLuint shader_name,
    GLsizei bufsize,
    GLsizei *length,
    GLchar *source)
{
    if (!fxglHasCurrentContext())
        return;
    ShaderObject *shader = find_shader(shader_name);
    if (!shader)
    {
        fxgl::report_error(GL_INVALID_VALUE);
        return;
    }
    if (bufsize < 0)
    {
        fxgl::report_error(GL_INVALID_VALUE);
        return;
    }
    copy_log(shader->source, bufsize, length, source);
}

GLboolean glIsShader(GLuint shader_name)
{
    if (!fxglHasCurrentContext())
        return GL_FALSE;
    return find_shader(shader_name) ? GL_TRUE : GL_FALSE;
}

GLuint glCreateProgram(void)
{
    if (!fxglHasCurrentContext())
        return 0;
    for (size_t i = 0; i < FXGL_MAX_PROGRAM_OBJECTS; ++i)
    {
        if (!g_programs[i].alive)
        {
            g_programs[i] = {};
            g_programs[i].alive = true;
            return static_cast<GLuint>(i + 1);
        }
    }
    fxgl::report_error(GL_OUT_OF_MEMORY);
    return 0;
}

void glDeleteProgram(GLuint program_name)
{
    if (!fxglHasCurrentContext())
        return;
    ProgramObject *program = find_program(program_name);
    if (!program)
        return;
    program->delete_pending = true;
    release_program_if_pending(*program);
}

void glAttachShader(GLuint program_name, GLuint shader_name)
{
    if (!fxglHasCurrentContext())
        return;
    ProgramObject *program = find_program(program_name);
    ShaderObject *shader = find_shader(shader_name);
    if (!program || !shader)
    {
        fxgl::report_error(GL_INVALID_VALUE);
        return;
    }

    GLuint *attachment = shader->type == GL_VERTEX_SHADER
        ? &program->vertex_shader : &program->fragment_shader;
    if (*attachment != 0)
    {
        fxgl::report_error(GL_INVALID_OPERATION);
        return;
    }
    *attachment = shader_name;
    ++shader->attached_count;
    program->linked = false;
}

void glDetachShader(GLuint program_name, GLuint shader_name)
{
    if (!fxglHasCurrentContext())
        return;
    ProgramObject *program = find_program(program_name);
    ShaderObject *shader = find_shader(shader_name);
    if (!program || !shader)
    {
        fxgl::report_error(GL_INVALID_VALUE);
        return;
    }

    GLuint *attachment = nullptr;
    if (program->vertex_shader == shader_name)
        attachment = &program->vertex_shader;
    else if (program->fragment_shader == shader_name)
        attachment = &program->fragment_shader;
    if (!attachment)
    {
        fxgl::report_error(GL_INVALID_OPERATION);
        return;
    }

    *attachment = 0;
    if (shader->attached_count > 0)
        --shader->attached_count;
    release_shader_if_pending(*shader);
    program->linked = false;
}

void glLinkProgram(GLuint program_name)
{
    if (!fxglHasCurrentContext())
        return;
    ProgramObject *program = find_program(program_name);
    if (!program)
    {
        fxgl::report_error(GL_INVALID_VALUE);
        return;
    }
    link_program(*program);
}

void glUseProgram(GLuint program_name)
{
    if (!fxglHasCurrentContext())
        return;
    if (program_name != 0)
    {
        ProgramObject *program = find_program(program_name);
        if (!program || !program->linked || program->delete_pending)
        {
            fxgl::report_error(GL_INVALID_OPERATION);
            return;
        }
    }

    const GLuint old_program = g_current_program;
    g_current_program = program_name;
    if (old_program != 0 && old_program != program_name)
    {
        ProgramObject *old = find_program(old_program);
        if (old)
            release_program_if_pending(*old);
    }
}

void glGetProgramiv(GLuint program_name, GLenum pname, GLint *params)
{
    if (!fxglHasCurrentContext())
        return;
    ProgramObject *program = find_program(program_name);
    if (!program)
    {
        fxgl::report_error(GL_INVALID_VALUE);
        return;
    }
    if (!params)
    {
        fxgl::report_error(GL_INVALID_VALUE);
        return;
    }

    switch (pname)
    {
        case GL_LINK_STATUS:
            *params = program->linked ? GL_TRUE : GL_FALSE;
            break;
        case GL_VALIDATE_STATUS:
            *params = program->validated ? GL_TRUE : GL_FALSE;
            break;
        case GL_DELETE_STATUS:
            *params = program->delete_pending ? GL_TRUE : GL_FALSE;
            break;
        case GL_INFO_LOG_LENGTH:
            *params = static_cast<GLint>(std::strlen(program->info_log) + 1);
            break;
        case GL_ATTACHED_SHADERS:
            *params = (program->vertex_shader != 0 ? 1 : 0) +
                      (program->fragment_shader != 0 ? 1 : 0);
            break;
        case GL_ACTIVE_ATTRIBUTES:
            *params = program->linked
                ? static_cast<GLint>(program->vertex_module.attribute_count) : 0;
            break;
        case GL_ACTIVE_UNIFORMS:
            *params = program->linked
                ? static_cast<GLint>(program->uniform_count) : 0;
            break;
        default:
            fxgl::report_error(GL_INVALID_ENUM);
            break;
    }
}

void glGetProgramInfoLog(
    GLuint program_name,
    GLsizei max_length,
    GLsizei *length,
    GLchar *info_log)
{
    if (!fxglHasCurrentContext())
        return;
    ProgramObject *program = find_program(program_name);
    if (!program)
    {
        fxgl::report_error(GL_INVALID_VALUE);
        return;
    }
    if (max_length < 0)
    {
        fxgl::report_error(GL_INVALID_VALUE);
        return;
    }
    copy_log(program->info_log, max_length, length, info_log);
}

GLboolean glIsProgram(GLuint program_name)
{
    if (!fxglHasCurrentContext())
        return GL_FALSE;
    return find_program(program_name) ? GL_TRUE : GL_FALSE;
}

void glBindAttribLocation(
    GLuint program_name,
    GLuint index,
    const GLchar *name)
{
    if (!fxglHasCurrentContext())
        return;
    ProgramObject *program = find_program(program_name);
    if (!program)
    {
        fxgl::report_error(GL_INVALID_VALUE);
        return;
    }
    if (!name || index >= fxgl::GLSL_MAX_ATTRIBUTES)
    {
        fxgl::report_error(GL_INVALID_VALUE);
        return;
    }
    const size_t length = std::strlen(name);
    if (length == 0 || length >= fxgl::GLSL_NAME_BYTES)
    {
        fxgl::report_error(GL_INVALID_VALUE);
        return;
    }

    for (size_t i = 0; i < program->attribute_binding_count; ++i)
    {
        if (std::strcmp(program->attribute_bindings[i].name, name) == 0)
        {
            program->attribute_bindings[i].location = index;
            program->linked = false;
            return;
        }
    }
    if (program->attribute_binding_count >= fxgl::GLSL_MAX_ATTRIBUTES)
    {
        fxgl::report_error(GL_OUT_OF_MEMORY);
        return;
    }

    AttributeBinding& binding =
        program->attribute_bindings[program->attribute_binding_count++];
    std::snprintf(binding.name, sizeof(binding.name), "%s", name);
    binding.location = index;
    program->linked = false;
}

GLint glGetAttribLocation(GLuint program_name, const GLchar *name)
{
    if (!fxglHasCurrentContext())
        return -1;
    ProgramObject *program = find_program(program_name);
    if (!program || !name)
    {
        fxgl::report_error(GL_INVALID_VALUE);
        return -1;
    }
    if (!program->linked)
    {
        fxgl::report_error(GL_INVALID_OPERATION);
        return -1;
    }

    const int index = find_attribute(*program, name);
    return index < 0 ? -1 : program->attribute_locations[index];
}

GLint glGetUniformLocation(GLuint program_name, const GLchar *name)
{
    if (!fxglHasCurrentContext())
        return -1;
    ProgramObject *program = find_program(program_name);
    if (!program || !name)
    {
        fxgl::report_error(GL_INVALID_VALUE);
        return -1;
    }
    if (!program->linked)
    {
        fxgl::report_error(GL_INVALID_OPERATION);
        return -1;
    }
    return find_uniform(*program, name);
}

void glGetAttachedShaders(
    GLuint program_name,
    GLsizei max_count,
    GLsizei *count,
    GLuint *shaders)
{
    if (!fxglHasCurrentContext())
        return;
    ProgramObject *program = find_program(program_name);
    if (!program)
    {
        fxgl::report_error(GL_INVALID_VALUE);
        return;
    }
    if (max_count < 0 || (max_count > 0 && !shaders))
    {
        fxgl::report_error(GL_INVALID_VALUE);
        return;
    }

    GLsizei written = 0;
    if (program->vertex_shader != 0 && written < max_count)
        shaders[written++] = program->vertex_shader;
    if (program->fragment_shader != 0 && written < max_count)
        shaders[written++] = program->fragment_shader;
    if (count)
        *count = written;
}

void glGetActiveAttrib(
    GLuint program_name,
    GLuint index,
    GLsizei bufsize,
    GLsizei *length,
    GLint *size,
    GLenum *type,
    GLchar *name)
{
    if (!fxglHasCurrentContext())
        return;
    ProgramObject *program = find_program(program_name);
    if (!program)
    {
        fxgl::report_error(GL_INVALID_VALUE);
        return;
    }
    if (!program->linked)
    {
        fxgl::report_error(GL_INVALID_OPERATION);
        return;
    }
    if (index >= program->vertex_module.attribute_count || bufsize < 0)
    {
        fxgl::report_error(GL_INVALID_VALUE);
        return;
    }
    const fxgl::ShaderVariable& variable =
        program->vertex_module.attributes[index];
    if (size) *size = 1;
    if (type) *type = gl_type_for_shader_type(variable.type);
    copy_name(variable.name, bufsize, length, name);
}

void glGetActiveUniform(
    GLuint program_name,
    GLuint index,
    GLsizei bufsize,
    GLsizei *length,
    GLint *size,
    GLenum *type,
    GLchar *name)
{
    if (!fxglHasCurrentContext())
        return;
    ProgramObject *program = find_program(program_name);
    if (!program)
    {
        fxgl::report_error(GL_INVALID_VALUE);
        return;
    }
    if (!program->linked)
    {
        fxgl::report_error(GL_INVALID_OPERATION);
        return;
    }
    if (index >= program->uniform_count || bufsize < 0)
    {
        fxgl::report_error(GL_INVALID_VALUE);
        return;
    }
    const fxgl::ShaderVariable& variable = program->uniforms[index];
    if (size) *size = 1;
    if (type) *type = gl_type_for_shader_type(variable.type);
    copy_name(variable.name, bufsize, length, name);
}

void glValidateProgram(GLuint program_name)
{
    if (!fxglHasCurrentContext())
        return;
    ProgramObject *program = find_program(program_name);
    if (!program)
    {
        fxgl::report_error(GL_INVALID_VALUE);
        return;
    }
    program->validated = program->linked;
    if (!program->validated)
        set_log(program->info_log, sizeof(program->info_log),
                "Validation failed: program is not successfully linked");
}

void glUniform1f(GLint location, GLfloat v0)
{
    set_uniform(location, fxgl::ShaderType::Float, &v0, 1);
}

void glUniform2f(GLint location, GLfloat v0, GLfloat v1)
{
    const GLfloat values[2] = {v0, v1};
    set_uniform(location, fxgl::ShaderType::Vec2, values, 2);
}

void glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2)
{
    const GLfloat values[3] = {v0, v1, v2};
    set_uniform(location, fxgl::ShaderType::Vec3, values, 3);
}

void glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3)
{
    const GLfloat values[4] = {v0, v1, v2, v3};
    set_uniform(location, fxgl::ShaderType::Vec4, values, 4);
}

void glUniform1i(GLint location, GLint v0)
{
    if (!fxglHasCurrentContext())
        return;
    if (location == -1)
        return;
    ProgramObject *program = current_program();
    if (!program)
    {
        fxgl::report_error(GL_INVALID_OPERATION);
        return;
    }
    if (location < 0 || static_cast<size_t>(location) >= program->uniform_count)
    {
        fxgl::report_error(GL_INVALID_OPERATION);
        return;
    }

    const fxgl::ShaderType type = program->uniforms[location].type;
    if (type != fxgl::ShaderType::Int &&
        type != fxgl::ShaderType::Bool &&
        type != fxgl::ShaderType::Sampler2D)
    {
        fxgl::report_error(GL_INVALID_OPERATION);
        return;
    }

    program->uniform_values[location] = {};
    program->uniform_values[location].type = type;
    program->uniform_values[location].data[0] =
        type == fxgl::ShaderType::Bool
            ? (v0 != 0 ? 1.0f : 0.0f)
            : static_cast<float>(v0);
}

void glUniform1fv(GLint location, GLsizei count, const GLfloat *value)
{
    if (!fxglHasCurrentContext()) return;
    if (location == -1 || count == 0) return;
    if (count != 1 || !value)
    {
        fxgl::report_error(GL_INVALID_VALUE);
        return;
    }
    set_uniform(location, fxgl::ShaderType::Float, value, 1);
}

void glUniform2fv(GLint location, GLsizei count, const GLfloat *value)
{
    if (!fxglHasCurrentContext()) return;
    if (location == -1 || count == 0) return;
    if (count != 1 || !value)
    {
        fxgl::report_error(GL_INVALID_VALUE);
        return;
    }
    set_uniform(location, fxgl::ShaderType::Vec2, value, 2);
}

void glUniform3fv(GLint location, GLsizei count, const GLfloat *value)
{
    if (!fxglHasCurrentContext()) return;
    if (location == -1 || count == 0) return;
    if (count != 1 || !value)
    {
        fxgl::report_error(GL_INVALID_VALUE);
        return;
    }
    set_uniform(location, fxgl::ShaderType::Vec3, value, 3);
}

void glUniform4fv(GLint location, GLsizei count, const GLfloat *value)
{
    if (!fxglHasCurrentContext()) return;
    if (location == -1 || count == 0) return;
    if (count != 1 || !value)
    {
        fxgl::report_error(GL_INVALID_VALUE);
        return;
    }
    set_uniform(location, fxgl::ShaderType::Vec4, value, 4);
}

void glGetUniformfv(GLuint program_name, GLint location, GLfloat *params)
{
    if (!fxglHasCurrentContext()) return;
    ProgramObject *program = find_program(program_name);
    if (!program || !program->linked)
    {
        fxgl::report_error(program ? GL_INVALID_OPERATION : GL_INVALID_VALUE);
        return;
    }
    if (!params || location < 0 ||
        static_cast<size_t>(location) >= program->uniform_count)
    {
        fxgl::report_error(GL_INVALID_VALUE);
        return;
    }
    const fxgl::ShaderValue& value = program->uniform_values[location];
    const size_t components = shader_component_count(value.type);
    for (size_t i = 0; i < components; ++i)
        params[i] = value.data[i];
}

void glGetUniformiv(GLuint program_name, GLint location, GLint *params)
{
    if (!fxglHasCurrentContext()) return;
    ProgramObject *program = find_program(program_name);
    if (!program || !program->linked)
    {
        fxgl::report_error(program ? GL_INVALID_OPERATION : GL_INVALID_VALUE);
        return;
    }
    if (!params || location < 0 ||
        static_cast<size_t>(location) >= program->uniform_count)
    {
        fxgl::report_error(GL_INVALID_VALUE);
        return;
    }
    const fxgl::ShaderValue& value = program->uniform_values[location];
    const size_t components = shader_component_count(value.type);
    for (size_t i = 0; i < components; ++i)
        params[i] = static_cast<GLint>(value.data[i]);
}

void glUniform2i(GLint location, GLint v0, GLint v1)
{
    const GLint values[2] = {v0, v1};
    set_uniform_integer(location, values, 2);
}

void glUniform3i(GLint location, GLint v0, GLint v1, GLint v2)
{
    const GLint values[3] = {v0, v1, v2};
    set_uniform_integer(location, values, 3);
}

void glUniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3)
{
    const GLint values[4] = {v0, v1, v2, v3};
    set_uniform_integer(location, values, 4);
}

void glUniform1iv(GLint location, GLsizei count, const GLint *value)
{
    if (!fxglHasCurrentContext() || location == -1 || count == 0) return;
    if (count != 1 || !value)
    {
        fxgl::report_error(GL_INVALID_VALUE);
        return;
    }
    set_uniform_integer(location, value, 1);
}

void glUniform2iv(GLint location, GLsizei count, const GLint *value)
{
    if (!fxglHasCurrentContext() || location == -1 || count == 0) return;
    if (count != 1 || !value)
    {
        fxgl::report_error(GL_INVALID_VALUE);
        return;
    }
    set_uniform_integer(location, value, 2);
}

void glUniform3iv(GLint location, GLsizei count, const GLint *value)
{
    if (!fxglHasCurrentContext() || location == -1 || count == 0) return;
    if (count != 1 || !value)
    {
        fxgl::report_error(GL_INVALID_VALUE);
        return;
    }
    set_uniform_integer(location, value, 3);
}

void glUniform4iv(GLint location, GLsizei count, const GLint *value)
{
    if (!fxglHasCurrentContext() || location == -1 || count == 0) return;
    if (count != 1 || !value)
    {
        fxgl::report_error(GL_INVALID_VALUE);
        return;
    }
    set_uniform_integer(location, value, 4);
}

void glUniformMatrix2fv(
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const GLfloat *value)
{
    if (!fxglHasCurrentContext()) return;
    if (location == -1) return;
    ProgramObject *program = current_program();
    if (!program)
    {
        fxgl::report_error(GL_INVALID_OPERATION);
        return;
    }
    if (count < 0 || !value)
    {
        fxgl::report_error(GL_INVALID_VALUE);
        return;
    }
    if (count == 0) return;
    if (count != 1 || transpose != GL_FALSE || location < 0 ||
        static_cast<size_t>(location) >= program->uniform_count ||
        program->uniforms[location].type != fxgl::ShaderType::Mat2)
    {
        fxgl::report_error(GL_INVALID_OPERATION);
        return;
    }

    fxgl::ShaderValue& destination = program->uniform_values[location];
    destination = {};
    destination.type = fxgl::ShaderType::Mat2;
    std::memcpy(destination.data, value, 4 * sizeof(GLfloat));
}

void glUniformMatrix3fv(
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const GLfloat *value)
{
    if (!fxglHasCurrentContext()) return;
    if (location == -1) return;
    ProgramObject *program = current_program();
    if (!program)
    {
        fxgl::report_error(GL_INVALID_OPERATION);
        return;
    }
    if (count < 0 || !value)
    {
        fxgl::report_error(GL_INVALID_VALUE);
        return;
    }
    if (count == 0) return;
    if (count != 1 || transpose != GL_FALSE || location < 0 ||
        static_cast<size_t>(location) >= program->uniform_count ||
        program->uniforms[location].type != fxgl::ShaderType::Mat3)
    {
        fxgl::report_error(GL_INVALID_OPERATION);
        return;
    }

    fxgl::ShaderValue& destination = program->uniform_values[location];
    destination = {};
    destination.type = fxgl::ShaderType::Mat3;
    std::memcpy(destination.data, value, 9 * sizeof(GLfloat));
}

void glUniformMatrix4fv(
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const GLfloat *value)
{
    if (!fxglHasCurrentContext())
        return;
    if (location == -1)
        return;
    ProgramObject *program = current_program();
    if (!program)
    {
        fxgl::report_error(GL_INVALID_OPERATION);
        return;
    }
    if (count < 0 || !value)
    {
        fxgl::report_error(GL_INVALID_VALUE);
        return;
    }
    if (count == 0)
        return;
    if (count != 1 || transpose != GL_FALSE ||
        location < 0 || static_cast<size_t>(location) >= program->uniform_count ||
        program->uniforms[location].type != fxgl::ShaderType::Mat4)
    {
        fxgl::report_error(GL_INVALID_OPERATION);
        return;
    }

    fxgl::ShaderValue& destination = program->uniform_values[location];
    destination = {};
    destination.type = fxgl::ShaderType::Mat4;
    std::memcpy(destination.data, value, 16 * sizeof(GLfloat));
}

} // extern "C"
