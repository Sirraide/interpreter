#include <interpreter/interp.hh>

/// ===========================================================================
///  Miscellaneous.
/// ===========================================================================
void interp::interpreter::defun(const std::string& name, interp::native_function func) {
    if (functions_map.contains(name)) die("Function '{}' is already defined.", name);
    functions_map[name] = functions.size();
    functions.emplace_back(std::move(func));
}

/// ===========================================================================
///  Stack manipulation.
/// ===========================================================================
/// Push a value onto the stack.
void interp::interpreter::push(elem value) {
    if (data_stack.size() >= max_stack_size) die("Stack overflow");
    data_stack.push_back(value);
}

/// Pop a value from the stack.
auto interp::interpreter::pop() -> elem {
    if (data_stack.empty()) die("Stack underflow");
    auto value = data_stack.back();
    data_stack.pop_back();
    return value;
}

/// ===========================================================================
///  Operations.
/// ===========================================================================
void interp::interpreter::create_return() { bytecode.push_back(static_cast<opcode_t>(opcode::ret)); }
void interp::interpreter::create_addi() { bytecode.push_back(static_cast<opcode_t>(opcode::addi)); }
void interp::interpreter::create_subi() { bytecode.push_back(static_cast<opcode_t>(opcode::subi)); }
void interp::interpreter::create_muli() { bytecode.push_back(static_cast<opcode_t>(opcode::muli)); }
void interp::interpreter::create_mulu() { bytecode.push_back(static_cast<opcode_t>(opcode::mulu)); }
void interp::interpreter::create_divi() { bytecode.push_back(static_cast<opcode_t>(opcode::divi)); }
void interp::interpreter::create_divu() { bytecode.push_back(static_cast<opcode_t>(opcode::divu)); }
void interp::interpreter::create_remi() { bytecode.push_back(static_cast<opcode_t>(opcode::remi)); }
void interp::interpreter::create_remu() { bytecode.push_back(static_cast<opcode_t>(opcode::remu)); }
void interp::interpreter::create_shift_left() { bytecode.push_back(static_cast<opcode_t>(opcode::shl)); }
void interp::interpreter::create_shift_right_logical() { bytecode.push_back(static_cast<opcode_t>(opcode::shr)); }
void interp::interpreter::create_shift_right_arithmetic() { bytecode.push_back(static_cast<opcode_t>(opcode::sar)); }

void interp::interpreter::create_push_int(i64 value) {
    bytecode.push_back(static_cast<opcode_t>(opcode::push_int));
    bytecode.push_back(static_cast<elem>(value));
}

void interp::interpreter::create_call(const std::string& name) {
    /// Make sure the function exists.
    auto it = functions_map.find(name);
    if (it == functions_map.end()) throw error("Cannot create call to unknown function \"{}\"", name);

    /// Should never happen.
    if (it->second >= max_call_index) throw error("Cannot create call to function \"{}\" because the call index is too large.", name);

    /// Push the opcode and call index.
    bytecode.push_back(static_cast<opcode_t>(opcode::call) | (u64(it->second) << 8u));
}

void interp::interpreter::create_branch(addr target) {
    bytecode.push_back(static_cast<opcode_t>(opcode::jmp));
    bytecode.push_back(target);
}

void interp::interpreter::create_branch_ifnz(addr target) {
    bytecode.push_back(static_cast<opcode_t>(opcode::jnz));
    bytecode.push_back(target);
}

void interp::interpreter::create_dup() {
    bytecode.push_back(static_cast<opcode_t>(opcode::dup));
}

auto interp::interpreter::current_addr() const -> addr { return bytecode.size(); }

