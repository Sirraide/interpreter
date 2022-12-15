#include <algorithm>
#include <fmt/color.h>
#include <interpreter/internal.hh>
#include <interpreter/interp.hh>
#include <ranges>
#include <utility>

#ifndef _WIN32
#    include <dlfcn.h>
#else
#    include <windows.h>
#endif

#define L(x) \
    x:

namespace ranges = std::ranges;
namespace views = std::views;
using namespace interp::integers;

/// ===========================================================================
///  Miscellaneous.
/// ===========================================================================
interp::interpreter::interpreter() {
    /// Push an invalid instruction to make sure jumps to 0 throw.
    bytecode.push_back(+opcode::invalid);

    /// Make sure the bottommost address is unused so that NULL is always invalid.
    gp = static_cast<ptr>(1);

    /// Create the entry point.
    create_function("__entry__");
}

interp::interpreter::~interpreter() noexcept {
    /// Unload all libraries.
    for (auto& [_, lib] : libraries) {
#ifndef _WIN32
        dlclose(lib.handle);
#else
        FreeLibrary((HMODULE) lib.handle);
#endif
    }
}

void interp::interpreter::defun(const std::string& name, interp::native_function func) {
    /// Function is already declared.
    if (auto it = functions_map.find(name); it != functions_map.end()) {
        /// Duplicate function definition.
        if (not std::holds_alternative<std::monostate>(functions.at(it->second).address))
            throw error("Function '{}' is already defined.", name);

        /// Resolve the forward declaration.
        functions[it->second].address = func;
    }

    /// Add the function.
    else {
        functions_map[name] = functions.size();
        functions.push_back({});
        functions.back().address = func;
    }
}

interp::word interp::interpreter::arg(usz index, interp_size_mask sz) const {
    /// r2 is the first argument register.
    index += 2;

    /// Check that the register is valid.
    if (index < 2 or index >= _registers_.size())
        throw error("Argument index {} is out of bounds.", index);

    /// Return the value.
    return read_register(static_cast<reg>(index) | sz);
}

interp::word interp::interpreter::load_mem(ptr p, usz sz) const {
    /// Make sure the pointer is valid.
    if (not +p or +p >= _memory_.size()) [[unlikely]] { throw error("Segmentation fault. Invalid pointer: {:#08x}", +p); }

    /// Return the value.
    switch (sz) {
        case 1: return _memory_[+p];
        case 2: return *reinterpret_cast<const u16*>(_memory_.data() + +p);
        case 4: return *reinterpret_cast<const u32*>(_memory_.data() + +p);
        case 8: return *reinterpret_cast<const u64*>(_memory_.data() + +p);
        default: throw error("Invalid size: {}", sz);
    }
}

void interp::interpreter::push(word value) {
    if (+sp == max_memory) throw error("Stack overflow");
    *reinterpret_cast<word*>(_memory_.data() + +sp) = value;
    sp = static_cast<ptr>(+sp + sizeof(word));
}

auto interp::interpreter::pop() -> word {
    if (sp <= gp) throw error("Stack underflow");
    sp = static_cast<ptr>(+sp - sizeof(word));
    return *reinterpret_cast<word*>(_memory_.data() + +sp);
}

interp::word interp::interpreter::r(reg r) const {
    return read_register(r);
}

void interp::interpreter::r(reg r, word value) {
    set_register(r, value);
}

void interp::interpreter::set_return_value(word value) {
    _registers_[1] = value;
}

void interp::interpreter::store_mem(ptr p, word value, usz sz) {
    /// Make sure the pointer is valid.
    if (not +p or +p >= _memory_.size()) [[unlikely]] { throw error("Segmentation fault. Invalid pointer: {:#08x}", +p); }

    /// Store the value.
    switch (sz) {
        case 1: _memory_[+p] = static_cast<u8>(value); break;
        case 2: *reinterpret_cast<u16*>(_memory_.data() + +p) = static_cast<u16>(value); break;
        case 4: *reinterpret_cast<u32*>(_memory_.data() + +p) = static_cast<u32>(value); break;
        case 8: *reinterpret_cast<u64*>(_memory_.data() + +p) = static_cast<u64>(value); break;
        default: throw error("Invalid size: {}", sz);
    }
}

constexpr static usz address_operand_size(interp::opcode op) {
    switch (op) {
        case interp::opcode::call8:
        case interp::opcode::jmp8:
        case interp::opcode::jnz8:
        case interp::opcode::load8:
        case interp::opcode::store8:
        case interp::opcode::load_rel8:
        case interp::opcode::store_rel8:
            return 1;

        case interp::opcode::call16:
        case interp::opcode::jmp16:
        case interp::opcode::jnz16:
        case interp::opcode::load16:
        case interp::opcode::store16:
        case interp::opcode::load_rel16:
        case interp::opcode::store_rel16:
            return 2;

        case interp::opcode::call32:
        case interp::opcode::jmp32:
        case interp::opcode::jnz32:
        case interp::opcode::load32:
        case interp::opcode::store32:
        case interp::opcode::load_rel32:
        case interp::opcode::store_rel32:
            return 4;

        case interp::opcode::call64:
        case interp::opcode::jmp64:
        case interp::opcode::jnz64:
        case interp::opcode::load64:
        case interp::opcode::store64:
        case interp::opcode::load_rel64:
        case interp::opcode::store_rel64:
            return 8;

        default: return 0;
    }
}

