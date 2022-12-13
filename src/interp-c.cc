#include <interpreter/interp.hh>

extern "C" {

constexpr inline interp_code INTERP_ERR = 1;
/*

/// ===========================================================================
///  Interpreter creation and destruction.
/// ===========================================================================
interp_handle interp_create() try {
    return new interp::interpreter();
} catch (const std::exception& e) {
    return nullptr;
}

void interp_destroy(interp_handle handle) { delete handle; }

/// ===========================================================================
///  Interpreter misc.
/// ===========================================================================
char* interp_get_error(interp_handle handle) {
    auto i = static_cast<interp::interpreter*>(handle);
    if (i == nullptr) return nullptr;
    if (i->last_error.empty()) return nullptr;
    return strdup(i->last_error.c_str());
}

interp_code interp_push_int(interp_handle handle, interp_int value) {
    auto i = static_cast<interp::interpreter*>(handle);
    if (i == nullptr) return INTERP_ERR;
    try {
        i->create_push_int(value);
        return INTERP_OK;
    } catch (const std::exception& e) {
        i->last_error = e.what();
        return INTERP_ERR;
    }
}

interp_code interp_pop_int(interp_handle handle, interp_int* out) {
    auto i = static_cast<interp::interpreter*>(handle);
    if (i == nullptr) return INTERP_ERR;
    try {
        auto val = i->pop();
        if (out) *out = static_cast<interp_int>(val);
        return INTERP_OK;
    } catch (const std::exception& e) {
        i->last_error = e.what();
        return INTERP_ERR;
    }
}

interp_code interp_defun(
    interp_handle handle,
    const char* name,
    void func(interp_handle, void*),
    void* user
) {
    auto i = static_cast<interp::interpreter*>(handle);
    if (i == nullptr) return INTERP_ERR;
    try {
        i->defun(name, [=](interp::interpreter&) { func(handle, user); });
        return INTERP_OK;
    } catch (const std::exception& e) {
        i->last_error = e.what();
        return INTERP_ERR;
    }
}

interp_code interp_run(interp_handle handle, interp_int *retval) {
    auto i = static_cast<interp::interpreter*>(handle);
    if (i == nullptr) return INTERP_ERR;
    try {
        auto result = i->run();
        if (retval) *retval = result;
        return INTERP_OK;
    } catch (const std::exception& e) {
        i->last_error = e.what();
        return INTERP_ERR;
    }
}

char* interp_disassemble(interp_handle handle) {
    auto i = static_cast<interp::interpreter*>(handle);
    if (i == nullptr) return nullptr;
    try {
        auto str = i->disassemble();
        return strdup(str.c_str());
    } catch (const std::exception& e) {
        i->last_error = e.what();
        return nullptr;
    }
}

/// ===========================================================================
///  Operations.
/// ===========================================================================
void interp_create_return_void(interp_handle handle) {
    static_cast<interp::interpreter*>(handle)->create_return_void();
}

void interp_create_return_value(interp_handle handle) {
    static_cast<interp::interpreter*>(handle)->create_return_value();
}

void interp_create_push_int(interp_handle handle, i64 value) {
    static_cast<interp::interpreter*>(handle)->create_push_int(value);
}

void interp_create_addi(interp_handle handle) {
    static_cast<interp::interpreter*>(handle)->create_addi();
}

void interp_create_subi(interp_handle handle) {
    static_cast<interp::interpreter*>(handle)->create_subi();
}

void interp_create_muli(interp_handle handle) {
    static_cast<interp::interpreter*>(handle)->create_muli();
}

void interp_create_mulu(interp_handle handle) {
    static_cast<interp::interpreter*>(handle)->create_mulu();
}

void interp_create_divi(interp_handle handle) {
    static_cast<interp::interpreter*>(handle)->create_divi();
}

void interp_create_divu(interp_handle handle) {
    static_cast<interp::interpreter*>(handle)->create_divu();
}

void interp_create_remi(interp_handle handle) {
    static_cast<interp::interpreter*>(handle)->create_remi();
}

void interp_create_remu(interp_handle handle) {
    static_cast<interp::interpreter*>(handle)->create_remu();
}

void interp_create_shift_left(interp_handle handle) {
    static_cast<interp::interpreter*>(handle)->create_shift_left();
}

void interp_create_shift_right_arithmetic(interp_handle handle) {
    static_cast<interp::interpreter*>(handle)->create_shift_right_arithmetic();
}

void interp_create_shift_right_logical(interp_handle handle) {
    static_cast<interp::interpreter*>(handle)->create_shift_right_logical();
}

void interp_create_dup(interp_handle handle) {
    static_cast<interp::interpreter*>(handle)->create_dup();
}

interp_code interp_create_call(interp_handle handle, const char* name) {
    auto i = static_cast<interp::interpreter*>(handle);
    if (i == nullptr) return INTERP_ERR;
    try {
        i->create_call(name);
        return INTERP_OK;
    } catch (const std::exception& e) {
        i->last_error = e.what();
        return INTERP_ERR;
    }
}

interp_code interp_create_branch(interp_handle handle, interp_address target) {
    auto i = static_cast<interp::interpreter*>(handle);
    if (i == nullptr) return INTERP_ERR;
    try {
        i->create_branch(target);
        return INTERP_OK;
    } catch (const std::exception& e) {
        i->last_error = e.what();
        return INTERP_ERR;
    }
}

interp_code interp_create_branch_ifnz(interp_handle handle, interp_address target) {
    auto i = static_cast<interp::interpreter*>(handle);
    if (i == nullptr) return INTERP_ERR;
    try {
        i->create_branch_ifnz(target);
        return INTERP_OK;
    } catch (const std::exception& e) {
        i->last_error = e.what();
        return INTERP_ERR;
    }
}

interp_code interp_current_address(interp_handle handle, interp_address* out) {
    auto i = static_cast<interp::interpreter*>(handle);
    if (i == nullptr) return INTERP_ERR;
    try {
        if (out) *out = i->current_addr();
        return INTERP_OK;
    } catch (const std::exception& e) {
        i->last_error = e.what();
        return INTERP_ERR;
    }
}
*/

//
}