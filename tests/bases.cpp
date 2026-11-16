#include <reflect/reflect.hpp>

#include "check.hpp"

#include <string>
#include <vector>

struct Tag {
    int id;
};

struct Entity {
    int entity_id = 100;
};
struct Audited {
    long created_at = 7;
};
struct Secret {
    int key = 1;
};

struct User : Entity, [[=Tag{9}]] Audited, private Secret {
    int id = 1;
};

struct Root {
    int root = 2;
};
struct Left : virtual Root {};
struct Guarded : protected Entity {};

static_assert(reflect::get_bases_of<User>().size() == 3);
static_assert(reflect::get_bases_of<User, reflect::public_access>().size() == 2);
static_assert(reflect::get_bases_of<Entity>().empty());

using First = decltype(reflect::get_bases_of<User>().get<0>());
using Second = decltype(reflect::get_bases_of<User>().get<1>());
using Third = decltype(reflect::get_bases_of<User>().get<2>());

static_assert(std::same_as<First::type, Entity>);
static_assert(std::same_as<Second::type, Audited>);
static_assert(First::is_public() && !First::is_virtual());
static_assert(Third::is_private());
static_assert(reflect::get_bases_of<Guarded>().get<0>().is_protected());
static_assert(reflect::get_bases_of<Left>().get<0>().is_virtual());

// An annotation on a base lives on the base-specifier.
static_assert(reflect::has_annotation<Tag>(Second{}));
static_assert(reflect::get_annotation<Tag>(Second{}).id == 9);
static_assert(!reflect::has_annotation<Tag>(First{}));
static_assert(reflect::get_bases_of<User>().with<Tag>().size() == 1);

static_assert(reflect::name_of<User>() == "User");
static_assert(reflect::name_of<First::type>() == "Entity");

int main() {
    User user;
    const User constant;

    static_assert(std::same_as<decltype(First::get(user)), Entity&>);
    static_assert(std::same_as<decltype(First::get(constant)), const Entity&>);
    CHECK(First::get(user).entity_id == 100);
    First::get(user).entity_id = 5;
    CHECK(user.entity_id == 5);
    CHECK(Third::get(user).key == 1);

    // Walk one level up: every base, then that base's own fields.
    std::vector<std::string> seen;
    reflect::for_each_base<User>([&](auto base) {
        using Base = typename decltype(base)::type;
        reflect::for_each_field<Base>([&](auto field) {
            seen.push_back(std::string(reflect::name_of<Base>()) + "::" +
                           std::string(field.name()));
        });
    });
    CHECK((seen == std::vector<std::string>{"Entity::entity_id", "Audited::created_at",
                                            "Secret::key"}));

    // The display text of an unnamed type is compiler specific; it only has
    // to be there.
    CHECK(!reflect::name_of<int>().empty());

    return test::result();
}
