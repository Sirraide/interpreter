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

    /// Loop.
    interp.create_move(2_r, 9_w);
    auto start = interp.current_addr();
    interp.create_call("display");
    interp.create_sub(2_r, 2_r, 1_w);
    interp.create_branch_ifnz(2_r, start);

    /// Return 42.
    interp.create_move(1_r, 42_w);
    interp.create_return();

    /// Define a function.
    interp.defun("display", [](interp::interpreter& i) { fmt::print("{}\n", i.registers[2]); });

    /// Disassemble.
    if (options::get<"-d">()) {
        fmt::print("{}", interp.disassemble());
        std::exit(0);
    }

    /// Run.
    return int(interp.run());
}