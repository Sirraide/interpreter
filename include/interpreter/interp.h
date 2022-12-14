#ifndef INTERPRETER_INTERP_H
#define INTERPRETER_INTERP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Error codes.
#define INTERP_OK (0)

/// Opaque type.
typedef struct interp_handle_t* interp_handle;

/// Error code.
typedef int interp_code;

/// Register type.
typedef uint8_t interp_reg;

/// Integer type.
typedef uint64_t interp_word;

/// Address type.
typedef uint64_t interp_address;

/// Size flags.
typedef enum interp_size_mask {
    INTERP_SIZE_MASK_64 = 0,
    INTERP_SIZE_MASK_32 = 0b10000000,
    INTERP_SIZE_MASK_16 = 0b01000000,
    INTERP_SIZE_MASK_8 = 0b11000000,
} interp_size_mask;

/// ===========================================================================
///  Interpreter creation and destruction.
/// ===========================================================================
/// Create a new interpreter.
/// \return A handle to the interpreter, or NULL on failure.
interp_handle interp_create();

/// Destroy an interpreter.
/// \param handle The interpreter to destroy.
void interp_destroy(interp_handle handle);

/// ===========================================================================
///  Driver and utils.
/// ===========================================================================
/// Get the last error message.
///
/// \param handle The interpreter handle.
/// \return The error message corresponding to the last error that occurred
///         or NULL if there was no error or if the handle was invalid. The
///         error message is allocated as if by a call to strdup() and must
///         be freed by the caller.
char* interp_get_error(interp_handle handle);

/// Define a binding to a native function.
///
/// \param handle The interpreter handle.
/// \param name The name of the binding.
/// \param func The function to bind.
/// \param user User data to pass to the function. Can be NULL.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_defun(
    interp_handle handle,
    const char* name,
    void func(interp_handle, void*),
    void* user
);

/// Disassemble the bytecode.
///
/// \param handle The interpreter handle.
/// \return The disassembly of the bytecode, or NULL on error. The string
///         is allocated as if by a call to strdup() and must be freed by
///         the caller.
char* interp_disassemble(interp_handle handle);

/// Run the interpreter.
///
/// \param handle The interpreter handle.
/// \param retval Out parameter for the return value. May be NULL.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_run(interp_handle handle, interp_word* retval);

/// ===========================================================================
///  State manipulation.
/// ===========================================================================
/// Get the value of an argument register.
///
/// \param handle The interpreter handle.
/// \param index The index of the argument register.
/// \param sz The size of the register.
/// \param value Out parameter for the value of the register.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_arg(
    interp_handle handle,
    size_t index,
    interp_size_mask sz,
    interp_word* value
);

/// Push a value onto the stack.
///
/// \param handle The interpreter handle.
/// \param value The value to push.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_push(interp_handle handle, interp_word value);

/// Pop a value from the stack.
///
/// \param handle The interpreter handle.
/// \param value Out parameter for the value.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_pop(interp_handle handle, interp_word* value);

/// Get the value of a register.
///
/// \param handle The interpreter handle.
/// \param reg The register.
/// \param value Out parameter for the value of the register.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_get_register(interp_handle handle, interp_reg reg, interp_word* value);

/// Set the value of a register.
///
/// \param handle The interpreter handle.
/// \param reg The register.
/// \param value The value to set.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_set_register(interp_handle handle, interp_reg reg, interp_word value);

/// Set the return value of the current native call.
///
/// \param handle The interpreter handle.
/// \param value The return value.
void interp_set_return_value(interp_handle handle, interp_word value);

/// ===========================================================================
///  Linker.
/// ===========================================================================
/// Emit a call a function in a shared library.
///
/// \param handle The interpreter handle.
/// \param name The path to the shared library.
/// \param func The name of the function to call.
/// \param argc The number of arguments that the function takes.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_library_call_unsafe(
    interp_handle handle,
    const char* name,
    const char* func,
    size_t argc
);

