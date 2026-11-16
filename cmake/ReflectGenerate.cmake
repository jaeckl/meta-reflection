# Code generation with reflect. Experimental.
#
# reflect_process: generated code from annotations, with ready-made
# processors. reflect_generate: the step below it, for a generator program
# you write yourself.

# reflect_generate: source code written by a program that uses reflection.
#
#     reflect_generate(<target>
#         GENERATOR <source>...   the sources of the generator program
#         OUTPUT <file>           the file it writes
#         [DEPENDS <file>...])    more files that make the output out of date
#
# Builds the generator, runs it as `<generator> <path of the output file>`
# before <target> is compiled, and adds the output to <target>. The output
# lands in a directory that is put on the include path of <target>, so a
# generated header is included as #include "<file>".
#
# The generator is compiled with the include directories of <target>. It
# sees the same headers, and it is rebuilt and run again whenever one of the
# headers it includes changes.
#
# The generator runs on the machine that builds. When cross compiling it
# needs CMAKE_CROSSCOMPILING_EMULATOR.

function(reflect_generate target)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "OUTPUT" "GENERATOR;DEPENDS")
    if(NOT TARGET ${target})
        message(FATAL_ERROR "reflect_generate: ${target} is not a target")
    endif()
    if(NOT ARG_GENERATOR OR NOT ARG_OUTPUT)
        message(FATAL_ERROR "reflect_generate: GENERATOR and OUTPUT are required")
    endif()
    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "reflect_generate: unknown arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif()

    string(MAKE_C_IDENTIFIER "${target}_generate_${ARG_OUTPUT}" generator)
    add_executable(${generator} ${ARG_GENERATOR})
    target_link_libraries(${generator} PRIVATE reflect::reflect)
    target_include_directories(${generator} PRIVATE
        $<TARGET_PROPERTY:${target},INCLUDE_DIRECTORIES>)

    set(directory ${CMAKE_CURRENT_BINARY_DIR}/${target}_generated)
    set(output ${directory}/${ARG_OUTPUT})
    cmake_path(GET output PARENT_PATH output_directory)

    add_custom_command(
        OUTPUT ${output}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${output_directory}
        COMMAND ${generator} ${output}
        DEPENDS ${generator} ${ARG_DEPENDS}
        COMMENT "Generating ${ARG_OUTPUT}"
        VERBATIM)

    target_sources(${target} PRIVATE ${output})
    target_include_directories(${target} PRIVATE ${directory})
endfunction()

# reflect_process: code generated from annotations, without writing a
# generator program.
#
#     reflect_process(<target>
#         HEADERS <header>...              the headers with the annotated classes
#         PROCESSORS <class>...            the processors to run, as C++ class names
#         [PROCESSOR_HEADERS <header>...]  headers that declare your own processors
#         [OUTPUT <file>])                 default: <target>_generated.hpp
#
# Writes the generator program, builds it, runs it before <target> is
# compiled and adds the generated header to <target>, to be included as
# #include "<file>". The generated header includes the HEADERS itself.
#
# The processors run in the order given, over every class declared in one
# of the HEADERS. reflect::codegen::builder_processor comes with the
# library. A header in PROCESSOR_HEADERS is a path, or <name> for one that
# is found on the include path.

function(reflect_process target)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "OUTPUT"
        "HEADERS;PROCESSORS;PROCESSOR_HEADERS")
    if(NOT TARGET ${target})
        message(FATAL_ERROR "reflect_process: ${target} is not a target")
    endif()
    if(NOT ARG_HEADERS OR NOT ARG_PROCESSORS)
        message(FATAL_ERROR "reflect_process: HEADERS and PROCESSORS are required")
    endif()
    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "reflect_process: unknown arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT ARG_OUTPUT)
        set(ARG_OUTPUT ${target}_generated.hpp)
    endif()

    # Absolute paths: the generator finds its classes by the file they are
    # declared in, and the generated header works from any directory.
    set(input_includes "")
    set(inputs "")
    foreach(header IN LISTS ARG_HEADERS)
        cmake_path(ABSOLUTE_PATH header BASE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} NORMALIZE)
        if(NOT EXISTS ${header})
            message(FATAL_ERROR "reflect_process: no such header: ${header}")
        endif()
        string(APPEND input_includes "#include \"${header}\"\n")
        list(APPEND inputs "\"${header}\"")
    endforeach()
    list(JOIN inputs ",\n            " inputs)

    set(processor_includes "")
    foreach(header IN LISTS ARG_PROCESSOR_HEADERS)
        if(header MATCHES "^<.+>$")
            string(APPEND processor_includes "#include ${header}\n")
        else()
            cmake_path(ABSOLUTE_PATH header BASE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} NORMALIZE)
            if(NOT EXISTS ${header})
                message(FATAL_ERROR "reflect_process: no such header: ${header}")
            endif()
            string(APPEND processor_includes "#include \"${header}\"\n")
        endif()
    endforeach()
    list(JOIN ARG_PROCESSORS ",\n        " processors)

    string(MAKE_C_IDENTIFIER "${ARG_OUTPUT}" name)
    set(generator ${CMAKE_CURRENT_BINARY_DIR}/${target}_generated/${name}_generator.cpp)
    file(CONFIGURE OUTPUT ${generator} @ONLY CONTENT [[
// Written by reflect_process() for the target @target@. Do not edit.

@input_includes@
#include <reflect/codegen.hpp>
@processor_includes@
int main(int argc, char** argv) {
    return reflect::codegen::run<
        reflect::codegen::inputs<
            @inputs@>,
        @processors@>(argc, argv);
}
]])

    reflect_generate(${target} GENERATOR ${generator} OUTPUT ${ARG_OUTPUT})
endfunction()
