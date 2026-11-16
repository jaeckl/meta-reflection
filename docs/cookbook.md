# Cookbook

Worked solutions to common problems. Each recipe shows the part that
matters; the complete, runnable program is in [examples/](../examples).

The [Getting Started Guide](getting-started.md) covers the individual
building blocks. This page shows how they combine.

- [Print any struct](#print-any-struct)
- [Write JSON](#write-json)
- [Enum to text and back](#enum-to-text-and-back)
- [Support std::format](#support-stdformat)
- [Parse command line options](#parse-command-line-options)
- [Validate with rules on the fields](#validate-with-rules-on-the-fields)
- [Generate SQL](#generate-sql)
- [Compare, diff and sort](#compare-diff-and-sort)
- [Use a struct as a hash map key](#use-a-struct-as-a-hash-map-key)
- [Call a function by a run-time name](#call-a-function-by-a-run-time-name)
- [Check the memory layout of a struct](#check-the-memory-layout-of-a-struct)
- [Work with class hierarchies](#work-with-class-hierarchies)
- [Patterns that come up again and again](#patterns-that-come-up-again-and-again)

## Print any struct

**Problem.** You want to see the contents of a struct while debugging,
without writing a print function for each one.

```cpp
template <typename T>
void print(const T& object) {
    std::cout << reflect::name_of<T>() << " {\n";
    reflect::for_each_field(object, [](auto field, const auto& value) {
        std::cout << "  " << field.name() << " = " << value << '\n';
    });
    std::cout << "}\n";
}
```

```text
User {
  id = 1
  name = Ada
  balance = 12.5
}
```

Full program: [print_struct.cpp](../examples/print_struct.cpp)

This version needs every field to be printable with `<<`. The next recipe
shows how to handle nested structs, containers and enums.

## Write JSON

**Problem.** Serialize structs, including nested ones, with the option to
rename a key or leave a field out.

The annotations:

```cpp
struct Skip {};
struct Key {
    reflect::static_string name;
};
```

The types to serialize stay plain:

```cpp
struct Address {
    std::string city;
    [[=Key{"postalCode"}]] int zip;
};

struct User {
    int id;
    [[=Key{"displayName"}]] std::string name;
    [[=Skip{}]] std::string password;
    Role role;
    bool active;
    Address address;
    std::vector<std::string> tags;
};
```

Writing an object is a loop over its fields:

```cpp
template <typename T>
void write_object(std::ostream& out, const T& object) {
    out << '{';
    bool first = true;
    reflect::for_each_field(object, [&](auto field, const auto& value) {
        if constexpr (reflect::has_annotation<Skip>(field))
            return;

        std::string_view key = field.name();
        if constexpr (reflect::has_annotation<Key>(field))
            key = reflect::get_annotation<Key>(field).name;

        out << (first ? "" : ",") << '"' << key << "\":";
        write(out, value);
        first = false;
    });
    out << '}';
}
```

and `write` decides by type what a value looks like. Anything that is not a
number, an enum, a string or a container is treated as an object, which makes
nesting work:

```cpp
template <typename T>
void write(std::ostream& out, const T& value) {
    if constexpr (std::is_same_v<T, bool>) {
        out << (value ? "true" : "false");
    } else if constexpr (std::is_arithmetic_v<T>) {
        out << value;
    } else if constexpr (std::is_enum_v<T>) {
        out << '"' << reflect::enum_name(value) << '"';
    } else if constexpr (std::is_same_v<T, std::string>) {
        out << '"' << value << '"';  // escaping left out to keep this short
    } else if constexpr (requires { value.begin(); value.end(); }) {
        out << '[';
        bool first = true;
        for (const auto& element : value) {
            out << (first ? "" : ",");
            write(out, element);
            first = false;
        }
        out << ']';
    } else {
        write_object(out, value);
    }
}
```

```text
{"id":7,"displayName":"Ada","role":"Admin","active":true,"address":{"city":"Berlin","postalCode":10115},"tags":["a","b"]}
```

Full program: [json.cpp](../examples/json.cpp)

**Reading JSON** follows the same shape in the other direction. Use
[`reflect::make`](getting-started.md#build-an-object-from-input) to build
the object, or [`reflect::visit_field`](getting-started.md#find-a-field-by-a-run-time-name)
to assign each key as it is parsed.

## Enum to text and back

**Problem.** Print an enum value, and parse one from user input.

```cpp
enum class Color { Red, Green, Blue };

// Value to name.
std::cout << reflect::enum_name(Color::Green) << '\n';

// Name to value. The result is empty for an unknown name.
for (std::string text : {"Blue", "Purple"}) {
    if (auto color = reflect::enum_cast<Color>(text))
        std::cout << text << " -> " << static_cast<int>(*color) << '\n';
    else
        std::cout << text << " is not a Color\n";
}

// Every value, for a menu or a help text.
std::cout << reflect::name_of<Color>() << " has "
          << reflect::enum_count<Color>() << " values:";
for (Color color : reflect::enum_values<Color>())
    std::cout << ' ' << reflect::enum_name(color);
std::cout << '\n';
```

```text
Green
Blue -> 2
Purple is not a Color
Color has 3 values: Red Green Blue
```

Full program: [enum_names.cpp](../examples/enum_names.cpp)

**Display names.** If the text shown to users should differ from the
enumerator, annotate the enumerator and read the annotation with
`reflect::for_each_enumerator`:

```cpp
enum class Role {
    Guest,
    Admin [[=Label{"administrator"}]] = 5,
    Owner,
};
```

## Support std::format

**Problem.** Make your structs work with `std::format` without writing a
formatter for each.

One formatter, constrained with `reflect::annotated_with`, covers every type
that opts in:

```cpp
// Marks a type as printable through std::format.
struct Formattable {};

// Hides a field from the output.
struct Secret {};

template <reflect::annotated_with<Formattable> T>
struct std::formatter<T> {
    constexpr auto parse(std::format_parse_context& context) { return context.begin(); }

    auto format(const T& object, std::format_context& context) const {
        auto out = std::format_to(context.out(), "{}{{", reflect::name_of<T>());
        bool first = true;
        reflect::for_each_field(object, [&](auto field, const auto& value) {
            out = std::format_to(out, "{}{}=", first ? "" : ", ", field.name());
            if constexpr (reflect::has_annotation<Secret>(field))
                out = std::format_to(out, "***");
            else
                out = std::format_to(out, "{}", value);
            first = false;
        });
        return std::format_to(out, "}}");
    }
};
```

```cpp
struct [[=Formattable{}]] Point {
    int x;
    int y;
};

struct [[=Formattable{}]] Login {
    std::string user;
    [[=Secret{}]] std::string password;
    Point position;  // a formattable struct inside another one
};
```

```text
Point{x=3, y=4}
Login{user=ada, password=***, position=Point{x=1, y=2}}
moved to Point{x=3, y=4} at step 7
```

Full program: [format.cpp](../examples/format.cpp)

The opt-in annotation matters. A formatter for *every* class type would
clash with the formatters the standard library already has.

## Parse command line options

**Problem.** Options of the form `--name=value` should end up in a struct,
and the help text should stay in sync with it.

The struct is the single source of truth. Adding an option means adding a
member:

```cpp
struct Help {
    reflect::static_string text;
};

enum class Mode { Fast, Safe };

struct Options {
    [[=Help{"number of worker threads"}]] int jobs = 1;
    [[=Help{"file to write the result to"}]] std::string output = "out.txt";
    [[=Help{"Fast or Safe"}]] Mode mode = Mode::Safe;
    [[=Help{"print what is going on"}]] bool verbose = false;
};
```

The option name arrives at run time, so `visit_field` finds the member, and
a small `from_text` function converts the text to the type of that member:

```cpp
// The name is only known at run time; visit_field finds the member.
bool accepted = false;
bool known = reflect::visit_field(options, name, [&](auto, auto& value) {
    accepted = from_text(text, value);
});
if (!known) {
    std::cout << "unknown option --" << name << '\n';
    return false;
}
```

The help text is a loop over the same struct:

```cpp
template <typename T>
void print_help() {
    std::cout << "options:\n";
    reflect::for_each_field<T>([](auto field) {
        std::cout << "  --" << field.name();
        if constexpr (reflect::has_annotation<Help>(field))
            std::cout << "  " << reflect::get_annotation<Help>(field).text;
        std::cout << '\n';
    });
}
```

Output for `--jobs=8 --mode=Fast --verbose`, followed by two mistyped
options:

```text
options:
  --jobs  number of worker threads
  --output  file to write the result to
  --mode  Fast or Safe
  --verbose  print what is going on
values:
  jobs = 8
  output = out.txt
  mode = Fast
  verbose = true
unknown option --job
bad value for --mode: 'Quick'
```

Full program: [command_line.cpp](../examples/command_line.cpp)

The same structure works for configuration files and environment
variables: only the source of the name and value pairs changes.

## Validate with rules on the fields

**Problem.** Check that the members of a struct hold sensible values, with
the rules written next to the members.

A rule is an annotation that has a `check` function:

```cpp
struct Range {
    int min;
    int max;

    constexpr bool check(int value) const { return min <= value && value <= max; }
    std::string describe() const {
        return "must be between " + std::to_string(min) + " and " + std::to_string(max);
    }
};

struct Signup {
    [[=NotEmpty{}, =MaxLength{12}]] std::string username;
    [[=Range{13, 120}]] int age = 0;
    [[=Range{0, 5}]] int newsletters = 0;
    std::string comment;  // no rules
};
```

The validator runs every annotation of every field. It never mentions a
rule type, so new kinds of rules need no change here:

```cpp
template <typename T>
std::vector<std::string> validate(const T& object) {
    std::vector<std::string> errors;
    reflect::for_each_field(object, [&](auto field, const auto& value) {
        // `rule` is an annotation proxy; rule.value() is the Range, NotEmpty, ...
        reflect::for_each_annotation(field, [&](auto rule) {
            if (!rule.value().check(value))
                errors.push_back(std::string(field.name()) + " " +
                                 rule.value().describe());
        });
    });
    return errors;
}
```

```text
"ada": valid
"": invalid
  username must not be empty
  age must be between 13 and 120
"a_very_long_username": invalid
  username must be at most 12 characters
  newsletters must be between 0 and 5
```

Full program: [validate.cpp](../examples/validate.cpp)

If a field carries annotations that are not rules, select the rules
explicitly, for example with `reflect::for_each_annotation<Range>(field, ...)`,
or test `requires { rule.value().check(value); }` inside the loop.

## Generate SQL

**Problem.** Create `CREATE TABLE` and `INSERT` statements from the structs
that hold the rows.

```cpp
struct [[=Table{"users"}]] User {
    [[=PrimaryKey{}]] int id;
    [[=Column{"display_name"}]] std::string name;
    double balance;
    bool active;
    [[=Transient{}]] std::string session_token;
};
```

Three library features carry this recipe:

- `reflect::annotated_with<Table>` rejects types that have no table name.
- `.without<Transient>()` removes the fields that are not stored.
- `get_annotation_or` makes `Column` optional, with the member name as the
  fallback.

```cpp
// Only types that opted in with a Table annotation are accepted.
template <reflect::annotated_with<Table> T>
std::string create_table() {
    std::string sql = "CREATE TABLE ";
    sql += reflect::get_annotation<Table, T>().name.view();
    sql += " (";

    bool first = true;
    // without<Transient>() drops the fields that should not be stored.
    reflect::get_fields_of<T>().template without<Transient>().for_each([&](auto field) {
        // Column{} is optional; the member name is the fallback.
        Column column = reflect::get_annotation_or<Column>(field, Column{field.name()});

        sql += first ? "" : ", ";
        sql += column.name.view();
        sql += ' ';
        sql += sql_type<typename decltype(field)::type>();
        if constexpr (reflect::has_annotation<PrimaryKey>(field))
            sql += " PRIMARY KEY";
        first = false;
    });
    return sql + ");";
}
```

```text
CREATE TABLE users (id INTEGER PRIMARY KEY, display_name TEXT, balance REAL, active BOOLEAN);
INSERT INTO users (id, display_name, balance, active) VALUES (?, ?, ?, ?);
CREATE TABLE products (sku INTEGER PRIMARY KEY, title TEXT);
INSERT INTO products (sku, title) VALUES (?, ?);
```

Full program: [sql.cpp](../examples/sql.cpp)

Note `.template without<Transient>()`. Inside a template, C++ needs the word
`template` before a member template of a dependent type.
[Troubleshooting](troubleshooting.md#expected-primary-expression-before--token)
explains when.

## Compare, diff and sort

**Problem.** Equality, ordering and "what changed?" for a struct.

`reflect::tie` turns an object into a tuple of references, which gives
equality and ordering for free:

```cpp
template <typename T>
bool equal(const T& left, const T& right) {
    return reflect::tie(left) == reflect::tie(right);
}

template <typename T>
bool less(const T& left, const T& right) {
    return reflect::tie(left) < reflect::tie(right);
}
```

For a diff, walk both objects at once:

```cpp
template <typename T>
void print_changes(const T& before, const T& after) {
    reflect::for_each_field(before, after,
                            [](auto field, const auto& old_value, const auto& new_value) {
                                if (old_value != new_value)
                                    std::cout << "  " << field.name() << ": " << old_value
                                              << " -> " << new_value << '\n';
                            });
}
```

```text
equal to itself: true
equal after edit: false
changes:
  team: Compilers -> Runtime
  level: 3 -> 4
sorted:
  Ada, Compilers
  Ada, Runtime
  Grace, Compilers
```

Full program: [compare.cpp](../examples/compare.cpp)

Fields are compared in declaration order. If you need a different order or
want to ignore some fields, use the two-object loop with an annotation such
as `[[=NoCompare{}]]`.

## Use a struct as a hash map key

**Problem.** `std::unordered_map` needs a hash function and equality for
its key type.

```cpp
// Combines the hashes of all fields.
template <typename T>
std::size_t hash_fields(const T& object) {
    std::size_t seed = 0;
    reflect::for_each_field(object, [&](auto, const auto& value) {
        std::size_t hash = std::hash<std::remove_cvref_t<decltype(value)>>{}(value);
        seed ^= hash + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
    });
    return seed;
}
```

Wrapped into two function objects, this works for any struct whose fields
can be hashed and compared:

```cpp
std::unordered_set<Coordinate, FieldHash, FieldEqual> visited;
std::unordered_map<Route, int, FieldHash, FieldEqual> distance;
```

```text
2 coordinates
true false
2 routes, Berlin to Paris: 1054
```

Full program: [hash.cpp](../examples/hash.cpp)

## Call a function by a run-time name

**Problem.** A command arrives as text, from a console, a script or the
network, and the matching member function should run.

An annotation marks which functions may be called from outside:

```cpp
class Thermostat {
public:
    [[=Command{"current target temperature"}]] double target() const { return target_; }

    [[=Command{"set the target temperature"}]] void set_target(double degrees) {
        target_ = degrees;
    }

    [[=Command{"change the target by a number of steps of a given size"}]]
    double nudge(int steps, double step_size) {
        target_ += steps * step_size;
        return target_;
    }

    void recalibrate() {}  // no Command annotation: not callable by name
    // ...
};
```

The dispatcher compares the incoming name with each annotated function and
checks the number of arguments:

```cpp
// with<Command>() keeps only the functions that carry the annotation.
reflect::get_functions_of<Object>().template with<Command>().for_each(
    [&](auto function) {
        if (found || function.name() != words[0])
            return;
        found = true;
        if (words.size() - 1 != function.arity()) {
            std::cout << function.name() << " takes " << function.arity()
                      << " argument(s)\n";
            return;
        }
        call(object, function, words, std::make_index_sequence<function.arity()>{});
    });
```

`call` converts each word to the type of the parameter at that position,
which the function proxy provides as `parameter_type<I>`:

```cpp
// The type to parse argument I into: the parameter type without const and &.
template <typename Function, std::size_t I>
using argument_t = std::remove_cvref_t<typename Function::template parameter_type<I>>;

function.invoke(object, from_word<argument_t<Function, I>>(words[I + 1])...);
```

```text
commands:
  target()  current target temperature
  set_target(degrees)  set the target temperature
  nudge(steps, step_size)  change the target by a number of steps of a given size
  mode(name)  switch to a named mode
> target
20
> set_target 21.5
ok
> nudge 3 0.5
23
> mode eco
mode is now eco
> nudge 3
nudge takes 2 argument(s)
> recalibrate
unknown command recalibrate
```

Full program: [dispatch.cpp](../examples/dispatch.cpp)

The list of commands with their parameter names, at the top of the output,
comes from `reflect::for_each_parameter`.

## Check the memory layout of a struct

**Problem.** A struct has to match a file format or a network packet, and
you want to see where the compiler put each member.

`reflect::field_table` returns plain data, so this is an ordinary loop:

```cpp
static constexpr auto fields = reflect::field_table<T>();

for (const reflect::field_info& field : fields) {
    if (field.offset > end_of_previous) {
        std::cout << "  " << std::setw(6) << end_of_previous << "  "
                  << field.offset - end_of_previous << " byte(s) of padding\n";
        padding += field.offset - end_of_previous;
    }
    // ...
    end_of_previous = field.offset + field.size;
}
```

```text
Header (32 bytes, alignment 8)
       0  version    1 byte(s)
       1  3 byte(s) of padding
       4  length     4 byte(s)
       8  flags      2 byte(s)
      10  6 byte(s) of padding
      16  timestamp  8 byte(s)
      24  checksum   1 byte(s)
      25  7 byte(s) of padding
  padding in total: 16 byte(s)

PackedHeader (16 bytes, alignment 8)
       0  timestamp  8 byte(s)
       8  length     4 byte(s)
      12  flags      2 byte(s)
      14  version    1 byte(s)
      15  checksum   1 byte(s)
  padding in total: 0 byte(s)
```

The table is a constant, so the same check can run at compile time and stop
the build when someone reorders the members:

```cpp
template <typename T>
constexpr bool has_no_padding() {
    std::size_t used = 0;
    for (const reflect::field_info& field : reflect::field_table<T>())
        used += field.size;
    return used == sizeof(T);
}
static_assert(!has_no_padding<Header>());
static_assert(has_no_padding<PackedHeader>());
```

Full program: [layout.cpp](../examples/layout.cpp)

## Work with class hierarchies

**Problem.** A class has base classes, and you want the inherited members
as well.

`get_all_fields_of` lists the inherited fields first. `declaring_type` says
where each one comes from:

```cpp
reflect::get_all_fields_of<Document>().for_each([&](auto field) {
    using Owner = typename decltype(field)::declaring_type;
    std::cout << "  " << reflect::name_of<Owner>() << "::" << field.name() << " = "
              << field.get(document) << '\n';
});
```

To walk the hierarchy yourself, recurse over the base classes:

```cpp
// Prints a class and, indented below it, each of its base classes.
template <typename T>
void print_tree(int depth = 0) {
    std::cout << std::string(depth * 2, ' ') << reflect::name_of<T>() << " {";
    reflect::for_each_field<T>([](auto field) { std::cout << ' ' << field.name(); });
    std::cout << " }\n";

    reflect::for_each_base<T>([&](auto base) {
        // base::type is the base class; hand it back in to go one level up.
        print_tree<typename decltype(base)::type>(depth + 1);
    });
}
```

```text
all fields:
  Entity::id = 1
  Timestamped::created = 1700000000
  Timestamped::updated = 1700000500
  Document::title = Report
  Document::pages = 12
class tree:
Document { title pages }
  Entity { id }
  Timestamped { created updated }
```

Full program: [inheritance.cpp](../examples/inheritance.cpp)

## Patterns that come up again and again

**Dispatch on the field type.**

```cpp
using Type = typename decltype(field)::type;
if constexpr (std::is_enum_v<Type>) { /* ... */ }
else if constexpr (std::is_arithmetic_v<Type>) { /* ... */ }
else { /* ... */ }
```

**Recurse into nested structs.** Call your own function template from
inside the field loop. Let the last `else` branch be "treat it as an object".

**Opt in with an annotation.** Constrain the function with
`reflect::annotated_with<Marker>`, so that it only accepts types that asked
for it.

**Optional annotation with a default.**

```cpp
Column column = reflect::get_annotation_or<Column>(field, Column{field.name()});
```

**Select by annotation.**

```cpp
reflect::get_fields_of<T>().template without<Skip>().for_each(/* ... */);
```

**A run-time name.** Loop over the candidates and compare names, or let
`reflect::visit_field` do it for fields.

**Stop at the first failure.** Use `reflect::for_each_field_while` and
return `false`.
