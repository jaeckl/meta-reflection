#include <reflect/reflect.hpp>

#include "check.hpp"

#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

struct Entity {};
struct Global {};

namespace shop {

struct [[=Entity{}]] User {
    int id;
};
struct Plain {};
struct Declared;                       // incomplete: not reported
template <typename T> struct Box {};   // a template: not reported
using Alias = User;                    // an alias: not reported a second time
enum class Color { Red };              // not a class
union Either {                         // not a class
    int i;
    float f;
};
int counter = 0;
void function();

namespace inner {
struct [[=Entity{}]] Order {
    struct Line {};
};
}  // namespace inner

}  // namespace shop

namespace empty {}

namespace {
struct Hidden {};
}  // namespace
namespace shop {
namespace {
struct Secret {};
}  // namespace
}  // namespace shop

template <typename T>
constexpr std::string_view local_name() {
    struct Local {};
    return reflect::qualified_name_of<Local>();
}

// Names.
static_assert(reflect::qualified_name_of<Global>() == "Global");
static_assert(reflect::qualified_name_of<shop::User>() == "shop::User");
static_assert(reflect::qualified_name_of<shop::inner::Order>() == "shop::inner::Order");
static_assert(reflect::qualified_name_of<shop::inner::Order::Line>() ==
              "shop::inner::Order::Line");
static_assert(reflect::qualified_name_of<shop::Alias>() == "shop::User");
static_assert(reflect::qualified_name_of<shop::Color>() == "shop::Color");
static_assert(reflect::name_of<shop::User>() == "User");

// The proxy says the same, and it is not limited to classes.
static_assert(std::is_same_v<reflect::type_info<^^shop::User>::type, shop::User>);
static_assert(reflect::type_info<^^shop::User>::name() == "User");
static_assert(reflect::type_info<^^shop::User>::qualified_name() == "shop::User");
static_assert(reflect::type_info<^^shop::User>::template has_annotation<Entity>());
static_assert(!reflect::type_info<^^shop::Plain>::template has_annotation<Entity>());
static_assert(reflect::type_info<^^shop::Color>::name() == "Color");
static_assert(reflect::type_info<^^shop::Color>::qualified_name() == "shop::Color");
static_assert(reflect::type_info<^^shop::Either>::name() == "Either");
static_assert(!reflect::type_info<^^int>::name().empty());
// A scope that cannot be named from outside ends the name.
static_assert(reflect::qualified_name_of<Hidden>() == "Hidden");
static_assert(reflect::qualified_name_of<shop::Secret>() == "Secret");
static_assert(local_name<int>() == "Local");
// No identifier: the compiler's text.
static_assert(!reflect::qualified_name_of<int>().empty());
static_assert(!reflect::qualified_name_of<shop::Box<int>>().empty());

// Usable at compile time.
constexpr int count_classes() {
    int count = 0;
    reflect::for_each_class_in<^^shop>([&](auto) { ++count; });
    return count;
}
static_assert(count_classes() == 2);

int main() {
    std::vector<std::string_view> all;
    std::vector<std::string_view> entities;
    reflect::for_each_class_in<^^shop>([&](auto cls) {
        all.push_back(cls.name());
        if constexpr (cls.template has_annotation<Entity>())
            entities.push_back(cls.qualified_name());
    });
    CHECK((all == std::vector<std::string_view>{"User", "Plain"}));
    CHECK((entities == std::vector<std::string_view>{"shop::User"}));

    // Nested namespaces are separate.
    std::vector<std::string_view> inner;
    reflect::for_each_class_in<^^shop::inner>([&](auto cls) {
        inner.push_back(cls.qualified_name());
    });
    CHECK((inner == std::vector<std::string_view>{"shop::inner::Order"}));

    int none = 0;
    reflect::for_each_class_in<^^empty>([&](auto) { ++none; });
    CHECK(none == 0);

    // The global namespace works too.
    bool found = false;
    reflect::for_each_class_in<^^::>([&](auto cls) {
        found = found || std::is_same_v<typename decltype(cls)::type, Global>;
    });
    CHECK(found);

    return test::result();
}
