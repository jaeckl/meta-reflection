#pragma once

#include "annotation.hpp"
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

/// A type, the way a field proxy is a field.
///
/// The type is part of the proxy (type_info<^^User>), so every function is
/// static and a type_info object is empty. Discovery hands one of these to
/// its callback: [](auto cls) { cls.name(); }.
///
/// Any type is accepted: a class, an enum, a union, an alias, int. Finding
/// those types is a separate question. The class walks still report classes
/// only.
template <std::meta::info Info>
    requires(std::meta::is_type(Info) || std::meta::is_type_alias(Info))
struct type_info : annotated<Info> {
    /// The type itself. User for type_info<^^User>.
    using type = [:Info:];

    /// "User" for struct User. A type without a plain identifier (int,
    /// Box<int>, a pointer) gets the compiler's display text instead, which
    /// differs between compilers.
    static constexpr std::string_view name() {
        return std::meta::has_identifier(Info) ? std::meta::identifier_of(Info)
                                               : std::meta::display_string_of(Info);
    }

    /// "shop::User". This is the name to write in generated code. An alias
    /// gives the name of the type it stands for. Scopes that code outside
    /// cannot name, a function or an unnamed namespace, are left out.
    static constexpr std::string_view qualified_name() {
        return detail::qualified_name(Info);
    }
};

/// The name of a type: "User" for struct User.
///
/// A type without a plain identifier (int, Box<int>, a pointer) gets the
/// compiler's display text instead, which differs between compilers.
template <typename T>
constexpr std::string_view name_of() {
    return type_info<^^T>::name();
}

/// The name of a type with the namespaces and classes around it:
/// "shop::User" for struct User in namespace shop. This is the name to
/// write in generated code.
///
/// Like name_of, a type without a plain identifier gets the compiler's
/// display text.
template <typename T>
constexpr std::string_view qualified_name_of() {
    return type_info<^^T>::qualified_name();
}

}  // namespace reflect
