#include <algorithm>
#include <fmt/color.h>
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

/// ===========================================================================
///  Miscellaneous.
/// ===========================================================================
interp::interpreter::interpreter() {
    /// Push an invalid instruction to make sure jumps to 0 throw.
    bytecode.push_back(static_cast<opcode_t>(opcode::invalid));
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
        if (not std::holds_alternative<std::monostate>(functions.at(it->second)))
            throw error("Function '{}' is already defined.", name);

        /// Resolve the forward declaration.
        functions[it->second] = func;
    }

    /// Add the function.
    else {
        functions_map[name] = functions.size();
        functions.emplace_back(std::move(func));
    }
}

interp::word interp::interpreter::arg(usz index, interp_size sz) const {
    /// r2 is the first argument register.
    index += 2;

    /// Check that the register is valid.
    if (index < 2 or index >= _registers_.size())
        throw error("Argument index {} is out of bounds.", index);

    /// Return the value.
    return read_register(static_cast<reg>(index) | sz);
}

void interp::interpreter::push(word value) {
    if (sp == max_stack_size) throw error("Stack overflow.");
    stack[sp++] = value;
}

auto interp::interpreter::pop() -> word {
    if (sp == 0) throw error("Stack underflow.");
    return stack[--sp];
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

constexpr static usz address_operand_size(interp::opcode op) {
    switch (op) {
        case interp::opcode::call8:
        case interp::opcode::jmp8:
        case interp::opcode::jnz8:
            return 1;

        case interp::opcode::call16:
        case interp::opcode::jmp16:
        case interp::opcode::jnz16:
            return 2;

        case interp::opcode::call32:
        case interp::opcode::jmp32:
        case interp::opcode::jnz32:
            return 4;

        case interp::opcode::call64:
        case interp::opcode::jmp64:
        case interp::opcode::jnz64:
            return 8;

        default: return 0;
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
    bytecode.push_back(static_cast<opcode_t>(op));
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
    bytecode.push_back(static_cast<opcode_t>(op));
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
    bytecode.push_back(static_cast<opcode_t>(op));
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
        case INTERP_SZ_8: *reinterpret_cast<u8*>(&_registers_[index(r)]) = static_cast<u8>(value); break;
        case INTERP_SZ_16: *reinterpret_cast<u16*>(&_registers_[index(r)]) = static_cast<u16>(value); break;
        case INTERP_SZ_32: *reinterpret_cast<u32*>(&_registers_[index(r)]) = static_cast<u32>(value); break;
        case INTERP_SZ_64: _registers_[index(r)] = value; break;
        default: std::unreachable();
    }
}

interp::word interp::interpreter::read_register(reg r) const {
    switch (+r & osz_mask) {
        case INTERP_SZ_8: return *reinterpret_cast<const u8*>(&_registers_[index(r)]);
        case INTERP_SZ_16: return *reinterpret_cast<const u16*>(&_registers_[index(r)]);
        case INTERP_SZ_32: return *reinterpret_cast<const u32*>(&_registers_[index(r)]);
        case INTERP_SZ_64: return _registers_[index(r)];
        default: std::unreachable();
    }
}

/// ===========================================================================
///  Linker.
/// ===========================================================================
/// TODO: Write a tool that uses libtooling to generate
///       signatures and allow for type-safe-ish calls?
void interp::interpreter::library_call_unsafe(const std::string& library_path, const std::string& function_name, usz num_params) {
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
    functions.emplace_back(library_function{sym, num_params, function_name});

    /// Add the function to the library.
    lib->functions[function_name] = functions.size() - 1;

    /// Create the call.
    create_call_internal(functions.size() - 1);
}

/// ===========================================================================
///  Operations.
/// ===========================================================================
void interp::interpreter::create_return() { bytecode.push_back(static_cast<opcode_t>(opcode::ret)); }

void interp::interpreter::create_move(reg dest, reg src) {
    /// Make sure the registers are valid.
    check_regs(dest, src);

    /// Encode the instruction.
    bytecode.push_back(static_cast<opcode_t>(opcode::mov));
    bytecode.push_back(+dest);
    bytecode.push_back(+src);
}

void interp::interpreter::create_move(reg dest, word imm) {
    /// Make sure the registers are valid.
    check_regs(dest);

    /// Encode the instruction.
    bytecode.push_back(static_cast<opcode_t>(opcode::mov));
    bytecode.push_back(+dest);
    bytecode.push_back(imm > UINT32_MAX ? +reg::arith_imm_64 : +reg::arith_imm_32);

    /// Encode the immediate.
    write_imm(bytecode, imm);
}

#define ARITH(name, ...)                                                            \
    void interp::interpreter::CAT(create_, name)(reg dest, reg src1, reg src2) /**/ \
    { encode_arithmetic(opcode::name, dest, src1, src2); }                          \
    void interp::interpreter::CAT(create_, name)(reg dest, reg src, word imm) /**/  \
    { encode_arithmetic(opcode::name, dest, src, imm); }                            \
    void interp::interpreter::CAT(create_, name)(reg dest, word imm, reg src) /**/  \
    { encode_arithmetic(opcode::name, dest, imm, src); }
INTERP_ALL_ARITHMETIC_INSTRUCTIONS(ARITH)
#undef ARITH

void interp::interpreter::create_call_internal(usz index) {
    if (index < UINT8_MAX) bytecode.push_back(static_cast<opcode_t>(opcode::call8));
    else if (index < UINT16_MAX) bytecode.push_back(static_cast<opcode_t>(opcode::call16));
    else if (index < UINT32_MAX) bytecode.push_back(static_cast<opcode_t>(opcode::call32));
    else bytecode.push_back(static_cast<opcode_t>(opcode::call64));
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
        functions.emplace_back(std::monostate{});
    }

    /// Function found. Push the opcode and call index.
    else { create_call_internal(it->second); }
}

