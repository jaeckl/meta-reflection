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
reflect_add_test(tables)

# The snippets of docs/getting-started.md and of the README.
reflect_add_test(guide)
reflect_add_test(readme)

# The generated single header has to behave like the split headers.
add_executable(test_single_header guide.cpp)
target_include_directories(test_single_header PRIVATE ${PROJECT_SOURCE_DIR}/single_include)
target_compile_features(test_single_header PRIVATE cxx_std_26)
target_compile_options(test_single_header PRIVATE -freflection -Wall -Wextra -Wpedantic)
add_test(NAME single_header COMMAND test_single_header)
