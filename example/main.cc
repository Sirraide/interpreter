#include <interpreter/interp.hh>

int main() {
    interp::interpreter interp;
    interp.defun("display", [](interp::interpreter& i) { fmt::print("{}\n", i.pop()); });
    interp.create_push_int(1);
    interp.create_push_int(2);
    interp.create_addi();
    interp.create_push_int(3);
    interp.create_muli();
    interp.create_call("display");
    interp.create_return();
    interp.run();
}