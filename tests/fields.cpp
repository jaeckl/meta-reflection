#include <reflect/reflect.hpp>

#include "check.hpp"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

struct Skip {};
struct Column {
    int id;
    constexpr bool operator==(const Column&) const = default;
};
struct Rename {
    reflect::static_string value;
};

struct Entity {
    int entity_id = 100;
};
struct Audited {
    long created_at = 0;
};

struct User : Entity, Audited {
    [[=Column{7}]] int id = 1;
    [[=Rename{"display name"}, =Column{8}]] std::string name = "Ada";
    [[=Skip{}]] std::string password = "pw";
    static int instances;

    int age() const { return 36; }
    void probe_access() const;

private:
    int secret = 42;
};
int User::instances = 0;

// --- discovery ---------------------------------------------------------------

static_assert(reflect::get_fields_of<User>().size() == 4);
static_assert(!reflect::get_fields_of<User>().empty());
static_assert(reflect::get_fields_of<User>().names()[1] == "name");
static_assert(reflect::get_fields_of<User>().get<3>().name() == "secret");
static_assert(reflect::get_fields_of<Skip>().empty());

// Inherited fields come first.
static_assert(reflect::get_all_fields_of<User>().size() == 6);
static_assert(reflect::get_all_fields_of<User>().names()[0] == "entity_id");
static_assert(reflect::get_all_fields_of<User>().names()[1] == "created_at");
static_assert(reflect::get_all_fields_of<User>().names()[2] == "id");

// --- access policies ---------------------------------------------------------

static_assert(reflect::get_fields_of<User, reflect::unchecked_access>().size() == 4);
static_assert(reflect::get_fields_of<User, reflect::public_access>().size() == 3);
static_assert(reflect::get_fields_of<User, reflect::current_access()>().size() == 3);

void User::probe_access() const {
    // From inside the class the caller's own access includes private members.
    static_assert(reflect::get_fields_of<User, reflect::current_access()>().size() == 4);
}

// --- one field ---------------------------------------------------------------

using Id = decltype(reflect::get_field_of<User, "id">());
using Name = decltype(reflect::get_field_of<User, "name">());
using Secret = decltype(reflect::get_field_of<User, "secret">());

static_assert(std::same_as<Id, reflect::field<^^User::id>>);
static_assert(std::same_as<Id::type, int>);
static_assert(std::same_as<Name::type, std::string>);
static_assert(std::same_as<Id::declaring_type, User>);
static_assert(Id::name() == "id" && Id::has_name());
static_assert(Id::is_public() && !Id::is_private() && !Id::is_protected());
static_assert(Secret::is_private());
static_assert(Id::has_default_initializer());
static_assert(!Id::is_bit_field() && !Id::is_mutable());
static_assert(Id::pointer() == &User::id);
static_assert(std::meta::is_nonstatic_data_member(Id::native()));

// --- annotations on fields ---------------------------------------------------

static_assert(reflect::has_annotation<Column>(Id{}));
static_assert(!reflect::has_annotation<Skip>(Id{}));
static_assert(reflect::get_annotation<Column>(Id{}) == Column{7});
static_assert(Id::get_annotation<Column>().id == 7);
static_assert(reflect::find_annotation<Rename>(Name{})->value == "display name");
static_assert(!reflect::find_annotation<Rename>(Id{}).has_value());
static_assert(reflect::get_annotations<Column>(Name{}).size() == 1);
static_assert(reflect::get_annotations_of(Name{}).size() == 2);
static_assert(Name::annotations().size() == 2);

static_assert(reflect::get_fields_of<User>().with<Column>().size() == 2);
static_assert(reflect::get_fields_of<User>().without<Skip>().size() == 3);
static_assert(reflect::get_fields_of<User>().with<Column>().without<Rename>().size() == 1);
static_assert(reflect::get_fields_of<User>().with<Column>().names()[1] == "name");

// --- unusual members ---------------------------------------------------------

struct Flags {
    unsigned ready : 1 = 1;
    unsigned level : 3 = 5;
    int& ref;
    const int fixed = 9;
    mutable int cache = 0;
    union {
        int as_int;
        float as_float;
    };
};

static_assert(reflect::get_fields_of<Flags>().size() == 6);
static_assert(reflect::get_field_of<Flags, "ready">().is_bit_field());
static_assert(reflect::get_field_of<Flags, "cache">().is_mutable());
static_assert(std::same_as<decltype(reflect::get_field_of<Flags, "ref">())::type, int&>);
static_assert(std::same_as<decltype(reflect::get_field_of<Flags, "fixed">())::type, const int>);
static_assert(!reflect::get_fields_of<Flags>().get<5>().has_name());

