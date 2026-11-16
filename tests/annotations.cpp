#include <reflect/reflect.hpp>

#include "check.hpp"

#include <string>
#include <string_view>
#include <vector>

struct Serializable {};
struct Skip {};
struct Tag {
    int id;
};
struct Table {
    reflect::static_string name;
};
// No default constructor on purpose.
struct Limit {
    int value;
    constexpr explicit Limit(int v) : value(v) {}
};

struct [[=Serializable{}, =Tag{1}, =Tag{2}, =Table{"users"}, =Limit{5}]] User {};
struct Plain {};

// Presence.
static_assert(reflect::has_annotation<Serializable, User>());
static_assert(reflect::has_annotation<Tag, User>());
static_assert(!reflect::has_annotation<Skip, User>());
static_assert(!reflect::has_annotation<Serializable, Plain>());
static_assert(!reflect::has_annotation<Serializable, int>());

// Exactly one, at most one, all of them.
static_assert(reflect::get_annotation<Table, User>().name == "users");
static_assert(reflect::get_annotation<Limit, User>().value == 5);
static_assert(reflect::find_annotation<Table, User>().has_value());
static_assert(!reflect::find_annotation<Skip, User>().has_value());
static_assert(reflect::get_annotations<Tag, User>().size() == 2);
static_assert(reflect::get_annotations<Tag, User>()[0].id == 1);
static_assert(reflect::get_annotations<Tag, User>()[1].id == 2);
static_assert(reflect::get_annotations<Skip, User>().empty());
static_assert(reflect::get_annotations<Limit, User>()[0].value == 5);

// The untyped list.
static_assert(reflect::get_annotations_of<User>().size() == 5);
static_assert(reflect::get_annotations_of<Plain>().empty());
static_assert(reflect::get_annotations_of<User>().get<0>().is<Serializable>());
static_assert(reflect::get_annotations_of<User>().get<1>().value().id == 1);
static_assert(std::same_as<decltype(reflect::get_annotations_of<User>().get<3>())::type,
                           Table>);

// static_string.
static_assert(reflect::static_string("abc").view() == "abc");
static_assert(reflect::static_string("abc").size == 3);
static_assert(std::string_view(reflect::static_string("abc").c_str()) == "abc");

int main() {
    std::vector<int> tags;
    reflect::for_each_annotation<Tag, User>([&](Tag tag) { tags.push_back(tag.id); });
    CHECK((tags == std::vector<int>{1, 2}));

    int seen = 0;
    std::string table;
    reflect::for_each_annotation<User>([&](auto annotation) {
        ++seen;
        if constexpr (annotation.template is<Table>())
            table = annotation.value().name.view();
    });
    CHECK(seen == 5);
    CHECK(table == "users");

    int walked = 0;
    template for (auto annotation : reflect::get_annotations_of<User>()) {
        if constexpr (annotation.template is<Tag>())
            walked += annotation.value().id;
    }
    CHECK(walked == 3);

    // An annotation value survives into run time as an ordinary object.
    Table stored = reflect::get_annotation<Table, User>();
    CHECK(std::string(stored.name.c_str()) == "users");

    return test::result();
}
