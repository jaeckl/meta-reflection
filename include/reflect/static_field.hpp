#pragma once

#include "access.hpp"
#include "annotation.hpp"
#include "config.hpp"
#include "detail/meta.hpp"
#include "list.hpp"

#include <meta>
#include <string_view>
#include <utility>

namespace reflect {

/// A static data member. Like field, but there is no object to pass in.
template <std::meta::info Info>
    requires(std::meta::is_variable(Info) && std::meta::is_static_member(Info))
struct static_field : annotated<Info> {
    /// The declared type of the member. Keeps const.
    using type = [:std::meta::type_of(Info):];
    /// The class that declares the member.
    using declaring_type = [:std::meta::parent_of(Info):];

    static constexpr std::string_view name() {
        return std::meta::identifier_of(Info);
    }

    static consteval bool is_public() { return std::meta::is_public(Info); }
    static consteval bool is_protected() { return std::meta::is_protected(Info); }
    static consteval bool is_private() { return std::meta::is_private(Info); }

    /// A reference to the variable.
    static constexpr type& get() { return [:Info:]; }

    template <typename Value>
    static constexpr void set(Value&& value) {
        [:Info:] = std::forward<Value>(value);
    }

    /// A plain pointer to the variable.
    static constexpr auto pointer() { return &[:Info:]; }
};

template <typename T, access_context Access = default_access>
using static_fields_of_t =
    [:detail::make_list(^^static_field,
                        std::meta::static_data_members_of(^^T, Access)):];

/// The static data members T declares, in declaration order.
template <typename T, access_context Access = default_access>
constexpr static_fields_of_t<T, Access> get_static_fields_of() {
    return {};
}

/// Calls fn(static_field) for every static data member of T.
template <typename T, access_context Access = default_access, typename Fn>
constexpr void for_each_static_field(Fn&& fn) {
    static_fields_of_t<T, Access>::for_each(fn);
}

}  // namespace reflect