constexpr static usz register_size(interp::reg r) {
    switch (+r & interp::osz_mask) {
        case INTERP_SIZE_MASK_8: return 1;
        case INTERP_SIZE_MASK_16: return 2;
        case INTERP_SIZE_MASK_32: return 4;
        case INTERP_SIZE_MASK_64: return 8;
        default: std::unreachable();
    }
}

/// ===========================================================================
///  Instruction Encoder.
/// ===========================================================================
static void write_imm(std::vector<u8>& bytecode, interp::word imm) {
    /// Encode the immediate.
    if (imm > UINT32_MAX) {
        bytecode.resize(bytecode.size() + 8);
        std::memcpy(bytecode.data() + bytecode.size() - 8, &imm, 8);
    } else {
        bytecode.resize(bytecode.size() + 4);
        std::memcpy(bytecode.data() + bytecode.size() - 4, &imm, 4);
    }
}

static void write_addr(std::vector<u8>& bytecode, interp::word imm) {
    usz sz;
    if (imm < UINT8_MAX) sz = 1;
    else if (imm < UINT16_MAX) sz = 2;
    else if (imm < UINT32_MAX) sz = 4;
    else sz = 8;
    bytecode.resize(bytecode.size() + sz);
    std::memcpy(bytecode.data() + bytecode.size() - sz, &imm, sz);
}

static bool is_imm(interp::reg r) {
    return r == interp::reg::arith_imm_32 or r == interp::reg::arith_imm_64;
}

void interp::interpreter::encode_arithmetic(opcode op, reg rdest, reg r1, reg r2) {
    /// These are invalid here.
    if (is_imm(r1) or is_imm(r2))
        throw error("This overload of encode_arithmetic() cannot be used with source registers 0 or arith_imm_64.");

    /// Make sure the registers are valid.
    check_regs(rdest, r1, r2);

    /// Encode the instruction.
    bytecode.push_back(+op);
    bytecode.push_back(+rdest);
    bytecode.push_back(+r1);
    bytecode.push_back(+r2);
}

void interp::interpreter::encode_arithmetic(opcode op, reg dest, reg src, word imm) {
    /// Invalid here.
    if (is_imm(src)) throw error("Source register may not be 0 or reg::arith_imm_64.");

    /// Make sure the registers are valid.
    check_regs(dest, src);

    /// Encode the instruction.
    bytecode.push_back(+op);
    bytecode.push_back(+dest);
    bytecode.push_back(+src);
    bytecode.push_back(imm > UINT32_MAX ? +reg::arith_imm_64 : +reg::arith_imm_32);

    /// Encode the immediate.
    write_imm(bytecode, imm);
}

void interp::interpreter::encode_arithmetic(opcode op, reg dest, word imm, reg src) {
    /// Invalid here.
    if (is_imm(src)) throw error("Source register may not be 0 or reg::arith_imm_64.");

    /// Make sure the registers are valid.
    check_regs(dest, src);

    /// Encode the instruction.
    bytecode.push_back(+op);
    bytecode.push_back(+dest);
    bytecode.push_back(imm > UINT32_MAX ? +reg::arith_imm_64 : +reg::arith_imm_32);
    bytecode.push_back(+src);

    /// Encode the immediate.
    write_imm(bytecode, imm);
}

interp::interpreter::arith_t interp::interpreter::decode_arithmetic() {
    /// Decode the registers.
    auto dest = bytecode[ip++];
    auto reg_src1 = bytecode[ip++];
    auto reg_src2 = bytecode[ip++];

    /// Sanity check.
    if ((reg_src1 == +reg::arith_imm_32 or reg_src2 == +reg::arith_imm_64) and //
        (reg_src1 == +reg::arith_imm_64 or reg_src2 == +reg::arith_imm_32))
        throw error("Invalid arithmetic instruction.");

    /// Either src1 or src2 may be a 32/64 bit immediate.
    const auto decode_value = [&](u8 reg_src) -> word {
        /// 32 bit immediate.
        if (reg_src == +reg::arith_imm_32) {
            word imm{};
            std::memcpy(&imm, bytecode.data() + ip, 4);
            ip += 4;
            return imm;
        }

        /// 64 bit immediate.
        if (reg_src == +reg::arith_imm_64) {
            word imm{};
            std::memcpy(&imm, bytecode.data() + ip, 8);
            ip += 8;
            return imm;
        }

        /// Register.
        return read_register(static_cast<reg>(reg_src));
    };

    /// Extract the source values.
    word src1 = decode_value(reg_src1);
    word src2 = decode_value(reg_src2);

    /// Return the instruction.
    return {static_cast<reg>(dest), src1, src2};
}

interp::word interp::interpreter::read_sized_address_at_ip(opcode op) {
    usz sz = address_operand_size(op);
    if (not sz) throw error("Opcode {} does not support read_sized_address_at_ip()", static_cast<u8>(op));

    word value{};
    std::memcpy(&value, bytecode.data() + ip, sz);
    ip += sz;
    return value;
}

void interp::interpreter::set_register(reg r, word value) {
    switch (+r & osz_mask) {
        case INTERP_SIZE_MASK_8: *reinterpret_cast<u8*>(&_registers_[index(r)]) = static_cast<u8>(value); break;
        case INTERP_SIZE_MASK_16: *reinterpret_cast<u16*>(&_registers_[index(r)]) = static_cast<u16>(value); break;
        case INTERP_SIZE_MASK_32: *reinterpret_cast<u32*>(&_registers_[index(r)]) = static_cast<u32>(value); break;
        case INTERP_SIZE_MASK_64: _registers_[index(r)] = value; break;
        default: std::unreachable();
    }
}

