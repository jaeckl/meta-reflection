#pragma once

#include "annotation.hpp"
#include "config.hpp"
#include "detail/meta.hpp"
#include "list.hpp"

#include <array>
#include <cstddef>
#include <meta>
#include <optional>
#include <string_view>
#include <type_traits>

namespace reflect {

/// One enumerator of an enumeration.
template <std::meta::info Info>
    requires(std::meta::is_enumerator(Info))
struct enumerator : annotated<Info> {
    /// The enumeration type.
    using type = [:std::meta::type_of(Info):];

    static constexpr std::string_view name() {
        return std::meta::identifier_of(Info);
    }
    static constexpr type value() { return std::meta::extract<type>(Info); }
};

template <typename E>
    requires std::is_enum_v<E>
using enumerators_of_t =
    [:detail::make_list(^^enumerator, std::meta::enumerators_of(^^E)):];

/// The enumerators of E, in declaration order.
template <typename E>
    requires std::is_enum_v<E>
constexpr enumerators_of_t<E> get_enumerators_of() {
    return {};
}

/// Calls fn(enumerator) for every enumerator of E.
template <typename E, typename Fn>
    requires std::is_enum_v<E>
constexpr void for_each_enumerator(Fn&& fn) {
    enumerators_of_t<E>::for_each(fn);
}

namespace detail {

template <typename E, std::meta::info... Infos>
constexpr std::array<E, sizeof...(Infos)> enum_values(list<enumerator, Infos...>) {
    return {enumerator<Infos>::value()...};
}

}  // namespace detail

/// The number of enumerators of E.
template <typename E>
    requires std::is_enum_v<E>
constexpr std::size_t enum_count() {
    return enumerators_of_t<E>::size();
}

/// All values of E as an array, in declaration order.
template <typename E>
    requires std::is_enum_v<E>
constexpr std::array<E, enumerators_of_t<E>::size()> enum_values() {
    return detail::enum_values<E>(enumerators_of_t<E>{});
}

/// All enumerator names of E as an array, in declaration order.
template <typename E>
    requires std::is_enum_v<E>
constexpr std::array<std::string_view, enumerators_of_t<E>::size()> enum_names() {
    return enumerators_of_t<E>::names();
}

/// The name of an enumerator: enum_name(Role::Admin) == "Admin".
/// Empty if no enumerator has that value. If several enumerators share the
/// value, the first one declared wins.
template <typename E>
    requires std::is_enum_v<E>
constexpr std::string_view enum_name(E value) {
    std::string_view result;
    for_each_enumerator<E>([&](auto e) {
        if (result.empty() && e.value() == value)
            result = e.name();
    });
    return result;
}

/// The enumerator with that name, or nullopt. The match is exact and case
/// sensitive.
template <typename E>
    requires std::is_enum_v<E>
constexpr std::optional<E> enum_cast(std::string_view name) {
    std::optional<E> result;
    for_each_enumerator<E>([&](auto e) {
        if (!result && e.name() == name)
            result = e.value();
    });
    return result;
}

}  // namespace reflect
