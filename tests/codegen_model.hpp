// Input header for tests/codegen.cpp. The line numbers matter to nobody;
// the names and annotations do.

#pragma once

#include <reflect/reflect.hpp>

#include <string>

struct Printable {};

struct [[=Printable{}]] TopLevel {
    int a = 0;
};

namespace shop {

struct Entity {
    [[=reflect::required]] long id = 0;
};

struct [[=reflect::generate_builder{}, =Printable{}]] User : Entity {
    [[=reflect::required]] std::string name;
    int age = 18;
    const int fixed = 1;      // cannot be assigned: no setter
    int flag : 1 = 0;         // bit-field: no setter

private:
    int secret = 0;           // private: no setter
};

struct Plain {
    int x = 0;
};

namespace detail {
struct [[=reflect::generate_builder{"set_"}]] Deep {
    double depth = 0;
};
}  // namespace detail

inline namespace v2 {
struct [[=Printable{}]] Versioned {};
}  // namespace v2

struct Outer {
    struct [[=Printable{}]] Nested {};
};

template <typename T>
struct [[=Printable{}]] Box {
    T value;
};

struct Declared;

}  // namespace shop