void interp::interpreter::create_branch(addr target) {
    /// Push the opcode and target.
    if (target < UINT8_MAX) bytecode.push_back(static_cast<opcode_t>(opcode::jmp8));
    else if (target < UINT16_MAX) bytecode.push_back(static_cast<opcode_t>(opcode::jmp16));
    else if (target < UINT32_MAX) bytecode.push_back(static_cast<opcode_t>(opcode::jmp32));
    else bytecode.push_back(static_cast<opcode_t>(opcode::jmp64));
    write_addr(bytecode, target);
}

void interp::interpreter::create_branch_ifnz(reg cond, addr target) {
    /// Push the opcode, condition and target.
    if (target < UINT8_MAX) bytecode.push_back(static_cast<opcode_t>(opcode::jnz8));
    else if (target < UINT16_MAX) bytecode.push_back(static_cast<opcode_t>(opcode::jnz16));
    else if (target < UINT32_MAX) bytecode.push_back(static_cast<opcode_t>(opcode::jnz32));
    else bytecode.push_back(static_cast<opcode_t>(opcode::jnz64));
    bytecode.push_back(+cond);
    write_addr(bytecode, target);
}

void interp::interpreter::create_function(const std::string& name) {
    /// Make sure the function doesn’t already exist.
    if (auto it = functions_map.find(name); it != functions_map.end()) {
        if (not std::holds_alternative<std::monostate>(functions[it->second])) throw error("Function already exists.");
        functions[it->second] = addr{bytecode.size()};
    }

    /// Add the function.
    else {
        functions_map[name] = functions.size();
        functions.emplace_back(addr{bytecode.size()});
    }
}

auto interp::interpreter::current_addr() const -> addr { return bytecode.size(); }

