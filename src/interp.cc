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

/// ===========================================================================
///  Execute bytecode.
/// ===========================================================================
void interp::interpreter::run() {
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
                ip = addr_stack.back();
                addr_stack.pop_back();
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
                    addr_stack.push_back(ip);
                    ip = std::get<addr>(func);
                    stack_frame_count++;
                }

                /// Should never happen.
                else { die("Invalid function type."); }
            } break;
            default: die("Invalid opcode.");
        }
    }
}
