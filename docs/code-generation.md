# Generating code

> **Experimental.** `reflect::builder`, `reflect::for_each_class_in`,
> `reflect::qualified_name_of` and the CMake function `reflect_generate` are
> a prototype. Names and behaviour may change.

In Java, an annotation processor or a tool such as Lombok reads annotations
and writes new classes: `@Builder` on a class gives you a `UserBuilder`.
This page shows how far C++26 gets, and two ways to do it with this library.

- [What C++26 can generate](#what-c26-can-generate)
- [Way 1: a builder without generated code](#way-1-a-builder-without-generated-code)
- [Way 2: a generator program in the build](#way-2-a-generator-program-in-the-build)
  - [The classes](#the-classes)
  - [The generator](#the-generator)
  - [The result](#the-result)
  - [CMake](#cmake)
  - [Make](#make)
  - [Meson](#meson)
  - [Rules for generators](#rules-for-generators)
- [Which way to choose](#which-way-to-choose)

## What C++26 can generate

Reading is complete in C++26. Writing is not:

| You want to | C++26 |
| --- | --- |
| Read annotations, fields, functions, base classes | Yes. That is this library. |
| Find the annotated classes of a namespace | Yes: [`reflect::for_each_class_in`](reference.md#namespaces). |
| Create a new struct with computed fields | Yes, with `std::meta::define_aggregate`. Fields only. The library does not wrap it. |
| Create member functions with computed names, such as `with_name()` | No. |
| Add members to an existing class | No. |
| Write a source file while compiling | No. Compile-time code cannot write files. |

Functions with computed names and members added from outside are proposed
for a later standard. Until then there are two ways around it.

## Way 1: a builder without generated code

If the name of the setter does not have to be `with_name`, no code needs to
be generated. `reflect::builder` works for any class:

```cpp
struct Connection {
    [[=reflect::required]] std::string host;
    [[=reflect::required]] int port = 0;
    int timeout_seconds = 30;
    bool encrypted = true;
};
```

```cpp
Connection connection = reflect::builder<Connection>()
                            .set<"port">(443)
                            .set<"host">("example.org")
                            .set<"timeout_seconds">(5)
                            .build();
```

```text
example.org:443, timeout 5s, encrypted
```

The compiler checks:

- **The field name.** `set<"prot">(443)` does not compile.
- **The value.** It has to be assignable to the field.
- **Required fields.** `build()` does not compile as long as a field marked
  `[[=reflect::required]]` has not been set. The message names the field:

  ```text
  error: uncaught exception of type 'std::meta::exception'; 'what()':
  'reflect::builder::build: the required field 'port' was not set'
  ```

- **Setting a field twice.**

Fields that are not set keep the value a default-constructed object gives
them, so the class needs a default constructor. Fields of base classes and
private fields can be set too.

To make the last two checks possible, every `set` returns a builder of a
new type that records which fields were set. Keep the calls in one chain, or
store the steps in `auto` variables:

```cpp
auto local = reflect::builder<Connection>().set<"host">("localhost").set<"encrypted">(false);
print(local.set<"port">(8080).build());
print(local.set<"port">(9090).build());
```

```text
localhost:8080, timeout 30s, plain
localhost:9090, timeout 30s, plain
```

This also means a builder cannot be filled in a loop or behind an `if`: the
type of the variable would have to change. For that, default construct the
object and use [`visit_field`](getting-started.md#find-a-field-by-a-run-time-name).

Full program: [builder.cpp](../examples/builder.cpp)

## Way 2: a generator program in the build

For real generated classes, do what a Java annotation processor does: run a
program during the build that writes source code.

```text
model.hpp ──► generate_builders.cpp ──compile, run──► builders.hpp ──┐
    │                                                                ├──► main.cpp ──► app
    └────────────────────────────────────────────────────────────────┘
```

The generator is a small C++ program. It includes the headers it reports
on, asks the library about them, and prints C++ code. There is no parser
and no separate tool: the compiler has read the headers already.

The complete example is in [examples/codegen/](../examples/codegen).

### The classes

`model.hpp` declares the classes of the program. An annotation marks those
that get a builder, and carries an option:

```cpp
namespace shop {

/// Asks for a generated builder. Every setter name starts with `prefix`.
struct Builder {
    reflect::static_string prefix;
};

/// A field the generated builder gets no setter for.
struct Fixed {};

struct [[=Builder{"with_"}]] User {
    int id = 0;
    std::string name;
    std::vector<std::string> roles;
    [[=Fixed{}]] int version = 1;
};

struct [[=Builder{"set_"}]] Order {
    long number = 0;
    double total = 0;
};

// Not marked: no builder.
struct Receipt {
    long order = 0;
};

}  // namespace shop
```

### The generator

`generate_builders.cpp` finds the marked classes and writes one builder
class for each:

```cpp
int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: generate_builders <output file>\n";
        return 1;
    }
    std::ofstream out(argv[1]);

    out << "// Generated by generate_builders from model.hpp. Do not edit.\n\n"
        << "#pragma once\n\n"
        << "#include \"model.hpp\"\n\n"
        << "#include <utility>\n\n"
        << "namespace shop {\n\n";

    reflect::for_each_class_in<^^shop>([&]<typename T>(std::type_identity<T>) {
        if constexpr (reflect::has_annotation<shop::Builder, T>())
            write_builder<T>(out);
    });

    out << "}  // namespace shop\n";
    return out ? 0 : 1;
}
```

`^^shop` is the C++26 way to name the namespace as a value.
`for_each_class_in` calls the lambda once per class, with the class as `T`.

```cpp
template <typename T>
void write_builder(std::ostream& out) {
    std::string_view name = reflect::name_of<T>();
    std::string_view type = reflect::qualified_name_of<T>();
    std::string_view prefix = reflect::get_annotation<shop::Builder, T>().prefix;

    out << "class " << name << "Builder {\n"
        << "public:\n";

    // The generated code is ordinary code and cannot touch private members,
    // so only the public fields get a setter.
    reflect::for_each_field<T, reflect::public_access>([&](auto field) {
        if constexpr (!reflect::has_annotation<shop::Fixed>(field)) {
            // decltype(shop::User::name) spells the type of the field
            // without this program having to print a type name.
            out << "    " << name << "Builder& " << prefix << field.name()
                << "(decltype(" << type << "::" << field.name() << ") value) {\n"
                << "        object_." << field.name() << " = std::move(value);\n"
                << "        return *this;\n"
                << "    }\n";
        }
    });

    out << "    " << type << " build() { return std::move(object_); }\n"
        << "\n"
        << "private:\n"
        << "    " << type << " object_{};\n"
        << "};\n\n";
}
```

### The result

The build runs the generator, and it writes `builders.hpp`:

```cpp
// Generated by generate_builders from model.hpp. Do not edit.

#pragma once

#include "model.hpp"

#include <utility>

namespace shop {

class UserBuilder {
public:
    UserBuilder& with_id(decltype(shop::User::id) value) {
        object_.id = std::move(value);
        return *this;
    }
    UserBuilder& with_name(decltype(shop::User::name) value) {
        object_.name = std::move(value);
        return *this;
    }
    UserBuilder& with_roles(decltype(shop::User::roles) value) {
        object_.roles = std::move(value);
        return *this;
    }
    shop::User build() { return std::move(object_); }

private:
    shop::User object_{};
};

class OrderBuilder {
public:
    OrderBuilder& set_number(decltype(shop::Order::number) value) {
        object_.number = std::move(value);
        return *this;
    }
    OrderBuilder& set_total(decltype(shop::Order::total) value) {
        object_.total = std::move(value);
        return *this;
    }
    shop::Order build() { return std::move(object_); }

private:
    shop::Order object_{};
};

}  // namespace shop
```

`main.cpp` includes it like any other header:

```cpp
shop::User user = shop::UserBuilder()
                      .with_id(7)
                      .with_name("Ada")
                      .with_roles({"admin", "developer"})
                      .build();

shop::Order order = shop::OrderBuilder().set_number(1001).set_total(59.9).build();
```

```text
7 Ada 2 1
1001 59.9
```

### CMake

The library brings a function for the build step. It is available after
`find_package(reflect)`, `FetchContent` or `add_subdirectory`:

```cmake
add_executable(app main.cpp)
target_link_libraries(app PRIVATE reflect::reflect)
# The generated header includes "model.hpp" from another directory.
target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

reflect_generate(app
    GENERATOR generate_builders.cpp
    OUTPUT builders.hpp)
```

`reflect_generate(<target> GENERATOR <source>... OUTPUT <file> [DEPENDS <file>...])`:

1. Builds the generator from the `GENERATOR` sources, with the include
   directories of the target, so that it sees the same headers.
2. Runs it as `<generator> <path of the output file>` before the target is
   compiled.
3. Adds the output to the target, and its directory to the include path of
   the target: `#include "builders.hpp"`.

When `model.hpp` changes, the generator is rebuilt and run again, and
everything that includes the generated header is recompiled. `DEPENDS` is
for files the generator reads at run time.

The generator runs on the machine that builds. A cross-compiling build needs
`CMAKE_CROSSCOMPILING_EMULATOR`, or a generator built for the host.

### Make

No special support is needed. The pattern is "build the generator, run it,
compile the rest":

```make
CXXFLAGS = -std=c++26 -freflection
REFLECT  = path/to/meta-reflection/include

app: main.cpp model.hpp builders.hpp
	$(CXX) $(CXXFLAGS) -I$(REFLECT) main.cpp -o $@

# The generated header: run the generator.
builders.hpp: generate_builders
	./generate_builders $@

# The generator is rebuilt when the header it reports on changes.
generate_builders: generate_builders.cpp model.hpp
	$(CXX) $(CXXFLAGS) -I$(REFLECT) generate_builders.cpp -o $@
```

### Meson

```meson
project('app', 'cpp', default_options: ['cpp_std=c++26'])
add_project_arguments('-freflection', language: 'cpp')

reflect = declare_dependency(include_directories: include_directories('reflect-include'))

generator = executable('generate_builders', 'generate_builders.cpp',
    dependencies: reflect)

builders = custom_target('builders',
    output: 'builders.hpp',
    command: [generator, '@OUTPUT@'])

executable('app', 'main.cpp', builders, dependencies: reflect)
```

`reflect-include` is a directory in your project that contains the
`reflect` header directory.

### Rules for generators

- **The generator has to compile without the generated file.** It includes
  `model.hpp`, not `builders.hpp`. A header that needs generated code cannot
  be an input of the generator that produces it.
- **Generated code is ordinary code.** It obeys access rules, unlike the
  library. Give setters to public fields only, or declare the generated class a
  friend in the class it builds.
- **Spell types with `decltype`.** `decltype(shop::User::name)` is exact for
  every type. The text the compiler prints for a type is meant for people,
  and is not always valid code.
- **Use `qualified_name_of` for class names.** It gives `shop::User`, which
  is correct wherever the generated code ends up.
- **`for_each_class_in` reports what the compiler has seen so far** in the
  source file of the generator. Call it after all headers are included. It
  lists classes declared directly in the namespace; nested namespaces need
  their own call, and class templates are not listed.
- **Write the output only through the path the build passes in.** The build
  system has to know every file a step produces.

## Which way to choose

| | `reflect::builder` | Generator program |
| --- | --- | --- |
| Build setup | none | one extra step |
| Call syntax | `.set<"name">("Ada")` | `.with_name("Ada")` |
| Wrong field name | compile error | compile error |
| Required fields | checked at compile time | whatever the generator writes |
| Private fields | yes | no, or through `friend` |
| Editor completion for setters | no | yes |
| Can produce | builders | any code: builders, serializers, bindings, SQL, documentation |

Start with the first. Use a generator when the generated names matter, when
other tools have to read the result, or when the output is not C++.
