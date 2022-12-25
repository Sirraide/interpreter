#include <interpreter/interp.hh>
#include <clopts.hh>

namespace detail {
using namespace command_line_options;
using options = clopts< // clang-format off
    flag<"-d", "Print disassembly and exit">,
    help
>; // clang-format on
}

using detail::options;

int main(int argc, char** argv) {
    options::parse(argc, argv);
    interp::interpreter interp;
    using namespace interp::literals;

    /// Print hello world using libc puts().
    interp.create_move(2_r, (interp::word) "Hello, world!");
    interp.create_library_call_unsafe("libc.so.6", "puts", 1);

    /// Loop.
    interp.create_move(3_r, 9_w);
    auto start = interp.current_addr();
    interp.create_move(2_r, 3_r);
    interp.create_call("display");
    interp.create_call("桜 square 桜 print 桜"); /// Function names are just strings, so they can be anything.
    interp.create_sub(3_r, 3_r, 1_w);
    interp.create_branch_ifnz(3_r, start);

    /// Add a global and a local.
    auto global = interp.create_global(1);
    interp.create_move(4_r, 34);
    interp.create_store(global, 4_r);

    auto local = interp.create_alloca(1);
    interp.create_move(4_r, 35);
    interp.create_store(0_r, local, 4_r);

    interp.create_load(5_r, global);
    interp.create_load(6_r, 0_r, local);

    /// Add the two.
    interp.create_add(2_r, 5_r, 6_r);

    /// A perhaps rather odd way of truncating something.
    interp.create_add(2_r, 2_r, 0b10000000000000000000000000000000000000_w);
    interp.create_xchg(2_r, 2_r | INTERP_SIZE_MASK_8);
    interp.create_call("display");


    /// Demonstrate that we can load from/store to host memory.
    interp::word w1 = 20, w2 = 22, result;
    interp.create_load(3_r, &w1);
    interp.create_load(4_r, &w2);
    interp.create_add(1_r, 3_r, 4_r);
    interp.create_store(&result, 1_r);

    /// Return 42.
    interp.create_return();

    /// Define a function in code.
    interp.create_function("桜 square 桜 print 桜");
    interp.create_mulu(2_r | INTERP_SIZE_MASK_32, 2_r | INTERP_SIZE_MASK_32, 2_r | INTERP_SIZE_MASK_32);
    interp.create_call("display");
    interp.create_return();

    /// Define a function.
    interp.defun("display", [](interp::interpreter& i) { fmt::print("{}\n", i.arg(0, INTERP_SIZE_MASK_32)); });

    /// Disassemble.
    if (options::get<"-d">()) {
        fmt::print("{}", interp.disassemble());
        std::exit(0);
    }

    return result == interp.run();
}