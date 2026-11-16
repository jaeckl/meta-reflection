#pragma once

// Experimental: a builder for any class, without generated code.

#include "access.hpp"
#include "config.hpp"
#include "detail/meta.hpp"
#include "field.hpp"
#include "string.hpp"

#include <concepts>
#include <initializer_list>
#include <meta>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace reflect {

/// Marks a field that a reflect::builder has to be given a value for:
///
///     [[=reflect::required]] std::string name;
struct required_t {};
inline constexpr required_t required{};

/// Asks the code generator for a builder class, UserBuilder for a class
/// User. Every setter is called `prefix` plus the name of its field:
///
///     struct [[=reflect::generate_builder{}]] User { ... };
///     struct [[=reflect::generate_builder{"set_"}]] Order { ... };
///
/// Read by reflect::codegen::builder_processor; without a generator run the
/// annotation does nothing.
struct generate_builder {
    static_string prefix = "with_";
};

namespace detail {

// The field called `name` in `type` or, if it declares none, in one of its
// base classes. The null reflection if there is none.
consteval std::meta::info find_field_in_hierarchy(std::meta::info type,
                                                  std::string_view name) {
    for (std::meta::info member :
         std::meta::nonstatic_data_members_of(type, unchecked_access)) {
        if (std::meta::has_identifier(member) &&
            std::meta::identifier_of(member) == name)
            return member;
    }
    for (std::meta::info base : std::meta::bases_of(type, unchecked_access)) {
        std::meta::info found =
            find_field_in_hierarchy(std::meta::type_of(base), name);
        if (found != std::meta::info{})
            return found;
    }
    return std::meta::info{};
}

consteval std::meta::info builder_field(std::meta::info type,
                                        std::string_view name) {
    std::meta::info found = find_field_in_hierarchy(type, name);
    if (found == std::meta::info{})
        throw std::meta::exception(
            "reflect::builder::set: no field with that name", type);
    return found;
}

// Throws, with the name of the field in the message, if a field marked
// [[=reflect::required]] is not among those that were set.
consteval bool check_required_fields(std::meta::info type,
                                     std::initializer_list<std::meta::info> set) {
    for (std::meta::info member : all_fields_of(type, unchecked_access)) {
        if (annotations(member, ^^required_t).empty())
            continue;
        bool was_set = false;
        for (std::meta::info given : set)
            was_set = was_set || given == member;
        if (!was_set)
            throw std::meta::exception(
                std::string("reflect::builder::build: the required field '") +
                    std::string(std::meta::identifier_of(member)) +
                    "' was not set",
                type);
    }
    return true;
}

}  // namespace detail

/// Builds a T step by step, one named field at a time:
///
///     User user = reflect::builder<User>()
///                     .set<"name">("Ada")
///                     .set<"id">(7)
///                     .build();
///
/// The names are checked while compiling. So is the rule that every field
/// marked [[=reflect::required]] has been set before build(), and that no
/// field is set twice. The price for those two checks is that every set()
/// returns a builder of a new type: keep the calls in one chain, or store
/// the steps in `auto` variables.
///
/// Fields that are not set keep the value a default-constructed T gives
/// them. T therefore needs a default constructor.
template <typename T, std::meta::info... Set>
class builder {
    static_assert(std::default_initializable<T>,
                  "reflect::builder: the class needs a default constructor");

public:
    constexpr builder() = default;

    /// Gives the field called Name a value. Fields of base classes count.
    template <fixed_string Name, typename Value>
    [[nodiscard]] constexpr auto set(this builder self, Value&& value) {
        constexpr std::meta::info target = detail::builder_field(^^T, Name.view());
        static_assert(!((Set == target) || ...),
                      "reflect::builder::set: this field was already set");
        static_assert(
            std::is_assignable_v<typename field<target>::type&, Value&&>,
            "reflect::builder::set: the value cannot be assigned to this field");
        field<target>::set(self.object_, std::forward<Value>(value));
        return builder<T, Set..., target>(std::move(self.object_));
    }

    /// The finished object. A compile error if a required field is missing.
    [[nodiscard]] constexpr T build(this builder self) {
        static_assert(detail::check_required_fields(^^T, {Set...}));
        return std::move(self.object_);
    }

private:
    template <typename, std::meta::info...>
    friend class builder;

    constexpr explicit builder(T&& object) : object_(std::move(object)) {}

    T object_{};
};

}  // namespace reflect
