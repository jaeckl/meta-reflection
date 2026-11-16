#pragma once

#include "config.hpp"

#include <concepts>
#include <meta>

namespace reflect {

/// Decides which members a discovery function may see.
using access_context = std::meta::access_context;

/// Only what code at global scope may access.
inline constexpr access_context public_access = access_context::unprivileged();

/// Every declared member, private and protected included.
inline constexpr access_context unchecked_access = access_context::unchecked();

/// The policy used when a discovery function is called without one.
inline constexpr access_context default_access = unchecked_access;

/// What the calling code may access: public members from outside a class,
/// everything from inside one of its member functions.
consteval access_context current_access(
    access_context context = access_context::current()) {
    return context;
}

/// Satisfied by every proxy type: field, function, base, enumerator, member,
/// annotation and the rest.
template <typename D>
concept descriptor = requires {
    { D::native() } -> std::same_as<std::meta::info>;
};

}  // namespace reflect
