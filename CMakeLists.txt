cmake_minimum_required(VERSION 3.28)

project(reflect
    VERSION 0.1.0
    DESCRIPTION "A readable API on top of C++26 std::meta reflection"
    LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

add_library(reflect INTERFACE)
add_library(reflect::reflect ALIAS reflect)

target_include_directories(reflect INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_features(reflect INTERFACE cxx_std_26)

# GCC keeps reflection behind a flag, so everything that uses the library needs it.
target_compile_options(reflect INTERFACE
    $<$<CXX_COMPILER_ID:GNU>:-freflection>)

option(REFLECT_BUILD_EXAMPLES "Build the example programs" ${PROJECT_IS_TOP_LEVEL})
option(REFLECT_BUILD_TESTS "Build the tests" ${PROJECT_IS_TOP_LEVEL})

if(REFLECT_BUILD_EXAMPLES OR REFLECT_BUILD_TESTS)
    include(cmake/CheckReflection.cmake)
    if(NOT REFLECT_HAVE_REFLECTION)
        message(WARNING
            "reflect: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} cannot "
            "compile C++26 reflection, so the examples and tests are skipped. "
            "GCC 16 or newer is needed.")
    endif()
endif()

if(REFLECT_BUILD_TESTS AND REFLECT_HAVE_REFLECTION)
    enable_testing()
    add_subdirectory(tests)
endif()

if(REFLECT_BUILD_EXAMPLES AND REFLECT_HAVE_REFLECTION)
    add_subdirectory(examples)
endif()

# Install: headers plus a package so that find_package(reflect) works.
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS reflect EXPORT reflectTargets)
install(EXPORT reflectTargets
    NAMESPACE reflect::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/reflect)

configure_package_config_file(
    cmake/reflectConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/reflectConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/reflect)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/reflectConfigVersion.cmake
    COMPATIBILITY SameMinorVersion
    ARCH_INDEPENDENT)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/reflectConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/reflectConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/reflect)