interp::word interp::interpreter::read_register(reg r) const {
    switch (+r & osz_mask) {
        case INTERP_SIZE_MASK_8: return *reinterpret_cast<const u8*>(&_registers_[index(r)]);
        case INTERP_SIZE_MASK_16: return *reinterpret_cast<const u16*>(&_registers_[index(r)]);
        case INTERP_SIZE_MASK_32: return *reinterpret_cast<const u32*>(&_registers_[index(r)]);
        case INTERP_SIZE_MASK_64: return _registers_[index(r)];
        default: std::unreachable();
    }
}

/// ===========================================================================
///  Linker.
/// ===========================================================================
/// TODO: Write a tool that uses libtooling to generate
///       signatures and allow for type-safe-ish calls?
void interp::interpreter::create_library_call_unsafe(const std::string& library_path, const std::string& function_name, usz num_params) {
    /// Load the library.
    library* lib;
    if (auto it = libraries.find(library_path); it != libraries.end()) {
        lib = &it->second;
    } else {
#ifndef _WIN32
        void* handle = dlopen(library_path.c_str(), RTLD_LAZY);
        if (not handle) throw error("Failed to load library {}: {}", library_path, dlerror());
#else
        void* handle = (void*) LoadLibraryA(library_path.c_str());
        if (not handle) throw error("Failed to load library {}: {}", library_path, GetLastError());
#endif
        libraries[library_path] = {};
        lib = &libraries[library_path];
        lib->handle = handle;
    }

    /// If the function has already been added, just call it.
    if (auto f = lib->functions.find(function_name); f != lib->functions.end()) {
        create_call_internal(f->second);
        return;
    }

    /// Otherwise, search for the function.
#ifndef _WIN32
    auto sym = dlsym(lib->handle, function_name.c_str());
    if (not sym) throw error("Failed to load function \"{}\" from library {}: {}", function_name, library_path, dlerror());
#else
    auto sym = (void*) GetProcAddress((HMODULE) lib->handle, function_name.c_str());
    if (not sym) throw error("Failed to load function \"{}\" from library {}: {}", function_name, library_path, GetLastError());
#endif

    /// Create the function.
    functions.push_back({});
    functions.back().address = library_function{sym, num_params, function_name};

    /// Add the function to the library.
    lib->functions[function_name] = functions.size() - 1;

    /// Create the call.
    create_call_internal(functions.size() - 1);
}


/// ===========================================================================
///  Memory.
/// ===========================================================================
/// Allocate memory on the stack.
interp::word interp::interpreter::create_alloca(usz size) {
    size = std::max(size, sizeof(word));
    auto& f = functions[current_function];
    auto p = f.locals_size;
    f.locals_size += size;
    return p;
}

/// Create a global variable.
interp::ptr interp::interpreter::create_global(usz size) {
    size = std::max(size, sizeof(word));
    if (+gp + size > max_memory) throw error("Global memory overflow.");
    auto p = gp;
    gp = static_cast<ptr>(+gp + size);
    return p;
}

/// Load from memory.
void interp::interpreter::create_load(reg dest, ptr src) {
    /// Check that the pointer is valid.
    if (not +src or +src >= max_memory) throw error("Segmentation fault. Invalid pointer: {}", +src);

    /// Make sure the destination is a register.
    check_regs(dest);

    /// Write the opcode.
    if (+src < UINT8_MAX) bytecode.push_back(+opcode::load8);
    else if (+src < UINT16_MAX) bytecode.push_back(+opcode::load16);
    else if (+src < UINT32_MAX) bytecode.push_back(+opcode::load32);
    else bytecode.push_back(+opcode::load64);

    /// Write the destination register and source address.
    bytecode.push_back(+dest);
    write_addr(bytecode, +src);
}

/// Indirect load from memory.
void interp::interpreter::create_load(reg dest, reg src, word offs) {
    /// Make sure the registers are valid.
    check_regs(dest, src);

    /// Write the opcode.
    if (offs < UINT8_MAX) bytecode.push_back(+opcode::load_rel8);
    else if (offs < UINT16_MAX) bytecode.push_back(+opcode::load_rel16);
    else if (offs < UINT32_MAX) bytecode.push_back(+opcode::load_rel32);
    else bytecode.push_back(+opcode::load_rel64);

    /// Write the destination and source registers.
    bytecode.push_back(+dest);
    bytecode.push_back(+src);

    /// Write the offset.
    write_addr(bytecode, offs);
}

/// Store to memory.
void interp::interpreter::create_store(ptr dest, reg src) {
    /// Check that the pointer is valid.
    if (not +dest or +dest >= max_memory) throw error("Segmentation fault. Invalid pointer: {}", +dest);

    /// Make sure the source is a register.
    check_regs(src);

    /// Write the opcode.
    if (+dest < UINT8_MAX) bytecode.push_back(+opcode::store8);
    else if (+dest < UINT16_MAX) bytecode.push_back(+opcode::store16);
    else if (+dest < UINT32_MAX) bytecode.push_back(+opcode::store32);
    else bytecode.push_back(+opcode::store64);

    /// Write the destination address and source register.
    bytecode.push_back(+src);
    write_addr(bytecode, +dest);
}

/// Indirect store to memory.
void interp::interpreter::create_store(reg dest, word offs, reg src) {
    /// Make sure the registers are valid.
    check_regs(dest, src);

    /// Write the opcode.
    if (offs < UINT8_MAX) bytecode.push_back(+opcode::store_rel8);
    else if (offs < UINT16_MAX) bytecode.push_back(+opcode::store_rel16);
    else if (offs < UINT32_MAX) bytecode.push_back(+opcode::store_rel32);
    else bytecode.push_back(+opcode::store_rel64);

    /// Write the destination and source registers.
    bytecode.push_back(+dest);
    bytecode.push_back(+src);

    /// Write the offset.
    write_addr(bytecode, offs);
}

