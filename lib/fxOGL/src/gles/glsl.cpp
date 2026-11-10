#include "glsl.hpp"

#include <algorithm>
#include <math.h>
#include <cstdio>
#include <stdlib.h>
#include <cstring>

namespace fxgl
{
namespace
{
enum class TokenKind : uint8_t
{
    End,
    Identifier,
    Number,
    Punctuation,
    Invalid
};

struct Token
{
    TokenKind kind;
    char text[GLSL_NAME_BYTES];
    float number;
    bool integer;
    size_t offset;
};

class Lexer
{
public:
    explicit Lexer(const char *source) : source_(source ? source : ""), offset_(0) {}

    Token next()
    {
        skip_space_and_comments();
        Token token = {};
        token.offset = offset_;

        const char c = source_[offset_];
        if (c == '\0')
        {
            token.kind = TokenKind::End;
            return token;
        }

        if (is_identifier_start(c))
        {
            const size_t start = offset_++;
            while (is_identifier_part(source_[offset_]))
                ++offset_;
            const size_t length = offset_ - start;
            if (length >= sizeof(token.text))
            {
                token.kind = TokenKind::Invalid;
                copy_error_text(token.text, sizeof(token.text), "identifier too long");
                return token;
            }
            std::memcpy(token.text, source_ + start, length);
            token.text[length] = '\0';
            token.kind = TokenKind::Identifier;
            return token;
        }

        if ((c >= '0' && c <= '9') ||
            (c == '.' && source_[offset_ + 1] >= '0' &&
             source_[offset_ + 1] <= '9'))
        {
            const size_t start = offset_;
            bool has_dot = false;
            bool has_exponent = false;
            if (source_[offset_] == '.')
            {
                has_dot = true;
                ++offset_;
            }
            while (source_[offset_] >= '0' && source_[offset_] <= '9')
                ++offset_;
            if (source_[offset_] == '.' && !has_dot)
            {
                has_dot = true;
                ++offset_;
                while (source_[offset_] >= '0' && source_[offset_] <= '9')
                    ++offset_;
            }
            if (source_[offset_] == 'e' || source_[offset_] == 'E')
            {
                has_exponent = true;
                ++offset_;
                if (source_[offset_] == '+' || source_[offset_] == '-')
                    ++offset_;
                const size_t exponent_start = offset_;
                while (source_[offset_] >= '0' && source_[offset_] <= '9')
                    ++offset_;
                if (offset_ == exponent_start)
                {
                    token.kind = TokenKind::Invalid;
                    copy_error_text(token.text, sizeof(token.text), "invalid exponent");
                    return token;
                }
            }

            const size_t length = offset_ - start;
            if (length >= sizeof(token.text))
            {
                token.kind = TokenKind::Invalid;
                copy_error_text(token.text, sizeof(token.text), "number too long");
                return token;
            }
            std::memcpy(token.text, source_ + start, length);
            token.text[length] = '\0';
            char *end = nullptr;
            token.number = ::strtof(token.text, &end);
            if (!end || *end != '\0' || !isfinite(token.number))
            {
                token.kind = TokenKind::Invalid;
                copy_error_text(token.text, sizeof(token.text), "invalid number");
                return token;
            }
            token.integer = !has_dot && !has_exponent;
            token.kind = TokenKind::Number;
            return token;
        }

        if (std::strchr("=!<>&|+-", c))
        {
            const char next = source_[offset_ + 1];
            const bool pair =
                ((c == '=' || c == '!' || c == '<' || c == '>') && next == '=') ||
                ((c == '&' || c == '|') && next == c) ||
                ((c == '+' || c == '-') && next == c);
            token.kind = TokenKind::Punctuation;
            token.text[0] = c;
            token.text[1] = pair ? next : '\0';
            token.text[pair ? 2 : 1] = '\0';
            offset_ += pair ? 2 : 1;
            return token;
        }

        if (std::strchr("{}();,[]=.+-*/?:", c))
        {
            token.kind = TokenKind::Punctuation;
            token.text[0] = c;
            token.text[1] = '\0';
            ++offset_;
            return token;
        }

        token.kind = TokenKind::Invalid;
        std::snprintf(token.text, sizeof(token.text),
                      "unexpected character '%c'", c);
        ++offset_;
        return token;
    }

private:
    static bool is_identifier_start(char c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    }

    static bool is_identifier_part(char c)
    {
        return is_identifier_start(c) || (c >= '0' && c <= '9');
    }

    static void copy_error_text(char *destination, size_t size, const char *source)
    {
        if (size == 0) return;
        std::snprintf(destination, size, "%s", source);
    }

    void skip_space_and_comments()
    {
        for (;;)
        {
            while (source_[offset_] == ' ' || source_[offset_] == '\t' ||
                   source_[offset_] == '\r' || source_[offset_] == '\n')
                ++offset_;

            if (source_[offset_] == '/' && source_[offset_ + 1] == '/')
            {
                offset_ += 2;
                while (source_[offset_] != '\0' && source_[offset_] != '\n')
                    ++offset_;
                continue;
            }

            if (source_[offset_] == '/' && source_[offset_ + 1] == '*')
            {
                offset_ += 2;
                while (source_[offset_] != '\0' &&
                       !(source_[offset_] == '*' && source_[offset_ + 1] == '/'))
                    ++offset_;
                if (source_[offset_] == '\0')
                    return;
                offset_ += 2;
                continue;
            }
            return;
        }
    }

    const char *source_;
    size_t offset_;
};

enum class Storage : uint8_t
{
    Attribute,
    Uniform,
    Varying,
    Local,
    Position,
    PointSize,
    FragmentColor
};

struct ResolvedVariable
{
    Storage storage;
    ShaderType type;
    uint8_t index;
    bool writable = false;
    bool is_swizzle = false;
    uint8_t components[4] = {};
    /* Non-zero for an unindexed local array; type is the element type. */
    uint8_t array_size = 0;
};

static constexpr uint16_t CALL_OUTPUT_TARGET_FULL = 0x8000;
static constexpr uint16_t CALL_OUTPUT_TARGET_NONE = 0xffff;
static constexpr uint16_t CALL_OUTPUT_TARGET_STORAGE_SHIFT = 11;
static constexpr uint16_t CALL_OUTPUT_TARGET_STORAGE_MASK = 0x7800;

enum class CallOutputStorage : uint8_t
{
    Local,
    Varying,
    Position,
    PointSize,
    FragmentColor
};

enum class BuiltinFunction : uint8_t
{
    Sin, Cos, Tan, Asin, Acos, Atan, Exp, Log, Exp2, Log2,
    Sqrt, InverseSqrt, Abs, Sign, Floor, Ceil, Fract, Radians, Degrees,
    Atan2, Pow, Mod, Step, Distance, Cross, FaceForward, Reflect,
    Refract, SmoothStep, MatrixCompMult,
    LessThan, LessThanEqual, GreaterThan, GreaterThanEqual,
    EqualVector, NotEqualVector, Any, All, NotVector
};

static size_t component_count(ShaderType type)
{
    switch (type)
    {
        case ShaderType::Float:
        case ShaderType::Int:
        case ShaderType::Bool:
        case ShaderType::Sampler2D:
            return 1;
        case ShaderType::Vec2:
            return 2;
        case ShaderType::Vec3:
            return 3;
        case ShaderType::Vec4:
        case ShaderType::IVec4:
        case ShaderType::BVec4:
            return 4;
        case ShaderType::IVec2:
        case ShaderType::BVec2:
            return 2;
        case ShaderType::IVec3:
        case ShaderType::BVec3:
            return 3;
        case ShaderType::Mat2:
            return 4;
        case ShaderType::Mat3:
            return 9;
        case ShaderType::Mat4:
            return 16;
        default:
            return 0;
    }
}

static ShaderType vector_type(size_t components)
{
    switch (components)
    {
        case 1: return ShaderType::Float;
        case 2: return ShaderType::Vec2;
        case 3: return ShaderType::Vec3;
        case 4: return ShaderType::Vec4;
        default: return ShaderType::Invalid;
    }
}

static bool is_vector(ShaderType type)
{
    return type == ShaderType::Vec2 || type == ShaderType::Vec3 ||
           type == ShaderType::Vec4;
}

static bool is_int_vector(ShaderType type)
{
    return type == ShaderType::IVec2 || type == ShaderType::IVec3 ||
           type == ShaderType::IVec4;
}

static bool is_bool_vector(ShaderType type)
{
    return type == ShaderType::BVec2 || type == ShaderType::BVec3 ||
           type == ShaderType::BVec4;
}

static bool is_any_vector(ShaderType type)
{
    return is_vector(type) || is_int_vector(type) || is_bool_vector(type);
}

static size_t vector_dimension(ShaderType type)
{
    switch (type)
    {
        case ShaderType::Vec2:
        case ShaderType::IVec2:
        case ShaderType::BVec2: return 2;
        case ShaderType::Vec3:
        case ShaderType::IVec3:
        case ShaderType::BVec3: return 3;
        case ShaderType::Vec4:
        case ShaderType::IVec4:
        case ShaderType::BVec4: return 4;
        default: return 0;
    }
}

static ShaderType vector_type_for(size_t components, ShaderType base)
{
    if (components == 1)
        return base;
    if (base == ShaderType::Float)
        return vector_type(components);
    if (base == ShaderType::Int)
    {
        switch (components)
        {
            case 2: return ShaderType::IVec2;
            case 3: return ShaderType::IVec3;
            case 4: return ShaderType::IVec4;
            default: return ShaderType::Invalid;
        }
    }
    if (base == ShaderType::Bool)
    {
        switch (components)
        {
            case 2: return ShaderType::BVec2;
            case 3: return ShaderType::BVec3;
            case 4: return ShaderType::BVec4;
            default: return ShaderType::Invalid;
        }
    }
    return ShaderType::Invalid;
}

static ShaderType vector_base_type(ShaderType type)
{
    if (is_vector(type)) return ShaderType::Float;
    if (is_int_vector(type)) return ShaderType::Int;
    if (is_bool_vector(type)) return ShaderType::Bool;
    return ShaderType::Invalid;
}

static bool is_gen_type(ShaderType type)
{
    return type == ShaderType::Float || is_vector(type);
}

static size_t matrix_dimension(ShaderType type)
{
    switch (type)
    {
        case ShaderType::Mat2: return 2;
        case ShaderType::Mat3: return 3;
        case ShaderType::Mat4: return 4;
        default: return 0;
    }
}

static bool is_matrix(ShaderType type)
{
    return matrix_dimension(type) != 0;
}

static bool is_numeric(ShaderType type)
{
    return type == ShaderType::Float || type == ShaderType::Int ||
           is_vector(type) || is_int_vector(type) || is_matrix(type);
}

class Parser
{
public:
    Parser(ShaderStage stage, const char *source, ShaderModule& module)
        : stage_(stage), lexer_(source), module_(module), current_(),
          failed_(false), recursion_depth_(0), active_function_(NO_ACTIVE_FUNCTION),
          main_terminated_(false), expression_lvalue_{}, loop_stack_{},
          jump_patches_{}, loop_depth_(0), jump_patch_count_(0)
    {
        advance();
    }

    bool parse()
    {
        module_.has_main = false;
        module_.function_count = 0;
        module_.local_count = 0;

        while (!failed_ && current_.kind != TokenKind::End)
        {
            if (is_identifier("precision"))
            {
                if (module_.has_main)
                {
                    fail("precision declarations must precede main()");
                    break;
                }
                parse_precision();
            }
            else if (is_identifier("attribute") ||
                     is_identifier("uniform") ||
                     is_identifier("varying"))
            {
                if (module_.has_main)
                {
                    fail("global declarations must precede main()");
                    break;
                }
                parse_global_declaration();
            }
            else
            {
                // main() may call a function whose prototype appeared earlier
                // and whose definition follows main(). Terminate main's bytecode
                // before emitting any later function body.
                if (module_.has_main && !main_terminated_)
                {
                    if (!emit_simple(ShaderOp::Return, ShaderType::Invalid))
                        break;
                    main_terminated_ = true;
                }
                parse_function_definition();
            }
        }

        if (!failed_ && !module_.has_main)
            fail("shader must define void main()");
        if (!failed_)
        {
            for (size_t i = 0; i < module_.function_count; ++i)
            {
                if (!module_.functions[i].defined)
                {
                    fail("function prototype has no definition");
                    break;
                }
            }
        }
        return !failed_;
    }

private:
    static constexpr size_t MAX_LOOP_NESTING = 8;

    struct LoopFrame
    {
        size_t patch_start;
    };

    struct LoopJumpPatch
    {
        size_t instruction;
        bool is_break;
    };

    void advance()
    {
        current_ = lexer_.next();
        // The lexer leaves a reason (e.g. "identifier too long") in the text.
        if (current_.kind == TokenKind::Invalid && !failed_)
            fail(current_.text[0] != '\0' ? current_.text : "invalid token");
    }

    bool is_identifier(const char *name) const
    {
        return current_.kind == TokenKind::Identifier &&
               std::strcmp(current_.text, name) == 0;
    }

    bool is_punctuation(char value) const
    {
        return current_.kind == TokenKind::Punctuation &&
               current_.text[0] == value;
    }

    bool accept_punctuation(char value)
    {
        if (!is_punctuation(value))
            return false;
        advance();
        return true;
    }

    bool expect_punctuation(char value)
    {
        if (accept_punctuation(value))
            return true;
        char message[64];
        std::snprintf(message, sizeof(message), "expected '%c'", value);
        fail(message);
        return false;
    }

    bool expect_identifier(const char *name)
    {
        if (!is_identifier(name))
        {
            char message[64];
            std::snprintf(message, sizeof(message), "expected %s", name);
            fail(message);
            return false;
        }
        advance();
        return true;
    }

    void fail(const char *message)
    {
        if (failed_)
            return;
        failed_ = true;
        std::snprintf(module_.info_log, sizeof(module_.info_log),
                      "GLSL ES subset: %s at byte %lu",
                      message, static_cast<unsigned long>(current_.offset));
    }

    ShaderType parse_type()
    {
        if (current_.kind != TokenKind::Identifier)
        {
            fail("expected a type name");
            return ShaderType::Invalid;
        }

        ShaderType type = ShaderType::Invalid;
        if (std::strcmp(current_.text, "float") == 0)
            type = ShaderType::Float;
        else if (std::strcmp(current_.text, "int") == 0)
            type = ShaderType::Int;
        else if (std::strcmp(current_.text, "bool") == 0)
            type = ShaderType::Bool;
        else if (std::strcmp(current_.text, "vec2") == 0)
            type = ShaderType::Vec2;
        else if (std::strcmp(current_.text, "vec3") == 0)
            type = ShaderType::Vec3;
        else if (std::strcmp(current_.text, "vec4") == 0)
            type = ShaderType::Vec4;
        else if (std::strcmp(current_.text, "ivec2") == 0)
            type = ShaderType::IVec2;
        else if (std::strcmp(current_.text, "ivec3") == 0)
            type = ShaderType::IVec3;
        else if (std::strcmp(current_.text, "ivec4") == 0)
            type = ShaderType::IVec4;
        else if (std::strcmp(current_.text, "bvec2") == 0)
            type = ShaderType::BVec2;
        else if (std::strcmp(current_.text, "bvec3") == 0)
            type = ShaderType::BVec3;
        else if (std::strcmp(current_.text, "bvec4") == 0)
            type = ShaderType::BVec4;
        else if (std::strcmp(current_.text, "mat2") == 0)
            type = ShaderType::Mat2;
        else if (std::strcmp(current_.text, "mat3") == 0)
            type = ShaderType::Mat3;
        else if (std::strcmp(current_.text, "mat4") == 0)
            type = ShaderType::Mat4;
        else if (std::strcmp(current_.text, "sampler2D") == 0)
            type = ShaderType::Sampler2D;

        if (type == ShaderType::Invalid)
        {
            fail("unsupported type");
            return type;
        }
        advance();
        return type;
    }

    void parse_precision()
    {
        advance(); /* precision */
        if (!is_identifier("lowp") && !is_identifier("mediump") &&
            !is_identifier("highp"))
        {
            fail("expected lowp, mediump or highp precision qualifier");
            return;
        }
        advance();
        const ShaderType type = parse_type();
        if (type != ShaderType::Float && type != ShaderType::Int &&
            type != ShaderType::Bool)
            fail("precision statements only support scalar types");
        expect_punctuation(';');
    }

    void parse_global_declaration()
    {
        Storage storage;
        if (is_identifier("attribute"))
            storage = Storage::Attribute;
        else if (is_identifier("uniform"))
            storage = Storage::Uniform;
        else if (is_identifier("varying"))
            storage = Storage::Varying;
        else
        {
            fail("expected attribute, uniform, varying or precision declaration");
            return;
        }
        advance();

        const ShaderType type = parse_type();
        if (failed_)
            return;

        if (current_.kind != TokenKind::Identifier)
        {
            fail("expected variable name");
            return;
        }
        char name[GLSL_NAME_BYTES];
        std::snprintf(name, sizeof(name), "%s", current_.text);
        advance();

        size_t array_size = 0;
        if (accept_punctuation('['))
        {
            if (storage != Storage::Uniform)
            {
                fail("only uniform and local arrays are supported");
                return;
            }
            if (current_.kind != TokenKind::Number || !current_.integer ||
                current_.number < 1.0f ||
                current_.number > static_cast<float>(GLSL_MAX_UNIFORMS))
            {
                fail("array size must be a positive integer constant");
                return;
            }
            array_size = static_cast<size_t>(current_.number);
            advance();
            if (!expect_punctuation(']'))
                return;
        }
        expect_punctuation(';');
        if (failed_)
            return;

        if (lookup(name).type != ShaderType::Invalid)
        {
            fail("duplicate variable declaration");
            return;
        }

        ShaderVariable *variables = nullptr;
        size_t *count = nullptr;
        size_t capacity = 0;

        if (storage == Storage::Attribute)
        {
            if (stage_ != ShaderStage::Vertex)
            {
                fail("attributes are only valid in vertex shaders");
                return;
            }
            variables = module_.attributes;
            count = &module_.attribute_count;
            capacity = GLSL_MAX_ATTRIBUTES;
        }
        else if (storage == Storage::Uniform)
        {
            variables = module_.uniforms;
            count = &module_.uniform_count;
            capacity = GLSL_MAX_UNIFORMS;
        }
        else
        {
            variables = module_.varyings;
            count = &module_.varying_count;
            capacity = GLSL_MAX_VARYINGS;
        }

        const size_t slots = array_size == 0 ? 1 : array_size;
        if (*count + slots > capacity)
        {
            fail("shader variable limit exceeded");
            return;
        }

        // Uniform arrays use consecutive slots like local arrays: the first
        // entry carries the name and size, later elements are unnamed.
        for (size_t slot = 0; slot < slots; ++slot)
        {
            ShaderVariable& variable = variables[*count];
            variable = {};
            if (slot == 0)
            {
                std::snprintf(variable.name, sizeof(variable.name), "%s", name);
                variable.array_size = static_cast<uint8_t>(array_size);
            }
            variable.type = type;
            variable.index = static_cast<uint8_t>(*count);
            ++(*count);
        }
    }

