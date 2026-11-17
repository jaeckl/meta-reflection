# A registry: the build collects the classes marked [[=reflect::registered]]
# in a generated header, and the program finds them by name when it runs.

add_executable(example_registry main.cpp)
target_link_libraries(example_registry PRIVATE reflect::reflect)
set_target_properties(example_registry PROPERTIES OUTPUT_NAME registry)

reflect_process(example_registry
    HEADERS shapes.hpp
    PROCESSORS reflect::codegen::registry_processor<>
    OUTPUT shapes_registry.hpp)

if(REFLECT_BUILD_TESTS)
    add_test(NAME example_registry COMMAND example_registry)
endif()
