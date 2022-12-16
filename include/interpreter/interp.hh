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
static_assert(sizeof(void*) == 8 && sizeof(size_t) == 8, "Only 64-bit systems are supported.");

/// This is so we can downcast more easily.
struct interp_handle_t {};

/// The interpreter namespace.
namespace interp {
/// Forward decls.
class interpreter;

/// Opcode of an instruction.
using opcode_t = u8;

/// Bytecode address.
using addr = usz;

/// Native function handle.
using native_function = std::function<void(interpreter&)>;

/// Register size.
using word = u64;

/// Register.
enum struct reg : u8 { };

/// Pointer.
enum struct ptr : u64 {null = 0};

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

/// Pointer helpers.
constexpr u64 operator+(ptr p) { return static_cast<u64>(p); }
constexpr ptr operator+(ptr p, word a) { return static_cast<ptr>(static_cast<u64>(p) + a); }

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
    shift_left,

    /// Shift right.
    /// Encoding: arithmetic encoding.
    shift_right_arithmetic,
    shift_right_logical,

    /// Function call.
    /// Operands: index.
    /// The index is the index of the function in the function table.
    call8,
    call16,
    call32,
    call64,

    /// Jump to an address.
    /// Operands: address (word)
    jmp8,
    jmp16,
    jmp32,
    jmp64,

    /// Jump to an address if a register is nonzero.
    /// Operands: condition (register), address (word)
    jnz8,
    jnz16,
    jnz32,
    jnz64,

    /// Load a value from memory.
    load8,
    load16,
    load32,
    load64,

    /// Load a value relative to a register. ‘r0’ is the stack base.
    load_rel8,
    load_rel16,
    load_rel32,
    load_rel64,

    /// Store a register to memory.
    store8,
    store16,
    store32,
    store64,

    /// Store a register relative to a register. ‘r0’ is the stack base.
    store_rel8,
    store_rel16,
    store_rel32,
    store_rel64,

    /// For sanity checks.
    max_opcode
};

/// Opcode helpers.
constexpr opcode_t operator+(opcode o) { return static_cast<opcode_t>(o); }

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
    F(shift_left, <<, word)                   \
    F(shift_right_arithmetic, >>, i64)        \
    F(shift_right_logical, >>, word)

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
class interpreter : public ::interp_handle_t {
    /// Instruction pointer.
    addr ip{};

    /// Registers.
    ///
    /// The underscores are due to the fact that, sometimes, you don’t
    /// want to use this directly, and they make you think twice about
    /// using this.
    std::array<word, 64> _registers_{};

    /// Stack pointer. This *must* always be aligned to 8 bytes.
    ptr sp{};

    /// The code that we’re executing.
    std::vector<u8> bytecode;

    /// Global variables and stack.
    std::vector<u8> _memory_;
    ptr stack_base{};

    /// Globals pointer.
    ptr gp{};

    /// Loaded libraries.
    struct library {
        void* handle;
        std::unordered_map<std::string, usz> functions;
    };

    struct library_function {
        void* handle;
        usz num_params;
        std::string name;
    };

    std::unordered_map<std::string, library> libraries;

    /// Functions in the bytecode. NEVER reorder or remove elements from these.
    struct function {
        std::variant<std::monostate, addr, native_function, library_function> address = std::monostate{};
        usz locals_size{};
    };
    std::vector<function> functions;
    std::unordered_map<std::string, usz> functions_map;

    /// The index of the function that we’re currently emitting.
    usz current_function = 0;

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

    /// Decode a register operand that may also be an immediate.
    word decode_register_operand(reg r);

    /// Decode an arithmetic instruction.
    struct arith_t {
        reg dest;
        word src1;
        word src2;
    };
    arith_t decode_arithmetic();

    /// Read an address from the bytecode at ip.
    word read_sized_address_at_ip(opcode op);

    /// Create a call.
    void create_call_internal(usz index);

    /// Separate function because it’s just too horrible.
    void do_library_call_unsafe(library_function& f);

public:
    /// Maximum memory for globals and the stack.
    usz max_memory = 1024 * 1024;

    /// Last error. Used by the C API.
    std::string last_error;

    /// ===========================================================================
    ///  Driver and Utils.
    /// ===========================================================================

    /// Construct an interpreter.
    explicit interpreter();

    /// Copying/moving this is a bad idea.
    interpreter(const interpreter&) = delete;
    interpreter(interpreter&&) noexcept = delete;
    interpreter& operator=(const interpreter&) = delete;
    interpreter& operator=(interpreter&&) noexcept = delete;

    /// Free resources.
    ~interpreter() noexcept;

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
    word arg(usz index, interp_size_mask sz) const;

    /// Load a value from memory.
    word load_mem(ptr p, usz sz) const;

    /// Push a value onto the stack.
    void push(word value);

    /// Pop a value from the stack.
    word pop();

    /// Get the value of a register.
    word r(reg r) const;
    void r(reg r, word value);

    /// Set the return value.
    void set_return_value(word value);

    /// Store a value to memory.
    void store_mem(ptr p, word value, usz sz);

    /// ===========================================================================
    ///  Linker.
    /// ===========================================================================
    /// Call a function in a shared library by name.
    void create_library_call_unsafe(const std::string& library_path, const std::string& function_name, usz num_params);

    /// ===========================================================================
    ///  Memory.
    /// ===========================================================================
    /// Allocate memory on the stack.
    ///
    /// This allocates *at least* `size` bytes, but may allocate more.
    ///
    /// \param size The size of the allocation.
    /// \return A stack offset corresponding to the start of the allocation.
    word create_alloca(word size);

    /// Create a global variable.
    ///
    /// This allocates *at least* `size` bytes, but may allocate more.
    ///
    /// \param size The size of the allocation.
    /// \return A global pointer corresponding to the start of the allocation.
    ptr create_global(word size);

    /// Load from memory.
    void create_load(reg dest, ptr src);

    /// Indirect load from memory.
    ///
    /// \param dest The destination register.
    /// \param src The register containing the base address. r0 represents the stack base pointer.
    /// \param offs The offset from the base address.
    void create_load(reg dest, reg src, word offs);

    /// Store to memory.
    void create_store(ptr dest, reg src);

    /// Indirect store to memory.
    ///
    /// \param dest The register containing the base address. r0 represents the stack base pointer.
    /// \param offs The offset from the base address.
    /// \param src The source register.
    void create_store(reg dest, word offs, reg src);

    /// ===========================================================================
    ///  Operations.
    /// ===========================================================================
    /// Create a return instruction.
    void create_return();

    /// Create a move instruction.
    void create_move(reg dest, reg src);
    void create_move(reg dest, word imm);

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

    /// Arithmetic instructions
#define ARITH(name, ...)                                          \
    void INTERP_CAT(create_, name)(reg dest, reg src1, reg src2); \
    void INTERP_CAT(create_, name)(reg dest, reg src, word imm);  \
    void INTERP_CAT(create_, name)(reg dest, word imm, reg src);
    INTERP_ALL_ARITHMETIC_INSTRUCTIONS(ARITH)
#undef ARITH
};

} // namespace interp

#endif // INTERPRETER_INTERP_HH
