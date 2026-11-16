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
