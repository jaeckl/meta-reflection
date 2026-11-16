# One program per file. With the tests enabled every example also runs as a
# test, so an example that stops working fails the build.
function(reflect_add_example name)
    add_executable(example_${name} ${name}.cpp)
    target_link_libraries(example_${name} PRIVATE reflect::reflect)
    set_target_properties(example_${name} PROPERTIES OUTPUT_NAME ${name})
    if(REFLECT_BUILD_TESTS)
        add_test(NAME example_${name} COMMAND example_${name})
    endif()
endfunction()

reflect_add_example(print_struct)
reflect_add_example(enum_names)
reflect_add_example(json)
reflect_add_example(command_line)
reflect_add_example(compare)
reflect_add_example(validate)
reflect_add_example(sql)
reflect_add_example(dispatch)
reflect_add_example(layout)
reflect_add_example(inheritance)
reflect_add_example(format)
reflect_add_example(hash)

# Several files and a generation step: it has its own directory.
add_subdirectory(codegen)
