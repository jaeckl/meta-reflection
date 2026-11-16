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
