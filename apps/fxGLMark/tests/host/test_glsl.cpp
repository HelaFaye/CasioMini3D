#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "gles/glsl.hpp"

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
static float sampled_dudx = 0.0f;
static float sampled_dvdx = 0.0f;
static float sampled_dudy = 0.0f;
static float sampled_dvdy = 0.0f;

static bool test_texture_sample(
    int unit,
    float u,
    float v,
    float dudx,
    float dvdx,
    float dudy,
    float dvdy,
    float rgba[4],
    void *)
{
    sampled_unit = unit;
    sampled_u = u;
    sampled_v = v;
    sampled_dudx = dudx;
    sampled_dvdx = dvdx;
    sampled_dudy = dudy;
    sampled_dvdy = dvdy;
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
        "  gl_FragColor = texture2D(u_texture, v_uv * 2.0);\n"
        "}\n";

    ShaderModule module = {};
    assert(fxgl::compile_glsl(ShaderStage::Fragment, source, module));
    assert(module.uniform_count == 1);
    assert(module.varying_count == 1);
    assert(module.uniforms[0].type == ShaderType::Sampler2D);

    ShaderRuntime runtime = {};
    runtime.uniforms[0] = value(ShaderType::Sampler2D, 1.0f);
    runtime.varyings[0] = value(ShaderType::Vec2, 0.25f, 0.75f);
    runtime.varyings[0].ddx[0] = 0.01f;
    runtime.varyings[0].ddx[1] = 0.02f;
    runtime.varyings[0].ddy[0] = 0.03f;
    runtime.varyings[0].ddy[1] = 0.04f;

    sampled_unit = -1;
    assert(fxgl::execute_glsl(module, runtime, test_texture_sample));
    assert(sampled_unit == 1);
    assert(near(sampled_u, 0.5f));
    assert(near(sampled_v, 1.5f));
    assert(near(sampled_dudx, 0.02f));
    assert(near(sampled_dvdx, 0.04f));
    assert(near(sampled_dudy, 0.06f));
    assert(near(sampled_dvdy, 0.08f));
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

static void test_bounded_for_and_while_loops()
{
    static const char for_source[] =
        "#version 100\n"
        "void main() {\n"
        "  float accumulated = 0.0;\n"
        "  for (int i = 0; i < 4; i++) {\n"
        "    accumulated = accumulated + 0.25;\n"
        "  }\n"
        "  gl_FragColor = vec4(accumulated);\n"
        "}\n";

    ShaderModule for_module = {};
    const bool for_compiled =
        fxgl::compile_glsl(ShaderStage::Fragment, for_source, for_module);
    if (!for_compiled)
        std::fprintf(stderr, "for-loop compile diagnostic: %s\\n",
            for_module.info_log);
    assert(for_compiled);
    ShaderRuntime for_runtime = {};
    assert(fxgl::execute_glsl(for_module, for_runtime));
    assert(near(for_runtime.fragment_color.data[0], 1.0f));

    static const char while_source[] =
        "void main() {\n"
        "  int i = 0;\n"
        "  float accumulated = 0.0;\n"
        "  while (i < 3) {\n"
        "    accumulated = accumulated + 0.5;\n"
        "    i = i + 1;\n"
        "  }\n"
        "  gl_FragColor = vec4(accumulated);\n"
        "}\n";

    ShaderModule while_module = {};
    assert(fxgl::compile_glsl(ShaderStage::Fragment, while_source, while_module));
    ShaderRuntime while_runtime = {};
    assert(fxgl::execute_glsl(while_module, while_runtime));
    assert(near(while_runtime.fragment_color.data[0], 1.5f));
}

static void test_loop_break_continue_and_nested_targets()
{
    static const char for_source[] =
        "void main() {\n"
        "  int visited = 0;\n"
        "  float accumulated = 0.0;\n"
        "  for (int i = 0; i < 8; i++) {\n"
        "    if (i == 2) continue;\n"
        "    if (i == 6) break;\n"
        "    visited = visited + 1;\n"
        "    accumulated = accumulated + 0.25;\n"
        "  }\n"
        "  gl_FragColor = vec4(accumulated, float(visited), 0.0, 1.0);\n"
        "}\n";

    ShaderModule for_module = {};
    assert(fxgl::compile_glsl(ShaderStage::Fragment, for_source, for_module));
    ShaderRuntime for_runtime = {};
    assert(fxgl::execute_glsl(for_module, for_runtime));
    assert(near(for_runtime.fragment_color.data[0], 1.25f));
    assert(near(for_runtime.fragment_color.data[1], 5.0f));

    static const char while_source[] =
        "void main() {\n"
        "  int i = 0;\n"
        "  float accumulated = 0.0;\n"
        "  while (i < 8) {\n"
        "    i = i + 1;\n"
        "    if (i == 3) continue;\n"
        "    if (i == 6) break;\n"
        "    accumulated = accumulated + 0.25;\n"
        "  }\n"
        "  gl_FragColor = vec4(accumulated);\n"
        "}\n";

    ShaderModule while_module = {};
    assert(fxgl::compile_glsl(ShaderStage::Fragment, while_source, while_module));
    ShaderRuntime while_runtime = {};
    assert(fxgl::execute_glsl(while_module, while_runtime));
    assert(near(while_runtime.fragment_color.data[0], 1.0f));

    static const char nested_source[] =
        "void main() {\n"
        "  int outer_count = 0;\n"
        "  float accumulated = 0.0;\n"
        "  for (int i = 0; i < 3; i++) {\n"
        "    if (i == 1) continue;\n"
        "    int inner = 0;\n"
        "    while (inner < 3) {\n"
        "      inner = inner + 1;\n"
        "      if (inner == 2) break;\n"
        "      accumulated = accumulated + 0.5;\n"
        "    }\n"
        "    outer_count = outer_count + 1;\n"
        "  }\n"
        "  gl_FragColor = vec4(accumulated, float(outer_count), 0.0, 1.0);\n"
        "}\n";

    ShaderModule nested_module = {};
    assert(fxgl::compile_glsl(
        ShaderStage::Fragment, nested_source, nested_module));
    ShaderRuntime nested_runtime = {};
    assert(fxgl::execute_glsl(nested_module, nested_runtime));
    assert(near(nested_runtime.fragment_color.data[0], 1.0f));
    assert(near(nested_runtime.fragment_color.data[1], 2.0f));
}

static void test_user_defined_functions_and_value_returns()
{
    static const char source[] =
        "float square(float value) {\n"
        "  float result = value * value;\n"
        "  return result;\n"
        "}\n"
        "float signed_square(float value) {\n"
        "  if (value < 0.0) return -square(-value);\n"
        "  return square(value);\n"
        "}\n"
        "vec3 make_color(float red, float green) {\n"
        "  return vec3(red, green, red + green);\n"
        "}\n"
        "void write_color(vec4 color) {\n"
        "  if (color.a > 0.5) { gl_FragColor = color; return; }\n"
        "  gl_FragColor = vec4(0.0);\n"
        "}\n"
        "void main() {\n"
        "  vec3 rgb = make_color(0.25, signed_square(-3.0));\n"
        "  write_color(vec4(rgb, 1.0));\n"
        "}\n";

    ShaderModule module = {};
    const bool compiled =
        fxgl::compile_glsl(ShaderStage::Fragment, source, module);
    if (!compiled)
        std::fprintf(stderr, "user-function diagnostic: %s\n", module.info_log);
    assert(compiled);
    assert(module.has_main);
    assert(module.function_count == 5);
    assert(module.functions[0].parameter_count == 1);
    assert(module.functions[2].return_type == ShaderType::Vec3);

    ShaderRuntime runtime = {};
    assert(fxgl::execute_glsl(module, runtime));
    assert(near(runtime.fragment_color.data[0], 0.25f));
    assert(near(runtime.fragment_color.data[1], -9.0f));
    assert(near(runtime.fragment_color.data[2], -8.75f));
    assert(near(runtime.fragment_color.data[3], 1.0f));
}

static void test_user_function_prototypes_and_forward_definitions()
{
    // main() calls a function declared above it but defined after it. The
    // compiler must patch the function table and keep main from falling into
    // the later function's bytecode.
    static const char main_forward_source[] =
        "float scale_value(float);\n"
        "void main() {\n"
        "  gl_FragColor = vec4(scale_value(2.0), scale_value(-1.0), 0.0, 1.0);\n"
        "}\n"
        "float scale_value(float input) { return input * 2.5; }\n";

    ShaderModule main_forward_module = {};
    assert(fxgl::compile_glsl(
        ShaderStage::Fragment, main_forward_source, main_forward_module));
    assert(main_forward_module.function_count == 2);
    assert(main_forward_module.main_function == 1);
    assert(main_forward_module.functions[0].defined);
    assert(main_forward_module.functions[1].defined);
    ShaderRuntime main_forward_runtime = {};
    assert(fxgl::execute_glsl(main_forward_module, main_forward_runtime));
    assert(near(main_forward_runtime.fragment_color.data[0], 5.0f));
    assert(near(main_forward_runtime.fragment_color.data[1], -2.5f));
    assert(near(main_forward_runtime.fragment_color.data[3], 1.0f));

    // A helper may call a later-defined helper when its prototype was
    // declared first. Parameter identifiers may differ between prototype
    // and definition; the signature is based on parameter types.
    static const char nested_forward_source[] =
        "float deferred(float value);\n"
        "float caller(float argument) { return deferred(argument) + 1.0; }\n"
        "float deferred(float input) { return input * 3.0; }\n"
        "void main() { gl_FragColor = vec4(caller(2.0)); }\n";

    ShaderModule nested_forward_module = {};
    assert(fxgl::compile_glsl(
        ShaderStage::Fragment, nested_forward_source, nested_forward_module));
    assert(nested_forward_module.function_count == 3);
    ShaderRuntime nested_forward_runtime = {};
    assert(fxgl::execute_glsl(nested_forward_module, nested_forward_runtime));
    assert(near(nested_forward_runtime.fragment_color.data[0], 7.0f));
}

static void test_user_function_overloads_use_exact_argument_types()
{
    static const char source[] =
        "float measure(float value);\n"
        "vec2 measure(vec2 value);\n"
        "float measure(float input) { return input * 10.0; }\n"
        "vec2 measure(vec2 input) { return input * 2.0; }\n"
        "void main() {\n"
        "  float scalar_result = measure(1.5);\n"
        "  vec2 vector_result = measure(vec2(2.0, 3.0));\n"
        "  gl_FragColor = vec4(scalar_result, vector_result.x, vector_result.y, 1.0);\n"
        "}\n";

    ShaderModule module = {};
    const bool compiled =
        fxgl::compile_glsl(ShaderStage::Fragment, source, module);
    if (!compiled)
        std::fprintf(stderr, "overload diagnostic: %s\n", module.info_log);
    assert(compiled);
    assert(module.function_count == 3);
    ShaderRuntime runtime = {};
    assert(fxgl::execute_glsl(module, runtime));
    assert(near(runtime.fragment_color.data[0], 15.0f));
    assert(near(runtime.fragment_color.data[1], 4.0f));
    assert(near(runtime.fragment_color.data[2], 6.0f));
    assert(near(runtime.fragment_color.data[3], 1.0f));

    // Definitions may be encountered before prototypes; matching uses only
    // name and parameter types, not parameter identifiers or declaration order.
    static const char mixed_order_source[] =
        "float select_value(float x) { return x + 1.0; }\n"
        "vec2 select_value(vec2 x) { return x + vec2(1.0); }\n"
        "float select_value(float);\n"
        "vec2 select_value(vec2);\n"
        "void main() { gl_FragColor = vec4(select_value(2.0), select_value(vec2(3.0)).x, 0.0, 1.0); }\n";
    ShaderModule mixed_order_module = {};
    assert(fxgl::compile_glsl(
        ShaderStage::Fragment, mixed_order_source, mixed_order_module));
    ShaderRuntime mixed_order_runtime = {};
    assert(fxgl::execute_glsl(mixed_order_module, mixed_order_runtime));
    assert(near(mixed_order_runtime.fragment_color.data[0], 3.0f));
    assert(near(mixed_order_runtime.fragment_color.data[1], 4.0f));
}

static void test_user_function_overload_errors()
{
    const char *invalid_sources[] = {
        // The return type cannot distinguish two overloads.
        "float convert_value(float x); vec2 convert_value(float x); void main() { gl_FragColor = vec4(1.0); }",
        // Same signature may be declared repeatedly but defined only once.
        "float convert_value(float x) { return x; } float convert_value(float x) { return x + 1.0; } void main() { gl_FragColor = vec4(1.0); }",
        // No implicit int-to-float promotion is used for user overloads.
        "float convert_value(float x) { return x; } void main() { gl_FragColor = vec4(convert_value(1)); }",
        // Every overload needs a definition, even if a different overload is defined.
        "float convert_value(float x); vec2 convert_value(vec2 x) { return x; } void main() { gl_FragColor = vec4(1.0); }"
    };

    for (const char *source : invalid_sources)
    {
        ShaderModule module = {};
        assert(!fxgl::compile_glsl(ShaderStage::Fragment, source, module));
        assert(!module.compiled);
        assert(module.info_log[0] != '\0');
    }
}

static void test_user_function_out_and_inout_parameters()
{
    static const char split_source[] =
        "void split(float source, out float whole, out float fraction);\n"
        "void main() {\n"
        "  float whole = 0.0;\n"
        "  float fraction = 0.0;\n"
        "  split(3.75, (whole), fraction);\n"
        "  gl_FragColor = vec4(whole, fraction, 0.0, 1.0);\n"
        "}\n"
        "void split(float input_value, out float integer_part, out float fractional_part) {\n"
        "  integer_part = floor(input_value);\n"
        "  fractional_part = input_value - integer_part;\n"
        "}\n";

    ShaderModule split_module = {};
    const bool split_compiled =
        fxgl::compile_glsl(ShaderStage::Fragment, split_source, split_module);
    if (!split_compiled)
        std::fprintf(stderr, "out-parameter diagnostic: %s\n", split_module.info_log);
    assert(split_compiled);
    assert(split_module.functions[0].parameters[0].qualifier ==
           fxgl::ShaderParameterQualifier::In);
    assert(split_module.functions[0].parameters[1].qualifier ==
           fxgl::ShaderParameterQualifier::Out);
    assert(split_module.functions[0].parameters[2].qualifier ==
           fxgl::ShaderParameterQualifier::Out);
    ShaderRuntime split_runtime = {};
    assert(fxgl::execute_glsl(split_module, split_runtime));
    assert(near(split_runtime.fragment_color.data[0], 3.0f));
    assert(near(split_runtime.fragment_color.data[1], 0.75f));

    // An explicit in qualifier and the default qualifier are compatible
    // between prototype and definition. Values are copied into parameters.
    static const char inout_source[] =
        "void bump(in float amount, inout float current);\n"
        "void main() {\n"
        "  float state = 10.0;\n"
        "  bump(2.0, state);\n"
        "  gl_FragColor = vec4(state, 0.0, 0.0, 1.0);\n"
        "}\n"
        "void bump(float delta, inout float value) {\n"
        "  value = value + delta;\n"
        "}\n";

    ShaderModule inout_module = {};
    assert(fxgl::compile_glsl(
        ShaderStage::Fragment, inout_source, inout_module));
    ShaderRuntime inout_runtime = {};
    assert(fxgl::execute_glsl(inout_module, inout_runtime));
    assert(near(inout_runtime.fragment_color.data[0], 12.0f));

    // A non-void return value and output parameters must survive the same
    // return path: copy-outs update the caller while the result is pushed.
    static const char return_out_source[] =
        "float update(inout float state, out float previous);\n"
        "void main() {\n"
        "  float state = 2.0;\n"
        "  float previous = 0.0;\n"
        "  float result = update(state, previous);\n"
        "  gl_FragColor = vec4(result, state, previous, 1.0);\n"
        "}\n"
        "float update(inout float value, out float old_value) {\n"
        "  old_value = value;\n"
        "  value = value + 2.0;\n"
        "  return value * 3.0;\n"
        "}\n";

    ShaderModule return_out_module = {};
    assert(fxgl::compile_glsl(
        ShaderStage::Fragment, return_out_source, return_out_module));
    ShaderRuntime return_out_runtime = {};
    assert(fxgl::execute_glsl(return_out_module, return_out_runtime));
    assert(near(return_out_runtime.fragment_color.data[0], 12.0f));
    assert(near(return_out_runtime.fragment_color.data[1], 4.0f));
    assert(near(return_out_runtime.fragment_color.data[2], 2.0f));

    // Copy-out propagates through nested calls and runs on explicit returns.
    static const char nested_source[] =
        "void inner(out float doubled, inout float state);\n"
        "void outer(inout float state, out float total);\n"
        "void main() {\n"
        "  float state = 3.0;\n"
        "  float total = 0.0;\n"
        "  outer(state, total);\n"
        "  gl_FragColor = vec4(state, total, 0.0, 1.0);\n"
        "}\n"
        "void outer(inout float value, out float sum) {\n"
        "  float doubled = 0.0;\n"
        "  inner(doubled, value);\n"
        "  sum = doubled + value;\n"
        "  return;\n"
        "}\n"
        "void inner(out float twice, inout float value) {\n"
        "  twice = value * 2.0;\n"
        "  value = value + 1.0;\n"
        "  return;\n"
        "}\n";

    ShaderModule nested_module = {};
    const bool nested_compiled =
        fxgl::compile_glsl(ShaderStage::Fragment, nested_source, nested_module);
    if (!nested_compiled)
        std::fprintf(stderr, "nested out/inout diagnostic: %s\n",
                     nested_module.info_log);
    assert(nested_compiled);
    ShaderRuntime nested_runtime = {};
    assert(fxgl::execute_glsl(nested_module, nested_runtime));
    assert(near(nested_runtime.fragment_color.data[0], 4.0f));
    assert(near(nested_runtime.fragment_color.data[1], 10.0f));
}

static void test_user_function_out_inout_rejections()
{
    const char *invalid_sources[] = {
        "void store(out float value); void main() { store(1.0); gl_FragColor = vec4(1.0); } void store(out float value) { value = 2.0; }",
        "void store(out float value); void main() { float x = 1.0; store(x + 1.0); gl_FragColor = vec4(1.0); } void store(out float value) { value = 2.0; }",
        "uniform float u_value; void store(out float value); void main() { store(u_value); gl_FragColor = vec4(1.0); } void store(out float value) { value = 2.0; }",
        "void store(out vec2 value); void main() { vec3 color = vec3(1.0); store(color.xy); gl_FragColor = vec4(1.0); } void store(out vec2 value) { value = vec2(0.0); }",
        "void store(in float source, out float value); void main() { float x = 1.0; store(x, x); gl_FragColor = vec4(1.0); } void store(in float source, out float value) { value = source; }",
        "void edit(in float value) { value = 2.0; } void main() { edit(1.0); gl_FragColor = vec4(1.0); }",
        "void store(out float value); void relay(in float source) { store(source); } void store(out float value) { value = 2.0; } void main() { relay(1.0); gl_FragColor = vec4(1.0); }",
        "void store(out float value); void main() { float x = 0.0; store(x); gl_FragColor = vec4(1.0); } void store(inout float value) { value = 2.0; }",
        "void select_value(in float value); void select_value(out float value); void main() { gl_FragColor = vec4(1.0); }",
        "void store(out vec2 value); void main() { float x = 0.0; store(x); gl_FragColor = vec4(1.0); } void store(out vec2 value) { value = vec2(1.0); }"
    };

    for (const char *source : invalid_sources)
    {
        ShaderModule module = {};
        assert(!fxgl::compile_glsl(ShaderStage::Fragment, source, module));
        assert(!module.compiled);
        assert(module.info_log[0] != '\0');
    }
}

static void test_user_function_call_depth_is_bounded()
{
    static const char source[] =
        "float recurse(float value) {\n"
        "  return recurse(value + 1.0);\n"
        "}\n"
        "void main() { gl_FragColor = vec4(recurse(0.0)); }\n";

    ShaderModule module = {};
    assert(fxgl::compile_glsl(ShaderStage::Fragment, source, module));
    ShaderRuntime runtime = {};
    assert(!fxgl::execute_glsl(module, runtime));
}

static void test_non_terminating_shader_hits_execution_budget()
{
    static const char source[] =
        "void main() {\n"
        "  while (true) { }\n"
        "}\n";
    ShaderModule module = {};
    assert(fxgl::compile_glsl(ShaderStage::Fragment, source, module));
    ShaderRuntime runtime = {};
    assert(!fxgl::execute_glsl(module, runtime));
}

static void test_essl_math_and_geometric_builtins()
{
    static const char scalar_source[] =
        "void main() {\n"
        "  vec3 x = vec3(1.0, 0.0, 0.0);\n"
        "  vec3 y = vec3(0.0, 1.0, 0.0);\n"
        "  vec3 crossed = cross(x, y);\n"
        "  float distance_value = distance(x, y);\n"
        "  float power_value = pow(2.0, 3.0);\n"
        "  float smooth_value = smoothstep(0.0, 1.0, 0.5);\n"
        "  float fractional = fract(-1.25);\n"
        "  gl_FragColor = vec4(crossed.z, distance_value, power_value, smooth_value);\n"
        "}\n";

    ShaderModule module = {};
    assert(fxgl::compile_glsl(ShaderStage::Fragment, scalar_source, module));
    ShaderRuntime runtime = {};
    assert(fxgl::execute_glsl(module, runtime));
    assert(near(runtime.fragment_color.data[0], 1.0f));
    assert(near(runtime.fragment_color.data[1], std::sqrt(2.0f)));
    assert(near(runtime.fragment_color.data[2], 8.0f));
    assert(near(runtime.fragment_color.data[3], 0.5f));

    static const char vector_source[] =
        "void main() {\n"
        "  vec3 stepped = step(0.5, vec3(0.25, 0.5, 0.75));\n"
        "  vec3 reflected = reflect(vec3(1.0, -1.0, 0.0), vec3(0.0, 1.0, 0.0));\n"
        "  vec3 smooth = smoothstep(0.0, 1.0, vec3(0.25, 0.5, 0.75));\n"
        "  float angle = radians(180.0);\n"
        "  float back = degrees(angle);\n"
        "  gl_FragColor = vec4(stepped.x, stepped.y, smooth.z, back / 180.0);\n"
        "}\n";

    module = {};
    assert(fxgl::compile_glsl(ShaderStage::Fragment, vector_source, module));
    runtime = {};
    assert(fxgl::execute_glsl(module, runtime));
    assert(near(runtime.fragment_color.data[0], 0.0f));
    assert(near(runtime.fragment_color.data[1], 1.0f));
    assert(near(runtime.fragment_color.data[2], 0.84375f));
    assert(near(runtime.fragment_color.data[3], 1.0f));

    static const char unary_source[] =
        "void main() {\n"
        "  vec4 values = vec4(sin(0.0), cos(0.0), sqrt(4.0), inversesqrt(4.0));\n"
        "  gl_FragColor = values;\n"
        "}\n";
    module = {};
    assert(fxgl::compile_glsl(ShaderStage::Fragment, unary_source, module));
    runtime = {};
    assert(fxgl::execute_glsl(module, runtime));
    assert(near(runtime.fragment_color.data[0], 0.0f));
    assert(near(runtime.fragment_color.data[1], 1.0f));
    assert(near(runtime.fragment_color.data[2], 2.0f));
    assert(near(runtime.fragment_color.data[3], 0.5f));
}

static void test_matrix_component_multiply_and_scalar_broadcast()
{
    static const char source[] =
        "void main() {\n"
        "  mat2 a = mat2(2.0);\n"
        "  mat2 b = mat2(3.0);\n"
        "  mat2 c = matrixCompMult(a, b);\n"
        "  vec2 values = min(vec2(4.0, 2.0), 3.0);\n"
        "  vec2 diagonal = c * vec2(1.0, 1.0);\n"
        "  gl_FragColor = vec4(diagonal.x, diagonal.y, values.x, values.y);\n"
        "}\n";

    ShaderModule module = {};
    assert(fxgl::compile_glsl(ShaderStage::Fragment, source, module));
    ShaderRuntime runtime = {};
    assert(fxgl::execute_glsl(module, runtime));
    assert(near(runtime.fragment_color.data[0], 6.0f));
    assert(near(runtime.fragment_color.data[1], 6.0f));
    assert(near(runtime.fragment_color.data[2], 3.0f));
    assert(near(runtime.fragment_color.data[3], 2.0f));
}

static void test_ivec_bvec_constructors_swizzles_and_vector_relations()
{
    static const char source[] =
        "#version 100\n"
        "void main() {\n"
        "  ivec3 integers = ivec3(1.9, 2.2, -3.8);\n"
        "  bvec3 flags = greaterThan(integers, ivec3(0));\n"
        "  bvec3 equal_flags = equal(flags, bvec3(false, true, false));\n"
        "  bool any_flag = any(flags);\n"
        "  bool all_flags = all(not(equal_flags));\n"
        "  bvec3 inverted = not(flags);\n"
        "  gl_FragColor = vec4(float(integers.y), float(any_flag), "
        "float(all_flags), float(inverted.x));\n"
        "}\n";

    ShaderModule module = {};
    assert(fxgl::compile_glsl(ShaderStage::Fragment, source, module));
    ShaderRuntime runtime = {};
    assert(fxgl::execute_glsl(module, runtime));
    assert(near(runtime.fragment_color.data[0], 2.0f));
    assert(near(runtime.fragment_color.data[1], 1.0f));
    assert(near(runtime.fragment_color.data[2], 0.0f));
    assert(near(runtime.fragment_color.data[3], 0.0f));

    static const char ordering_source[] =
        "void main() {\n"
        "  bvec2 lt = lessThan(vec2(1.0, 3.0), vec2(2.0, 2.0));\n"
        "  bvec2 le = lessThanEqual(ivec2(2, 4), ivec2(2, 3));\n"
        "  bvec2 gt = greaterThanEqual(vec2(3.0, 4.0), vec2(2.0, 4.0));\n"
        "  gl_FragColor = vec4(float(all(lt)), float(any(le)), "
        "float(all(gt)), 1.0);\n"
        "}\n";

    module = {};
    assert(fxgl::compile_glsl(ShaderStage::Fragment, ordering_source, module));
    runtime = {};
    assert(fxgl::execute_glsl(module, runtime));
    assert(near(runtime.fragment_color.data[0], 0.0f));
    assert(near(runtime.fragment_color.data[1], 1.0f));
    assert(near(runtime.fragment_color.data[2], 1.0f));
    assert(near(runtime.fragment_color.data[3], 1.0f));
}

static void test_compiler_rejects_unsupported_or_invalid_source()
{
    const char *unsupported[] = {
        "void main() { do { gl_FragColor = vec4(1.0); } while (false); }",
        "attribute vec4 a_position[4]; void main() { gl_Position = a_position[0]; }",
        "void main() { gl_FragColor = unknown_name; }",
        "void main() { gl_FragColor = vec4(1.0, 2.0); }",
        "void main() { gl_Position = vec4(1.0); }",
        "float missing(float value); void main() { gl_FragColor = vec4(1.0); }",
        "float helper(float value); float helper(float input) { return input; } float helper(float other) { return other; } void main() { gl_FragColor = vec4(1.0); }",
        "void main() { gl_FragColor = vec4(later(1.0)); } float later(float value) { return value; }",
        "float helper(float value) { return value; } void main() { gl_FragColor = vec4(helper(vec2(1.0))); }",
        "void main() { break; gl_FragColor = vec4(1.0); }",
        "void main() { continue; gl_FragColor = vec4(1.0); }"
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
static void test_reused_runtime_resets_locals_and_outputs()
{
    static const char source[] =
        "uniform vec4 u_color;\n"
        "varying vec4 v_color;\n"
        "void main() {\n"
        "  vec4 combined = u_color + v_color;\n"
        "  gl_FragColor = combined;\n"
        "}\n";

    ShaderModule module = {};
    assert(fxgl::compile_glsl(ShaderStage::Fragment, source, module));
    assert(module.local_count == 1);

    ShaderRuntime runtime = {};
    runtime.locals[0] = value(ShaderType::Vec4, 91.0f, 92.0f, 93.0f, 94.0f);
    runtime.position = value(ShaderType::Vec4, 81.0f, 82.0f, 83.0f, 84.0f);
    runtime.fragment_color = value(ShaderType::Vec4, 71.0f, 72.0f, 73.0f, 74.0f);
    runtime.locals[0].ddx[0] = 55.0f;
    runtime.locals[0].ddy[0] = 66.0f;

    fxgl::prepare_shader_runtime(module, runtime);
    assert(runtime.locals[0].type == ShaderType::Vec4);
    for (size_t i = 0; i < 4; ++i)
    {
        assert(runtime.locals[0].data[i] == 0.0f);
        assert(runtime.locals[0].ddx[i] == 0.0f);
        assert(runtime.locals[0].ddy[i] == 0.0f);
    }
    assert(runtime.position.type == ShaderType::Invalid);
    assert(runtime.fragment_color.type == ShaderType::Invalid);

    runtime.uniforms[0] = value(ShaderType::Vec4, 0.1f, 0.2f, 0.3f, 0.4f);
    runtime.varyings[0] = value(ShaderType::Vec4, 0.4f, 0.3f, 0.2f, 0.1f);
    assert(fxgl::execute_glsl(module, runtime));
    assert(near(runtime.fragment_color.data[0], 0.5f));
    assert(near(runtime.fragment_color.data[1], 0.5f));
    assert(near(runtime.fragment_color.data[2], 0.5f));
    assert(near(runtime.fragment_color.data[3], 0.5f));

    // Poison the prior invocation's local/output values, then prove the
    // preparation step removes them before new fragment inputs are executed.
    runtime.locals[0] = value(ShaderType::Vec4, 101.0f, 102.0f, 103.0f, 104.0f);
    runtime.fragment_color = value(ShaderType::Vec4, 111.0f, 112.0f, 113.0f, 114.0f);
    fxgl::prepare_shader_runtime(module, runtime);
    assert(runtime.locals[0].data[0] == 0.0f);
    assert(runtime.fragment_color.type == ShaderType::Invalid);

    runtime.uniforms[0] = value(ShaderType::Vec4, 0.2f, 0.1f, 0.4f, 0.3f);
    runtime.varyings[0] = value(ShaderType::Vec4, 0.3f, 0.4f, 0.1f, 0.2f);
    assert(fxgl::execute_glsl(module, runtime));
    assert(near(runtime.fragment_color.data[0], 0.5f));
    assert(near(runtime.fragment_color.data[1], 0.5f));
    assert(near(runtime.fragment_color.data[2], 0.5f));
    assert(near(runtime.fragment_color.data[3], 0.5f));
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
    test_bounded_for_and_while_loops();
    test_loop_break_continue_and_nested_targets();
    test_user_defined_functions_and_value_returns();
    test_user_function_prototypes_and_forward_definitions();
    test_user_function_overloads_use_exact_argument_types();
    test_user_function_overload_errors();
    test_user_function_out_and_inout_parameters();
    test_user_function_out_inout_rejections();
    test_user_function_call_depth_is_bounded();
    test_non_terminating_shader_hits_execution_budget();
    test_essl_math_and_geometric_builtins();
    test_matrix_component_multiply_and_scalar_broadcast();
    test_ivec_bvec_constructors_swizzles_and_vector_relations();
    test_compiler_rejects_unsupported_or_invalid_source();
    test_instruction_budget_and_stack_safety();
    test_reused_runtime_resets_locals_and_outputs();

    std::puts("GLSL ES subset compiler/VM tests: PASS");
    return 0;
}
