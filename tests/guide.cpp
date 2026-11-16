// Every code snippet in docs/getting-started.md, in the same order.
//
// The guide is only useful if its code works, so the snippets live here,
// where they are compiled and run with the other tests. When you change the
// guide, change this file with it.

#include <reflect/reflect.hpp>

#include <algorithm>
#include <iostream>
#include <map>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

// --- the types the guide uses -------------------------------------------------

struct User {
    int id = 0;
    std::string name;
    int age = 0;
};

// Annotations are ordinary structs.
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

struct [[=Serializable{}]] Account {
    int id = 0;
    [[=Rename{"mail"}]] std::string email;
    [[=Range{0, 150}]] int age = 0;
    [[=Skip{}]] std::string password;
    [[=Alias{"nick"}, =Alias{"handle"}]] std::string nickname;
};

struct [[=Serializable{}, =Rename{"accounts"}]] StoredAccount {};
struct [[=Rename{"admins"}]] AdminAccount : StoredAccount {};
struct GuestAccount : StoredAccount {};

enum class Color { Red, Green, Blue };

class Counter {
public:
    int value() const { return value_; }
    void add(int amount) { value_ += amount; }
    void reset() { value_ = 0; }
    static int instances() { return 1; }

private:
    int value_ = 0;
};

struct Shape {
    double scale(int factor) { return factor * 1.0; }
    double scale(double factor) { return factor * 2.0; }
    double area() const { return 3.5; }
};

struct Entity {
    int id = 7;
};
struct Timestamps {
    long created = 100;
};
struct Document : Entity, Timestamps {
    std::string title = "Report";
};

class Vault {
public:
    int door = 1;

private:
    int code = 1234;
};

struct Settings {
    static inline int instances = 0;
    static constexpr int version = 3;
    int volume = 5;
};

struct Point {
    int x = 0;
    int y = 0;
};

struct Address {
    std::string city;
    int zip = 0;
};
struct Employee {
    std::string name;
    Address home;
    Address office;
};

class Connection {
public:
    Connection(std::string host, int port) : host_(std::move(host)), port_(port) {}

    const std::string& host() const { return host_; }
    int port() const { return port_; }

private:
    std::string host_;
    int port_;
};

// --- First program -------------------------------------------------------------

template <typename T>
void print(const T& object) {
    reflect::for_each_field(object, [](auto field, const auto& value) {
        std::cout << field.name() << " = " << value << '\n';
    });
}

void first_program() {
    print(User{1, "Ada", 36});
}

// --- Fields --------------------------------------------------------------------

void loop_over_fields_of_a_type() {
    reflect::for_each_field<User>([](auto field) {
        std::cout << field.name() << '\n';
    });
}

void loop_over_fields_of_an_object() {
    User user{1, "Ada", 36};

    reflect::for_each_field(user, [](auto field, auto& value) {
        std::cout << field.name() << " = " << value << '\n';
    });
}

void change_fields_in_a_loop() {
    User user{1, "Ada", 36};

    reflect::for_each_field(user, [](auto field, auto& value) {
        if constexpr (std::is_same_v<typename decltype(field)::type, int>)
            value += 1;  // only the int fields
    });

    std::cout << user.id << ' ' << user.age << '\n';
}

void get_one_field() {
    User user{1, "Ada", 36};

    auto name = reflect::get_field_of<User, "name">();

    std::cout << name.get(user) << '\n';  // read
    name.set(user, "Grace");              // write
    name.get(user) += " Hopper";          // get returns a reference
    std::cout << user.name << '\n';
}

void get_one_field_by_pointer() {
    User user{1, "Ada", 36};

    auto age = reflect::get_field_of<&User::age>();
    std::cout << age.name() << " = " << age.get(user) << '\n';
}

void check_for_a_field() {
    static_assert(reflect::has_field<User, "name">());
    static_assert(!reflect::has_field<User, "email">());

    std::cout << reflect::field_index_of<User, "age">() << '\n';
}

// Works for any type; uses the "id" field if there is one.
template <typename T>
int id_or_zero(const T& object) {
    if constexpr (reflect::has_field<T, "id">())
        return reflect::get_field_of<T, "id">().get(object);
    else
        return 0;
}

