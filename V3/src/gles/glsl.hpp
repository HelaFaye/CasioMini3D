#pragma once

#include <stddef.h>
#include <stdint.h>

namespace fxgl
{

constexpr size_t GLSL_MAX_ATTRIBUTES = 8;
constexpr size_t GLSL_MAX_UNIFORMS = 16;
constexpr size_t GLSL_MAX_VARYINGS = 8;
constexpr size_t GLSL_MAX_LOCALS = 8;
constexpr size_t GLSL_MAX_INSTRUCTIONS = 128;
constexpr size_t GLSL_MAX_EXECUTION_STEPS = 1024;
constexpr size_t GLSL_MAX_STACK = 32;
constexpr size_t GLSL_NAME_BYTES = 32;

enum class ShaderStage : uint8_t
{
    Vertex,
    Fragment
};

enum class ShaderType : uint8_t
{
    Invalid,
    Float,
    Int,
    Bool,
    Vec2,
    Vec3,
    Vec4,
    IVec2,
    IVec3,
    IVec4,
    BVec2,
    BVec3,
    BVec4,
    Mat2,
    Mat3,
    Mat4,
    Sampler2D
};

struct ShaderValue
{
    ShaderType type;
    float data[16];
};

struct ShaderVariable
{
    char name[GLSL_NAME_BYTES];
    ShaderType type;
    uint8_t index;
};

enum class ShaderOp : uint8_t
{
    PushFloat,
    PushInt,
    LoadAttribute,
    LoadUniform,
    LoadVarying,
    LoadLocal,
    StoreVarying,
    StoreLocal,
    StorePosition,
    StoreFragmentColor,
    Construct,
    Swizzle,
    Negate,
    Add,
    Subtract,
    Multiply,
    Divide,
    Dot,
    Length,
    Normalize,
    Mix,
    Clamp,
    Minimum,
    Maximum,
    Builtin,
    Equal,
    NotEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    LogicalAnd,
    LogicalOr,
    LogicalNot,
    BranchIfFalse,
    Jump,
    Texture2D
};

struct ShaderInstruction
{
    ShaderOp op;
    ShaderType type;
    uint8_t index;
    uint8_t argument_count;
    uint8_t swizzle[4];
    float literal;
};

struct ShaderModule
{
    ShaderStage stage;
    ShaderVariable attributes[GLSL_MAX_ATTRIBUTES];
    ShaderVariable uniforms[GLSL_MAX_UNIFORMS];
    ShaderVariable varyings[GLSL_MAX_VARYINGS];
    ShaderVariable locals[GLSL_MAX_LOCALS];
    size_t attribute_count;
    size_t uniform_count;
    size_t varying_count;
    size_t local_count;
    ShaderInstruction instructions[GLSL_MAX_INSTRUCTIONS];
    size_t instruction_count;
    bool compiled;
    char info_log[160];
};

struct ShaderRuntime
{
    ShaderValue attributes[GLSL_MAX_ATTRIBUTES];
    ShaderValue uniforms[GLSL_MAX_UNIFORMS];
    ShaderValue varyings[GLSL_MAX_VARYINGS];
    ShaderValue locals[GLSL_MAX_LOCALS];
    ShaderValue position;
    ShaderValue fragment_color;
};

using TextureSampleFunction = bool (*)(
    int texture_unit,
    float u,
    float v,
    float rgba[4],
    void *user_data);

/* Compile the explicitly documented GLSL ES 1.00 subset. Arrays, structs,
   user-defined functions, control flow, preprocessor directives and precision
   qualifiers other than ordinary precision statements are unsupported and
   produce a diagnostic in module.info_log. */
bool compile_glsl(
    ShaderStage stage,
    const char *source,
    ShaderModule& module);

/* Execute compiled shader bytecode without allocation. Vertex shaders write
   position/varyings; fragment shaders read interpolated varyings and write
   fragment_color. texture2D requires a caller-provided texture callback. */
bool execute_glsl(
    const ShaderModule& module,
    ShaderRuntime& runtime,
    TextureSampleFunction texture_sample = nullptr,
    void *user_data = nullptr);

} // namespace fxgl
