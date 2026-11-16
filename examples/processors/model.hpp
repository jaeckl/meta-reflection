// The classes of the program. The annotations say which code to generate
// for them; no generator program has to be written.

#pragma once

#include "printable.hpp"

#include <reflect/reflect.hpp>

#include <string>
#include <vector>

namespace shop {

struct [[=reflect::generate_builder{}, =Printable{}]] User {
    [[=reflect::required]] int id = 0;
    [[=reflect::required]] std::string name;
    std::string email = "none";
};

struct [[=reflect::generate_builder{"set_"}]] Order {
    long number = 0;
    double total = 0;
};

struct [[=Printable{}]] Receipt {
    long order = 0;
    bool paid = false;
};

}  // namespace shop
