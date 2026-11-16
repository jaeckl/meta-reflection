#pragma once

#include "access.hpp"
#include "annotation.hpp"
#include "config.hpp"
#include "detail/meta.hpp"
#include "list.hpp"
#include "parameter.hpp"

#include <cstddef>
#include <meta>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace reflect {

namespace detail {

// A constructor cannot be spliced the way a member function can, so calling
// one specific constructor needs a detour: a function with exactly the
// parameters of that constructor. Arguments convert at its boundary, and the
// call inside then matches the constructor exactly.
template <std::meta::info Constructor,
          typename Indices = std::make_index_sequence<
              std::meta::parameters_of(Constructor).size()>>
struct constructor_thunk;

template <std::meta::info Constructor, std::size_t... I>
struct constructor_thunk<Constructor, std::index_sequence<I...>> {
    using type = [:std::meta::parent_of(Constructor):];

    static constexpr type call(
        typename parameter<Constructor, I>::type... arguments) {
        return type(
            static_cast<typename parameter<Constructor, I>::type&&>(arguments)...);
    }
};

}  // namespace detail

/// A constructor of a class.
template <std::meta::info Info>
    requires(std::meta::is_constructor(Info))
struct constructor : annotated<Info> {
    /// The class the constructor belongs to, which is what it builds.
    using declaring_type = [:std::meta::parent_of(Info):];

    /// The type of parameter I, as declared.
    template <std::size_t I>
    using parameter_type =
        [:std::meta::type_of(std::meta::parameters_of(Info)[I]):];

    /// The number of parameters.
    static consteval std::size_t arity() {
        return std::meta::parameters_of(Info).size();
    }

    /// True if the constructor can be called without arguments.
    static consteval bool is_default() {
        return std::meta::is_default_constructor(Info);
    }
    static consteval bool is_copy() { return std::meta::is_copy_constructor(Info); }
    static consteval bool is_move() { return std::meta::is_move_constructor(Info); }
    static consteval bool is_explicit() { return std::meta::is_explicit(Info); }
    static consteval bool is_noexcept() { return std::meta::is_noexcept(Info); }
    static consteval bool is_public() { return std::meta::is_public(Info); }
    static consteval bool is_protected() { return std::meta::is_protected(Info); }
    static consteval bool is_private() { return std::meta::is_private(Info); }

    /// Builds an object with exactly this constructor. The arguments are
    /// converted to the parameter types as in an ordinary call; which
    /// constructor runs is already decided.
    ///
    /// Only for public constructors. Reflection can read a private field,
    /// but it cannot call a private constructor.
    template <typename... Args>
        requires(sizeof...(Args) == std::meta::parameters_of(Info).size())
    static constexpr declaring_type invoke(Args&&... args) {
        static_assert(std::meta::is_public(Info),
                      "reflect::constructor::invoke: only a public constructor "
                      "can be called");
        return detail::constructor_thunk<Info>::call(std::forward<Args>(args)...);
    }
};

