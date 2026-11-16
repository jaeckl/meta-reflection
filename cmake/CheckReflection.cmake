# Sets REFLECT_HAVE_REFLECTION when the compiler can build C++26 reflection.

include(CheckCXXSourceCompiles)
include(CMakePushCheckState)

cmake_push_check_state(RESET)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_REQUIRED_FLAGS "-std=c++26 -freflection")
else()
    set(CMAKE_REQUIRED_FLAGS "-std=c++26")
endif()

check_cxx_source_compiles("
#include <meta>
struct probe { int value; };
static_assert(std::meta::nonstatic_data_members_of(
                  ^^probe, std::meta::access_context::unchecked()).size() == 1);
int main() { return 0; }
" REFLECT_HAVE_REFLECTION)

cmake_pop_check_state()
