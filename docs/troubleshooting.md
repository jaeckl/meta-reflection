# Troubleshooting

Compiler errors you may run into, what they mean and how to fix them. The
error texts are the ones GCC 16.2 prints.

- [Reading an error from the library](#reading-an-error-from-the-library)
- [Setup errors](#setup-errors)
- [Errors in callbacks](#errors-in-callbacks)
- [Errors from lookups](#errors-from-lookups)
- [Errors with annotations](#errors-with-annotations)
- [Errors with lists and loops](#errors-with-lists-and-loops)
- [Errors when calling functions](#errors-when-calling-functions)
- [Errors when walking nested objects](#errors-when-walking-nested-objects)
- [Errors when building objects](#errors-when-building-objects)
- [Known compiler problems](#known-compiler-problems)
- [Questions](#questions)

## Reading an error from the library

Mistakes are found while compiling, and the library reports them through
the mechanism C++26 offers for that. The result looks like this:

```text
main.cpp:29:55:   required from here
reflect/field.hpp:178:57: error: uncaught exception of type 'std::meta::exception';
    'what()': 'reflect::get_field_of: no field with that name'
```

Read two things:

- the text after `what()`, which says what is wrong, and
- the line marked `required from here`, which is the place in *your* code.

The location inside `reflect/...` is where the check lives and can be
ignored.

## Setup errors

### `#error "reflect needs C++26 static reflection. ..."`

Followed by many errors such as `'meta' in namespace 'std' does not name a type`.

The file was compiled without reflection support. Check that

- the compiler is GCC 16 or newer (`g++ --version`), and
- both flags are present: `-std=c++26 -freflection`.

With CMake, link your target to `reflect::reflect`; that adds the flags.

### `unrecognized command-line option '-freflection'`

The compiler is older than GCC 16. If several versions are installed, the
new one may be called `g++-16`. See [Installation](installation.md#get-a-compiler).

### CMake warns that examples and tests are skipped

```text
reflect: GNU 15.2.0 cannot compile C++26 reflection, so the examples and tests are skipped.
```

CMake picked an older compiler. Select the right one when configuring a
fresh build directory:

```sh
cmake -B build -DCMAKE_CXX_COMPILER=g++-16
```

## Errors in callbacks

### `request for member 'size' in 'value', which is of non-class type 'const int'`

Or any other error saying that an operation is not possible for the type of
*one* of the fields.

```cpp
reflect::for_each_field(user, [](auto field, const auto& value) {
    if (field.name() == "name")          // plain if
        std::cout << value.size();       // compiled for every field, int included
});
```

The callback is compiled once per field. A plain `if` is decided when the
program runs, so the compiler still has to compile its body for every field.
Decide at compile time, by type:

```cpp
reflect::for_each_field(user, [](auto field, const auto& value) {
    if constexpr (std::is_same_v<typename decltype(field)::type, std::string>)
        std::cout << value.size();
});
```

[How it works](concepts.md#what-for_each_field-really-does) has the
background.

### `no match for 'operator<<'`

```text
error: no match for 'operator<<' in '... << value' (operand types are
    'std::basic_ostream<char>' and 'const Other')
```

A field has a type that cannot be printed, typically a nested struct or a
container. Give that kind of type its own branch:

```cpp
if constexpr (std::is_class_v<Type> && !std::is_same_v<Type, std::string>)
    print(value);            // recurse into the nested struct
else
    std::cout << value;
```

[examples/json.cpp](../examples/json.cpp) shows the complete pattern.

### `expected primary-expression before '>' token`

```cpp
reflect::for_each_field<User>([](auto field) {
    if constexpr (field.has_annotation<Skip>())   // error
        return;
});
```

`field` has a type that depends on a template parameter, and C++ then wants
the `template` keyword before a member template. Either write it,

```cpp
if constexpr (field.template has_annotation<Skip>())
```

or use the free function, which needs no keyword:

```cpp
if constexpr (reflect::has_annotation<Skip>(field))
```

The same applies to `annotation.is<A>()`, `list.with<A>()` and
`list.get<I>()` when the object they are called on is a template-dependent
value.

### `invalid use of non-static data member 'User::id'`

A field of one class was applied to an object of another:

```cpp
Other other;
reflect::get_field_of<User, "id">().get(other);   // other is not a User
```

## Errors from lookups

### `reflect::get_field_of: no field with that name`

The class does not declare a member with that name. Things to check:

- Spelling and case.
- The member may be inherited. `get_field_of<T, "name">()` only looks at
  what `T` declares itself; use the base class as `T`.
- The member may be static. Those are found with `get_static_fields_of`.

### `no matching function for call to 'get_field_of<User, wanted>()'`

```cpp
std::string wanted = "id";
auto field = reflect::get_field_of<User, wanted>();   // error
```

The name has to be a literal, because the lookup happens while compiling.
For a name that is only known at run time use
[`visit_field`](getting-started.md#find-a-field-by-a-run-time-name).

### `reflect::get_function_of: the name is overloaded; add a signature, ...`

Several member functions have that name. Say which one:

```cpp
reflect::get_function_of<Shape, "scale", double(double)>()
```

or get all of them with `get_functions_of<Shape, "scale">()`.

### `reflect::get_function_of: no overload with that signature`

The signature has to match the declaration exactly: return type, parameter
types, and a trailing `const`, `&` or `&&` if the function has one.

```cpp
struct Buffer {
    const char* data() const;
};

reflect::get_function_of<Buffer, "data", const char*()>()         // error: const is missing
reflect::get_function_of<Buffer, "data", const char*() const>()   // ok
```

`noexcept` is the one thing that is left out.

## Errors with annotations

### `reflect::get_annotation: expected exactly one annotation of this type`

`get_annotation` insists on exactly one. The declaration has either none or
several of that type.

| Situation | Use instead |
| --- | --- |
| it may be missing | `find_annotation`, `get_annotation_or`, or check `has_annotation` first |
| there may be several | `get_annotations` |

### `annotation does not have structural type`

```cpp
struct Label {
    std::string text;      // not allowed in an annotation
};
```

An annotation value is a compile-time constant, and the language restricts
which types can be one: all members public, and no member that manages
memory. For text, use `reflect::static_string`:

```cpp
struct Label {
    reflect::static_string text;
};
```

### An annotation on an enumerator is not accepted

For enumerators the annotation goes *after* the name:

```cpp
enum class Role {
    Guest,
    Admin [[=Label{"administrator"}]] = 5,
};
```

## Errors with lists and loops

### `'pred' is not a constant expression` at `filter`

```cpp
int limit = 1;
auto big = fields.filter([&](auto field) { return field.offset() > limit; });   // error
```

`filter` returns a new list *type*, so its answer is needed at compile time
and the lambda must not capture. If the condition depends on a run-time
value, test it where you use the element:

```cpp
fields.for_each([&](auto field) {
    if (field.offset() > limit) { /* ... */ }
});
```

`any_of`, `all_of`, `none_of`, `count_if`, `map` and `for_each_while` accept
capturing lambdas.

### `'<anonymous>' is not a constant expression` at `template for`

```cpp
template for (constexpr auto field : reflect::get_fields_of<User>())   // error
```

Leave out `constexpr`. A proxy carries everything in its type, so it does
not have to be a constant itself:

```cpp
template for (auto field : reflect::get_fields_of<User>()) {
    constexpr Tag tag = reflect::get_annotation<Tag>(field);   // still fine
}
```

### A list does not work in a normal `for` loop

The elements of a list have different types; see
[How it works](concepts.md#idea-2-a-list-carries-its-elements-in-its-type).
Use `list.for_each(fn)`, `template for`, or `list.names()` if the names are
all you need.

## Errors when calling functions

### `no match for call to '(reflect::detail::overload_set<...>) (...)'`

`reflect::invoke<"name">(object, args...)` found functions with that name,
but none of them accepts these arguments. It is the same situation as
`object.name(args...)` not compiling. The functions that were considered are
listed between the angle brackets.

Two cases that differ from a direct call:

- **Default arguments are not applied.** Pass every argument.
- **Inherited functions are not found.** Only the functions the class
  declares itself are considered.

### `call of '(reflect::detail::overload_set<...>) (...)' is ambiguous`

More than one overload fits equally well, for example `long` passed where
there is an `int` and a `double` overload. Cast the argument, or pick the
overload yourself with a signature:

```cpp
reflect::get_function_of<User, "over", int(double)>().invoke(user, 5L);
```

### `reflect::invoke: no member function with that name`

The class declares no function with that name. Check the spelling, and
whether the function is inherited from a base class.

## Errors when walking nested objects

### `reflect::walk: the callback does not accept the type of this field`

`reflect::walk` found a field your callback cannot be called with, and the
field is not a class, so there is nothing to walk into. Add a lambda for the
type. The line `required from here` in the compiler output points at your
call, and the lines above it name the field.

The same error appears, often several times, when the walk opens up a class
you meant to treat as one value, a `std::vector` for example, and arrives at
the pointers inside it. Add a lambda for the class:

```cpp
reflect::walk(person, reflect::overloaded{
    [](reflect::field_path path, auto, const std::string& value) { /* ... */ },
    [](reflect::field_path path, auto, const std::vector<int>& value) { /* ... */ },
});
```

### An error inside the callback, for a type you did not expect

```cpp
reflect::walk(person, [](reflect::field_path path, auto, const auto& value) {
    std::cout << path << " = " << value << '\n';   // error for nested structs
});
```

A parameter declared `const auto&` accepts every type, nested structs
included. The walk then hands the struct to the callback instead of opening
it, and the body does not compile for it. Say which types the lambda is
for:

```cpp
reflect::walk(person, reflect::overloaded{
    [](reflect::field_path path, auto, const std::string& value) { /* ... */ },
    [](reflect::field_path path, auto, const std::integral auto& value) { /* ... */ },
});
```

## Errors when building objects

### `no matching function for call to 'T::T(<brace-enclosed initializer list>)'`

`reflect::make<T>(fn)` on a class that is not an aggregate and has no
default constructor. `make` has no way to create the object before it fills
the fields. Use `reflect::construct<T>(fn)`, which goes through a
constructor instead.

### `reflect::construct: the class needs exactly one public constructor that takes arguments`

`reflect::construct<T>(fn)` picks the constructor for you, which only works
if there is exactly one candidate. With several, say which one:

```cpp
reflect::construct(reflect::get_constructor_of<User, int, std::string>(), fn);
```

A plain struct without constructors has no candidate at all. Use
`reflect::make<T>(fn)` for it.

### `reflect::get_constructor_of: no constructor with these parameter types`

The types have to match the declaration exactly, except for a top-level
`const`. A constructor `User(const std::string& name)` is found with
`get_constructor_of<User, const std::string&>()`, not with
`get_constructor_of<User, std::string>()`.

### `reflect::constructor::invoke: only a public constructor can be called`

The constructor is private or protected. Reflection can read and write
private fields, but it cannot call a private constructor: the language
offers no way around the access check there. Give the class a public
constructor or a public factory function.

## Known compiler problems

These are defects in the compiler, not in your code. They are listed here so
that you do not lose time on them.

### `symbol ... is already defined`, or `Two symbols with same comdat_group`

Seen with GCC 16.2 and with the development version of GCC at the time of
writing.

It happens when a function proxy type is written out by hand and the same
function is also reached through the library in the same source file:

```cpp
reflect::function<^^User::plain>::invoke(user);        // written by hand

reflect::for_each_function<User>([&](auto function) {  // reaches plain() too
    function.invoke(user);
});
```

**Workaround:** let the library create the proxy. All of these can be mixed
freely:

```cpp
reflect::get_function_of<User, "plain">()
reflect::get_function_of<&User::plain>()
reflect::get_function_of<User, "plain", int()>()
reflect::invoke<"plain">(user)
```

Fields are not affected; `reflect::field<^^User::id>` works.

### The compiler crashes in a debug build

GCC 16 crashes when it writes debug information (`-g`) for a class template
that has the reflection of a function *parameter* as a template argument.

The library avoids this internally, which is why the parameter proxy is
`parameter<Function, Index>`. You can only run into it in your own templates.
If you do, pass the function and the position instead of the parameter
itself.

### `'... == 0' is not a constant expression` with `-fsanitize=undefined`

Seen with GCC 16.2, with `-fsanitize=undefined` and with
`-fno-delete-null-pointer-checks`, which the sanitizer turns on.

With these options the compiler does not accept the comparison of some
addresses with a null pointer in a constant expression. `std::string` makes
that comparison when it is built from a `std::string_view`, and the address
of a template argument is one of those the compiler rejects:

```cpp
template <reflect::fixed_string Name>
constexpr std::size_t length_of = std::string(Name.view()).size();

static_assert(length_of<"id"> == 2);   // an error with -fsanitize=undefined
```

The library is built and tested with the sanitizers, so its own code does
not run into this. You can run into it in your own compile-time code.

**Workaround:** build the string from two iterators:

```cpp
std::string(Name.view().begin(), Name.view().end())
```

## Questions

**Which compilers are supported?**

GCC 16 and newer. The library is tested with GCC 16.2. Other compilers will
be added once they implement the C++26 reflection that was standardized.

**Can it see private members?**

Yes, and it does so by default. Pass `reflect::public_access` to list public
members only. See [Private fields](getting-started.md#private-fields).

**Does it work with class templates, unions and standard library types?**

Yes. `reflect::get_fields_of<std::pair<int, long>>()` lists `first` and
`second`. Be careful with standard library classes other than the simple
aggregates: their private members are implementation details and differ
between library versions.

**Is there any cost at run time?**

The library keeps no tables and registers nothing at start-up. What ends up
in the program is the code of your callbacks, once per member.

**Can I look up a field by a string from a file?**

Yes, with [`visit_field`](getting-started.md#find-a-field-by-a-run-time-name),
or with [`field_table`](getting-started.md#field-metadata-as-plain-data) if
you only need the metadata.

**Why does `get()` return a bit-field by value?**

C++ does not allow a reference to a bit-field. `set()` still works.

**Can it reflect on free functions or namespaces?**

It can list the classes of a namespace, with
[`for_each_class_in`](reference.md#namespaces). Free functions and variables
are not covered. For everything
else, `std::meta` is available directly, and
[How it works](concepts.md#how-this-maps-to-stdmeta) shows how the two fit
together.

**Can it add members to a class or generate new types?**

Not at compile time: C++26 cannot add members to a class or write files.
[Generating code](code-generation.md) shows what works instead: a generic
builder, and a generator program that runs during the build.

**My error is not on this page.**

Please [open an issue](https://github.com/jaeckl/meta-reflection/issues)
with the code and the full compiler output.
