#pragma once

#include "config.hpp"

#include <algorithm>
#include <cstddef>
#include <meta>
#include <string_view>

namespace reflect {

/// Text that can be stored in an annotation.
///
/// An annotation value has to be a structural type, which rules out
/// std::string and std::string_view. static_string keeps the characters in
/// static storage and stays structural:
///
///     struct Rename { reflect::static_string value; };
///     struct User { [[=Rename{"display name"}]] std::string name; };
struct static_string {
    const char* data;
    std::size_t size;

    consteval static_string(const char* text)
        : data(std::define_static_string(std::string_view(text))),
          size(std::string_view(text).size()) {}

    consteval static_string(std::string_view text)
        : data(std::define_static_string(text)), size(text.size()) {}

    constexpr std::string_view view() const { return {data, size}; }
    constexpr const char* c_str() const { return data; }
    constexpr operator std::string_view() const { return view(); }

    friend constexpr bool operator==(static_string left, static_string right) {
        return left.view() == right.view();
    }
};

/// A string literal usable as a template argument, as in
/// reflect::get_field_of<User, "id">().
template <std::size_t N>
struct fixed_string {
    char text[N]{};

    constexpr fixed_string(const char (&literal)[N]) {
        std::copy_n(literal, N, text);
    }

    constexpr std::string_view view() const { return {text, N - 1}; }
};

}  // namespace reflect
