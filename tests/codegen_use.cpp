// Compiles and uses the code the build generates for codegen_model.hpp.
// tests/codegen.cpp checks the text of the generated code; this checks that
// the compiler accepts it and that it does what it says.

#include "codegen_model_generated.hpp"

#include "check.hpp"

#include <stdexcept>
#include <string>

int main() {
    // An inherited field, a required field, an optional one.
    shop::User user = shop::UserBuilder().with_id(7).with_name("Ada").with_age(40).build();
    CHECK(user.id == 7);
    CHECK(user.name == "Ada");
    CHECK(user.age == 40);
    CHECK(user.fixed == 1);

    // Fields without a call keep their default.
    shop::User defaults = shop::UserBuilder().with_name("Bob").with_id(1).build();
    CHECK(defaults.age == 18);

    // A missing required field is reported when build() runs.
    std::string message;
    try {
        shop::UserBuilder().with_id(1).build();
    } catch (const std::logic_error& error) {
        message = error.what();
    }
    CHECK(message == "UserBuilder: the required field 'name' was not set");

    // A builder kept in a variable.
    shop::UserBuilder builder;
    builder.with_id(3);
    builder.with_name("Cy");
    shop::User kept = builder.build();
    CHECK(kept.id == 3 && kept.name == "Cy");

    // The prefix option, in a nested namespace.
    shop::detail::Deep deep = shop::detail::DeepBuilder().set_depth(2.5).build();
    CHECK(deep.depth == 2.5);

    return test::result();
}
