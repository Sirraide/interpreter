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
    interp.library_call_unsafe("libc.so.6", "puts", 1);

    /// Loop.
    interp.create_move(3_r, 9_w);
    auto start = interp.current_addr();
    interp.create_move(2_r, 3_r);
    interp.create_call("display");
    interp.create_call("桜 square 桜 print 桜"); /// Function names are just strings, so they can be anything.
    interp.create_sub(3_r, 3_r, 1_w);
    interp.create_branch_ifnz(3_r, start);

    /// Return 42.
    interp.create_move(1_r, 42_w);
    interp.create_return();

    /// Define a function in code.
    interp.create_function("桜 square 桜 print 桜");
    interp.create_mulu(2_r | INTERP_SZ_32, 2_r | INTERP_SZ_32, 2_r | INTERP_SZ_32);
    interp.create_call("display");
    interp.create_return();

    /// Define a function.
    interp.defun("display", [](interp::interpreter& i) { fmt::print("{}\n", i.arg(0, INTERP_SZ_32)); });

    /// Disassemble.
    if (options::get<"-d">()) {
        fmt::print("{}", interp.disassemble());
        std::exit(0);
    }

    /// Run.
    return int(interp.run());
}