/// ===========================================================================
///  Memory.
/// ===========================================================================
/// Allocate memory on the stack.
///
/// This allocates *at least* `size` bytes, but may allocate more.
///
/// \param handle The interpreter handle.
/// \param size The size of the allocation.
/// \param address Out parameter for the address of the allocation.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_alloca(interp_handle handle, size_t size, interp_address* address);

/// Create a global variable.
///
/// This allocates *at least* `size` bytes, but may allocate more.
///
/// \param handle The interpreter handle.
/// \param size The size of the allocation.
/// \param address Out parameter for a global pointer corresponding to
///     the start of the allocation.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_global(interp_handle handle, size_t size, interp_address* address);

/// Emit a direct load from memory.
///
/// \param handle The interpreter handle.
/// \param reg The register to load into.
/// \param ptr The address to load from.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_load(interp_handle handle, interp_reg reg, interp_address ptr);

/// Emit a load from native memory.
///
/// \param dest The register to load into.
/// \param src The native address to load from.
/// \param sz The size of the load.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_load_native(
    interp_handle handle,
    interp_reg dest,
    void* src,
    interp_size_mask sz
);

/// Wrapper for interp_create_load_native() that loads a value of type `uint8_t`.
inline interp_code interp_create_load_native_u8(
    interp_handle handle,
    interp_reg dest,
    uint8_t* src
) { return interp_create_load_native(handle, dest, src, INTERP_SIZE_MASK_8); }

/// Wrapper for interp_create_load_native() that loads a value of type `uint16_t`.
inline interp_code interp_create_load_native_u16(
    interp_handle handle,
    interp_reg dest,
    uint16_t* src
) { return interp_create_load_native(handle, dest, src, INTERP_SIZE_MASK_16); }

/// Wrapper for interp_create_load_native() that loads a value of type `uint32_t`.
inline interp_code interp_create_load_native_u32(
    interp_handle handle,
    interp_reg dest,
    uint32_t* src
) { return interp_create_load_native(handle, dest, src, INTERP_SIZE_MASK_32); }

/// Wrapper for interp_create_load_native() that loads a value of type `uint64_t`.
inline interp_code interp_create_load_native_u64(
    interp_handle handle,
    interp_reg dest,
    uint64_t* src
) { return interp_create_load_native(handle, dest, src, INTERP_SIZE_MASK_64); }

/// Emit an indirect load from memory.
///
/// \param handle The interpreter handle.
/// \param dest The register to load into.
/// \param src src The register containing the base address. r0 represents the stack base pointer.
/// \param offs The offset from the base address.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_load_indirect(
    interp_handle handle,
    interp_reg dest,
    interp_reg src,
    interp_word offs
);

/// Emit a store to memory.
///
/// \param handle The interpreter handle.
/// \param dest The address to store to.
/// \param src The register containing the value to store.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_store(interp_handle handle, interp_address dest, interp_reg src);

/// Emit a store to native memory.
///
/// \param handle The interpreter handle.
/// \param dest The native address to store to.
/// \param src The register containing the value to store.
/// \param sz The size of the store.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_store_native(
    interp_handle handle,
    void* dest,
    interp_reg src,
    interp_size_mask sz
);

/// Wrapper for interp_create_store_native() that stores a value of type `uint8_t`.
inline interp_code interp_create_store_native_u8(
    interp_handle handle,
    uint8_t* dest,
    interp_reg src
) { return interp_create_store_native(handle, dest, src, INTERP_SIZE_MASK_8); }

/// Wrapper for interp_create_store_native() that stores a value of type `uint16_t`.
inline interp_code interp_create_store_native_u16(
    interp_handle handle,
    uint16_t* dest,
    interp_reg src
) { return interp_create_store_native(handle, dest, src, INTERP_SIZE_MASK_16); }

/// Wrapper for interp_create_store_native() that stores a value of type `uint32_t`.
inline interp_code interp_create_store_native_u32(
    interp_handle handle,
    uint32_t* dest,
    interp_reg src
) { return interp_create_store_native(handle, dest, src, INTERP_SIZE_MASK_32); }

