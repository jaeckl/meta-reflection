# `reflect` — a high-level API over C++26 `std::meta`

Specification and API reference, version 0.1 (draft), 2026-11-16.

This document is the implementation reference for the `reflect` library. Every
declaration in section 6 was compiled and exercised as a prototype on
GCC 16.2 with `-std=c++26 -freflection` before it was written down. Section 9
lists exactly what was and was not checked. Section 10 lists the decisions
that were made and the ones still open.

## 1. Purpose

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

- No `^^`, no `std::meta::info`, no `template for`, and no splice in ordinary
  application code.
- Results the caller can keep working with: named, typed proxy objects with
  methods, instead of a `std::vector<std::meta::info>`.
- No runtime cost and no runtime metadata. Everything resolves at compile time.
- Nothing hidden: every proxy hands back its raw `std::meta::info`.

Non-goals:

- New language features. This is a C++26 library, not a C++29 proposal.
- Runtime reflection, a type registry, or "find every type annotated with X".
  C++26 cannot enumerate those.
- Generating members or functions from annotations.

## 2. Requirements

| Item | Value |
|---|---|
| Language | C++26 with reflection (P2996), annotations (P3394), expansion statements (P1306) |
| Reference compiler | GCC 16.2, flags `-std=c++26 -freflection` |
| Form | Header-only, single header `include/reflect/reflect.hpp` |
| Namespace | `reflect` (decision D2) |
| Dependencies | Standard library only: `<meta>`, `<array>`, `<optional>`, `<string_view>`, `<type_traits>`, `<utility>`, `<vector>`, `<algorithm>` |

GCC 15 and earlier cannot build this library. The repository's current local
toolchain is MinGW GCC 15.2, so implementation needs a GCC 16 toolchain and
`-freflection` added to the build first.

## 3. Design model

Five rules shape the whole API.

**R1. A proxy carries its identity in its type.** `reflect::field<Info>` is an
empty class template whose template argument is the `std::meta::info` of one
member. Two fields have two different proxy types. All proxy members are
`static`. This is what makes the API usable from a normal generic lambda: the
parameter `field` is not a constant, but its type is, so `field.name()` and
`reflect::has_annotation<Skip>(field)` are constant expressions and work inside
`if constexpr`.

**R2. Proxies are ordinary empty objects.** They are default-constructible,
copyable, and may exist at run time. Only `native()` returns the consteval-only
`std::meta::info`.

**R3. A discovery function returns a list type, not a container.**
`reflect::get_fields_of<User>()` returns an object of type
`reflect::list<reflect::field, ^^User::id, ^^User::name, ...>`. The list is
heterogeneous, because each element has its own proxy type. It is consumed
with `for_each`, with `template for`, or by index.

**R4. Types are named with template arguments; members are reached through
proxies.** C++ accepts `User` as a template argument but not `User::id`. Types
therefore use `reflect::f<User>()`. A member is reached by discovery
(`for_each_field`), by name (`reflect::get_field_of<User, "id">()`), or directly
(`reflect::field<^^User::id>`). Every operation that takes a type also takes a
proxy as a function argument: `reflect::has_annotation<Skip, User>()` and
`reflect::has_annotation<Skip>(field)`.

**R5. Annotations are 0..n.** The language allows the same annotation type
several times on one declaration. The API has separate queries for "is there
one", "exactly one", "at most one", and "all of them".

## 4. Tour

This program was compiled and run with the prototype header in place of the
`#include <reflect/reflect.hpp>` line. Output follows it.