void field_properties() {
    auto field = reflect::get_field_of<User, "name">();

    std::cout << field.name() << '\n';
    std::cout << field.is_public() << '\n';
    std::cout << field.offset() << '\n';

    // The type is a member typedef.
    using Type = decltype(field)::type;
    static_assert(std::is_same_v<Type, std::string>);

    std::cout << id_or_zero(User{5, "Ada", 36}) << id_or_zero(Point{}) << '\n';
}

void inherited_fields() {
    Document document;

    // Only what Document declares itself.
    std::cout << reflect::get_fields_of<Document>().size() << '\n';

    // Base classes first, then Document.
    reflect::get_all_fields_of<Document>().for_each([&](auto field) {
        std::cout << field.name() << " = " << field.get(document) << '\n';
    });
}

void private_fields() {
    // By default every declared member is listed, private ones included.
    std::cout << reflect::get_fields_of<Vault>().size() << '\n';

    // Ask for the public ones only.
    std::cout << reflect::get_fields_of<Vault, reflect::public_access>().size() << '\n';

    Vault vault;
    reflect::for_each_field<reflect::public_access>(vault, [](auto field, auto& value) {
        std::cout << field.name() << " = " << value << '\n';
    });
}

// --- Annotations ---------------------------------------------------------------

void check_for_an_annotation() {
    // On a type: annotation first, then the type.
    static_assert(reflect::has_annotation<Serializable, Account>());
    static_assert(!reflect::has_annotation<Serializable, User>());

    // On a field: pass the field.
    reflect::for_each_field<Account>([](auto field) {
        if constexpr (reflect::has_annotation<Skip>(field))
            std::cout << field.name() << " is skipped\n";
    });
}

void read_an_annotation() {
    auto age = reflect::get_field_of<Account, "age">();

    constexpr Range range = reflect::get_annotation<Range>(age);
    std::cout << range.min << ".." << range.max << '\n';
}

void optional_annotations() {
    reflect::for_each_field<Account>([](auto field) {
        // find_annotation gives a std::optional.
        if (auto rename = reflect::find_annotation<Rename>(field))
            std::cout << field.name() << " -> " << rename->name << '\n';
    });

    // Or supply the value to use when the annotation is missing.
    auto id = reflect::get_field_of<Account, "id">();
    Range range = reflect::get_annotation_or<Range>(id, Range{0, 999});
    std::cout << range.max << '\n';
}

void repeated_annotations() {
    auto nickname = reflect::get_field_of<Account, "nickname">();

    for (const Alias& alias : reflect::get_annotations<Alias>(nickname))
        std::cout << alias.name << '\n';
}

void loop_over_annotations() {
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
}

void select_fields_by_annotation() {
    Account account{1, "ada@example.com", 36, "secret", "ada"};

    reflect::get_fields_of<Account>().without<Skip>().for_each([&](auto field) {
        std::cout << field.name() << " = " << field.get(account) << '\n';
    });

    std::cout << reflect::get_fields_of<Account>().with<Range>().size() << '\n';
}

// Accepts only types marked [[=Serializable{}]].
template <reflect::annotated_with<Serializable> T>
void save(const T&) {
    std::cout << "saving " << reflect::name_of<T>() << '\n';
}

void restrict_to_annotated_types() {
    save(Account{});
    // save(User{});   // does not compile: User is not Serializable
}

void annotations_of_base_classes() {
    std::cout << reflect::has_annotation<Serializable, GuestAccount>() << '\n';
    std::cout << reflect::has_inherited_annotation<Serializable, GuestAccount>() << '\n';

    std::cout << reflect::get_inherited_annotation<Rename, GuestAccount>().name << '\n';
    std::cout << reflect::get_inherited_annotation<Rename, AdminAccount>().name << '\n';
}

// --- Enums ---------------------------------------------------------------------

void enum_to_text() {
    std::cout << reflect::enum_name(Color::Green) << '\n';
}

