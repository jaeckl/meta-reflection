#pragma once

// Experimental: the classes of a namespace.

#include "access.hpp"
#include "config.hpp"
#include "list.hpp"
#include "type.hpp"

#include <meta>
#include <vector>

namespace reflect {

namespace detail {

consteval std::meta::info classes_in(std::meta::info scope) {
    std::vector<std::meta::info> found;
    for (std::meta::info member : std::meta::members_of(scope, unchecked_access)) {
        // An alias would report its class a second time.
        if (!std::meta::is_type(member) || std::meta::is_type_alias(member))
            continue;
        if (!std::meta::is_class_type(member) || !std::meta::is_complete_type(member))
            continue;
        found.push_back(member);
    }
    return make_list(^^type_info, found);
}

}  // namespace detail

/// Calls fn(type_info<...>{}) for every class declared directly in a
/// namespace:
///
///     reflect::for_each_class_in<^^model>([](auto cls) {
///         if constexpr (cls.template has_annotation<Entity>())
///             std::cout << cls.qualified_name() << '\n';
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