    ResolvedVariable lookup(const char *name) const
    {
        if (stage_ == ShaderStage::Vertex &&
            std::strcmp(name, "gl_Position") == 0)
            return {Storage::Position, ShaderType::Vec4, 0, true};
        if (stage_ == ShaderStage::Vertex &&
            std::strcmp(name, "gl_PointSize") == 0)
            return {Storage::PointSize, ShaderType::Float, 0, true};
        if (stage_ == ShaderStage::Fragment &&
            std::strcmp(name, "gl_FragColor") == 0)
            return {Storage::FragmentColor, ShaderType::Vec4, 0, true};

        if (active_function_ < module_.function_count)
        {
            const ShaderFunction& function =
                module_.functions[active_function_];
            const ShaderVariable *local = find_variable(
                function.locals, function.local_count, name);
            if (local)
            {
                ResolvedVariable resolved = {
                    Storage::Local, local->type, local->index, true};
                resolved.array_size = local->array_size;
                return resolved;
            }
        }

        const ShaderVariable *variable = find_variable(
            module_.attributes, module_.attribute_count, name);
        if (variable)
            return {Storage::Attribute, variable->type, variable->index};

        variable = find_variable(
            module_.uniforms, module_.uniform_count, name);
        if (variable)
        {
            ResolvedVariable resolved = {
                Storage::Uniform, variable->type, variable->index};
            resolved.array_size = variable->array_size;
            return resolved;
        }

        variable = find_variable(
            module_.varyings, module_.varying_count, name);
        if (variable)
            return {Storage::Varying, variable->type, variable->index,
                    stage_ == ShaderStage::Vertex};

        return {Storage::Local, ShaderType::Invalid, 0};
    }

    static const ShaderVariable *find_variable(
        const ShaderVariable *variables,
        size_t count,
        const char *name)
    {
        for (size_t i = 0; i < count; ++i)
            if (std::strcmp(variables[i].name, name) == 0)
                return &variables[i];
        return nullptr;
    }

    bool emit(const ShaderInstruction& instruction)
    {
        if (module_.instruction_count >= GLSL_MAX_INSTRUCTIONS)
        {
            fail("instruction budget exceeded");
            return false;
        }
        module_.instructions[module_.instruction_count++] = instruction;
        return true;
    }

    bool emit_simple(ShaderOp op, ShaderType type, uint8_t index = 0,
                     uint8_t argument_count = 0)
    {
        ShaderInstruction instruction = {};
        instruction.op = op;
        instruction.type = type;
        instruction.index = index;
        instruction.argument_count = argument_count;
        return emit(instruction);
    }

    int find_function(
        const char *name,
        const ShaderVariable *parameters,
        size_t parameter_count) const
    {
        for (size_t i = 0; i < module_.function_count; ++i)
        {
            const ShaderFunction& function = module_.functions[i];
            if (std::strcmp(function.name, name) != 0 ||
                function.parameter_count != parameter_count)
                continue;

            bool matches = true;
            for (size_t parameter = 0; parameter < parameter_count; ++parameter)
            {
                if (function.parameters[parameter].type !=
                        parameters[parameter].type ||
                    function.parameters[parameter].array_size !=
                        parameters[parameter].array_size)
                {
                    matches = false;
                    break;
                }
            }
            if (matches)
                return static_cast<int>(i);
        }
        return -1;
    }

    /* argument_arrays[i] is the element count of an array argument that
       starts at position i (its elements occupy positions i..i+n-1), else 0.
       Arrays must match array parameters of the same size exactly. */
    int find_function(
        const char *name,
        const ShaderType *argument_types,
        const uint8_t *argument_arrays,
        size_t argument_count) const
    {
        for (size_t i = 0; i < module_.function_count; ++i)
        {
            const ShaderFunction& function = module_.functions[i];
            if (std::strcmp(function.name, name) != 0 ||
                function.parameter_count != argument_count)
                continue;

            bool matches = true;
            for (size_t argument = 0; argument < argument_count; ++argument)
            {
                if (function.parameters[argument].type != argument_types[argument] ||
                    function.parameters[argument].array_size !=
                        argument_arrays[argument])
                {
                    matches = false;
                    break;
                }
            }
            if (matches)
                return static_cast<int>(i);
        }
        return -1;
    }

    bool has_function_name(const char *name) const
    {
        for (size_t i = 0; i < module_.function_count; ++i)
            if (std::strcmp(module_.functions[i].name, name) == 0)
                return true;
        return false;
    }

    bool add_function_local(
        ShaderFunction& function,
        const char *name,
        ShaderType type,
        size_t array_size = 0)
    {
        const size_t slots = array_size == 0 ? 1 : array_size;
        if (function.local_count + slots > GLSL_MAX_LOCALS)
        {
            fail("function local limit exceeded");
            return false;
        }
        for (size_t i = 0; i < function.local_count; ++i)
        {
            if (std::strcmp(function.locals[i].name, name) == 0)
            {
                fail("duplicate function local or parameter");
                return false;
            }
        }
        // Array elements after the first are unnamed entries so name lookup
        // finds only the array itself, while per-slot zeroing and out/inout
        // copy-out still see every element's type and slot index.
        for (size_t slot = 0; slot < slots; ++slot)
        {
            ShaderVariable& local = function.locals[function.local_count];
            local = {};
            if (slot == 0)
            {
                std::snprintf(local.name, sizeof(local.name), "%s", name);
                local.array_size = static_cast<uint8_t>(array_size);
            }
            local.type = type;
            local.index = static_cast<uint8_t>(function.local_count);
            ++function.local_count;
        }
        return true;
    }

    /* Parse `[index]` after an array name; the '[' is current. A lone
       integer literal yields a constant element l-value without emitting
       code. Any other int expression is emitted for a dynamic access. */
    bool parse_array_index(
        const ResolvedVariable& array,
        ResolvedVariable& element,
        bool& dynamic)
    {
        if (!expect_punctuation('['))
            return false;
        element = {};
        element.storage = array.storage;
        element.type = array.type;
        element.index = array.index;
        dynamic = false;
        if (is_punctuation('-'))
        {
            Lexer lookahead = lexer_;
            const Token operand = lookahead.next();
            if (operand.kind == TokenKind::Number && operand.integer &&
                operand.number > 0.0f)
            {
                fail("array index is out of range");
                return false;
            }
        }
        if (current_.kind == TokenKind::Number && current_.integer &&
            peek_punctuation(']'))
        {
            if (current_.number < 0.0f || current_.number >= array.array_size)
            {
                fail("array index is out of range");
                return false;
            }
            element.index = static_cast<uint8_t>(
                array.index + static_cast<size_t>(current_.number));
            element.writable = array.writable;
            advance();
        }
        else
        {
            // GLSL ES 1.00 only allows constant indices into sampler arrays.
            if (array.type == ShaderType::Sampler2D)
            {
                fail("sampler arrays require a constant integer index");
                return false;
            }
            if (parse_expression(1) != ShaderType::Int)
            {
                fail("array index must be an int expression");
                return false;
            }
            dynamic = true;
        }
        expression_lvalue_ = {};
        return expect_punctuation(']') && !failed_;
    }

    void parse_function_definition()
    {
        ShaderType return_type = ShaderType::Invalid;
        if (is_identifier("void"))
            advance();
        else
            return_type = parse_type();
        if (failed_)
            return;

        if (current_.kind != TokenKind::Identifier)
        {
            fail("expected function name");
            return;
        }
        char name[GLSL_NAME_BYTES] = {};
        std::snprintf(name, sizeof(name), "%s", current_.text);
        advance();
        expect_punctuation('(');
        if (failed_)
            return;

        ShaderVariable parameters[GLSL_MAX_PARAMETERS] = {};
        size_t parameter_count = 0;
        if (!accept_punctuation(')'))
        {
            for (;;)
            {
                if (parameter_count >= GLSL_MAX_PARAMETERS)
                {
                    fail("function parameter limit exceeded");
                    return;
                }
                ShaderParameterQualifier qualifier =
                    ShaderParameterQualifier::In;
                if (is_identifier("in"))
                    advance();
                else if (is_identifier("out"))
                {
                    qualifier = ShaderParameterQualifier::Out;
                    advance();
                }
                else if (is_identifier("inout"))
                {
                    qualifier = ShaderParameterQualifier::InOut;
                    advance();
                }

                const ShaderType parameter_type = parse_type();
                if (failed_)
                    return;

                ShaderVariable& parameter = parameters[parameter_count];
                parameter.type = parameter_type;
                parameter.index = static_cast<uint8_t>(parameter_count);
                parameter.qualifier = qualifier;
                if (current_.kind == TokenKind::Identifier)
                {
                    std::snprintf(parameter.name, sizeof(parameter.name), "%s",
                                  current_.text);
                    advance();
                    for (size_t i = 0; i < parameter_count; ++i)
                    {
                        if (parameters[i].name[0] &&
                            std::strcmp(parameters[i].name, parameter.name) == 0)
                        {
                            fail("duplicate function parameter name");
                            return;
                        }
                    }
                }
                else if (!is_punctuation(',') && !is_punctuation(')'))
                {
                    fail("expected parameter name, ',' or ')'");
                    return;
                }

                size_t parameter_slots = 1;
                if (accept_punctuation('['))
                {
                    // An array parameter expands to one VM parameter per
                    // element in consecutive slots, laid out like a local
                    // array: the first carries the name and size.
                    if (parameter_type == ShaderType::Sampler2D)
                    {
                        fail("sampler arrays are not supported");
                        return;
                    }
                    if (current_.kind != TokenKind::Number || !current_.integer ||
                        current_.number < 1.0f ||
                        current_.number > static_cast<float>(GLSL_MAX_PARAMETERS))
                    {
                        fail("array parameter size must be a positive integer constant");
                        return;
                    }
                    parameter_slots = static_cast<size_t>(current_.number);
                    advance();
                    if (!expect_punctuation(']'))
                        return;
                    if (parameter_count + parameter_slots > GLSL_MAX_PARAMETERS)
                    {
                        fail("function parameter limit exceeded (array elements count individually)");
                        return;
                    }
                    parameter.array_size = static_cast<uint8_t>(parameter_slots);
                    for (size_t element = 1; element < parameter_slots; ++element)
                    {
                        ShaderVariable& slot = parameters[parameter_count + element];
                        slot = {};
                        slot.type = parameter_type;
                        slot.index = static_cast<uint8_t>(parameter_count + element);
                        slot.qualifier = qualifier;
                    }
                }
                parameter_count += parameter_slots;
                if (accept_punctuation(')'))
                    break;
                if (!expect_punctuation(','))
                    return;
            }
        }

        const bool is_main = std::strcmp(name, "main") == 0;
        if (is_main &&
            (return_type != ShaderType::Invalid || parameter_count != 0))
        {
            fail("main must have signature void main()");
            return;
        }

        const bool is_prototype = accept_punctuation(';');
        if (is_main && is_prototype)
        {
            fail("main must be defined, not prototyped");
            return;
        }
        if (!is_prototype && !is_punctuation('{'))
        {
            fail("expected ';' or a function body");
            return;
        }

        // GLSL ES overload identity is the function name plus the exact
        // parameter-type sequence. Return type does not participate.
        int existing_index = find_function(name, parameters, parameter_count);
        size_t function_index = 0;
        if (existing_index >= 0)
        {
            function_index = static_cast<size_t>(existing_index);
            ShaderFunction& existing = module_.functions[function_index];
            bool matches = existing.return_type == return_type &&
                           existing.parameter_count == parameter_count;
            for (size_t i = 0; matches && i < parameter_count; ++i)
                matches = existing.parameters[i].type == parameters[i].type &&
                    existing.parameters[i].qualifier == parameters[i].qualifier;
            if (!matches)
            {
                fail("function declaration conflicts with an existing signature");
                return;
            }
            if (is_prototype)
                return;
            if (existing.defined)
            {
                fail("function redefinition is unsupported");
                return;
            }
        }
        else
        {
            if (module_.function_count >= GLSL_MAX_FUNCTIONS)
            {
                fail("function count limit exceeded");
                return;
            }
            function_index = module_.function_count++;
            ShaderFunction& function = module_.functions[function_index];
            std::snprintf(function.name, sizeof(function.name), "%s", name);
            function.return_type = return_type;
            function.parameter_count = parameter_count;
            for (size_t i = 0; i < parameter_count; ++i)
                function.parameters[i] = parameters[i];
            if (is_prototype)
                return;
        }

        ShaderFunction& function = module_.functions[function_index];
        function.return_type = return_type;
        function.parameter_count = parameter_count;
        function.local_count = 0;
        function.has_return = false;
        for (size_t i = 0; i < parameter_count; ++i)
            function.parameters[i] = parameters[i];
        for (size_t i = 0; i < parameter_count;)
        {
            if (parameters[i].name[0] == '\0')
            {
                fail("function definitions require parameter names");
                return;
            }
            if (!add_function_local(function, parameters[i].name,
                    parameters[i].type, parameters[i].array_size))
                return;
            i += parameters[i].array_size == 0 ? 1 : parameters[i].array_size;
        }

        function.entry_pc = module_.instruction_count;
        function.defined = true;
        active_function_ = function_index;
        advance(); /* opening brace */
        while (!failed_ && !is_punctuation('}') &&
               current_.kind != TokenKind::End)
            parse_statement();
        expect_punctuation('}');

        if (!failed_ && return_type != ShaderType::Invalid &&
            !function.has_return)
        {
            fail("non-void function must contain a return statement");
            return;
        }
        // main() at end-of-source retains its original implicit fallthrough
        // representation for the two-instruction fragment fast path. If
        // another function follows main(), parse() inserts an explicit
        // void Return before that function's bytecode.
        if (!failed_ && !is_main)
        {
            ShaderInstruction implicit_return = {};
            implicit_return.op = ShaderOp::Return;
            implicit_return.type = return_type;
            emit(implicit_return);
        }

        if (!failed_ && is_main)
        {
            module_.has_main = true;
            module_.main_function = static_cast<uint8_t>(function_index);
            module_.local_count = function.local_count;
            for (size_t i = 0; i < function.local_count; ++i)
                module_.locals[i] = function.locals[i];
        }
        active_function_ = NO_ACTIVE_FUNCTION;
    }

    bool peek_punctuation(char value) const
    {
        Lexer lookahead = lexer_;
        const Token token = lookahead.next();
        return token.kind == TokenKind::Punctuation && token.text[0] == value;
    }

    void parse_return_statement()
    {
        advance(); /* return */
        if (active_function_ >= module_.function_count)
        {
            fail("return is only valid inside a function");
            return;
        }

        ShaderFunction& function = module_.functions[active_function_];
        const ShaderType expected = function.return_type;
        const bool has_value = !is_punctuation(';');
        if (expected == ShaderType::Invalid)
        {
            if (has_value)
            {
                fail("void function cannot return a value");
                return;
            }
        }
        else
        {
            if (!has_value)
            {
                fail("non-void function must return a value");
                return;
            }
            const ShaderType actual = parse_expression(1);
            if (actual != expected)
            {
                fail("return expression type does not match function return type");
                return;
            }
        }

        expect_punctuation(';');
        if (!failed_)
        {
            function.has_return = true;
            emit_simple(ShaderOp::Return, expected, 0,
                        has_value ? 1 : 0);
        }
    }

    bool is_type_token() const
    {
        if (current_.kind != TokenKind::Identifier)
            return false;
        return std::strcmp(current_.text, "float") == 0 ||
               std::strcmp(current_.text, "int") == 0 ||
               std::strcmp(current_.text, "bool") == 0 ||
               std::strcmp(current_.text, "vec2") == 0 ||
               std::strcmp(current_.text, "vec3") == 0 ||
               std::strcmp(current_.text, "vec4") == 0 ||
               std::strcmp(current_.text, "ivec2") == 0 ||
               std::strcmp(current_.text, "ivec3") == 0 ||
               std::strcmp(current_.text, "ivec4") == 0 ||
               std::strcmp(current_.text, "bvec2") == 0 ||
               std::strcmp(current_.text, "bvec3") == 0 ||
               std::strcmp(current_.text, "bvec4") == 0 ||
               std::strcmp(current_.text, "mat2") == 0 ||
               std::strcmp(current_.text, "mat3") == 0 ||
               std::strcmp(current_.text, "mat4") == 0 ||
               std::strcmp(current_.text, "sampler2D") == 0;
    }

    void parse_nested_statement()
    {
        if (accept_punctuation('{'))
        {
            while (!failed_ && current_.kind != TokenKind::End &&
                   !is_punctuation('}'))
                parse_statement();
            expect_punctuation('}');
        }
        else
        {
            parse_statement();
        }
    }

    void patch_branch_target(size_t instruction_index, size_t target)
    {
        if (instruction_index >= module_.instruction_count ||
            target > module_.instruction_count || target > 255)
        {
            fail("control-flow target exceeds bytecode bounds");
            return;
        }
        module_.instructions[instruction_index].index =
            static_cast<uint8_t>(target);
    }

    bool begin_loop()
    {
        if (loop_depth_ >= MAX_LOOP_NESTING)
        {
            fail("loop nesting limit exceeded");
            return false;
        }
        loop_stack_[loop_depth_++].patch_start = jump_patch_count_;
        return true;
    }

    void finish_loop(size_t continue_target, size_t break_target)
    {
        if (loop_depth_ == 0)
        {
            fail("internal loop stack underflow");
            return;
        }

        const LoopFrame frame = loop_stack_[--loop_depth_];
        for (size_t i = frame.patch_start; i < jump_patch_count_; ++i)
        {
            const LoopJumpPatch& patch = jump_patches_[i];
            patch_branch_target(
                patch.instruction,
                patch.is_break ? break_target : continue_target);
            if (failed_)
                return;
        }

        // All jumps belonging to this loop have been resolved. Reuse their
        // bounded patch slots for subsequent and enclosing loop bodies.
        jump_patch_count_ = frame.patch_start;
    }

