# Design Notes

Why the library looks the way it does, what was decided, what is still open,
and what an implementer has to know before changing the headers.

For how to use the library, read the [Getting Started Guide](getting-started.md).
For the exact signatures, read the [API Reference](reference.md).

- [Purpose](#purpose)
- [The five rules](#the-five-rules)
- [Access](#access)
- [Naming](#naming)
- [Decisions](#decisions)
- [Implementation notes](#implementation-notes)
- [Compiler defects the code works around](#compiler-defects-the-code-works-around)
- [How the library is verified](#how-the-library-is-verified)
- [Not in this version](#not-in-this-version)

## Purpose

C++26 reflection is a small set of primitives built around one opaque handle,
`std::meta::info`. Application code has to write `^^User`, pass an
`access_context`, loop with `template for`, call free functions such as
`identifier_of`, and splice with `[: :]`.

`reflect` is a thin, header-only layer that gives the common cases a direct
spelling:

```cpp
reflect::get_fields_of<User>();
reflect::get_members_of<User>();
reflect::get_bases_of<User>();
reflect::get_annotations_of<User>();
reflect::has_annotation<Serializable, User>();
```

Goals:

- No `^^`, no `std::meta::info`, no `template for` and no splice in ordinary
  application code.
- Results the caller can keep working with: named, typed proxy objects with
  member functions, instead of a `std::vector<std::meta::info>`.
- No run-time cost and no run-time metadata, unless the caller asks for a
  table.
- Nothing hidden: every proxy hands back its `std::meta::info` through
  `native()`.

Non-goals:

- New language features. This is a C++26 library.
- Run-time reflection, a type registry, or "find every type annotated with
  X". C++26 cannot enumerate those.
- Generating members or functions from annotations.

## The five rules

**R1. A proxy carries its identity in its type.** `reflect::field<Info>` is
an empty class template whose template argument is the `std::meta::info` of
one member. Two fields have two different proxy types, and all proxy members
are `static`.

This is what makes the API usable from a normal generic lambda. Inside
`[](auto field) { ... }` the parameter `field` is not a constant, but its
type is. `field.name()` and `reflect::has_annotation<Skip>(field)` are
therefore constant expressions and work in `if constexpr`.

The alternative is a proxy that stores the `info` as a data member. It
forces the caller into `template for` and `constexpr` variables for every
compile-time question, which is the code the library is meant to remove.

**R2. Proxies are ordinary empty objects.** They are default constructible,
copyable, and may exist at run time. Only `native()` returns the
consteval-only `std::meta::info`.

**R3. A discovery function returns a list type, not a container.**
`reflect::get_fields_of<User>()` returns an object of type
`reflect::list<reflect::field, ^^User::id, ^^User::name, ...>`. The list is
heterogeneous, because each element has its own proxy type. It is walked
with `for_each`, with `template for`, or by index.

**R4. Types are named with template arguments; members are reached through
proxies.** C++ accepts `User` as a template argument but not `User::id`.
Types therefore use `reflect::f<User>()`. A member is reached by discovery
(`for_each_field`), by name (`get_field_of<User, "id">()`) or by pointer
(`get_field_of<&User::id>()`). Every operation that takes a type also takes
a proxy as a function argument: `has_annotation<Skip, User>()` and
`has_annotation<Skip>(field)`.

**R5. Annotations are 0..n.** The language allows the same annotation type
several times on one declaration. The API has separate queries for "is there
one" (`has_annotation`), "exactly one" (`get_annotation`), "at most one"
(`find_annotation`, `get_annotation_or`) and "all of them"
(`get_annotations`).

## Access

Discovery takes an access policy as an optional template argument. It has to
be a template argument: `access_context` is a consteval-only type and cannot
be a run-time function parameter, and the list type depends on it.

The policy governs **discovery only**. `get`, `set`, `invoke` and `pointer`
use splices, and a splice performs no access check. Any proxy that was
discovered can read and write its member, whatever its access. Lookups by
name take no policy for the same reason: the caller named the member, so
there is nothing to filter.

The default is `unchecked_access` (decision D1). The typical first use of
the library is a serializer or a debug printer. With a public-only default
such code silently drops private fields, and the mistake is only found when
data is missing. Listing everything and letting the caller narrow it with
`public_access` fails in the visible direction.

## Naming

- Namespace `reflect` (decision D2). The first choice was `meta`. It is
  ambiguous with `std::meta` in any file that has `using namespace std;`,
  and such a file does not compile.
- `get_X_of<T>()` returns a list, `get_X_of<T, ...>()` with a name, pointer
  or index returns one element, `for_each_X` iterates, `has_X` answers yes
  or no.
- The annotation type comes first in every annotation function, so that the
  type form and the proxy form read the same: `has_annotation<A, T>()`,
  `has_annotation<A>(proxy)`. The concept `annotated_with<T, A>` is the one
  exception, because a concept used as a constraint receives the constrained
  type as its first argument.
- Where a function takes an object, the access policy is the first template
  parameter and the object type is deduced: `for_each_field<public_access>(user, fn)`,
  `tie<public_access>(user)`.

## Decisions

Decided:

| Id | Question | Decision | Reason |
| --- | --- | --- | --- |
| D1 | Default access policy | `unchecked_access`: every declared member, private included | See [Access](#access). Matches `getDeclaredFields` in Java |
| D2 | Namespace name | `reflect` | `meta` clashes with `std::meta` under `using namespace std;` |
| D3 | `get_` prefix | Kept: `get_fields_of`, for every function that retrieves something | One verb for everything that is looked up, next to `for_each_` and `has_` |
| D4 | Inherited fields | Two functions: `get_fields_of` lists declared fields only, `get_all_fields_of` adds those of the base classes | The name at the call says which fields a loop sees. One function with an option would put that choice into a template argument next to the access policy |
| D5 | Content of `get_functions_of` | Functions with a name only | An operator has no name to print or to look up, and `invoke<"name">` cannot reach it. Operators and conversion functions stay available through `get_members_of` and `as_function()` |
| D6 | Where a recursive walk stops | The callback decides: a field it can be called with is a value, any other field is opened | See implementation note 18 |
| D7 | `make` on a class with base classes | The callback is asked for the inherited fields too | An object built from input should not have half of its fields left at their defaults |

No decision is open at the moment.

## Implementation notes

Each of these was a compile error or a wrong result at some point.

1. **Build list types with `substitute`.** A `std::vector<std::meta::info>`
   becomes a type by substituting into the `list` template; the result is
   spliced as a type.

   ```cpp
   consteval std::meta::info make_list(std::meta::info proxy_template,
                                       const std::vector<std::meta::info>& items) {
       std::vector<std::meta::info> arguments;
       arguments.push_back(proxy_template);              // e.g. ^^field
       for (std::meta::info item : items)
           arguments.push_back(std::meta::reflect_constant(item));
       return std::meta::substitute(^^list, arguments);
   }

   template <typename T, access_context Access = default_access>
   using fields_of_t = [:detail::make_list(
       ^^field, std::meta::nonstatic_data_members_of(^^T, Access)):];
   ```

2. **Write `^^list` outside the class.** Inside `list` the name is the
   injected class name and reflects the current specialization. `substitute`
   then fails with "reflection does not represent a template". `make_list`
   is a namespace-scope function declared after a forward declaration of
   `list`.

3. **No pack indexing for `list::get<I>`.** `Proxy<Infos...[I]>` in a member
   declaration is a hard error ("cannot index an empty pack") as soon as an
   empty list is instantiated. `get` is a constrained function with a
   deduced return type and a `consteval` helper that indexes a
   `std::initializer_list<std::meta::info>`.

4. **Qualify with `::reflect` inside `namespace std`.** The `tuple_size` and
   `tuple_element` specializations are looked up in `std` first. The leading
   `::` keeps the header immune to a future `std::reflect`.

5. **Qualify every `std::meta::` call.** `member::is_function()` hides
   `std::meta::is_function` inside the class.

6. **`field::get` returns bit-fields by value.** A reference cannot bind to
   a bit-field. The function branches with
   `if constexpr (std::meta::is_bit_field(Info))` inside a `decltype(auto)`
   function.

7. **`static_field::get` has the declared return type `type&`.**
   `decltype(auto)` with a parenthesized splice returned a copy for a static
   member on GCC 16.2.

8. **A deduced return type wherever the result is valid for only some
   `Info`.** A declared return type is instantiated together with the class.
   `member::as_field` declared as returning `field<Info>` would violate the
   constraint of `field` for every member that is not a field.

9. **Every annotation query goes through one guard.**
   `std::meta::annotations_of` throws for entities that cannot carry an
   annotation, a member template for one. `detail::annotations` returns an
   empty vector for those. Without it `get_members_of<T>()` would not
   compile for a class with a member template.

10. **`get_annotations` is built by pack expansion**, not by filling a
    default-initialized array, so that annotation types without a default
    constructor work. `static_string` has none.

11. **`static_cast<void>` on each callback call** in the fold expression of
    `for_each`, so that a user-defined `operator,` cannot interfere.

12. **`list::filter` evaluates the predicate in a template argument.** The
    result is a type, so the predicate has to be usable in a constant
    expression without its captures. That is the reason for the "no
    captures" rule.

13. **Overloads are matched on the function type.** `get_function_of<T,
    "name", Signature>()` compares return type, parameter types, `const`
    and the reference qualifier of `type_of(function)` with those of the
    signature. `noexcept` is left out on purpose: a caller should not have
    to repeat it.

14. **`invoke<"name">` builds a real overload set.** One call operator per
    overload, each with exactly the parameter types of the function it
    forwards to, inherited into one class with `using ...::operator()...`.
    The compiler then performs ordinary overload resolution. The price is
    one extra move for by-value parameters, and no default arguments.

15. **`make` uses braces for aggregates.** `T{fn(field)...}` guarantees
    that the callback runs in declaration order, and it works for const and
    reference members. Parentheses would leave the order unspecified.

    A base class of an aggregate is an element of the braces, before the
    fields. `make` builds each base by calling itself, so the expression is
    `T{make<Base>(fn)..., fn(field)...}`. A class that is not an aggregate
    is default constructed and then assigned field by field, over the list
    from `get_all_fields_of`.

16. **A constructor is called through a thunk.** A reflection of a
    constructor cannot be spliced ("cannot use constructor or destructor in
    a splice expression") and has no type, so there is no pointer to take
    and nothing to call directly. `constructor::invoke` forwards to a
    function whose parameters are exactly the parameter types of that
    constructor and whose body is `return type(arguments...)`. Overload
    resolution then has one exact match, the constructor the proxy stands
    for.

    The call in the thunk is ordinary code, and it is access checked. That
    is why a private constructor can be listed but not called, unlike a
    private field or function, which a splice reaches.

    `get_constructors_of` leaves out copy and move constructors and deleted
    ones. Constructor templates and constructors inherited with
    `using Base::Base;` are not reported by `std::meta::members_of` as
    constructors, so they are not in the list.

17. **`construct` stores the arguments before the call.** The callback has
    to run once per parameter, in declaration order. The order of evaluation
    of function arguments is unspecified, that of a braced list is not. The
    results go into a braced `std::tuple` and are moved from there into the
    call.

18. **`walk` asks the callback whether a field is a value.** A walk over
    nested objects needs a rule for when to stop. A `std::string` is a class
    with fields, and so is a `std::vector`; nobody wants those opened.

    Two rules were tried first. "Descend into aggregates" opens `std::array`,
    which is an aggregate, and reports its internal member. "Descend into types
    marked with an annotation" is exact, but cannot be applied to types the
    user does not own.

    The rule in the library needs no list of types: if
    `fn(path, field, value)` is a valid call, the field is a value;
    otherwise it is opened. The callback is an overload set, so the user
    states the value types by writing the overloads that handle them, which
    has to be done anyway. The check is a `requires` expression in an
    `if constexpr`. The price is that an unconstrained `const auto&`
    parameter accepts everything and stops the descent.

19. **The headers include each other with relative paths**
    (`#include "list.hpp"`), so the include directory does not have to be on
    the search path, and `tools/amalgamate.py` can inline them into
    `single_include/reflect/reflect.hpp`.

## Compiler defects the code works around

Found with GCC 16.2. The user-visible side of each is described in
[Troubleshooting](troubleshooting.md#known-compiler-problems).

1. **A parameter reflection as a template argument.** A class template
   specialized on the reflection of a function parameter makes the compiler
   crash when debug information is written (`-g`). `parameter` is therefore
   `parameter<Function, Index>`, and obtains the parameter reflection inside
   its member functions.

2. **A parameter reflection inside a lambda.** Using such a template
   argument in a lambda fails with "`^^` cannot be applied a local entity
   for which there is an intervening lambda expression".
   `annotated::get_annotations` uses a helper function with an index
   sequence instead of a lambda.

3. **Two spellings of one function proxy.** A `reflect::function<^^C::f>`
   written by hand and a proxy for the same function produced by the library
   are emitted as two definitions of one symbol, and the build fails. Every
   way the library offers to obtain a
   function proxy can be mixed freely. `reflect::field<^^C::m>` written by
   hand is not affected. The documentation tells users not to write
   `reflect::function<...>` themselves.

## How the library is verified

- Every header has a test in `tests/`. The tests are plain programs: mostly
  `static_assert`, plus run-time checks through `CHECK`, which makes the
  program return a non-zero exit code on failure.
- Every code snippet in the Getting Started Guide is part of
  `tests/guide.cpp`, and every snippet in the README is part of
  `tests/readme.cpp`. Output shown in the documentation is the output of
  those programs. The cookbook quotes the programs in `examples/`, which are
  built and run as tests too.
- `tests/guide.cpp` is built a second time against the single header.
- The reference compiler is GCC 16.2 with
  `-std=c++26 -freflection -Wall -Wextra -Wpedantic`. The headers produce no
  warnings with these flags.

Not checked:

- Any compiler other than GCC. At the time of writing the Clang reflection
  fork on Compiler Explorer does not accept the library: its standard
  library predates the final C++26 names (`annotations_of_with_type`,
  `std::meta::exception`). MSVC has no reflection support.
- Compile-time cost on large types.
- Use from C++ modules.

## Not in this version

Both of these were tried in a prototype and work with GCC 16.2. They were
left out to keep the first version small.

- **Constraints on annotations**, such as "only on fields" or "at most
  once", declared on the annotation type. The library can check them when a
  type is inspected; C++26 cannot check them at the annotated declaration,
  so a type nobody reflects on is never checked.
- **Namespaces, free functions and variables** as reflection targets.
  `function::invoke` assumes a member function and would need a second form.

Not possible with C++26:

- Calling a private constructor, a constructor template or an inherited
  constructor through a proxy. See implementation note 16.
- Letting `make` skip a field. Every field gets a value from the callback.
  To fill only some fields, default construct the object and use
  `for_each_field`.