void text_to_enum() {
    if (auto color = reflect::enum_cast<Color>("Blue"))
        std::cout << "found " << reflect::enum_name(*color) << '\n';

    if (!reflect::enum_cast<Color>("Purple"))
        std::cout << "Purple is not a Color\n";
}

void list_enum_values() {
    for (Color color : reflect::enum_values<Color>())
        std::cout << reflect::enum_name(color) << ' ';
    std::cout << '\n';

    for (std::string_view name : reflect::enum_names<Color>())
        std::cout << name << ' ';
    std::cout << '\n';

    std::cout << reflect::enum_count<Color>() << '\n';
}

// --- Member functions ----------------------------------------------------------

void list_functions() {
    reflect::for_each_function<Counter>([](auto function) {
        std::cout << function.name() << " takes " << function.arity() << " argument(s)";
        if constexpr (function.is_const())
            std::cout << ", const";
        if constexpr (function.is_static())
            std::cout << ", static";
        std::cout << '\n';
    });
}

void call_a_function() {
    Counter counter;

    auto add = reflect::get_function_of<Counter, "add">();
    add.invoke(counter, 5);

    auto value = reflect::get_function_of<&Counter::value>();
    std::cout << value.invoke(counter) << '\n';

    // A static function needs no object.
    std::cout << reflect::get_function_of<Counter, "instances">().invoke() << '\n';
}

void call_every_function_that_fits() {
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
}

void overloaded_functions() {
    Shape shape;

    // Every overload.
    std::cout << reflect::get_functions_of<Shape, "scale">().size() << '\n';

    // One overload, picked by its signature.
    auto by_double = reflect::get_function_of<Shape, "scale", double(double)>();
    std::cout << by_double.invoke(shape, 2) << '\n';

    // Or let the compiler pick, as in shape.scale(2).
    std::cout << reflect::invoke<"scale">(shape, 2) << '\n';
    std::cout << reflect::invoke<"scale">(shape, 2.0) << '\n';
}

void function_parameters() {
    auto add = reflect::get_function_of<Counter, "add">();

    reflect::for_each_parameter(add, [](auto parameter) {
        std::cout << parameter.index() << ": " << parameter.name() << '\n';
    });

    using First = decltype(reflect::get_parameter_of<0>(add))::type;
    static_assert(std::is_same_v<First, int>);
}

// --- Base classes --------------------------------------------------------------

void base_classes() {
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
}

// --- Static members ------------------------------------------------------------

void static_members() {
    reflect::for_each_static_field<Settings>([](auto field) {
        std::cout << field.name() << " = " << field.get() << '\n';
    });

    reflect::get_static_fields_of<Settings>().get<0>().set(2);
    std::cout << Settings::instances << '\n';
}

// --- Lists ---------------------------------------------------------------------

void lists() {
    auto fields = reflect::get_fields_of<User>();

    std::cout << fields.size() << '\n';
    std::cout << fields.get<1>().name() << '\n';

    for (std::string_view name : fields.names())
        std::cout << name << ' ';
    std::cout << '\n';
}

void list_questions() {
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
}

void template_for() {
    User user{1, "Ada", 36};

    template for (auto field : reflect::get_fields_of<User>()) {
        std::cout << field.name() << " = " << field.get(user) << '\n';
    }
}

// --- Whole objects -------------------------------------------------------------

void compare_objects() {
    Point a{1, 2};
    Point b{1, 3};

    std::cout << (reflect::tie(a) == reflect::tie(b)) << '\n';
    std::cout << (reflect::tie(a) < reflect::tie(b)) << '\n';
}

void two_objects() {
    User before{1, "Ada", 36};
    User after{1, "Ada", 37};

    reflect::for_each_field(before, after, [](auto field, const auto& a, const auto& b) {
        if (a != b)
            std::cout << field.name() << ": " << a << " -> " << b << '\n';
    });
}

void field_by_run_time_name() {
    User user{1, "Ada", 36};
    std::string wanted = "age";  // from a file, the network, the user

    bool found = reflect::visit_field(user, wanted, [](auto field, auto& value) {
        std::cout << field.name() << " = " << value << '\n';
    });
    std::cout << found << '\n';
}

