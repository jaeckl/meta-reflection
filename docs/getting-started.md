# Getting Started Guide

This guide shows how to do the common things with `reflect`, one task at a
time. Each section is short and stands on its own, so you can jump straight
to the one you need.

Every snippet on this page is compiled and run as part of the test suite
([tests/guide.cpp](../tests/guide.cpp)), and the output shown is the real
output.

- [Before you start](#before-you-start)
- [Your first program](#your-first-program)
- [Fields](#fields)
  - [Loop over the fields of a type](#loop-over-the-fields-of-a-type)
  - [Loop over the fields of an object](#loop-over-the-fields-of-an-object)
  - [Treat fields differently by type](#treat-fields-differently-by-type)
  - [Read and write one field by name](#read-and-write-one-field-by-name)
  - [Get a field by member pointer](#get-a-field-by-member-pointer)
  - [Check whether a type has a field](#check-whether-a-type-has-a-field)
  - [Ask a field about itself](#ask-a-field-about-itself)
  - [Include inherited fields](#include-inherited-fields)
  - [Private fields](#private-fields)
- [Annotations](#annotations)
  - [Define and attach an annotation](#define-and-attach-an-annotation)
  - [Check for an annotation](#check-for-an-annotation)
  - [Read an annotation](#read-an-annotation)
  - [Annotations that may be missing](#annotations-that-may-be-missing)
  - [The same annotation several times](#the-same-annotation-several-times)
  - [Loop over annotations](#loop-over-annotations)
  - [Select fields by annotation](#select-fields-by-annotation)
  - [Accept only annotated types](#accept-only-annotated-types)
  - [Annotations of base classes](#annotations-of-base-classes)
- [Enums](#enums)
  - [Enum to text](#enum-to-text)
  - [Text to enum](#text-to-enum)
  - [List all values](#list-all-values)
- [Member functions](#member-functions)
  - [List the functions of a class](#list-the-functions-of-a-class)
  - [Call a function](#call-a-function)
  - [Call every function that fits](#call-every-function-that-fits)
  - [Overloaded functions](#overloaded-functions)
  - [Function parameters](#function-parameters)
- [Base classes](#base-classes)
- [Static members](#static-members)
- [Working with lists](#working-with-lists)
  - [Size, index and names](#size-index-and-names)
  - [Ask questions and filter](#ask-questions-and-filter)
  - [Use a language loop instead of a callback](#use-a-language-loop-instead-of-a-callback)
- [Whole objects](#whole-objects)
  - [Compare two objects](#compare-two-objects)
  - [Walk two objects side by side](#walk-two-objects-side-by-side)
  - [Find a field by a run-time name](#find-a-field-by-a-run-time-name)
  - [Walk nested objects](#walk-nested-objects)
  - [Build an object from input](#build-an-object-from-input)
  - [Build an object through its constructor](#build-an-object-through-its-constructor)
  - [Stop a loop early](#stop-a-loop-early)
- [Field metadata as plain data](#field-metadata-as-plain-data)
- [When the library does not cover it](#when-the-library-does-not-cover-it)
- [Common issues](#common-issues)

## Before you start

You need GCC 16 or newer. If you do not have it yet,
[Installation](installation.md) shows the quickest ways to get it, including
one that needs no installation at all.

Include one header:

```cpp
#include <reflect/reflect.hpp>
```

Compile with C++26 and reflection switched on:

```sh
g++ -std=c++26 -freflection main.cpp -o main
```

Everything lives in the namespace `reflect`.

Most examples on this page use this struct:

```cpp
struct User {
    int id = 0;
    std::string name;
    int age = 0;
};
```

## Your first program

Print any struct, whatever its members are:

```cpp
#include <reflect/reflect.hpp>

#include <iostream>
#include <string>

struct User {
    int id = 0;
    std::string name;
    int age = 0;
};

template <typename T>
void print(const T& object) {
    reflect::for_each_field(object, [](auto field, const auto& value) {
        std::cout << field.name() << " = " << value << '\n';
    });
}

int main() {
    print(User{1, "Ada", 36});
}
```

```text
id = 1
name = Ada
age = 36
```

`for_each_field` calls the lambda once per member. `field` describes the
member (its name, its type, its annotations) and `value` is the member of
this particular object.

The lambda has to be generic, `[](auto field, ...)`, because every field has
its own type. [How it works](concepts.md) explains why; for using the library
it is enough to write `auto`.

## Fields

A *field* is a non-static data member.

### Loop over the fields of a type

When you only need information about the type and have no object, pass the
type as a template argument:

```cpp
reflect::for_each_field<User>([](auto field) {
    std::cout << field.name() << '\n';
});
```

```text
id
name
age
```

### Loop over the fields of an object

Pass an object, and the callback also receives a reference to each member:

```cpp
User user{1, "Ada", 36};

reflect::for_each_field(user, [](auto field, auto& value) {
    std::cout << field.name() << " = " << value << '\n';
});
```

```text
id = 1
name = Ada
age = 36
```

`value` is a real reference. If `user` is not const, you can assign to it.

### Treat fields differently by type

Inside the callback, `if constexpr` chooses code per field at compile time.
The type of a field is `decltype(field)::type`:

```cpp
User user{1, "Ada", 36};

reflect::for_each_field(user, [](auto field, auto& value) {
    if constexpr (std::is_same_v<typename decltype(field)::type, int>)
        value += 1;  // only the int fields
});

std::cout << user.id << ' ' << user.age << '\n';
```

```text
2 37
```

Use `if constexpr`, not a plain `if`. The callback is compiled once for every
field, and `value += 1` would not compile for the `std::string` member. With
`if constexpr` the branch is left out where it does not apply.

### Read and write one field by name

```cpp
User user{1, "Ada", 36};

auto name = reflect::get_field_of<User, "name">();

std::cout << name.get(user) << '\n';  // read
name.set(user, "Grace");              // write
name.get(user) += " Hopper";          // get returns a reference
std::cout << user.name << '\n';
```

```text
Ada
Grace Hopper
```

The name is checked by the compiler. A typo such as
`get_field_of<User, "nmae">()` is a compile error, not a run-time surprise.

The name has to be a string literal here. For a name that is only known when
the program runs, see
[Find a field by a run-time name](#find-a-field-by-a-run-time-name).

### Get a field by member pointer

If you prefer not to put member names into strings, use a pointer to member.
A rename done by your editor then updates this code too:

```cpp
User user{1, "Ada", 36};

auto age = reflect::get_field_of<&User::age>();
std::cout << age.name() << " = " << age.get(user) << '\n';
```

```text
age = 36
```

### Check whether a type has a field

```cpp
static_assert(reflect::has_field<User, "name">());
static_assert(!reflect::has_field<User, "email">());

std::cout << reflect::field_index_of<User, "age">() << '\n';
```

```text
2
```

`has_field` is most useful in generic code, to support types with and
without a certain member:

```cpp
// Works for any type; uses the "id" field if there is one.
template <typename T>
int id_or_zero(const T& object) {
    if constexpr (reflect::has_field<T, "id">())
        return reflect::get_field_of<T, "id">().get(object);
    else
        return 0;
}
```

### Ask a field about itself

```cpp
auto field = reflect::get_field_of<User, "name">();

std::cout << field.name() << '\n';
std::cout << field.is_public() << '\n';
std::cout << field.offset() << '\n';

// The type is a member typedef.
using Type = decltype(field)::type;
static_assert(std::is_same_v<Type, std::string>);
```

```text
name
true
8
```

A field also knows `is_private()`, `is_protected()`, `is_mutable()`,
`is_bit_field()`, `has_default_initializer()`, the class that declares it
(`declaring_type`) and a pointer to member (`pointer()`). The full list is in
the [reference](reference.md#field).

### Include inherited fields

`get_fields_of` and `for_each_field` list what a class declares itself.
`get_all_fields_of` adds the fields of its base classes, base classes first:

```cpp
struct Entity {
    int id = 7;
};
struct Timestamps {
    long created = 100;
};
struct Document : Entity, Timestamps {
    std::string title = "Report";
};
```

```cpp
Document document;

// Only what Document declares itself.
std::cout << reflect::get_fields_of<Document>().size() << '\n';

// Base classes first, then Document.
reflect::get_all_fields_of<Document>().for_each([&](auto field) {
    std::cout << field.name() << " = " << field.get(document) << '\n';
});
```

```text
1
id = 7
created = 100
title = Report
```

### Private fields

By default every declared member is listed, private and protected ones
included. That is what a serializer or a debug printer wants. To see only
the public members, pass `reflect::public_access`:

```cpp
class Vault {
public:
    int door = 1;

private:
    int code = 1234;
};
```

```cpp
// By default every declared member is listed, private ones included.
std::cout << reflect::get_fields_of<Vault>().size() << '\n';

// Ask for the public ones only.
std::cout << reflect::get_fields_of<Vault, reflect::public_access>().size() << '\n';

Vault vault;
reflect::for_each_field<reflect::public_access>(vault, [](auto field, auto& value) {
    std::cout << field.name() << " = " << value << '\n';
});
```

```text
2
1
door = 1
```

There are three policies:

| Policy | Lists |
| --- | --- |
| `reflect::unchecked_access` (default) | every declared member |
| `reflect::public_access` | public members only |
| `reflect::current_access()` | what the calling code may access: public members from outside the class, everything from inside one of its member functions |

Every `get_..._of` and `for_each_...` function accepts a policy.

## Annotations

An annotation is a piece of data that you attach to a declaration: a field,
a type, a function, an enumerator. The program can read it back at compile
time. Annotations are how a field says "skip me when saving", "my column is
called `mail`" or "I must be between 0 and 150".

If you know attributes from Java or C#, this is the same idea.

### Define and attach an annotation

An annotation is an ordinary struct. Nothing has to be registered.

```cpp
struct Skip {};
struct Serializable {};
struct Range {
    int min;
    int max;
};
struct Rename {
    reflect::static_string name;
};
struct Alias {
    reflect::static_string name;
};
```

Attach it with `[[=value]]` in front of the declaration. Separate several
annotations with commas:

```cpp
struct [[=Serializable{}]] Account {
    int id = 0;
    [[=Rename{"mail"}]] std::string email;
    [[=Range{0, 150}]] int age = 0;
    [[=Skip{}]] std::string password;
    [[=Alias{"nick"}, =Alias{"handle"}]] std::string nickname;
};
```

For text inside an annotation use `reflect::static_string` as the member
type. `std::string` and `std::string_view` are not allowed there by the
language. A `static_string` converts to `std::string_view` and can be
printed directly.

### Check for an annotation

```cpp
// On a type: annotation first, then the type.
static_assert(reflect::has_annotation<Serializable, Account>());
static_assert(!reflect::has_annotation<Serializable, User>());

// On a field: pass the field.
reflect::for_each_field<Account>([](auto field) {
    if constexpr (reflect::has_annotation<Skip>(field))
        std::cout << field.name() << " is skipped\n";
});
```

```text
password is skipped
```

The same two forms exist for every annotation function: `<Annotation, Type>()`
for a type, and `<Annotation>(thing)` for a field, function, base class,
enumerator or parameter.

### Read an annotation

`get_annotation` returns the annotation value:

```cpp
auto age = reflect::get_field_of<Account, "age">();

constexpr Range range = reflect::get_annotation<Range>(age);
std::cout << range.min << ".." << range.max << '\n';
```

```text
0..150
```

It expects exactly one annotation of that type. If there is none, or more
than one, the program does not compile.

### Annotations that may be missing

`find_annotation` returns a `std::optional`, and `get_annotation_or` lets you
supply the value to use instead:

```cpp
reflect::for_each_field<Account>([](auto field) {
    // find_annotation gives a std::optional.
    if (auto rename = reflect::find_annotation<Rename>(field))
        std::cout << field.name() << " -> " << rename->name << '\n';
});

// Or supply the value to use when the annotation is missing.
auto id = reflect::get_field_of<Account, "id">();
Range range = reflect::get_annotation_or<Range>(id, Range{0, 999});
std::cout << range.max << '\n';
```

```text
email -> mail
999
```

### The same annotation several times

A declaration may carry the same annotation type more than once.
`get_annotations` returns all of them as an array:

```cpp
auto nickname = reflect::get_field_of<Account, "nickname">();

for (const Alias& alias : reflect::get_annotations<Alias>(nickname))
    std::cout << alias.name << '\n';
```

```text
nick
handle
```

Which function to use:

| Function | Number of annotations of that type | Result |
| --- | --- | --- |
| `has_annotation` | any | `bool` |
| `get_annotation` | exactly one, otherwise a compile error | the value |
| `find_annotation` | none or one | `std::optional` |
| `get_annotation_or` | none or one | the value, or your fallback |
| `get_annotations` | any | `std::array` of values |

### Loop over annotations

```cpp
auto nickname = reflect::get_field_of<Account, "nickname">();

// Every annotation, whatever its type.
reflect::for_each_annotation(nickname, [](auto annotation) {
    if constexpr (annotation.template is<Alias>())
        std::cout << "alias " << annotation.value().name << '\n';
});

// Only the ones of one type; the callback gets the value directly.
reflect::for_each_annotation<Alias>(nickname, [](const Alias& alias) {
    std::cout << alias.name << '\n';
});

// The annotations of a type.
std::cout << reflect::get_annotations_of<Account>().size() << '\n';
```

```text
alias nick
alias handle
nick
handle
1
```

### Select fields by annotation

`with<A>()` and `without<A>()` narrow a list down:

```cpp
Account account{1, "ada@example.com", 36, "secret", "ada"};

reflect::get_fields_of<Account>().without<Skip>().for_each([&](auto field) {
    std::cout << field.name() << " = " << field.get(account) << '\n';
});

std::cout << reflect::get_fields_of<Account>().with<Range>().size() << '\n';
```

```text
id = 1
email = ada@example.com
age = 36
nickname = ada
1
```

### Accept only annotated types

`reflect::annotated_with` is a concept. It lets a function refuse types that
were not marked for it. The error then appears at the call, not somewhere
inside the function:

```cpp
// Accepts only types marked [[=Serializable{}]].
template <reflect::annotated_with<Serializable> T>
void save(const T&) {
    std::cout << "saving " << reflect::name_of<T>() << '\n';
}
```

```cpp
save(Account{});
// save(User{});   // does not compile: User is not Serializable
```

```text
saving Account
```

### Annotations of base classes

An annotation belongs to the class it is written on. A derived class does
not have it:

```cpp
struct [[=Serializable{}, =Rename{"accounts"}]] StoredAccount {};
struct [[=Rename{"admins"}]] AdminAccount : StoredAccount {};
struct GuestAccount : StoredAccount {};
```

```cpp
std::cout << reflect::has_annotation<Serializable, GuestAccount>() << '\n';
std::cout << reflect::has_inherited_annotation<Serializable, GuestAccount>() << '\n';

std::cout << reflect::get_inherited_annotation<Rename, GuestAccount>().name << '\n';
std::cout << reflect::get_inherited_annotation<Rename, AdminAccount>().name << '\n';
```

```text
false
true
accounts
admins
```

The `inherited` functions look at the class first and then at its base
classes. The nearest class that has the annotation answers, so
`AdminAccount` replaces the name its base class gives.

There is also `find_inherited_annotation`, which returns a `std::optional`,
and `get_inherited_annotation_or`, which takes a fallback.

## Enums

```cpp
enum class Color { Red, Green, Blue };
```

### Enum to text

```cpp
std::cout << reflect::enum_name(Color::Green) << '\n';
```

```text
Green
```

For a value that has no enumerator, `enum_name` returns an empty string.

### Text to enum

`enum_cast` returns a `std::optional`, which is empty for an unknown name:

```cpp
if (auto color = reflect::enum_cast<Color>("Blue"))
    std::cout << "found " << reflect::enum_name(*color) << '\n';

if (!reflect::enum_cast<Color>("Purple"))
    std::cout << "Purple is not a Color\n";
```

```text
found Blue
Purple is not a Color
```

The comparison is exact and case sensitive.

### List all values

```cpp
for (Color color : reflect::enum_values<Color>())
    std::cout << reflect::enum_name(color) << ' ';
std::cout << '\n';

for (std::string_view name : reflect::enum_names<Color>())
    std::cout << name << ' ';
std::cout << '\n';

std::cout << reflect::enum_count<Color>() << '\n';
```

```text
Red Green Blue 
Red Green Blue 
3
```

Enumerators can carry annotations too. Write the annotation after the name:
`Admin [[=Label{"administrator"}]] = 5`. Use `reflect::for_each_enumerator`
to read them.

## Member functions

```cpp
class Counter {
public:
    int value() const { return value_; }
    void add(int amount) { value_ += amount; }
    void reset() { value_ = 0; }
    static int instances() { return 1; }

private:
    int value_ = 0;
};
```

### List the functions of a class

```cpp
reflect::for_each_function<Counter>([](auto function) {
    std::cout << function.name() << " takes " << function.arity() << " argument(s)";
    if constexpr (function.is_const())
        std::cout << ", const";
    if constexpr (function.is_static())
        std::cout << ", static";
    std::cout << '\n';
});
```

```text
value takes 0 argument(s), const
add takes 1 argument(s)
reset takes 0 argument(s)
instances takes 0 argument(s), static
```

Only functions with a name are listed. Constructors, destructors, operators
and function templates are left out; see
[When the library does not cover it](#when-the-library-does-not-cover-it)
if you need them.

### Call a function

```cpp
Counter counter;

auto add = reflect::get_function_of<Counter, "add">();
add.invoke(counter, 5);

auto value = reflect::get_function_of<&Counter::value>();
std::cout << value.invoke(counter) << '\n';

// A static function needs no object.
std::cout << reflect::get_function_of<Counter, "instances">().invoke() << '\n';
```

```text
5
1
```

### Call every function that fits

`return_type` and `parameter_type<I>` are member typedefs of a function,
so the callback can decide at compile time which functions it handles:

```cpp
Counter counter;
counter.add(5);

// Call everything that takes no arguments and returns nothing.
reflect::for_each_function<Counter>([&](auto function) {
    using Function = decltype(function);
    if constexpr (function.arity() == 0 && !function.is_static() &&
                  std::is_void_v<typename Function::return_type>) {
        std::cout << "calling " << function.name() << '\n';
        function.invoke(counter);
    }
});

std::cout << counter.value() << '\n';
```

```text
calling reset
0
```

### Overloaded functions

When several functions share a name, the name alone is not enough.

```cpp
struct Shape {
    double scale(int factor) { return factor * 1.0; }
    double scale(double factor) { return factor * 2.0; }
    double area() const { return 3.5; }
};
```

```cpp
Shape shape;

// Every overload.
std::cout << reflect::get_functions_of<Shape, "scale">().size() << '\n';

// One overload, picked by its signature.
auto by_double = reflect::get_function_of<Shape, "scale", double(double)>();
std::cout << by_double.invoke(shape, 2) << '\n';

// Or let the compiler pick, as in shape.scale(2).
std::cout << reflect::invoke<"scale">(shape, 2) << '\n';
std::cout << reflect::invoke<"scale">(shape, 2.0) << '\n';
```

```text
2
4
2
4
```

The signature is written the way the function is declared:
`double(double)`, or `int() const` for a const member function.
`get_function_of<Shape, "scale">()` without a signature is a compile error
here, because the name is ambiguous.

### Function parameters

```cpp
auto add = reflect::get_function_of<Counter, "add">();

reflect::for_each_parameter(add, [](auto parameter) {
    std::cout << parameter.index() << ": " << parameter.name() << '\n';
});

using First = decltype(reflect::get_parameter_of<0>(add))::type;
static_assert(std::is_same_v<First, int>);
```

```text
0: amount
```

Parameters can carry annotations, and `has_default_argument()` tells whether
one has a default value.

## Base classes

Using `Document` from [above](#include-inherited-fields):

```cpp
Document document;

reflect::for_each_base<Document>([&](auto base) {
    using Base = typename decltype(base)::type;
    std::cout << reflect::name_of<Base>() << " with "
              << reflect::get_fields_of<Base>().size() << " field(s)\n";
});

// The base part of an object.
auto first = reflect::get_bases_of<Document>().get<0>();
Entity& entity = first.get(document);
std::cout << entity.id << '\n';
```

```text
Entity with 1 field(s)
Timestamps with 1 field(s)
7
```

`decltype(base)::type` is the base class itself. Pass it to any other
function of the library to keep going up the hierarchy.

Only direct base classes are listed. A base also knows `is_virtual()`,
`is_public()`, `is_protected()` and `is_private()`.

## Static members

Static data members have their own list. They need no object:

```cpp
struct Settings {
    static inline int instances = 0;
    static constexpr int version = 3;
    int volume = 5;
};
```

```cpp
reflect::for_each_static_field<Settings>([](auto field) {
    std::cout << field.name() << " = " << field.get() << '\n';
});

reflect::get_static_fields_of<Settings>().get<0>().set(2);
std::cout << Settings::instances << '\n';
```

```text
instances = 0
version = 3
2
```

## Working with lists

Every `get_..._of` function returns a *list*. The `for_each_...` functions
are shortcuts for getting the list and calling `for_each` on it.

### Size, index and names

```cpp
auto fields = reflect::get_fields_of<User>();

std::cout << fields.size() << '\n';
std::cout << fields.get<1>().name() << '\n';

for (std::string_view name : fields.names())
    std::cout << name << ' ';
std::cout << '\n';
```

```text
3
name
id name age 
```

`names()` is an ordinary `std::array`, the easiest thing to use when all you
need is the names.

### Ask questions and filter

```cpp
auto fields = reflect::get_fields_of<User>();
auto is_int = [](auto field) {
    return std::is_same_v<typename decltype(field)::type, int>;
};

std::cout << fields.any_of(is_int) << '\n';
std::cout << fields.all_of(is_int) << '\n';
std::cout << fields.count_if(is_int) << '\n';

// filter gives a new list.
fields.filter(is_int).for_each([](auto field) { std::cout << field.name() << ' '; });
std::cout << '\n';
```

```text
true
false
2
id age 
```

A list also has `none_of`, `map`, `for_each_while`, `with<A>()` and
`without<A>()`.

The lambda given to `filter` must not capture anything, because the result
is decided at compile time. The other functions accept any lambda.

### Use a language loop instead of a callback

C++26 has a loop for exactly this kind of list, `template for`. Use it if
you prefer a loop body over a lambda, or need `break`, `continue` or `return`
from the surrounding function:

```cpp
User user{1, "Ada", 36};

template for (auto field : reflect::get_fields_of<User>()) {
    std::cout << field.name() << " = " << field.get(user) << '\n';
}
```

```text
id = 1
name = Ada
age = 36
```

## Whole objects

### Compare two objects

`reflect::tie` returns a tuple of references to all fields, and tuples
already know how to compare:

```cpp
struct Point {
    int x = 0;
    int y = 0;
};
```

```cpp
Point a{1, 2};
Point b{1, 3};

std::cout << (reflect::tie(a) == reflect::tie(b)) << '\n';
std::cout << (reflect::tie(a) < reflect::tie(b)) << '\n';
```

```text
false
true
```

### Walk two objects side by side

Give `for_each_field` two objects of the same type, and the callback gets
both values. This is the base for a diff, a merge or a selective copy:

```cpp
User before{1, "Ada", 36};
User after{1, "Ada", 37};

reflect::for_each_field(before, after, [](auto field, const auto& a, const auto& b) {
    if (a != b)
        std::cout << field.name() << ": " << a << " -> " << b << '\n';
});
```

```text
age: 36 -> 37
```

### Find a field by a run-time name

`get_field_of<User, "age">()` needs the name at compile time. When the name
comes from a file, a request or the keyboard, use `visit_field`. It returns
`false` if no field has that name:

```cpp
User user{1, "Ada", 36};
std::string wanted = "age";  // from a file, the network, the user

bool found = reflect::visit_field(user, wanted, [](auto field, auto& value) {
    std::cout << field.name() << " = " << value << '\n';
});
std::cout << found << '\n';
```

```text
age = 36
true
```

The callback has to compile for every field of the type, because any of
them could be the one that matches. Use `if constexpr` on the field type if
the fields need different handling.

### Walk nested objects

`for_each_field` looks at one object. `walk` also goes into the objects
stored inside it, and tells you where it is:

```cpp
struct Address {
    std::string city;
    int zip = 0;
};
struct Employee {
    std::string name;
    Address home;
    Address office;
};
```

```cpp
Employee employee{"Ada", {"London", 1000}, {"Paris", 7500}};

reflect::walk(employee, reflect::overloaded{
    [](reflect::field_path path, auto, const std::string& value) {
        std::cout << path << " = \"" << value << "\"\n";
    },
    [](reflect::field_path path, auto, int value) {
        std::cout << path << " = " << value << '\n';
    },
});
```

```text
name = "Ada"
home.city = "London"
home.zip = 1000
office.city = "Paris"
office.zip = 7500
```

The callback receives the path, the field and the value. `reflect::overloaded`
bundles several lambdas into one callback, one per type you want to handle.

Your callback decides how deep the walk goes:

- A field whose type one of the lambdas accepts is a **value**. It is passed
  to that lambda. Here that is `std::string` and `int`.
- Any other field is **opened**, and its fields are walked. Here that is
  `Address`.

That is why a `std::string` is not taken apart into its internals, although
it is a class: there is a lambda for it. If you want `Address` handled as
one value, add a lambda that takes `const Address&`.

`path.to_string()` gives the path as a `std::string`, `path.back()` the name
of the field itself, and `path.size()` the depth.

### Build an object from input

`make` is the opposite of `for_each_field`: it asks your function for the
value of each field and returns the finished object.

```cpp
std::map<std::string, std::string> row{{"id", "7"}, {"name", "Ada"}, {"age", "36"}};

User user = reflect::make<User>([&](auto field) -> typename decltype(field)::type {
    const std::string& text = row.at(std::string(field.name()));
    if constexpr (std::is_same_v<typename decltype(field)::type, int>)
        return std::stoi(text);
    else
        return text;
});

std::cout << user.id << ' ' << user.name << ' ' << user.age << '\n';
```

```text
7 Ada 36
```

The return type `-> typename decltype(field)::type` says "return whatever
type this field has".

`make` asks for inherited fields too, base classes first. It needs a type
it can fill field by field: a plain struct, or a class with a default
constructor. For everything else there is `construct`.

### Build an object through its constructor

Many classes keep their fields private and can only be created through a
constructor:

```cpp
class Connection {
public:
    Connection(std::string host, int port) : host_(std::move(host)), port_(port) {}

    const std::string& host() const { return host_; }
    int port() const { return port_; }

private:
    std::string host_;
    int port_;
};
```

`construct` asks your function for each constructor argument, by parameter
name, and calls the constructor with the answers:

```cpp
std::map<std::string, std::string> settings{{"host", "example.org"}, {"port", "443"}};

Connection connection = reflect::construct<Connection>(
    [&](auto parameter) -> typename decltype(parameter)::type {
        const std::string& text = settings.at(std::string(parameter.name()));
        if constexpr (std::is_same_v<typename decltype(parameter)::type, int>)
            return std::stoi(text);
        else
            return text;
    });

std::cout << connection.host() << ':' << connection.port() << '\n';
```

```text
example.org:443
```

`construct<T>(fn)` works when the class has exactly one public constructor
that takes arguments. If it has several, say which one you mean by its
parameter types:

```cpp
auto constructor = reflect::get_constructor_of<Connection, std::string, int>();

Connection local = constructor.invoke("localhost", 8080);
std::cout << local.host() << ':' << local.port() << '\n';

reflect::for_each_parameter(constructor, [](auto parameter) {
    std::cout << parameter.name() << '\n';
});
```

```text
localhost:8080
host
port
```

`reflect::construct(constructor, fn)` combines the two: a chosen
constructor, filled by a callback. `get_constructors_of<T>()` lists all
constructors of a class.

Only public constructors can be called. Reflection can read a private
field, but the language does not let it call a private constructor.

### Stop a loop early

`for_each_field_while` stops as soon as the callback returns `false`, and
tells you whether it got to the end:

```cpp
User user{1, "", 36};

bool complete = reflect::for_each_field_while(user, [](auto field, const auto& value) {
    if (value == std::remove_cvref_t<decltype(value)>{}) {
        std::cout << field.name() << " is not set\n";
        return false;  // stop here
    }
    return true;
});
std::cout << complete << '\n';
```

```text
name is not set
false
```

## Field metadata as plain data

If you want no templates at all, `field_table` gives you an ordinary array
of structs with the name, offset and size of every field:

```cpp
static constexpr auto fields = reflect::field_table<Point>();

for (const reflect::field_info& field : fields)
    std::cout << field.name << " at offset " << field.offset << ", " << field.size
              << " bytes\n";
```

```text
x at offset 0, 4 bytes
y at offset 4, 4 bytes
```

The table cannot read or write the members, since it does not know their
types. It is meant for layout dumps, debug output and lookups by name.

## When the library does not cover it

`get_members_of` lists everything a class has, unfiltered: constructors,
destructors, operators, nested types, templates.

```cpp
int constructors = 0;
reflect::for_each_member<Counter>([&](auto member) {
    if constexpr (member.kind() == reflect::member_kind::constructor)
        ++constructors;
});
std::cout << constructors << " constructors\n";
```

```text
3 constructors
```

`Counter` declares no constructor, yet there are three: the default, copy
and move constructors that the compiler adds.

And every object the library hands you has `native()`, which returns the
`std::meta::info` behind it. From there the whole of `std::meta` is
available:

```cpp
auto field = reflect::get_field_of<User, "name">();

// native() is the std::meta::info behind the proxy.
constexpr std::meta::info info = field.native();
std::cout << std::meta::display_string_of(std::meta::type_of(info)) << '\n';
```

```text
std::__cxx11::basic_string<char>
```

## Common issues

**`reflect needs C++26 static reflection`**

The compiler flags are missing or the compiler is too old. Compile with
`-std=c++26 -freflection` on GCC 16 or newer.

**`request for member 'size' in 'value', which is of non-class type 'const int'`**
(or a similar error about a member that only some fields have)

A plain `if` was used where `if constexpr` is needed. The callback is
compiled for every field, so code that only works for some of them has to
sit inside `if constexpr`. See
[Treat fields differently by type](#treat-fields-differently-by-type).

**`no match for 'operator<<'`** inside a `for_each_field` callback

One of the fields has a type that cannot be printed, for example a nested
struct. Handle that type in its own `if constexpr` branch, as
[examples/json.cpp](../examples/json.cpp) does.

**`expected primary-expression before '>' token`** at `field.has_annotation<Skip>()`

Inside a generic lambda the member form needs the `template` keyword:
`field.template has_annotation<Skip>()`. The free function avoids this:
`reflect::has_annotation<Skip>(field)`.

**`uncaught exception of type 'std::meta::exception'`**

This is how the library reports a mistake at compile time. The text after
`what()` says what is wrong, for example
`reflect::get_field_of: no field with that name`.

**`'pred' is not a constant expression`** at a call to `filter`

The lambda passed to `filter` captures a variable. `filter` produces a new
list type and has to know the answer at compile time. Use `count_if`,
`any_of` or a check inside `for_each` when run-time values are involved.

More errors, and known compiler problems, are in
[Troubleshooting](troubleshooting.md).
