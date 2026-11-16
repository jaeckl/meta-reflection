#pragma once

// Experimental: the classes of a namespace.

#include "access.hpp"
#include "config.hpp"

#include <meta>
#include <type_traits>
#include <vector>

namespace reflect {

namespace detail {

template <std::meta::info... Types>
struct class_list {
    template <typename Fn>
    static constexpr void for_each(Fn& fn) {
        (static_cast<void>(fn(std::type_identity<typename [:Types:]>{})), ...);
    }
};

consteval std::meta::info classes_in(std::meta::info scope) {
    std::vector<std::meta::info> arguments;
    for (std::meta::info member : std::meta::members_of(scope, unchecked_access)) {
        // An alias would report its class a second time.
        if (!std::meta::is_type(member) || std::meta::is_type_alias(member))
            continue;
        if (!std::meta::is_class_type(member) || !std::meta::is_complete_type(member))
            continue;
        arguments.push_back(std::meta::reflect_constant(member));
    }
    return std::meta::substitute(^^class_list, arguments);
}

}  // namespace detail

/// Calls fn(std::type_identity<T>{}) for every class T declared directly in
/// a namespace:
///
///     reflect::for_each_class_in<^^model>([]<typename T>(std::type_identity<T>) {
///         if constexpr (reflect::has_annotation<Entity, T>())
///             std::cout << reflect::name_of<T>() << '\n';
///     });
///
/// Only complete, non-template classes are reported, and nested namespaces
/// are not searched. The compiler knows the classes it has seen so far in
/// the current source file: call this after the headers that declare them.
template <std::meta::info Namespace, typename Fn>
    requires(std::meta::is_namespace(Namespace))
constexpr void for_each_class_in(Fn&& fn) {
    using classes = [:detail::classes_in(Namespace):];
    classes::for_each(fn);
}

}  // namespace reflect