```cpp
#include <reflect/reflect.hpp>

#include <iostream>
#include <string>

struct Serializable {};
struct Skip {};
struct Column {
    int id;
};
struct Rename {
    reflect::static_string value;
};

struct Entity {
    long entity_id = 100;
};

struct [[=Serializable{}]] User : Entity {
    [[=Column{7}]] int id = 1;
    [[=Rename{"display name"}, =Column{8}]] std::string name = "Ada";
    [[=Skip{}]] std::string password = "secret";

    int age_in(int year) const { return year - 1990; }
    static int count() { return 5; }

private:
    int revision = 3;
};

template <typename T>
void print(const T& object) {
    static_assert(reflect::has_annotation<Serializable, T>(),
                  "print: type is not marked Serializable");

    std::cout << reflect::name_of<T>() << " {\n";
    reflect::for_each_field(object, [](auto field, const auto& value) {
        if constexpr (reflect::has_annotation<Skip>(field))
            return;

        std::string_view label = field.name();
        if constexpr (constexpr auto renamed = reflect::find_annotation<Rename>(field))
            label = renamed->value;

        std::cout << "  " << label << " = " << value;
        if constexpr (reflect::has_annotation<Column>(field))
            std::cout << "  (column " << reflect::get_annotation<Column>(field).id << ")";
        std::cout << '\n';
    });
    std::cout << "}\n";
}

int main() {
    User user;
    print(user);
}
```

```text
User {
  id = 1  (column 7)
  display name = Ada  (column 8)
  revision = 3
}
```

`revision` is private and is printed, because the default access policy
discovers every declared field (section 5). `entity_id` is not printed,
because `for_each_field` covers direct fields only (use `get_all_fields_of`
for inherited ones).

Rules for callbacks:

- Write a generic lambda: `[&](auto field) { ... }`. A non-generic parameter
  cannot work, because every element has a different type.
- `return` inside the callback moves on to the next element. There is no
  early exit from the whole loop.
- Prefer the free functions (`reflect::has_annotation<Skip>(field)`) inside
  generic code. The member forms need the `template` keyword on a dependent
  proxy: `field.template has_annotation<Skip>()`.
- The loop functions are `constexpr` and usable in constant evaluation.

## 5. Access policy

Discovery of fields, functions, members, and bases takes an access policy as
an optional non-type template argument of type `reflect::access_context`
(an alias of `std::meta::access_context`).

| Policy | Meaning | Underlying primitive |
|---|---|---|
| `reflect::unchecked_access` | Every declared member, including private and protected | `access_context::unchecked()` |
| `reflect::public_access` | What code at global scope may access | `access_context::unprivileged()` |
| `reflect::current_access()` | What the calling code may access | `access_context::current()` evaluated at the call site |
| `reflect::default_access` | Used when none is given. Equal to `unchecked_access` (decision D1) | |

```cpp
reflect::get_fields_of<User>();                          // 4: id, name, password, revision
reflect::get_fields_of<User, reflect::public_access>();     // 3
reflect::get_fields_of<User, reflect::current_access()>();  // 3 at namespace scope, 4 inside a User member
reflect::for_each_field<User, reflect::public_access>(fn);
reflect::for_each_field<reflect::public_access>(user, fn);
```

Two facts the implementer and the user must both know:

- The policy governs **discovery only**. `get`, `set`, `invoke`, and
  `pointer` use splices, and a splice performs no access check. Any proxy that
  was discovered, or constructed directly, can read and write its member. The
  prototype read a private field of a private base class this way.
- `reflect::get_field_of<T, "name">()` and a directly written
  `reflect::field<^^T::m>` take no policy and reach any member.

Annotation queries take no policy. Annotations are not subject to access
control.

## 6. API reference

All names are in namespace `reflect`. "Proxy" means one of `field`, `function`,
`base`, `enumerator`, `member`, `annotation`.

### 6.1 Common vocabulary

```cpp
using access_context = std::meta::access_context;

inline constexpr access_context public_access    = access_context::unprivileged();
inline constexpr access_context unchecked_access = access_context::unchecked();
inline constexpr access_context default_access   = unchecked_access;

consteval access_context current_access(
    access_context ctx = access_context::current());

template <typename D>
concept descriptor = requires {
    { D::native() } -> std::same_as<std::meta::info>;
};
```

`descriptor` is satisfied by every proxy type. Function templates that accept
"any proxy" are constrained with it.

### 6.2 Annotation members shared by proxies

`field`, `function`, `base`, `enumerator`, and `member` derive publicly from
`reflect::annotated<Info>` and inherit these static members. `A` is the
annotation type.