namespace detail {

// Copy and move constructors are left out: they are not a way to build an
// object from its parts. So is anything deleted.
consteval std::vector<std::meta::info> constructors_of(std::meta::info type,
                                                       access_context access) {
    std::vector<std::meta::info> constructors;
    for (std::meta::info member : std::meta::members_of(type, access)) {
        if (std::meta::is_constructor(member) &&
            !std::meta::is_copy_constructor(member) &&
            !std::meta::is_move_constructor(member) &&
            !std::meta::is_deleted(member))
            constructors.push_back(member);
    }
    return constructors;
}

// The parameter type the way a caller would write it: no alias, and no
// top-level const, which is not part of a signature.
consteval std::meta::info plain_parameter_type(std::meta::info type) {
    return std::meta::remove_cv(std::meta::dealias(type));
}

consteval std::meta::info constructor_with(
    std::meta::info type, const std::vector<std::meta::info>& wanted) {
    for (std::meta::info candidate : constructors_of(type, unchecked_access)) {
        std::vector<std::meta::info> parameters = std::meta::parameters_of(candidate);
        if (parameters.size() != wanted.size())
            continue;
        bool same = true;
        for (std::size_t i = 0; i < parameters.size(); ++i) {
            if (plain_parameter_type(std::meta::type_of(parameters[i])) !=
                plain_parameter_type(wanted[i]))
                same = false;
        }
        if (same)
            return candidate;
    }
    throw std::meta::exception(
        "reflect::get_constructor_of: no constructor with these parameter types",
        type);
}

// The constructor reflect::construct<T>(fn) uses: the only public one that
// takes arguments.
consteval std::meta::info only_constructor(std::meta::info type) {
    std::meta::info found{};
    std::size_t count = 0;
    for (std::meta::info candidate : constructors_of(type, public_access)) {
        if (std::meta::parameters_of(candidate).empty())
            continue;
        found = candidate;
        ++count;
    }
    if (count != 1)
        throw std::meta::exception(
            "reflect::construct: the class needs exactly one public constructor "
            "that takes arguments; otherwise pick one, for example "
            "construct(get_constructor_of<T, int, std::string>(), fn)",
            type);
    return found;
}

template <std::meta::info Constructor, typename Fn, std::size_t... I>
constexpr typename constructor<Constructor>::declaring_type construct(
    Fn& fn, std::index_sequence<I...>) {
    // The arguments of a call are evaluated in no particular order, the
    // elements of a braced list from left to right. A callback that reads
    // from a stream depends on that.
    [[maybe_unused]] std::tuple<decltype(fn(parameter<Constructor, I>{}))...> arguments{
        fn(parameter<Constructor, I>{})...};
    return constructor<Constructor>::invoke(std::get<I>(std::move(arguments))...);
}

}  // namespace detail

template <typename T, access_context Access = default_access>
using constructors_of_t =
    [:detail::make_list(^^constructor, detail::constructors_of(^^T, Access)):];

/// The constructors of T, in declaration order, without the copy and the
/// move constructor. Includes the default constructor the compiler declares.
template <typename T, access_context Access = default_access>
constexpr constructors_of_t<T, Access> get_constructors_of() {
    return {};
}

/// One constructor, picked by its parameter types:
///
///     get_constructor_of<User, int, std::string>()   // User(int, std::string)
///     get_constructor_of<User>()                     // User()
///
/// A compile error if T has no such constructor.
template <typename T, typename... Parameters>
constexpr auto get_constructor_of() {
    return constructor<detail::constructor_with(^^T, {^^Parameters...})>{};
}

/// Calls fn(constructor) for every constructor of T.
template <typename T, access_context Access = default_access, typename Fn>
constexpr void for_each_constructor(Fn&& fn) {
    constructors_of_t<T, Access>::for_each(fn);
}

/// Builds an object by asking fn(parameter) for every argument of a
/// constructor, from left to right:
///
///     reflect::construct(reflect::get_constructor_of<User, int, std::string>(),
///                        [&](auto parameter) { return read(parameter.name()); });
template <std::meta::info Constructor, typename Fn>
constexpr typename constructor<Constructor>::declaring_type construct(
    constructor<Constructor>, Fn&& fn) {
    return detail::construct<Constructor>(
        fn, std::make_index_sequence<constructor<Constructor>::arity()>{});
}

/// Like the form above, for a class that has exactly one public constructor
/// that takes arguments: reflect::construct<Connection>(fn).
///
/// This is how to build a class that reflect::make cannot: one that is not
/// an aggregate and has no default constructor.
template <typename T, typename Fn>
    requires std::is_class_v<T>
constexpr T construct(Fn&& fn) {
    return detail::construct<detail::only_constructor(^^T)>(
        fn, std::make_index_sequence<
                std::meta::parameters_of(detail::only_constructor(^^T)).size()>{});
}

}  // namespace reflect
