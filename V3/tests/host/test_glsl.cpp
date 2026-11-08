#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "../../src/gles/glsl.hpp"

namespace
{
using fxgl::ShaderModule;
using fxgl::ShaderRuntime;
using fxgl::ShaderStage;
using fxgl::ShaderType;
using fxgl::ShaderValue;

static bool near(float left, float right, float epsilon = 0.0001f)
{
    return std::fabs(left - right) <= epsilon;
}

static ShaderValue value(ShaderType type, float a, float b = 0.0f,
                         float c = 0.0f, float d = 0.0f)
{
    ShaderValue result = {};
    result.type = type;
    result.data[0] = a;
    result.data[1] = b;
    result.data[2] = c;
    result.data[3] = d;
    return result;
}

static void identity_matrix(ShaderValue& matrix)
{
    matrix = {};
    matrix.type = ShaderType::Mat4;
    matrix.data[0] = 1.0f;
    matrix.data[5] = 1.0f;
    matrix.data[10] = 1.0f;
    matrix.data[15] = 1.0f;
}

static void test_compile_and_execute_vertex_shader()
{
    static const char source[] =
        "attribute vec4 a_position;\n"
        "attribute vec2 a_uv;\n"
        "uniform mat4 u_mvp;\n"
        "varying vec2 v_uv;\n"
        "void main() {\n"
        "  gl_Position = u_mvp * a_position;\n"
        "  v_uv = a_uv;\n"
        "}\n";

    ShaderModule module = {};
    assert(fxgl::compile_glsl(ShaderStage::Vertex, source, module));
    assert(module.compiled);
    assert(module.attribute_count == 2);
    assert(module.uniform_count == 1);
    assert(module.varying_count == 1);
    assert(std::strcmp(module.attributes[0].name, "a_position") == 0);
    assert(std::strcmp(module.attributes[1].name, "a_uv") == 0);
    assert(std::strcmp(module.uniforms[0].name, "u_mvp") == 0);
    assert(std::strcmp(module.varyings[0].name, "v_uv") == 0);

    ShaderRuntime runtime = {};
    runtime.attributes[0] = value(ShaderType::Vec4, 0.25f, -0.5f, 0.75f, 1.0f);
    runtime.attributes[1] = value(ShaderType::Vec2, 0.125f, 0.875f);
    identity_matrix(runtime.uniforms[0]);

    assert(fxgl::execute_glsl(module, runtime));
    assert(runtime.position.type == ShaderType::Vec4);
    assert(near(runtime.position.data[0], 0.25f));
    assert(near(runtime.position.data[1], -0.5f));
    assert(near(runtime.position.data[2], 0.75f));
    assert(near(runtime.position.data[3], 1.0f));
    assert(runtime.varyings[0].type == ShaderType::Vec2);
    assert(near(runtime.varyings[0].data[0], 0.125f));
    assert(near(runtime.varyings[0].data[1], 0.875f));
}

static int sampled_unit = -1;
static float sampled_u = 0.0f;
static float sampled_v = 0.0f;

static bool test_texture_sample(
    int unit,
    float u,
    float v,
    float rgba[4],
    void *)
{
    sampled_unit = unit;
    sampled_u = u;
    sampled_v = v;
    rgba[0] = 0.1f;
    rgba[1] = 0.2f;
    rgba[2] = 0.3f;
    rgba[3] = 0.4f;
    return true;
}

static void test_compile_and_execute_texture_fragment_shader()
{
    static const char source[] =
        "#version 100\n"
        "precision mediump float;\n"
        "uniform sampler2D u_texture;\n"
        "varying vec2 v_uv;\n"
        "void main() {\n"
        "  gl_FragColor = texture2D(u_texture, v_uv);\n"
        "}\n";

    ShaderModule module = {};
    assert(fxgl::compile_glsl(ShaderStage::Fragment, source, module));
    assert(module.uniform_count == 1);
    assert(module.varying_count == 1);
    assert(module.uniforms[0].type == ShaderType::Sampler2D);

    ShaderRuntime runtime = {};
    runtime.uniforms[0] = value(ShaderType::Sampler2D, 1.0f);
    runtime.varyings[0] = value(ShaderType::Vec2, 0.25f, 0.75f);

    sampled_unit = -1;
    assert(fxgl::execute_glsl(module, runtime, test_texture_sample));
    assert(sampled_unit == 1);
    assert(near(sampled_u, 0.25f));
    assert(near(sampled_v, 0.75f));
    assert(runtime.fragment_color.type == ShaderType::Vec4);
    assert(near(runtime.fragment_color.data[0], 0.1f));
    assert(near(runtime.fragment_color.data[1], 0.2f));
    assert(near(runtime.fragment_color.data[2], 0.3f));
    assert(near(runtime.fragment_color.data[3], 0.4f));
    assert(!fxgl::execute_glsl(module, runtime, nullptr));
}

static void test_constructors_swizzles_locals_and_scalar_vector_math()
{
    static const char source[] =
        "uniform vec4 u_tint;\n"
        "void main() {\n"
        "  vec4 reordered = vec4(u_tint.bgr, 1.0);\n"
        "  gl_FragColor = 0.5 * reordered;\n"
        "}\n";

    ShaderModule module = {};
    assert(fxgl::compile_glsl(ShaderStage::Fragment, source, module));
    assert(module.local_count == 1);
    assert(std::strcmp(module.locals[0].name, "reordered") == 0);

    ShaderRuntime runtime = {};
    runtime.uniforms[0] = value(ShaderType::Vec4, 0.2f, 0.4f, 0.6f, 0.8f);
    assert(fxgl::execute_glsl(module, runtime));
    assert(near(runtime.fragment_color.data[0], 0.3f));
    assert(near(runtime.fragment_color.data[1], 0.2f));
    assert(near(runtime.fragment_color.data[2], 0.1f));
    assert(near(runtime.fragment_color.data[3], 0.5f));
}

static void test_vector_builtins_and_functions()
{
    static const char source[] =
        "uniform vec3 u_direction;\n"
        "uniform vec3 u_min;\n"
        "uniform vec3 u_max;\n"
        "void main() {\n"
        "  vec3 n = normalize(u_direction);\n"
        "  vec3 mixed = mix(u_min, u_max, 0.25);\n"
        "  gl_FragColor = vec4(clamp(n + mixed, 0.0, 1.0), 1.0);\n"
        "}\n";

    ShaderModule module = {};
    assert(fxgl::compile_glsl(ShaderStage::Fragment, source, module));
    ShaderRuntime runtime = {};
    runtime.uniforms[0] = value(ShaderType::Vec3, 0.0f, 3.0f, 4.0f);
    runtime.uniforms[1] = value(ShaderType::Vec3, 0.1f, 0.2f, 0.3f);
    runtime.uniforms[2] = value(ShaderType::Vec3, 0.5f, 0.6f, 0.7f);

    assert(fxgl::execute_glsl(module, runtime));
    assert(near(runtime.fragment_color.data[0], 0.2f));
    assert(near(runtime.fragment_color.data[1], 0.9f));
    assert(near(runtime.fragment_color.data[2], 1.0f));
    assert(near(runtime.fragment_color.data[3], 1.0f));
}

static void test_mat2_mat3_uniforms_and_matrix_vector_multiplication()
{
    static const char source[] =
        "#version 100\n"
        "attribute vec2 a_position;\n"
        "uniform mat2 u_rotation;\n"
        "uniform mat3 u_scale;\n"
        "void main() {\n"
        "  vec2 rotated = u_rotation * a_position;\n"
        "  vec3 transformed = u_scale * vec3(rotated, 1.0);\n"
        "  gl_Position = vec4(transformed, 1.0);\n"
        "}\n";

    ShaderModule module = {};
    assert(fxgl::compile_glsl(ShaderStage::Vertex, source, module));
    assert(module.attribute_count == 1);
    assert(module.uniform_count == 2);
    assert(module.attributes[0].type == ShaderType::Vec2);
    assert(module.uniforms[0].type == ShaderType::Mat2);
    assert(module.uniforms[1].type == ShaderType::Mat3);

    ShaderRuntime runtime = {};
    runtime.attributes[0] = value(ShaderType::Vec2, 2.0f, 3.0f);
    runtime.uniforms[0] = {};
    runtime.uniforms[0].type = ShaderType::Mat2;
    runtime.uniforms[0].data[0] = 0.0f;
    runtime.uniforms[0].data[1] = -1.0f;
    runtime.uniforms[0].data[2] = 1.0f;
    runtime.uniforms[0].data[3] = 0.0f;

    runtime.uniforms[1] = {};
    runtime.uniforms[1].type = ShaderType::Mat3;
    runtime.uniforms[1].data[0] = 2.0f;
    runtime.uniforms[1].data[4] = 3.0f;
    runtime.uniforms[1].data[8] = 4.0f;

    assert(fxgl::execute_glsl(module, runtime));
    assert(near(runtime.position.data[0], 6.0f));
    assert(near(runtime.position.data[1], -6.0f));
    assert(near(runtime.position.data[2], 4.0f));
    assert(near(runtime.position.data[3], 1.0f));
}

static void test_matrix_construction_and_dimension_conversion()
{
    static const char source[] =
        "void main() {\n"
        "  mat2 m2 = mat2(2.0);\n"
        "  mat3 m3 = mat3(m2);\n"
        "  vec3 p = m3 * vec3(1.0, 2.0, 3.0);\n"
        "  gl_Position = vec4(p, 1.0);\n"
        "}\n";

    ShaderModule module = {};
    assert(fxgl::compile_glsl(ShaderStage::Vertex, source, module));
    assert(module.local_count == 3);

    ShaderRuntime runtime = {};
    assert(fxgl::execute_glsl(module, runtime));
    assert(near(runtime.position.data[0], 2.0f));
    assert(near(runtime.position.data[1], 4.0f));
    // mat3(mat2) promotes the new third row/column with an identity diagonal.
    assert(near(runtime.position.data[2], 3.0f));
    assert(near(runtime.position.data[3], 1.0f));
}

static void test_if_else_comparisons_and_logical_operators()
{
    static const char source[] =
        "#version 100\n"
        "uniform float u_value;\n"
        "void main() {\n"
        "  float red = 0.0;\n"
        "  if (u_value > 0.5 && u_value < 2.0) {\n"
        "    red = 1.0;\n"
        "  } else {\n"
        "    red = 0.25;\n"
        "  }\n"
        "  gl_FragColor = vec4(red, 0.0, 0.0, 1.0);\n"
        "}\n";

    ShaderModule module = {};
    assert(fxgl::compile_glsl(ShaderStage::Fragment, source, module));

    ShaderRuntime runtime = {};
    runtime.uniforms[0] = value(ShaderType::Float, 1.0f);
    assert(fxgl::execute_glsl(module, runtime));
    assert(near(runtime.fragment_color.data[0], 1.0f));
    assert(near(runtime.fragment_color.data[3], 1.0f));

    runtime = {};
    runtime.uniforms[0] = value(ShaderType::Float, 3.0f);
    assert(fxgl::execute_glsl(module, runtime));
    assert(near(runtime.fragment_color.data[0], 0.25f));
    assert(near(runtime.fragment_color.data[3], 1.0f));
}

static void test_nested_if_else_and_boolean_not()
{
    static const char source[] =
        "uniform bool u_flag;\n"
        "void main() {\n"
        "  float channel = 0.0;\n"
        "  if (!u_flag) {\n"
        "    channel = 0.2;\n"
        "  } else {\n"
        "    if (2.0 >= 1.0) channel = 0.8;\n"
        "    else channel = 0.4;\n"
        "  }\n"
        "  gl_FragColor = vec4(channel);\n"
        "}\n";

    ShaderModule module = {};
    assert(fxgl::compile_glsl(ShaderStage::Fragment, source, module));

    ShaderRuntime runtime = {};
    runtime.uniforms[0] = value(ShaderType::Bool, 0.0f);
    assert(fxgl::execute_glsl(module, runtime));
    assert(near(runtime.fragment_color.data[0], 0.2f));

    runtime = {};
    runtime.uniforms[0] = value(ShaderType::Bool, 1.0f);
    assert(fxgl::execute_glsl(module, runtime));
    assert(near(runtime.fragment_color.data[0], 0.8f));
}

static void test_compiler_rejects_unsupported_or_invalid_source()
{
    const char *unsupported[] = {
        "void main() { while (true) { gl_FragColor = vec4(1.0); } }",
        "attribute vec4 a_position[4]; void main() { gl_Position = a_position[0]; }",
        "void main() { gl_FragColor = unknown_name; }",
        "void main() { gl_FragColor = vec4(1.0, 2.0); }",
        "void main() { gl_Position = vec4(1.0); }"
    };

    for (const char *source : unsupported)
    {
        ShaderModule module = {};
        assert(!fxgl::compile_glsl(ShaderStage::Fragment, source, module));
        assert(!module.compiled);
        assert(module.info_log[0] != '\0');
    }

    ShaderModule module = {};
    assert(!fxgl::compile_glsl(ShaderStage::Vertex, nullptr, module));
    assert(module.info_log[0] != '\0');
}

static void test_instruction_budget_and_stack_safety()
{
    static const char source[] =
        "void main() {\n"
        "  gl_FragColor = vec4(1.0 + 2.0 + 3.0 + 4.0 + 5.0 + 6.0 + "
        "7.0 + 8.0 + 9.0 + 10.0, 0.0, 0.0, 1.0);\n"
        "}\n";
    ShaderModule module = {};
    assert(fxgl::compile_glsl(ShaderStage::Fragment, source, module));
    ShaderRuntime runtime = {};
    assert(fxgl::execute_glsl(module, runtime));
    assert(near(runtime.fragment_color.data[0], 55.0f));
}
} // namespace

int main()
{
    test_compile_and_execute_vertex_shader();
    test_compile_and_execute_texture_fragment_shader();
    test_constructors_swizzles_locals_and_scalar_vector_math();
    test_vector_builtins_and_functions();
    test_mat2_mat3_uniforms_and_matrix_vector_multiplication();
    test_matrix_construction_and_dimension_conversion();
    test_if_else_comparisons_and_logical_operators();
    test_nested_if_else_and_boolean_not();
    test_compiler_rejects_unsupported_or_invalid_source();
    test_instruction_budget_and_stack_safety();

    std::puts("GLSL ES subset compiler/VM tests: PASS");
    return 0;
}
