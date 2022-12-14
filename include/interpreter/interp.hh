#ifndef INTERPRETER_INTERP_HH
#define INTERPRETER_INTERP_HH

/// Make sure we don’t try to include this header from C.
#ifndef __cplusplus
#    error "This header is C++ only. Use <interpreter/interp.h> instead."
#endif

#include <functional>
#include <interpreter/interp.h>
#include <interpreter/utils.hh>
#include <unordered_map>
#include <variant>
#include <vector>

/// Don’t want to deal w/ this rn.
static_assert(sizeof(void*) == 8 && sizeof(usz) == 8, "Only 64-bit systems are supported.");

/// This is so we can downcast more easily.
struct interp_handle_t {};

/// The interpreter namespace.
namespace interp {
/// Forward decls.
class interpreter;

/// Typedefs.
using opcode_t = u8;
using addr = usz;
using native_function = std::function<void(interpreter&)>;
enum struct reg : u8 {
    arith_imm_32 = 0,
    arith_imm_64 = 128,
};
using word = u64;

/// Constants.
constexpr static usz ip_start_addr = 1;

constexpr static u8 osz_mask = 0b1100'0000;
constexpr static u8 reg_mask = static_cast<u8>(~osz_mask);

/// Literals.
namespace literals {
constexpr reg operator""_r(unsigned long long r) { return static_cast<reg>(r); }
constexpr word operator""_w(unsigned long long a) { return static_cast<word>(a); }
} // namespace literals

/// Register helpers.
constexpr reg operator|(reg r, u8 s) { return static_cast<reg>(static_cast<u8>(r) | s); }
constexpr u8 operator+(reg r) { return static_cast<u8>(r); }
constexpr u8 index(reg r) { return static_cast<u8>(r) & reg_mask; }

/// Opcode.
enum struct opcode : opcode_t {
    invalid = 0,

    /// Does nothing.
    nop,

    /// Return from a function or stop the interpreter.
    ret,

    /// Move a register or immediate to a register.
    mov,

    /// Add integers.
    /// Encoding: arithmetic encoding.
    add,

    /// Subtract integers.
    /// Encoding: arithmetic encoding.
    sub,

    /// Multiply values.
    /// Encoding: arithmetic encoding.
    muli,
    mulu,

    /// Divide values.
    /// Encoding: arithmetic encoding.
    divi,
    divu,

    /// Compute the remainder of a division.
    /// Encoding: arithmetic encoding.
    remi,
    remu,

    /// Shift left.
    /// Encoding: arithmetic encoding.
    shl,

    /// Shift right.
    /// Encoding: arithmetic encoding.
    sar,
    shr,

    /// Function call.
    /// Operands: index (word).
    /// The index is the index of the function in the function table.
    call,

    /// Jump to an address.
    /// Operands: address (word)
    jmp,

    /// Jump to an address if a register is nonzero.
    /// Operands: condition (register), address (word)
    jnz,

    /// For sanity checks.
    max_opcode
};

/// Macro used for codegenning arithmetic instructions.
#define INTERP_ALL_ARITHMETIC_INSTRUCTIONS(F) \
    F(add, +, word)                           \
    F(sub, -, word)                           \
    F(muli, *, i64)                           \
    F(mulu, *, word)                          \
    F(divi, /, i64)                           \
    F(divu, /, word)                          \
    F(remi, %, i64)                           \
    F(remu, %, word)                          \
    F(shl, <<, word)                          \
    F(sar, >>, i64)                           \
    F(shr, >>, word)

/// Error type.
struct error : std::runtime_error {
    template <typename... arguments>
    error(fmt::format_string<arguments...> fmt, arguments&&... args)
        : std::runtime_error(fmt::format(fmt, std::forward<arguments>(args)...)) {}
};

/// ===========================================================================
///  Interpreter struct.
/// ===========================================================================
/// This holds the interpreter state.
class interpreter : ::interp_handle_t {
    /// Instruction pointer.
    addr ip{};