struct Point {
    int x = 1;
    int y = 2;
    int z = 3;
};
static_assert(reflect::get_field_of<Point, "y">().offset() == offsetof(Point, y));

// --- class templates, unions, virtual bases ----------------------------------

template <typename T>
struct Box {
    T value;
};
static_assert(reflect::get_fields_of<Box<int>>().size() == 1);
static_assert(reflect::get_fields_of<std::pair<int, long>>().names()[1] == "second");

union Number {
    int as_int;
    float as_float;
};
static_assert(reflect::get_fields_of<Number>().size() == 2);

struct Root {
    int root = 2;
};
struct Left : virtual Root {
    int left = 3;
};
struct Right : virtual Root {
    int right = 4;
};
struct Diamond : Left, Right {
    int own = 5;
};
static_assert(reflect::get_all_fields_of<Diamond>().size() == 4);
static_assert(reflect::get_all_fields_of<Diamond>().names()[0] == "root");
static_assert(reflect::get_all_fields_of<Diamond>().names()[3] == "own");

class Hidden {
    int hidden_value = 4;
};
struct Derived : Entity, private Hidden {
    int own = 5;
};
static_assert(reflect::get_all_fields_of<Derived>().size() == 3);
static_assert(reflect::get_all_fields_of<Derived, reflect::public_access>().size() == 2);

// --- constant evaluation -----------------------------------------------------

constexpr int sum(const Point& point) {
    int total = 0;
    reflect::for_each_field(point, [&](auto, int value) { total += value; });
    return total;
}
static_assert(sum(Point{}) == 6);

template <typename T, reflect::access_context Access>
constexpr int count_fields() {
    int count = 0;
    reflect::for_each_field<T, Access>([&](auto) { ++count; });
    return count;
}
static_assert(count_fields<User, reflect::unchecked_access>() == 4);
static_assert(count_fields<User, reflect::public_access>() == 3);

int main() {
    User user;
    const User constant;

    // get follows the constness and value category of the object.
    static_assert(std::same_as<decltype(Id::get(user)), int&>);
    static_assert(std::same_as<decltype(Id::get(constant)), const int&>);
    static_assert(std::same_as<decltype(Id::get(User{})), int&&>);

    CHECK(Id::get(user) == 1);
    Id::set(user, 99);
    CHECK(user.id == 99);
    Id::get(user) = 5;
    CHECK(user.*Id::pointer() == 5);

    // The access policy limits discovery only; a proxy reaches private data.
    CHECK(Secret::get(user) == 42);

    std::vector<std::string> seen;
    reflect::for_each_field<User>([&](auto field) {
        if constexpr (reflect::has_annotation<Skip>(field))
            return;
        seen.emplace_back(field.name());
    });
    CHECK((seen == std::vector<std::string>{"id", "name", "secret"}));

    seen.clear();
    reflect::for_each_field<reflect::public_access>(
        user, [&](auto field, const auto&) { seen.emplace_back(field.name()); });
    CHECK((seen == std::vector<std::string>{"id", "name", "password"}));

    reflect::for_each_field(user, [](auto field, auto& value) {
        if constexpr (std::same_as<typename decltype(field)::type, std::string>)
            value += "!";
    });
    CHECK(user.name == "Ada!" && user.password == "pw!");

    seen.clear();
    template for (auto field : reflect::get_fields_of<User>().with<Column>()) {
        constexpr Column column = reflect::get_annotation<Column>(field);
        seen.push_back(std::string(field.name()) + ":" + std::to_string(column.id));
    }
    CHECK((seen == std::vector<std::string>{"id:7", "name:8"}));

    // Inherited fields are read through the derived object.
    Derived derived;
    int total = 0;
    reflect::get_all_fields_of<Derived>().for_each(
        [&](auto field) { total += field.get(derived); });
    CHECK(total == 100 + 4 + 5);

    // Bit-fields come back by value, and can still be set.
    int target = 1;
    Flags flags{.ref = target, .as_int = 0};
    static_assert(std::same_as<
        decltype(reflect::get_field_of<Flags, "level">().get(flags)), unsigned>);
    CHECK(reflect::get_field_of<Flags, "level">().get(flags) == 5);
    reflect::get_field_of<Flags, "level">().set(flags, 2u);
    CHECK(flags.level == 2);
    reflect::get_field_of<Flags, "ref">().set(flags, 77);
    CHECK(target == 77);

    // names() gives a plain array for ordinary loops.
    std::string joined;
    for (std::string_view name : reflect::get_fields_of<Point>().names())
        joined += name;
    CHECK(joined == "xyz");

    return test::result();
}
