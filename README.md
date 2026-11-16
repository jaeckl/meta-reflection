# reflect

[![CI](https://github.com/jaeckl/meta-reflection/actions/workflows/ci.yml/badge.svg)](https://github.com/jaeckl/meta-reflection/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

A readable API on top of C++26 static reflection (`std::meta`).

C++26 lets a program look at its own types: the members of a struct, the
values of an enum, the annotations on a declaration. The language feature is
powerful and low level. `reflect` puts the everyday tasks behind functions
with plain names, so that using reflection does not require learning `^^`,
splices and `template for` first.

```cpp
#include <reflect/reflect.hpp>

#include <iostream>
#include <string>

struct User {
    int id;
    std::string name;
    int age;
};

int main() {
    User user{1, "Ada", 36};

    reflect::for_each_field(user, [](auto field, const auto& value) {
        std::cout << field.name() << " = " << value << '\n';
    });
}
```

```text
id = 1
name = Ada
age = 36
```

No macros, no code generator, nothing to register per type. `User` is an
ordinary struct.

## What it does

| You want to | Use |
| --- | --- |
| Loop over the fields of a struct | `reflect::for_each_field` |
| Read or write a field by name | `reflect::get_field_of<User, "name">()` |
| Turn an enum into text and back | `reflect::enum_name`, `reflect::enum_cast` |
| Attach metadata to a field and read it | `[[=Rename{"mail"}]]`, `reflect::get_annotation` |
| Skip, rename or validate fields by annotation | `reflect::has_annotation`, `.with<A>()`, `.without<A>()` |
| List and call member functions | `reflect::for_each_function`, `reflect::invoke<"name">` |
| Walk base classes and inherited fields | `reflect::for_each_base`, `reflect::get_all_fields_of` |
| Compare or diff two objects | `reflect::tie`, `reflect::for_each_field(a, b, fn)` |
| Find a field by a name known only at run time | `reflect::visit_field` |
| Visit nested structs with their path (`home.city`) | `reflect::walk` |
| Build an object from parsed input | `reflect::make` |
| Call a constructor with arguments chosen by parameter name | `reflect::construct` |
| Get field names, offsets and sizes as plain data | `reflect::field_table` |

The reflection itself happens at compile time. There is no run-time
registry, and a type you never reflect on costs nothing.

## A second example

Annotations are what make reflection useful for serialization, databases
and user interfaces. An annotation is an ordinary struct, written in front of
the declaration it describes:

```cpp
struct Skip {};
struct Key {
    reflect::static_string name;
};

struct User {
    int id;
    [[=Key{"displayName"}]] std::string name;
    [[=Skip{}]] std::string password;
};

template <typename T>
void write_json(std::ostream& out, const T& object) {
    out << '{';
    bool first = true;
    reflect::for_each_field(object, [&](auto field, const auto& value) {
        if constexpr (reflect::has_annotation<Skip>(field))
            return;

        std::string_view key = field.name();
        if constexpr (reflect::has_annotation<Key>(field))
            key = reflect::get_annotation<Key>(field).name;

        out << (first ? "" : ",") << '"' << key << "\":" << value;
        first = false;
    });
    out << '}';
}
```

This short version writes the values as they are. The complete one, which
quotes strings and handles nested structs, enums and containers, is
[examples/json.cpp](examples/json.cpp).

## Requirements

- **GCC 16 or newer**, with the flags `-std=c++26 -freflection`.

That is the whole list. The library is header-only and depends on nothing
but the standard library.

Reflection is new, and so is compiler support for it. GCC 16 is the compiler
this library is developed and tested with; others will be added as they
implement C++26 reflection. [Compiler setup](docs/installation.md#get-a-compiler)
shows how to get GCC 16 with Docker or on Windows, and how to try the library
in the browser without installing anything.

## Install

With CMake, the library can be fetched at configure time:

```cmake
include(FetchContent)
FetchContent_Declare(reflect
    GIT_REPOSITORY https://github.com/jaeckl/meta-reflection.git
    GIT_TAG main)
FetchContent_MakeAvailable(reflect)

target_link_libraries(my_app PRIVATE reflect::reflect)
```

Without CMake, copy
[single_include/reflect/reflect.hpp](single_include/reflect/reflect.hpp)
into your project. It is the whole library in one file.

```sh
g++ -std=c++26 -freflection main.cpp -o main
```

More options, including `find_package` and `add_subdirectory`, are in
[Installation](docs/installation.md).

## Documentation

| | |
| --- | --- |
| [Getting Started Guide](docs/getting-started.md) | Task by task: how do I ...? Start here. |
| [Installation](docs/installation.md) | Compiler setup and build system integration. |
| [How it works](docs/concepts.md) | The three ideas behind the API, and why `if constexpr` shows up so often. |
| [Cookbook](docs/cookbook.md) | Larger worked examples: JSON, SQL, command line options, validation. |
| [Coming from another language](docs/coming-from.md) | For Java, C#, Python, Go and Rust developers, and for users of earlier C++ reflection libraries. |
| [API Reference](docs/reference.md) | Every type and function. |
| [Troubleshooting](docs/troubleshooting.md) | Compiler errors and what they mean. |
| [Design notes](docs/design.md) | Why the API looks the way it does. |

## Examples

Each file in [examples/](examples) is a complete program.

| Example | Shows |
| --- | --- |
| [print_struct.cpp](examples/print_struct.cpp) | The smallest useful program: print any struct. |
| [enum_names.cpp](examples/enum_names.cpp) | Enum to text and back. |
| [json.cpp](examples/json.cpp) | A JSON writer with rename and skip annotations. |
| [command_line.cpp](examples/command_line.cpp) | `--name=value` options parsed into a struct, with generated help. |
| [compare.cpp](examples/compare.cpp) | Equality, ordering and a field-by-field diff. |
| [validate.cpp](examples/validate.cpp) | Validation rules as annotations. |
| [sql.cpp](examples/sql.cpp) | `CREATE TABLE` and `INSERT` statements from annotated structs. |
| [dispatch.cpp](examples/dispatch.cpp) | Calling member functions by a name that arrives at run time. |
| [format.cpp](examples/format.cpp) | `std::format` support for every struct that opts in. |
| [hash.cpp](examples/hash.cpp) | Structs as keys in unordered containers. |
| [layout.cpp](examples/layout.cpp) | Offsets, sizes and padding of a struct. |
| [inheritance.cpp](examples/inheritance.cpp) | Base classes and inherited fields. |

To build and run them:

```sh
cmake -B build -DCMAKE_CXX_COMPILER=g++-16
cmake --build build
./build/examples/json
```

## Status

Version 0.1. The API is young and may still change before 1.0.

Tested with GCC 16.2. Known compiler problems and their workarounds are
listed in [Troubleshooting](docs/troubleshooting.md#known-compiler-problems).

## Contributing

Bug reports, questions and pull requests are welcome. See
[CONTRIBUTING.md](CONTRIBUTING.md).

## License

MIT. See [LICENSE](LICENSE).
