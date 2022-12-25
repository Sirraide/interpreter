#include <interpreter/interp.hh>

using namespace interp::integers;
using interp::reg;

extern "C" {

constexpr inline interp_code INTERP_ERR = 1;

/// ===========================================================================
///  Interpreter creation and destruction.
/// ===========================================================================
interp_handle interp_create() try {
    return new interp::interpreter();
} catch (const std::exception& e) {
    return nullptr;
}

void interp_destroy(interp_handle handle) { delete static_cast<interp::interpreter*>(handle); }

/// ===========================================================================
///  Driver and utils.
/// ===========================================================================
char* interp_get_error(interp_handle handle) {
    auto i = static_cast<interp::interpreter*>(handle);
    if (i->last_error.empty()) return nullptr;
    return strdup(i->last_error.c_str());
}

interp_code interp_defun(
    interp_handle handle,
    const char* name,
    void func(interp_handle, void*),
    void* user
) {
    auto i = static_cast<interp::interpreter*>(handle);
    try {
        i->defun(name, [=](interp::interpreter&) { func(handle, user); });
        return INTERP_OK;
    } catch (const std::exception& e) {
        i->last_error = e.what();
        return INTERP_ERR;
    }
}

char* interp_disassemble(interp_handle handle) {
    auto i = static_cast<interp::interpreter*>(handle);
    try {
        auto str = i->disassemble();
        return strdup(str.c_str());
    } catch (const std::exception& e) {
        i->last_error = e.what();
        return nullptr;
    }
}

interp_code interp_run(interp_handle handle, interp_word* retval) {
    auto i = static_cast<interp::interpreter*>(handle);
    try {
        auto result = i->run();
        if (retval) *retval = result;
        return INTERP_OK;
    } catch (const std::exception& e) {
        i->last_error = e.what();
        return INTERP_ERR;
    }
}

/// ===========================================================================
///  State manipulation.
/// ===========================================================================

interp_code interp_arg(
    interp_handle handle,
    usz index,
    interp_size_mask sz,
    interp_word* value
) {
    auto i = static_cast<interp::interpreter*>(handle);
    try {
        if (value) *value = i->arg(index, sz);
        return INTERP_OK;
    } catch (const std::exception& e) {
        i->last_error = e.what();
        return INTERP_ERR;
    }
}

interp_code interp_push(interp_handle handle, interp_word value) {
    auto i = static_cast<interp::interpreter*>(handle);
    try {
        i->push(value);
        return INTERP_OK;
    } catch (const std::exception& e) {
        i->last_error = e.what();
        return INTERP_ERR;
    }
}

interp_code interp_pop(interp_handle handle, interp_word* value) {
    auto i = static_cast<interp::interpreter*>(handle);
    try {
        auto val = i->pop();
        if (value) *value = val;
        return INTERP_OK;
    } catch (const std::exception& e) {
        i->last_error = e.what();
        return INTERP_ERR;
    }
}

interp_code interp_get_register(interp_handle handle, interp_reg r, interp_word* value) {
    auto i = static_cast<interp::interpreter*>(handle);
    try {
        if (value) *value = i->r(static_cast<reg>(r));
        return INTERP_OK;
    } catch (const std::exception& e) {
        i->last_error = e.what();
        return INTERP_ERR;
    }
}

interp_code interp_set_register(interp_handle handle, interp_reg r, interp_word value) {
    auto i = static_cast<interp::interpreter*>(handle);
    try {
        i->r(static_cast<reg>(r), value);
        return INTERP_OK;
    } catch (const std::exception& e) {
        i->last_error = e.what();
        return INTERP_ERR;
    }
}

void interp_set_return_value(interp_handle handle, interp_word value) {
    auto i = static_cast<interp::interpreter*>(handle);
    i->set_return_value(value);
}

/// ===========================================================================
///  Linker.
/// ===========================================================================
interp_code interp_library_call_unsafe(
    interp_handle handle,
    const char* name,
    const char* func,
    size_t argc
) {
    auto i = static_cast<interp::interpreter*>(handle);
    try {
        i->create_library_call_unsafe(name, func, argc);
        return INTERP_OK;
    } catch (const std::exception& e) {
        i->last_error = e.what();
        return INTERP_ERR;
    }
}

/// ===========================================================================
///  Memory.
/// ===========================================================================
interp_code interp_create_alloca(interp_handle handle, size_t size, interp_address* address) {
    auto i = static_cast<interp::interpreter*>(handle);
    try {
        if (address) *address = i->create_alloca(size);
        return INTERP_OK;
    } catch (const std::exception& e) {
        i->last_error = e.what();
        return INTERP_ERR;
    }
}

interp_code interp_create_global(interp_handle handle, size_t size, interp_address* address) {
    auto i = static_cast<interp::interpreter*>(handle);
    try {
        if (address) *address = +i->create_global(size);
        return INTERP_OK;
    } catch (const std::exception& e) {
        i->last_error = e.what();
        return INTERP_ERR;
    }
}

interp_code interp_create_load(interp_handle handle, interp_reg r, interp_address p) {
    auto i = static_cast<interp::interpreter*>(handle);
    try {
        i->create_load(static_cast<interp::reg>(r), static_cast<interp::ptr>(p));
        return INTERP_OK;
    } catch (const std::exception& e) {
        i->last_error = e.what();
        return INTERP_ERR;
    }
}

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
) {
    auto i = static_cast<interp::interpreter*>(handle);
    try {
        switch (sz) {
            case INTERP_SIZE_MASK_64: i->create_load(static_cast<interp::reg>(dest), static_cast<u64*>(src)); break;
            case INTERP_SIZE_MASK_32: i->create_load(static_cast<interp::reg>(dest), static_cast<u32*>(src)); break;
            case INTERP_SIZE_MASK_16: i->create_load(static_cast<interp::reg>(dest), static_cast<u16*>(src)); break;
            case INTERP_SIZE_MASK_8: i->create_load(static_cast<interp::reg>(dest), static_cast<u8*>(src));  break;
            default: throw std::runtime_error(fmt::format("Invalid size: {}", static_cast<int>(sz)));
        }
        return INTERP_OK;
    } catch (const std::exception& e) {
        i->last_error = e.what();
        return INTERP_ERR;
    }
}