/// ===========================================================================
///  Execute bytecode.
/// ===========================================================================
interp::word interp::interpreter::run() {
    stack_base = 0;
    stack.clear();
    stack.reserve(1024 * 1024);
    ip = ip_start_addr;
    sp = 0;

    /// Initialise registers.
    for (auto& reg : _registers_) reg = 0;

    /// Run the code.
    for (;;) {
        if (ip >= bytecode.size()) [[unlikely]] { throw error("Instruction pointer out of bounds."); }
        switch (auto op = static_cast<opcode>(bytecode[ip++])) {
            static_assert(opcode_t(opcode::max_opcode) == 27);
            default: throw error("Invalid opcode {}", u8(op));

            /// Do nothing.
            case opcode::nop: break;

            /// Return from a function.
            case opcode::ret: {
                if (stack_base == 0) return _registers_[1];

                /// Pop the stack frame.
                sp = stack_base;
                stack_base = pop();
                ip = pop();
            } break;

            /// Move an immediate or a value from one register to another.
            case opcode::mov: {
                auto dest = static_cast<reg>(bytecode[ip++]);
                auto src = static_cast<reg>(bytecode[ip++]);

                /// Immediate.
                if (src == reg::arith_imm_32 || src == reg::arith_imm_64) {
                    word imm{};
                    std::memcpy(&imm, bytecode.data() + ip, src == reg::arith_imm_32 ? 4 : 8);
                    ip += src == reg::arith_imm_32 ? 4 : 8;
                    set_register(dest, imm);
                }

                /// Register.
                else { set_register(dest, read_register(src)); }
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
            case opcode::shl: {
                auto [dest, src1, src2] = decode_arithmetic();
                set_register(dest, src1 << (src2 & 63));
            } break;

            /// Logical shift right.
            case opcode::shr: {
                auto [dest, src1, src2] = decode_arithmetic();
                set_register(dest, src1 >> (src2 & 63));
            } break;

            /// Arithmetic shift right.
            case opcode::sar: {
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
                if (std::holds_alternative<native_function>(func)) {
                    std::get<native_function>(func)(*this);
                }

                /// Otherwise, push the return address and jump to the function.
                else if (std::holds_alternative<addr>(func)) {
                    push(ip);
                    push(stack_base);
                    stack_base = sp;
                    ip = std::get<addr>(func);
                }

                /// If it’s a library function, we need to do some black magic.
                else if (std::holds_alternative<library_function>(func)) {
                    auto& lib_func = std::get<library_function>(func);
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
    static constexpr auto comma = styled(",", fg(white));

    /// String representation of a register.
    const auto reg_str = [](u8 r) {
        std::string_view suffix;
        switch (r & osz_mask) {
            case INTERP_SZ_8: suffix = "b"; break;
            case INTERP_SZ_16: suffix = "w"; break;
            case INTERP_SZ_32: suffix = "d"; break;
            case INTERP_SZ_64: suffix = ""; break;
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

    /// Entry point.
    result += fmt::format(fg(green), "__entry__{}\n", styled(":", fg(orange)));

    /// Iterate over the bytecode and disassemble each instruction.
    while (i < bytecode.size()) {
        /// Check if there is a function that starts at this address.
        auto func = ranges::find_if(functions_map, [&](auto&& p) {
            return p.second < functions.size()
                   and std::holds_alternative<addr>(functions[p.second])
                   and std::get<addr>(functions[p.second]) == i;
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
            static_assert(opcode_t(opcode::max_opcode) == 27);
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
                r = fmt::format_to(r, "            {} {}", styled("mov", fg(yellow)), reg_str(bytecode[i]));
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

            case opcode::add: print_arith("add"); break;
            case opcode::sub: print_arith("sub"); break;
            case opcode::muli: print_arith("muli"); break;
            case opcode::mulu: print_arith("mulu"); break;
            case opcode::divi: print_arith("divi"); break;
            case opcode::divu: print_arith("divu"); break;
            case opcode::remi: print_arith("remi"); break;
            case opcode::remu: print_arith("remu"); break;
            case opcode::shl: print_arith("shl"); break;
            case opcode::sar: print_arith("sar"); break;
            case opcode::shr: print_arith("shr"); break;

            case opcode::call8:
            case opcode::call16:
            case opcode::call32:
            case opcode::call64: {
                auto index = print_and_read_addr(green, op);
                repeat (8 - address_operand_size(op)) result += "   ";

                /// Try and resolve the function name.
                auto it = ranges::find_if(functions_map, [&](auto&& f) { return f.second == index; });
                if (index < functions.size()
                    and not functions[index].valueless_by_exception()
                    and not std::holds_alternative<std::monostate>(functions[index])) {
                    /// Print the name if we know it; otherwise, print the index.
                    const auto islib = std::holds_alternative<library_function>(functions[index]);
                    if (it != functions_map.end()) result += fmt::format(fg(yellow), "       call {}", styled(it->first, fg(green)));
                    else if (not islib) result += fmt::format(fg(yellow), "       call {}", styled(index, fg(magenta)));

                    /// Print the type of the call.
                    if (std::holds_alternative<native_function>(functions[index])) {
                        result += fmt::format(fg(orange), " @ native\n");
                    } else if (islib) {
                        result += fmt::format(fg(yellow), "       call {} {}\n",                                    //
                                              styled(std::get<library_function>(functions[index]).name, fg(green)), //
                                              styled("@ library", fg(orange)));
                    } else {
                        result += fmt::format(fg(orange), " @ {:08x}\n", std::get<addr>(functions[index]));
                    }
                } else result += fmt::format(fg(yellow), "       call {}\n", styled(index, fg(white)));
            } break;

            case opcode::jmp8:
            case opcode::jmp16:
            case opcode::jmp32:
            case opcode::jmp64: {
                auto a = print_and_read_addr(orange, op);
                repeat (8 - address_operand_size(op)) result += "   ";
                result += fmt::format("       {} {:08x}\n", styled("jmp", fg(yellow)), styled(a, fg(orange)));
            } break;

            case opcode::jnz8:
            case opcode::jnz16:
            case opcode::jnz32:
            case opcode::jnz64: {
                auto r = bytecode[i++];
                result += fmt::format(fg(red), "{:02x} ", r);
                auto a = print_and_read_addr(orange, op);
                repeat (8 - address_operand_size(op)) result += "   ";
                result += fmt::format("    {} {}{} {:08x}\n", styled("jnz", fg(yellow)), reg_str(r), comma, styled(a, fg(orange)));
            } break;
        }
    }

    return result;
}