/// ===========================================================================
///  Operations.
/// ===========================================================================
void interp::interpreter::create_return() { bytecode.push_back(+opcode::ret); }

void interp::interpreter::create_move(reg dest, reg src) {
    /// Make sure the registers are valid.
    check_regs(dest, src);

    /// Encode the instruction.
    bytecode.push_back(+opcode::mov);
    bytecode.push_back(+dest);
    bytecode.push_back(+src);
}

void interp::interpreter::create_move(reg dest, word imm) {
    /// Make sure the registers are valid.
    check_regs(dest);

    /// Encode the instruction.
    bytecode.push_back(+opcode::mov);
    bytecode.push_back(+dest);
    bytecode.push_back(imm > UINT32_MAX ? +reg::arith_imm_64 : +reg::arith_imm_32);

    /// Encode the immediate.
    write_imm(bytecode, imm);
}

#define ARITH(name, ...)                                                                   \
    void interp::interpreter::INTERP_CAT(create_, name)(reg dest, reg src1, reg src2) /**/ \
    { encode_arithmetic(opcode::name, dest, src1, src2); }                                 \
    void interp::interpreter::INTERP_CAT(create_, name)(reg dest, reg src, word imm) /**/  \
    { encode_arithmetic(opcode::name, dest, src, imm); }                                   \
    void interp::interpreter::INTERP_CAT(create_, name)(reg dest, word imm, reg src) /**/  \
    { encode_arithmetic(opcode::name, dest, imm, src); }
INTERP_ALL_ARITHMETIC_INSTRUCTIONS(ARITH)
#undef ARITH

void interp::interpreter::create_call_internal(usz index) {
    if (index < UINT8_MAX) bytecode.push_back(+opcode::call8);
    else if (index < UINT16_MAX) bytecode.push_back(+opcode::call16);
    else if (index < UINT32_MAX) bytecode.push_back(+opcode::call32);
    else bytecode.push_back(+opcode::call64);
    write_addr(bytecode, index);
}

void interp::interpreter::create_call(const std::string& name) {
    /// Make sure the function exists.
    auto it = functions_map.find(name);

    /// Function not found. Add an empty record.
    if (it == functions_map.end()) {
        /// Push the opcode and call index.
        create_call_internal(functions.size());

        /// Add the record.
        functions_map[name] = functions.size();
        functions.push_back({});
    }

    /// Function found. Push the opcode and call index.
    else { create_call_internal(it->second); }
}

void interp::interpreter::create_branch(addr target) {
    /// Push the opcode and target.
    if (target < UINT8_MAX) bytecode.push_back(+opcode::jmp8);
    else if (target < UINT16_MAX) bytecode.push_back(+opcode::jmp16);
    else if (target < UINT32_MAX) bytecode.push_back(+opcode::jmp32);
    else bytecode.push_back(+opcode::jmp64);
    write_addr(bytecode, target);
}

void interp::interpreter::create_branch_ifnz(reg cond, addr target) {
    /// Push the opcode, condition and target.
    if (target < UINT8_MAX) bytecode.push_back(+opcode::jnz8);
    else if (target < UINT16_MAX) bytecode.push_back(+opcode::jnz16);
    else if (target < UINT32_MAX) bytecode.push_back(+opcode::jnz32);
    else bytecode.push_back(+opcode::jnz64);
    bytecode.push_back(+cond);
    write_addr(bytecode, target);
}

void interp::interpreter::create_function(const std::string& name) {
    /// Make sure the function doesn’t already exist.
    if (auto it = functions_map.find(name); it != functions_map.end()) {
        if (not std::holds_alternative<std::monostate>(functions[it->second].address)) throw error("Function already exists.");
        functions[it->second].address = addr{bytecode.size()};
        current_function = it->second;
    }

    /// Add the function.
    else {
        functions_map[name] = current_function = functions.size();
        functions.push_back({});
        functions.back().address = addr{bytecode.size()};

    }
}

auto interp::interpreter::current_addr() const -> addr { return bytecode.size(); }