/// Wrapper for interp_create_store_native() that stores a value of type `uint64_t`.
inline interp_code interp_create_store_native_u64(
    interp_handle handle,
    uint64_t* dest,
    interp_reg src
) { return interp_create_store_native(handle, dest, src, INTERP_SIZE_MASK_64); }

/// Emit an indirect store to memory.
///
/// \param handle The interpreter handle.
/// \param dest The register containing the base address. r0 represents the stack base pointer.
/// \param offs The offset from the base address.
/// \param src The register containing the value to store.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_store_indirect(
    interp_handle handle,
    interp_reg dest,
    interp_word offs,
    interp_reg src
);

/// ===========================================================================
///  Operations.
/// ===========================================================================
/// Emit a return instruction.
/// \param handle The interpreter handle.
void interp_create_return(interp_handle handle);

/// Emit an instruction to move a value from one register to another.
///
/// \param handle The interpreter handle.
/// \param dest The source register.
/// \param src The destination register.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_move_rr(
    interp_handle handle,
    interp_reg dest,
    interp_reg src
);

/// Emit an instruction to move an immediate value into a register.
///
/// \param handle The interpreter handle.
/// \param dest The destination register.
/// \param value The immediate value.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_move_ri(
    interp_handle handle,
    interp_reg dest,
    interp_word value
);

/// Create a call to a function.
///
/// \param handle The interpreter handle.
/// \param name The name of the function to call.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_call(interp_handle handle, const char* name);

/// Create a direct branch.
///
/// \param handle The interpreter handle.
/// \param target The address to branch to.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_branch(interp_handle handle, interp_address target);

/// Create a conditional branch.
///
/// \param handle The interpreter handle.
/// \param cond The condition to branch on.
/// \param target The address to branch to.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_branch_ifnz(interp_handle handle, interp_reg cond, interp_address target);

/// Create a function at the current address.
///
/// \param handle The interpreter handle.
/// \param name The name of the function.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_function(interp_handle handle, const char* name);

/// Emit an instruction to exchange two registers.
///
/// If r1 and r2 represent the same register, but their sizes differ, then the
/// value in that register is truncated to the smaller size.
///
/// \param handle The interpreter handle.
/// \param r1 The first register.
/// \param r2 The second register.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_xchg_rr(interp_handle handle, interp_reg r1, interp_reg r2);

/// Get the current address.
///
/// \param handle The interpreter handle.
/// \param out The output address. May be NULL.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_address interp_current_address(interp_handle handle);

/// ===========================================================================
///  Arithmetic and bitwise instructions.
/// ===========================================================================
/// Emit an instruction to add two registers.
///
/// \param handle The interpreter handle.
/// \param dest The destination register.
/// \param src1 The first source register.
/// \param src2 The second source register.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_add_rr(
    interp_handle handle,
    interp_reg dest,
    interp_reg src1,
    interp_reg src2
);

/// Emit an instruction to add a register and an immediate value.
///
/// \param handle The interpreter handle.
/// \param dest The destination register.
/// \param src The source register.
/// \param value The immediate value.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_add_ri(
    interp_handle handle,
    interp_reg dest,
    interp_reg src,
    interp_word value
);

/// Emit an instruction to subtract two registers.
///
/// \param handle The interpreter handle.
/// \param dest The destination register.
/// \param src1 The first source register.
/// \param src2 The second source register.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_sub_rr(
    interp_handle handle,
    interp_reg dest,
    interp_reg src1,
    interp_reg src2
);

/// Emit an instruction to subtract a register and an immediate value.
///
/// \param handle The interpreter handle.
/// \param dest The destination register.
/// \param src The source register.
/// \param value The immediate value.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_sub_ri(
    interp_handle handle,
    interp_reg dest,
    interp_reg src,
    interp_word value
);

/// Emit an instruction to subtract an immediate and a register.
///
/// \param handle The interpreter handle.
/// \param dest The destination register.
/// \param value The immediate value.
/// \param src The source register.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_sub_ir(
    interp_handle handle,
    interp_reg dest,
    interp_word value,
    interp_reg src
);

