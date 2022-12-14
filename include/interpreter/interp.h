#ifndef INTERPRETER_INTERP_H
#define INTERPRETER_INTERP_H

#include <interpreter/utils.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Error codes.
#define INTERP_OK (0)

/// Opaque type.
typedef struct interp_handle_t* interp_handle;

/// Error code.
typedef int interp_code;

/// Integer type.
typedef i64 interp_int;

/// Address type.
typedef u64 interp_address;

/// Size flags.
typedef enum interp_size {
    INTERP_SZ_64 = 0,
    INTERP_SZ_32 = 0b10000000,
    INTERP_SZ_16 = 0b01000000,
    INTERP_SZ_8 = 0b11000000,
} interp_size;

/// ===========================================================================
///  Interpreter creation and destruction.
/// ===========================================================================
/// Create a new interpreter.
interp_handle interp_create();

/// Destroy an interpreter.
/// \param handle The interpreter to destroy.
void interp_destroy(interp_handle handle);

/// ===========================================================================
///  Interpreter misc.
/// ===========================================================================
/// Get the last error message.
///
/// \param handle The interpreter handle.
/// \return The error message corresponding to the last error that occurred
///         or NULL if there was no error or if the handle was invalid. The
///         error message is allocated as if by a call to strdup() and must
///         be freed by the caller.
char* interp_get_error(interp_handle handle);

/// Push an integer onto the stack.
///
/// \param handle The interpreter handle.
/// \param value The value to push.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_push_int(interp_handle handle, interp_int value);

/// Pop an integer from the stack.
///
/// \param handle The interpreter handle.
/// \param out The output value. May be NULL.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_pop_int(interp_handle handle, interp_int* out);

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

/// Run the interpreter.
///
/// \param handle The interpreter handle.
/// \param retval The return value. May be NULL.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_run(interp_handle handle, interp_int* retval);

/// Disassemble the bytecode.
///
/// \param handle The interpreter handle.
/// \return The disassembly of the bytecode, or NULL on error. The string
///         is allocated as if by a call to strdup() and must be freed by
///         the caller.
char* interp_disassemble(interp_handle handle);

/// ===========================================================================
///  Operations.
/// ===========================================================================
/// Create a return instruction. This does not return a value.
/// \param handle The interpreter handle.
void interp_create_return_void(interp_handle handle);

/// Create a return instruction. Returns the top value on the stack to the caller.
/// \param handle The interpreter handle.
void interp_create_return_value(interp_handle handle);

/// Create an instruction that pushes an integer onto the stack.
/// \param handle The interpreter handle.
/// \param value The value to push.
void interp_create_push_int(interp_handle handle, i64 value);

/// Create an instruction that adds the top two values on the stack
/// and pushes the result.
///
/// \param handle The interpreter handle.
void interp_create_addi(interp_handle handle);

/// Create an instruction that subtracts the top of the stack from the
/// second value on the stack and pushes the result.
///
/// \param handle The interpreter handle.
void interp_create_subi(interp_handle handle);

/// Create an instruction that multiplies the top two values on the stack
/// and pushes the result.
///
/// This is signed multiplication, for unsigned multiplication use `interp_create_mulu`.
///
/// \param handle The interpreter handle.
void interp_create_muli(interp_handle handle);

/// Create an instruction that multiplies the top two values on the stack
/// and pushes the result.
///
/// This is unsigned multiplication, for signed multiplication use `interp_create_muli`.
///
/// \param handle The interpreter handle.
void interp_create_mulu(interp_handle handle);

/// Create an instruction that divides the second value on the stack by the
/// top value and pushes the result.
///
/// This is signed division, for unsigned division use `interp_create_divu`.
///
/// \param handle The interpreter handle.
void interp_create_divi(interp_handle handle);

/// Create an instruction that divides the second value on the stack by the
/// top value and pushes the result.
///
/// This is unsigned division, for signed division use `interp_create_divi`.
///
/// \param handle The interpreter handle.
void interp_create_divu(interp_handle handle);

/// Create an instruction that computes the remainder of the second value on the stack
/// divided by the top value and pushes the result.
///
/// This operation is signed, for unsigned remainder use `interp_create_remu`.
///
/// \param handle The interpreter handle.
void interp_create_remi(interp_handle handle);

/// Create an instruction that computes the remainder of the second value on the stack
/// divided by the top value and pushes the result.
///
/// This operation is unsigned, for signed remainder use `interp_create_remi`.
///
/// \param handle The interpreter handle.
void interp_create_remu(interp_handle handle);

/// Create an instruction that shifts the second value on the stack left by the
/// top value and pushes the result.
///
/// \param handle The interpreter handle.
void interp_create_shift_left(interp_handle handle);

/// Create an instruction that shifts the second value on the stack right by the
/// top value and pushes the result.
///
/// \param handle The interpreter handle.
void interp_create_shift_right_arithmetic(interp_handle handle);

/// Create an instruction that shifts the second value on the stack right by the
/// top value and pushes the result.
///
/// \param handle The interpreter handle.
void interp_create_shift_right_logical(interp_handle handle);

/// Duplicate the top of the stack.
/// \param handle The interpreter handle.
void interp_create_dup(interp_handle handle);

/// Create a call to a function.
///
/// This function WILL fail if no function with the given name exists.
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

/// Create a conditional branch that branches if the top of the stack is nonzero.
///
/// \param handle The interpreter handle.
/// \param target The address to branch to.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_create_branch_ifnz(interp_handle handle, interp_address target);

/// Get the current address.
///
/// \param handle The interpreter handle.
/// \param out The output address. May be NULL.
/// \return INTERP_OK (0) on success; a nonzero value on failure.
interp_code interp_current_address(interp_handle handle, interp_address* out);

#ifdef __cplusplus
}
#endif

#endif // INTERPRETER_INTERP_H
