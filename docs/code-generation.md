# Generating code

> **Experimental.** Everything on this page is a prototype:
> `reflect::builder`, the namespace `reflect::codegen`, and the CMake
> functions `reflect_process` and `reflect_generate`. Names and behaviour
> may change.

In Java, an annotation processor or a tool such as Lombok reads annotations
and writes new classes: `@Builder` on a class gives you a `UserBuilder`.
This page shows how far C++26 gets, and three ways to do it with this
library.

- [What C++26 can generate](#what-c26-can-generate)
- [Way 1: a builder without generated code](#way-1-a-builder-without-generated-code)
- [Way 2: processors](#way-2-processors)
  - [Annotate the classes](#annotate-the-classes)
  - [Add the step to the build](#add-the-step-to-the-build)
  - [Use the generated code](#use-the-generated-code)
  - [What the build wrote](#what-the-build-wrote)
  - [The builder processor](#the-builder-processor)
  - [Write a processor](#write-a-processor)
  - [Errors from a processor](#errors-from-a-processor)
  - [Without CMake](#without-cmake)
  - [How it works](#how-it-works)
- [Way 3: a generator program of your own](#way-3-a-generator-program-of-your-own)
  - [The classes](#the-classes)
  - [The generator](#the-generator)
  - [The result](#the-result)
  - [CMake](#cmake)
  - [Make](#make)
  - [Meson](#meson)
  - [Rules for generators](#rules-for-generators)
- [Which way to choose](#which-way-to-choose)
- [Compared to Java annotation processing](#compared-to-java-annotation-processing)

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
for a later standard. Until then there are three ways around it.

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

## Way 2: processors

For real generated classes, do what a Java annotation processor does: run a
program during the build that reads the annotations and writes source code.
You do not write that program. You annotate classes, and list in the build
which headers to read and which *processors* to run.

The complete example is in [examples/processors/](../examples/processors).

### Annotate the classes

```cpp
namespace shop {

struct [[=reflect::generate_builder{}, =Printable{}]] User {
    [[=reflect::required]] int id = 0;
    [[=reflect::required]] std::string name;
    std::string email = "none";
};

struct [[=reflect::generate_builder{"set_"}]] Order {
    long number = 0;
    double total = 0;
};

struct [[=Printable{}]] Receipt {
    long order = 0;
    bool paid = false;
};

}  // namespace shop
```

`reflect::generate_builder` comes with the library, and so does the
processor that reads it. `Printable` is an annotation of the example
project, with a processor of its own; see [Write a processor](#write-a-processor).

### Add the step to the build

```cmake
add_executable(app main.cpp)
target_link_libraries(app PRIVATE reflect::reflect)

reflect_process(app
    HEADERS model.hpp
    PROCESSORS
        reflect::codegen::builder_processor   # comes with the library
        print_processor                       # this project's own
    PROCESSOR_HEADERS print_processor.hpp
    OUTPUT generated.hpp)
```

| Argument | Meaning |
| --- | --- |
| `HEADERS` | The headers with the annotated classes. Every class declared in one of them is offered to the processors, whatever namespace it is in. |
| `PROCESSORS` | The processors to run, as C++ class names, in this order. |
| `PROCESSOR_HEADERS` | Headers that declare processors of your own: a path, or `<name>` for a header on the include path. Not needed for the processors of the library. |
| `OUTPUT` | The name of the generated header. The default is `<target>_generated.hpp`. |

`reflect_process` is available after `find_package(reflect)`,
`FetchContent` or `add_subdirectory`. When one of the headers changes, the
code is generated again.

### Use the generated code

The generated header includes the input headers, so one include is enough:

```cpp
#include "generated.hpp"
```

```cpp
shop::User user = shop::UserBuilder().with_id(7).with_name("Ada").build();
shop::Order order = shop::OrderBuilder().set_number(1001).set_total(59.9).build();

std::cout << user << '\n';
std::cout << shop::Receipt{order.number, true} << '\n';

// The generated builder checks the required fields when build() runs.
try {
    shop::UserBuilder().with_id(8).build();
} catch (const std::logic_error& error) {
    std::cout << error.what() << '\n';
}
```

```text
User{id=7, name=Ada, email=none}
Receipt{order=1001, paid=1}
UserBuilder: the required field 'name' was not set
```

### What the build wrote

From the builder processor, for `User`:

```cpp
namespace shop {

class UserBuilder {
public:
    UserBuilder& with_id(decltype(shop::User::id) value) {
        object_.id = std::move(value);
        has_id_ = true;
        return *this;
    }
    UserBuilder& with_name(decltype(shop::User::name) value) {
        object_.name = std::move(value);
        has_name_ = true;
        return *this;
    }
    UserBuilder& with_email(decltype(shop::User::email) value) {
        object_.email = std::move(value);
        return *this;
    }
    shop::User build() {
        if (!has_id_)
            throw std::logic_error("UserBuilder: the required field 'id' was not set");
        if (!has_name_)
            throw std::logic_error("UserBuilder: the required field 'name' was not set");
        return std::move(object_);
    }

private:
    shop::User object_{};
    bool has_id_ = false;
    bool has_name_ = false;
};

}  // namespace shop
```

From the print processor, for `Receipt`:

```cpp
namespace shop {

inline std::ostream& operator<<(std::ostream& out, const shop::Receipt& value) {
    out << "Receipt{";
    out << "order=" << value.order;
    out << ", paid=" << value.paid;
    return out << "}";
}

}  // namespace shop
```

The file is in the build directory. It is not meant to be edited or
committed.

### The builder processor

`reflect::codegen::builder_processor` writes a class `<Name>Builder` for
every class marked `[[=reflect::generate_builder{}]]`, in the namespace of
that class.

- **Setters.** One per public field that can be assigned, fields of base
  classes included. Private fields, `const` fields, reference fields and
  bit-fields get none: generated code is ordinary code and cannot do more
  than code you write by hand.
- **Setter names.** The prefix in the annotation plus the field name. The
  default prefix is `with_`; `[[=reflect::generate_builder{"set_"}]]`
  changes it.
- **Required fields.** `build()` throws `std::logic_error` if a field marked
  `[[=reflect::required]]` was not set. This check happens at run time;
  [`reflect::builder`](#way-1-a-builder-without-generated-code) does it at
  compile time.
- **Everything else** keeps the value of a default-constructed object, so
  the class needs a default constructor.

The processor reports an error, and the build stops, for a class without a
default constructor, for a required field that is not public or cannot be
assigned, and for a class in an unnamed namespace.

### Write a processor

A processor is a class with a function template `process`. The build calls
it once for every class that carries the annotation named by `annotation`:

```cpp
struct print_processor {
    using annotation = Printable;

    template <typename T>
    static void process(reflect::codegen::context& context) {
        std::string_view type = reflect::qualified_name_of<T>();

        if (reflect::get_fields_of<T, reflect::public_access>().empty()) {
            // Printed like a compiler error, at the line of the class.
            context.error<T>("Printable: the class has no public fields to print");
            return;
        }

        context.include("<ostream>");
        reflect::codegen::source_writer& out = context.out();

        // The operator goes into the namespace of the class, where the
        // compiler looks for it.
        reflect::codegen::namespace_scope scope(out, reflect::codegen::namespace_of<T>());

        out.line("inline std::ostream& operator<<(std::ostream& out, const ", type,
                 "& value) {");
        {
            auto body = out.indented();
            out.line("out << \"", reflect::name_of<T>(), "{\";");
            bool first = true;
            reflect::for_each_field<T, reflect::public_access>([&](auto field) {
                out.line("out << \"", first ? "" : ", ", field.name(), "=\" << value.",
                         field.name(), ";");
                first = false;
            });
            out.line("return out << \"}\";");
        }
        out.line("}");
        out.blank();
    }
};
```

Inside `process`, `T` is the class, and the whole library is available to
ask about it. What the processor gets to work with:

| | |
| --- | --- |
| `context.out()` | The writer for the generated code. |
| `out.line(parts...)` | Writes one line: the current indentation, then the parts. A part is anything a stream can print. |
| `out.blank()` | Writes an empty line. |
| `out.indented()` | Indents every line written while the returned object is alive. |
| `namespace_scope(out, name)` | Opens `namespace name {` and closes it at the end of the scope. |
| `context.include("<header>")` | Asks for an `#include` at the top of the generated file. Asking twice is harmless. |
| `context.error<T>(message)` | Reports an error at the declaration of the class. |
| `context.error(field, message)` | Reports an error at the declaration of a member. |
| `reflect::qualified_name_of<T>()` | The class name to write in code: `shop::User`. |
| `reflect::codegen::namespace_of<T>()` | The namespace of the class: `shop`. |
| `reflect::codegen::can_be_named<T>()` | Whether generated code can refer to the class at all. |

A processor without `annotation` is called for every class of the input
headers.

The same advice as for any generated code applies: spell the type of a
field as `decltype(shop::User::name)` instead of printing a type name, and
remember that the generated code cannot reach private members.

### Errors from a processor

`context.error` prints the message in the format of a compiler error, with
the file and line of the declaration, and makes the generator fail. The
build stops, nothing is written, and an editor can jump to the line. If
`Receipt` had only private fields, the build of the example would end with:

```text
/project/examples/processors/model.hpp:26:25: error: Printable: the class has no public fields to print
```

Report every problem you find instead of stopping at the first; the user
then sees all of them in one build.

### Without CMake

`reflect_process` writes a small program and builds it. With another build
system, write that program yourself. It is the same for every project:

```cpp
#include "model.hpp"
#include "print_processor.hpp"

#include <reflect/codegen.hpp>

int main(int argc, char** argv) {
    return reflect::codegen::run<
        reflect::codegen::inputs<"model.hpp">,
        reflect::codegen::builder_processor,
        print_processor>(argc, argv);
}
```

Compile it, run it with the path of the header to write as its argument,
and compile the rest of the project afterwards. The [Make](#make) and
[Meson](#meson) rules further down do exactly that for a generator program;
with the file names changed they work for this one.

A name in `inputs<...>` is the header as it is included, or with more of its
path in front. The generated header includes the inputs under the same
names.

### How it works

- The generator includes the input headers, so the compiler has parsed
  them. No other parser is involved.
- It walks every namespace, starting at the global one, and keeps the
  classes whose declaration is in one of the input headers. Namespace `std`
  is skipped.
- **Found:** classes and structs in any namespace, including inline and
  nested namespaces, and classes nested in other classes.
- **Not found:** class templates, classes that are only declared, and
  classes from headers that are merely included by an input header. List
  such a header in `HEADERS` to have its classes processed too.
- All processors of one `reflect_process` call write into one header.
- `<reflect/codegen.hpp>` is only for generator programs. It is not part of
  `<reflect/reflect.hpp>` or of the single header, and the annotations
  (`reflect::generate_builder`, `reflect::required`) do not need it.

## Way 3: a generator program of your own

Processors write code class by class into one header. When that does not
fit, because the output is several files or is not C++, or because the
classes are chosen in some other way, write the whole generator program
yourself. This is the layer `reflect_process` is built on.

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

| | `reflect::builder` | Processors | Own generator program |
| --- | --- | --- | --- |
| Build setup | none | one `reflect_process` call | one `reflect_generate` call and a program |
| You write | nothing | annotations; a processor for anything the library does not bring | the whole generator |
| Call syntax | `.set<"name">("Ada")` | `.with_name("Ada")` | whatever you generate |
| Wrong field name | compile error | compile error | compile error |
| Required fields | checked at compile time | checked at run time by the builder processor | whatever you generate |
| Private fields | yes | no | no, or through `friend` |
| Editor completion for setters | no | yes, after a build | yes, after a build |
| Can produce | builders | C++ code per class, in one header | anything: several files, SQL, documentation |

Start with the first. Use processors when the generated names matter or
when the same generated code is wanted for many classes. Write your own
generator for output that processors cannot produce.

## Compared to Java annotation processing

| Java | Here |
| --- | --- |
| A processor is a class that implements `Processor` | A processor is a class with `process<T>(context&)`. |
| `RoundEnvironment.getElementsAnnotatedWith` | The classes of the input headers that carry `annotation`. Members are reached through their class. |
| `Filer`, and libraries such as JavaPoet | `context`, `source_writer`. One output file. |
| `Messager.printMessage` with an element | `context.error`, at the file and line of the declaration. |
| The compiler runs the processors | The build system does. C++ compilers have no common interface for it. |
| Processors are found on the processor path | They are listed in `reflect_process`. |
| Several rounds: generated code is processed again | One round. |
| An annotated class may use the code generated for it | No. The input headers have to compile before anything is generated. Code in other files uses the result. |
| Maven, Gradle and others | A CMake function. Other build systems by the pattern shown above. |
| Processors change nothing in existing classes (Lombok does, through compiler internals) | The same. C++26 cannot add members to a class either. |

Not done yet: processors shipped as packages, several output files, more
than one round, writing the output only when it changed, and support for
build systems other than CMake.
