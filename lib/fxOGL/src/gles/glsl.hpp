#pragma once

#include <stddef.h>
#include <stdint.h>

namespace fxgl
{

constexpr size_t GLSL_MAX_ATTRIBUTES = 8;
constexpr size_t GLSL_MAX_UNIFORMS = 16;
constexpr size_t GLSL_MAX_VARYINGS = 8;
constexpr size_t GLSL_MAX_LOCALS = 8;
constexpr size_t GLSL_MAX_FUNCTIONS = 8;
constexpr size_t GLSL_MAX_PARAMETERS = 4;
constexpr size_t GLSL_MAX_CALL_DEPTH = 4;
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
    float ddx[4];
    float ddy[4];
};

enum class ShaderParameterQualifier : uint8_t
{
    In,
    Out,
    InOut
};

struct ShaderVariable
{
    char name[GLSL_NAME_BYTES];
    ShaderType type;
    uint8_t index;
    /* Used only for function parameters. In is the default qualifier. */
    ShaderParameterQualifier qualifier;
    /* Local and uniform arrays: element count, zero for non-arrays. An
       array of N elements occupies N consecutive slots starting at index;
       the slots after the first are recorded as unnamed element entries. */
    uint8_t array_size;
};

struct ShaderFunction
{
    char name[GLSL_NAME_BYTES];
    ShaderType return_type; /* Invalid denotes void. */
    ShaderVariable parameters[GLSL_MAX_PARAMETERS];
    ShaderVariable locals[GLSL_MAX_LOCALS];
    size_t parameter_count;
    size_t local_count;
    size_t entry_pc;
    bool defined;
    bool has_return;
};

enum class ShaderOp : uint8_t
{
    PushFloat,
    PushInt,
    LoadAttribute,
    LoadUniform,
    LoadVarying,
    LoadLocal,
    LoadPosition,
    LoadPointSize,
    LoadFragmentColor,
    StoreVarying,
    StoreLocal,
    StorePosition,
    StorePointSize,
    StoreFragmentColor,
    StoreSwizzle,
    LoadMatrixColumn,
    StoreMatrixColumn,
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
    Call,
    Return,
    Texture2D,
    /* Array opcodes are appended so earlier opcodes keep their values and
       the interpreter's existing dispatch layout (measured on hardware). */
    /* Pops an int index; index is the array's first slot, argument_count
       its element count. Out-of-range indices fail the invocation. */
    LoadLocalIndexed,
    /* Pops the value, then the int index pushed before it. */
    StoreLocalIndexed,
    /* Dynamic uniform-array read; operands as for LoadLocalIndexed. */
    LoadUniformIndexed
};

struct ShaderInstruction
{
    ShaderOp op;
    ShaderType type;
    uint8_t index;
    uint8_t argument_count;
    union
    {
        uint8_t swizzle[4];
        /* Call op: compact local-variable copy-out descriptors. */
        uint16_t output_targets[GLSL_MAX_PARAMETERS];
    };
    float literal;
};

struct ShaderModule
{
    ShaderStage stage;
    ShaderVariable attributes[GLSL_MAX_ATTRIBUTES];
    ShaderVariable uniforms[GLSL_MAX_UNIFORMS];
    ShaderVariable varyings[GLSL_MAX_VARYINGS];
    ShaderVariable locals[GLSL_MAX_LOCALS]; /* Main-function locals. */
    ShaderFunction functions[GLSL_MAX_FUNCTIONS];
    size_t attribute_count;
    size_t uniform_count;
    size_t varying_count;
    size_t local_count;
    size_t function_count;
    uint8_t main_function;
    bool has_main;
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
    float varying_ddx[GLSL_MAX_VARYINGS];
    float varying_ddy[GLSL_MAX_VARYINGS];
    ShaderValue position;
    ShaderValue point_size;
    ShaderValue fragment_color;
};

using TextureSampleFunction = bool (*)(
    int texture_unit,
    float u,
    float v,
    float dudx,
    float dvdx,
    float dudy,
    float dvdy,
    float rgba[4],
    void *user_data);

/* Compile the explicitly documented GLSL ES 1.00 subset. The bounded parser
   supports overloaded typed user functions with up to four parameters,
   prototypes and forward definitions, typed returns, bounded nested calls,
   and in/out/inout parameter copy semantics. User overload resolution
   requires an exact match for every argument type; parameter qualifiers and
   return type do not distinguish overloads, but declarations of the same
   parameter types must agree on return type and qualifiers. Function
   parameters are mutable local copies. out/inout actual arguments currently
   target writable local variables or non-overlapping vector swizzles thereof;
   global outputs are not supported as copy-out targets. Input/output aliases
   follow value-return semantics; overlapping output destinations are rejected.
   Built-in function dispatch remains on its existing fixed-signature path.
   The ?: conditional operator takes a bool condition and two operands of
   the same non-sampler type and evaluates only the selected operand.
   One-dimensional local arrays (`float a[4];`, no initializer) of any
   non-sampler type are supported. Elements are read and written with
   constant integer indices, including element swizzles, matrix columns and
   out/inout arguments, or with int index expressions for whole-element
   reads and writes. Dynamic indices are bounds-checked at run time and an
   out-of-range access fails the invocation. Each element uses one local
   slot. Uniform arrays (`uniform vec4 u[3];`) follow the same read rules
   and use one uniform slot per element; sampler arrays
   (`uniform sampler2D s[2];`) take constant indices only, as GLSL ES 1.00
   requires. Varying arrays (`varying vec2 v[2];`) are written per element
   in the vertex stage and read in the fragment stage, with constant
   indices only, also as GLSL ES 1.00 requires. Array function parameters
   (`float f(inout float a[3])`) take a whole local, uniform or varying
   array of the same element type and size by value, with copy-out for
   out/inout; each element counts toward the four-parameter limit.
   Whole-array assignment, structs, do-while, switch, preprocessor
   directives and precision qualifiers other than ordinary precision
   statements remain unsupported. GLSL ES 1.00 has no array return types
   and forbids attribute arrays. */
bool compile_glsl(
    ShaderStage stage,
    const char *source,
    ShaderModule& module);

/* Execute compiled shader bytecode without allocation. Vertex shaders write
   position/varyings; fragment shaders read interpolated varyings and write
   fragment_color. texture2D requires a caller-provided texture callback. */
/* Prepare a reused runtime for one invocation. Declared locals are reset
   to zero and stage outputs are invalidated; callers must refresh the
   invocation's attributes, uniforms and varyings before execute_glsl(). */
void prepare_shader_runtime(
    const ShaderModule& module,
    ShaderRuntime& runtime);

bool execute_glsl(
    const ShaderModule& module,
    ShaderRuntime& runtime,
    TextureSampleFunction texture_sample = nullptr,
    void *user_data = nullptr);

} // namespace fxgl
