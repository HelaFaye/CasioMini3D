#include "glsl.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
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
            token.number = std::strtof(token.text, &end);
            if (!end || *end != '\0' || !std::isfinite(token.number))
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

        if (std::strchr("{}();,=.+-*/.", c))
        {
            token.kind = TokenKind::Punctuation;
            token.text[0] = c;
            token.text[1] = '\0';
            ++offset_;
            return token;
        }

        token.kind = TokenKind::Invalid;
        token.text[0] = c;
        token.text[1] = '\0';
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
    FragmentColor
};

struct ResolvedVariable
{
    Storage storage;
    ShaderType type;
    uint8_t index;
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
          failed_(false), recursion_depth_(0)
    {
        advance();
    }

    bool parse()
    {
        while (!failed_ && current_.kind != TokenKind::End &&
               !is_identifier("void"))
        {
            if (is_identifier("precision"))
                parse_precision();
            else
                parse_global_declaration();
        }

        if (!failed_)
            parse_main();

        if (!failed_ && current_.kind != TokenKind::End)
            fail("unexpected tokens after main function");
        return !failed_;
    }

private:
    void advance()
    {
        current_ = lexer_.next();
        if (current_.kind == TokenKind::Invalid && !failed_)
            fail("invalid token");
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

        if (accept_punctuation('['))
        {
            fail("arrays are not supported yet");
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

        if (*count >= capacity)
        {
            fail("shader variable limit exceeded");
            return;
        }

        ShaderVariable& variable = variables[*count];
        std::snprintf(variable.name, sizeof(variable.name), "%s", name);
        variable.type = type;
        variable.index = static_cast<uint8_t>(*count);
        ++(*count);
    }

    ResolvedVariable lookup(const char *name) const
    {
        if (stage_ == ShaderStage::Vertex &&
            std::strcmp(name, "gl_Position") == 0)
            return {Storage::Position, ShaderType::Vec4, 0};
        if (stage_ == ShaderStage::Fragment &&
            std::strcmp(name, "gl_FragColor") == 0)
            return {Storage::FragmentColor, ShaderType::Vec4, 0};

        const ShaderVariable *variable = find_variable(
            module_.attributes, module_.attribute_count, name);
        if (variable)
            return {Storage::Attribute, variable->type, variable->index};

        variable = find_variable(
            module_.uniforms, module_.uniform_count, name);
        if (variable)
            return {Storage::Uniform, variable->type, variable->index};

        variable = find_variable(
            module_.varyings, module_.varying_count, name);
        if (variable)
            return {Storage::Varying, variable->type, variable->index};

        variable = find_variable(
            module_.locals, module_.local_count, name);
        if (variable)
            return {Storage::Local, variable->type, variable->index};

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

    void parse_main()
    {
        expect_identifier("void");
        expect_identifier("main");
        expect_punctuation('(');
        expect_punctuation(')');
        expect_punctuation('{');

        while (!failed_ && !is_punctuation('}') &&
               current_.kind != TokenKind::End)
            parse_statement();

        expect_punctuation('}');
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
        const ResolvedVariable target = lookup(name);
        if (target.type == ShaderType::Invalid)
        {
            fail("assignment target is not declared");
            return;
        }
        if (target.storage == Storage::Attribute ||
            target.storage == Storage::Uniform ||
            (target.storage == Storage::FragmentColor && stage_ == ShaderStage::Vertex) ||
            (target.storage == Storage::Position && stage_ == ShaderStage::Fragment) ||
            (target.storage == Storage::Varying && stage_ == ShaderStage::Fragment))
        {
            fail("assignment target is read-only or unavailable in this shader stage");
            return;
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
        emit_assignment_store(target);
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
        if (target.storage != Storage::Local)
        {
            fail("for-loop increment target must be a local variable");
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
        parse_nested_statement();
        if (failed_)
            return;

        const size_t jump_index = module_.instruction_count;
        emit_simple(ShaderOp::Jump, ShaderType::Invalid);
        patch_branch_target(jump_index, condition_start);
        patch_branch_target(branch_index, module_.instruction_count);
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

        parse_nested_statement();
        if (failed_)
            return;

        for (size_t i = 0; i < increment_count; ++i)
            emit(increment_code[i]);
        const size_t jump_index = module_.instruction_count;
        emit_simple(ShaderOp::Jump, ShaderType::Invalid);
        patch_branch_target(jump_index, condition_start);
        patch_branch_target(branch_index, module_.instruction_count);
    }

    void parse_statement()
    {
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

        char name[GLSL_NAME_BYTES];
        std::snprintf(name, sizeof(name), "%s", current_.text);
        advance();

        if (lookup(name).type != ShaderType::Invalid)
        {
            fail("duplicate local variable");
            return;
        }
        if (module_.local_count >= GLSL_MAX_LOCALS)
        {
            fail("local variable limit exceeded");
            return;
        }

        ShaderVariable& variable = module_.locals[module_.local_count];
        std::snprintf(variable.name, sizeof(variable.name), "%s", name);
        variable.type = type;
        variable.index = static_cast<uint8_t>(module_.local_count);
        ++module_.local_count;

        if (accept_punctuation('='))
        {
            const ShaderType expression_type = parse_expression(1);
            if (expression_type != type)
            {
                fail("local initializer type mismatch");
                return;
            }
            emit_simple(ShaderOp::StoreLocal, type, variable.index);
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
        }

        --recursion_depth_;
        return left_type;
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
             variable.storage == Storage::FragmentColor) &&
            ((variable.storage == Storage::Position && stage_ != ShaderStage::Vertex) ||
             (variable.storage == Storage::FragmentColor && stage_ != ShaderStage::Fragment)))
        {
            fail("built-in variable is unavailable in this shader stage");
            result_type = ShaderType::Invalid;
            return;
        }

        ShaderOp op = ShaderOp::LoadLocal;
        switch (variable.storage)
        {
            case Storage::Attribute: op = ShaderOp::LoadAttribute; break;
            case Storage::Uniform: op = ShaderOp::LoadUniform; break;
            case Storage::Varying: op = ShaderOp::LoadVarying; break;
            case Storage::Local: op = ShaderOp::LoadLocal; break;
            default:
                fail("output built-ins cannot be used as expressions");
                result_type = ShaderType::Invalid;
                return;
        }
        emit_simple(op, variable.type, variable.index);
        result_type = variable.type;
        parse_swizzle(result_type);
    }

    ShaderType parse_call(const char *name)
    {
        ShaderType arguments[4] = {};
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
                arguments[argument_count++] = parse_expression(1);
                if (failed_)
                    return ShaderType::Invalid;
                if (accept_punctuation(')'))
                    break;
                if (!expect_punctuation(','))
                    return ShaderType::Invalid;
            }
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

        fail("unsupported built-in function");
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
    }

    ShaderStage stage_;
    Lexer lexer_;
    ShaderModule& module_;
    Token current_;
    bool failed_;
    size_t recursion_depth_;
};

static ShaderValue zero_value(ShaderType type)
{
    ShaderValue value = {};
    value.type = type;
    return value;
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
    value = stack[--stack_size];
    return true;
}

static bool push_value(
    ShaderValue stack[GLSL_MAX_STACK],
    size_t& stack_size,
    const ShaderValue& value)
{
    if (stack_size >= GLSL_MAX_STACK)
        return false;
    stack[stack_size++] = value;
    return true;
}

static void apply_binary(
    ShaderOp op,
    const ShaderValue& left,
    const ShaderValue& right,
    ShaderType result_type,
    ShaderValue& result)
{
    result = zero_value(result_type);
    const size_t count = component_count(result_type);

    const size_t left_dimension = matrix_dimension(left.type);
    const size_t right_dimension = matrix_dimension(right.type);

    if (op == ShaderOp::Multiply &&
        left_dimension != 0 && right.type == vector_type(left_dimension))
    {
        result = zero_value(right.type);
        for (size_t row = 0; row < left_dimension; ++row)
            for (size_t column = 0; column < left_dimension; ++column)
                result.data[row] +=
                    left.data[column * left_dimension + row] * right.data[column];
        return;
    }

    if (op == ShaderOp::Multiply &&
        left.type == vector_type(right_dimension) && right_dimension != 0)
    {
        result = zero_value(left.type);
        for (size_t column = 0; column < right_dimension; ++column)
            for (size_t row = 0; row < right_dimension; ++row)
                result.data[column] +=
                    left.data[row] * right.data[column * right_dimension + row];
        return;
    }

    if (op == ShaderOp::Multiply &&
        left_dimension != 0 && left_dimension == right_dimension)
    {
        result = zero_value(left.type);
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
    for (size_t i = 0; i < count; ++i)
    {
        const float a = left.data[left_components == 1 ? 0 : i];
        const float b = right.data[right_components == 1 ? 0 : i];
        switch (op)
        {
            case ShaderOp::Add: result.data[i] = a + b; break;
            case ShaderOp::Subtract: result.data[i] = a - b; break;
            case ShaderOp::Multiply: result.data[i] = a * b; break;
            case ShaderOp::Divide:
                result.data[i] = b == 0.0f ? 0.0f : a / b;
                break;
            default: break;
        }
    }
}

static bool finite_value(const ShaderValue& value)
{
    const size_t count = component_count(value.type);
    for (size_t i = 0; i < count; ++i)
        if (!std::isfinite(value.data[i]))
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

bool execute_glsl(
    const ShaderModule& module,
    ShaderRuntime& runtime,
    TextureSampleFunction texture_sample,
    void *user_data)
{
    if (!module.compiled)
        return false;

    ShaderValue stack[GLSL_MAX_STACK] = {};
    size_t stack_size = 0;
    size_t pc = 0;
    size_t executed_steps = 0;

    while (pc < module.instruction_count)
    {
        if (++executed_steps > GLSL_MAX_EXECUTION_STEPS)
            return false;
        const ShaderInstruction& instruction = module.instructions[pc++];
        ShaderValue a = {};
        ShaderValue b = {};
        ShaderValue c = {};
        ShaderValue result = {};

        switch (instruction.op)
        {
            case ShaderOp::PushFloat:
            case ShaderOp::PushInt:
                result = zero_value(instruction.type);
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
                if (instruction.index >= GLSL_MAX_VARYINGS ||
                    !push_value(stack, stack_size, runtime.varyings[instruction.index]))
                    return false;
                break;
            case ShaderOp::LoadLocal:
                if (instruction.index >= GLSL_MAX_LOCALS ||
                    !push_value(stack, stack_size, runtime.locals[instruction.index]))
                    return false;
                break;

            case ShaderOp::StoreVarying:
                if (!pop_value(stack, stack_size, a) ||
                    instruction.index >= GLSL_MAX_VARYINGS ||
                    a.type != instruction.type)
                    return false;
                runtime.varyings[instruction.index] = a;
                break;
            case ShaderOp::StoreLocal:
                if (!pop_value(stack, stack_size, a) ||
                    instruction.index >= GLSL_MAX_LOCALS ||
                    a.type != instruction.type)
                    return false;
                runtime.locals[instruction.index] = a;
                break;
            case ShaderOp::StorePosition:
                if (!pop_value(stack, stack_size, a) ||
                    a.type != ShaderType::Vec4)
                    return false;
                runtime.position = a;
                break;
            case ShaderOp::StoreFragmentColor:
                if (!pop_value(stack, stack_size, a) ||
                    a.type != ShaderType::Vec4)
                    return false;
                runtime.fragment_color = a;
                break;

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

            case ShaderOp::LogicalNot:
                if (!pop_value(stack, stack_size, a) ||
                    a.type != ShaderType::Bool)
                    return false;
                result = zero_value(ShaderType::Bool);
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
                ShaderValue args[4] = {};
                for (size_t i = instruction.argument_count; i > 0; --i)
                    if (!pop_value(stack, stack_size, args[i - 1]))
                        return false;

                result = zero_value(instruction.type);
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
                            result.data[i] = convert_component(instruction.type, scalar);
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
                result = zero_value(instruction.type);
                for (size_t i = 0; i < instruction.argument_count; ++i)
                    result.data[i] = a.data[instruction.swizzle[i]];
                if (!push_value(stack, stack_size, result))
                    return false;
                break;

            case ShaderOp::Negate:
                if (!pop_value(stack, stack_size, a) ||
                    a.type != instruction.type)
                    return false;
                result = a;
                for (size_t i = 0; i < component_count(a.type); ++i)
                    result.data[i] = -result.data[i];
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

                result = zero_value(ShaderType::Bool);
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
                result = zero_value(ShaderType::Float);
                for (size_t i = 0; i < component_count(a.type); ++i)
                    result.data[0] += a.data[i] * b.data[i];
                if (!push_value(stack, stack_size, result))
                    return false;
                break;

            case ShaderOp::Length:
                if (!pop_value(stack, stack_size, a) || !is_vector(a.type))
                    return false;
                result = zero_value(ShaderType::Float);
                for (size_t i = 0; i < component_count(a.type); ++i)
                    result.data[0] += a.data[i] * a.data[i];
                result.data[0] = std::sqrt(result.data[0]);
                if (!push_value(stack, stack_size, result))
                    return false;
                break;

            case ShaderOp::Normalize:
                if (!pop_value(stack, stack_size, a) || !is_vector(a.type))
                    return false;
                result = a;
                {
                    float length_squared = 0.0f;
                    for (size_t i = 0; i < component_count(a.type); ++i)
                        length_squared += a.data[i] * a.data[i];
                    const float inverse_length = length_squared > 0.0f
                        ? 1.0f / std::sqrt(length_squared) : 0.0f;
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
                result = zero_value(a.type);
                for (size_t i = 0; i < component_count(a.type); ++i)
                {
                    const float factor = c.data[component_count(c.type) == 1 ? 0 : i];
                    result.data[i] = a.data[i] * (1.0f - factor) +
                                     b.data[i] * factor;
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
                result = zero_value(a.type);
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
                result = zero_value(a.type);
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

                ShaderValue args[3] = {};
                for (size_t i = instruction.argument_count; i > 0; --i)
                    if (!pop_value(stack, stack_size, args[i - 1]))
                        return false;

                const BuiltinFunction function =
                    static_cast<BuiltinFunction>(instruction.index);
                result = zero_value(instruction.type);

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
                                case BuiltinFunction::Sin: output = std::sin(value); break;
                                case BuiltinFunction::Cos: output = std::cos(value); break;
                                case BuiltinFunction::Tan: output = std::tan(value); break;
                                case BuiltinFunction::Asin: output = std::asin(value); break;
                                case BuiltinFunction::Acos: output = std::acos(value); break;
                                case BuiltinFunction::Atan: output = std::atan(value); break;
                                case BuiltinFunction::Exp: output = std::exp(value); break;
                                case BuiltinFunction::Log: output = std::log(value); break;
                                case BuiltinFunction::Exp2: output = std::exp2(value); break;
                                case BuiltinFunction::Log2: output = std::log2(value); break;
                                case BuiltinFunction::Sqrt: output = std::sqrt(value); break;
                                case BuiltinFunction::InverseSqrt:
                                    output = value > 0.0f ? 1.0f / std::sqrt(value) : 0.0f;
                                    break;
                                case BuiltinFunction::Abs: output = std::fabs(value); break;
                                case BuiltinFunction::Sign:
                                    output = value > 0.0f ? 1.0f : value < 0.0f ? -1.0f : 0.0f;
                                    break;
                                case BuiltinFunction::Floor: output = std::floor(value); break;
                                case BuiltinFunction::Ceil: output = std::ceil(value); break;
                                case BuiltinFunction::Fract: output = value - std::floor(value); break;
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
                                case BuiltinFunction::Atan2: output = std::atan2(x, y); break;
                                case BuiltinFunction::Pow: output = std::pow(x, y); break;
                                case BuiltinFunction::Mod:
                                    output = y == 0.0f ? 0.0f :
                                        x - y * std::floor(x / y);
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
                        result.data[0] = std::sqrt(result.data[0]);
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
                                result = zero_value(instruction.type);
                            }
                            else
                            {
                                const float factor = eta * dot + std::sqrt(k);
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
                if (!pop_value(stack, stack_size, b) ||
                    !pop_value(stack, stack_size, a) ||
                    a.type != ShaderType::Sampler2D ||
                    b.type != ShaderType::Vec2 ||
                    !texture_sample)
                    return false;
                result = zero_value(ShaderType::Vec4);
                if (!texture_sample(static_cast<int>(a.data[0]),
                                    b.data[0], b.data[1],
                                    result.data, user_data))
                    return false;
                if (!push_value(stack, stack_size, result))
                    return false;
                break;
        }
    }

    return stack_size == 0;
}

} // namespace fxgl
