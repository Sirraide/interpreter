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

    /// Loop.
    interp.create_push_int(9);
    auto start = interp.current_addr();
    interp.create_dup();
    interp.create_call("display");
    interp.create_push_int(1);
    interp.create_subi();
    interp.create_dup();
    interp.create_branch_ifnz(start);

    /// Return 42.
    interp.create_push_int(42);
    interp.create_return_value();

    /// Define a function.
    interp.defun("display", [](interp::interpreter& i) { fmt::print("{}\n", i.pop()); });

    /// Disassemble.
    if (options::get<"-d">()) {
        fmt::print("{}", interp.disassemble());
        std::exit(0);
    }

    /// Run.
    return int(interp.run());
}