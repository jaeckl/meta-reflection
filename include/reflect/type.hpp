#pragma once

#include "config.hpp"

#include <meta>
#include <string_view>

namespace reflect {

/// The name of a type: "User" for struct User.
///
/// A type without a plain identifier (int, Box<int>, a pointer) gets the
/// compiler's display text instead, which differs between compilers.
template <typename T>
constexpr std::string_view name_of() {
    return std::meta::has_identifier(^^T) ? std::meta::identifier_of(^^T)
                                         : std::meta::display_string_of(^^T);
}

}  // namespace reflect
