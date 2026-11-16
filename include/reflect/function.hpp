#pragma once

#include "access.hpp"
#include "annotation.hpp"
#include "config.hpp"
#include "detail/meta.hpp"
#include "list.hpp"

#include <cstddef>
#include <meta>
#include <string_view>
#include <utility>
#include <vector>

namespace reflect {

/// A member function, static or not. Each overload is its own function.
template <std::meta::info Info>
    requires(std::meta::is_function(Info))
struct function : annotated<Info> {
    using return_type = [:std::meta::return_type_of(Info):];

    /// The type of parameter I, as declared.
    template <std::size_t I>
    using parameter_type =
        [:std::meta::type_of(std::meta::parameters_of(Info)[I]):];

    static constexpr std::string_view name() {
        return std::meta::identifier_of(Info);
    }

    /// The number of parameters.
    static consteval std::size_t arity() {
        return std::meta::parameters_of(Info).size();
    }

    static consteval bool is_static() {
        return std::meta::is_static_member(Info);
    }
    static consteval bool is_const() { return std::meta::is_const(Info); }
    static consteval bool is_virtual() { return std::meta::is_virtual(Info); }
    static consteval bool is_noexcept() { return std::meta::is_noexcept(Info); }
    static consteval bool is_public() { return std::meta::is_public(Info); }
    static consteval bool is_protected() { return std::meta::is_protected(Info); }
    static consteval bool is_private() { return std::meta::is_private(Info); }

    /// Calls the function on `object`.
    template <typename Object, typename... Args>
        requires(!std::meta::is_static_member(Info))
    static constexpr decltype(auto) invoke(Object&& object, Args&&... args) {
        return std::forward<Object>(object).[:Info:](std::forward<Args>(args)...);
    }

    /// Calls a static member function.
    template <typename... Args>
        requires(std::meta::is_static_member(Info))
    static constexpr decltype(auto) invoke(Args&&... args) {
        return [:Info:](std::forward<Args>(args)...);
    }

    /// Pointer to member function, or a plain function pointer for a static one.
    static constexpr auto pointer() { return &[:Info:]; }
};

namespace detail {

// Named functions only: no constructors, destructors, operators or
// conversion functions, and nothing that was deleted.
consteval std::vector<std::meta::info> functions_of(std::meta::info type,
                                                    access_context access) {
    std::vector<std::meta::info> functions;
    for (std::meta::info member : std::meta::members_of(type, access)) {
        if (std::meta::is_function(member) &&
            std::meta::has_identifier(member) && !std::meta::is_deleted(member))
            functions.push_back(member);
    }
    return functions;
}

}  // namespace detail

template <typename T, access_context Access = default_access>
using functions_of_t =
    [:detail::make_list(^^function, detail::functions_of(^^T, Access)):];

/// The named member functions T declares itself, in declaration order.
template <typename T, access_context Access = default_access>
constexpr functions_of_t<T, Access> get_functions_of() {
    return {};
}

/// Calls fn(function) for every named member function of T.
template <typename T, access_context Access = default_access, typename Fn>
constexpr void for_each_function(Fn&& fn) {
    functions_of_t<T, Access>::for_each(fn);
}

}  // namespace reflect