interp_code interp_create_load_indirect(
    interp_handle handle,
    interp_reg dest,
    interp_reg src,
    interp_word offs
) {
    auto i = static_cast<interp::interpreter*>(handle);
    try {
        i->create_load(static_cast<interp::reg>(dest), static_cast<interp::reg>(src), offs);
        return INTERP_OK;
    } catch (const std::exception& e) {
        i->last_error = e.what();
        return INTERP_ERR;
    }
}

interp_code interp_create_store(interp_handle handle, interp_address dest, interp_reg src) {
    auto i = static_cast<interp::interpreter*>(handle);
    try {
        i->create_store(static_cast<interp::ptr>(dest), static_cast<interp::reg>(src));
        return INTERP_OK;
    } catch (const std::exception& e) {
        i->last_error = e.what();
        return INTERP_ERR;
    }
}

interp_code interp_create_store_native(
    interp_handle handle,
    void* dest,
    interp_reg src,
    interp_size_mask sz
) {
    auto i = static_cast<interp::interpreter*>(handle);
    try {
        switch (sz) {
            case INTERP_SIZE_MASK_64: i->create_store(static_cast<u64*>(dest), static_cast<interp::reg>(src)); break;
            case INTERP_SIZE_MASK_32: i->create_store(static_cast<u32*>(dest), static_cast<interp::reg>(src)); break;
            case INTERP_SIZE_MASK_16: i->create_store(static_cast<u16*>(dest), static_cast<interp::reg>(src)); break;
            case INTERP_SIZE_MASK_8: i->create_store(static_cast<u8*>(dest), static_cast<interp::reg>(src));  break;
            default: throw std::runtime_error(fmt::format("Invalid size: {}", static_cast<int>(sz)));
        }
        return INTERP_OK;
    } catch (const std::exception& e) {
        i->last_error = e.what();
        return INTERP_ERR;
    }
}

interp_code interp_create_store_indirect(
    interp_handle handle,
    interp_reg dest,
    interp_word offs,
    interp_reg src
) {
    auto i = static_cast<interp::interpreter*>(handle);
    try {
        i->create_store(static_cast<interp::reg>(dest), offs, static_cast<interp::reg>(src));
        return INTERP_OK;
    } catch (const std::exception& e) {
        i->last_error = e.what();
        return INTERP_ERR;
    }
}

/// ===========================================================================
///  Operations.
/// ===========================================================================
void interp_create_return(interp_handle handle) {
    auto i = static_cast<interp::interpreter*>(handle);
    i->create_return();
}

