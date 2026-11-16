# How it works

You can use `reflect` without reading this page. Read it when you want to
know why the API looks the way it does: why callbacks take `auto`, why
`if constexpr` appears so often, and why a list of fields cannot go into a
normal `for` loop.

There are three ideas. Everything else follows from them.

- [Reflection happens at compile time](#reflection-happens-at-compile-time)
- [Idea 1: a proxy carries its member in its type](#idea-1-a-proxy-carries-its-member-in-its-type)
- [Idea 2: a list carries its elements in its type](#idea-2-a-list-carries-its-elements-in-its-type)
- [Idea 3: types in angle brackets, things in parentheses](#idea-3-types-in-angle-brackets-things-in-parentheses)
- [What for_each_field really does](#what-for_each_field-really-does)
- [Compile time and run time](#compile-time-and-run-time)
- [How this maps to std::meta](#how-this-maps-to-stdmeta)

## Reflection happens at compile time

In Java, C# or Python, a running program can ask an object for its class and
the class for its fields. The information is kept in memory while the
program runs.

C++ does it differently. The compiler knows the members of every type, and
C++26 lets code ask the compiler *while it compiles*. Nothing is kept for
later. A program that prints the fields of a struct contains, after
compilation, simply the code that prints those fields, as if you had typed
it out by hand.

This has consequences, and all of them show up in the API:

- A member name used for lookup has to be known to the compiler:
  `get_field_of<User, "name">()` takes a literal. For names that arrive at
  run time there is [`visit_field`](getting-started.md#find-a-field-by-a-run-time-name),
  which compares against every field name.
- Decisions about fields are made with `if constexpr`.
- There is no run-time cost for the reflection itself: no table to look up,
  no string comparison, unless you ask for one.

## Idea 1: a proxy carries its member in its type

Functions such as `get_field_of` return a *proxy*: a small object that stands
for a member. The surprising part is that the object is empty. The member it
stands for is written into its **type**:

```cpp
auto id = reflect::get_field_of<User, "id">();

static_assert(std::is_same_v<decltype(id), reflect::field<^^User::id>>);
static_assert(std::is_empty_v<decltype(id)>);
```

`^^User::id` is the C++26 way to say "the member `id` of `User`, as a value
the compiler can work with". You do not have to write it; the library does.

Why build it this way? Because a function parameter is never a compile-time
constant in C++, but its type always is. Since the member is in the type,
every question about a proxy can be answered at compile time, even after the
proxy has been passed to another function:

```cpp
template <typename Field>
void describe(Field field) {
    static_assert(!field.name().empty());                    // compile time
    if constexpr (std::is_same_v<typename Field::type, int>)  // compile time
        std::cout << field.name() << " is an int\n";
}
```

This is the reason for two things you see everywhere:

- **Callbacks take `auto`.** The proxy for `id` and the proxy for `name` are
  different types, so the callback has to be a template.
- **The type of a field is `decltype(field)::type`.** It is a member typedef
  of the proxy type.

The kinds of proxy are `field`, `static_field`, `function`, `parameter`,
`base`, `enumerator`, `annotation` and `member`.

## Idea 2: a list carries its elements in its type

`get_fields_of<User>()` returns a list of field proxies. As each proxy has
its own type, they cannot go into a `std::vector` or an array. The list uses
the same trick as the proxy, and keeps its elements in its type:

```cpp
static_assert(std::is_same_v<
    decltype(reflect::get_fields_of<User>()),
    reflect::list<reflect::field, ^^User::id, ^^User::name, ^^User::age>>);
```

The list object is empty as well. Getting one costs nothing.

A normal `for` loop needs one element type, so it cannot walk such a list.
These can:

| | |
| --- | --- |
| `list.for_each(fn)` | calls `fn` once per element |
| `template for (auto x : list)` | the C++26 loop for this kind of sequence |
| `list.get<0>()` | one element by position |
| `list.names()` | the names as a `std::array<std::string_view, N>`, for a normal loop |

Because the elements are part of the type, operations such as `filter` and
`with<A>()` return a *different list type*. That is why the lambda given to
`filter` must not capture: its answer decides a type, and types are fixed at
compile time.

## Idea 3: types in angle brackets, things in parentheses

Every function follows one rule for its arguments:

- A **type** is a template argument: `get_fields_of<User>()`,
  `has_annotation<Serializable, User>()`.
- A **proxy** or an **object** is a function argument:
  `has_annotation<Skip>(field)`, `for_each_field(user, fn)`.

So each query has two forms, and the arguments tell you which one you are
looking at:

```cpp
reflect::has_annotation<Serializable, User>()   // is the type User annotated?
reflect::has_annotation<Skip>(field)            // is this field annotated?

reflect::for_each_field<User>(fn)               // fields of the type
reflect::for_each_field(user, fn)               // fields of this object, with values
```

The annotation type always comes first.

## What for_each_field really does

This call

```cpp
reflect::for_each_field(user, [](auto field, const auto& value) {
    std::cout << field.name() << " = " << value << '\n';
});
```

is compiled as if you had written

```cpp
auto print = [](auto field, const auto& value) {
    std::cout << field.name() << " = " << value << '\n';
};
print(reflect::field<^^User::id>{}, user.id);
print(reflect::field<^^User::name>{}, user.name);
print(reflect::field<^^User::age>{}, user.age);
```

There is no loop in the compiled program. The lambda is instantiated three
times, once per field, each time with the exact type of that member.

This explains the most common compile error. The body of the lambda is
compiled for *every* field. A statement that is valid for only some of them,
such as `value.size()`, breaks the others, unless it sits inside an
`if constexpr` that is false for them.

## Compile time and run time

A useful way to read any piece of code that uses the library:

| Compile time (usable in `if constexpr`, `static_assert`, template arguments) | Run time |
| --- | --- |
| which fields, functions, bases and enumerators exist | the values of the members |
| names, types, access, offsets | calling a function |
| which annotations there are and their values | text that comes from outside the program |

```cpp
User user{1, "Ada", 36};
auto id = reflect::get_field_of<User, "id">();

constexpr std::string_view name = id.name();  // compile time
int value = id.get(user);                     // run time
```

The two meet in ordinary code. `field.name()` is a constant, and it is also
just a `std::string_view` that you can print, compare with input, or use as
a map key.

## How this maps to std::meta

`reflect` adds no capability to the language. It is a thin layer, and you
can leave it at any point: every proxy has `native()`, which returns the
`std::meta::info` it wraps.

| reflect | std::meta |
| --- | --- |
| `get_fields_of<T>()` | `nonstatic_data_members_of(^^T, ctx)` |
| `get_static_fields_of<T>()` | `static_data_members_of(^^T, ctx)` |
| `get_functions_of<T>()` | `members_of(^^T, ctx)`, filtered to named functions |
| `get_members_of<T>()` | `members_of(^^T, ctx)` |
| `get_bases_of<T>()` | `bases_of(^^T, ctx)` |
| `get_enumerators_of<E>()` | `enumerators_of(^^E)` |
| `get_annotations_of<T>()` | `annotations_of(^^T)` |
| `field.name()` | `identifier_of(info)` |
| `decltype(field)::type` | `[: type_of(info) :]` |
| `field.get(object)` | `object.[: info :]` |
| `function.invoke(object, args...)` | `object.[: info :](args...)` |
| `reflect::unchecked_access` | `access_context::unchecked()` |
| `reflect::public_access` | `access_context::unprivileged()` |
| `reflect::current_access()` | `access_context::current()` |

For comparison, here is the first example of the guide written against
`std::meta` directly:

```cpp
constexpr auto context = std::meta::access_context::unchecked();
template for (constexpr std::meta::info member :
              std::define_static_array(
                  std::meta::nonstatic_data_members_of(^^User, context))) {
    std::cout << std::meta::identifier_of(member) << " = " << user.[:member:] << '\n';
}
```

and with the library:

```cpp
reflect::for_each_field(user, [](auto field, const auto& value) {
    std::cout << field.name() << " = " << value << '\n';
});
```

Both produce the same output. The first form is the right tool for writing
a library like this one. The second is meant for everything else.
