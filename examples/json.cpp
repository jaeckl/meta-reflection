// A JSON writer in about fifty lines.
//
// Shows the three things most serializers need:
//   - loop over the fields of any struct, including nested ones
//   - annotations that change the output (rename a key, leave a field out)
//   - enums written by name

#include <reflect/reflect.hpp>

#include <iostream>
#include <string>
#include <type_traits>
#include <vector>

// Annotations are ordinary types. Nothing has to be registered.
struct Skip {};
struct Key {
    reflect::static_string name;
};

template <typename T>
void write(std::ostream& out, const T& value);

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

// --- the types to serialize: plain structs ----------------------------------

enum class Role { Guest, Member, Admin };

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

int main() {
    User user{7, "Ada", "hunter2", Role::Admin, true, {"Berlin", 10115}, {"a", "b"}};
    write(std::cout, user);
    std::cout << '\n';
}