    void emit_loop_jump(bool is_break)
    {
        if (loop_depth_ == 0)
        {
            fail(is_break ? "break is only valid inside a loop"
                          : "continue is only valid inside a loop");
            return;
        }
        if (jump_patch_count_ >= GLSL_MAX_INSTRUCTIONS)
        {
            fail("loop jump patch budget exceeded");
            return;
        }

        const size_t instruction = module_.instruction_count;
        if (emit_simple(ShaderOp::Jump, ShaderType::Invalid))
            jump_patches_[jump_patch_count_++] = {instruction, is_break};
    }

    void parse_if_statement()
    {
        advance(); /* if */
        expect_punctuation('(');
        const ShaderType condition_type = parse_expression(1);
        expect_punctuation(')');
        if (condition_type != ShaderType::Bool)
        {
            fail("if condition must be bool");
            return;
        }

        const size_t branch_index = module_.instruction_count;
        emit_simple(ShaderOp::BranchIfFalse, ShaderType::Bool);
        parse_nested_statement();
        if (failed_)
            return;

        if (is_identifier("else"))
        {
            const size_t end_jump_index = module_.instruction_count;
            emit_simple(ShaderOp::Jump, ShaderType::Invalid);
            patch_branch_target(branch_index, module_.instruction_count);
            advance(); /* else */
            parse_nested_statement();
            patch_branch_target(end_jump_index, module_.instruction_count);
        }
        else
        {
            patch_branch_target(branch_index, module_.instruction_count);
        }
    }

    bool emit_assignment_store(const ResolvedVariable& target)
    {
        switch (target.storage)
        {
            case Storage::Varying:
                return emit_simple(ShaderOp::StoreVarying, target.type, target.index);
            case Storage::Local:
                return emit_simple(ShaderOp::StoreLocal, target.type, target.index);
            case Storage::Position:
                return emit_simple(ShaderOp::StorePosition, target.type);
            case Storage::PointSize:
                return emit_simple(ShaderOp::StorePointSize, target.type);
            case Storage::FragmentColor:
                return emit_simple(ShaderOp::StoreFragmentColor, target.type);
            default:
                fail("unsupported assignment storage");
                return false;
        }
    }

    void parse_assignment_statement(bool consume_semicolon)
    {
        if (current_.kind != TokenKind::Identifier)
        {
            fail("expected an assignment target");
            return;
        }

        char name[GLSL_NAME_BYTES];
        std::snprintf(name, sizeof(name), "%s", current_.text);
        advance();
        const ResolvedVariable base = lookup(name);
        if (base.type == ShaderType::Invalid)
        {
            fail("assignment target is not declared");
            return;
        }
        if ((base.storage == Storage::Local && !base.writable) ||
            base.storage == Storage::Attribute ||
            base.storage == Storage::Uniform ||
            (base.storage == Storage::FragmentColor && stage_ == ShaderStage::Vertex) ||
            (base.storage == Storage::Position && stage_ == ShaderStage::Fragment) ||
            (base.storage == Storage::PointSize && stage_ != ShaderStage::Vertex) ||
            (base.storage == Storage::Varying && stage_ == ShaderStage::Fragment))
        {
            fail("assignment target is read-only or unavailable in this shader stage");
            return;
        }

        ResolvedVariable target = base;
        if (base.array_size != 0)
        {
            if (!is_punctuation('['))
            {
                fail("arrays must be indexed; whole-array assignment is unsupported");
                return;
            }
            ResolvedVariable element;
            bool dynamic = false;
            if (!parse_array_index(base, element, dynamic))
                return;
            if (dynamic)
            {
                if (is_punctuation('.') || is_punctuation('['))
                {
                    fail("dynamically indexed array assignments must write whole elements");
                    return;
                }
                expect_punctuation('=');
                if (parse_expression(1) != base.type)
                {
                    fail("assignment type does not match target type");
                    return;
                }
                if (consume_semicolon)
                    expect_punctuation(';');
                emit_simple(ShaderOp::StoreLocalIndexed, base.type,
                            base.index, base.array_size);
                return;
            }
            // A constant element is an ordinary local slot from here on, so
            // swizzle and matrix-column writes follow the existing paths.
            target = element;
        }
        const ResolvedVariable element_base = target;
        bool matrix_column_assignment = false;
        uint8_t matrix_column = 0;
        if (accept_punctuation('['))
        {
            const size_t dimension = matrix_dimension(element_base.type);
            if (dimension == 0 || element_base.storage != Storage::Local ||
                !element_base.writable)
            {
                fail("matrix column assignment requires a writable local matrix");
                return;
            }
            if (current_.kind != TokenKind::Number || !current_.integer ||
                current_.number < 0.0f || current_.number >= dimension)
            {
                fail("matrix column index must be a constant in range");
                return;
            }
            matrix_column = static_cast<uint8_t>(current_.number);
            advance();
            expect_punctuation(']');
            matrix_column_assignment = true;
            target.type = vector_type(dimension);
            emit_simple(ShaderOp::LoadLocal, element_base.type, element_base.index);
        }
        const bool swizzle_assignment = !matrix_column_assignment && is_punctuation('.');
        if (swizzle_assignment)
        {
            if (!is_any_vector(element_base.type))
            {
                fail("swizzle assignment requires a vector target");
                return;
            }
            const size_t instruction_start = module_.instruction_count;
            ShaderType swizzle_type = element_base.type;
            expression_lvalue_ = element_base;
            parse_swizzle(swizzle_type);
            target = expression_lvalue_;
            module_.instruction_count = instruction_start;
            expression_lvalue_ = {};
            if (failed_)
                return;
            if (!target.is_swizzle || target.type != swizzle_type)
            {
                fail("swizzle assignment target is not writable");
                return;
            }
            for (size_t i = 0; i < component_count(swizzle_type); ++i)
            {
                for (size_t j = i + 1; j < component_count(swizzle_type); ++j)
                {
                    if (target.components[i] == target.components[j])
                    {
                        fail("swizzle assignment cannot write a component more than once");
                        return;
                    }
                }
            }
        }

        expect_punctuation('=');
        const ShaderType expression_type = parse_expression(1);
        if (expression_type != target.type)
        {
            fail("assignment type does not match target type");
            return;
        }
        if (consume_semicolon)
            expect_punctuation(';');

        if (matrix_column_assignment)
        {
            ShaderInstruction instruction = {};
            instruction.op = ShaderOp::StoreMatrixColumn;
            instruction.type = element_base.type;
            instruction.index = element_base.index;
            instruction.argument_count = matrix_column;
            emit(instruction);
            return;
        }

        if (!swizzle_assignment)
        {
            emit_assignment_store(element_base);
            return;
        }

        ShaderInstruction instruction = {};
        instruction.op = ShaderOp::StoreSwizzle;
        instruction.type = element_base.type;
        instruction.index = element_base.index;
        instruction.argument_count = static_cast<uint8_t>(component_count(target.type));
        instruction.literal = static_cast<float>(element_base.storage);
        for (size_t i = 0; i < instruction.argument_count; ++i)
            instruction.swizzle[i] = target.components[i];
        emit(instruction);
    }

    void parse_for_increment()
    {
        if (is_punctuation(')'))
            return;
        if (current_.kind != TokenKind::Identifier)
        {
            fail("for-loop increment must update a local variable");
            return;
        }

        char name[GLSL_NAME_BYTES];
        std::snprintf(name, sizeof(name), "%s", current_.text);
        advance();
        const ResolvedVariable target = lookup(name);
        if (target.storage != Storage::Local || !target.writable ||
            target.array_size != 0)
        {
            fail("for-loop increment target must be a writable local variable");
            return;
        }

        if (current_.kind == TokenKind::Punctuation &&
            (std::strcmp(current_.text, "++") == 0 ||
             std::strcmp(current_.text, "--") == 0))
        {
            const bool increment = std::strcmp(current_.text, "++") == 0;
            advance();
            if (target.type != ShaderType::Int && target.type != ShaderType::Float)
            {
                fail("increment/decrement requires an int or float local");
                return;
            }

            emit_simple(ShaderOp::LoadLocal, target.type, target.index);
            ShaderInstruction one = {};
            one.op = target.type == ShaderType::Int
                ? ShaderOp::PushInt : ShaderOp::PushFloat;
            one.type = target.type;
            one.literal = 1.0f;
            emit(one);
            emit_simple(increment ? ShaderOp::Add : ShaderOp::Subtract, target.type);
            emit_assignment_store(target);
            return;
        }

        expect_punctuation('=');
        const ShaderType expression_type = parse_expression(1);
        if (expression_type != target.type)
        {
            fail("for-loop increment type mismatch");
            return;
        }
        emit_assignment_store(target);
    }

    void parse_while_statement()
    {
        advance(); /* while */
        const size_t condition_start = module_.instruction_count;
        expect_punctuation('(');
        const ShaderType condition_type = parse_expression(1);
        expect_punctuation(')');
        if (condition_type != ShaderType::Bool)
        {
            fail("while condition must be bool");
            return;
        }

        const size_t branch_index = module_.instruction_count;
        emit_simple(ShaderOp::BranchIfFalse, ShaderType::Bool);
        if (!begin_loop())
            return;
        parse_nested_statement();
        if (failed_)
            return;

        const size_t jump_index = module_.instruction_count;
        emit_simple(ShaderOp::Jump, ShaderType::Invalid);
        patch_branch_target(jump_index, condition_start);
        patch_branch_target(branch_index, module_.instruction_count);
        finish_loop(condition_start, module_.instruction_count);
    }

    void parse_for_statement()
    {
        advance(); /* for */
        expect_punctuation('(');

        if (is_type_token())
            parse_local_declaration();
        else if (!is_punctuation(';'))
            parse_assignment_statement(true);
        else
            advance();

        const size_t condition_start = module_.instruction_count;
        ShaderType condition_type = ShaderType::Bool;
        if (is_punctuation(';'))
        {
            ShaderInstruction always_true = {};
            always_true.op = ShaderOp::PushInt;
            always_true.type = ShaderType::Bool;
            always_true.literal = 1.0f;
            emit(always_true);
            advance();
        }
        else
        {
            condition_type = parse_expression(1);
            expect_punctuation(';');
        }
        if (condition_type != ShaderType::Bool)
        {
            fail("for-loop condition must be bool");
            return;
        }

        const size_t branch_index = module_.instruction_count;
        emit_simple(ShaderOp::BranchIfFalse, ShaderType::Bool);

        // Parse increment bytecode now, then relocate it after the loop body.
        ShaderInstruction increment_code[GLSL_MAX_INSTRUCTIONS] = {};
        const size_t increment_start = module_.instruction_count;
        if (!is_punctuation(')'))
            parse_for_increment();
        if (failed_)
            return;

        const size_t increment_count = module_.instruction_count - increment_start;
        if (increment_count > GLSL_MAX_INSTRUCTIONS - increment_start)
        {
            fail("for-loop increment instruction budget exceeded");
            return;
        }
        if (increment_count)
            std::memcpy(increment_code, &module_.instructions[increment_start],
                        increment_count * sizeof(ShaderInstruction));
        module_.instruction_count = increment_start;

        expect_punctuation(')');
        if (failed_)
            return;

        if (!begin_loop())
            return;
        parse_nested_statement();
        if (failed_)
            return;

        // A continue in a for-loop must execute the loop expression before
        // re-checking the condition. The relocated increment block begins at
        // the first instruction after the body.
        const size_t continue_target = module_.instruction_count;
        for (size_t i = 0; i < increment_count; ++i)
            emit(increment_code[i]);
        const size_t jump_index = module_.instruction_count;
        emit_simple(ShaderOp::Jump, ShaderType::Invalid);
        patch_branch_target(jump_index, condition_start);
        patch_branch_target(branch_index, module_.instruction_count);
        finish_loop(continue_target, module_.instruction_count);
    }

    void parse_statement()
    {
        if (is_identifier("return"))
        {
            parse_return_statement();
            return;
        }
        if (is_identifier("if"))
        {
            parse_if_statement();
            return;
        }
        if (is_identifier("while"))
        {
            parse_while_statement();
            return;
        }
        if (is_identifier("break"))
        {
            advance();
            expect_punctuation(';');
            if (!failed_)
                emit_loop_jump(true);
            return;
        }
        if (is_identifier("continue"))
        {
            advance();
            expect_punctuation(';');
            if (!failed_)
                emit_loop_jump(false);
            return;
        }
        if (is_identifier("for"))
        {
            parse_for_statement();
            return;
        }

        if (is_type_token())
        {
            parse_local_declaration();
            return;
        }

        if (current_.kind == TokenKind::Identifier &&
            peek_punctuation('('))
        {
            ShaderType result_type = ShaderType::Invalid;
            parse_primary(result_type);
            if (!failed_ && result_type != ShaderType::Invalid)
                fail("non-void function call results must be used");
            if (!failed_)
                expect_punctuation(';');
            return;
        }

        parse_assignment_statement(true);
    }

    void parse_local_declaration()
    {
        const ShaderType type = parse_type();
        if (current_.kind != TokenKind::Identifier)
        {
            fail("expected local variable name");
            return;
        }
        if (active_function_ >= module_.function_count)
        {
            fail("local declarations are only valid inside functions");
            return;
        }

        char name[GLSL_NAME_BYTES];
        std::snprintf(name, sizeof(name), "%s", current_.text);
        advance();

        if (lookup(name).type != ShaderType::Invalid)
        {
            fail("duplicate local variable");
            return;
        }

        size_t array_size = 0;
        if (accept_punctuation('['))
        {
            if (current_.kind != TokenKind::Number || !current_.integer ||
                current_.number < 1.0f ||
                current_.number > static_cast<float>(GLSL_MAX_LOCALS))
            {
                fail("array size must be a positive integer constant");
                return;
            }
            if (type == ShaderType::Sampler2D)
            {
                fail("sampler arrays are not supported");
                return;
            }
            array_size = static_cast<size_t>(current_.number);
            advance();
            if (!expect_punctuation(']'))
                return;
            if (is_punctuation('='))
            {
                fail("array initializers are not supported in GLSL ES 1.00");
                return;
            }
        }

        ShaderFunction& function = module_.functions[active_function_];
        if (!add_function_local(function, name, type, array_size))
            return;
        const uint8_t local_index =
            static_cast<uint8_t>(function.local_count - 1);

        if (accept_punctuation('='))
        {
            const ShaderType expression_type = parse_expression(1);
            if (expression_type != type)
            {
                fail("local initializer type mismatch");
                return;
            }
            emit_simple(ShaderOp::StoreLocal, type, local_index);
        }
        expect_punctuation(';');
    }

    int precedence() const
    {
        if (current_.kind != TokenKind::Punctuation)
            return 0;
        if (std::strcmp(current_.text, "||") == 0) return 1;
        if (std::strcmp(current_.text, "&&") == 0) return 2;
        if (std::strcmp(current_.text, "==") == 0 ||
            std::strcmp(current_.text, "!=") == 0) return 3;
        if (std::strcmp(current_.text, "<") == 0 ||
            std::strcmp(current_.text, "<=") == 0 ||
            std::strcmp(current_.text, ">") == 0 ||
            std::strcmp(current_.text, ">=") == 0) return 4;
        if (is_punctuation('+') || is_punctuation('-')) return 5;
        if (is_punctuation('*') || is_punctuation('/')) return 6;
        return 0;
    }

    void parse_unary(ShaderType& result_type)
    {
        if (current_.kind == TokenKind::Punctuation &&
            std::strcmp(current_.text, "!") == 0)
        {
            advance();
            parse_unary(result_type);
            expression_lvalue_ = {};
            if (result_type != ShaderType::Bool)
            {
                fail("logical not requires a bool operand");
                return;
            }
            emit_simple(ShaderOp::LogicalNot, ShaderType::Bool);
            return;
        }
        if (accept_punctuation('-'))
        {
            parse_unary(result_type);
            expression_lvalue_ = {};
            if (!is_numeric(result_type))
            {
                fail("unary minus requires a numeric scalar, vector or matrix");
                return;
            }
            emit_simple(ShaderOp::Negate, result_type);
            return;
        }
        if (accept_punctuation('+'))
        {
            parse_unary(result_type);
            expression_lvalue_ = {};
            return;
        }
        parse_primary(result_type);
    }

    ShaderType parse_expression(int minimum_precedence)
    {
        if (++recursion_depth_ > 32)
        {
            fail("expression nesting limit exceeded");
            --recursion_depth_;
            return ShaderType::Invalid;
        }

        ShaderType left_type = ShaderType::Invalid;
        parse_unary(left_type);
        while (!failed_)
        {
            const int current_precedence = precedence();
            if (current_precedence < minimum_precedence || current_precedence == 0)
                break;

            char operation[GLSL_NAME_BYTES] = {};
            std::snprintf(operation, sizeof(operation), "%s", current_.text);
            advance();
            const ShaderType right_type = parse_expression(current_precedence + 1);
            const ShaderType result_type =
                result_of_binary(operation, left_type, right_type);
            if (result_type == ShaderType::Invalid)
            {
                fail("incompatible operand types");
                break;
            }

            ShaderOp op = ShaderOp::Add;
            bool operator_found = true;
            if (std::strcmp(operation, "+") == 0) op = ShaderOp::Add;
            else if (std::strcmp(operation, "-") == 0) op = ShaderOp::Subtract;
            else if (std::strcmp(operation, "*") == 0) op = ShaderOp::Multiply;
            else if (std::strcmp(operation, "/") == 0) op = ShaderOp::Divide;
            else if (std::strcmp(operation, "==") == 0) op = ShaderOp::Equal;
            else if (std::strcmp(operation, "!=") == 0) op = ShaderOp::NotEqual;
            else if (std::strcmp(operation, "<") == 0) op = ShaderOp::Less;
            else if (std::strcmp(operation, "<=") == 0) op = ShaderOp::LessEqual;
            else if (std::strcmp(operation, ">") == 0) op = ShaderOp::Greater;
            else if (std::strcmp(operation, ">=") == 0) op = ShaderOp::GreaterEqual;
            else if (std::strcmp(operation, "&&") == 0) op = ShaderOp::LogicalAnd;
            else if (std::strcmp(operation, "||") == 0) op = ShaderOp::LogicalOr;
            else operator_found = false;
            if (!operator_found)
            {
                fail("unsupported binary operator");
                break;
            }
            emit_simple(op, result_type);
            left_type = result_type;
            expression_lvalue_ = {};
        }

        // The conditional operator binds more loosely than || and groups to
        // the right, so it is only recognized at full-expression level. Only
        // the selected operand is evaluated, as GLSL ES requires.
        if (!failed_ && minimum_precedence <= 1 && is_punctuation('?'))
            left_type = parse_conditional_tail(left_type);

        --recursion_depth_;
        return left_type;
    }

