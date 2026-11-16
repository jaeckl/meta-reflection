#include <reflect/reflect.hpp>

#include "check.hpp"

#include <map>
#include <string>

struct Tag {};

struct Widget {
    Widget() = default;
    explicit Widget(int v) : value(v) {}
    ~Widget() = default;

    [[=Tag{}]] int value = 0;
    static int count;

    int get() const { return value; }
    static int make() { return 1; }
    bool operator==(const Widget&) const = default;
    explicit operator bool() const { return value != 0; }

    struct Nested {};
    using alias = int;
    template <typename T>
    void generic(T) {}

private:
    int hidden = 0;
};
int Widget::count = 0;

using reflect::member_kind;

template <typename T, reflect::access_context Access = reflect::default_access>
constexpr int count_kind(member_kind kind) {
    int count = 0;
    reflect::for_each_member<T, Access>([&](auto member) {
        if (member.kind() == kind)
            ++count;
    });
    return count;
}

static_assert(count_kind<Widget>(member_kind::field) == 2);
static_assert(count_kind<Widget, reflect::public_access>(member_kind::field) == 1);
static_assert(count_kind<Widget>(member_kind::static_field) == 1);
static_assert(count_kind<Widget>(member_kind::function) == 1);
static_assert(count_kind<Widget>(member_kind::static_function) == 1);
static_assert(count_kind<Widget>(member_kind::destructor) == 1);
static_assert(count_kind<Widget>(member_kind::operator_function) >= 1);
static_assert(count_kind<Widget>(member_kind::conversion_function) == 1);
static_assert(count_kind<Widget>(member_kind::type) == 2);
static_assert(count_kind<Widget>(member_kind::template_) == 1);
// The two written ones plus the implicit copy and move constructors.
static_assert(count_kind<Widget>(member_kind::constructor) >= 2);

// The raw view is a superset of the filtered ones.
static_assert(reflect::get_members_of<Widget>().size() >
              reflect::get_fields_of<Widget>().size() +
                  reflect::get_functions_of<Widget>().size());

template <typename T, reflect::fixed_string Name>
constexpr bool has_member_named() {
    bool found = false;
    reflect::for_each_member<T>([&](auto member) {
        if constexpr (member.has_name())
            found = found || member.name() == Name.view();
    });
    return found;
}
static_assert(has_member_named<Widget, "value">());
static_assert(has_member_named<Widget, "Nested">());
static_assert(has_member_named<Widget, "generic">());
static_assert(!has_member_named<Widget, "nothing">());

int main() {
    Widget widget{7};

    // Narrow a raw member down to a typed proxy.
    int sum = 0;
    int calls = 0;
    reflect::for_each_member<Widget>([&](auto member) {
        if constexpr (member.is_field()) {
            sum += member.as_field().get(widget);
        } else if constexpr (member.kind() == member_kind::function) {
            calls += member.as_function().invoke(widget);
        } else if constexpr (member.kind() == member_kind::static_function) {
            calls += member.as_function().invoke();
        }
    });
    CHECK(sum == 7);
    CHECK(calls == 7 + 1);

    // Annotation queries are guarded: asking a member template or a
    // constructor is fine and simply answers "no".
    int tagged = 0;
    reflect::for_each_member<Widget>([&](auto member) {
        if constexpr (reflect::has_annotation<Tag>(member))
            ++tagged;
    });
    CHECK(tagged == 1);

    // display_name works for everything, name only where there is one.
    std::map<bool, int> named;
    reflect::for_each_member<Widget>([&](auto member) {
        CHECK(!member.display_name().empty());
        ++named[member.has_name()];
    });
    CHECK(named[true] >= 8);
    CHECK(named[false] >= 4);

    int accessible = 0;
    reflect::for_each_member<Widget, reflect::public_access>([&](auto member) {
        CHECK(member.is_public());
        CHECK(!member.is_private() && !member.is_protected());
        ++accessible;
    });
    CHECK(accessible + 1 == int(reflect::get_members_of<Widget>().size()));

    return test::result();
}