/// ===========================================================================
///  Execute bytecode.
/// ===========================================================================
interp::word interp::interpreter::run() {
    /// Make sure the memory has the right size.
    _memory_.resize(max_memory);

    /// Set the instruction pointer to the entry point.
    ip = ip_start_addr;

    /// Allocate memory on the stack for the local variables of the entry point.
    const auto zero_frame_ptr = static_cast<ptr>(+gp + functions[0].locals_size);
    sp = zero_frame_ptr;

    /// Set the base of the stack.
    stack_base = zero_frame_ptr;


    /// Initialise registers.
    for (auto& reg : _registers_) reg = 0;

    /// Run the code.
    for (;;) {
        if (ip >= bytecode.size()) [[unlikely]] { throw error("Instruction pointer out of bounds."); }
        switch (auto op = static_cast<opcode>(bytecode[ip++])) {
            static_assert(opcode_t(opcode::max_opcode) == 43);
            default: throw error("Invalid opcode {}", u8(op));

            /// Do nothing.
            case opcode::nop: break;

            /// Return from a function.
            case opcode::ret: {
                /// Top stack frame. Halt the interpreter and return the value in the return register.
                if (stack_base == zero_frame_ptr) return _registers_[1];

                /// Pop the stack frame.
                sp = stack_base;
                stack_base = static_cast<ptr>(pop());
                ip = pop();
            } break;

            /// Move an immediate or a value from one register to another.
            case opcode::mov: {
                auto dest = static_cast<reg>(bytecode[ip++]);
                auto src = static_cast<reg>(bytecode[ip++]);

                /// Immediate.
                if (src == reg::arith_imm_32 or src == reg::arith_imm_64) {
                    word imm{};
                    std::memcpy(&imm, bytecode.data() + ip, src == reg::arith_imm_32 ? 4 : 8);
                    ip += src == reg::arith_imm_32 ? 4 : 8;
                    set_register(dest, imm);
                }

                /// Register.
                else { set_register(dest, read_register(src)); }
            } break;

            /// Load a value from memory.
            case opcode::load8:
            case opcode::load16:
            case opcode::load32:
            case opcode::load64: {
                auto dest = static_cast<reg>(bytecode[ip++]);
                auto p = read_sized_address_at_ip(op);

                /// Load the value.
                set_register(dest, load_mem(static_cast<ptr>(p), register_size(dest)));
            } break;

            /// Indirect load from memory.
            case opcode::load_rel8:
            case opcode::load_rel16:
            case opcode::load_rel32:
            case opcode::load_rel64: {
                auto dest = static_cast<reg>(bytecode[ip++]);
                auto src = static_cast<reg>(bytecode[ip++]);
                auto offset = read_sized_address_at_ip(op);
                ptr source_address{};

                /// Compute the source address. Here, r0 is the stack base pointer.
                source_address = (+src == 0 ? stack_base : static_cast<ptr>(read_register(src))) + offset;

                /// Load the value.
                set_register(dest, load_mem(source_address, register_size(dest)));
            } break;

            /// Store a value to memory.
            case opcode::store8:
            case opcode::store16:
            case opcode::store32:
            case opcode::store64: {
                auto src = static_cast<reg>(bytecode[ip++]);
                auto p = read_sized_address_at_ip(op);

                /// Store the value.
                store_mem(static_cast<ptr>(p), read_register(src), register_size(src));
            } break;

            /// Indirect store to memory.
            case opcode::store_rel8:
            case opcode::store_rel16:
            case opcode::store_rel32:
            case opcode::store_rel64: {
                auto dest = static_cast<reg>(bytecode[ip++]);
                auto src = static_cast<reg>(bytecode[ip++]);
                auto offset = read_sized_address_at_ip(op);
                ptr dest_address{};

                /// Compute the destination address. Here, r0 is the stack base pointer.
                dest_address = (+dest == 0 ? stack_base : static_cast<ptr>(read_register(dest))) + offset;

                /// Store the value.
                store_mem(dest_address, read_register(src), register_size(src));
            } break;

            /// Add two integers.
            case opcode::add: {
                auto [dest, src1, src2] = decode_arithmetic();
                set_register(dest, src1 + src2);
            } break;

            /// Subtract two integers.
            case opcode::sub: {
                auto [dest, src1, src2] = decode_arithmetic();
                set_register(dest, src1 - src2);
            } break;

            /// Multiply two integers (signed).
            case opcode::muli: {
                auto [dest, src1, src2] = decode_arithmetic();
                set_register(dest, word(i64(src1) * i64(src2)));
            } break;

            /// Multiply two integers (unsigned).
            case opcode::mulu: {
                auto [dest, src1, src2] = decode_arithmetic();
                set_register(dest, src1 * src2);
            } break;

            /// Divide two integers (signed).
            case opcode::divi: {
                auto [dest, src1, src2] = decode_arithmetic();
                set_register(dest, word(i64(src1) / i64(src2)));
            } break;

            /// Divide two integers (unsigned).
            case opcode::divu: {
                auto [dest, src1, src2] = decode_arithmetic();
                set_register(dest, src1 / src2);
            } break;

            /// Remainder of two integers (signed).
            case opcode::remi: {
                auto [dest, src1, src2] = decode_arithmetic();
                set_register(dest, word(i64(src1) % i64(src2)));
            } break;

            /// Remainder of two integers (unsigned).
            case opcode::remu: {
                auto [dest, src1, src2] = decode_arithmetic();
                set_register(dest, src1 % src2);
            } break;

            /// Shift left.
            case opcode::shift_left: {
                auto [dest, src1, src2] = decode_arithmetic();
                set_register(dest, src1 << (src2 & 63));
            } break;

            /// Logical shift right.
            case opcode::shift_right_logical: {
                auto [dest, src1, src2] = decode_arithmetic();
                set_register(dest, src1 >> (src2 & 63));
            } break;

            /// Arithmetic shift right.
            case opcode::shift_right_arithmetic: {
                auto [dest, src1, src2] = decode_arithmetic();
                set_register(dest, word(i64(src1) >> (src2 & 63)));
            } break;

            /// Call a function.
            case opcode::call8:
            case opcode::call16:
            case opcode::call32:
            case opcode::call64: {
                auto index = read_sized_address_at_ip(op);

                /// Make sure the index is valid.
                if (index >= functions.size()) [[unlikely]] { throw error("Call index out of bounds"); }

                /// If it’s a native function, call it.
                auto& func = functions.at(index);
                if (std::holds_alternative<native_function>(func.address)) {
                    std::get<native_function>(func.address)(*this);
                }

                /// Otherwise, push the return address and jump to the function.
                else if (std::holds_alternative<addr>(func.address)) {
                    push(ip);
                    push(static_cast<word>(stack_base));
                    stack_base = sp;
                    sp = static_cast<ptr>(+sp + func.locals_size);
                    ip = std::get<addr>(func.address);

                    /// Make sure we didn’t overflow the stack.
                    if (+sp >= max_memory) [[unlikely]] { throw error("Stack overflow"); }
                }

                /// If it’s a library function, we need to do some black magic.
                else if (std::holds_alternative<library_function>(func.address)) {
                    auto& lib_func = std::get<library_function>(func.address);
                    do_library_call_unsafe(lib_func);
                }

                /// Unknown function.
                else {
                    /// Try to get the function name.
                    auto it = ranges::find_if(functions_map, [&](auto& f) { return f.second == index; });
                    if (it != functions_map.end()) throw error("Unknown function \"{}\" called.", it->first);
                    else throw error("Unknown function with index {} called.", index);
                }
            } break;

            /// Jump to an address.
            case opcode::jmp8:
            case opcode::jmp16:
            case opcode::jmp32:
            case opcode::jmp64: {
                ip = read_sized_address_at_ip(op);
                if (ip >= bytecode.size()) [[unlikely]] { throw error("Jump target out of bounds"); }
            } break;

            /// Jump to an address if the top of the stack is not zero.
            case opcode::jnz8:
            case opcode::jnz16:
            case opcode::jnz32:
            case opcode::jnz64: {
                reg r = static_cast<reg>(bytecode[ip++]);
                auto target = read_sized_address_at_ip(op);
                if (target >= bytecode.size()) [[unlikely]] { throw error("Jump target out of bounds"); }
                if (read_register(r)) ip = target;
            } break;
        }
    }
}