interp_code interp_create_move_rr(
    interp_handle handle,
    interp_reg dest,
    interp_reg src
) {
    auto i = static_cast<interp::interpreter*>(handle);
    try {
        i->create_move(static_cast<reg>(dest), static_cast<reg>(src));
        return INTERP_OK;
    } catch (const std::exception& e) {
        i->last_error = e.what();
        return INTERP_ERR;
    }
}

interp_code interp_create_move_ri(
    interp_handle handle,
    interp_reg dest,
    interp_word value
) {
    auto i = static_cast<interp::interpreter*>(handle);
    try {
        i->create_move(static_cast<reg>(dest), value);
        return INTERP_OK;
    } catch (const std::exception& e) {
        i->last_error = e.what();
        return INTERP_ERR;
    }
}

interp_code interp_create_call(interp_handle handle, const char* name) {
    auto i = static_cast<interp::interpreter*>(handle);
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
    try {
        i->create_branch(target);
        return INTERP_OK;
    } catch (const std::exception& e) {
        i->last_error = e.what();
        return INTERP_ERR;
    }
}

interp_code interp_create_branch_ifnz(interp_handle handle, interp_reg cond, interp_address target) {
    auto i = static_cast<interp::interpreter*>(handle);
    try {
        i->create_branch_ifnz(static_cast<reg>(cond), target);
        return INTERP_OK;
    } catch (const std::exception& e) {
        i->last_error = e.what();
        return INTERP_ERR;
    }
}

interp_code interp_create_function(interp_handle handle, const char* name) {
    auto i = static_cast<interp::interpreter*>(handle);
    try {
        i->create_function(name);
        return INTERP_OK;
    } catch (const std::exception& e) {
        i->last_error = e.what();
        return INTERP_ERR;
    }
}

interp_code interp_create_xchg_rr(interp_handle handle, interp_reg r1, interp_reg r2) {
    auto i = static_cast<interp::interpreter*>(handle);
    try {
        i->create_xchg(static_cast<reg>(r1), static_cast<reg>(r2));
        return INTERP_OK;
    } catch (const std::exception& e) {
        i->last_error = e.what();
        return INTERP_ERR;
    }
}

interp_address interp_current_address(interp_handle handle) {
    auto i = static_cast<interp::interpreter*>(handle);
    return i->current_addr();
}

/// ===========================================================================
///  Arithmetic and bitwise instructions.
/// ===========================================================================
#define CREATE_OP(name, ...)                                                                          \
    interp_code interp_create_##name##_rr(                                                            \
        interp_handle handle,                                                                         \
        interp_reg dest,                                                                              \
        interp_reg src1,                                                                              \
        interp_reg src2                                                                               \
    ) {                                                                                               \
        auto i = static_cast<interp::interpreter*>(handle);                                           \
        try {                                                                                         \
            i->create_##name(static_cast<reg>(dest), static_cast<reg>(src1), static_cast<reg>(src2)); \
            return INTERP_OK;                                                                         \
        } catch (const std::exception& e) {                                                           \
            i->last_error = e.what();                                                                 \
            return INTERP_ERR;                                                                        \
        }                                                                                             \
    }                                                                                                 \
    interp_code interp_create_##name##_ri(                                                            \
        interp_handle handle,                                                                         \
        interp_reg dest,                                                                              \
        interp_reg src,                                                                               \
        interp_word value                                                                             \
    ) {                                                                                               \
        auto i = static_cast<interp::interpreter*>(handle);                                           \
        try {                                                                                         \
            i->create_##name(static_cast<reg>(dest), static_cast<reg>(src), value);                   \
            return INTERP_OK;                                                                         \
        } catch (const std::exception& e) {                                                           \
            i->last_error = e.what();                                                                 \
            return INTERP_ERR;                                                                        \
        }                                                                                             \
    }                                                                                                 \
    interp_code interp_create_##name##_ir(                                                            \
        interp_handle handle,                                                                         \
        interp_reg dest,                                                                              \
        interp_word value,                                                                            \
        interp_reg src                                                                                \
    ) {                                                                                               \
        auto i = static_cast<interp::interpreter*>(handle);                                           \
        try {                                                                                         \
            i->create_##name(static_cast<reg>(dest), value, static_cast<reg>(src));                   \
            return INTERP_OK;                                                                         \
        } catch (const std::exception& e) {                                                           \
            i->last_error = e.what();                                                                 \
            return INTERP_ERR;                                                                        \
        }                                                                                             \
    }

INTERP_ALL_ARITHMETIC_INSTRUCTIONS(CREATE_OP)

#undef CREATE_OP

} // extern "C"