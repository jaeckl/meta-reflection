// Uses builder classes that did not exist before the build started.
//
// builders.hpp is written by generate_builders.cpp while the project is
// built; see CMakeLists.txt in this directory.

#include "builders.hpp"
#include "model.hpp"

#include <iostream>

int main() {
    shop::User user = shop::UserBuilder()
                          .with_id(7)
                          .with_name("Ada")
                          .with_roles({"admin", "developer"})
                          .build();

    shop::Order order = shop::OrderBuilder().set_number(1001).set_total(59.9).build();

    std::cout << user.id << ' ' << user.name << ' ' << user.roles.size() << ' '
              << user.version << '\n';
    std::cout << order.number << ' ' << order.total << '\n';
}
