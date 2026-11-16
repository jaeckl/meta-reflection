// A generator program whose input is wrong. It has to print errors and
// write no file; tests/CMakeLists.txt checks the output.

#include <reflect/codegen.hpp>

#include "codegen_bad_model.hpp"

int main(int argc, char** argv) {
    return reflect::codegen::run<reflect::codegen::inputs<"codegen_bad_model.hpp">,
                                 reflect::codegen::builder_processor>(argc, argv);
}
