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

template <typename T, access_context Access, typename Fn>
constexpr T make(Fn& fn);

// An aggregate is brace-initialized: one initializer per base class, built
// the same way, then one per field. Braces also guarantee that the callback
// runs in that order.
template <typename T, access_context Access, typename Fn, std::meta::info... Bases,
          std::meta::info... Fields>
constexpr T make_aggregate(Fn& fn, list<base, Bases...>, list<field, Fields...>) {
    return T{make<typename base<Bases>::type, Access>(fn)...,
             fn(field<Fields>{})...};
}

template <typename T, typename Fn, std::meta::info... Fields>
constexpr T make_assigned(Fn& fn, list<field, Fields...>) {
    T object{};
    (field<Fields>::set(object, fn(field<Fields>{})), ...);
    return object;
}

template <typename T, access_context Access, typename Fn>
constexpr T make(Fn& fn) {
    if constexpr (std::is_aggregate_v<T>) {
        return make_aggregate<T, Access>(fn, bases_of_t<T, Access>{},
                                         fields_of_t<T, Access>{});
    } else {
        return make_assigned<T>(fn, all_fields_of_t<T, Access>{});
    }
}

}  // namespace detail

/// A tuple of references to the fields of `object`, like std::tie.
///
///     reflect::tie(a) == reflect::tie(b)     // memberwise comparison
///     reflect::tie(a) <  reflect::tie(b)     // lexicographic ordering
///
/// Bit-fields are copied into the tuple, since no reference can bind to one.
template <access_context Access = default_access, typename Object>
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

/// Builds a T by asking fn(field) for the value of every field. The
/// counterpart of for_each_field for reading input.
///
/// Inherited fields are part of the object, so fn is asked for them too:
/// the fields of the base classes first, then those of T, in the order of
/// get_all_fields_of<T>().
///
/// An aggregate is brace-initialized, so const and reference members work.
/// Any other type has to be default constructible and is assigned field by
/// field.
template <typename T, access_context Access = default_access, typename Fn>
    requires std::is_class_v<T>
constexpr T make(Fn&& fn) {
    return detail::make<T, Access>(fn);
}

}  // namespace reflect
