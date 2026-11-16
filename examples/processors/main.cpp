// Uses code that the build generated from the annotations in model.hpp:
// builder classes, from a processor that comes with the library, and
// operator<<, from the processor in print_processor.hpp.

#include "generated.hpp"

#include <iostream>
#include <stdexcept>

int main() {
    shop::User user = shop::UserBuilder().with_id(7).with_name("Ada").build();
    shop::Order order = shop::OrderBuilder().set_number(1001).set_total(59.9).build();

    std::cout << user << '\n';
    std::cout << shop::Receipt{order.number, true} << '\n';

    // The generated builder checks the required fields when build() runs.
    try {
        shop::UserBuilder().with_id(8).build();
    } catch (const std::logic_error& error) {
        std::cout << error.what() << '\n';
    }
}
