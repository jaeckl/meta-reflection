#pragma once

// Helpers that work on whole objects rather than on one member.

#include "access.hpp"
#include "base.hpp"
#include "config.hpp"
#include "field.hpp"
#include "list.hpp"

#include <concepts>
#include <meta>
#include <string_view>
#include <tuple>
#include <type_traits>

namespace reflect {

namespace detail {

template <typename Object, std::meta::info... Infos>
constexpr auto tie(Object& object, list<field, Infos...>) {
    return std::tuple<decltype(field<Infos>::get(object))...>{
        field<Infos>::get(object)...};
}

template <typename Object, typename Fn, std::meta::info... Infos>
constexpr bool for_each_field_while(Object& object, Fn& fn,
                                    list<field, Infos...>) {
    return (static_cast<bool>(fn(field<Infos>{}, field<Infos>::get(object))) &&
            ...);
}

template <typename T, typename Fn, std::meta::info... Infos>
constexpr T make(Fn& fn, list<field, Infos...>) {
    if constexpr (std::is_aggregate_v<T> && bases_of_t<T>::empty()) {
        // Braces guarantee that the callback runs in declaration order.
        return T{fn(field<Infos>{})...};
    } else {
        T object{};
        (field<Infos>::set(object, fn(field<Infos>{})), ...);
        return object;
    }
}

}  // namespace detail

/// A tuple of references to the fields of `object`, like std::tie.
///
///     reflect::tie(a) == reflect::tie(b)     // memberwise comparison
///     reflect::tie(a) <  reflect::tie(b)     // lexicographic ordering
///
/// Bit-fields are copied into the tuple, since no reference can bind to one.
template <typename Object, access_context Access = default_access>
    requires std::is_class_v<Object>
constexpr auto tie(Object& object) {
    return detail::tie(object,
                       fields_of_t<std::remove_const_t<Object>, Access>{});
}

/// Calls fn(field, value_a, value_b) for every field of two objects of the
/// same type. The building block for compare, diff, merge and copy.
template <access_context Access = default_access, typename A, typename B,
          typename Fn>
    requires std::same_as<std::remove_cvref_t<A>, std::remove_cvref_t<B>> &&
             std::is_class_v<std::remove_cvref_t<A>>
constexpr void for_each_field(A&& a, B&& b, Fn&& fn) {
    fields_of_t<std::remove_cvref_t<A>, Access>::for_each(
        [&](auto f) { fn(f, f.get(a), f.get(b)); });
}

/// Like for_each_field(object, fn), but stops as soon as fn returns false.
/// Returns true if every field was visited.
template <access_context Access = default_access, typename Object, typename Fn>
    requires std::is_class_v<std::remove_cvref_t<Object>>
constexpr bool for_each_field_while(Object&& object, Fn&& fn) {
    return detail::for_each_field_while(
        object, fn, fields_of_t<std::remove_cvref_t<Object>, Access>{});
}

/// Finds a field by a name that is only known at run time and calls
/// fn(field, value) for it. Returns false if no field has that name.
///
/// fn is instantiated for every field of the type, so it has to compile for
/// each field type, not just for the one that ends up matching.
template <access_context Access = default_access, typename Object, typename Fn>
    requires std::is_class_v<std::remove_cvref_t<Object>>
constexpr bool visit_field(Object&& object, std::string_view name, Fn&& fn) {
    bool found = false;
    fields_of_t<std::remove_cvref_t<Object>, Access>::for_each([&](auto f) {
        if constexpr (f.has_name()) {
            if (!found && f.name() == name) {
                found = true;
                fn(f, f.get(object));
            }
        }
    });
    return found;
}

/// Builds a T by asking fn(field) for the value of every field, in
/// declaration order. The counterpart of for_each_field for reading input.
///
/// An aggregate without base classes is brace-initialized, so const and
/// reference members work. Any other type has to be default constructible
/// and is assigned field by field.
template <typename T, access_context Access = default_access, typename Fn>
    requires std::is_class_v<T>
constexpr T make(Fn&& fn) {
    return detail::make<T>(fn, fields_of_t<T, Access>{});
}

}  // namespace reflect
