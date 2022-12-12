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

/// This is so we can downcast more easily.
struct interp_handle_t {};

/// The interpreter namespace.
namespace interp {
/// Forward decls.
struct interpreter;

/// Typedefs.
using opcode_t = u64;
using addr = usz;
using native_function = std::function<void(interpreter&)>;
using elem = u64;

/// Opcode.
enum struct opcode : opcode_t {
    invalid = 0,

    /// Does nothing.
    nop,

    /// Return from a function or stop the interpreter.
    ret,

    /// Push an integer onto the stack.
    /// Operands: value (7 last bytes of the opcode, or 8 extra bytes)
    push_int,

    /// Add the top two values on the stack as integers.
    addi,

    /// Subtract the second value on the stack from the top value (integers).
    subi,

    /// Multiply the top two values on the stack (integers).
    muli,
    mulu,

    /// Divide the top of the stack by the second value (integers).
    divi,
    divu,

    /// Compute the remainder of the top of the stack divided by the second value (integers).
    remi,
    remu,

    /// Shift the top of the stack left by the second value.
    shl,

    /// Shift the top of the stack right by the second value.
    sar,
    shr,

    /// Function call.
    /// Operands: index (7 remaining bytes of operands).
    /// The index is the index of the function in the function table.
    call,

    /// Jump to an address.
    /// Operands: address (7 last bytes of the opcode, or 8 extra bytes)
    jmp,

    /// Jump to an address if the top of the stack is nonzero.
    /// Operands: address (7 last bytes of the opcode, or 8 extra bytes)
    jnz,

    /// Duplicate the top of the stack.
    dup,

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
struct interpreter : ::interp_handle_t {
    /// Stack frame.
    struct frame {
        addr return_address{};

        /// TODO: locals.
    };

    /// The code that we’re executing.
    std::vector<elem> bytecode;

    /// Instruction pointer.
    addr ip{};

    /// The stack.
    std::vector<elem> data_stack;
    std::vector<frame> frame_stack;

    /// Functions in the bytecode. NEVER reorder or remove elements from these.
    std::vector<std::variant<std::monostate, addr, native_function>> functions;
    std::unordered_map<std::string, usz> functions_map;

    /// How many stack frames deep we are.
    usz stack_frame_count{};

    /// Last error. Used by the C API.
    std::string last_error;

    /// Constants.
    constexpr static usz bit_mask_56 = 0x00ff'ffff'ffff'ffffzu;
    constexpr static usz max_call_index = bit_mask_56;
    constexpr static usz ip_start_addr = 1;

public:
    /// ===========================================================================
    ///  Stack manipulation.
    /// ===========================================================================
    /// Push a value onto the stack.
    void push(elem value);

    /// Pop a value from the stack.
    elem pop();

    /// Maximum stack size.
    usz max_stack_size = 1024 * 1024;

    /// Construct an interpreter.
    interpreter() {
        /// Push an invalid instruction to make sure jumps to 0 throw.
        bytecode.push_back(static_cast<opcode_t>(opcode::invalid));
    }

    /// Copying/moving this is a bad idea.
    interpreter(const interpreter&) = delete;
    interpreter(interpreter&&) = delete;
    interpreter& operator=(const interpreter&) = delete;
    interpreter& operator=(interpreter&&) = delete;

    /// Define a binding to a native function.
    void defun(const std::string& name, native_function func);

    /// Disassemble the bytecode.
    std::string disassemble() const;

    /// Run the interpreter().
    void run();

    /// ===========================================================================
    ///  Operations.
    /// ===========================================================================
    /// Create a return instruction.
    void create_return();

    /// Create an instruction that pushes an integer onto the stack.
    void create_push_int(i64 value);

    /// Create an instruction that adds the top two values on the stack
    /// and pushes the result.
    void create_addi();

    /// Create an instruction that subtracts the top of the stack from the
    /// second value on the stack and pushes the result.
    void create_subi();

    /// Create an instruction that multiplies the top two values on the stack
    /// and pushes the result.
    ///
    /// This is signed multiplication, for unsigned multiplication use `create_mulu`.
    void create_muli();

    /// Create an instruction that multiplies the top two values on the stack
    /// and pushes the result.
    ///
    /// This is unsigned multiplication, for signed multiplication use `create_muli`.
    void create_mulu();

    /// Create an instruction that divides the second value on the stack by the
    /// top value and pushes the result.
    ///
    /// This is signed division, for unsigned division use `create_divu`.
    void create_divi();

    /// Create an instruction that divides the second value on the stack by the
    /// top value and pushes the result.
    ///
    /// This is unsigned division, for signed division use `create_divi`.
    void create_divu();

    /// Create an instruction that computes the remainder of the second value on the stack
    /// divided by the top value and pushes the result.
    ///
    /// This operation is signed, for unsigned remainder use `create_remu`.
    void create_remi();

    /// Create an instruction that computes the remainder of the second value on the stack
    /// divided by the top value and pushes the result.
    ///
    /// This operation is unsigned, for signed remainder use `create_remi`.
    void create_remu();

    /// Create an instruction that shifts the second value on the stack left by the
    /// top value and pushes the result.
    void create_shift_left();

    /// Create an instruction that shifts the second value on the stack right by the
    /// top value and pushes the result.
    void create_shift_right_arithmetic();

    /// Create an instruction that shifts the second value on the stack right by the
    /// top value and pushes the result.
    void create_shift_right_logical();

    /// Create a call to a function.
    void create_call(const std::string& name);

    /// Create a direct branch.
    void create_branch(addr target);

    /// Create a conditional branch that branches if the top of the stack is nonzero.
    void create_branch_ifnz(addr target);

    /// Duplicate the top of the stack.
    void create_dup();

    /// Get the current address.
    addr current_addr() const;
};

}

#endif // INTERPRETER_INTERP_HH
