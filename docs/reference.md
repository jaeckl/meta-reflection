# API Reference

Every public name of the library, version 0.1. For an introduction by task
see the [Getting Started Guide](getting-started.md).

- [Conventions](#conventions)
- [Headers](#headers)
- [Access policies](#access-policies)
- [Discovery](#discovery)
- [Looking up one member](#looking-up-one-member)
- [Iteration](#iteration)
- [Proxies](#proxies)
  - [Annotation members shared by all proxies](#annotation-members-shared-by-all-proxies)
  - [field](#field)
  - [static_field](#static_field)
  - [function](#function)
  - [constructor](#constructor)
  - [parameter](#parameter)
  - [base](#base)
  - [enumerator](#enumerator)
  - [member and member_kind](#member-and-member_kind)
  - [annotation](#annotation)
- [list](#list)
- [Annotation functions](#annotation-functions)
  - [Annotations of base classes](#annotations-of-base-classes)
- [Whole objects](#whole-objects)
- [Walking nested objects](#walking-nested-objects)
- [Constructing objects](#constructing-objects)
- [Builder](#builder)
- [Calling functions by name](#calling-functions-by-name)
- [Enums](#enums)
- [Types](#types)
- [Namespaces](#namespaces)
- [Registry](#registry)
- [Code generation](#code-generation)
- [Field table](#field-table)
- [Strings](#strings)
- [List type aliases](#list-type-aliases)
- [Compile-time errors](#compile-time-errors)
- [Version macros](#version-macros)

## Conventions

- All names are in namespace `reflect`. Names in `reflect::detail` are not
  part of the API.
- `T` is a complete class type, `E` a complete enumeration type, `A` an
  annotation type.
- A **proxy** is an object of one of the types `field`, `static_field`,
  `function`, `constructor`, `parameter`, `base`, `enumerator`, `member` or
  `annotation`.
  Proxies are empty; what they stand for is part of their type. All their
  member functions are `static`, and can be called on an object
  (`field.name()`) or on the type (`Field::name()`).
- Types are passed as template arguments, proxies and objects as function
  arguments: `has_annotation<A, T>()` and `has_annotation<A>(proxy)`.
- A function marked *compile time* can be used in `if constexpr`,
  `static_assert` and as a template argument.
- "Compile error" means the program is rejected with the message listed
  under [Compile-time errors](#compile-time-errors).

```cpp
template <typename D>
concept descriptor = requires {
    { D::native() } -> std::same_as<std::meta::info>;
};
```

`descriptor` is satisfied by every proxy type. Functions that accept "any
proxy" are constrained with it.

## Headers

`#include <reflect/reflect.hpp>` includes everything. The individual headers
can be included on their own.

| Header | Contents |
| --- | --- |
| `reflect/reflect.hpp` | everything |
| `reflect/access.hpp` | access policies, `descriptor` |
| `reflect/annotation.hpp` | `annotation`, annotation functions, `annotated_with` |
| `reflect/base.hpp` | `base`, `get_bases_of`, `for_each_base` |
| `reflect/builder.hpp` | `builder`, `required`, `generate_builder` (experimental) |
| `reflect/constructor.hpp` | `constructor`, `get_constructors_of`, `get_constructor_of`, `for_each_constructor`, `construct` |
| `reflect/enumerator.hpp` | `enumerator`, `enum_name`, `enum_cast`, `enum_values`, `enum_names`, `enum_count` |
| `reflect/field.hpp` | `field`, `get_fields_of`, `get_all_fields_of`, `get_field_of`, `has_field`, `field_index_of`, `field_types_t`, `for_each_field` |
| `reflect/function.hpp` | `function`, `get_functions_of`, `get_function_of`, `has_function`, `for_each_function`, `invoke` |
| `reflect/list.hpp` | `list` |
| `reflect/member.hpp` | `member`, `member_kind`, `get_members_of`, `for_each_member` |
| `reflect/namespace.hpp` | `for_each_class_in` (experimental) |
| `reflect/object.hpp` | `tie`, `make`, `visit_field`, `for_each_field_while`, two-object `for_each_field` |
| `reflect/parameter.hpp` | `parameter`, `has_parameters`, `get_parameter_of`, `get_parameter_names_of`, `for_each_parameter` |
| `reflect/registry.hpp` | `registered`, `registry`, `registered_classes`, `for_each_registered`, `create_registered` (experimental) |
| `reflect/static_field.hpp` | `static_field`, `get_static_fields_of`, `for_each_static_field` |
| `reflect/string.hpp` | `static_string`, `fixed_string` |
| `reflect/table.hpp` | `field_info`, `field_table` |
| `reflect/type.hpp` | `name_of`, `qualified_name_of` |
| `reflect/walk.hpp` | `walk`, `field_path`, `overloaded` |
| `reflect/config.hpp` | version macros, compiler check |
| `reflect/codegen.hpp` | `reflect::codegen`, for generator programs (experimental). Not included by `reflect/reflect.hpp`, and not in the single header. |

## Access policies

```cpp
using access_context = std::meta::access_context;

inline constexpr access_context unchecked_access;   // every declared member
inline constexpr access_context public_access;      // public members only
inline constexpr access_context default_access = unchecked_access;

consteval access_context current_access(access_context context = access_context::current());
```

| Policy | Lists |
| --- | --- |
| `unchecked_access` | every declared member, private and protected included |
| `public_access` | what code outside the class may access |
| `current_access()` | what the code calling it may access: public members from outside the class, everything from inside one of its member functions or friends |
| `default_access` | the policy used when none is given; equal to `unchecked_access` |

A policy is an optional template argument, written after the type:

```cpp
reflect::get_fields_of<User>();                          // default
reflect::get_fields_of<User, reflect::public_access>();
reflect::get_fields_of<User, reflect::current_access()>();
reflect::for_each_field<User, reflect::public_access>(fn);
reflect::for_each_field<reflect::public_access>(user, fn);   // object form: policy first
```

The policy decides what is **listed**. It does not restrict what a proxy can
do: a proxy for a private member reads, writes and calls that member.
Lookups by name or by pointer take no policy and find any member.
Annotations are not subject to access control.

## Discovery

Each function returns a [`list`](#list). The result is a constant, and the
object is empty.

```cpp
template <typename T, access_context Access = default_access> constexpr auto get_fields_of();
template <typename T, access_context Access = default_access> constexpr auto get_all_fields_of();
template <typename T, access_context Access = default_access> constexpr auto get_static_fields_of();
template <typename T, access_context Access = default_access> constexpr auto get_functions_of();
template <typename T, access_context Access = default_access> constexpr auto get_constructors_of();
template <typename T, access_context Access = default_access> constexpr auto get_members_of();
template <typename T, access_context Access = default_access> constexpr auto get_bases_of();
template <typename E>                                         constexpr auto get_enumerators_of();
template <typename T>                                         constexpr auto get_annotations_of();
template <descriptor D>                                       constexpr auto get_annotations_of(D proxy);
```

| Function | Elements | Content and order |
| --- | --- | --- |
| `get_fields_of<T>()` | `field` | Non-static data members declared by `T`, in declaration order. No inherited and no static members. |
| `get_all_fields_of<T>()` | `field` | The fields of every base class first, depth first in base order, then those of `T`. A virtual base contributes once. |
| `get_static_fields_of<T>()` | `static_field` | Static data members declared by `T`. |
| `get_functions_of<T>()` | `function` | Member functions declared by `T` that have a name, static and non-static. Each overload is its own element. Constructors, destructors, operators, conversion functions, deleted functions and function templates are not included. |
| `get_constructors_of<T>()` | `constructor` | The constructors of `T`, including a default constructor the compiler declares. The copy constructor, the move constructor, deleted constructors, constructor templates and constructors inherited with `using Base::Base;` are not included. |
| `get_members_of<T>()` | `member` | Everything, unfiltered, including implicitly declared special member functions. |
| `get_bases_of<T>()` | `base` | Direct base classes, in declaration order. |
| `get_enumerators_of<E>()` | `enumerator` | Enumerators, in declaration order. |
| `get_annotations_of<T>()`, `get_annotations_of(proxy)` | `annotation` | The annotations on that one declaration. The annotations of a class do not include those of its members. |

## Looking up one member

```cpp
// Fields
template <typename T, fixed_string Name> constexpr auto get_field_of();     // field
template <auto Pointer>                  constexpr auto get_field_of();     // field
template <typename T, fixed_string Name> consteval bool has_field();
template <typename T, fixed_string Name> consteval std::size_t field_index_of();

// Functions
template <typename T, fixed_string Name>                     constexpr auto get_functions_of();  // list of function
template <typename T, fixed_string Name>                     constexpr auto get_function_of();   // function
template <typename T, fixed_string Name, typename Signature> constexpr auto get_function_of();   // function
template <auto Pointer>                                      constexpr auto get_function_of();   // function
template <typename T, fixed_string Name>                     consteval bool has_function();

// Constructors
template <typename T, typename... Parameters> constexpr auto get_constructor_of();              // constructor

// Parameters
template <typename D>
concept has_parameters = descriptor<D> && std::meta::is_function(D::native());

template <std::size_t I, has_parameters D> constexpr auto get_parameter_of(D proxy);             // parameter
template <has_parameters D>                constexpr auto get_parameter_names_of(D proxy);       // std::array<std::string_view, N>
```

| Call | Result |
| --- | --- |
| `get_field_of<User, "id">()` | The field of `User` named `id`. Compile error if `User` declares no such field. Inherited fields are not found. |
| `get_field_of<&User::id>()` | The field a pointer to member points to. The class is taken from the pointer type, so for an inherited member it is the base class. Does not work for bit-fields and reference members, which have no pointer to member. |
| `has_field<User, "id">()` | Whether `User` declares a field with that name. |
| `field_index_of<User, "id">()` | Position of the field in `get_fields_of<User>()` with the default policy. Compile error if there is none. |
| `get_functions_of<User, "f">()` | Every overload named `f`. Empty if there is none. |
| `get_function_of<User, "f">()` | The one function named `f`. Compile error if there is none or if `f` is overloaded. |
| `get_function_of<User, "f", int(double) const>()` | The overload with exactly that signature. Compile error if there is none. |
| `get_function_of<&User::f>()` | The function a pointer to member function points to. For an overloaded function the pointer needs a cast that selects one. Not available for static member functions. |
| `has_function<User, "f">()` | Whether `User` declares a member function with that name. |
| `get_constructor_of<User, int, std::string>()` | The constructor `User(int, std::string)`. The parameter types are compared without top-level `const`; references count. Compile error if there is none. |
| `get_constructor_of<User>()` | The default constructor, if it takes no parameters. |
| `get_parameter_of<0>(proxy)` | The first parameter of a function or constructor. |
| `get_parameter_names_of(proxy)` | The parameter names. An unnamed parameter gives an empty string. |

`has_parameters` is satisfied by `function` and `constructor` proxies.

A **signature** is a function type, written as the function is declared:
return type, parameter types, and a trailing `const`, `&` or `&&` where the
function has one. `noexcept` is not written.

```cpp
struct Buffer {
    char* data();
    const char* data() const;
};

reflect::get_function_of<Buffer, "data", char*()>();
reflect::get_function_of<Buffer, "data", const char*() const>();
```

## Iteration

Each function calls `fn` once per element, in list order.

```cpp
template <typename T, access_context Access = default_access, typename Fn>
constexpr void for_each_field(Fn&& fn);                        // fn(field)
template <access_context Access = default_access, typename Object, typename Fn>
constexpr void for_each_field(Object&& object, Fn&& fn);       // fn(field, value)
template <access_context Access = default_access, typename A, typename B, typename Fn>
constexpr void for_each_field(A&& a, B&& b, Fn&& fn);          // fn(field, value_a, value_b)
template <access_context Access = default_access, typename Object, typename Fn>
constexpr bool for_each_field_while(Object&& object, Fn&& fn); // fn(field, value) -> bool

template <typename T, access_context Access = default_access, typename Fn>
constexpr void for_each_static_field(Fn&& fn);                 // fn(static_field)
template <typename T, access_context Access = default_access, typename Fn>
constexpr void for_each_function(Fn&& fn);                     // fn(function)
template <typename T, access_context Access = default_access, typename Fn>
constexpr void for_each_member(Fn&& fn);                       // fn(member)
template <typename T, access_context Access = default_access, typename Fn>
constexpr void for_each_base(Fn&& fn);                         // fn(base)
template <typename E, typename Fn>
constexpr void for_each_enumerator(Fn&& fn);                   // fn(enumerator)
template <typename T, access_context Access = default_access, typename Fn>
constexpr void for_each_constructor(Fn&& fn);                  // fn(constructor)
template <has_parameters D, typename Fn>
constexpr void for_each_parameter(D proxy, Fn&& fn);           // fn(parameter)

template <typename T, typename Fn>
constexpr void for_each_annotation(Fn&& fn);                   // fn(annotation), on the type T
template <descriptor D, typename Fn>
constexpr void for_each_annotation(D proxy, Fn&& fn);          // fn(annotation), on a proxy
template <typename A, typename T, typename Fn>
constexpr void for_each_annotation(Fn&& fn);                   // fn(const A&), on the type T
template <typename A, descriptor D, typename Fn>
constexpr void for_each_annotation(D proxy, Fn&& fn);          // fn(const A&), on a proxy
```

- The callback has to accept every element, so it is a generic lambda:
  `[](auto field) { ... }`. The typed annotation forms are the exception:
  their callback receives a value of type `A` and may be an ordinary lambda.
- The return value of the callback is ignored, except by
  `for_each_field_while`.
- `return` inside the callback continues with the next element.
- The object forms pass `field.get(object)` as the value. That is a
  reference into the object, with the constness of `object`, and a copy for
  a bit-field.
- The two-object form requires both arguments to have the same class type.
- `for_each_field_while` stops when the callback returns `false`. It returns
  `true` if every field was visited.
- All of them are `constexpr` and can be used in constant evaluation.

## Proxies

### Annotation members shared by all proxies

`field`, `static_field`, `function`, `constructor`, `parameter`, `base`,
`enumerator` and `member` have these members. The [free functions](#annotation-functions) do
the same and are easier to call in generic code.

```cpp
static consteval std::meta::info native();

template <typename A> static consteval bool             has_annotation();
template <typename A> static consteval A                get_annotation();
template <typename A> static consteval std::optional<A> find_annotation();
template <typename A> static consteval std::array<A, N> get_annotations();
static constexpr auto                                   annotations();   // list of annotation
```

| Member | Result | Compile error when |
| --- | --- | --- |
| `native()` | the `std::meta::info` behind the proxy | |
| `has_annotation<A>()` | whether there is at least one annotation of type `A` | |
| `get_annotation<A>()` | the annotation value | there is none, or more than one |
| `find_annotation<A>()` | the value, or `std::nullopt` | there is more than one |
| `get_annotations<A>()` | all values of type `A`, in declaration order; may be empty | |
| `annotations()` | every annotation, of any type | |

`A` is matched ignoring top-level `const`. `A` does not need a default
constructor.

A member that cannot carry annotations at all, such as a member template,
is treated as having none.

When the proxy is a parameter of a generic lambda, a member template call
needs the `template` keyword: `field.template has_annotation<A>()`.

### field

A non-static data member.

```cpp
template <std::meta::info Info>
struct field {
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
    static consteval std::size_t offset();

    template <typename Object>
    static constexpr decltype(auto) get(Object&& object);
    template <typename Object, typename Value>
    static constexpr void set(Object& object, Value&& value);
    static constexpr auto pointer();
};
```

| Member | Notes |
| --- | --- |
| `type` | Keeps `const` and references: `const int`, `int&`. |
| `declaring_type` | For a field from `get_all_fields_of`, this can be a base class. |
| `has_name()` | `false` for an unnamed member, such as an anonymous union. |
| `name()` | Compile error if `has_name()` is false. |
| `offset()` | Bytes from the start of `declaring_type`. |
| `get(object)` | `type&` for a non-const object, `const type&` for a const one, `type&&` for an rvalue. A bit-field is returned by value. |
| `set(object, value)` | Assigns. Works for bit-fields and reference members. Not available for a `const` member. |
| `pointer()` | Pointer to member. Not available for bit-fields and reference members. |

`get` and `set` accept an object of `declaring_type` or of a class derived
from it.

### static_field

A static data member.

```cpp
template <std::meta::info Info>
struct static_field {
    using type           = /* declared type of the member */;
    using declaring_type = /* class that declares the member */;

    static constexpr std::string_view name();

    static consteval bool is_public();
    static consteval bool is_protected();
    static consteval bool is_private();

    static constexpr type& get();
    template <typename Value>
    static constexpr void set(Value&& value);
    static constexpr auto pointer();
};
```

`get()` returns a reference to the variable; for a `constexpr` member it can
be used at compile time. `pointer()` is an ordinary pointer.

### function

A member function that has a name.

```cpp
template <std::meta::info Info>
struct function {
    using return_type = /* return type */;
    template <std::size_t I>
    using parameter_type = /* declared type of parameter I */;

    static constexpr std::string_view name();
    static consteval std::size_t      arity();

    static consteval bool is_static();
    static consteval bool is_const();
    static consteval bool is_virtual();
    static consteval bool is_noexcept();
    static consteval bool is_public();
    static consteval bool is_protected();
    static consteval bool is_private();

    template <typename Object, typename... Args>
    static constexpr decltype(auto) invoke(Object&& object, Args&&... args);   // non-static
    template <typename... Args>
    static constexpr decltype(auto) invoke(Args&&... args);                    // static

    static constexpr auto pointer();
};
```

| Member | Notes |
| --- | --- |
| `arity()` | Number of parameters. |
| `invoke(object, args...)` | Calls the function on `object`. The call goes to exactly this function, so no overload resolution takes place; the arguments are converted to its parameter types. |
| `invoke(args...)` | For a static member function. |
| `pointer()` | Pointer to member function, or a plain function pointer for a static one. |

Do not write `reflect::function<^^T::f>` yourself; obtain function proxies
from the library. See
[Known compiler problems](troubleshooting.md#known-compiler-problems).

### constructor

A constructor of a class.

```cpp
template <std::meta::info Info>
struct constructor {
    using declaring_type = /* the class */;
    template <std::size_t I>
    using parameter_type = /* declared type of parameter I */;

    static consteval std::size_t arity();

    static consteval bool is_default();
    static consteval bool is_copy();
    static consteval bool is_move();
    static consteval bool is_explicit();
    static consteval bool is_noexcept();
    static consteval bool is_public();
    static consteval bool is_protected();
    static consteval bool is_private();

    template <typename... Args>
    static constexpr declaring_type invoke(Args&&... args);
};
```

| Member | Notes |
| --- | --- |
| `declaring_type` | The class the constructor belongs to, which is the type `invoke` returns. |
| `arity()` | Number of parameters. |
| `is_default()` | Whether the constructor can be called without arguments. True for `User(int id = 0)` as well. |
| `is_copy()`, `is_move()` | Only ever true for a proxy obtained through `member::as_constructor()`; `get_constructors_of` leaves those two out. |
| `invoke(args...)` | Creates an object with exactly this constructor. The arguments are converted to the parameter types as in a normal call; overload resolution between constructors does not take place. Requires as many arguments as the constructor has parameters, so default arguments are not applied. |

`invoke` works for **public constructors only**. A private field can be read
through reflection, because a field can be spliced without an access check;
a constructor cannot be spliced at all, so the call is an ordinary one and
subject to access control. Calling `invoke` on a constructor that is not
public is a compile error.

The object is created in place: a class that can be neither copied nor moved
works. An argument for a parameter taken by value is moved once more than in
a direct call.

A constructor has no name and no return type. Its parameters are available
through `get_parameter_of`, `for_each_parameter` and `get_parameter_names_of`.

### parameter

One parameter of a member function or of a constructor.

```cpp
template <std::meta::info Function, std::size_t Index>
struct parameter {
    using type = /* declared type of the parameter */;

    static consteval std::size_t      index();
    static consteval bool             has_name();
    static constexpr std::string_view name();
    static consteval bool             has_default_argument();
};
```

| Member | Notes |
| --- | --- |
| `index()` | Position in the parameter list, from 0. |
| `has_name()` | `false` for an unnamed parameter, and for one that has different names in different declarations of the function. |
| `name()` | Compile error if `has_name()` is false. |

Unlike the other proxies, `parameter` is identified by a function and a
position. `native()` still returns the reflection of the parameter.

### base

A direct base class.

```cpp
template <std::meta::info Info>
struct base {
    using type = /* the base class */;

    static consteval bool is_virtual();
    static consteval bool is_public();
    static consteval bool is_protected();
    static consteval bool is_private();

    template <typename Object>
    static constexpr decltype(auto) get(Object&& object);
};
```

`get(object)` returns a reference to the base class part of `object`, with
the constness of `object`.

To continue into the base class, pass `type` on:
`reflect::get_fields_of<typename decltype(base)::type>()`.

An annotation on a base class is written in the base list:
`struct User : [[=Tag{9}]] Audited { ... };`

### enumerator

```cpp
template <std::meta::info Info>
struct enumerator {
    using type = /* the enumeration type */;

    static constexpr std::string_view name();
    static constexpr type             value();
};
```

An annotation on an enumerator is written after its name:
`Admin [[=Label{"administrator"}]] = 5`.

### member and member_kind

Any member of a class, as the language reports it.

```cpp
enum class member_kind {
    field,
    static_field,
    function,
    static_function,
    constructor,
    destructor,
    operator_function,
    conversion_function,
    type,
    template_,
    other,
};

template <std::meta::info Info>
struct member {
    static consteval member_kind kind();
    static consteval bool is_field();
    static consteval bool is_function();

    static consteval bool             has_name();
    static constexpr std::string_view name();
    static constexpr std::string_view display_name();

    static consteval bool is_public();
    static consteval bool is_protected();
    static consteval bool is_private();

    static constexpr auto as_field();          // field
    static constexpr auto as_static_field();   // static_field
    static constexpr auto as_function();       // function
    static constexpr auto as_constructor();    // constructor
};
```

| Member | Notes |
| --- | --- |
| `kind()` | See the table below. |
| `is_field()` | `kind() == member_kind::field`. |
| `is_function()` | True for every function, including constructors, destructors and operators. |
| `has_name()` | `false` for constructors, destructors, operators, conversion functions and unnamed members. |
| `name()` | Compile error if `has_name()` is false. |
| `display_name()` | Always available. The text is chosen by the compiler; use it for messages, not as a key. |
| `as_field()` | Only for `member_kind::field`. |
| `as_static_field()` | Only for `member_kind::static_field`. |
| `as_function()` | Only if `is_function()`. |
| `as_constructor()` | Only for `member_kind::constructor`. |

| `member_kind` | Member |
| --- | --- |
| `field` | non-static data member |
| `static_field` | static data member |
| `function` | non-static member function with a name |
| `static_function` | static member function |
| `constructor`, `destructor` | including the ones the compiler declares |
| `operator_function` | `operator==`, `operator()`, the assignment operators, ... |
| `conversion_function` | `operator bool`, ... |
| `type` | nested class, enumeration or alias |
| `template_` | member template of any kind |
| `other` | anything else |

### annotation

One annotation on a declaration.

```cpp
template <std::meta::info Info>
struct annotation {
    using type = /* the annotation type */;

    static consteval std::meta::info native();
    template <typename A> static consteval bool is();
    static constexpr type value();
};
```

```cpp
reflect::for_each_annotation(field, [](auto annotation) {
    if constexpr (annotation.template is<Range>()) {
        constexpr Range range = annotation.value();
    }
});
```

## list

The result of every discovery function.

```cpp
template <template <std::meta::info> class Proxy, std::meta::info... Infos>
struct list {
    static constexpr std::size_t size();
    static constexpr bool        empty();

    template <std::size_t I>
    static constexpr auto get();                                   // element I

    template <typename Fn>   static constexpr void for_each(Fn&& fn);
    template <typename Fn>   static constexpr bool for_each_while(Fn&& fn);

    template <typename Pred> static constexpr bool        any_of(Pred&& pred);
    template <typename Pred> static constexpr bool        all_of(Pred&& pred);
    template <typename Pred> static constexpr bool        none_of(Pred&& pred);
    template <typename Pred> static constexpr std::size_t count_if(Pred&& pred);

    template <typename Pred> static constexpr auto filter(Pred pred);   // list
    template <typename Fn>   static constexpr auto map(Fn&& fn);        // std::tuple

    template <typename A>    static constexpr auto with();              // list
    template <typename A>    static constexpr auto without();           // list

    static constexpr std::array<std::string_view, sizeof...(Infos)> names();
};
```

| Member | Notes |
| --- | --- |
| `size()`, `empty()` | Compile time. |
| `get<I>()` | The element at position `I`. Compile error if `I` is out of range. |
| `for_each(fn)` | Calls `fn(element)` for every element, in order. |
| `for_each_while(fn)` | Stops when `fn` returns `false`. Returns `true` if every element was visited. |
| `any_of`, `all_of`, `none_of`, `count_if` | Like the standard algorithms. `pred(element)` returns something convertible to `bool`. For an empty list `all_of` and `none_of` are `true`. |
| `filter(pred)` | A new list with the elements for which `pred(element)` is `true`. `pred` is evaluated at compile time: it may only use what the proxy provides, and a lambda must not capture. |
| `map(fn)` | A `std::tuple` of the results of `fn(element)`. |
| `with<A>()` | The elements that have an annotation of type `A`. |
| `without<A>()` | The elements that have none. |
| `names()` | The element names as an array. Requires every element to have a name. |

`filter`, `with` and `without` return lists again and can be chained.

A list supports the tuple protocol (`std::tuple_size`, `std::tuple_element`
and `get<I>()`), which makes it usable with `template for`:

```cpp
template for (auto field : reflect::get_fields_of<User>()) {
    // ...
}
```

Write `auto`, not `constexpr auto`, in that loop.

## Annotation functions

Each function has a form for a type and a form for a proxy. The annotation
type comes first.

```cpp
template <typename A, typename T>   consteval bool             has_annotation();
template <typename A, descriptor D> consteval bool             has_annotation(D proxy);

template <typename A, typename T>   consteval A                get_annotation();
template <typename A, descriptor D> consteval A                get_annotation(D proxy);

template <typename A, typename T>   consteval std::optional<A> find_annotation();
template <typename A, descriptor D> consteval std::optional<A> find_annotation(D proxy);

template <typename A, typename T>   consteval A                get_annotation_or(A fallback);
template <typename A, descriptor D> consteval A                get_annotation_or(D proxy, A fallback);

template <typename A, typename T>   consteval auto             get_annotations();          // std::array<A, N>
template <typename A, descriptor D> consteval auto             get_annotations(D proxy);   // std::array<A, N>

template <typename T, typename A>
concept annotated_with = has_annotation<A, T>();
```

| Function | Annotations of type `A` present | Result |
| --- | --- | --- |
| `has_annotation` | any number | `bool` |
| `get_annotation` | exactly one; otherwise a compile error | the value |
| `find_annotation` | none or one; more is a compile error | `std::optional<A>` |
| `get_annotation_or` | none or one; more is a compile error | the value, or `fallback` |
| `get_annotations` | any number | `std::array<A, N>`, in declaration order |

`annotated_with` takes the type first, so that it can be used as a
constraint: `template <reflect::annotated_with<Serializable> T>`.

All of these are evaluated at compile time. The fallback given to
`get_annotation_or` therefore has to be a constant.

See also `get_annotations_of` under [Discovery](#discovery) and
`for_each_annotation` under [Iteration](#iteration).

### Annotations of base classes

An annotation belongs to the class it is written on, and the functions
above look at that class only. These also look at its base classes:

```cpp
template <typename A, typename T, access_context Access = default_access>
consteval bool             has_inherited_annotation();

template <typename A, typename T, access_context Access = default_access>
consteval A                get_inherited_annotation();

template <typename A, typename T, access_context Access = default_access>
consteval std::optional<A> find_inherited_annotation();

template <typename A, typename T, access_context Access = default_access>
consteval A                get_inherited_annotation_or(A fallback);
```

The search starts at `T` and continues with its base classes, depth first
and in the order they are declared. It stops at the first class that has an
annotation of type `A`. That class answers, so a derived class can replace
what a base class says.

| Function | No class has `A` | The nearest class with `A` has one | ... has several |
| --- | --- | --- | --- |
| `has_inherited_annotation` | `false` | `true` | `true` |
| `get_inherited_annotation` | compile error | the value | compile error |
| `find_inherited_annotation` | `std::nullopt` | the value | compile error |
| `get_inherited_annotation_or` | `fallback` | the value | compile error |

- `Access` decides which base classes are searched. With `public_access`,
  private and protected bases are left out.
- For a `T` that is not a class, only `T` itself is looked at.
- There is no form for proxies: fields and functions have no base classes.

## Whole objects

```cpp
template <access_context Access = default_access, typename Object>
constexpr auto tie(Object& object);

template <access_context Access = default_access, typename Object, typename Fn>
constexpr bool visit_field(Object&& object, std::string_view name, Fn&& fn);

template <typename T, access_context Access = default_access, typename Fn>
constexpr T make(Fn&& fn);
```

**`tie(object)`** returns a `std::tuple` of references to the fields of
`object`, in declaration order: `std::tuple<int&, std::string&>` for a
non-const object, references to const for a const one. A bit-field is copied
into the tuple. The tuple can be compared, assigned from another tuple and
used with structured bindings.

**`visit_field(object, name, fn)`** calls `fn(field, value)` for the field
whose name equals `name`, which may be a run-time value, and returns `true`.
It returns `false`, without calling `fn`, if there is no such field. `fn` is
instantiated for every field of the type and has to compile for each.

**`make<T>(fn)`** builds a `T`. It calls `fn(field)` for every field and
uses the result as the value of that field.

Inherited fields belong to the object, so `fn` is asked for them as well:
the fields of the base classes first, then those of `T`. That is the order
of `get_all_fields_of<T>()`.

- If `T` is an aggregate, the object is created with braces, `T{...}`, and
  each base class is built the same way. Const and reference members work,
  and `T` needs no default constructor.
- Otherwise `T` is default constructed and each field is assigned. Every
  field then has to be assignable.

`fn` usually declares its return type as
`-> typename decltype(field)::type`.

For `for_each_field(a, b, fn)` and `for_each_field_while` see
[Iteration](#iteration).

## Walking nested objects

```cpp
template <access_context Access = default_access, typename Object, typename Fn>
constexpr void walk(Object&& object, Fn&& fn);                 // fn(path, field, value)

class field_path {
public:
    constexpr std::size_t      size() const;
    constexpr std::string_view operator[](std::size_t index) const;
    constexpr std::string_view back() const;
    constexpr auto             begin() const;
    constexpr auto             end() const;
    constexpr std::string      to_string(std::string_view separator = ".") const;

    template <typename Traits>
    friend std::basic_ostream<char, Traits>& operator<<(
        std::basic_ostream<char, Traits>&, field_path);
};

template <typename... Fns>
struct overloaded : Fns... {
    using Fns::operator()...;
};
```

**`walk(object, fn)`** visits the fields of `object`, and the fields of the
objects nested inside it, depth first. For every field it does one of two
things:

1. If `fn(path, field, value)` is a valid call, it makes that call. The
   field is a value as far as the walk is concerned.
2. Otherwise the field has to be of class type, and the walk continues with
   the fields of that object. It is a compile error if the field is not a
   class.

The callback therefore lists the types it treats as values, usually as
`reflect::overloaded{...}` with one lambda per type, or with constrained
parameters such as `const std::integral auto&`.

- `value` is `field.get(parent)`: a reference into the object, const for a
  const object, and a copy for a bit-field.
- Inherited fields are included, base classes first.
- The access policy applies at every level.
- A callback that accepts every type, such as one taking `const auto&`,
  never lets the walk descend.
- The return value of `fn` is ignored.

**`field_path`** is the chain of field names from the object `walk` was
called with down to the current field.

| Member | Result |
| --- | --- |
| `size()` | The depth: 1 for a field of the outermost object. |
| `path[i]` | The i-th name, from the outside in. |
| `back()` | The name of the current field. |
| `begin()`, `end()` | Iterators over the names, as `std::string_view`. |
| `to_string(separator)` | The names joined into a `std::string`: `"home.city"`. |
| `out << path` | Writes the names joined with `.` to a stream. |

A `field_path` refers to storage owned by the walk. It is valid during the
call it was passed to; use `to_string()` to keep it.

**`overloaded`** combines several function objects into one that has all
their call operators.

## Constructing objects

```cpp
template <std::meta::info C, typename Fn>
constexpr auto construct(constructor<C> constructor, Fn&& fn);   // the class of the constructor

template <typename T, typename Fn>
constexpr T construct(Fn&& fn);
```

**`construct(constructor, fn)`** calls `fn(parameter)` for every parameter
of the constructor, from left to right, and then calls the constructor with
the results. Each result has to be convertible to the type of its parameter.

**`construct<T>(fn)`** does the same with the one public constructor of `T`
that takes arguments. It is a compile error if `T` has no such constructor,
or more than one; choose one with `get_constructor_of` in that case.

| | `make<T>(fn)` | `construct<T>(fn)` |
| --- | --- | --- |
| Asks the callback for | every field | every constructor parameter |
| The class needs | to be an aggregate, or a default constructor and assignable fields | a public constructor |
| Private members | are set directly | are set by the constructor |
| Invariants checked by the constructor | bypassed | kept |

The order of the calls to `fn` is guaranteed. Every argument is stored
before the constructor runs, which costs one move per argument compared to a
direct call.

A default argument cannot be used: reflection tells whether a parameter has
one (`parameter.has_default_argument()`), not what its value is. To leave a
trailing argument out, pick a constructor with fewer parameters.

## Builder

Experimental. See [Generating code](code-generation.md#way-1-a-builder-without-generated-code)
for an introduction.

```cpp
struct required_t {};
inline constexpr required_t required{};          // [[=reflect::required]]

struct generate_builder {                        // [[=reflect::generate_builder{}]]
    static_string prefix = "with_";
};

template <typename T, std::meta::info... Set>
class builder {
public:
    constexpr builder();

    template <fixed_string Name, typename Value>
    [[nodiscard]] constexpr auto set(this builder self, Value&& value);   // builder<T, Set..., field>

    [[nodiscard]] constexpr T build(this builder self);
};
```

`reflect::builder<T>()` starts with a default-constructed `T`.

**`set<"name">(value)`** assigns `value` to the field `name` and returns a
builder whose type has that field added to `Set`. The field may be declared
by `T` or by one of its base classes, and may be private. Compile error if
there is no such field, if it was set before, or if the value cannot be
assigned to it.

**`build()`** returns the object. Compile error if a field of `T` or of a
base class is annotated with `reflect::required` and is not in `Set`.

- Both functions take the builder by value: called on a temporary they
  move the object along, called on a variable they copy it and leave the
  variable usable.
- The result of `set` has to be used; the builder it was called on is not
  changed.
- `T` needs a default constructor, and the fields have to be assignable.
- A builder works at compile time if `T` does.

`generate_builder` is not used by `reflect::builder`. It asks the
[builder processor](#code-generation) for a generated builder class, and
does nothing in a build without that processor.

## Calling functions by name

```cpp
template <fixed_string Name, typename Object, typename... Args>
constexpr decltype(auto) invoke(Object&& object, Args&&... args);
```

`reflect::invoke<"name">(object, args...)` calls the member function `name`
of `object`. If the name is overloaded, the compiler chooses the overload by
the usual rules, as it would for `object.name(args...)`. The return type is
that of the chosen function.

Differences to a direct call:

- Default arguments are not applied. Every argument has to be passed.
- Only functions declared by the class of `object` are considered, not
  inherited ones.
- A parameter taken by value is moved once more than in a direct call.

Compile error if the class has no function with that name, if no overload
accepts the arguments, or if the call is ambiguous.

To call one specific overload regardless of the argument types, get it with
`get_function_of<T, "name", Signature>()` and use its `invoke`.

## Enums

```cpp
template <typename E> constexpr std::string_view enum_name(E value);
template <typename E> constexpr std::optional<E> enum_cast(std::string_view name);
template <typename E> constexpr std::size_t      enum_count();
template <typename E> constexpr auto             enum_values();   // std::array<E, N>
template <typename E> constexpr auto             enum_names();    // std::array<std::string_view, N>
```

| Function | Result |
| --- | --- |
| `enum_name(value)` | The name of the enumerator with that value. Empty if there is none. If several enumerators share the value, the first one declared. |
| `enum_cast<E>(name)` | The enumerator with exactly that name, or `std::nullopt`. Case sensitive. |
| `enum_count<E>()` | The number of enumerators. |
| `enum_values<E>()` | All enumerator values, in declaration order. |
| `enum_names<E>()` | All enumerator names, in declaration order. |

`enum_name` and `enum_cast` work on run-time values. They compare against
each enumerator in turn.

## Types

```cpp
template <typename T>
constexpr std::string_view name_of();

template <typename T>
constexpr std::string_view qualified_name_of();

template <typename T, access_context Access = default_access>
using field_types_t = /* std::tuple<field types...> */;
```

`name_of<T>()` is the name of a type: `"User"` for `struct User`. A type
that has no simple name, such as `int`, a pointer or `Box<int>`, gives the
text the compiler prints for it. That text is not the same on every
compiler, so do not store or compare it.

`qualified_name_of<T>()` adds the namespaces and classes around the type:
`"shop::User"`, or `"shop::Order::Line"` for a nested class. An alias gives
the name of the type it stands for. This is the name to print in generated
code. The enclosing scopes are left out if the type cannot be named from
outside one of them: a class in an unnamed namespace or inside a function. A type without a
simple name gives the compiler's text, as with `name_of`.

`field_types_t<T>` is a tuple type with the declared types of the fields of
`T`: `std::tuple<int, std::string>` for a struct with an `int` and a
`std::string` member.

## Namespaces

Experimental.

```cpp
template <std::meta::info Namespace, typename Fn>
constexpr void for_each_class_in(Fn&& fn);          // fn(std::type_identity<T>{})
```

Calls `fn` once for every class declared directly in a namespace, in
declaration order. The namespace is passed as a reflection: `^^shop`, or
`^^::` for the global namespace.

```cpp
reflect::for_each_class_in<^^shop>([]<typename T>(std::type_identity<T>) {
    if constexpr (reflect::has_annotation<Entity, T>())
        std::cout << reflect::qualified_name_of<T>() << '\n';
});
```

- Listed: complete classes and structs.
- Not listed: class templates, classes that are only declared, unions,
  enums, aliases, and the classes of nested namespaces.
- The list is what the compiler has seen of the namespace at the point of
  the call. Classes declared later in the file are missing, so call it
  after the headers are included, and do not call it for the same namespace
  from a header that different source files include at different points.

This is the one place where the library reflects on something other than a
class or an enum. Free functions and variables of a namespace are not
covered.

## Registry

Experimental. The classes of a project that carry an annotation. The list
is written by a code generator during the build; see
[The registry processor](code-generation.md#the-registry-processor).

```cpp
struct registered_t {};
inline constexpr registered_t registered{};          // [[=reflect::registered]]

template <typename Annotation>
struct registry;                                     // defined by generated code

template <template <typename...> typename Template, typename Annotation = registered_t>
using registered_classes = /* Template<the registered classes...> */;

template <typename Annotation = registered_t, typename Fn>
constexpr void for_each_registered(Fn&& fn);         // fn(std::type_identity<T>{})

template <typename Base, typename Annotation = registered_t>
std::unique_ptr<Base> create_registered(std::string_view name);
```

`Annotation` says which registry is meant: the classes marked
`[[=reflect::registered]]` by default, or the classes that carry an
annotation of your own for which `reflect::codegen::registry_processor<Annotation>`
was run.

All three need the header that the processor generated. In a source file
that does not include it, `for_each_registered` and `create_registered` are
a compile error, and `registered_classes` names an incomplete type.

**`registered_classes<Template>`** is `Template` with the registered classes
as its arguments, in the order the generator found them:
`registered_classes<std::variant>` is
`std::variant<shapes::Circle, shapes::Square, shapes::Triangle>`. For an
annotation that no class carries it is `Template<>`.

**`for_each_registered(fn)`** calls `fn` once per registered class, in the
same order. It can be used at compile time.

**`create_registered<Base>(name)`** creates an object of the registered
class whose `qualified_name_of` equals `name`, and returns it as
`std::unique_ptr<Base>`. The name may be computed while the program runs.

- The result is empty if no registered class has that name, if the class is
  neither `Base` nor publicly derived from it, or if it has no default
  constructor. An abstract class has none.
- `Base` needs a virtual destructor, unless the only class that can be
  created is `Base` itself. Otherwise: compile error.
- The object is value-initialized.
- The name is compared with one class after the other.

**`registry<Annotation>`** is only declared by the library. The generated
header defines it for one annotation, with a member template `apply` that
the functions above use. There is nothing to call on it directly.

## Code generation

Experimental. `#include <reflect/codegen.hpp>`, namespace
`reflect::codegen`. These are for generator programs, which run while a
project is built; see [Generating code](code-generation.md#way-2-processors).

### Finding classes

```cpp
template <fixed_string... Files>
struct inputs {};

template <fixed_string... Files, typename Fn>
constexpr void for_each_class_from(Fn&& fn);        // fn(std::type_identity<T>{})

template <typename T> constexpr std::string_view namespace_of();
template <typename T> consteval bool             can_be_named();
```

**`for_each_class_from<"model.hpp", ...>(fn)`** calls `fn` for every class
declared in one of the given header files. The order is that of the
declarations, with the classes of a namespace kept together.

- Every namespace is searched, except `std` and namespaces whose name
  starts with two underscores. Classes nested in other classes are
  included, after the class around them.
- Class templates, classes that are only declared, unions and enums are not
  reported.
- A file name matches if it is the path the compiler reports for the
  declaration, or the end of that path, or the other way round, counted in
  whole directory and file names. `model.hpp` and `src/model.hpp` match
  `/project/src/model.hpp`; `data_model.hpp` does not. Paths are compared
  as text.
- Two headers with the same name in different directories are told apart
  only if enough of the path is given.

**`namespace_of<T>()`** is the namespace `T` is declared in:
`"shop::detail"`, or `""` for the global namespace. Classes around `T` are
not part of it.

**`can_be_named<T>()`** is false if code in another file cannot refer to
`T`: it is in an unnamed namespace or inside a function, or has no name.

### Writing code

```cpp
class source_writer {
public:
    explicit source_writer(std::ostream& out);

    template <typename... Parts>
    source_writer& line(const Parts&... parts);
    source_writer& blank();
    [[nodiscard]] indentation indented();
};

class namespace_scope {
public:
    namespace_scope(source_writer& out, std::string_view name);
    ~namespace_scope();
};
```

| | |
| --- | --- |
| `line(parts...)` | Writes the indentation, the parts in order, and a newline. A part is anything that can be written to a stream. |
| `blank()` | Writes an empty line. |
| `indented()` | Adds four spaces of indentation to every line written during the lifetime of the returned object. Can be nested. |
| `namespace_scope(out, "a::b")` | Writes `namespace a::b {` and an empty line, and at the end of its scope `}  // namespace a::b` and an empty line. Writes nothing for an empty name. |

### Context

```cpp
class context {
public:
    explicit context(std::ostream& errors);

    source_writer& out();
    void include(std::string_view header);

    template <typename T>   void error(std::string_view message);
    template <descriptor D> void error(D proxy, std::string_view message);
    bool failed() const;

    std::string body() const;
    const std::set<std::string>& includes() const;
};
```

A processor receives the context of the current run.

| | |
| --- | --- |
| `out()` | The writer for the generated code. |
| `include("<utility>")` | Asks for that `#include` at the top of the generated file. The argument carries its brackets or quotes. The includes are written sorted, each once. |
| `error<T>(message)` | Writes `file:line:column: error: message` to the error stream, with the place where `T` is declared, and marks the run as failed. |
| `error(proxy, message)` | The same, with the place where the member behind the proxy is declared. |
| `failed()` | Whether `error` has been called. |
| `body()`, `includes()` | What the processors produced so far. |

### Processors and running them

```cpp
template <typename Inputs, typename... Processors>
bool generate(std::ostream& out, std::ostream& errors);

template <typename Inputs, typename... Processors>
int run(int argc, char** argv);

struct builder_processor;     // for [[=reflect::generate_builder{}]]

template <typename Annotation = registered_t>
class registry_processor;     // for [[=reflect::registered]], or for Annotation
```

A **processor** is a class with

```cpp
template <typename T>
void process(reflect::codegen::context& context);     // may be static
```

and optionally a member type `annotation`. With it, `process` is called for
the classes of the input headers that carry an annotation of that type;
without it, for every class.

A processor may also have one or both of

```cpp
void begin(reflect::codegen::context& context);       // may be static
void end(reflect::codegen::context& context);         // may be static
```

| | |
| --- | --- |
| `begin` | Called once, before `process` is called for the first class. |
| `end` | Called once, after `process` was called for the last class. |

- One object of every processor is created for a run, with its default
  constructor, and all three functions are called on it. A processor can
  keep data between the calls.
- `begin` and `end` are called even if `process` is called for no class.
- A member named `begin` or `end` that cannot be called with a `context&`
  is a compile error, as is a processor without a default constructor.

**`generate<inputs<...>, Processors...>(out, errors)`** runs the processors
in the order given, each with its `begin`, all classes and its `end`, and
writes the generated file to `out`: a comment, `#pragma once`, an
`#include` for every input file under the name given in `inputs`, the
includes the processors asked for, and their code. If a processor reported an error, nothing is written to `out`
and the result is `false`.

**`run<inputs<...>, Processors...>(argc, argv)`** is the `main` function of
a generator program. It expects the path of the file to write as the only
argument.

| Result | Meaning |
| --- | --- |
| `0` | The file was written. |
| `1` | A processor reported an error, or the file could not be written. The errors are on standard error. |
| `2` | Wrong arguments. |

**`builder_processor`** is described in
[The builder processor](code-generation.md#the-builder-processor).

**`registry_processor<Annotation>`** writes the definition of
`reflect::registry<Annotation>` for the classes that carry the annotation,
and asks for `#include <reflect/reflect.hpp>`. It reports an error for a
class in an unnamed namespace or inside a function. The annotation itself
has to be a class with a name that generated code can use; otherwise:
compile error. See
[The registry processor](code-generation.md#the-registry-processor).

### CMake functions

```cmake
reflect_process(<target>
    HEADERS <header>...
    PROCESSORS <class>...
    [PROCESSOR_HEADERS <header>...]
    [OUTPUT <file>])

reflect_generate(<target>
    GENERATOR <source>...
    OUTPUT <file>
    [DEPENDS <file>...])
```

`reflect_process` writes a generator program that calls `run` with the
given headers and processors, and passes it to `reflect_generate`. See
[Add the step to the build](code-generation.md#add-the-step-to-the-build).

`reflect_generate` builds a generator program from the given sources, runs
it with the path of the output file before the target is compiled, and adds
the output to the target. See [CMake](code-generation.md#cmake).

## Field table

```cpp
struct field_info {
    std::string_view name;
    std::string_view type_name;
    std::size_t      index;
    std::size_t      offset;
    std::size_t      size;
    bool             is_public;
};

template <typename T, access_context Access = default_access>
constexpr auto field_table();   // std::array<field_info, N>
```

| Member | Meaning |
| --- | --- |
| `name` | The member name; empty for an unnamed member. |
| `type_name` | The type as the compiler prints it. For display only. |
| `index` | Position in the table, which is the position in `get_fields_of<T, Access>()`. |
| `offset` | Bytes from the start of the object. |
| `size` | `sizeof` the member; `0` for a bit-field. |
| `is_public` | Whether the member is public. |

`field_table<T>()` returns one entry per field. It is a constant expression;
store it in a `static constexpr` variable to use it at run time.

## Strings

```cpp
struct static_string {
    const char* data;
    std::size_t size;

    consteval static_string(const char* text);
    consteval static_string(std::string_view text);

    constexpr std::string_view view() const;
    constexpr const char*      c_str() const;
    constexpr operator std::string_view() const;

    friend constexpr bool operator==(static_string, static_string);
    template <typename Traits>
    friend std::basic_ostream<char, Traits>& operator<<(
        std::basic_ostream<char, Traits>&, static_string);
};

template <std::size_t N>
struct fixed_string {
    char text[N];

    constexpr fixed_string(const char (&literal)[N]);
    constexpr std::string_view view() const;
};
```

**`static_string`** is text that can be a member of an annotation.
`std::string` and `std::string_view` cannot. It is created from a string
literal, or from any `std::string_view` that is known at compile time:

```cpp
struct Rename {
    reflect::static_string name;
};

struct User {
    [[=Rename{"mail"}]] std::string email;
};
```

The characters are stored in static storage and are zero terminated.

**`fixed_string`** makes a string literal usable as a template argument. It
is the type behind `get_field_of<User, "id">()`. You rarely name it; you
need it when you write your own function that takes a name the same way:

```cpp
template <typename T, reflect::fixed_string Name>
constexpr bool has_member_named();
```

## List type aliases

For code that needs to name the type a discovery function returns.

```cpp
template <typename T, access_context Access = default_access> using fields_of_t        = /* list<field, ...> */;
template <typename T, access_context Access = default_access> using all_fields_of_t    = /* list<field, ...> */;
template <typename T, access_context Access = default_access> using static_fields_of_t = /* list<static_field, ...> */;
template <typename T, access_context Access = default_access> using functions_of_t     = /* list<function, ...> */;
template <typename T, fixed_string Name>                      using functions_named_t  = /* list<function, ...> */;
template <typename T, access_context Access = default_access> using constructors_of_t  = /* list<constructor, ...> */;
template <typename T, access_context Access = default_access> using members_of_t       = /* list<member, ...> */;
template <typename T, access_context Access = default_access> using bases_of_t         = /* list<base, ...> */;
template <typename E>                                         using enumerators_of_t   = /* list<enumerator, ...> */;
template <std::meta::info Target>                             using annotations_of_t   = /* list<annotation, ...> */;
```

`fields_of_t<User>` is the type of `get_fields_of<User>()`, and so on. The
members of a list are static, so they can be used on the type:
`reflect::fields_of_t<User>::size()`.

## Compile-time errors

The library reports misuse while compiling. The message appears after
`'what()':` in the compiler output.

| Message | Cause |
| --- | --- |
| `reflect needs C++26 static reflection. With GCC 16 or newer compile with: -std=c++26 -freflection` | The flags are missing or the compiler does not support reflection. |
| `reflect::get_field_of: no field with that name` | `get_field_of<T, "name">()`, and `T` declares no such field. |
| `reflect::get_field_of: the pointer does not name a field declared by that class` | `get_field_of<Pointer>()` with a pointer that matches no field. |
| `reflect::field_index_of: no field with that name` | `field_index_of<T, "name">()`, and `T` declares no such field. |
| `reflect::get_function_of: no function with that name` | `get_function_of<T, "name">()`, and `T` declares no such function. |
| `reflect::get_function_of: the name is overloaded; add a signature, ...` | `get_function_of<T, "name">()` for an overloaded name. |
| `reflect::get_function_of: no overload with that signature` | The signature matches none of the overloads. |
| `reflect::get_function_of: the third argument has to be a function type such as int(int) const` | The signature argument is not a function type. |
| `reflect::get_function_of: the pointer does not name a member function declared by that class` | `get_function_of<Pointer>()` with a pointer that matches no function. |
| `reflect::get_constructor_of: no constructor with these parameter types` | `get_constructor_of<T, Parameters...>()`, and `T` has no constructor with exactly these parameters. |
| `reflect::construct: the class needs exactly one public constructor that takes arguments; ...` | `construct<T>(fn)` on a class with no such constructor, or with several. Pass a constructor from `get_constructor_of` as the first argument. |
| `reflect::constructor::invoke: only a public constructor can be called` | `invoke` or `construct` with a private or protected constructor. |
| `reflect::walk: the callback does not accept the type of this field, and the field is not a class that could be walked into; add an overload for the type` | `walk` reached a field the callback cannot be called with, and that is not a class. |
| `reflect::get_annotation: expected exactly one annotation of this type` | `get_annotation` with no annotation of that type, or several. |
| `reflect::find_annotation: more than one annotation of this type` | `find_annotation` or `get_annotation_or` with several. |
| `reflect::get_inherited_annotation: neither the class nor a base class has an annotation of this type` | `get_inherited_annotation` found the annotation nowhere. |
| `reflect::get_inherited_annotation: more than one annotation of this type on the nearest class that has one` | The class that answers has the annotation several times. `find_inherited_annotation` reports the same under its own name. |
| `reflect::builder::set: no field with that name` | `set<"name">` on a builder, and neither the class nor a base class has that field. |
| `reflect::builder::set: this field was already set` | Two `set` calls for one field in the same chain. |
| `reflect::builder::set: the value cannot be assigned to this field` | The argument of `set` does not convert to the type of the field. |
| `reflect::builder::build: the required field '...' was not set` | `build()` before every `[[=reflect::required]]` field was set. The message names the field. |
| `reflect::builder: the class needs a default constructor` | `builder<T>` for a `T` that cannot be default constructed. |
| `reflect::invoke: no member function with that name` | `invoke<"name">` on a class without such a function. |
| `reflect::for_each_registered: there is no registry for this annotation here; include the header that reflect::codegen::registry_processor generated for it` | `for_each_registered` or `create_registered` in a source file that does not include the generated header, or for an annotation without a registry. |
| `reflect::create_registered: the base class needs a virtual destructor` | `create_registered<Base>` could create a class derived from `Base`, and `Base` has no virtual destructor. |

[Troubleshooting](troubleshooting.md) lists these and the errors that come
from the compiler itself, with examples and fixes.

## Version macros

```cpp
#define REFLECT_VERSION_MAJOR 0
#define REFLECT_VERSION_MINOR 1
#define REFLECT_VERSION_PATCH 0
```