```cpp
template <std::meta::info Info>
struct annotated {
    static consteval std::meta::info native();

    template <typename A> static consteval bool             has_annotation();
    template <typename A> static consteval A                get_annotation();
    template <typename A> static consteval std::optional<A> find_annotation();
    template <typename A> static consteval std::array<A, N> get_annotations();

    static constexpr /* list<annotation, ...> */ annotations();
};
```

| Member | Result | Fails to compile when |
|---|---|---|
| `native()` | The raw reflection | never |
| `has_annotation<A>()` | `true` if at least one annotation of type `A` | never |
| `get_annotation<A>()` | The single annotation value | there are zero or several |
| `find_annotation<A>()` | The value, or `std::nullopt` when absent | there are several |
| `get_annotations<A>()` | All values of type `A`, in declaration order. `N` may be 0 | never |
| `annotations()` | A list of `annotation` proxies for every annotation, any type | never |

Type matching follows `std::meta::annotations_of_with_type`: `A` is compared
ignoring top-level `const`. Values are produced with `std::meta::extract<A>`.
`A` does not have to be default-constructible.

Some entities cannot carry annotations at all, a member template for one, and
`std::meta::annotations_of` throws for them. The library treats such an entity
as having no annotations: `has_annotation` is `false`, `get_annotations` is
empty, and `list::with` / `list::without` work on a class that contains member
templates.

### 6.3 `list`

```cpp
template <template <std::meta::info> class Proxy, std::meta::info... Infos>
struct list {
    static constexpr std::size_t size();
    static constexpr bool        empty();

    template <std::size_t I> requires (I < sizeof...(Infos))
    static constexpr auto get();                 // Proxy<I-th element>

    template <typename Fn>
    static constexpr void for_each(Fn&& fn);     // fn(Proxy<Infos>{}) for each, in order

    template <typename A> static constexpr auto with();     // only elements annotated with A
    template <typename A> static constexpr auto without();  // only elements not annotated with A

    static constexpr std::array<std::string_view, sizeof...(Infos)> names();
};
```

- A list is an empty object. Its content is its type.
- `with` and `without` return another `list` of the same proxy kind and can
  be chained. They are valid for lists of `field`, `function`, `base`,
  `enumerator`, and `member`. They are not valid on a list of `annotation`.
- `names()` is valid when every element has a name. It gives a plain array
  that a run-time `for` loop can walk.
- `std::tuple_size` and `std::tuple_element` are specialized for `list`, and
  the member `get<I>()` completes the tuple protocol. That is what makes
  `template for` work:

```cpp
template for (auto field : reflect::get_fields_of<User>().without<Skip>()) {
    std::cout << field.name() << '\n';
}
```

Write `auto`, not `constexpr auto`, when the list is a temporary. The
`constexpr` form compiles only over a `static constexpr` list variable.

Result type aliases, for code that needs to name the list type:

```cpp
template <typename T, access_context Access = default_access> using fields_of_t     = /* list<field, ...> */;
template <typename T, access_context Access = default_access> using all_fields_of_t = /* list<field, ...> */;
template <typename T, access_context Access = default_access> using functions_of_t  = /* list<function, ...> */;
template <typename T, access_context Access = default_access> using members_of_t    = /* list<member, ...> */;
template <typename T, access_context Access = default_access> using bases_of_t      = /* list<base, ...> */;
template <typename E>                                         using enumerators_of_t = /* list<enumerator, ...> */;
template <std::meta::info Target>                             using annotations_of_t = /* list<annotation, ...> */;
```

### 6.4 `field`

A non-static data member.

```cpp
template <std::meta::info Info>
    requires (std::meta::is_nonstatic_data_member(Info))
struct field : annotated<Info> {
    using type           = /* declared type of the member */;
    using declaring_type = /* class that declares the member */;

    static consteval bool             has_name();
    static constexpr std::string_view name();

    static consteval bool is_public();
    static consteval bool is_protected();
    static consteval bool is_private();
    static consteval bool is_mutable();
    static consteval bool is_bit_field();
    static consteval bool has_default_initializer();
    static consteval std::size_t offset();        // bytes from the start of declaring_type

    template <typename Object>
    static constexpr decltype(auto) get(Object&& object);

    template <typename Object, typename Value>
    static constexpr void set(Object& object, Value&& value);

    static constexpr auto pointer();              // type declaring_type::*
};
```

