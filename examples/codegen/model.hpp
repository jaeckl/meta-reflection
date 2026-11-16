// The classes of the program. A class marked [[=Builder{...}]] gets a
// builder class written for it by generate_builders.cpp.

#pragma once

#include <reflect/reflect.hpp>

#include <string>
#include <vector>

namespace shop {

/// Asks for a generated builder. Every setter name starts with `prefix`.
struct Builder {
    reflect::static_string prefix;
};

/// A field the generated builder gets no setter for.
struct Fixed {};

struct [[=Builder{"with_"}]] User {
    int id = 0;
    std::string name;
    std::vector<std::string> roles;
    [[=Fixed{}]] int version = 1;
};

struct [[=Builder{"set_"}]] Order {
    long number = 0;
    double total = 0;
};

// Not marked: no builder.
struct Receipt {
    long order = 0;
};

}  // namespace shop
