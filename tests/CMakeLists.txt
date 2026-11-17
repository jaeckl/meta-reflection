# One executable per file. A test passes when it compiles (most checks are
# static_asserts) and exits with 0.
function(reflect_add_test name)
    add_executable(test_${name} ${name}.cpp)
    target_link_libraries(test_${name} PRIVATE reflect::reflect)
    target_compile_options(test_${name} PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME ${name} COMMAND test_${name})
endfunction()

reflect_add_test(annotations)
reflect_add_test(fields)
reflect_add_test(functions)
reflect_add_test(bases)
reflect_add_test(enums)
reflect_add_test(members)
reflect_add_test(lists)
reflect_add_test(field_lookup)
reflect_add_test(objects)
reflect_add_test(static_fields)
reflect_add_test(overloads)
reflect_add_test(parameters)
reflect_add_test(constructors)
reflect_add_test(tables)
reflect_add_test(walk)
reflect_add_test(builder)
reflect_add_test(namespaces)
reflect_add_test(codegen)

# A generator that has to refuse its input, with messages in the format of
# compiler errors: file:line:column: error: text.
add_executable(test_codegen_errors codegen_errors.cpp)
target_link_libraries(test_codegen_errors PRIVATE reflect::reflect)
target_compile_options(test_codegen_errors PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME codegen_errors
    COMMAND test_codegen_errors ${CMAKE_CURRENT_BINARY_DIR}/never_written.hpp)
set_tests_properties(codegen_errors PROPERTIES PASS_REGULAR_EXPRESSION
    "codegen_bad_model.hpp:10:[0-9]+: error: reflect::generate_builder: the class needs a default constructor")

# The generated code has to compile without warnings and work, not only
# look right.
add_executable(test_codegen_use codegen_use.cpp)
target_link_libraries(test_codegen_use PRIVATE reflect::reflect)
target_compile_options(test_codegen_use PRIVATE -Wall -Wextra -Wpedantic)
reflect_process(test_codegen_use
    HEADERS codegen_model.hpp
    PROCESSORS reflect::codegen::builder_processor
    OUTPUT codegen_model_generated.hpp)
add_test(NAME codegen_use COMMAND test_codegen_use)

# The registry: its classes come from a generated header.
reflect_add_test(registry)
reflect_process(test_registry
    HEADERS registry_model.hpp
    PROCESSORS
        reflect::codegen::registry_processor<>
        reflect::codegen::registry_processor<Component>
        reflect::codegen::registry_processor<Unused>
    OUTPUT registry_model_generated.hpp)

# The snippets of docs/getting-started.md and of the README.
reflect_add_test(guide)
reflect_add_test(readme)

# The generated single header has to behave like the split headers.
add_executable(test_single_header guide.cpp)
target_include_directories(test_single_header PRIVATE ${PROJECT_SOURCE_DIR}/single_include)
target_compile_features(test_single_header PRIVATE cxx_std_26)
target_compile_options(test_single_header PRIVATE -freflection -Wall -Wextra -Wpedantic)
add_test(NAME single_header COMMAND test_single_header)
