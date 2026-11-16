# Installation

`reflect` is header-only. Installing it means two things: having a compiler
that supports C++26 reflection, and letting that compiler find the headers.

- [Get a compiler](#get-a-compiler)
  - [Try it in the browser](#try-it-in-the-browser)
  - [Docker](#docker)
  - [Windows with MSYS2](#windows-with-msys2)
  - [Linux packages](#linux-packages)
- [Add the library to your project](#add-the-library-to-your-project)
  - [CMake: FetchContent](#cmake-fetchcontent)
  - [CMake: add_subdirectory](#cmake-add_subdirectory)
  - [CMake: find_package](#cmake-find_package)
  - [One header, no build system](#one-header-no-build-system)
- [Compiler flags](#compiler-flags)
- [Build the examples and tests](#build-the-examples-and-tests)
- [Editor support](#editor-support)

## Get a compiler

You need **GCC 16 or newer**. Check what you have:

```sh
g++ --version
```

If that prints 16 or higher, skip to
[Add the library to your project](#add-the-library-to-your-project).

### Try it in the browser

The fastest way to see the library work needs no installation.

1. Open [Compiler Explorer](https://godbolt.org).
2. Choose the compiler **x86-64 gcc 16.2** or newer.
3. Put this into the compiler options box: `-std=c++26 -freflection`
4. Include the single header by its URL:

```cpp
#include <https://raw.githubusercontent.com/jaeckl/meta-reflection/main/single_include/reflect/reflect.hpp>

#include <iostream>
#include <string>

struct User {
    int id;
    std::string name;
};

int main() {
    reflect::for_each_field(User{1, "Ada"}, [](auto field, const auto& value) {
        std::cout << field.name() << " = " << value << '\n';
    });
}
```

Open an *Execution* pane to see the output.

### Docker

The official `gcc` image has GCC 16. This works the same on Linux, macOS
and Windows, and leaves your system compiler alone.

Compile and run one file from the current directory:

```sh
docker run --rm -v "$PWD":/src -w /src gcc:16 \
    sh -c "g++ -std=c++26 -freflection main.cpp -o main && ./main"
```

In PowerShell write `${PWD}` instead of `"$PWD"`.

For a CMake project, start a shell in the container and install CMake once:

```sh
docker run --rm -it -v "$PWD":/src -w /src gcc:16 bash
apt-get update && apt-get install -y cmake ninja-build
cmake -B build -G Ninja && cmake --build build
```

### Windows with MSYS2

[MSYS2](https://www.msys2.org) provides a native Windows build of GCC 16.

1. Install MSYS2 and open the **MSYS2 UCRT64** terminal.
2. Install the compiler, and CMake and Ninja if you use them:

```sh
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja
```

3. Check the version:

```sh
g++ --version
```

To use this compiler outside the MSYS2 terminal, add
`C:\msys64\ucrt64\bin` to your `PATH`.

### Linux packages

If your distribution ships GCC 16, install it from there. The compiler is
then often called `g++-16`, next to an older default `g++`. Tell CMake which
one to use:

```sh
cmake -B build -DCMAKE_CXX_COMPILER=g++-16
```

If your distribution does not have GCC 16 yet, use [Docker](#docker).

## Add the library to your project

### CMake: FetchContent

CMake downloads the library when it configures your project. Nothing has to
be installed beforehand.

```cmake
cmake_minimum_required(VERSION 3.28)
project(my_app LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(reflect
    GIT_REPOSITORY https://github.com/jaeckl/meta-reflection.git
    GIT_TAG main)
FetchContent_MakeAvailable(reflect)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE reflect::reflect)
```

Linking to `reflect::reflect` sets the include path, C++26 and the
`-freflection` flag for your target. You do not have to set them yourself.

For a build that does not change under you, replace `main` with a release
tag or a commit hash.

### CMake: add_subdirectory

If the library is inside your source tree, as a copy or a git submodule:

```sh
git submodule add https://github.com/jaeckl/meta-reflection.git external/reflect
```

```cmake
add_subdirectory(external/reflect)
target_link_libraries(my_app PRIVATE reflect::reflect)
```

The library's own examples and tests are not built when it is used this way.

### CMake: find_package

Install the library once:

```sh
git clone https://github.com/jaeckl/meta-reflection.git
cd meta-reflection
cmake -B build -DREFLECT_BUILD_TESTS=OFF -DREFLECT_BUILD_EXAMPLES=OFF
cmake --install build --prefix /usr/local
```

Writing to `/usr/local` usually needs `sudo`. Any other prefix works as
well, for example `--prefix ~/.local`.

Then, in any project:

```cmake
find_package(reflect 0.1 REQUIRED)
target_link_libraries(my_app PRIVATE reflect::reflect)
```

If you installed to a directory CMake does not search by default, point it
there with `-DCMAKE_PREFIX_PATH=/path/to/prefix`.

### One header, no build system

[single_include/reflect/reflect.hpp](../single_include/reflect/reflect.hpp)
is the whole library in one file. Copy it anywhere and include it:

```cpp
#include "reflect.hpp"
```

```sh
g++ -std=c++26 -freflection main.cpp -o main
```

Alternatively keep the `include/` directory of this repository and pass it
with `-I`:

```sh
g++ -std=c++26 -freflection -I path/to/meta-reflection/include main.cpp -o main
```

```cpp
#include <reflect/reflect.hpp>
```

## Compiler flags

| Flag | Why |
| --- | --- |
| `-std=c++26` | Reflection is a C++26 feature. `-std=gnu++26` works too. |
| `-freflection` | GCC keeps reflection behind this flag. |

Every source file that includes a `reflect` header needs both. If one is
missing, the first error says so:

```text
error: #error "reflect needs C++26 static reflection. With GCC 16 or newer compile with: -std=c++26 -freflection"
```

## Build the examples and tests

```sh
git clone https://github.com/jaeckl/meta-reflection.git
cd meta-reflection
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The example programs end up in `build/examples/`.

| CMake option | Default | Meaning |
| --- | --- | --- |
| `REFLECT_BUILD_EXAMPLES` | `ON` when built on its own, otherwise `OFF` | Build the programs in `examples/`. |
| `REFLECT_BUILD_TESTS` | `ON` when built on its own, otherwise `OFF` | Build the tests and register them with CTest. |

If the compiler cannot compile reflection, CMake prints a warning and skips
the examples and tests. Installing the headers still works.

## Editor support

Reflection syntax is new. An editor whose code analysis is not based on
GCC 16 may underline `^^`, `[[=...]]` or `template for` as errors even though
the code compiles. If that happens, trust the compiler output.