/// ===========================================================================
///  Execute bytecode.
/// ===========================================================================
void interp::interpreter::run() {
    ip = ip_start_addr;

    for (;;) {
        if (ip >= bytecode.size()) [[unlikely]] { die("Instruction pointer out of bounds."); }
        switch (auto instruction = bytecode[ip]; static_cast<opcode>(instruction & 0xff)) {
            /// Do nothing.
            case opcode::nop: ip++; break;

            /// Return from a function.
            case opcode::ret: {
                if (stack_frame_count == 0) return;

                /// Pop the stack frame.
                stack_frame_count--;

                /// Pop the return address.
                ip = frame_stack.back().return_address;
                frame_stack.pop_back();
            } break;

            /// Push an integer.
            case opcode::push_int: {
                ip++;
                push(bytecode[ip]);
                ip++;
            } break;

            /// Add two integers.
            case opcode::addi: {
                ip++;
                auto a = pop();
                auto b = pop();
                push(a + b);
            } break;

            /// Subtract two integers.
            case opcode::subi: {
                ip++;
                auto a = pop();
                auto b = pop();
                push(b - a);
            } break;

            /// Multiply two integers (signed).
            case opcode::muli: {
                ip++;
                i64 a = static_cast<i64>(pop());
                i64 b = static_cast<i64>(pop());
                push(static_cast<u64>(a * b));
            } break;

            /// Multiply two integers (unsigned).
            case opcode::mulu: {
                ip++;
                auto a = pop();
                auto b = pop();
                push(a * b);
            } break;

            /// Divide two integers (signed).
            case opcode::divi: {
                ip++;
                i64 a = static_cast<i64>(pop());
                i64 b = static_cast<i64>(pop());
                push(static_cast<u64>(b / a));
            } break;

            /// Divide two integers (unsigned).
            case opcode::divu: {
                ip++;
                auto a = pop();
                auto b = pop();
                push(b / a);
            } break;

            /// Remainder of two integers (signed).
            case opcode::remi: {
                ip++;
                i64 a = static_cast<i64>(pop());
                i64 b = static_cast<i64>(pop());
                push(static_cast<u64>(b % a));
            } break;

            /// Remainder of two integers (unsigned).
            case opcode::remu: {
                ip++;
                auto a = pop();
                auto b = pop();
                push(b % a);
            } break;

            /// Shift left.
            case opcode::shl: {
                ip++;
                auto a = pop();
                auto b = pop();
                push(b << a);
            } break;

            /// Logical shift right.
            case opcode::shr: {
                ip++;
                auto a = pop();
                auto b = pop();
                push(b >> a);
            } break;

            /// Arithmetic shift right.
            case opcode::sar: {
                ip++;
                auto a = pop();
                i64 b = static_cast<i64>(pop());
                push(static_cast<u64>(static_cast<i64>(b) >> a));
            } break;

            /// Call a function.
            case opcode::call: {
                usz index = instruction >> 8u;
                ip++;

                /// If it’s a native function, call it.
                auto& func = functions.at(index);
                if (std::holds_alternative<native_function>(func)) {
                    std::get<native_function>(func)(*this);
                }

                /// Otherwise, push the return address and jump to the function.
                else if (std::holds_alternative<addr>(func)) {
                    frame_stack.push_back({ip});
                    ip = std::get<addr>(func);
                    stack_frame_count++;
                }

                /// Should never happen.
                else { die("Invalid function type."); }
            } break;

            /// Jump to an address.
            case opcode::jmp: {
                ip++;
                auto target = bytecode[ip];
                if (target >= bytecode.size()) [[unlikely]] { die("Jump target out of bounds."); }
                ip = target;
            } break;

            /// Jump to an address if the top of the stack is not zero.
            case opcode::jnz: {
                ip++;
                auto target = bytecode[ip];
                if (target >= bytecode.size()) [[unlikely]] { die("Jump target out of bounds."); }
                if (pop()) ip = target;
                else ip++;
            } break;

            /// Duplicate the top of the stack.
            case opcode::dup: {
                ip++;
                push(data_stack.back());
            } break;

            default: die("Invalid opcode.");
        }
    }
}
