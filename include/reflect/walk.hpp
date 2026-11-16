#pragma once

// Walking an object together with the objects nested inside it.

#include "access.hpp"
#include "config.hpp"
#include "field.hpp"
#include "list.hpp"

#include <cstddef>
#include <iosfwd>
#include <meta>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace reflect {

/// Where reflect::walk currently is: the field names from the outermost
/// object down to the current field, for example {"home", "city"}.
///
/// Only valid during the call it was passed to. Use to_string() to keep it.
class field_path {
public:
    constexpr field_path() = default;
    constexpr explicit field_path(std::span<const std::string_view> names)
        : names_(names) {}

    /// How deep the current field is nested: 1 for a field of the object
    /// walk was called with.
    constexpr std::size_t size() const { return names_.size(); }
    constexpr std::string_view operator[](std::size_t index) const {
        return names_[index];
    }
    /// The name of the current field itself.
    constexpr std::string_view back() const { return names_.back(); }

    constexpr auto begin() const { return names_.begin(); }
    constexpr auto end() const { return names_.end(); }

    /// The names joined into one string: "home.city".
    constexpr std::string to_string(std::string_view separator = ".") const {
        std::string text;
        for (std::string_view name : names_) {
            if (!text.empty())
                text += separator;
            text += name;
        }
        return text;
    }

    /// std::cout << path prints "home.city".
    template <typename Traits>
    friend std::basic_ostream<char, Traits>& operator<<(
        std::basic_ostream<char, Traits>& out, field_path path) {
        bool first = true;
        for (std::string_view name : path.names_) {
            if (!first)
                out << '.';
            out << name;
            first = false;
        }
        return out;
    }

private:
    std::span<const std::string_view> names_;
};

/// Several lambdas as one function object, one per type to handle:
///
///     reflect::overloaded{
///         [](reflect::field_path path, auto field, int value) { ... },
///         [](reflect::field_path path, auto field, const std::string& value) { ... },
///     }
template <typename... Fns>
struct overloaded : Fns... {
    using Fns::operator()...;
};

namespace detail {

template <access_context Access, typename Object, typename Fn>
constexpr void walk(Object& object, Fn& fn, std::vector<std::string_view>& names) {
    all_fields_of_t<std::remove_const_t<Object>, Access>::for_each([&](auto f) {
        names.push_back(f.has_name() ? f.name() : std::string_view{});
        // The callback decides what a value is: a field it can be called
        // with is handed over, any other field is opened up.
        if constexpr (requires { fn(field_path(names), f, f.get(object)); }) {
            fn(field_path(names), f, f.get(object));
        } else {
            static_assert(
                std::is_class_v<std::remove_cvref_t<decltype(f.get(object))>>,
                "reflect::walk: the callback does not accept the type of this "
                "field, and the field is not a class that could be walked "
                "into; add an overload for the type");
            detail::walk<Access>(f.get(object), fn, names);
        }
        names.pop_back();
    });
}

}  // namespace detail

/// Calls fn(path, field, value) for every field of `object` and of the
/// objects nested inside it.
///
/// The callback decides where the walk stops. A field whose value fn can be
/// called with is passed to fn. Any other field has to be a class, and its
/// fields are walked in turn. So fn lists the types it treats as values:
///
///     reflect::walk(employee, reflect::overloaded{
///         [](reflect::field_path path, auto, int value) { ... },
///         [](reflect::field_path path, auto, const std::string& value) { ... },
///     });
///
/// Inherited fields are included, base classes first.
template <access_context Access = default_access, typename Object, typename Fn>
    requires std::is_class_v<std::remove_cvref_t<Object>>
constexpr void walk(Object&& object, Fn&& fn) {
    std::vector<std::string_view> names;
    detail::walk<Access>(object, fn, names);
}

}  // namespace reflect