/// ===========================================================================
///  Disassembler.
/// ===========================================================================
std::string interp::interpreter::disassemble() const {
    std::string result;

    /*/// Determine the number of nonzero bytes of the greatest number in the bytecode.
    auto padd_to = ranges::max(bytecode | views::transform(std::bind_front(number_width, 16)));

    /// Round up to the nearest multiple of 2, and divide by 2.
    padd_to = (padd_to + (padd_to & 1)) / 2;*/

    /// Declare this here since it’s used by some of the lambdas below.
    usz i = ip_start_addr;

    /// For colours.
    using enum fmt::terminal_color;
    using fmt::fg;
    using fmt::styled;
    static constexpr auto orange = static_cast<fmt::color>(0xF59762);
    static constexpr auto dark_green = static_cast<fmt::color>(0x7DBDA2);
    static constexpr auto comma = styled(",", fg(white));
    static constexpr auto plus = styled("+", fg(white));
    static constexpr auto lbrack = styled("[", fg(white));
    static constexpr auto rbrack = styled("]", fg(white));

    /// String representation of a register.
    const auto reg_str = [](u8 r) {
        std::string_view suffix;
        switch (r & osz_mask) {
            case INTERP_SIZE_MASK_8: suffix = "b"; break;
            case INTERP_SIZE_MASK_16: suffix = "w"; break;
            case INTERP_SIZE_MASK_32: suffix = "d"; break;
            case INTERP_SIZE_MASK_64: suffix = ""; break;
            default: std::unreachable();
        }
        return fmt::format(fg(red), "r{}{}", index(static_cast<reg>(r)), suffix);
    };

    /// Print a register byte. As weird as `const u8&` looks, it’s necessary
    /// because the type returned by `styled()` stores a const& to the thing
    /// being styled.
    const auto rbyte = [](const u8& r) {
        return is_imm(static_cast<reg>(r)) ? styled(r, fg(white)) : styled(r, fg(red));
    };

    /// Print an arithmetic instruction.
    const auto print_arith = [&](auto&& str) { // clang-format off
        /// Bytes for the dest, src1, and src2.
        auto r = fmt::format_to(std::back_inserter(result), "{:02x} {:02x} {:02x} ",
            rbyte(bytecode[i]), rbyte(bytecode[i + 1]), rbyte(bytecode[i + 2]));

        /// Check if we have an immediate operand.
        usz imm = 0;
        if (bytecode[i + 1] == +reg::arith_imm_32 or bytecode[i + 1] == +reg::arith_imm_64) imm = 1;
        else if (bytecode[i + 2] == +reg::arith_imm_32 or bytecode[i + 2] == +reg::arith_imm_64) imm = 2;

        /// Extract the immediate value and print the first 4 bytes of the immediate if we have one.
        word imm_value{};
        if (imm) {
            std::memcpy(&imm_value, bytecode.data() + i + 3,
                bytecode[i + imm] == +reg::arith_imm_32 ? 4 : 8);

            r = fmt::format_to(r, fg(magenta), "{:02x} {:02x} {:02x} {:02x} ",
                bytecode[i + 3], bytecode[i + 4], bytecode[i + 5], bytecode[i + 6]);
        } else repeat (4) r = fmt::format_to(r, "   ");

        /// Print the mnemonic.
        r = fmt::format_to(r, "         {} {}", styled(str, fg(yellow)), reg_str(bytecode[i]));
        r = imm == 1
            ? fmt::format_to(r, "{} {}", comma, styled(imm_value, fg(magenta)))
            : fmt::format_to(r, "{} {}", comma, reg_str(bytecode[i + 1]));
        r = imm == 2
            ? fmt::format_to(r, "{} {}\n", comma, styled(imm_value, fg(magenta)))
            : fmt::format_to(r, "{} {}\n", comma, reg_str(bytecode[i + 2]));

        /// Print 4 more bytes if we have a a 64-bit immediate.
        if (imm and bytecode[i + imm] == +reg::arith_imm_64) {
            r = fmt::format_to(r, fg(orange), "[{:08x}]: ", i + 7);
            fmt::format_to(r, fg(magenta), "{:02x} {:02x} {:02x} {:02x}\n",
                bytecode[i + 7], bytecode[i + 8], bytecode[i + 9], bytecode[i + 10]);
        }

       /// Yeet the instruction.
       i += 3;
       if (imm) i += bytecode[i + imm] == +reg::arith_imm_32 ? 4zu : 8zu;
    }; // clang-format on

    /// Print an address and return it.
    const auto print_and_read_addr = [&](auto c, opcode op) {
        usz sz = address_operand_size(op);
        switch (sz) {
            case 1:
                result += fmt::format(fg(c), "{:02x}", bytecode[i]);
                break;

            case 2:
                result += fmt::format(fg(c), "{:02x} {:02x}", bytecode[i], bytecode[i + 1]);
                break;

            case 4:
                result += fmt::format(fg(c), "{:02x} {:02x} {:02x} {:02x}", //
                                      bytecode[i], bytecode[i + 1], bytecode[i + 2], bytecode[i + 3]);
                break;

            case 8:
                result += fmt::format(fg(c), "{:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x}",                //
                                      bytecode[i], bytecode[i + 1], bytecode[i + 2], bytecode[i + 3], bytecode[i + 4], //
                                      bytecode[i + 5], bytecode[i + 6], bytecode[i + 7]);
                break;

            default: result += fmt::format(fg(white), "???");
        }

        /// Extract the index.
        addr index{};
        std::memcpy(&index, bytecode.data() + i, sz);
        i += sz;
        return index;
    };

    /// Iterate over the bytecode and disassemble each instruction.
    while (i < bytecode.size()) {
        /// Check if there is a function that starts at this address.
        auto func = ranges::find_if(functions_map, [&](auto&& p) {
            return p.second < functions.size()
                   and std::holds_alternative<addr>(functions[p.second].address)
                   and std::get<addr>(functions[p.second].address) == i;
        });

        /// Print the function name if there is one.
        if (func != functions_map.end()) {
            if (i != 1) result += "\n";
            result += fmt::format(fg(green), "{}{}\n", func->first, styled(":", fg(orange)));
        }

        /// Print address.
        result += fmt::format(fg(orange), "[{:08x}]: ", i);
        if (i == 0) result += fmt::format(fg(white), "00 ", bytecode[i]);
        else if (bytecode[i] == 0) result += fmt::format(fg(bright_white), "{:02x} ", bytecode[i]);
        else result += fmt::format(fg(yellow), "{:02x} ", bytecode[i]);

        /// Print the instruction mnemonic.
        switch (auto op = static_cast<opcode>(bytecode[i++])) {
            static_assert(opcode_t(opcode::max_opcode) == 43);
            default:
                repeat (8) result += "   ";
                if (i == 1 and op == opcode::invalid) result += fmt::format(fg(white), "      .sentinel\n");
                else result += fmt::format(fg(bright_white), "      ???\n");
                break;
            case opcode::nop:
                repeat (8) result += "   ";
                result += fmt::format(fg(yellow), "      nop\n");
                break;
            case opcode::ret:
                repeat (8) result += "   ";
                result += fmt::format(fg(yellow), "      ret\n");
                break;

            case opcode::mov: { // clang-format off
                /// Bytes for the opcode and dest
                auto r = fmt::format_to(std::back_inserter(result), "{:02x} {:02x} ",
                    rbyte(bytecode[i]), rbyte(bytecode[i + 1]));

                /// Check if we have an immediate operand.
                bool imm = bytecode[i + 1] == +reg::arith_imm_32 or bytecode[i + 1] == +reg::arith_imm_64;

                /// Extract the immediate value and print the first 4 bytes of the immediate if we have one.
                word imm_value{};
                if (imm) {
                    std::memcpy(&imm_value, bytecode.data() + i + 2,
                        bytecode[i + imm] == +reg::arith_imm_32 ? 4 : 8);

                    r = fmt::format_to(r, fg(magenta), "{:02x} {:02x} {:02x} {:02x} ",
                        bytecode[i + 2], bytecode[i + 3], bytecode[i + 4], bytecode[i + 5]);
                } else repeat (4) r = fmt::format_to(r, "   ");

                /// Print the mnemonic.
                r = fmt::format_to(r, "            {}  {}", styled("mov", fg(yellow)), reg_str(bytecode[i]));
                r = imm
                    ? fmt::format_to(r, "{} {}\n", comma, styled(imm_value, fg(magenta)))
                    : fmt::format_to(r, "{} {}\n", comma, reg_str(bytecode[i + 1]));

                /// Print 4 more bytes if we have a a 64-bit immediate.
                if (imm and bytecode[i + 1] == +reg::arith_imm_64){
                    r = fmt::format_to(r, fg(orange), "[{:08x}]: ", i + 6);
                    fmt::format_to(r, fg(magenta), "{:02x} {:02x} {:02x} {:02x}\n",
                        bytecode[i + 6], bytecode[i + 7], bytecode[i + 8], bytecode[i + 9]);
                }

                /// Yeet the instruction.
                i += 2;
                if (imm) i += bytecode[i + 1] == +reg::arith_imm_32 ? 4zu : 8zu;
            } break; // clang-format on

            case opcode::load8:
            case opcode::load16:
            case opcode::load32:
            case opcode::load64: {
                /// Print the dest.
                auto regnum = bytecode[i++];
                auto r = fmt::format_to(std::back_inserter(result), "{:02x} ", rbyte(regnum));

                /// Print the address.
                auto addr = print_and_read_addr(dark_green, op);

                /// Align and print the mnemonic.
                repeat (8 - address_operand_size(op)) r = fmt::format_to(r, "   ");
                fmt::format_to(r, fg(yellow), "    ld   {}{} {}{}{}\n", reg_str(regnum), comma, lbrack, styled(addr, fg(dark_green)), rbrack);
            } break;

            case opcode::load_rel8:
            case opcode::load_rel16:
            case opcode::load_rel32:
            case opcode::load_rel64: {
                /// Print the dest and src.
                auto dest = bytecode[i++];
                auto src = bytecode[i++];
                auto r = fmt::format_to(std::back_inserter(result), fg(red), "{:02x} {:02x} ", dest, src);

                /// Print the address.
                auto addr = print_and_read_addr(dark_green, op);

                /// Align and print the mnemonic.
                repeat (8 - address_operand_size(op)) r = fmt::format_to(r, "   ");
                fmt::format_to(r, fg(yellow), " ld   {}{} {}{} {} {}{}\n", reg_str(dest), comma, lbrack, reg_str(src), plus, styled(addr, fg(dark_green)), rbrack);
            } break;

            case opcode::store8:
            case opcode::store16:
            case opcode::store32:
            case opcode::store64: {
                /// Print the source.
                auto regnum = bytecode[i++];
                auto r = fmt::format_to(std::back_inserter(result), "{:02x} ", rbyte(regnum));

                /// Print the address.
                auto addr = print_and_read_addr(dark_green, op);

                /// Align and print the mnemonic.
                repeat (8 - address_operand_size(op)) r = fmt::format_to(r, "   ");
                fmt::format_to(r, fg(yellow), "    st   {}{}{}{} {}\n", lbrack, styled(addr, fg(dark_green)), rbrack, comma, reg_str(regnum));
            } break;

            case opcode::store_rel8:
            case opcode::store_rel16:
            case opcode::store_rel32:
            case opcode::store_rel64: {
                /// Print the source and dest.
                auto dest = bytecode[i++];
                auto src = bytecode[i++];
                auto r = fmt::format_to(std::back_inserter(result), fg(red), "{:02x} {:02x} ", dest, src);

                /// Print the address.
                auto addr = print_and_read_addr(dark_green, op);

                /// Align and print the mnemonic.
                repeat (8 - address_operand_size(op)) r = fmt::format_to(r, "   ");
                fmt::format_to(r, fg(yellow), " st   {}{} {} {}{}{} {}\n", lbrack, reg_str(dest), plus, styled(addr, fg(dark_green)), rbrack, comma, reg_str(src));
            } break;

            case opcode::add: print_arith("add "); break;
            case opcode::sub: print_arith("sub "); break;
            case opcode::muli: print_arith("muli"); break;
            case opcode::mulu: print_arith("mulu"); break;
            case opcode::divi: print_arith("divi"); break;
            case opcode::divu: print_arith("divu"); break;
            case opcode::remi: print_arith("remi"); break;
            case opcode::remu: print_arith("remu"); break;
            case opcode::shift_left: print_arith("shl "); break;
            case opcode::shift_right_arithmetic: print_arith("sar "); break;
            case opcode::shift_right_logical: print_arith("shr "); break;

            case opcode::call8:
            case opcode::call16:
            case opcode::call32:
            case opcode::call64: {
                auto index = print_and_read_addr(green, op);
                repeat (8 - address_operand_size(op)) result += "   ";

                /// Try and resolve the function name.
                auto it = ranges::find_if(functions_map, [&](auto&& f) { return f.second == index; });
                if (index < functions.size()
                    and not functions[index].address.valueless_by_exception()
                    and not std::holds_alternative<std::monostate>(functions[index].address)) {
                    /// Print the name if we know it; otherwise, print the index.
                    const auto islib = std::holds_alternative<library_function>(functions[index].address);
                    if (it != functions_map.end()) result += fmt::format(fg(yellow), "       call {}", styled(it->first, fg(green)));
                    else if (not islib) result += fmt::format(fg(yellow), "       call {}", styled(index, fg(magenta)));

                    /// Print the type of the call.
                    if (std::holds_alternative<native_function>(functions[index].address)) {
                        result += fmt::format(fg(orange), " @ native\n");
                    } else if (islib) {
                        result += fmt::format(fg(yellow), "       call {} {}\n",                                    //
                                              styled(std::get<library_function>(functions[index].address).name, fg(green)), //
                                              styled("@ library", fg(orange)));
                    } else {
                        result += fmt::format(fg(orange), " @ {:08x}\n", std::get<addr>(functions[index].address));
                    }
                } else result += fmt::format(fg(yellow), "       call {}\n", styled(index, fg(white)));
            } break;

            case opcode::jmp8:
            case opcode::jmp16:
            case opcode::jmp32:
            case opcode::jmp64: {
                auto a = print_and_read_addr(orange, op);
                repeat (8 - address_operand_size(op)) result += "   ";
                result += fmt::format("       {} {:08x}\n", styled("jmp ", fg(yellow)), styled(a, fg(orange)));
            } break;

            case opcode::jnz8:
            case opcode::jnz16:
            case opcode::jnz32:
            case opcode::jnz64: {
                auto r = bytecode[i++];
                result += fmt::format(fg(red), "{:02x} ", r);
                auto a = print_and_read_addr(orange, op);
                repeat (8 - address_operand_size(op)) result += "   ";
                result += fmt::format("    {} {}{} {:08x}\n", styled("jnz ", fg(yellow)), reg_str(r), comma, styled(a, fg(orange)));
            } break;
        }
    }

    return result;
}