| Member | Primitive | Notes |
|---|---|---|
| `type` | `type_of` | Keeps `const` and references: `const int`, `int&` |
| `declaring_type` | `parent_of` | For an inherited field this is the base class |
| `has_name()` | `has_identifier` | `false` for an anonymous union member |
| `name()` | `identifier_of` | Requires `has_name()`. Compile error otherwise |
| `is_mutable()` | `is_mutable_member` | |
| `has_default_initializer()` | `has_default_member_initializer` | |
| `offset()` | `offset_of(Info).bytes` | |
| `get(object)` | `object.[:Info:]` | `T&` for an lvalue, `const T&` for a const lvalue, `T&&` for an rvalue. A bit-field is returned **by value** |
| `set(object, value)` | `object.[:Info:] = value` | Works for bit-fields and reference members. Ill-formed for a `const` member |
| `pointer()` | `&[:Info:]` | Ill-formed for bit-fields and reference members. Only an error when called |

`get` and `set` accept an object of `declaring_type` or of a class derived
from it.

### 6.5 `function`

A member function that has an identifier. Constructors, destructors,
operators, conversion functions, deleted functions, and function templates are
not `function` list elements; they are reachable through `get_members_of`.
Each overload is a separate element.

```cpp
template <std::meta::info Info>
    requires (std::meta::is_function(Info))
struct function : annotated<Info> {
    using return_type = /* return type */;
    template <std::size_t I> using parameter_type = /* type of parameter I */;

    static constexpr std::string_view name();
    static consteval std::size_t      arity();

    static consteval bool is_static();
    static consteval bool is_const();
    static consteval bool is_virtual();
    static consteval bool is_noexcept();
    static consteval bool is_public();
    static consteval bool is_protected();
    static consteval bool is_private();

    template <typename Object, typename... Args>       // non-static member functions
    static constexpr decltype(auto) invoke(Object&& object, Args&&... args);

    template <typename... Args>                        // static member functions
    static constexpr decltype(auto) invoke(Args&&... args);

    static constexpr auto pointer();
};
```

| Member | Primitive |
|---|---|
| `return_type` | `return_type_of` |
| `parameter_type<I>`, `arity()` | `parameters_of`, `type_of` |
| `is_static()` | `is_static_member` |
| `is_const()`, `is_virtual()`, `is_noexcept()` | same-named `std::meta` functions |
| `invoke(object, args...)` | `object.[:Info:](args...)` |
| `invoke(args...)` | `[:Info:](args...)` |
| `pointer()` | `&[:Info:]` |

### 6.6 `base`

A direct base class relationship.

```cpp
template <std::meta::info Info>
    requires (std::meta::is_base(Info))
struct base : annotated<Info> {
    using type = /* the base class */;

    static consteval bool is_virtual();
    static consteval bool is_public();
    static consteval bool is_protected();
    static consteval bool is_private();

    template <typename Object>
    static constexpr decltype(auto) get(Object&& object);   // the base subobject
};
```

`get(object)` returns a reference to the base subobject with the value
category and constness of `object`. Annotations on a base come from the
base-specifier: `struct User : [[=Tag{9}]] Audited`. To continue into the
base, pass `type` back in: `reflect::get_fields_of<typename decltype(base)::type>()`.

### 6.7 `enumerator`

```cpp
template <std::meta::info Info>
    requires (std::meta::is_enumerator(Info))
struct enumerator : annotated<Info> {
    using type = /* the enumeration type */;

    static constexpr std::string_view name();
    static constexpr type             value();
};
```

An annotation on an enumerator is written after its name:
`enum class Role { Admin [[=Rename{"administrator"}]] = 1, Guest };`

### 6.8 `member` and `member_kind`