    ShaderType parse_conditional_tail(ShaderType condition_type)
    {
        advance(); /* ? */
        expression_lvalue_ = {};
        if (condition_type != ShaderType::Bool)
        {
            fail("conditional operator requires a bool condition");
            return ShaderType::Invalid;
        }
        const size_t branch_index = module_.instruction_count;
        emit_simple(ShaderOp::BranchIfFalse, ShaderType::Bool);
        const ShaderType true_type = parse_expression(1);
        if (failed_ || !expect_punctuation(':'))
            return ShaderType::Invalid;
        const size_t end_jump_index = module_.instruction_count;
        emit_simple(ShaderOp::Jump, ShaderType::Invalid);
        patch_branch_target(branch_index, module_.instruction_count);
        const ShaderType false_type = parse_expression(1);
        patch_branch_target(end_jump_index, module_.instruction_count);
        expression_lvalue_ = {};
        if (failed_)
            return ShaderType::Invalid;
        if (true_type != false_type || true_type == ShaderType::Invalid ||
            true_type == ShaderType::Sampler2D)
        {
            fail("conditional operator operands must have the same type");
            return ShaderType::Invalid;
        }
        return true_type;
    }

    static ShaderType result_of_binary(
        const char *operation,
        ShaderType left,
        ShaderType right)
    {
        if (left == ShaderType::Invalid || right == ShaderType::Invalid)
            return ShaderType::Invalid;

        if (std::strcmp(operation, "&&") == 0 ||
            std::strcmp(operation, "||") == 0)
            return left == ShaderType::Bool && right == ShaderType::Bool
                ? ShaderType::Bool : ShaderType::Invalid;

        if (std::strcmp(operation, "==") == 0 ||
            std::strcmp(operation, "!=") == 0)
        {
            if (left == right &&
                (left == ShaderType::Float || left == ShaderType::Int ||
                 left == ShaderType::Bool || is_vector(left)))
                return ShaderType::Bool;
            return ShaderType::Invalid;
        }

        if (std::strcmp(operation, "<") == 0 ||
            std::strcmp(operation, "<=") == 0 ||
            std::strcmp(operation, ">") == 0 ||
            std::strcmp(operation, ">=") == 0)
        {
            if ((left == ShaderType::Float || left == ShaderType::Int) &&
                left == right)
                return ShaderType::Bool;
            return ShaderType::Invalid;
        }

        if (std::strcmp(operation, "+") == 0 ||
            std::strcmp(operation, "-") == 0)
        {
            if (left == right &&
                (left == ShaderType::Float || left == ShaderType::Int ||
                 is_vector(left) || is_int_vector(left) || is_matrix(left)))
                return left;
            return ShaderType::Invalid;
        }

        if (std::strcmp(operation, "*") == 0)
        {
            const size_t left_dimension = matrix_dimension(left);
            const size_t right_dimension = matrix_dimension(right);
            if (left_dimension != 0 && right == vector_type(left_dimension))
                return right;
            if (right_dimension != 0 && left == vector_type(right_dimension))
                return left;
            if (left_dimension != 0 && left_dimension == right_dimension)
                return left;
            if (left_dimension != 0 && right == ShaderType::Float)
                return left;
            if (left == ShaderType::Float && right_dimension != 0)
                return right;
            if (left == right &&
                (left == ShaderType::Float || left == ShaderType::Int ||
                 is_vector(left) || is_int_vector(left)))
                return left;
            if (left == ShaderType::Float &&
                (is_vector(right) || is_int_vector(right)))
                return right;
            if (right == ShaderType::Float &&
                (is_vector(left) || is_int_vector(left)))
                return left;
            return ShaderType::Invalid;
        }

        if (std::strcmp(operation, "/") == 0)
        {
            if (left == right &&
                (left == ShaderType::Float || is_vector(left) || is_matrix(left)))
                return left;
            if ((is_vector(left) || is_int_vector(left) || is_matrix(left)) &&
                right == ShaderType::Float)
                return left;
            if (left == ShaderType::Float && is_matrix(right))
                return right;
            return ShaderType::Invalid;
        }

        return ShaderType::Invalid;
    }

    void parse_primary(ShaderType& result_type)
    {
        expression_lvalue_ = {};
        if (current_.kind == TokenKind::Number)
        {
            ShaderInstruction instruction = {};
            instruction.op = current_.integer ? ShaderOp::PushInt : ShaderOp::PushFloat;
            instruction.type = current_.integer ? ShaderType::Int : ShaderType::Float;
            instruction.literal = current_.number;
            emit(instruction);
            result_type = instruction.type;
            advance();
            parse_swizzle(result_type);
            return;
        }

        if (accept_punctuation('('))
        {
            result_type = parse_expression(1);
            expect_punctuation(')');
            parse_swizzle(result_type);
            return;
        }

        if (current_.kind != TokenKind::Identifier)
        {
            fail("expected an expression");
            result_type = ShaderType::Invalid;
            return;
        }

        char name[GLSL_NAME_BYTES];
        std::snprintf(name, sizeof(name), "%s", current_.text);
        advance();

        if (accept_punctuation('('))
        {
            result_type = parse_call(name);
            expression_lvalue_ = {};
            parse_swizzle(result_type);
            return;
        }

        if (std::strcmp(name, "true") == 0 || std::strcmp(name, "false") == 0)
        {
            ShaderInstruction instruction = {};
            instruction.op = ShaderOp::PushInt;
            instruction.type = ShaderType::Bool;
            instruction.literal = std::strcmp(name, "true") == 0 ? 1.0f : 0.0f;
            emit(instruction);
            result_type = ShaderType::Bool;
            parse_swizzle(result_type);
            return;
        }

        const ResolvedVariable variable = lookup(name);
        if (variable.type == ShaderType::Invalid)
        {
            fail("identifier is not declared");
            result_type = ShaderType::Invalid;
            return;
        }
        if ((variable.storage == Storage::Position ||
             variable.storage == Storage::PointSize ||
             variable.storage == Storage::FragmentColor) &&
            ((variable.storage == Storage::Position && stage_ != ShaderStage::Vertex) ||
             (variable.storage == Storage::PointSize && stage_ != ShaderStage::Vertex) ||
             (variable.storage == Storage::FragmentColor && stage_ != ShaderStage::Fragment)))
        {
            fail("built-in variable is unavailable in this shader stage");
            result_type = ShaderType::Invalid;
            return;
        }

        if (variable.array_size != 0)
        {
            if (!is_punctuation('['))
            {
                fail("arrays must be indexed; whole-array use is unsupported");
                result_type = ShaderType::Invalid;
                return;
            }
            ResolvedVariable element;
            bool dynamic = false;
            if (!parse_array_index(variable, element, dynamic))
            {
                result_type = ShaderType::Invalid;
                return;
            }
            const bool uniform = variable.storage == Storage::Uniform;
            if (dynamic)
                emit_simple(uniform ? ShaderOp::LoadUniformIndexed
                                    : ShaderOp::LoadLocalIndexed,
                            variable.type, variable.index, variable.array_size);
            else
                emit_simple(uniform ? ShaderOp::LoadUniform : ShaderOp::LoadLocal,
                            element.type, element.index);
            result_type = element.type;
            expression_lvalue_ = dynamic ? ResolvedVariable{} : element;
            parse_matrix_column_read(result_type);
            parse_swizzle(result_type);
            return;
        }

        expression_lvalue_ = variable;
        ShaderOp op = ShaderOp::LoadLocal;
        switch (variable.storage)
        {
            case Storage::Attribute: op = ShaderOp::LoadAttribute; break;
            case Storage::Uniform: op = ShaderOp::LoadUniform; break;
            case Storage::Varying: op = ShaderOp::LoadVarying; break;
            case Storage::Local: op = ShaderOp::LoadLocal; break;
            case Storage::Position: op = ShaderOp::LoadPosition; break;
            case Storage::PointSize: op = ShaderOp::LoadPointSize; break;
            case Storage::FragmentColor: op = ShaderOp::LoadFragmentColor; break;
        }
        emit_simple(op, variable.type, variable.index);
        result_type = variable.type;
        parse_matrix_column_read(result_type);
        parse_swizzle(result_type);
    }

    /* Optional constant `[column]` applied to a matrix value already on the
       stack. Leaves non-indexed expressions untouched. */
    void parse_matrix_column_read(ShaderType& result_type)
    {
        if (!is_punctuation('['))
            return;
        const size_t dimension = matrix_dimension(result_type);
        if (dimension == 0)
        {
            fail("indexing is supported only for matrices and arrays");
            result_type = ShaderType::Invalid;
            return;
        }
        advance();
        if (current_.kind != TokenKind::Number || !current_.integer ||
            current_.number < 0.0f || current_.number >= dimension)
        {
            fail("matrix column index must be a constant in range");
            result_type = ShaderType::Invalid;
            return;
        }
        const uint8_t column = static_cast<uint8_t>(current_.number);
        advance();
        expect_punctuation(']');
        ShaderInstruction instruction = {};
        instruction.op = ShaderOp::LoadMatrixColumn;
        instruction.type = vector_type(dimension);
        instruction.index = column;
        emit(instruction);
        result_type = instruction.type;
        expression_lvalue_ = {};
    }

    /* Sentinel for emit_user_call when no user overload matches. */
    static constexpr ShaderType CALL_NOT_FOUND = static_cast<ShaderType>(0xff);

    ShaderType emit_user_call(
        const char *name,
        const ShaderType *arguments,
        const ResolvedVariable *argument_lvalues,
        const uint8_t *argument_arrays,
        size_t argument_count)
    {
        const int function_index =
            find_function(name, arguments, argument_arrays, argument_count);
        if (function_index >= 0)
        {
            const ShaderFunction& function =
                module_.functions[static_cast<size_t>(function_index)];
            ShaderInstruction call = {};
            call.op = ShaderOp::Call;
            call.type = function.return_type;
            call.index = static_cast<uint8_t>(function_index);
            call.argument_count = static_cast<uint8_t>(argument_count);
            uint8_t output_masks[GLSL_MAX_PARAMETERS] = {};
            for (size_t i = 0; i < GLSL_MAX_PARAMETERS; ++i)
                call.output_targets[i] = CALL_OUTPUT_TARGET_NONE;

            for (size_t i = 0; i < argument_count; ++i)
            {
                if (function.parameters[i].qualifier ==
                    ShaderParameterQualifier::In)
                    continue;

                const ResolvedVariable& target = argument_lvalues[i];
                if (!target.writable ||
                    target.type != function.parameters[i].type)
                {
                    fail("out/inout arguments require a writable variable or swizzle");
                    return ShaderType::Invalid;
                }

                uint16_t storage_bits = 0;
                switch (target.storage)
                {
                    case Storage::Local:
                        storage_bits = static_cast<uint16_t>(
                            CallOutputStorage::Local) << CALL_OUTPUT_TARGET_STORAGE_SHIFT;
                        break;
                    case Storage::Varying:
                        storage_bits = static_cast<uint16_t>(
                            CallOutputStorage::Varying) << CALL_OUTPUT_TARGET_STORAGE_SHIFT;
                        break;
                    case Storage::Position:
                        storage_bits = static_cast<uint16_t>(
                            CallOutputStorage::Position) << CALL_OUTPUT_TARGET_STORAGE_SHIFT;
                        break;
                    case Storage::PointSize:
                        storage_bits = static_cast<uint16_t>(
                            CallOutputStorage::PointSize) << CALL_OUTPUT_TARGET_STORAGE_SHIFT;
                        break;
                    case Storage::FragmentColor:
                        storage_bits = static_cast<uint16_t>(
                            CallOutputStorage::FragmentColor) << CALL_OUTPUT_TARGET_STORAGE_SHIFT;
                        break;
                    default:
                        fail("unsupported output argument storage");
                        return ShaderType::Invalid;
                }

                if (!target.is_swizzle)
                {
                    call.output_targets[i] = static_cast<uint16_t>(
                        CALL_OUTPUT_TARGET_FULL | storage_bits | target.index);
                    output_masks[i] = 0xff;
                    continue;
                }

                const size_t components =
                    component_count(function.parameters[i].type);
                if (components == 0 || components > 4)
                {
                    fail("copy-out swizzle has an unsupported component count");
                    return ShaderType::Invalid;
                }

                uint16_t descriptor =
                    static_cast<uint16_t>(storage_bits | target.index);
                uint8_t mask = 0;
                for (size_t component = 0; component < components; ++component)
                {
                    const uint8_t selector = target.components[component];
                    const uint8_t bit = static_cast<uint8_t>(1u << selector);
                    if (mask & bit)
                    {
                        fail("copy-out swizzle cannot repeat a component");
                        return ShaderType::Invalid;
                    }
                    mask = static_cast<uint8_t>(mask | bit);
                    descriptor = static_cast<uint16_t>(
                        descriptor | (static_cast<uint16_t>(selector)
                                      << (3 + 2 * component)));
                }
                call.output_targets[i] = descriptor;
                output_masks[i] = mask;
            }

            // Input arguments are copied before the call and may alias an
            // output destination. Only overlapping output destinations are
            // rejected; disjoint swizzles of one local are safe to merge.
            for (size_t i = 0; i < argument_count; ++i)
            {
                if (function.parameters[i].qualifier ==
                    ShaderParameterQualifier::In)
                    continue;
                for (size_t j = 0; j < i; ++j)
                {
                    if (function.parameters[j].qualifier ==
                        ShaderParameterQualifier::In)
                        continue;
                    if (argument_lvalues[i].index ==
                            argument_lvalues[j].index &&
                        argument_lvalues[i].storage ==
                            argument_lvalues[j].storage &&
                        (output_masks[i] & output_masks[j]) != 0)
                    {
                        fail("overlapping output arguments are unsupported");
                        return ShaderType::Invalid;
                    }
                }
            }

            emit(call);
            return function.return_type;
        }
        return CALL_NOT_FOUND;
    }

