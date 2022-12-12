#ifndef INTERPRETER_INTERP_HH
#define INTERPRETER_INTERP_HH

/// Make sure we don’t try to include this header from C.
#ifndef __cplusplus
#   error "This header is C++ only. Use <interpreter/interp.h> instead."
#endif

#include <interpreter/interp.h>
#include <interpreter/utils.hh>
#include <functional>
#include <unordered_map>
#include <vector>
#include <variant>

/// Don’t want to deal w/ this rn.
static_assert(sizeof(void*) == 8 && sizeof(usz) == 8, "Only 64-bit systems are supported.");

/// The interpreter namespace.
namespace interp {
/// Forward decls.
struct interpreter;

/// Typedefs.
using opcode_t = u32;
using addr = usz;
using native_function = std::function<void(interpreter&)>;
using elem = u64;

/// Opcode.
enum struct opcode : opcode_t {
    invalid = 0,

    /// Does nothing. Used for alignment.
    nop,

    /// Return from a function or stop the interpreter.
    ret,

    /// Push an integer onto the stack.
    /// Operands: value (8 bytes)
    push_int,

    /// Add the top two values on the stack as integers.
    addi,

    /// Function call.
    /// Operands: index (7 remaining bytes of operands).
    /// The index is the index of the function in the function table.
    call,

    /// For sanity checks.
    max_opcode
};

/// Opcodes may only be 1 byte.
static_assert(opcode_t(opcode::max_opcode) < 256);

/// Error type.
struct error : std::runtime_error {
    template <typename ...arguments>
    error(fmt::format_string<arguments...> fmt, arguments&& ...args)
        : std::runtime_error(fmt::format(fmt, std::forward<arguments>(args)...)) {}
};

/// ===========================================================================
///  Interpreter struct.
/// ===========================================================================
/// This holds the interpreter state.
/// TODO: Should be class. struct for now for testing purposes.
struct interpreter {
    /// The code that we’re executing.
    std::vector<elem> bytecode;

    /// Instruction pointer.
    addr ip{};

    /// The stack.
    std::vector<elem> data_stack;
    std::vector<addr> addr_stack;

    /// Functions in the bytecode. NEVER reorder or remove elements from these.
    std::vector<std::variant<addr, native_function>> functions;
    std::unordered_map<std::string, usz> functions_map;

    /// How many stack frames deep we are.
    usz stack_frame_count{};

    /// Constants.
    constexpr static usz max_call_index = 0x00ff'ffff'ffff'ffffzu;

    /// ===========================================================================
    ///  Stack manipulation.
    /// ===========================================================================
    /// Push a value onto the stack.
    void push(elem value);

    /// Pop a value from the stack.
    elem pop();

public:
    /// Maximum stack size.
    usz max_stack_size = 1024 * 1024;

    /// Construct an interpreter.
    interpreter() = default;

    /// Copying/moving this is a bad idea.
    interpreter(const interpreter&) = delete;
    interpreter(interpreter&&) = delete;
    interpreter& operator=(const interpreter&) = delete;
    interpreter& operator=(interpreter&&) = delete;

    /// Define a binding to a native function.
    void defun(const std::string& name, native_function func);

    /// Run the interpreter().
    void run();

    /// ===========================================================================
    ///  Operations.
    /// ===========================================================================
    /// Create a return instruction.
    void create_return();

    /// Create an instruction that pushes an integer onto the stack.
    void create_push_int(i64 value);

    /// Create an instruction that adds the top two values on the stack and pushes the result.
    void create_addi();

    /// Create a call to a function.
    void create_call(const std::string& name);
};

}

#endif // INTERPRETER_INTERP_HH