Any direct member of a class, exactly as `std::meta::members_of` reports it
and in the same order. This includes implicitly declared constructors,
assignment operators, and the destructor.

```cpp
enum class member_kind {
    field, static_field, function, static_function,
    constructor, destructor, operator_function, conversion_function,
    type, template_, other,
};

template <std::meta::info Info>
struct member : annotated<Info> {
    static consteval member_kind kind();
    static consteval bool is_field();       // kind() == member_kind::field
    static consteval bool is_function();    // any function, including special members

    static consteval bool             has_name();
    static constexpr std::string_view name();          // requires has_name()
    static constexpr std::string_view display_name();  // implementation-defined text, always available

    static consteval bool is_public();
    static consteval bool is_protected();
    static consteval bool is_private();

    static constexpr auto as_field();       // field<Info>,    only when is_field()
    static constexpr auto as_function();    // function<Info>, only when is_function()
};
```

`kind()` classifies in this order: non-static data member, variable
(`static_field`), constructor, destructor, operator function, conversion
function, other function (`static_function` or `function`), type, template,
`other`.

### 6.9 `annotation`

One annotation on a declaration.

```cpp
template <std::meta::info Info>
struct annotation {
    using type = /* annotation type, top-level const removed */;

    static consteval std::meta::info native();
    template <typename A> static consteval bool is();
    static constexpr type value();
};
```

`annotation` is the only proxy that does not derive from `annotated`.

```cpp
reflect::for_each_annotation(field, [](auto ann) {
    if constexpr (ann.template is<Column>()) {
        constexpr Column column = ann.value();
    }
});
```

### 6.10 Discovery

```cpp
template <typename T, access_context Access = default_access>
constexpr fields_of_t<T, Access>     get_fields_of();
template <typename T, access_context Access = default_access>
constexpr all_fields_of_t<T, Access> get_all_fields_of();
template <typename T, access_context Access = default_access>
constexpr functions_of_t<T, Access>  get_functions_of();
template <typename T, access_context Access = default_access>
constexpr members_of_t<T, Access>    get_members_of();
template <typename T, access_context Access = default_access>
constexpr bases_of_t<T, Access>      get_bases_of();
template <typename E>
constexpr enumerators_of_t<E>        get_enumerators_of();

template <typename T>   constexpr annotations_of_t<^^T>          get_annotations_of();
template <descriptor D> constexpr annotations_of_t<D::native()>  get_annotations_of(D);

template <typename T, fixed_string Name>
constexpr /* field<...> */ get_field_of();
```

| Function | Primitive | Content and order |
|---|---|---|
| `get_fields_of<T>()` | `nonstatic_data_members_of(^^T, Access)` | Direct non-static data members, declaration order. No statics, no inherited fields |
| `get_all_fields_of<T>()` | recursion over `bases_of` + the above | Fields of every base first, depth-first in base order, then own fields. A virtual base contributes once |
| `get_functions_of<T>()` | `members_of` filtered by `is_function && has_identifier && !is_deleted` | Named member functions, static and non-static, declaration order |
| `get_members_of<T>()` | `members_of(^^T, Access)` | Everything, unfiltered |
| `get_bases_of<T>()` | `bases_of(^^T, Access)` | Direct bases, declaration order |
| `get_enumerators_of<E>()` | `enumerators_of(^^E)` | Declaration order |
| `get_annotations_of<T>()`, `get_annotations_of(proxy)` | `annotations_of` | Every annotation on that one declaration. Class annotations do not include field annotations, and the reverse |
| `get_field_of<T, "name">()` | lookup over direct fields by `identifier_of` | The one field with that name. Compile error when absent. Ignores the access policy |

`T` must be a complete class type. `E` must be a complete enumeration type.

### 6.11 Iteration