/// Emit an instruction to multiply two registers (signed).
///
/// \param handle The interpreter handle.
/// \param dest The destination register.
/// \param src1 The first source register.
/// \param src2 The second source register.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_muli_rr(
    interp_handle handle,
    interp_reg dest,
    interp_reg src1,
    interp_reg src2
);

/// Emit an instruction to multiply a register and an immediate value (signed).
///
/// \param handle The interpreter handle.
/// \param dest The destination register.
/// \param src The source register.
/// \param value The immediate value.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_muli_ri(
    interp_handle handle,
    interp_reg dest,
    interp_reg src,
    interp_word value
);

/// Emit an instruction to multiply two registers (unsigned).
///
/// \param handle The interpreter handle.
/// \param dest The destination register.
/// \param src1 The first source register.
/// \param src2 The second source register.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_mulu_rr(
    interp_handle handle,
    interp_reg dest,
    interp_reg src1,
    interp_reg src2
);

/// Emit an instruction to multiply a register and an immediate value (unsigned).
///
/// \param handle The interpreter handle.
/// \param dest The destination register.
/// \param src The source register.
/// \param value The immediate value.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_mulu_ri(
    interp_handle handle,
    interp_reg dest,
    interp_reg src,
    interp_word value
);

/// Emit an instruction to divide two registers. (signed)
///
/// \param handle The interpreter handle.
/// \param dest The destination register.
/// \param src1 The first source register.
/// \param src2 The second source register.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_divi_rr(
    interp_handle handle,
    interp_reg dest,
    interp_reg src1,
    interp_reg src2
);

/// Emit an instruction to divide a register and an immediate value. (signed)
///
/// \param handle The interpreter handle.
/// \param dest The destination register.
/// \param src The source register.
/// \param value The immediate value.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_divi_ri(
    interp_handle handle,
    interp_reg dest,
    interp_reg src,
    interp_word value
);

/// Emit an instruction to divide an immediate and a register. (signed)
///
/// \param handle The interpreter handle.
/// \param dest The destination register.
/// \param value The immediate value.
/// \param src The source register.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_divi_ir(
    interp_handle handle,
    interp_reg dest,
    interp_word value,
    interp_reg src
);

/// Emit an instruction to divide two registers. (unsigned)
///
/// \param handle The interpreter handle.
/// \param dest The destination register.
/// \param src1 The first source register.
/// \param src2 The second source register.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_divu_rr(
    interp_handle handle,
    interp_reg dest,
    interp_reg src1,
    interp_reg src2
);

/// Emit an instruction to divide a register and an immediate value. (unsigned)
///
/// \param handle The interpreter handle.
/// \param dest The destination register.
/// \param src The source register.
/// \param value The immediate value.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_divu_ri(
    interp_handle handle,
    interp_reg dest,
    interp_reg src,
    interp_word value
);

/// Emit an instruction to divide an immediate and a register. (unsigned)
///
/// \param handle The interpreter handle.
/// \param dest The destination register.
/// \param value The immediate value.
/// \param src The source register.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_divu_ir(
    interp_handle handle,
    interp_reg dest,
    interp_word value,
    interp_reg src
);

/// Emit an instruction to compute the remainder of two registers. (signed)
///
/// \param handle The interpreter handle.
/// \param dest The destination register.
/// \param src1 The first source register.
/// \param src2 The second source register.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_modi_rr(
    interp_handle handle,
    interp_reg dest,
    interp_reg src1,
    interp_reg src2
);

/// Emit an instruction to compute the remainder of a register and an immediate value. (signed)
///
/// \param handle The interpreter handle.
/// \param dest The destination register.
/// \param src The source register.
/// \param value The immediate value.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_modi_ri(
    interp_handle handle,
    interp_reg dest,
    interp_reg src,
    interp_word value
);

/// Emit an instruction to compute the remainder of an immediate and a register. (signed)
///
/// \param handle The interpreter handle.
/// \param dest The destination register.
/// \param value The immediate value.
/// \param src The source register.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_modi_ir(
    interp_handle handle,
    interp_reg dest,
    interp_word value,
    interp_reg src
);

