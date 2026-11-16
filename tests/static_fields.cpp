#include <reflect/reflect.hpp>

#include "check.hpp"

#include <string>
#include <vector>

struct Tag {
    int id;
};

struct Counter {
    static int instances;
    static constexpr double pi = 3.14;
    [[=Tag{4}]] static inline std::string label = "counter";
    int value = 0;  // not static, must not show up

    static int total() { return instances; }

private:
    static inline long hidden = 1;
};
int Counter::instances = 0;

struct Plain {
    int a;
};

static_assert(reflect::get_static_fields_of<Counter>().size() == 4);
static_assert(reflect::get_static_fields_of<Counter, reflect::public_access>().size() == 3);
static_assert(reflect::get_static_fields_of<Plain>().empty());
static_assert(reflect::get_static_fields_of<Counter>().names()[1] == "pi");

// Static and non-static members are separate lists.
static_assert(reflect::get_fields_of<Counter>().size() == 1);

using Instances = decltype(reflect::get_static_fields_of<Counter>().get<0>());
using Pi = decltype(reflect::get_static_fields_of<Counter>().get<1>());
using Label = decltype(reflect::get_static_fields_of<Counter>().get<2>());
using Hidden = decltype(reflect::get_static_fields_of<Counter>().get<3>());

static_assert(std::same_as<Instances::type, int>);
static_assert(std::same_as<Pi::type, const double>);
static_assert(std::same_as<Instances::declaring_type, Counter>);
static_assert(Instances::name() == "instances");
static_assert(Instances::is_public() && Hidden::is_private());
static_assert(!Instances::is_protected());

// A constexpr static member is readable at compile time.
static_assert(Pi::get() == 3.14);

static_assert(reflect::has_annotation<Tag>(Label{}));
static_assert(reflect::get_annotation<Tag>(Label{}).id == 4);
static_assert(reflect::get_static_fields_of<Counter>().with<Tag>().size() == 1);

int main() {
    static_assert(std::same_as<decltype(Instances::get()), int&>);
    static_assert(std::same_as<decltype(Pi::get()), const double&>);

    Instances::set(10);
    CHECK(Counter::instances == 10);
    Instances::get() += 1;
    CHECK(Counter::total() == 11);
    *Instances::pointer() = 5;
    CHECK(Counter::instances == 5);
    CHECK(Instances::pointer() == &Counter::instances);

    Label::set("renamed");
    CHECK(Counter::label == "renamed");
    CHECK(Hidden::get() == 1);

    std::vector<std::string> names;
    reflect::for_each_static_field<Counter, reflect::public_access>(
        [&](auto field) { names.emplace_back(field.name()); });
    CHECK((names == std::vector<std::string>{"instances", "pi", "label"}));

    // Reachable from the raw member view too.
    int found = 0;
    reflect::for_each_member<Counter>([&](auto member) {
        if constexpr (member.kind() == reflect::member_kind::static_field) {
            CHECK(!member.as_static_field().name().empty());
            ++found;
        }
    });
    CHECK(found == 4);

    return test::result();
}