```cpp
template <typename T, access_context Access = default_access, typename Fn>
constexpr void for_each_field(Fn&& fn);                       // fn(field)

template <access_context Access = default_access, typename Object, typename Fn>
constexpr void for_each_field(Object&& object, Fn&& fn);      // fn(field, value)

template <typename T, access_context Access = default_access, typename Fn>
constexpr void for_each_function(Fn&& fn);                    // fn(function)
template <typename T, access_context Access = default_access, typename Fn>
constexpr void for_each_member(Fn&& fn);                      // fn(member)
template <typename T, access_context Access = default_access, typename Fn>
constexpr void for_each_base(Fn&& fn);                        // fn(base)
template <typename E, typename Fn>
constexpr void for_each_enumerator(Fn&& fn);                  // fn(enumerator)

template <typename T, typename Fn>
constexpr void for_each_annotation(Fn&& fn);                  // fn(annotation), on type T
template <descriptor D, typename Fn>
constexpr void for_each_annotation(D proxy, Fn&& fn);         // fn(annotation), on a proxy

template <typename A, typename T, typename Fn>
constexpr void for_each_annotation(Fn&& fn);                  // fn(const A&), on type T
template <typename A, descriptor D, typename Fn>
constexpr void for_each_annotation(D proxy, Fn&& fn);         // fn(const A&), on a proxy
```

- Each function calls `fn` once per element in list order and discards the
  result.
- The object form of `for_each_field` passes `field.get(object)` as the
  second argument, so it is a reference into `object` (a value for
  bit-fields). Take it as `const auto&` or `auto&&`. It requires a class type
  and deduces `T` from the argument.
- The typed annotation forms call `fn` once per annotation of type `A` with
  its value, zero times when there is none. The callback may be a
  non-generic lambda: `[](Column column) { ... }`.

### 6.12 Annotation queries as free functions

Each exists in a type form and a proxy form, with the semantics of the
same-named member in section 6.2.

```cpp
template <typename A, typename T>   consteval bool             has_annotation();
template <typename A, descriptor D> consteval bool             has_annotation(D);

template <typename A, typename T>   consteval A                get_annotation();
template <typename A, descriptor D> consteval A                get_annotation(D);

template <typename A, typename T>   consteval std::optional<A> find_annotation();
template <typename A, descriptor D> consteval std::optional<A> find_annotation(D);

template <typename A, typename T>   consteval auto             get_annotations();   // std::array<A, N>
template <typename A, descriptor D> consteval auto             get_annotations(D);  // std::array<A, N>
```

The annotation type always comes first: `reflect::has_annotation<Serializable, User>()`.

### 6.13 Type and enumeration utilities

```cpp
template <typename T>
constexpr std::string_view name_of();

template <typename E> requires std::is_enum_v<E>
constexpr std::string_view enum_name(E value);

template <typename E> requires std::is_enum_v<E>
constexpr std::optional<E> enum_cast(std::string_view name);
```

- `name_of<T>()` is `identifier_of(^^T)` when `T` has an identifier
  (`"User"`). Otherwise it is `display_string_of(^^T)`, whose text is
  implementation-defined (GCC 16.2 gives `"Box<int>"` and `"int"`).
- `enum_name(value)` returns the name of the first enumerator equal to
  `value`, or an empty view when there is none. It works on run-time values.
- `enum_cast<E>(name)` returns the enumerator with that exact name, or
  `std::nullopt`.

### 6.14 String helpers

An annotation value must be a structural type, so `std::string` and
`std::string_view` cannot be annotation members.

```cpp
struct static_string {
    const char* data;
    std::size_t size;

    consteval static_string(const char* text);
    consteval static_string(std::string_view text);

    constexpr std::string_view view() const;
    constexpr operator std::string_view() const;
};

template <std::size_t N>
struct fixed_string {
    char text[N]{};
    constexpr fixed_string(const char (&in)[N]);
    constexpr std::string_view view() const;
};
```

- `static_string` is the type to use for text inside an annotation:
  `struct Rename { reflect::static_string value; };` then
  `[[=Rename{"display name"}]]`. Its constructors store the characters with
  `std::define_static_string`. No factory function is needed.
- `fixed_string` exists to pass a string literal as a template argument. It
  is what `get_field_of<User, "id">()` takes.

## 7. Errors

Every misuse is a compile error. These are the messages GCC 16.2 produced for
the prototype.

