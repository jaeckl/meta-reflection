# Code generated from annotations: reflect_process() writes the generator
# program, so the project only lists its headers and the processors to run.

add_executable(example_processors main.cpp)
target_link_libraries(example_processors PRIVATE reflect::reflect)
set_target_properties(example_processors PROPERTIES OUTPUT_NAME processors)

reflect_process(example_processors
    HEADERS model.hpp
    PROCESSORS
        reflect::codegen::builder_processor   # comes with the library
        print_processor                       # this project's own
    PROCESSOR_HEADERS print_processor.hpp
    OUTPUT generated.hpp)

if(REFLECT_BUILD_TESTS)
    add_test(NAME example_processors COMMAND example_processors)
endif()