void walk_nested_objects() {
    Employee employee{"Ada", {"London", 1000}, {"Paris", 7500}};

    reflect::walk(employee, reflect::overloaded{
        [](reflect::field_path path, auto, const std::string& value) {
            std::cout << path << " = \"" << value << "\"\n";
        },
        [](reflect::field_path path, auto, int value) {
            std::cout << path << " = " << value << '\n';
        },
    });
}

void build_an_object() {
    std::map<std::string, std::string> row{{"id", "7"}, {"name", "Ada"}, {"age", "36"}};

    User user = reflect::make<User>([&](auto field) -> typename decltype(field)::type {
        const std::string& text = row.at(std::string(field.name()));
        if constexpr (std::is_same_v<typename decltype(field)::type, int>)
            return std::stoi(text);
        else
            return text;
    });

    std::cout << user.id << ' ' << user.name << ' ' << user.age << '\n';
}

void construct_an_object() {
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
}

void pick_a_constructor() {
    auto constructor = reflect::get_constructor_of<Connection, std::string, int>();

    Connection local = constructor.invoke("localhost", 8080);
    std::cout << local.host() << ':' << local.port() << '\n';

    reflect::for_each_parameter(constructor, [](auto parameter) {
        std::cout << parameter.name() << '\n';
    });
}

void stop_early() {
    User user{1, "", 36};

    bool complete = reflect::for_each_field_while(user, [](auto field, const auto& value) {
        if (value == std::remove_cvref_t<decltype(value)>{}) {
            std::cout << field.name() << " is not set\n";
            return false;  // stop here
        }
        return true;
    });
    std::cout << complete << '\n';
}

// --- Plain data ----------------------------------------------------------------

void field_table() {
    static constexpr auto fields = reflect::field_table<Point>();

    for (const reflect::field_info& field : fields)
        std::cout << field.name << " at offset " << field.offset << ", " << field.size
                  << " bytes\n";
}

// --- Everything else -----------------------------------------------------------

void raw_members() {
    int constructors = 0;
    reflect::for_each_member<Counter>([&](auto member) {
        if constexpr (member.kind() == reflect::member_kind::constructor)
            ++constructors;
    });
    std::cout << constructors << " constructors\n";
}

void escape_hatch() {
    auto field = reflect::get_field_of<User, "name">();

    // native() is the std::meta::info behind the proxy.
    constexpr std::meta::info info = field.native();
    std::cout << std::meta::display_string_of(std::meta::type_of(info)) << '\n';
}

int main() {
    std::cout << std::boolalpha;
#define SECTION(name)                    \
    std::cout << "-- " #name "\n";       \
    name()
    SECTION(first_program);
    SECTION(loop_over_fields_of_a_type);
    SECTION(loop_over_fields_of_an_object);
    SECTION(change_fields_in_a_loop);
    SECTION(get_one_field);
    SECTION(get_one_field_by_pointer);
    SECTION(check_for_a_field);
    SECTION(field_properties);
    SECTION(inherited_fields);
    SECTION(private_fields);
    SECTION(check_for_an_annotation);
    SECTION(read_an_annotation);
    SECTION(optional_annotations);
    SECTION(repeated_annotations);
    SECTION(loop_over_annotations);
    SECTION(select_fields_by_annotation);
    SECTION(restrict_to_annotated_types);
    SECTION(annotations_of_base_classes);
    SECTION(enum_to_text);
    SECTION(text_to_enum);
    SECTION(list_enum_values);
    SECTION(list_functions);
    SECTION(call_a_function);
    SECTION(call_every_function_that_fits);
    SECTION(overloaded_functions);
    SECTION(function_parameters);
    SECTION(base_classes);
    SECTION(static_members);
    SECTION(lists);
    SECTION(list_questions);
    SECTION(template_for);
    SECTION(compare_objects);
    SECTION(two_objects);
    SECTION(field_by_run_time_name);
    SECTION(walk_nested_objects);
    SECTION(build_an_object);
    SECTION(construct_an_object);
    SECTION(pick_a_constructor);
    SECTION(stop_early);
    SECTION(field_table);
    SECTION(raw_members);
    SECTION(escape_hatch);
}
