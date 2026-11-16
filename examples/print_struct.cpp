// Print any struct, field by field.
//
// The smallest useful reflect program: one loop, no macros, and nothing to
// register per type.

#include <reflect/reflect.hpp>

#include <iostream>
#include <string>

struct Address {
    std::string city;
    int zip;
};

struct User {
    int id;
    std::string name;
    double balance;
};

template <typename T>
void print(const T& object) {
    std::cout << reflect::name_of<T>() << " {\n";
    reflect::for_each_field(object, [](auto field, const auto& value) {
        std::cout << "  " << field.name() << " = " << value << '\n';
    });
    std::cout << "}\n";
}

int main() {
    print(User{1, "Ada", 12.5});
    print(Address{"Berlin", 10115});
}
