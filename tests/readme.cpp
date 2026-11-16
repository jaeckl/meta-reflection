// The code shown in README.md, compiled and checked.

#include <reflect/reflect.hpp>

#include "check.hpp"

#include <iostream>
#include <sstream>
#include <string>

namespace first {

struct User {
    int id;
    std::string name;
    int age;
};

std::string run() {
    std::ostringstream cout;
    User user{1, "Ada", 36};

    reflect::for_each_field(user, [&](auto field, const auto& value) {
        cout << field.name() << " = " << value << '\n';
    });
    return cout.str();
}

}  // namespace first

namespace second {

struct Skip {};
struct Key {
    reflect::static_string name;
};

struct User {
    int id;
    [[=Key{"displayName"}]] std::string name;
    [[=Skip{}]] std::string password;
};

template <typename T>
void write_json(std::ostream& out, const T& object) {
    out << '{';
    bool first = true;
    reflect::for_each_field(object, [&](auto field, const auto& value) {
        if constexpr (reflect::has_annotation<Skip>(field))
            return;

        std::string_view key = field.name();
        if constexpr (reflect::has_annotation<Key>(field))
            key = reflect::get_annotation<Key>(field).name;

        out << (first ? "" : ",") << '"' << key << "\":" << value;
        first = false;
    });
    out << '}';
}

}  // namespace second

int main() {
    CHECK(first::run() == "id = 1\nname = Ada\nage = 36\n");

    std::ostringstream out;
    second::write_json(out, second::User{7, "Ada", "hunter2"});
    CHECK(out.str() == R"({"id":7,"displayName":Ada})");

    return test::result();
}