    ShaderType parse_call(const char *name)
    {
        ShaderType arguments[4] = {};
        ResolvedVariable argument_lvalues[4] = {};
        uint8_t argument_arrays[4] = {};
        bool has_array_argument = false;
        size_t argument_count = 0;

        if (!accept_punctuation(')'))
        {
            for (;;)
            {
                if (argument_count >= 4)
                {
                    fail("function argument limit exceeded");
                    return ShaderType::Invalid;
                }
                // A whole array is passed as one value per element, in
                // order, matching an expanded array parameter.
                const ResolvedVariable array = current_.kind == TokenKind::Identifier
                    ? lookup(current_.text) : ResolvedVariable{};
                if (array.array_size != 0 &&
                    (peek_punctuation(',') || peek_punctuation(')')))
                {
                    if (argument_count + array.array_size > 4)
                    {
                        fail("function argument limit exceeded (array elements count individually)");
                        return ShaderType::Invalid;
                    }
                    advance(); /* array name */
                    argument_arrays[argument_count] = array.array_size;
                    for (size_t element = 0; element < array.array_size; ++element)
                    {
                        ResolvedVariable slot = {};
                        slot.storage = array.storage;
                        slot.type = array.type;
                        slot.index = static_cast<uint8_t>(array.index + element);
                        slot.writable = array.writable;
                        emit_simple(array.storage == Storage::Uniform
                                        ? ShaderOp::LoadUniform : ShaderOp::LoadLocal,
                                    slot.type, slot.index);
                        arguments[argument_count] = slot.type;
                        argument_lvalues[argument_count] = slot;
                        ++argument_count;
                    }
                    has_array_argument = true;
                    expression_lvalue_ = {};
                    if (failed_)
                        return ShaderType::Invalid;
                    if (accept_punctuation(')'))
                        break;
                    if (!expect_punctuation(','))
                        return ShaderType::Invalid;
                    continue;
                }
                const size_t argument_index = argument_count++;
                arguments[argument_index] = parse_expression(1);
                argument_lvalues[argument_index] = expression_lvalue_;
                if (failed_)
                    return ShaderType::Invalid;
                if (accept_punctuation(')'))
                    break;
                if (!expect_punctuation(','))
                    return ShaderType::Invalid;
            }
        }

        // Whole arrays only match user-defined array parameters; resolve them
        // before built-ins and constructors so expanded elements can never
        // match a built-in signature.
        if (has_array_argument)
        {
            const ShaderType user_result = emit_user_call(
                name, arguments, argument_lvalues, argument_arrays, argument_count);
            if (user_result == CALL_NOT_FOUND && !failed_)
                fail("arrays can only be passed to matching user-defined array parameters");
            return failed_ ? ShaderType::Invalid : user_result;
        }

        ShaderType constructor = constructor_type(name);
        if (constructor != ShaderType::Invalid)
        {
            if (!valid_constructor(constructor, arguments, argument_count))
            {
                fail("invalid constructor arguments");
                return ShaderType::Invalid;
            }
            emit_simple(ShaderOp::Construct, constructor, 0,
                        static_cast<uint8_t>(argument_count));
            return constructor;
        }

        if (std::strcmp(name, "texture2D") == 0)
        {
            if (argument_count != 2 ||
                arguments[0] != ShaderType::Sampler2D ||
                arguments[1] != ShaderType::Vec2)
            {
                fail("texture2D requires sampler2D and vec2 arguments");
                return ShaderType::Invalid;
            }
            emit_simple(ShaderOp::Texture2D, ShaderType::Vec4, 0, 2);
            return ShaderType::Vec4;
        }

        BuiltinFunction builtin = BuiltinFunction::Sin;
        bool unary_builtin = true;
        if (std::strcmp(name, "sin") == 0) builtin = BuiltinFunction::Sin;
        else if (std::strcmp(name, "cos") == 0) builtin = BuiltinFunction::Cos;
        else if (std::strcmp(name, "tan") == 0) builtin = BuiltinFunction::Tan;
        else if (std::strcmp(name, "asin") == 0) builtin = BuiltinFunction::Asin;
        else if (std::strcmp(name, "acos") == 0) builtin = BuiltinFunction::Acos;
        else if (std::strcmp(name, "atan") == 0 && argument_count == 1)
            builtin = BuiltinFunction::Atan;
        else if (std::strcmp(name, "exp") == 0) builtin = BuiltinFunction::Exp;
        else if (std::strcmp(name, "log") == 0) builtin = BuiltinFunction::Log;
        else if (std::strcmp(name, "exp2") == 0) builtin = BuiltinFunction::Exp2;
        else if (std::strcmp(name, "log2") == 0) builtin = BuiltinFunction::Log2;
        else if (std::strcmp(name, "sqrt") == 0) builtin = BuiltinFunction::Sqrt;
        else if (std::strcmp(name, "inversesqrt") == 0)
            builtin = BuiltinFunction::InverseSqrt;
        else if (std::strcmp(name, "abs") == 0) builtin = BuiltinFunction::Abs;
        else if (std::strcmp(name, "sign") == 0) builtin = BuiltinFunction::Sign;
        else if (std::strcmp(name, "floor") == 0) builtin = BuiltinFunction::Floor;
        else if (std::strcmp(name, "ceil") == 0) builtin = BuiltinFunction::Ceil;
        else if (std::strcmp(name, "fract") == 0) builtin = BuiltinFunction::Fract;
        else if (std::strcmp(name, "radians") == 0) builtin = BuiltinFunction::Radians;
        else if (std::strcmp(name, "degrees") == 0) builtin = BuiltinFunction::Degrees;
        else unary_builtin = false;

        if (unary_builtin)
        {
            if (argument_count != 1 || !is_gen_type(arguments[0]))
            {
                fail("built-in unary function requires one float or vector argument");
                return ShaderType::Invalid;
            }
            emit_simple(ShaderOp::Builtin, arguments[0],
                        static_cast<uint8_t>(builtin), 1);
            return arguments[0];
        }

        if (std::strcmp(name, "atan") == 0)
        {
            if (argument_count != 2 || !is_gen_type(arguments[0]) ||
                arguments[0] != arguments[1])
            {
                fail("two-argument atan requires matching float/vector arguments");
                return ShaderType::Invalid;
            }
            emit_simple(ShaderOp::Builtin, arguments[0],
                        static_cast<uint8_t>(BuiltinFunction::Atan2), 2);
            return arguments[0];
        }

        if (std::strcmp(name, "pow") == 0 || std::strcmp(name, "mod") == 0)
        {
            const bool same = argument_count == 2 &&
                is_gen_type(arguments[0]) && arguments[0] == arguments[1];
            const bool scalar_rhs = argument_count == 2 &&
                is_vector(arguments[0]) && arguments[1] == ShaderType::Float;
            if (!same && !(std::strcmp(name, "mod") == 0 && scalar_rhs))
            {
                fail("pow/mod requires matching values (mod also accepts a scalar divisor)");
                return ShaderType::Invalid;
            }
            builtin = std::strcmp(name, "pow") == 0
                ? BuiltinFunction::Pow : BuiltinFunction::Mod;
            emit_simple(ShaderOp::Builtin, arguments[0],
                        static_cast<uint8_t>(builtin), 2);
            return arguments[0];
        }

        if (std::strcmp(name, "min") == 0 || std::strcmp(name, "max") == 0)
        {
            const bool same = argument_count == 2 &&
                is_gen_type(arguments[0]) && arguments[0] == arguments[1];
            const bool scalar_rhs = argument_count == 2 &&
                is_vector(arguments[0]) && arguments[1] == ShaderType::Float;
            if (!same && !scalar_rhs)
            {
                fail("min/max requires matching scalar/vector values or vector-scalar arguments");
                return ShaderType::Invalid;
            }
            emit_simple(std::strcmp(name, "min") == 0
                ? ShaderOp::Minimum : ShaderOp::Maximum,
                arguments[0], 0, 2);
            return arguments[0];
        }

        if (std::strcmp(name, "step") == 0)
        {
            const bool same = argument_count == 2 &&
                is_gen_type(arguments[1]) && arguments[0] == arguments[1];
            const bool scalar_edge = argument_count == 2 &&
                is_vector(arguments[1]) && arguments[0] == ShaderType::Float;
            if (!same && !scalar_edge)
            {
                fail("step requires matching edge/value types or a scalar edge");
                return ShaderType::Invalid;
            }
            emit_simple(ShaderOp::Builtin, arguments[1],
                        static_cast<uint8_t>(BuiltinFunction::Step), 2);
            return arguments[1];
        }

        if (std::strcmp(name, "smoothstep") == 0)
        {
            const bool same = argument_count == 3 &&
                is_gen_type(arguments[2]) &&
                arguments[0] == arguments[1] &&
                arguments[1] == arguments[2];
            const bool scalar_edges = argument_count == 3 &&
                is_vector(arguments[2]) &&
                arguments[0] == ShaderType::Float &&
                arguments[1] == ShaderType::Float;
            if (!same && !scalar_edges)
            {
                fail("smoothstep requires matching types or scalar edges with a vector value");
                return ShaderType::Invalid;
            }
            emit_simple(ShaderOp::Builtin, arguments[2],
                        static_cast<uint8_t>(BuiltinFunction::SmoothStep), 3);
            return arguments[2];
        }

        if (std::strcmp(name, "distance") == 0)
        {
            if (argument_count != 2 || !is_vector(arguments[0]) ||
                arguments[0] != arguments[1])
            {
                fail("distance requires matching vector arguments");
                return ShaderType::Invalid;
            }
            emit_simple(ShaderOp::Builtin, ShaderType::Float,
                        static_cast<uint8_t>(BuiltinFunction::Distance), 2);
            return ShaderType::Float;
        }

        if (std::strcmp(name, "cross") == 0)
        {
            if (argument_count != 2 || arguments[0] != ShaderType::Vec3 ||
                arguments[1] != ShaderType::Vec3)
            {
                fail("cross requires two vec3 arguments");
                return ShaderType::Invalid;
            }
            emit_simple(ShaderOp::Builtin, ShaderType::Vec3,
                        static_cast<uint8_t>(BuiltinFunction::Cross), 2);
            return ShaderType::Vec3;
        }

        if (std::strcmp(name, "faceforward") == 0 ||
            std::strcmp(name, "reflect") == 0)
        {
            const bool is_reflect = std::strcmp(name, "reflect") == 0;
            if (argument_count != (is_reflect ? 2u : 3u) ||
                !is_vector(arguments[0]) || arguments[0] != arguments[1] ||
                (!is_reflect && arguments[0] != arguments[2]))
            {
                fail("geometric function requires matching vector arguments");
                return ShaderType::Invalid;
            }
            builtin = is_reflect
                ? BuiltinFunction::Reflect : BuiltinFunction::FaceForward;
            emit_simple(ShaderOp::Builtin, arguments[0],
                        static_cast<uint8_t>(builtin),
                        static_cast<uint8_t>(argument_count));
            return arguments[0];
        }

        if (std::strcmp(name, "refract") == 0)
        {
            if (argument_count != 3 || !is_vector(arguments[0]) ||
                arguments[0] != arguments[1] ||
                arguments[2] != ShaderType::Float)
            {
                fail("refract requires matching vectors and a scalar eta");
                return ShaderType::Invalid;
            }
            emit_simple(ShaderOp::Builtin, arguments[0],
                        static_cast<uint8_t>(BuiltinFunction::Refract), 3);
            return arguments[0];
        }

        if (std::strcmp(name, "matrixCompMult") == 0)
        {
            if (argument_count != 2 || !is_matrix(arguments[0]) ||
                arguments[0] != arguments[1])
            {
                fail("matrixCompMult requires two matching matrices");
                return ShaderType::Invalid;
            }
            emit_simple(ShaderOp::Builtin, arguments[0],
                        static_cast<uint8_t>(BuiltinFunction::MatrixCompMult), 2);
            return arguments[0];
        }

        if (std::strcmp(name, "lessThan") == 0 ||
            std::strcmp(name, "lessThanEqual") == 0 ||
            std::strcmp(name, "greaterThan") == 0 ||
            std::strcmp(name, "greaterThanEqual") == 0 ||
            std::strcmp(name, "equal") == 0 ||
            std::strcmp(name, "notEqual") == 0)
        {
            if (argument_count != 2 ||
                !is_any_vector(arguments[0]) ||
                arguments[0] != arguments[1] ||
                (is_bool_vector(arguments[0]) &&
                 std::strcmp(name, "equal") != 0 &&
                 std::strcmp(name, "notEqual") != 0))
            {
                fail("vector relational function requires matching vector arguments");
                return ShaderType::Invalid;
            }
            if ((std::strcmp(name, "lessThan") == 0 ||
                 std::strcmp(name, "lessThanEqual") == 0 ||
                 std::strcmp(name, "greaterThan") == 0 ||
                 std::strcmp(name, "greaterThanEqual") == 0) &&
                !is_vector(arguments[0]) && !is_int_vector(arguments[0]))
            {
                fail("ordered vector comparison requires float or integer vectors");
                return ShaderType::Invalid;
            }

            if (std::strcmp(name, "lessThan") == 0)
                builtin = BuiltinFunction::LessThan;
            else if (std::strcmp(name, "lessThanEqual") == 0)
                builtin = BuiltinFunction::LessThanEqual;
            else if (std::strcmp(name, "greaterThan") == 0)
                builtin = BuiltinFunction::GreaterThan;
            else if (std::strcmp(name, "greaterThanEqual") == 0)
                builtin = BuiltinFunction::GreaterThanEqual;
            else if (std::strcmp(name, "equal") == 0)
                builtin = BuiltinFunction::EqualVector;
            else
                builtin = BuiltinFunction::NotEqualVector;

            emit_simple(ShaderOp::Builtin,
                vector_type_for(vector_dimension(arguments[0]), ShaderType::Bool),
                static_cast<uint8_t>(builtin), 2);
            return vector_type_for(vector_dimension(arguments[0]), ShaderType::Bool);
        }

        if (std::strcmp(name, "any") == 0 || std::strcmp(name, "all") == 0)
        {
            if (argument_count != 1 || !is_bool_vector(arguments[0]))
            {
                fail("any/all requires a boolean vector argument");
                return ShaderType::Invalid;
            }
            emit_simple(ShaderOp::Builtin, ShaderType::Bool,
                static_cast<uint8_t>(std::strcmp(name, "any") == 0
                    ? BuiltinFunction::Any : BuiltinFunction::All), 1);
            return ShaderType::Bool;
        }

        if (std::strcmp(name, "not") == 0)
        {
            if (argument_count != 1 || !is_bool_vector(arguments[0]))
            {
                fail("not requires a boolean vector argument");
                return ShaderType::Invalid;
            }
            emit_simple(ShaderOp::Builtin, arguments[0],
                        static_cast<uint8_t>(BuiltinFunction::NotVector), 1);
            return arguments[0];
        }

        if (std::strcmp(name, "dot") == 0)
        {
            if (argument_count != 2 || !is_vector(arguments[0]) ||
                arguments[0] != arguments[1])
            {
                fail("dot requires two matching vector arguments");
                return ShaderType::Invalid;
            }
            emit_simple(ShaderOp::Dot, ShaderType::Float, 0, 2);
            return ShaderType::Float;
        }

        if (std::strcmp(name, "length") == 0)
        {
            if (argument_count != 1 || !is_vector(arguments[0]))
            {
                fail("length requires a vector argument");
                return ShaderType::Invalid;
            }
            emit_simple(ShaderOp::Length, ShaderType::Float, 0, 1);
            return ShaderType::Float;
        }

        if (std::strcmp(name, "normalize") == 0)
        {
            if (argument_count != 1 || !is_vector(arguments[0]))
            {
                fail("normalize requires a vector argument");
                return ShaderType::Invalid;
            }
            emit_simple(ShaderOp::Normalize, arguments[0], 0, 1);
            return arguments[0];
        }

        if (std::strcmp(name, "mix") == 0)
        {
            const bool scalar_factor = argument_count == 3 &&
                arguments[0] == arguments[1] &&
                (arguments[0] == ShaderType::Float || is_vector(arguments[0])) &&
                arguments[2] == ShaderType::Float;
            const bool vector_factor = argument_count == 3 &&
                is_vector(arguments[0]) && arguments[0] == arguments[1] &&
                arguments[2] == arguments[0];
            if (!scalar_factor && !vector_factor)
            {
                fail("mix requires matching values and scalar or vector factor");
                return ShaderType::Invalid;
            }
            emit_simple(ShaderOp::Mix, arguments[0], 0, 3);
            return arguments[0];
        }

        if (std::strcmp(name, "clamp") == 0)
        {
            const bool matching_bounds =
                argument_count == 3 &&
                arguments[0] == arguments[1] &&
                arguments[0] == arguments[2];
            const bool scalar_bounds =
                argument_count == 3 &&
                is_vector(arguments[0]) &&
                arguments[1] == ShaderType::Float &&
                arguments[2] == ShaderType::Float;
            if (argument_count != 3 ||
                (arguments[0] != ShaderType::Float &&
                 !is_vector(arguments[0])) ||
                (!matching_bounds && !scalar_bounds))
            {
                fail("clamp requires a scalar or vector with matching bounds or scalar bounds");
                return ShaderType::Invalid;
            }
            emit_simple(ShaderOp::Clamp, arguments[0], 0, 3);
            return arguments[0];
        }

        {
            const ShaderType user_result = emit_user_call(
                name, arguments, argument_lvalues, argument_arrays, argument_count);
            if (user_result != CALL_NOT_FOUND || failed_)
                return user_result;
        }

        if (has_function_name(name))
            fail("no exact matching user-defined function overload");
        else
            fail("unsupported built-in or undeclared user function");
        return ShaderType::Invalid;
    }

    static ShaderType constructor_type(const char *name)
    {
        if (std::strcmp(name, "float") == 0) return ShaderType::Float;
        if (std::strcmp(name, "int") == 0) return ShaderType::Int;
        if (std::strcmp(name, "bool") == 0) return ShaderType::Bool;
        if (std::strcmp(name, "vec2") == 0) return ShaderType::Vec2;
        if (std::strcmp(name, "vec3") == 0) return ShaderType::Vec3;
        if (std::strcmp(name, "vec4") == 0) return ShaderType::Vec4;
        if (std::strcmp(name, "ivec2") == 0) return ShaderType::IVec2;
        if (std::strcmp(name, "ivec3") == 0) return ShaderType::IVec3;
        if (std::strcmp(name, "ivec4") == 0) return ShaderType::IVec4;
        if (std::strcmp(name, "bvec2") == 0) return ShaderType::BVec2;
        if (std::strcmp(name, "bvec3") == 0) return ShaderType::BVec3;
        if (std::strcmp(name, "bvec4") == 0) return ShaderType::BVec4;
        if (std::strcmp(name, "mat2") == 0) return ShaderType::Mat2;
        if (std::strcmp(name, "mat3") == 0) return ShaderType::Mat3;
        if (std::strcmp(name, "mat4") == 0) return ShaderType::Mat4;
        return ShaderType::Invalid;
    }

    static bool valid_constructor(
        ShaderType destination,
        const ShaderType *arguments,
        size_t argument_count)
    {
        if (argument_count == 0)
            return false;
        const size_t required = component_count(destination);
        size_t supplied = 0;
        for (size_t i = 0; i < argument_count; ++i)
        {
            const size_t count = component_count(arguments[i]);
            if (count == 0 || arguments[i] == ShaderType::Sampler2D ||
                (is_matrix(arguments[i]) && !is_matrix(destination)))
                return false;
            supplied += count;
        }

        if (argument_count == 1 && arguments[0] == ShaderType::Float)
            return true;
        if (argument_count == 1 && arguments[0] == destination)
            return true;
        if (argument_count == 1 && is_matrix(destination) &&
            is_matrix(arguments[0]))
            return true;
        if (destination == ShaderType::Float || destination == ShaderType::Int ||
            destination == ShaderType::Bool)
            return argument_count == 1 &&
                (arguments[0] == ShaderType::Float ||
                 arguments[0] == ShaderType::Int ||
                 arguments[0] == ShaderType::Bool);
        if (is_any_vector(destination))
        {
            for (size_t i = 0; i < argument_count; ++i)
            {
                if (arguments[i] == ShaderType::Sampler2D ||
                    is_matrix(arguments[i]))
                    return false;
            }
            if (argument_count == 1 &&
                component_count(arguments[0]) == 1)
                return true;
            return supplied == required;
        }
        return supplied == required;
    }

    void parse_swizzle(ShaderType& type)
    {
        if (!accept_punctuation('.'))
            return;

        const ResolvedVariable base_lvalue = expression_lvalue_;
        expression_lvalue_ = {};
        if (!is_any_vector(type) || current_.kind != TokenKind::Identifier)
        {
            fail("swizzles require a vector and a component selector");
            type = ShaderType::Invalid;
            return;
        }

        const size_t length = std::strlen(current_.text);
        if (length == 0 || length > 4)
        {
            fail("invalid swizzle length");
            type = ShaderType::Invalid;
            return;
        }

        ShaderInstruction instruction = {};
        instruction.op = ShaderOp::Swizzle;
        instruction.type = vector_type_for(length, vector_base_type(type));
        instruction.argument_count = static_cast<uint8_t>(length);

        const char *selectors = "xyzw";
        const char *aliases = "rgba";
        const size_t source_components = component_count(type);
        for (size_t i = 0; i < length; ++i)
        {
            const char *position = std::strchr(selectors, current_.text[i]);
            if (!position)
                position = std::strchr(aliases, current_.text[i]);
            if (!position)
            {
                fail("invalid swizzle component");
                type = ShaderType::Invalid;
                return;
            }
            size_t component = static_cast<size_t>(
                position - (std::strchr(selectors, current_.text[i])
                    ? selectors : aliases));
            if (component >= source_components)
            {
                fail("swizzle component exceeds vector width");
                type = ShaderType::Invalid;
                return;
            }
            instruction.swizzle[i] = static_cast<uint8_t>(component);
        }

        advance();
        emit(instruction);
        type = instruction.type;

        if (base_lvalue.writable)
        {
            expression_lvalue_ = base_lvalue;
            expression_lvalue_.type = instruction.type;
            expression_lvalue_.is_swizzle = true;
            for (size_t i = 0; i < instruction.argument_count; ++i)
                expression_lvalue_.components[i] = instruction.swizzle[i];
        }
    }

