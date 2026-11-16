#pragma once

#include "access.hpp"
#include "annotation.hpp"
#include "config.hpp"
#include "detail/meta.hpp"
#include "list.hpp"

#include <meta>
#include <utility>

namespace reflect {

/// A direct base class of a type.
template <std::meta::info Info>
    requires(std::meta::is_base(Info))
struct base : annotated<Info> {
    /// The base class itself. Pass it back in to keep going:
    /// reflect::get_fields_of<typename decltype(base)::type>().
    using type = [:std::meta::type_of(Info):];

    static consteval bool is_virtual() { return std::meta::is_virtual(Info); }
    static consteval bool is_public() { return std::meta::is_public(Info); }
    static consteval bool is_protected() { return std::meta::is_protected(Info); }
    static consteval bool is_private() { return std::meta::is_private(Info); }

    /// The base class part of `object`.
    template <typename Object>
    static constexpr decltype(auto) get(Object&& object) {
        return (std::forward<Object>(object).[:Info:]);
    }
};

template <typename T, access_context Access = default_access>
using bases_of_t =
    [:detail::make_list(^^base, std::meta::bases_of(^^T, Access)):];

/// The direct base classes of T, in declaration order.
template <typename T, access_context Access = default_access>
constexpr bases_of_t<T, Access> get_bases_of() {
    return {};
}

/// Calls fn(base) for every direct base class of T.
template <typename T, access_context Access = default_access, typename Fn>
constexpr void for_each_base(Fn&& fn) {
    bases_of_t<T, Access>::for_each(fn);
}

}  // namespace reflect
