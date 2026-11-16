# Coming from another language or library

If you have used reflection elsewhere, this page maps what you know to
`reflect`. Find your background, read the table, and continue with the
[Getting Started Guide](getting-started.md).

- [The one big difference](#the-one-big-difference)
- [Java](#java)
- [C#](#c)
- [Python](#python)
- [Go](#go)
- [Rust](#rust)
- [C++ without reflection](#c-without-reflection)
  - [Boost.PFR](#boostpfr)
  - [magic_enum](#magic_enum)
  - [Registration macros and code generators](#registration-macros-and-code-generators)
- [New to C++ templates?](#new-to-c-templates)

## The one big difference

In Java, C#, Python and Go, reflection happens while the program runs. In
C++ it happens while the program is compiled.

What you gain:

- **Mistakes are compile errors.** A misspelled field name does not compile.
- **No run-time cost.** Looping over the fields of a struct produces the
  same kind of code as accessing each field by hand.
- **Full type information.** Inside a loop over fields, each value has its
  real type, not `Object`.

What works differently:

- **No type lookup by name.** There is no `Class.forName("User")`. The types
  you reflect on are named in the source code.
- **Loops are templates.** The body of a field loop is compiled once per
  field, so the callback takes `auto` parameters and uses `if constexpr` to
  treat types differently.
- **Member names for lookup are literals.** `get_field_of<User, "name">()`
  is resolved by the compiler. For a name from a file or a request, use
  `visit_field`, which checks the name against every field at run time.

## Java

```java
for (Field field : user.getClass().getDeclaredFields()) {
    field.setAccessible(true);
    System.out.println(field.getName() + " = " + field.get(user));
}
```

```cpp
reflect::for_each_field(user, [](auto field, const auto& value) {
    std::cout << field.name() << " = " << value << '\n';
});
```

| Java | reflect |
| --- | --- |
| `clazz.getDeclaredFields()` | `reflect::get_fields_of<T>()` |
| `clazz.getFields()` (public only) | `reflect::get_fields_of<T, reflect::public_access>()` |
| `field.getName()` | `field.name()` |
| `field.getType()` | `decltype(field)::type` |
| `field.get(obj)` | `field.get(obj)` |
| `field.set(obj, value)` | `field.set(obj, value)` |
| `field.setAccessible(true)` | not needed; private members are listed by default |
| `Modifier.isPrivate(field.getModifiers())` | `field.is_private()` |
| `clazz.getDeclaredField("name")` | `reflect::get_field_of<T, "name">()` |
| `clazz.getDeclaredMethods()` | `reflect::get_functions_of<T>()` |
| `method.invoke(obj, args)` | `function.invoke(obj, args...)` |
| `method.getParameterTypes()` | `decltype(function)::parameter_type<I>` |
| `clazz.getDeclaredConstructors()` | `reflect::get_constructors_of<T>()` |
| `clazz.getConstructor(String.class, int.class)` | `reflect::get_constructor_of<T, std::string, int>()` |
| `constructor.newInstance(args)` | `constructor.invoke(args...)` |
| `clazz.getSuperclass()`, `getInterfaces()` | `reflect::get_bases_of<T>()` |
| `clazz.getSimpleName()` | `reflect::name_of<T>()` |
| `@interface Range { int min(); int max(); }` | `struct Range { int min; int max; };` |
| `@Range(min = 0, max = 150) int age;` | `[[=Range{0, 150}]] int age;` |
| `field.isAnnotationPresent(Range.class)` | `reflect::has_annotation<Range>(field)` |
| `field.getAnnotation(Range.class)` | `reflect::get_annotation<Range>(field)` |
| `clazz.isAnnotationPresent(A.class)` with an `@Inherited` annotation | `reflect::has_inherited_annotation<A, T>()` |
| `color.name()` | `reflect::enum_name(color)` |
| `Color.valueOf("Red")` | `reflect::enum_cast<Color>("Red")` (returns an optional instead of throwing) |
| `Color.values()` | `reflect::enum_values<Color>()` |

Annotations need no retention policy and no target declaration. An
annotation is any struct, and it can be put on a type, a field, a function,
a parameter, a base class or an enumerator. There is no `@Inherited`
either: the code that reads the annotation chooses between
`has_annotation` and `has_inherited_annotation`.

## C#

| C# | reflect |
| --- | --- |
| `typeof(T).GetFields()` | `reflect::get_fields_of<T, reflect::public_access>()` |
| `GetFields(BindingFlags.NonPublic \| BindingFlags.Public \| BindingFlags.Instance)` | `reflect::get_fields_of<T>()` |
| `field.Name` | `field.name()` |
| `field.FieldType` | `decltype(field)::type` |
| `field.GetValue(obj)` | `field.get(obj)` |
| `field.SetValue(obj, value)` | `field.set(obj, value)` |
| `typeof(T).GetMethods()` | `reflect::get_functions_of<T>()` |
| `method.Invoke(obj, args)` | `function.invoke(obj, args...)` |
| `typeof(T).GetConstructors()` | `reflect::get_constructors_of<T, reflect::public_access>()` |
| `typeof(T).GetConstructor(new[] { typeof(string), typeof(int) })` | `reflect::get_constructor_of<T, std::string, int>()` |
| `constructor.Invoke(args)` | `constructor.invoke(args...)` |
| `typeof(T).BaseType` | `reflect::get_bases_of<T>()` |
| `typeof(T).Name`, `nameof(T)` | `reflect::name_of<T>()` |
| `class RangeAttribute : Attribute` | `struct Range { int min; int max; };` |
| `[Range(0, 150)] public int Age;` | `[[=Range{0, 150}]] int age;` |
| `Attribute.IsDefined(field, typeof(RangeAttribute))` | `reflect::has_annotation<Range>(field)` |
| `field.GetCustomAttribute<RangeAttribute>()` | `reflect::find_annotation<Range>(field)` |
| `field.GetCustomAttributes<AliasAttribute>()` | `reflect::get_annotations<Alias>(field)` |
| `typeof(T).GetCustomAttribute<TableAttribute>(inherit: true)` | `reflect::find_inherited_annotation<Table, T>()` |
| `color.ToString()`, `Enum.GetName` | `reflect::enum_name(color)` |
| `Enum.TryParse("Red", out Color c)` | `reflect::enum_cast<Color>("Red")` |
| `Enum.GetValues<Color>()` | `reflect::enum_values<Color>()` |

C# properties have no direct counterpart. C++ classes have data members
(fields) and member functions; the library lists both.

If you know C# source generators, the result here is similar, with the
generator being an ordinary function template in your own code.

## Python

```python
for f in dataclasses.fields(user):
    print(f.name, "=", getattr(user, f.name))
```

```cpp
reflect::for_each_field(user, [](auto field, const auto& value) {
    std::cout << field.name() << " = " << value << '\n';
});
```

| Python | reflect |
| --- | --- |
| `dataclasses.fields(obj)`, `vars(obj)` | `reflect::for_each_field(obj, fn)` |
| `getattr(obj, "name")` with a literal | `reflect::get_field_of<T, "name">().get(obj)` |
| `getattr(obj, name)` with a variable | `reflect::visit_field(obj, name, fn)` |
| `setattr(obj, "name", value)` | `reflect::get_field_of<T, "name">().set(obj, value)` |
| `hasattr(T, "name")` | `reflect::has_field<T, "name">()` |
| `type(obj).__name__` | `reflect::name_of<T>()` |
| `dataclasses.astuple(obj)` | `reflect::tie(obj)` |
| `field(metadata={"min": 0})` | `[[=Range{0, 150}]]` |
| `Color.RED.name` | `reflect::enum_name(Color::Red)` |
| `Color["RED"]` | `reflect::enum_cast<Color>("Red")` |
| `list(Color)` | `reflect::enum_values<Color>()` |
| `getattr(obj, "method")(args)` | `reflect::invoke<"method">(obj, args...)` |
| `@dataclass(eq=True, order=True)` | `reflect::tie(a) == reflect::tie(b)`, `reflect::tie(a) < reflect::tie(b)` |

A Python function can receive a value of any type and inspect it. The C++
counterpart is a function template, `template <typename T> void f(const T&)`,
which the compiler writes out separately for every type it is called with.

Members cannot be added to an object at run time, so there is no counterpart
to assigning a new attribute.

## Go

| Go | reflect |
| --- | --- |
| `reflect.TypeOf(v).NumField()` | `reflect::get_fields_of<T>().size()` |
| `t.Field(i).Name` | `reflect::get_fields_of<T>().get<I>().name()` |
| `t.Field(i).Type` | `decltype(field)::type` |
| `reflect.ValueOf(v).Field(i)` | `field.get(v)` |
| `v.FieldByName(name)` | `reflect::visit_field(v, name, fn)` |
| `` `json:"name,omitempty"` `` struct tags | annotations: `[[=Key{"name"}]]` |
| `t.Field(i).Tag.Get("json")` | `reflect::get_annotation<Key>(field)` |
| `v.MethodByName("Name").Call(args)` | `reflect::invoke<"Name">(v, args...)` |
| unexported fields cannot be set | private members can be read and written |

Struct tags are strings that each library parses for itself. An annotation
is a typed value: `Range{0, 150}` has two `int` members, and a typo in it is
a compile error.

## Rust

Rust has no run-time reflection either, and solves the same problems with
derive macros. The C++26 answer is different: instead of a macro that
generates an implementation for each type, you write *one* function template
that works for every type.

| Rust | reflect |
| --- | --- |
| `#[derive(Debug)]` | a `print` function template using `for_each_field` ([example](../examples/print_struct.cpp)) |
| `#[derive(PartialEq, PartialOrd)]` | `reflect::tie(a) == reflect::tie(b)`, `reflect::tie(a) < reflect::tie(b)` |
| `#[derive(Serialize)]` | a `write` function template ([example](../examples/json.cpp)) |
| `#[serde(rename = "mail")]` | `[[=Rename{"mail"}]]`, read with `reflect::get_annotation` |
| `#[serde(skip)]` | `[[=Skip{}]]`, checked with `reflect::has_annotation` |
| strum `Display`, `EnumString`, `EnumIter` | `reflect::enum_name`, `reflect::enum_cast`, `reflect::enum_values` |
| writing a proc macro | writing a function template |

There is no separate macro crate and no token stream to parse. The
"generator" is normal code, type checked and debuggable like the rest of the
program.

## C++ without reflection

### Boost.PFR

| Boost.PFR | reflect |
| --- | --- |
| `boost::pfr::for_each_field(obj, fn)` | `reflect::for_each_field(obj, fn)` |
| `boost::pfr::get<I>(obj)` | `reflect::get_fields_of<T>().get<I>().get(obj)` |
| `boost::pfr::tuple_size_v<T>` | `reflect::get_fields_of<T>().size()` |
| `boost::pfr::structure_tie(obj)` | `reflect::tie(obj)` |
| `boost::pfr::get_name<I, T>()` | `reflect::get_fields_of<T>().get<I>().name()` |
| `boost::pfr::eq(a, b)` | `reflect::tie(a) == reflect::tie(b)` |

What changes:

- The callback of `for_each_field` gets the field first and the value
  second, so the name and the annotations are at hand.
- Any class works, not just simple aggregates: classes with constructors,
  private members and base classes included.
- Annotations, member functions, base classes and enums are covered too.

### magic_enum

| magic_enum | reflect |
| --- | --- |
| `magic_enum::enum_name(value)` | `reflect::enum_name(value)` |
| `magic_enum::enum_cast<E>(text)` | `reflect::enum_cast<E>(text)` |
| `magic_enum::enum_values<E>()` | `reflect::enum_values<E>()` |
| `magic_enum::enum_names<E>()` | `reflect::enum_names<E>()` |
| `magic_enum::enum_count<E>()` | `reflect::enum_count<E>()` |

The functions were named to match. Since the enumerators come from the
compiler and not from probing a range of numbers, there is no limit on the
enumerator values.

### Registration macros and code generators

Libraries built before C++26 needed each type to be described a second
time: a macro listing the members, a registration call, or a build step that
generates code. With reflection in the language, that second description
goes away.

```cpp
struct User {
    int id;
    std::string name;
};
// Nothing else. No macro, no registration, no generated file.
```

Where the old description carried extra information, such as a JSON key that
differs from the member name, an annotation on the member takes its place.

## New to C++ templates?

You need three pieces of syntax to use the library comfortably.

**A generic lambda.** `auto` in the parameter list means "any type":

```cpp
[](auto field, const auto& value) { /* ... */ }
```

**`if constexpr`.** An `if` that the compiler decides. The branch that is
not taken is not compiled for that case:

```cpp
if constexpr (std::is_same_v<typename decltype(field)::type, int>)
    value += 1;
```

**`decltype(x)::type`.** "The typedef named `type` inside the type of `x`."
For a field this is the type of the member. Inside a lambda with `auto`
parameters, C++ requires the word `typename` in front:

```cpp
using Type = typename decltype(field)::type;
```

The [Getting Started Guide](getting-started.md) uses nothing beyond these.