    ShaderStage stage_;
    Lexer lexer_;
    ShaderModule& module_;
    Token current_;
    bool failed_;
    size_t recursion_depth_;
    size_t active_function_;
    bool main_terminated_;
    ResolvedVariable expression_lvalue_;
    static constexpr size_t NO_ACTIVE_FUNCTION = static_cast<size_t>(-1);
    LoopFrame loop_stack_[MAX_LOOP_NESTING];
    LoopJumpPatch jump_patches_[GLSL_MAX_INSTRUCTIONS];
    size_t loop_depth_;
    size_t jump_patch_count_;
};

static void set_zero_value(ShaderValue& value, ShaderType type)
{
    value.type = type;
    const size_t components = component_count(type);
    for (size_t i = 0; i < components; ++i)
        value.data[i] = 0.0f;
    const size_t derivative_components = std::min<size_t>(components, 4);
    for (size_t i = 0; i < derivative_components; ++i)
    {
        value.ddx[i] = 0.0f;
        value.ddy[i] = 0.0f;
    }
}

/* ShaderValue reserves storage for mat4, but most VM stack entries are
   scalars or short vectors. Copy only active components; derivative storage
   is relevant only to the first four components. Unused slots are never read
   for the copied type. */
static void copy_shader_value(ShaderValue& destination,
                              const ShaderValue& source)
{
    destination.type = source.type;
    const size_t components = component_count(source.type);
    for (size_t i = 0; i < components; ++i)
        destination.data[i] = source.data[i];
    const size_t derivative_components = std::min<size_t>(components, 4);
    for (size_t i = 0; i < derivative_components; ++i)
    {
        destination.ddx[i] = source.ddx[i];
        destination.ddy[i] = source.ddy[i];
    }
}

static float convert_component(ShaderType destination, float value)
{
    if (destination == ShaderType::Int || is_int_vector(destination))
        return static_cast<float>(static_cast<int32_t>(value));
    if (destination == ShaderType::Bool || is_bool_vector(destination))
        return value != 0.0f ? 1.0f : 0.0f;
    return value;
}

static bool pop_value(
    ShaderValue stack[GLSL_MAX_STACK],
    size_t& stack_size,
    ShaderValue& value)
{
    if (stack_size == 0)
        return false;
    copy_shader_value(value, stack[--stack_size]);
    return true;
}

static bool push_value(
    ShaderValue stack[GLSL_MAX_STACK],
    size_t& stack_size,
    const ShaderValue& value)
{
    if (stack_size >= GLSL_MAX_STACK)
        return false;
    copy_shader_value(stack[stack_size++], value);
    return true;
}

static ShaderValue *call_output_target_value(
    uint16_t descriptor,
    ShaderRuntime& runtime,
    ShaderValue *locals)
{
    const uint8_t storage = static_cast<uint8_t>(
        (descriptor & CALL_OUTPUT_TARGET_STORAGE_MASK) >>
        CALL_OUTPUT_TARGET_STORAGE_SHIFT);
    const uint8_t index = static_cast<uint8_t>(descriptor & 0x7);
    switch (static_cast<CallOutputStorage>(storage))
    {
        case CallOutputStorage::Local:
            return index < GLSL_MAX_LOCALS ? &locals[index] : nullptr;
        case CallOutputStorage::Varying:
            return index < GLSL_MAX_VARYINGS ? &runtime.varyings[index] : nullptr;
        case CallOutputStorage::Position:
            return index == 0 ? &runtime.position : nullptr;
        case CallOutputStorage::PointSize:
            return index == 0 ? &runtime.point_size : nullptr;
        case CallOutputStorage::FragmentColor:
            return index == 0 ? &runtime.fragment_color : nullptr;
        default:
            return nullptr;
    }
}

static ShaderType call_output_target_declared_type(
    const ShaderModule& module,
    size_t caller_function_index,
    uint16_t descriptor)
{
    const uint8_t storage = static_cast<uint8_t>(
        (descriptor & CALL_OUTPUT_TARGET_STORAGE_MASK) >>
        CALL_OUTPUT_TARGET_STORAGE_SHIFT);
    const uint8_t index = static_cast<uint8_t>(descriptor & 0x7);
    switch (static_cast<CallOutputStorage>(storage))
    {
        case CallOutputStorage::Local:
            if (caller_function_index >= module.function_count)
                return ShaderType::Invalid;
            for (size_t i = 0;
                 i < module.functions[caller_function_index].local_count; ++i)
            {
                const ShaderVariable& local =
                    module.functions[caller_function_index].locals[i];
                if (local.index == index)
                    return local.type;
            }
            return ShaderType::Invalid;
        case CallOutputStorage::Varying:
            return index < module.varying_count
                ? module.varyings[index].type : ShaderType::Invalid;
        case CallOutputStorage::Position:
        case CallOutputStorage::FragmentColor:
            return index == 0 ? ShaderType::Vec4 : ShaderType::Invalid;
        case CallOutputStorage::PointSize:
            return index == 0 ? ShaderType::Float : ShaderType::Invalid;
        default:
            return ShaderType::Invalid;
    }
}

static void apply_binary(
    ShaderOp op,
    const ShaderValue& left,
    const ShaderValue& right,
    ShaderType result_type,
    ShaderValue& result)
{
    set_zero_value(result, result_type);
    const size_t count = component_count(result_type);

    const size_t left_dimension = matrix_dimension(left.type);
    const size_t right_dimension = matrix_dimension(right.type);

    if (op == ShaderOp::Multiply &&
        left_dimension != 0 && right.type == vector_type(left_dimension))
    {
        set_zero_value(result, right.type);
        for (size_t row = 0; row < left_dimension; ++row)
        {
            for (size_t column = 0; column < left_dimension; ++column)
            {
                const size_t matrix_index = column * left_dimension + row;
                result.data[row] +=
                    left.data[matrix_index] * right.data[column];
                if (column < 4 && row < 4)
                {
                    result.ddx[row] += left.data[matrix_index] * right.ddx[column];
                    result.ddy[row] += left.data[matrix_index] * right.ddy[column];
                }
            }
        }
        return;
    }

    if (op == ShaderOp::Multiply &&
        left.type == vector_type(right_dimension) && right_dimension != 0)
    {
        set_zero_value(result, left.type);
        for (size_t column = 0; column < right_dimension; ++column)
        {
            for (size_t row = 0; row < right_dimension; ++row)
            {
                const size_t matrix_index = column * right_dimension + row;
                result.data[column] +=
                    left.data[row] * right.data[matrix_index];
                if (row < 4 && column < 4)
                {
                    result.ddx[column] += left.ddx[row] * right.data[matrix_index];
                    result.ddy[column] += left.ddy[row] * right.data[matrix_index];
                }
            }
        }
        return;
    }

    if (op == ShaderOp::Multiply &&
        left_dimension != 0 && left_dimension == right_dimension)
    {
        set_zero_value(result, left.type);
        for (size_t column = 0; column < left_dimension; ++column)
        {
            for (size_t row = 0; row < left_dimension; ++row)
            {
                for (size_t k = 0; k < left_dimension; ++k)
                    result.data[column * left_dimension + row] +=
                        left.data[k * left_dimension + row] *
                        right.data[column * right_dimension + k];
            }
        }
        return;
    }

    const size_t left_components = component_count(left.type);
    const size_t right_components = component_count(right.type);
    const bool scalar_left = left_components == 1;
    const bool scalar_right = right_components == 1;
    for (size_t i = 0; i < count; ++i)
    {
        const size_t li = scalar_left ? 0 : i;
        const size_t ri = scalar_right ? 0 : i;
        const float a = left.data[li];
        const float b = right.data[ri];
        const float left_dx = li < 4 ? left.ddx[li] : 0.0f;
        const float left_dy = li < 4 ? left.ddy[li] : 0.0f;
        const float right_dx = ri < 4 ? right.ddx[ri] : 0.0f;
        const float right_dy = ri < 4 ? right.ddy[ri] : 0.0f;
        switch (op)
        {
            case ShaderOp::Add:
                result.data[i] = a + b;
                result.ddx[i] = left_dx + right_dx;
                result.ddy[i] = left_dy + right_dy;
                break;
            case ShaderOp::Subtract:
                result.data[i] = a - b;
                result.ddx[i] = left_dx - right_dx;
                result.ddy[i] = left_dy - right_dy;
                break;
            case ShaderOp::Multiply:
                result.data[i] = a * b;
                result.ddx[i] = left_dx * b + a * right_dx;
                result.ddy[i] = left_dy * b + a * right_dy;
                break;
            case ShaderOp::Divide:
                result.data[i] = b == 0.0f ? 0.0f : a / b;
                if (b != 0.0f)
                {
                    const float inverse_square = 1.0f / (b * b);
                    result.ddx[i] = (left_dx * b - a * right_dx) * inverse_square;
                    result.ddy[i] = (left_dy * b - a * right_dy) * inverse_square;
                }
                break;
            default:
                break;
        }
    }
}

/* Resolve a dynamic array index to a local slot. GLSL ES leaves
   out-of-range indexing undefined; the VM fails the invocation instead of
   touching a neighboring variable. */
static bool array_element_slot(
    const ShaderInstruction& instruction,
    const ShaderValue& index,
    size_t& slot,
    size_t slot_limit = GLSL_MAX_LOCALS)
{
    if (index.type != ShaderType::Int)
        return false;
    const float element = index.data[0];
    if (!(element >= 0.0f) ||
        element >= static_cast<float>(instruction.argument_count))
        return false;
    slot = static_cast<size_t>(instruction.index) +
           static_cast<size_t>(element);
    return slot < slot_limit;
}

static bool finite_value(const ShaderValue& value)
{
    const size_t count = component_count(value.type);
    for (size_t i = 0; i < count; ++i)
        if (!isfinite(value.data[i]))
            return false;
    return true;
}

} // namespace

bool compile_glsl(
    ShaderStage stage,
    const char *source,
    ShaderModule& module)
{
    module = {};
    module.stage = stage;
    if (!source)
    {
        std::snprintf(module.info_log, sizeof(module.info_log),
                      "GLSL ES subset: source pointer is null");
        return false;
    }

    /* GLSL ES 1.00 requires the version directive to precede shader
       declarations. The subset accepts #version 100 and rejects other
       preprocessor directives explicitly. */
    const char *shader_source = source;
    while (*shader_source == ' ' || *shader_source == '\t' ||
           *shader_source == '\r' || *shader_source == '\n')
        ++shader_source;
    if (*shader_source == '#')
    {
        const char *line_end = shader_source;
        while (*line_end && *line_end != '\n' && *line_end != '\r')
            ++line_end;
        const char *expected = "#version 100";
        size_t length = static_cast<size_t>(line_end - shader_source);
        while (length > 0 &&
               (shader_source[length - 1] == ' ' ||
                shader_source[length - 1] == '\t'))
            --length;
        if (length != std::strlen(expected) ||
            std::strncmp(shader_source, expected, length) != 0)
        {
            std::snprintf(module.info_log, sizeof(module.info_log),
                          "GLSL ES subset: unsupported preprocessor directive");
            return false;
        }
        shader_source = line_end;
        while (*shader_source == '\n' || *shader_source == '\r')
            ++shader_source;
    }

    Parser parser(stage, shader_source, module);
    module.compiled = parser.parse();
    return module.compiled;
}

void prepare_shader_runtime(
    const ShaderModule& module,
    ShaderRuntime& runtime)
{
    for (size_t i = 0; i < module.local_count; ++i)
    {
        const ShaderVariable& variable = module.locals[i];
        if (variable.index < GLSL_MAX_LOCALS)
            set_zero_value(runtime.locals[variable.index], variable.type);
    }

    // A reused output must not make an invocation appear successful when
    // its shader failed to write the required stage result.
    runtime.position.type = ShaderType::Invalid;
    runtime.point_size.type = ShaderType::Invalid;
    runtime.fragment_color.type = ShaderType::Invalid;
}

bool execute_glsl(
    const ShaderModule& module,
    ShaderRuntime& runtime,
    TextureSampleFunction texture_sample,
    void *user_data)
{
    if (!module.compiled || !module.has_main ||
        module.main_function >= module.function_count)
        return false;

    struct CallFrame
    {
        size_t return_pc;
        size_t stack_base;
        uint8_t function_index;
        uint16_t output_targets[GLSL_MAX_PARAMETERS];
        ShaderValue locals[GLSL_MAX_LOCALS];
    };

    CallFrame call_frames[GLSL_MAX_CALL_DEPTH];
    size_t call_depth = 0;
    bool main_returned = false;

    // Stack slots are written by push_value before they can be read.
    // Clearing all 32 large ShaderValue objects for every fragment shader
    // invocation is unnecessary on this bounded interpreter.
    ShaderValue stack[GLSL_MAX_STACK];
    size_t stack_size = 0;
    size_t pc = module.functions[module.main_function].entry_pc;
    size_t executed_steps = 0;

    while (pc < module.instruction_count && !main_returned)
    {
        if (++executed_steps > GLSL_MAX_EXECUTION_STEPS)
            return false;
        const ShaderInstruction& instruction = module.instructions[pc++];
        // Each opcode initializes an operand with pop_value/load and
        // initializes result before pushing it. Avoid zeroing four full
        // ShaderValue temporaries for every bytecode instruction.
        ShaderValue a;
        ShaderValue b;
        ShaderValue c;
        ShaderValue result;
        ShaderValue *locals = call_depth == 0
            ? runtime.locals : call_frames[call_depth - 1].locals;

        switch (instruction.op)
        {
            case ShaderOp::PushFloat:
            case ShaderOp::PushInt:
                set_zero_value(result, instruction.type);
                result.data[0] = instruction.literal;
                if (!push_value(stack, stack_size, result))
                    return false;
                break;

            case ShaderOp::LoadAttribute:
                if (instruction.index >= GLSL_MAX_ATTRIBUTES ||
                    !push_value(stack, stack_size, runtime.attributes[instruction.index]))
                    return false;
                break;
            case ShaderOp::LoadUniform:
                if (instruction.index >= GLSL_MAX_UNIFORMS ||
                    !push_value(stack, stack_size, runtime.uniforms[instruction.index]))
                    return false;
                break;
            case ShaderOp::LoadVarying:
                if (instruction.index >= GLSL_MAX_VARYINGS)
                    return false;
                if (runtime.varyings[instruction.index].type == instruction.type)
                {
                    if (!push_value(stack, stack_size,
                                    runtime.varyings[instruction.index]))
                        return false;
                }
                else
                {
                    set_zero_value(result, instruction.type);
                    if (!push_value(stack, stack_size, result))
                        return false;
                }
                break;
            case ShaderOp::LoadPosition:
                if (instruction.type != ShaderType::Vec4)
                    return false;
                if (runtime.position.type == ShaderType::Vec4)
                    copy_shader_value(result, runtime.position);
                else
                    set_zero_value(result, ShaderType::Vec4);
                if (!push_value(stack, stack_size, result))
                    return false;
                break;
            case ShaderOp::LoadPointSize:
                if (instruction.type != ShaderType::Float)
                    return false;
                if (runtime.point_size.type == ShaderType::Float)
                    copy_shader_value(result, runtime.point_size);
                else
                    set_zero_value(result, ShaderType::Float);
                if (!push_value(stack, stack_size, result))
                    return false;
                break;
            case ShaderOp::LoadFragmentColor:
                if (instruction.type != ShaderType::Vec4)
                    return false;
                if (runtime.fragment_color.type == ShaderType::Vec4)
                    copy_shader_value(result, runtime.fragment_color);
                else
                    set_zero_value(result, ShaderType::Vec4);
                if (!push_value(stack, stack_size, result))
                    return false;
                break;
            case ShaderOp::LoadLocal:
                if (instruction.index >= GLSL_MAX_LOCALS ||
                    !push_value(stack, stack_size, locals[instruction.index]))
                    return false;
                break;

            case ShaderOp::StoreVarying:
                if (!pop_value(stack, stack_size, a) ||
                    instruction.index >= GLSL_MAX_VARYINGS ||
                    a.type != instruction.type)
                    return false;
                copy_shader_value(runtime.varyings[instruction.index], a);
                break;
            case ShaderOp::StoreLocal:
                if (!pop_value(stack, stack_size, a) ||
                    instruction.index >= GLSL_MAX_LOCALS ||
                    a.type != instruction.type)
                    return false;
                copy_shader_value(locals[instruction.index], a);
                break;
            case ShaderOp::StorePosition:
                if (!pop_value(stack, stack_size, a) ||
                    a.type != ShaderType::Vec4)
                    return false;
                copy_shader_value(runtime.position, a);
                break;
            case ShaderOp::StorePointSize:
                if (!pop_value(stack, stack_size, a) ||
                    a.type != ShaderType::Float)
                    return false;
                copy_shader_value(runtime.point_size, a);
                break;
            case ShaderOp::StoreFragmentColor:
                if (!pop_value(stack, stack_size, a) ||
                    a.type != ShaderType::Vec4)
                    return false;
                copy_shader_value(runtime.fragment_color, a);
                break;

            case ShaderOp::LoadMatrixColumn:
            {
                if (!pop_value(stack, stack_size, a))
                    return false;
                const size_t dimension = matrix_dimension(a.type);
                if (dimension == 0 || instruction.index >= dimension ||
                    instruction.type != vector_type(dimension))
                    return false;
                set_zero_value(result, instruction.type);
                const size_t offset = static_cast<size_t>(instruction.index) * dimension;
                for (size_t row = 0; row < dimension; ++row)
                    result.data[row] = a.data[offset + row];
                if (!push_value(stack, stack_size, result))
                    return false;
                break;
            }

            case ShaderOp::StoreMatrixColumn:
            {
                if (!pop_value(stack, stack_size, a) ||
                    !pop_value(stack, stack_size, b) ||
                    instruction.index >= GLSL_MAX_LOCALS)
                    return false;
                const size_t dimension = matrix_dimension(instruction.type);
                if (dimension == 0 || instruction.argument_count >= dimension ||
                    b.type != instruction.type ||
                    a.type != vector_type(dimension))
                    return false;
                const size_t offset = static_cast<size_t>(instruction.argument_count) * dimension;
                for (size_t row = 0; row < dimension; ++row)
                    b.data[offset + row] = a.data[row];
                copy_shader_value(locals[instruction.index], b);
                break;
            }

            case ShaderOp::StoreSwizzle:
            {
                if (!pop_value(stack, stack_size, a) ||
                    !is_any_vector(instruction.type) ||
                    instruction.argument_count == 0 ||
                    instruction.argument_count > 4 ||
                    a.type != vector_type_for(
                        instruction.argument_count,
                        vector_base_type(instruction.type)))
                    return false;

                ShaderValue *destination = nullptr;
                const Storage storage = static_cast<Storage>(
                    static_cast<uint8_t>(instruction.literal));
                switch (storage)
                {
                    case Storage::Local:
                        if (instruction.index >= GLSL_MAX_LOCALS)
                            return false;
                        destination = &locals[instruction.index];
                        break;
                    case Storage::Varying:
                        if (instruction.index >= GLSL_MAX_VARYINGS)
                            return false;
                        destination = &runtime.varyings[instruction.index];
                        break;
                    case Storage::Position:
                        destination = &runtime.position;
                        break;
                    case Storage::FragmentColor:
                        destination = &runtime.fragment_color;
                        break;
                    default:
                        return false;
                }
                if (destination->type != instruction.type)
                    return false;
                for (size_t i = 0; i < instruction.argument_count; ++i)
                {
                    const size_t component = instruction.swizzle[i];
                    if (component >= component_count(instruction.type))
                        return false;
                    for (size_t j = 0; j < i; ++j)
                    {
                        if (instruction.swizzle[j] == component)
                            return false;
                    }
                    destination->data[component] = a.data[i];
                    if (component < 4 && i < 4)
                    {
                        destination->ddx[component] = a.ddx[i];
                        destination->ddy[component] = a.ddy[i];
                    }
                }
                break;
            }

            case ShaderOp::BranchIfFalse:
                if (!pop_value(stack, stack_size, a) ||
                    a.type != ShaderType::Bool ||
                    instruction.index > module.instruction_count)
                    return false;
                if (a.data[0] == 0.0f)
                    pc = instruction.index;
                break;

            case ShaderOp::Jump:
                if (instruction.index > module.instruction_count)
                    return false;
                pc = instruction.index;
                break;

            case ShaderOp::Call:
            {
                if (instruction.index >= module.function_count ||
                    call_depth >= GLSL_MAX_CALL_DEPTH)
                    return false;
                const ShaderFunction& function =
                    module.functions[instruction.index];
                const size_t argument_count = instruction.argument_count;
                if (!function.defined ||
                    argument_count != function.parameter_count ||
                    argument_count > GLSL_MAX_PARAMETERS ||
                    stack_size < argument_count)
                    return false;

                ShaderValue call_arguments[GLSL_MAX_PARAMETERS];
                for (size_t i = argument_count; i > 0; --i)
                {
                    if (!pop_value(stack, stack_size, call_arguments[i - 1]) ||
                        call_arguments[i - 1].type !=
                            function.parameters[i - 1].type)
                        return false;
                }

                CallFrame& frame = call_frames[call_depth];
                frame.return_pc = pc;
                frame.stack_base = stack_size;
                frame.function_index = instruction.index;
                for (size_t i = 0; i < GLSL_MAX_PARAMETERS; ++i)
                    frame.output_targets[i] = CALL_OUTPUT_TARGET_NONE;

                for (size_t i = 0; i < function.local_count; ++i)
                {
                    const ShaderVariable& local = function.locals[i];
                    if (local.index >= GLSL_MAX_LOCALS)
                        return false;
                    set_zero_value(frame.locals[local.index], local.type);
                }

                for (size_t i = 0; i < argument_count; ++i)
                {
                    const ShaderVariable& parameter = function.parameters[i];
                    if (parameter.index >= GLSL_MAX_LOCALS)
                        return false;

                    if (parameter.qualifier == ShaderParameterQualifier::In)
                    {
                        copy_shader_value(
                            frame.locals[parameter.index], call_arguments[i]);
                    }
                    else
                    {
                        const uint16_t descriptor = instruction.output_targets[i];
                        if (descriptor == CALL_OUTPUT_TARGET_NONE)
                            return false;
                        ShaderValue *target_value = call_output_target_value(
                            descriptor, runtime, locals);
                        const size_t caller_function_index = call_depth == 0
                            ? module.main_function
                            : call_frames[call_depth - 1].function_index;
                        const ShaderType target_type =
                            call_output_target_declared_type(
                                module, caller_function_index, descriptor);
                        if (!target_value || target_type == ShaderType::Invalid)
                            return false;
                        if (target_value->type == ShaderType::Invalid)
                            set_zero_value(*target_value, target_type);
                        else if (target_value->type != target_type)
                            return false;

                        if (descriptor & CALL_OUTPUT_TARGET_FULL)
                        {
                            if (target_type != parameter.type)
                                return false;
                        }
                        else
                        {
                            if (!is_any_vector(target_type) ||
                                vector_type_for(
                                    component_count(parameter.type),
                                    vector_base_type(target_type)) != parameter.type)
                                return false;
                            for (size_t component = 0;
                                 component < component_count(parameter.type);
                                 ++component)
                            {
                                const uint8_t selector = static_cast<uint8_t>(
                                    (descriptor >> (3 + 2 * component)) & 0x3);
                                if (selector >= vector_dimension(target_type))
                                    return false;
                            }
                        }

                        frame.output_targets[i] = descriptor;
                        if (parameter.qualifier == ShaderParameterQualifier::InOut)
                            copy_shader_value(
                                frame.locals[parameter.index], call_arguments[i]);
                    }
                }

                ++call_depth;
                pc = function.entry_pc;
                break;
            }

            case ShaderOp::Return:
            {
                const bool returns_value =
                    instruction.type != ShaderType::Invalid;
                if (instruction.argument_count !=
                    static_cast<uint8_t>(returns_value ? 1 : 0))
                    return false;

                if (returns_value)
                {
                    if (!pop_value(stack, stack_size, result) ||
                        result.type != instruction.type)
                        return false;
                }

                if (call_depth == 0)
                {
                    if (returns_value || stack_size != 0)
                        return false;
                    main_returned = true;
                    break;
                }

                const CallFrame& frame = call_frames[call_depth - 1];
                if (stack_size != frame.stack_base ||
                    frame.function_index >= module.function_count)
                    return false;

                const ShaderFunction& function =
                    module.functions[frame.function_index];
                const size_t caller_function_index = call_depth == 1
                    ? module.main_function
                    : call_frames[call_depth - 2].function_index;
                ShaderValue *caller_locals = call_depth == 1
                    ? runtime.locals : call_frames[call_depth - 2].locals;
                for (size_t i = 0; i < function.parameter_count; ++i)
                {
                    const ShaderVariable& parameter = function.parameters[i];
                    if (parameter.qualifier == ShaderParameterQualifier::In)
                        continue;

                    const uint16_t descriptor = frame.output_targets[i];
                    if (descriptor == CALL_OUTPUT_TARGET_NONE ||
                        parameter.index >= GLSL_MAX_LOCALS ||
                        frame.locals[parameter.index].type != parameter.type)
                        return false;

                    ShaderValue *target_value = call_output_target_value(
                        descriptor, runtime, caller_locals);
                    const ShaderType target_type =
                        call_output_target_declared_type(
                            module, caller_function_index, descriptor);
                    if (!target_value || target_type == ShaderType::Invalid ||
                        target_value->type != target_type)
                        return false;

                    if (descriptor & CALL_OUTPUT_TARGET_FULL)
                    {
                        if (target_type != parameter.type)
                            return false;
                        copy_shader_value(
                            *target_value, frame.locals[parameter.index]);
                    }
                    else
                    {
                        if (!is_any_vector(target_type) ||
                            vector_type_for(
                                component_count(parameter.type),
                                vector_base_type(target_type)) != parameter.type)
                            return false;

                        for (size_t component = 0;
                             component < component_count(parameter.type);
                             ++component)
                        {
                            const uint8_t selector = static_cast<uint8_t>(
                                (descriptor >> (3 + 2 * component)) & 0x3);
                            if (selector >= vector_dimension(target_type))
                                return false;
                            target_value->data[selector] =
                                frame.locals[parameter.index].data[component];
                            if (selector < 4)
                            {
                                target_value->ddx[selector] =
                                    frame.locals[parameter.index].ddx[component];
                                target_value->ddy[selector] =
                                    frame.locals[parameter.index].ddy[component];
                            }
                        }
                    }
                }

                pc = frame.return_pc;
                --call_depth;
                if (returns_value && !push_value(stack, stack_size, result))
                    return false;
                break;
            }

            case ShaderOp::LogicalNot:
                if (!pop_value(stack, stack_size, a) ||
                    a.type != ShaderType::Bool)
                    return false;
                set_zero_value(result, ShaderType::Bool);
                result.data[0] = a.data[0] == 0.0f ? 1.0f : 0.0f;
                if (!push_value(stack, stack_size, result))
                    return false;
                break;

            case ShaderOp::Construct:
            {
                if (instruction.argument_count == 0 ||
                    instruction.argument_count > 4 ||
                    stack_size < instruction.argument_count)
                    return false;
                ShaderValue args[4];
                for (size_t i = instruction.argument_count; i > 0; --i)
                    if (!pop_value(stack, stack_size, args[i - 1]))
                        return false;

                set_zero_value(result, instruction.type);
                const size_t wanted = component_count(instruction.type);
                const size_t destination_dimension =
                    matrix_dimension(instruction.type);
                const size_t source_dimension =
                    instruction.argument_count == 1
                        ? matrix_dimension(args[0].type) : 0;
                if (instruction.argument_count == 1 &&
                    component_count(args[0].type) == 1)
                {
                    const float scalar = args[0].data[0];
                    if (destination_dimension != 0)
                    {
                        for (size_t i = 0; i < destination_dimension; ++i)
                            result.data[i * destination_dimension + i] = scalar;
                    }
                    else
                    {
                        for (size_t i = 0; i < wanted; ++i)
                        {
                            result.data[i] = convert_component(instruction.type, scalar);
                            if (i < 4)
                            {
                                result.ddx[i] = args[0].ddx[0];
                                result.ddy[i] = args[0].ddy[0];
                            }
                        }
                    }
                }
                else if (destination_dimension != 0 && source_dimension != 0)
                {
                    const size_t overlap = std::min(
                        destination_dimension, source_dimension);
                    for (size_t row = 0; row < destination_dimension; ++row)
                    {
                        for (size_t column = 0; column < destination_dimension; ++column)
                        {
                            if (row < overlap && column < overlap)
                                result.data[column * destination_dimension + row] =
                                    args[0].data[column * source_dimension + row];
                            else if (row == column)
                                result.data[column * destination_dimension + row] = 1.0f;
                        }
                    }
                }
                else
                {
                    size_t offset = 0;
                    for (size_t arg = 0; arg < instruction.argument_count; ++arg)
                    {
                        const size_t count = component_count(args[arg].type);
                        if (offset + count > wanted)
                            return false;
                        for (size_t i = 0; i < count; ++i)
                        {
                            result.data[offset] =
                                convert_component(instruction.type, args[arg].data[i]);
                            if (offset < 4 && i < 4)
                            {
                                result.ddx[offset] = args[arg].ddx[i];
                                result.ddy[offset] = args[arg].ddy[i];
                            }
                            ++offset;
                        }
                    }
                    if (offset != wanted)
                        return false;
                }
                if (instruction.type == ShaderType::Int ||
                    is_int_vector(instruction.type) ||
                    instruction.type == ShaderType::Bool ||
                    is_bool_vector(instruction.type))
                {
                    for (size_t i = 0; i < component_count(result.type); ++i)
                        result.data[i] = convert_component(instruction.type, result.data[i]);
                }
                if (!push_value(stack, stack_size, result))
                    return false;
                break;
            }

            case ShaderOp::Swizzle:
                if (!pop_value(stack, stack_size, a) ||
                    !is_any_vector(a.type) ||
                    instruction.argument_count == 0 ||
                    instruction.argument_count > 4)
                    return false;
                set_zero_value(result, instruction.type);
                for (size_t i = 0; i < instruction.argument_count; ++i)
                {
                    const size_t source = instruction.swizzle[i];
                    result.data[i] = a.data[source];
                    if (source < 4 && i < 4)
                    {
                        result.ddx[i] = a.ddx[source];
                        result.ddy[i] = a.ddy[source];
                    }
                }
                if (!push_value(stack, stack_size, result))
                    return false;
                break;

            case ShaderOp::Negate:
                if (!pop_value(stack, stack_size, a) ||
                    a.type != instruction.type)
                    return false;
                copy_shader_value(result, a);
                for (size_t i = 0; i < component_count(a.type); ++i)
                {
                    result.data[i] = -result.data[i];
                    if (i < 4)
                    {
                        result.ddx[i] = -result.ddx[i];
                        result.ddy[i] = -result.ddy[i];
                    }
                }
                if (!push_value(stack, stack_size, result))
                    return false;
                break;

            case ShaderOp::Add:
            case ShaderOp::Subtract:
            case ShaderOp::Multiply:
            case ShaderOp::Divide:
            {
                if (!pop_value(stack, stack_size, b) ||
                    !pop_value(stack, stack_size, a))
                    return false;

                ShaderType expected_type = ShaderType::Invalid;
                if (instruction.op == ShaderOp::Add ||
                    instruction.op == ShaderOp::Subtract)
                {
                    if (a.type == b.type &&
                        (a.type == ShaderType::Float || a.type == ShaderType::Int ||
                         is_vector(a.type) || is_int_vector(a.type) || is_matrix(a.type)))
                        expected_type = a.type;
                }
                else if (instruction.op == ShaderOp::Multiply)
                {
                    const size_t a_dimension = matrix_dimension(a.type);
                    const size_t b_dimension = matrix_dimension(b.type);
                    if (a_dimension != 0 && b.type == vector_type(a_dimension))
                        expected_type = b.type;
                    else if (b_dimension != 0 && a.type == vector_type(b_dimension))
                        expected_type = a.type;
                    else if (a_dimension != 0 && a_dimension == b_dimension)
                        expected_type = a.type;
                    else if (a_dimension != 0 && b.type == ShaderType::Float)
                        expected_type = a.type;
                    else if (b_dimension != 0 && a.type == ShaderType::Float)
                        expected_type = b.type;
                    else if (a.type == ShaderType::Float &&
                             (is_vector(b.type) || is_int_vector(b.type)))
                        expected_type = b.type;
                    else if ((is_vector(a.type) || is_int_vector(a.type)) &&
                             b.type == ShaderType::Float)
                        expected_type = a.type;
                    else if (a.type == b.type &&
                             (a.type == ShaderType::Float || a.type == ShaderType::Int ||
                               is_vector(a.type) || is_int_vector(a.type)))
                        expected_type = a.type;
                }
                else if (instruction.op == ShaderOp::Divide)
                {
                    if (is_matrix(a.type) && b.type == ShaderType::Float)
                        expected_type = a.type;
                    else if (a.type == ShaderType::Float && is_matrix(b.type))
                        expected_type = b.type;
                    else if ((is_vector(a.type) || is_int_vector(a.type) ||
                              is_matrix(a.type)) &&
                             b.type == ShaderType::Float)
                        expected_type = a.type;
                    else if (a.type == b.type &&
                             (a.type == ShaderType::Float || a.type == ShaderType::Int ||
                              is_vector(a.type) || is_int_vector(a.type) ||
                              is_matrix(a.type)))
                        expected_type = a.type;
                }

                if (expected_type == ShaderType::Invalid ||
                    instruction.type != expected_type)
                    return false;

                apply_binary(instruction.op, a, b, instruction.type, result);
                if (instruction.type == ShaderType::Int ||
                    is_int_vector(instruction.type) ||
                    instruction.type == ShaderType::Bool ||
                    is_bool_vector(instruction.type))
                    for (size_t i = 0; i < component_count(result.type); ++i)
                        result.data[i] = convert_component(instruction.type,
                            result.data[i]);
                if (!finite_value(result) || !push_value(stack, stack_size, result))
                    return false;
                break;
            }

            case ShaderOp::Equal:
            case ShaderOp::NotEqual:
            case ShaderOp::Less:
            case ShaderOp::LessEqual:
            case ShaderOp::Greater:
            case ShaderOp::GreaterEqual:
            case ShaderOp::LogicalAnd:
            case ShaderOp::LogicalOr:
            {
                if (!pop_value(stack, stack_size, b) ||
                    !pop_value(stack, stack_size, a) ||
                    a.type != b.type)
                    return false;

                set_zero_value(result, ShaderType::Bool);
                if (instruction.op == ShaderOp::LogicalAnd ||
                    instruction.op == ShaderOp::LogicalOr)
                {
                    if (a.type != ShaderType::Bool)
                        return false;
                    result.data[0] = instruction.op == ShaderOp::LogicalAnd
                        ? (a.data[0] != 0.0f && b.data[0] != 0.0f)
                        : (a.data[0] != 0.0f || b.data[0] != 0.0f);
                }
                else if (instruction.op == ShaderOp::Less ||
                         instruction.op == ShaderOp::LessEqual ||
                         instruction.op == ShaderOp::Greater ||
                         instruction.op == ShaderOp::GreaterEqual)
                {
                    if (a.type != ShaderType::Float &&
                        a.type != ShaderType::Int)
                        return false;
                    if (instruction.op == ShaderOp::Less)
                        result.data[0] = a.data[0] < b.data[0];
                    else if (instruction.op == ShaderOp::LessEqual)
                        result.data[0] = a.data[0] <= b.data[0];
                    else if (instruction.op == ShaderOp::Greater)
                        result.data[0] = a.data[0] > b.data[0];
                    else
                        result.data[0] = a.data[0] >= b.data[0];
                }
                else
                {
                    bool equal = true;
                    const size_t components = component_count(a.type);
                    for (size_t i = 0; i < components; ++i)
                        if (a.data[i] != b.data[i])
                            equal = false;
                    result.data[0] = instruction.op == ShaderOp::Equal
                        ? equal : !equal;
                }
                if (!push_value(stack, stack_size, result))
                    return false;
                break;
            }

            case ShaderOp::Dot:
                if (!pop_value(stack, stack_size, b) ||
                    !pop_value(stack, stack_size, a) ||
                    a.type != b.type || !is_vector(a.type))
                    return false;
                set_zero_value(result, ShaderType::Float);
                for (size_t i = 0; i < component_count(a.type); ++i)
                    result.data[0] += a.data[i] * b.data[i];
                if (!push_value(stack, stack_size, result))
                    return false;
                break;

            case ShaderOp::Length:
                if (!pop_value(stack, stack_size, a) || !is_vector(a.type))
                    return false;
                set_zero_value(result, ShaderType::Float);
                for (size_t i = 0; i < component_count(a.type); ++i)
                    result.data[0] += a.data[i] * a.data[i];
                result.data[0] = sqrtf(result.data[0]);
                if (!push_value(stack, stack_size, result))
                    return false;
                break;

            case ShaderOp::Normalize:
                if (!pop_value(stack, stack_size, a) || !is_vector(a.type))
                    return false;
                copy_shader_value(result, a);
                {
                    float length_squared = 0.0f;
                    for (size_t i = 0; i < component_count(a.type); ++i)
                        length_squared += a.data[i] * a.data[i];
                    const float inverse_length = length_squared > 0.0f
                        ? 1.0f / sqrtf(length_squared) : 0.0f;
                    for (size_t i = 0; i < component_count(a.type); ++i)
                        result.data[i] *= inverse_length;
                }
                if (!push_value(stack, stack_size, result))
                    return false;
                break;

            case ShaderOp::Mix:
                if (!pop_value(stack, stack_size, c) ||
                    !pop_value(stack, stack_size, b) ||
                    !pop_value(stack, stack_size, a) ||
                    a.type != b.type ||
                    (c.type != ShaderType::Float && c.type != a.type))
                    return false;
                set_zero_value(result, a.type);
                for (size_t i = 0; i < component_count(a.type); ++i)
                {
                    const float factor = c.data[component_count(c.type) == 1 ? 0 : i];
                    result.data[i] = a.data[i] * (1.0f - factor) +
                                     b.data[i] * factor;
                    if (i < 4)
                    {
                        const float factor_dx =
                            c.ddx[component_count(c.type) == 1 ? 0 : i];
                        const float factor_dy =
                            c.ddy[component_count(c.type) == 1 ? 0 : i];
                        result.ddx[i] = a.ddx[i] * (1.0f - factor) +
                            b.ddx[i] * factor + (b.data[i] - a.data[i]) * factor_dx;
                        result.ddy[i] = a.ddy[i] * (1.0f - factor) +
                            b.ddy[i] * factor + (b.data[i] - a.data[i]) * factor_dy;
                    }
                }
                if (!finite_value(result) || !push_value(stack, stack_size, result))
                    return false;
                break;

            case ShaderOp::Clamp:
                if (!pop_value(stack, stack_size, c) ||
                    !pop_value(stack, stack_size, b) ||
                    !pop_value(stack, stack_size, a) ||
                    (b.type != a.type && b.type != ShaderType::Float) ||
                    (c.type != a.type && c.type != ShaderType::Float) ||
                    (b.type != c.type &&
                     !(is_vector(a.type) && b.type == ShaderType::Float &&
                       c.type == ShaderType::Float)))
                    return false;
                set_zero_value(result, a.type);
                for (size_t i = 0; i < component_count(a.type); ++i)
                {
                    const float lower = b.data[component_count(b.type) == 1 ? 0 : i];
                    const float upper = c.data[component_count(c.type) == 1 ? 0 : i];
                    result.data[i] = std::max(lower,
                        std::min(a.data[i], upper));
                }
                if (!push_value(stack, stack_size, result))
                    return false;
                break;

            case ShaderOp::Minimum:
            case ShaderOp::Maximum:
                if (!pop_value(stack, stack_size, b) ||
                    !pop_value(stack, stack_size, a) ||
                    (a.type != b.type &&
                     !(is_vector(a.type) && b.type == ShaderType::Float)))
                    return false;
                set_zero_value(result, a.type);
                for (size_t i = 0; i < component_count(a.type); ++i)
                {
                    const float rhs = b.data[component_count(b.type) == 1 ? 0 : i];
                    result.data[i] = instruction.op == ShaderOp::Minimum
                        ? std::min(a.data[i], rhs)
                        : std::max(a.data[i], rhs);
                }
                if (!finite_value(result) || !push_value(stack, stack_size, result))
                    return false;
                break;

            case ShaderOp::Builtin:
            {
                if (instruction.argument_count == 0 ||
                    instruction.argument_count > 3 ||
                    stack_size < instruction.argument_count)
                    return false;

                ShaderValue args[3];
                for (size_t i = instruction.argument_count; i > 0; --i)
                    if (!pop_value(stack, stack_size, args[i - 1]))
                        return false;

                const BuiltinFunction function =
                    static_cast<BuiltinFunction>(instruction.index);
                set_zero_value(result, instruction.type);

                const auto component = [](const ShaderValue& value, size_t index)
                {
                    return value.data[component_count(value.type) == 1 ? 0 : index];
                };

                switch (function)
                {
                    case BuiltinFunction::Sin:
                    case BuiltinFunction::Cos:
                    case BuiltinFunction::Tan:
                    case BuiltinFunction::Asin:
                    case BuiltinFunction::Acos:
                    case BuiltinFunction::Atan:
                    case BuiltinFunction::Exp:
                    case BuiltinFunction::Log:
                    case BuiltinFunction::Exp2:
                    case BuiltinFunction::Log2:
                    case BuiltinFunction::Sqrt:
                    case BuiltinFunction::InverseSqrt:
                    case BuiltinFunction::Abs:
                    case BuiltinFunction::Sign:
                    case BuiltinFunction::Floor:
                    case BuiltinFunction::Ceil:
                    case BuiltinFunction::Fract:
                    case BuiltinFunction::Radians:
                    case BuiltinFunction::Degrees:
                        if (instruction.argument_count != 1 ||
                            args[0].type != instruction.type ||
                            !is_gen_type(args[0].type))
                            return false;
                        for (size_t i = 0; i < component_count(instruction.type); ++i)
                        {
                            const float value = args[0].data[i];
                            float output = 0.0f;
                            switch (function)
                            {
                                case BuiltinFunction::Sin: output = sinf(value); break;
                                case BuiltinFunction::Cos: output = cosf(value); break;
                                case BuiltinFunction::Tan: output = tanf(value); break;
                                case BuiltinFunction::Asin: output = asinf(value); break;
                                case BuiltinFunction::Acos: output = acosf(value); break;
                                case BuiltinFunction::Atan: output = atanf(value); break;
                                case BuiltinFunction::Exp: output = expf(value); break;
                                case BuiltinFunction::Log: output = logf(value); break;
                                case BuiltinFunction::Exp2: output = exp2f(value); break;
                                case BuiltinFunction::Log2: output = log2f(value); break;
                                case BuiltinFunction::Sqrt: output = sqrtf(value); break;
                                case BuiltinFunction::InverseSqrt:
                                    output = value > 0.0f ? 1.0f / sqrtf(value) : 0.0f;
                                    break;
                                case BuiltinFunction::Abs: output = fabsf(value); break;
                                case BuiltinFunction::Sign:
                                    output = value > 0.0f ? 1.0f : value < 0.0f ? -1.0f : 0.0f;
                                    break;
                                case BuiltinFunction::Floor: output = floorf(value); break;
                                case BuiltinFunction::Ceil: output = ceilf(value); break;
                                case BuiltinFunction::Fract: output = value - floorf(value); break;
                                case BuiltinFunction::Radians:
                                    output = value * 0.01745329251994329577f;
                                    break;
                                case BuiltinFunction::Degrees:
                                    output = value * 57.295779513082320876f;
                                    break;
                                default: return false;
                            }
                            result.data[i] = output;
                        }
                        break;

                    case BuiltinFunction::Atan2:
                    case BuiltinFunction::Pow:
                    case BuiltinFunction::Mod:
                    case BuiltinFunction::Step:
                    {
                        if (instruction.argument_count != 2)
                            return false;
                        if (function == BuiltinFunction::Step)
                        {
                            if (args[1].type != instruction.type ||
                                (args[0].type != instruction.type &&
                                 !(is_vector(instruction.type) &&
                                   args[0].type == ShaderType::Float)))
                                return false;
                        }
                        else if (args[0].type != instruction.type ||
                                 (args[1].type != instruction.type &&
                                  !(is_vector(instruction.type) &&
                                    args[1].type == ShaderType::Float)))
                        {
                            return false;
                        }

                        for (size_t i = 0; i < component_count(instruction.type); ++i)
                        {
                            const float x = function == BuiltinFunction::Step
                                ? component(args[1], i) : args[0].data[i];
                            const float y = function == BuiltinFunction::Step
                                ? component(args[0], i) : component(args[1], i);
                            float output = 0.0f;
                            switch (function)
                            {
                                case BuiltinFunction::Atan2: output = atan2f(x, y); break;
                                case BuiltinFunction::Pow: output = powf(x, y); break;
                                case BuiltinFunction::Mod:
                                    output = y == 0.0f ? 0.0f :
                                        x - y * floorf(x / y);
                                    break;
                                case BuiltinFunction::Step: output = x < y ? 0.0f : 1.0f; break;
                                default: return false;
                            }
                            result.data[i] = output;
                        }
                        break;
                    }

                    case BuiltinFunction::Distance:
                        if (instruction.argument_count != 2 ||
                            !is_vector(args[0].type) ||
                            args[0].type != args[1].type ||
                            instruction.type != ShaderType::Float)
                            return false;
                        for (size_t i = 0; i < component_count(args[0].type); ++i)
                        {
                            const float delta = args[0].data[i] - args[1].data[i];
                            result.data[0] += delta * delta;
                        }
                        result.data[0] = sqrtf(result.data[0]);
                        break;

                    case BuiltinFunction::Cross:
                        if (instruction.argument_count != 2 ||
                            args[0].type != ShaderType::Vec3 ||
                            args[1].type != ShaderType::Vec3 ||
                            instruction.type != ShaderType::Vec3)
                            return false;
                        result.data[0] = args[0].data[1] * args[1].data[2] -
                                         args[0].data[2] * args[1].data[1];
                        result.data[1] = args[0].data[2] * args[1].data[0] -
                                         args[0].data[0] * args[1].data[2];
                        result.data[2] = args[0].data[0] * args[1].data[1] -
                                         args[0].data[1] * args[1].data[0];
                        break;

                    case BuiltinFunction::FaceForward:
                        if (instruction.argument_count != 3 ||
                            !is_vector(args[0].type) ||
                            args[0].type != args[1].type ||
                            args[0].type != args[2].type ||
                            instruction.type != args[0].type)
                            return false;
                        {
                            float dot = 0.0f;
                            for (size_t i = 0; i < component_count(args[0].type); ++i)
                                dot += args[2].data[i] * args[1].data[i];
                            const float sign = dot < 0.0f ? 1.0f : -1.0f;
                            for (size_t i = 0; i < component_count(args[0].type); ++i)
                                result.data[i] = args[0].data[i] * sign;
                        }
                        break;

                    case BuiltinFunction::Reflect:
                        if (instruction.argument_count != 2 ||
                            !is_vector(args[0].type) ||
                            args[0].type != args[1].type ||
                            instruction.type != args[0].type)
                            return false;
                        {
                            float dot = 0.0f;
                            for (size_t i = 0; i < component_count(args[0].type); ++i)
                                dot += args[1].data[i] * args[0].data[i];
                            for (size_t i = 0; i < component_count(args[0].type); ++i)
                                result.data[i] = args[0].data[i] -
                                    2.0f * dot * args[1].data[i];
                        }
                        break;

                    case BuiltinFunction::Refract:
                        if (instruction.argument_count != 3 ||
                            !is_vector(args[0].type) ||
                            args[0].type != args[1].type ||
                            args[2].type != ShaderType::Float ||
                            instruction.type != args[0].type)
                            return false;
                        {
                            float dot = 0.0f;
                            for (size_t i = 0; i < component_count(args[0].type); ++i)
                                dot += args[1].data[i] * args[0].data[i];
                            const float eta = args[2].data[0];
                            const float k = 1.0f - eta * eta * (1.0f - dot * dot);
                            if (k < 0.0f)
                            {
                                set_zero_value(result, instruction.type);
                            }
                            else
                            {
                                const float factor = eta * dot + sqrtf(k);
                                for (size_t i = 0; i < component_count(args[0].type); ++i)
                                    result.data[i] = eta * args[0].data[i] -
                                        factor * args[1].data[i];
                            }
                        }
                        break;

                    case BuiltinFunction::SmoothStep:
                        if (instruction.argument_count != 3 ||
                            (args[0].type != instruction.type &&
                             args[0].type != ShaderType::Float) ||
                            (args[1].type != instruction.type &&
                             args[1].type != ShaderType::Float) ||
                            args[2].type != instruction.type)
                            return false;
                        for (size_t i = 0; i < component_count(instruction.type); ++i)
                        {
                            const float edge0 = component(args[0], i);
                            const float edge1 = component(args[1], i);
                            const float x = args[2].data[i];
                            const float denominator = edge1 - edge0;
                            const float t = denominator == 0.0f ? 0.0f :
                                std::max(0.0f, std::min(1.0f,
                                    (x - edge0) / denominator));
                            result.data[i] = t * t * (3.0f - 2.0f * t);
                        }
                        break;

                    case BuiltinFunction::LessThan:
                    case BuiltinFunction::LessThanEqual:
                    case BuiltinFunction::GreaterThan:
                    case BuiltinFunction::GreaterThanEqual:
                    case BuiltinFunction::EqualVector:
                    case BuiltinFunction::NotEqualVector:
                        if (instruction.argument_count != 2 ||
                            args[0].type != args[1].type ||
                            !is_any_vector(args[0].type) ||
                            vector_dimension(args[0].type) !=
                                vector_dimension(instruction.type) ||
                            !is_bool_vector(instruction.type))
                            return false;
                        for (size_t i = 0; i < vector_dimension(args[0].type); ++i)
                        {
                            const float left = args[0].data[i];
                            const float right = args[1].data[i];
                            bool comparison = false;
                            switch (function)
                            {
                                case BuiltinFunction::LessThan: comparison = left < right; break;
                                case BuiltinFunction::LessThanEqual: comparison = left <= right; break;
                                case BuiltinFunction::GreaterThan: comparison = left > right; break;
                                case BuiltinFunction::GreaterThanEqual: comparison = left >= right; break;
                                case BuiltinFunction::EqualVector: comparison = left == right; break;
                                case BuiltinFunction::NotEqualVector: comparison = left != right; break;
                                default: return false;
                            }
                            result.data[i] = comparison ? 1.0f : 0.0f;
                        }
                        break;

                    case BuiltinFunction::Any:
                    case BuiltinFunction::All:
                        if (instruction.argument_count != 1 ||
                            !is_bool_vector(args[0].type) ||
                            instruction.type != ShaderType::Bool)
                            return false;
                        {
                            bool value = function == BuiltinFunction::All;
                            for (size_t i = 0; i < vector_dimension(args[0].type); ++i)
                            {
                                if (function == BuiltinFunction::Any)
                                    value = value || args[0].data[i] != 0.0f;
                                else
                                    value = value && args[0].data[i] != 0.0f;
                            }
                            result.data[0] = value ? 1.0f : 0.0f;
                        }
                        break;

                    case BuiltinFunction::NotVector:
                        if (instruction.argument_count != 1 ||
                            !is_bool_vector(args[0].type) ||
                            args[0].type != instruction.type)
                            return false;
                        for (size_t i = 0; i < vector_dimension(args[0].type); ++i)
                            result.data[i] = args[0].data[i] == 0.0f ? 1.0f : 0.0f;
                        break;

                    case BuiltinFunction::MatrixCompMult:
                        if (instruction.argument_count != 2 ||
                            !is_matrix(args[0].type) ||
                            args[0].type != args[1].type ||
                            instruction.type != args[0].type)
                            return false;
                        for (size_t i = 0; i < component_count(instruction.type); ++i)
                            result.data[i] = args[0].data[i] * args[1].data[i];
                        break;
                }

                if (!finite_value(result) || !push_value(stack, stack_size, result))
                    return false;
                break;
            }

            case ShaderOp::Texture2D:
            {
                // The two operands are the top stack entries and the result
                // replaces both of them. Reuse the sampler slot for the vec4
                // instead of copying two operands out and a result back in.
                if (stack_size < 2 || !texture_sample)
                    return false;
                ShaderValue& sampler_value = stack[stack_size - 2];
                const ShaderValue& coordinate = stack[stack_size - 1];
                if (sampler_value.type != ShaderType::Sampler2D ||
                    coordinate.type != ShaderType::Vec2)
                    return false;

                const int texture_unit =
                    static_cast<int>(sampler_value.data[0]);
                sampler_value.type = ShaderType::Vec4;
                if (!texture_sample(texture_unit,
                                    coordinate.data[0], coordinate.data[1],
                                    coordinate.ddx[0], coordinate.ddx[1],
                                    coordinate.ddy[0], coordinate.ddy[1],
                                    sampler_value.data, user_data))
                    return false;

                // A texture lookup result has no propagated derivatives in
                // this VM, matching the previous zero-initialized result.
                for (size_t i = 0; i < 4; ++i)
                {
                    sampler_value.ddx[i] = 0.0f;
                    sampler_value.ddy[i] = 0.0f;
                }
                --stack_size;
                break;
            }

            case ShaderOp::LoadLocalIndexed:
            {
                size_t slot = 0;
                if (!pop_value(stack, stack_size, a) ||
                    !array_element_slot(instruction, a, slot) ||
                    locals[slot].type != instruction.type ||
                    !push_value(stack, stack_size, locals[slot]))
                    return false;
                break;
            }

            case ShaderOp::StoreLocalIndexed:
            {
                size_t slot = 0;
                if (!pop_value(stack, stack_size, a) ||
                    !pop_value(stack, stack_size, b) ||
                    a.type != instruction.type ||
                    !array_element_slot(instruction, b, slot))
                    return false;
                copy_shader_value(locals[slot], a);
                break;
            }

            case ShaderOp::LoadUniformIndexed:
            {
                size_t slot = 0;
                if (!pop_value(stack, stack_size, a) ||
                    !array_element_slot(instruction, a, slot, GLSL_MAX_UNIFORMS) ||
                    !push_value(stack, stack_size, runtime.uniforms[slot]))
                    return false;
                break;
            }
        }
    }

    const bool reached_main_end =
        call_depth == 0 && pc == module.instruction_count;
    return (main_returned || reached_main_end) &&
           call_depth == 0 && stack_size == 0;
}

} // namespace fxgl