/// Emit an instruction to compute the remainder of two registers. (unsigned)
///
/// \param handle The interpreter handle.
/// \param dest The destination register.
/// \param src1 The first source register.
/// \param src2 The second source register.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_modu_rr(
    interp_handle handle,
    interp_reg dest,
    interp_reg src1,
    interp_reg src2
);

/// Emit an instruction to compute the remainder of a register and an immediate value. (unsigned)
///
/// \param handle The interpreter handle.
/// \param dest The destination register.
/// \param src The source register.
/// \param value The immediate value.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_modu_ri(
    interp_handle handle,
    interp_reg dest,
    interp_reg src,
    interp_word value
);

/// Emit an instruction to compute the remainder of an immediate and a register. (unsigned)
///
/// \param handle The interpreter handle.
/// \param dest The destination register.
/// \param value The immediate value.
/// \param src The source register.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_modu_ir(
    interp_handle handle,
    interp_reg dest,
    interp_word value,
    interp_reg src
);

/// Emit an instruction to shift a register left by a register.
///
/// \param handle The interpreter handle.
/// \param dest The destination register.
/// \param src1 The first source register.
/// \param src2 The second source register.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_shift_left_rr(
    interp_handle handle,
    interp_reg dest,
    interp_reg src1,
    interp_reg src2
);

/// Emit an instruction to shift a register left by an immediate value.
///
/// \param handle The interpreter handle.
/// \param dest The destination register.
/// \param src The source register.
/// \param value The immediate value.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_shift_left_ri(
    interp_handle handle,
    interp_reg dest,
    interp_reg src,
    interp_word value
);

/// Emit an instruction to shift an immediate value left by a register.
///
/// \param handle The interpreter handle.
/// \param dest The destination register.
/// \param value The immediate value.
/// \param src The source register.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_shift_left_ir(
    interp_handle handle,
    interp_reg dest,
    interp_word value,
    interp_reg src
);

/// Emit an instruction to shift a register right by a register. (logical)
///
/// \param handle The interpreter handle.
/// \param dest The destination register.
/// \param src1 The first source register.
/// \param src2 The second source register.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_shift_right_logical_rr(
    interp_handle handle,
    interp_reg dest,
    interp_reg src1,
    interp_reg src2
);

/// Emit an instruction to shift a register right by an immediate value. (logical)
///
/// \param handle The interpreter handle.
/// \param dest The destination register.
/// \param src The source register.
/// \param value The immediate value.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_shift_right_logical_ri(
    interp_handle handle,
    interp_reg dest,
    interp_reg src,
    interp_word value
);

/// Emit an instruction to shift an immediate value right by a register. (logical)
///
/// \param handle The interpreter handle.
/// \param dest The destination register.
/// \param value The immediate value.
/// \param src The source register.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_shift_right_logical_ir(
    interp_handle handle,
    interp_reg dest,
    interp_word value,
    interp_reg src
);

/// Emit an instruction to shift a register right by a register. (arithmetic)
///
/// \param handle The interpreter handle.
/// \param dest The destination register.
/// \param src1 The first source register.
/// \param src2 The second source register.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_shift_right_arithmetic_rr(
    interp_handle handle,
    interp_reg dest,
    interp_reg src1,
    interp_reg src2
);

/// Emit an instruction to shift a register right by an immediate value. (arithmetic)
///
/// \param handle The interpreter handle.
/// \param dest The destination register.
/// \param src The source register.
/// \param value The immediate value.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_shift_right_arithmetic_ri(
    interp_handle handle,
    interp_reg dest,
    interp_reg src,
    interp_word value
);

/// Emit an instruction to shift an immediate value right by a register. (arithmetic)
///
/// \param handle The interpreter handle.
/// \param dest The destination register.
/// \param value The immediate value.
/// \param src The source register.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_shift_right_arithmetic_ir(
    interp_handle handle,
    interp_reg dest,
    interp_word value,
    interp_reg src
);

#ifdef __cplusplus
}
#endif

#endif // INTERPRETER_INTERP_H