| Misuse | Diagnostic |
|---|---|
| `get_annotation<A>` with zero or several annotations | `uncaught exception of type 'std::meta::exception'; 'what()': 'reflect::get_annotation: expected exactly one annotation of this type'` |
| `find_annotation<A>` with several annotations | same form, `'reflect::find_annotation: more than one annotation of this type'` |
| `get_field_of<T, "missing">()` | `'what()': 'reflect::get_field_of: no field with that name'` |
| Discovery on an incomplete type | `'what()': 'not a complete class type'` (from `std::meta`) |
| `reflect::field<^^SomeType>` (wrong entity kind) | `template constraint failure for ... struct reflect::field` |
| `pointer()` on a bit-field | `invalid pointer to bit-field` |
| `name()` or `names()` on an unnamed member | `'what()': 'reflection with has_identifier false'` (from `std::meta`) |

Library-detected errors are raised by throwing `std::meta::exception` from a
`consteval` function, with a message that starts with the `reflect::` function
name and with the offending reflection as the exception's `from` argument.

## 8. Implementation notes

These were all hit while building the prototype. Each is a requirement on the
implementation, not a suggestion.

1. **Build list types with `substitute`.** Turn a
   `std::vector<std::meta::info>` into a type by substituting into the `list`
   template, then splice the result as a type:

   ```cpp
   consteval std::meta::info make_list(std::meta::info proxy_template,
                                       const std::vector<std::meta::info>& items) {
       std::vector<std::meta::info> args;
       args.push_back(proxy_template);              // e.g. ^^field
       for (std::meta::info item : items)
           args.push_back(std::meta::reflect_constant(item));
       return std::meta::substitute(^^list, args);
   }

   template <typename T, access_context Access = default_access>
   using fields_of_t = [:detail::make_list(
       ^^field, std::meta::nonstatic_data_members_of(^^T, Access)):];
   ```

2. **Write `^^list` outside the class.** Inside `list`, the name is the
   injected-class-name and reflects the current specialization. `substitute`
   then fails with "reflection does not represent a template". `make_list`
   must be a namespace-scope function declared after a forward declaration of
   `list`.

3. **Do not use pack indexing for `get<I>`.** `Proxy<Infos...[I]>` in a
   member declaration is a hard error ("cannot index an empty pack") as soon
   as an empty list is instantiated. Use a constrained function with a
   deduced return type and a `consteval` helper that indexes an
   `std::initializer_list<std::meta::info>`.

4. **Qualify with `::reflect` inside `namespace std`.** The bodies of the
   `std::tuple_size` and `std::tuple_element` specializations are looked up
   in `std` first. Under the earlier namespace name `meta` this resolved to
   `std::meta` and failed; the leading `::` keeps the header immune to any
   future `std::reflect`.

5. **Do not name a member after a `std::meta` function you still need to
   call unqualified.** `member::is_function()` hides `std::meta::is_function`
   inside the class. Qualify every `std::meta::` call.

6. **Return bit-fields by value from `get`.** A reference cannot bind to a
   bit-field. Branch with `if constexpr (std::meta::is_bit_field(Info))`
   inside a `decltype(auto)` function.

7. **Use a deduced return type wherever the result type is valid for only
   some `Info`.** A declared return type is instantiated together with the
   class. `as_field` declared as returning `field<Info>` violates the
   constraint of `field` for every member that is not a field. `annotations()`
   declared as returning `annotations_of_t<Info>` evaluates the annotation
   query for every member, including ones that cannot carry annotations.

8. **Guard every annotation query.** Route all calls to
   `std::meta::annotations_of` and `annotations_of_with_type` through one
   helper that returns an empty vector unless the entity is a type, type
   alias, variable, function, function parameter, namespace, enumerator, base,
   or non-static data member. Without it, `get_members_of<T>()` fails to
   compile for any class with a member template.

9. **Build `get_annotations` by pack expansion**, not by filling a
   default-initialized array, so annotation types without a default
   constructor work (`static_string` has none).

10. **The access policy must be a template argument.** `access_context` is a
    consteval-only type and cannot be a run-time function parameter, and the
    list type depends on it. The caller's context is obtained only through
    `current_access()`, whose default function argument is evaluated at the
    call site.

