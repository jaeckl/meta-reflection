#pragma once

#include "config.hpp"

#include <meta>
#include <string>
#include <string_view>

namespace reflect {

namespace detail {

// "outer::inner::Name", built from the enclosing namespaces and classes.
consteval const char* qualified_name(std::meta::info type) {
    if (!std::meta::has_identifier(type))
        return std::define_static_string(std::meta::display_string_of(type));
    std::string name(std::meta::identifier_of(type));
    for (std::meta::info scope = std::meta::parent_of(type); scope != ^^::;
         scope = std::meta::parent_of(scope)) {
        // A function, for a local class, or an unnamed namespace: the type
        // cannot be named from outside such a scope, so the scopes are
        // left out.
        bool nameable = std::meta::is_namespace(scope) || std::meta::is_type(scope);
        if (!nameable || !std::meta::has_identifier(scope))
            break;
        name = std::string(std::meta::identifier_of(scope)) + "::" + name;
    }
    return std::define_static_string(name);
}

}  // namespace detail

/// The name of a type: "User" for struct User.
///
/// A type without a plain identifier (int, Box<int>, a pointer) gets the
/// compiler's display text instead, which differs between compilers.
template <typename T>
constexpr std::string_view name_of() {
    return std::meta::has_identifier(^^T) ? std::meta::identifier_of(^^T)
                                         : std::meta::display_string_of(^^T);
}

/// The name of a type with the namespaces and classes around it:
/// "shop::User" for struct User in namespace shop. This is the name to
/// write in generated code.
///
/// Like name_of, a type without a plain identifier gets the compiler's
/// display text.
template <typename T>
constexpr std::string_view qualified_name_of() {
    return detail::qualified_name(^^T);
}

}  // namespace reflect
