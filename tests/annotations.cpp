#include <reflect/reflect.hpp>

#include "check.hpp"

#include <sstream>
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

// With a fallback.
static_assert(reflect::get_annotation_or<Limit, User>(Limit{9}).value == 5);
static_assert(reflect::get_annotation_or<Limit, Plain>(Limit{9}).value == 9);

struct Widths {
    [[=Limit{3}]] int narrow;
    int normal;
};
static_assert(reflect::get_annotation_or<Limit>(reflect::get_field_of<Widths, "narrow">(),
                                               Limit{10})
                  .value == 3);
static_assert(reflect::get_annotation_or<Limit>(reflect::get_field_of<Widths, "normal">(),
                                               Limit{10})
                  .value == 10);

// As a constraint.
static_assert(reflect::annotated_with<User, Serializable>);
static_assert(!reflect::annotated_with<Plain, Serializable>);
static_assert(!reflect::annotated_with<int, Serializable>);

template <reflect::annotated_with<Serializable> T>
constexpr int save(const T&) {
    return 1;
}
constexpr int save(...) {
    return 0;
}
static_assert(save(User{}) == 1);
static_assert(save(Plain{}) == 0);

// The untyped list.
static_assert(reflect::get_annotations_of<User>().size() == 5);
static_assert(reflect::get_annotations_of<Plain>().empty());
static_assert(reflect::get_annotations_of<User>().get<0>().is<Serializable>());
static_assert(reflect::get_annotations_of<User>().get<1>().value().id == 1);
static_assert(std::same_as<decltype(reflect::get_annotations_of<User>().get<3>())::type,
                           Table>);

// Annotations of base classes.
struct [[=Serializable{}, =Table{"entities"}, =Limit{1}]] Entity {};
struct [[=Table{"customers"}]] Customer : Entity {};
struct Vip : Customer {};

// A class does not have the annotations of its bases...
static_assert(!reflect::has_annotation<Serializable, Vip>());
static_assert(!reflect::annotated_with<Vip, Serializable>);
// ...unless asked for.
static_assert(reflect::has_inherited_annotation<Serializable, Vip>());
static_assert(reflect::has_inherited_annotation<Serializable, Entity>());
static_assert(!reflect::has_inherited_annotation<Skip, Vip>());
static_assert(!reflect::has_inherited_annotation<Serializable, Plain>());
static_assert(!reflect::has_inherited_annotation<Serializable, int>());

// The nearest class answers.
static_assert(reflect::get_inherited_annotation<Table, Vip>().name == "customers");
static_assert(reflect::get_inherited_annotation<Table, Customer>().name == "customers");
static_assert(reflect::get_inherited_annotation<Table, Entity>().name == "entities");
static_assert(reflect::get_inherited_annotation<Limit, Vip>().value == 1);

static_assert(reflect::find_inherited_annotation<Table, Vip>()->name == "customers");
static_assert(!reflect::find_inherited_annotation<Skip, Vip>().has_value());
static_assert(!reflect::find_inherited_annotation<Table, int>().has_value());
static_assert(reflect::get_inherited_annotation_or<Limit, Vip>(Limit{9}).value == 1);
static_assert(reflect::get_inherited_annotation_or<Limit, Plain>(Limit{9}).value == 9);

// Several bases: depth first, in declaration order.
struct [[=Tag{10}]] Root {};
struct Left : Root {};
struct [[=Tag{20}]] Right {};
struct Both : Left, Right {};
struct Swapped : Right, Left {};
static_assert(reflect::get_inherited_annotation<Tag, Both>().id == 10);
static_assert(reflect::get_inherited_annotation<Tag, Swapped>().id == 20);

// More than one on a base is fine as long as a nearer class decides.
struct [[=Tag{3}]] Narrowed : User {};
static_assert(reflect::get_inherited_annotation<Tag, Narrowed>().id == 3);

// The access policy decides which bases count.
struct [[=Tag{30}]] Hidden {};
struct Wrapper : private Hidden {};
static_assert(reflect::has_inherited_annotation<Tag, Wrapper>());
static_assert(!reflect::has_inherited_annotation<Tag, Wrapper, reflect::public_access>());
static_assert(reflect::get_inherited_annotation_or<Tag, Wrapper, reflect::public_access>(Tag{0})
                  .id == 0);

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

    std::ostringstream out;
    out << stored.name << '/' << std::string(stored.name);
    CHECK(out.str() == "users/users");

    return test::result();
}
