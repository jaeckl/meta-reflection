# Code generation: generate_builders.cpp is compiled and run during the build
# and writes builders.hpp, which main.cpp includes.

add_executable(example_codegen main.cpp)
target_link_libraries(example_codegen PRIVATE reflect::reflect)
# The generated header includes "model.hpp" from another directory.
target_include_directories(example_codegen PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(example_codegen PROPERTIES OUTPUT_NAME codegen)

reflect_generate(example_codegen
    GENERATOR generate_builders.cpp
    OUTPUT builders.hpp)

if(REFLECT_BUILD_TESTS)
    add_test(NAME example_codegen COMMAND example_codegen)
endif()
