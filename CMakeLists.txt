cmake_minimum_required(VERSION 3.30)

project(meta-reflection LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(meta-reflection src/main.cpp)

# MinGW libstdc++ leaves std::print terminal helpers unresolved unless
# the experimental library is linked.
target_link_libraries(meta-reflection PRIVATE stdc++exp)