    /// Registers.
    ///
    /// The underscores are due to the fact that, sometimes, you don’t
    /// want to use this directly, and they make you think twice about
    /// using this.
    std::array<word, 64> _registers_;

    /// Stack pointer.
    addr sp{};

    /// The code that we’re executing.
    std::vector<u8> bytecode;

    /// The stack.
    std::vector<word> stack;
    addr stack_base;

    /// Functions in the bytecode. NEVER reorder or remove elements from these.
    std::vector<std::variant<std::monostate, addr, native_function>> functions;
    std::unordered_map<std::string, usz> functions_map;

    /// How many stack frames deep we are.
    usz stack_frame_count{};

    /// ===========================================================================
    ///  Encoder/decoder.
    /// ===========================================================================
    /// Check if registers are valid.
    void check_regs(std::same_as<reg> auto... regs) const {
        ([this](reg r) {
            if (index(r) >= _registers_.size()) {
                throw error("Invalid register: {}", index(r));
            }
        }(regs),
         ...);
    }

    /// Set (part of) a register to a value.
    void set_register(reg r, word value);

    /// Read (part of) a register.
    word read_register(reg r) const;

    /// Encode an arithmetic instruction.
    ///
    /// Arithmetic encoding works as follows: An instruction is 4 bytes
    /// long. The opcode is the first byte, the destination register the
    /// second byte, and the last two bytes are the two source registers.
    ///
    /// If one of the source registers is arith_imm_32 (or arith_imm_64),
    /// then the next 4 (or 8) bytes are an immediate value that is to
    /// be used instead.
    ///
    /// Both source registers cannot be arith_imm_32 or arith_imm_64.
    void encode_arithmetic(opcode op, reg dest, reg r1, reg r2);
    void encode_arithmetic(opcode op, reg dest, reg src, word imm);
    void encode_arithmetic(opcode op, reg dest, word imm, reg src);

    /// Decode an arithmetic instruction.
    struct arith_t {
        reg dest;
        word src1;
        word src2;
    };
    arith_t decode_arithmetic();

    /// Read a word at the current position of the instruction pointer.
    word read_word_at_ip();

public:
    /// Maximum stack size.
    usz max_stack_size = 1024 * 1024;

    /// Last error. Used by the C API.
    std::string last_error;

    /// ===========================================================================
    ///  Driver and Utils.
    /// ===========================================================================

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
    /// \return The return value of the program.
    word run();


    /// ===========================================================================
    ///  State manipulation.
    /// ===========================================================================
    /// Get the value of an argument register.
    word arg(usz index, interp_size sz) const;

    /// Push a value onto the stack.
    void push(word value);

    /// Pop a value from the stack.
    word pop();

    /// Get the value of a register.
    word r(reg r) const;
    void r(reg r, word value);

    /// ===========================================================================
    ///  Operations.
    /// ===========================================================================
    /// Create a return instruction.
    void create_return();

    /// Create a move instruction.
    void create_move(reg dest, reg src);
    void create_move(reg dest, word imm);

    /// Arithmetic instructions
#define ARITH(name, ...)                                   \
    void CAT(create_, name)(reg dest, reg src1, reg src2); \
    void CAT(create_, name)(reg dest, reg src, word imm);  \
    void CAT(create_, name)(reg dest, word imm, reg src);
    INTERP_ALL_ARITHMETIC_INSTRUCTIONS(ARITH)
#undef ARITH

    /// Create a call to a function.
    void create_call(const std::string& name);

    /// Create a direct branch.
    void create_branch(addr target);

    /// Create a conditional branch that branches if the top of the stack is nonzero.
    void create_branch_ifnz(reg condition, addr target);

    /// Create a function that can be called.
    void create_function(const std::string& name);

    /// Get the current address.
    addr current_addr() const;
};

} // namespace interp

#endif // INTERPRETER_INTERP_HH
