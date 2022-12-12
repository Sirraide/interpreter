#include <interpreter/interp.hh>

int main() {
    interp::interpreter interp;
    interp.defun("display", [](interp::interpreter& i) { fmt::print("{}\n", i.pop()); });
    interp.create_push_int(1);
    interp.create_push_int(2);
    interp.create_addi();
    interp.create_push_int(3);
    interp.create_muli();

    interp.create_push_int(9);
    auto start = interp.current_addr();
    interp.create_dup();
    interp.create_call("display");
    interp.create_push_int(1);
    interp.create_subi();
    interp.create_dup();
    interp.create_branch_ifnz(start);

    interp.create_return();
    interp.run();
}