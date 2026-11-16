# One executable per file. A test passes when it compiles (most checks are
# static_asserts) and exits with 0.
function(reflect_add_test name)
    add_executable(test_${name} ${name}.cpp)
    target_link_libraries(test_${name} PRIVATE reflect::reflect)
    target_compile_options(test_${name} PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME ${name} COMMAND test_${name})
endfunction()

reflect_add_test(annotations)
