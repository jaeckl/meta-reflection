#include <reflect/reflect.hpp>

#include "check.hpp"

#include <string>

struct Label {
    reflect::static_string text;
};

enum class Role {
    Guest,
    Admin [[=Label{"administrator"}]] = 5,
    Owner,
};

enum Plain { One = 1, Two = 2, AlsoTwo = 2 };
enum class Empty {};
enum class Small : unsigned char { A, B };

static_assert(reflect::get_enumerators_of<Role>().size() == 3);
static_assert(reflect::get_enumerators_of<Empty>().empty());
static_assert(reflect::get_enumerators_of<Role>().names()[1] == "Admin");

using Admin = decltype(reflect::get_enumerators_of<Role>().get<1>());
static_assert(std::same_as<Admin::type, Role>);
static_assert(Admin::name() == "Admin");
static_assert(Admin::value() == Role::Admin);
static_assert(reflect::has_annotation<Label>(Admin{}));
static_assert(reflect::get_annotation<Label>(Admin{}).text == "administrator");
static_assert(reflect::get_enumerators_of<Role>().with<Label>().size() == 1);

static_assert(reflect::enum_count<Role>() == 3);
static_assert(reflect::enum_count<Empty>() == 0);
static_assert(reflect::enum_values<Role>()[2] == Role::Owner);
static_assert(reflect::enum_names<Role>()[0] == "Guest");
static_assert(reflect::enum_values<Small>().size() == 2);

static_assert(reflect::enum_name(Role::Owner) == "Owner");
static_assert(reflect::enum_name(Role{42}).empty());
static_assert(reflect::enum_name(Two) == "Two");
static_assert(reflect::enum_name(AlsoTwo) == "Two");
static_assert(reflect::enum_name(Small::B) == "B");

static_assert(reflect::enum_cast<Role>("Admin") == Role::Admin);
static_assert(!reflect::enum_cast<Role>("admin"));
static_assert(!reflect::enum_cast<Role>(""));
static_assert(reflect::enum_cast<Plain>("AlsoTwo") == Two);

static_assert(reflect::name_of<Role>() == "Role");

int main() {
    std::string joined;
    for (Role role : reflect::enum_values<Role>()) {
        joined += reflect::enum_name(role);
        joined += ' ';
    }
    CHECK(joined == "Guest Admin Owner ");

    // Values that only exist at run time work too.
    volatile int raw = 5;
    CHECK(reflect::enum_name(static_cast<Role>(raw)) == "Admin");

    std::string input = "Owner";
    CHECK(reflect::enum_cast<Role>(input) == Role::Owner);
    CHECK(reflect::enum_cast<Role>("Nobody") == std::nullopt);

    std::string labels;
    reflect::for_each_enumerator<Role>([&](auto enumerator) {
        if constexpr (reflect::has_annotation<Label>(enumerator))
            labels += reflect::get_annotation<Label>(enumerator).text.view();
        else
            labels += enumerator.name();
        labels += ' ';
    });
    CHECK(labels == "Guest administrator Owner ");

    return test::result();
}