11. **`static_cast<void>` each callback call** in the fold expression of
    `for_each`, so a user-defined `operator,` cannot interfere.

12. **GCC 16.2 defect.** Using a hand-written `reflect::function<^^C::f>` and a
    list-derived proxy for the same member function `f` in one translation
    unit produces two specializations (duplicate symbol at assembly, or an
    internal compiler error in `finish_member_declaration`). Either form
    alone works, and GCC trunk accepts the mix. `reflect::field<^^C::m>` is not
    affected. Tests must not mix the two for functions while 16.2 is the
    reference compiler.

## 9. Verification record

Checked on Compiler Explorer, compiler "x86-64 gcc 16.2", flags
`-std=c++26 -freflection -Wall -Wextra`, by compiling and running a prototype
header that implements section 6. The header itself produced no warnings.

- Every declaration in sections 6.1 to 6.14 was instantiated at least once.
- The tour in section 4, with the output shown.
- Access policies: counts under `unchecked_access`, `public_access`, and
  `current_access()` at namespace scope and inside a member function, on the
  discovery functions and on both forms of the loop functions.
- `get` value categories (`T&`, `const T&`, `T&&`), bit-fields, reference
  members, a `const` member, an anonymous union member.
- Inherited fields through `get_all_fields_of`, including reading a private
  field of a private base, and a diamond with a virtual base (counted once).
- Class templates (`Box<int>`, `std::pair<int, long>`), a union, and a local
  class as `T`.
- A class containing every member kind: virtual destructor, overloads, a
  member template, an operator, a conversion function, a deleted function, a
  nested type, a static data member.
- `invoke` on virtual, `noexcept`, overloaded, `&`-qualified, and static
  member functions.
- Repeated annotations, annotations on a base-specifier, on an enumerator,
  and on a member function.
- `for_each_field` inside a `constexpr` function used in a `static_assert`.
- The error cases in section 7.
- `using namespace std;` placed before and after the header. `reflect::`
  compiles in both positions. The earlier name `meta::` did not: "reference
  to 'meta' is ambiguous" (decision D2).

Not checked:

- Any compiler other than GCC. The Clang P2996 fork on Compiler Explorer
  rejects the prototype: its library predates the final C++26 names
  (`annotations_of_with_type`, `std::meta::exception`) and it rejects
  annotations on base-specifiers. MSVC has no reflection support.
- `invoke` on `&&`-qualified functions, and lambdas as `T`.
- Compile-time cost on large types.
- Use from C++ modules.
- A local build. Nothing was compiled on this machine, which has GCC 15.2.

## 10. Decisions

Decided:

| Id | Question | Decision | Reason |
|---|---|---|---|
| D1 | Default access policy | `unchecked_access`: every declared member, including private | Matches Java's `getDeclaredFields`. With a public-only default a serializer silently drops private fields |
| D2 | Namespace name | `reflect` | The first choice, `meta`, is ambiguous with `std::meta` in any file that has `using namespace std;` and does not compile there |

Still open. The draft uses the choice in the third column:

| Id | Question | Current choice in this draft | Alternative |
|---|---|---|---|
| D3 | `get_` prefix | Kept: `get_fields_of` | `fields_of`, matching `std::meta::members_of` |
| D4 | Inherited fields | `get_fields_of` is direct only; `get_all_fields_of` is separate | One function with an option |
| D5 | `get_functions_of` content | Named functions only | Also operators and conversion functions |

## 11. Not in version 0.1

- A proxy for static data members with `get` and `set`. They appear in
  `get_members_of` with `kind() == member_kind::static_field`.
- Function parameter names and parameter annotations.
- Lookup of a function by name (overloads make the result ambiguous).
- Constraints on annotations: allowed targets, "at most once". The library
  could check these when a type is inspected; C++26 cannot check them at the
  annotated declaration.
- Inherited annotations.
- Namespaces, free functions, and variables as reflection targets.
- A run-time field table beyond `list::names()`.
