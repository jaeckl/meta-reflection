#include <reflect/reflect.hpp>

#include "check.hpp"

#include <string>
#include <tuple>

struct Column {
    int id;
};

struct Base {
    int base_value = 1;
};

struct User : Base {
    [[=Column{7}]] int id = 1;
    int other = 2;  // same type as id: the pointer value has to decide
    std::string name = "Ada";
    const double ratio = 0.5;
    unsigned flag : 1 = 0;

private:
    int secret = 0;
};

// --- by pointer to member ----------------------------------------------------

static_assert(std::same_as<decltype(reflect::get_field_of<&User::id>()),
                           reflect::field<^^User::id>>);
static_assert(std::same_as<decltype(reflect::get_field_of<&User::other>()),
                           reflect::field<^^User::other>>);
static_assert(reflect::get_field_of<&User::name>().name() == "name");
static_assert(reflect::get_field_of<&User::ratio>().name() == "ratio");
static_assert(reflect::get_annotation<Column>(reflect::get_field_of<&User::id>()).id == 7);

// Both spellings give the same proxy type.
static_assert(std::same_as<decltype(reflect::get_field_of<&User::id>()),
                           decltype(reflect::get_field_of<User, "id">())>);

// &User::base_value has the type int Base::*, so it resolves against Base.
static_assert(std::same_as<decltype(reflect::get_field_of<&User::base_value>()),
                           reflect::field<^^Base::base_value>>);

// --- has_field ---------------------------------------------------------------

static_assert(reflect::has_field<User, "id">());
static_assert(reflect::has_field<User, "flag">());
static_assert(reflect::has_field<User, "secret">());
static_assert(!reflect::has_field<User, "ID">());
static_assert(!reflect::has_field<User, "">());
static_assert(!reflect::has_field<User, "base_value">());  // declared by Base
static_assert(reflect::has_field<Base, "base_value">());

// Usable to pick an implementation.
template <typename T>
constexpr int id_of(const T& object) {
    if constexpr (reflect::has_field<T, "id">())
        return reflect::get_field_of<T, "id">().get(object);
    else
        return -1;
}
static_assert(id_of(User{}) == 1);
static_assert(id_of(Base{}) == -1);

// ... or as a constraint.
template <typename T>
    requires(reflect::has_field<T, "name">())
constexpr bool has_name_field(const T&) {
    return true;
}
constexpr bool has_name_field(...) {
    return false;
}
static_assert(has_name_field(User{}));
static_assert(!has_name_field(Base{}));

// --- field_index_of ----------------------------------------------------------

static_assert(reflect::field_index_of<User, "id">() == 0);
static_assert(reflect::field_index_of<User, "name">() == 2);
static_assert(reflect::field_index_of<User, "secret">() == 5);
static_assert(reflect::get_fields_of<User>()
                  .get<reflect::field_index_of<User, "ratio">()>()
                  .name() == "ratio");

// --- field_types_t -----------------------------------------------------------

struct Point {
    int x;
    double y;
};
struct Mixed {
    const int fixed;
    int& ref;
    std::string text;
};
struct Nothing {};

static_assert(std::same_as<reflect::field_types_t<Point>, std::tuple<int, double>>);
static_assert(std::same_as<reflect::field_types_t<Mixed>,
                           std::tuple<const int, int&, std::string>>);
static_assert(std::same_as<reflect::field_types_t<Nothing>, std::tuple<>>);
static_assert(std::tuple_size_v<reflect::field_types_t<User>> == 6);
static_assert(std::tuple_size_v<reflect::field_types_t<User, reflect::public_access>> == 5);

int main() {
    User user;

    auto id = reflect::get_field_of<&User::id>();
    id.set(user, 42);
    CHECK(user.id == 42 && user.other == 2);

    reflect::get_field_of<&User::other>().get(user) = 9;
    CHECK(user.other == 9 && user.id == 42);

    CHECK(reflect::get_field_of<&User::base_value>().get(user) == 1);

    // A tuple with the same element types as the struct.
    reflect::field_types_t<Point> row{3, 4.5};
    Point point = std::make_from_tuple<Point>(row);
    CHECK(point.x == 3 && point.y == 4.5);

    return test::result();
}
