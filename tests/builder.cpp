#include <reflect/reflect.hpp>

#include "check.hpp"

#include <memory>
#include <string>
#include <type_traits>
#include <utility>

struct User {
    [[=reflect::required]] int id = 0;
    [[=reflect::required]] std::string name;
    int age = 18;
    std::string email = "none";
};

struct Point {
    int x = 0;
    int y = 0;
};

struct Entity {
    [[=reflect::required]] long id = 0;
};
struct Customer : Entity {
    std::string name;
};

class Account {
public:
    const std::string& owner() const { return owner_; }
    int balance() const { return balance_; }

private:
    std::string owner_;
    int balance_ = 0;
};

struct Handle {
    std::unique_ptr<int> resource;
    int tag = 0;
};

// Compile time.
constexpr Point origin_shifted = reflect::builder<Point>().set<"y">(4).set<"x">(3).build();
static_assert(origin_shifted.x == 3 && origin_shifted.y == 4);

// A builder with nothing set is a builder<T>; every set changes the type.
static_assert(std::is_same_v<decltype(reflect::builder<Point>().set<"x">(1)),
                             reflect::builder<Point, ^^Point::x>>);

// What can be built, as a requirement.
template <typename Builder>
concept buildable = requires(Builder builder) { std::move(builder).build(); };
static_assert(buildable<reflect::builder<Point>>);  // nothing is required

int main() {
    // The order of the calls is free; unset fields keep their defaults.
    User user = reflect::builder<User>().set<"name">("Ada").set<"id">(7).build();
    CHECK(user.id == 7);
    CHECK(user.name == "Ada");
    CHECK(user.age == 18);
    CHECK(user.email == "none");

    User full = reflect::builder<User>()
                    .set<"id">(1)
                    .set<"name">(std::string("Bob"))
                    .set<"age">(40)
                    .set<"email">("bob@example.org")
                    .build();
    CHECK(full.age == 40);
    CHECK(full.email == "bob@example.org");

    // Steps kept in variables. A builder can be copied and continued twice.
    auto base = reflect::builder<User>().set<"id">(5);
    User first = base.set<"name">("first").build();
    User second = base.set<"name">("second").set<"age">(1).build();
    CHECK(first.id == 5 && first.name == "first" && first.age == 18);
    CHECK(second.id == 5 && second.name == "second" && second.age == 1);

    // Fields of base classes, required ones included.
    Customer customer = reflect::builder<Customer>().set<"name">("Eve").set<"id">(9L).build();
    CHECK(customer.id == 9);
    CHECK(customer.name == "Eve");

    // Private fields.
    Account account =
        reflect::builder<Account>().set<"owner_">("Ada").set<"balance_">(250).build();
    CHECK(account.owner() == "Ada");
    CHECK(account.balance() == 250);

    // Move-only members and objects.
    Handle handle = reflect::builder<Handle>()
                        .set<"resource">(std::make_unique<int>(42))
                        .set<"tag">(3)
                        .build();
    CHECK(handle.resource && *handle.resource == 42);
    CHECK(handle.tag == 3);

    return test::result();
}
