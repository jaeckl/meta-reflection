#pragma once

#include "access.hpp"
#include "annotation.hpp"
#include "config.hpp"
#include "detail/meta.hpp"
#include "list.hpp"
#include "string.hpp"

#include <algorithm>
#include <cstddef>
#include <meta>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace reflect {

/// A non-static data member.
///
/// The member is part of the type (field<^^User::id>), so every function is
/// static and a field object is empty. That is what lets a generic lambda ask
/// compile-time questions about its `auto field` parameter.
template <std::meta::info Info>
    requires(std::meta::is_nonstatic_data_member(Info))
struct field : annotated<Info> {
    /// The declared type of the member. Keeps const and references.
    using type = [:std::meta::type_of(Info):];
    /// The class that declares the member.
    using declaring_type = [:std::meta::parent_of(Info):];

    /// False for an unnamed member, such as an anonymous union.
    static consteval bool has_name() { return std::meta::has_identifier(Info); }
    static constexpr std::string_view name() {
        return std::meta::identifier_of(Info);
    }

    static consteval bool is_public() { return std::meta::is_public(Info); }
    static consteval bool is_protected() { return std::meta::is_protected(Info); }
    static consteval bool is_private() { return std::meta::is_private(Info); }
    static consteval bool is_mutable() {
        return std::meta::is_mutable_member(Info);
    }
    static consteval bool is_bit_field() { return std::meta::is_bit_field(Info); }
    static consteval bool has_default_initializer() {
        return std::meta::has_default_member_initializer(Info);
    }
    /// Offset in bytes from the start of declaring_type.
    static consteval std::size_t offset() {
        return std::meta::offset_of(Info).bytes;
    }

    /// The member of `object`. A reference that follows the constness and
    /// value category of `object`; a bit-field comes back by value, because
    /// no reference can bind to one.
    template <typename Object>
    static constexpr decltype(auto) get(Object&& object) {
        if constexpr (std::meta::is_bit_field(Info)) {
            return static_cast<type>(std::forward<Object>(object).[:Info:]);
        } else {
            return (std::forward<Object>(object).[:Info:]);
        }
    }

    /// Assigns to the member of `object`.
    template <typename Object, typename Value>
    static constexpr void set(Object& object, Value&& value) {
        object.[:Info:] = std::forward<Value>(value);
    }

    /// Pointer to member. Not available for bit-fields and reference members.
    static constexpr auto pointer() { return &[:Info:]; }
};

namespace detail {

consteval void collect_all_fields(std::meta::info type, access_context access,
                                  std::vector<std::meta::info>& seen_virtual,
                                  std::vector<std::meta::info>& fields) {
    for (std::meta::info base : std::meta::bases_of(type, access)) {
        std::meta::info base_type = std::meta::type_of(base);
        if (std::meta::is_virtual(base)) {
            if (std::ranges::contains(seen_virtual, base_type))
                continue;
            seen_virtual.push_back(base_type);
        }
        collect_all_fields(base_type, access, seen_virtual, fields);
    }
    for (std::meta::info member :
         std::meta::nonstatic_data_members_of(type, access))
        fields.push_back(member);
}

consteval std::vector<std::meta::info> all_fields_of(std::meta::info type,
                                                     access_context access) {
    std::vector<std::meta::info> seen_virtual;
    std::vector<std::meta::info> fields;
    collect_all_fields(type, access, seen_virtual, fields);
    return fields;
}

consteval std::meta::info field_named(std::meta::info type,
                                      std::string_view name) {
    for (std::meta::info member :
         std::meta::nonstatic_data_members_of(type, unchecked_access)) {
        if (std::meta::has_identifier(member) &&
            std::meta::identifier_of(member) == name)
            return member;
    }
    throw std::meta::exception(
        "reflect::get_field_of: no field with that name", type);
}

template <typename Class, typename Member>
Class class_of_member(Member Class::*);

// Is Pointer the address of the member Info? Bit-fields and reference
// members have no address, so they never match.
template <auto Pointer, std::meta::info Info>
consteval bool is_member_pointer_to() {
    if constexpr (std::meta::is_bit_field(Info) ||
                  std::is_reference_v<typename field<Info>::type>) {
        return false;
    } else if constexpr (!std::same_as<decltype(&[:Info:]), decltype(Pointer)>) {
        return false;
    } else {
        return &[:Info:] == Pointer;
    }
}

template <auto Pointer, std::meta::info... Infos>
consteval std::meta::info field_at(list<field, Infos...>) {
    std::meta::info result{};
    bool found = ((is_member_pointer_to<Pointer, Infos>() ? (result = Infos, true)
                                                          : false) ||
                  ...);
    if (!found)
        throw std::meta::exception(
            "reflect::get_field_of: the pointer does not name a field declared "
            "by that class",
            ^^void);
    return result;
}

template <std::meta::info... Infos>
std::tuple<typename field<Infos>::type...> field_types(list<field, Infos...>);

}  // namespace detail

template <typename T, access_context Access = default_access>
using fields_of_t = [:detail::make_list(
                          ^^field,
                          std::meta::nonstatic_data_members_of(^^T, Access)):];

template <typename T, access_context Access = default_access>
using all_fields_of_t =
    [:detail::make_list(^^field, detail::all_fields_of(^^T, Access)):];

/// The fields T declares itself, in declaration order.
template <typename T, access_context Access = default_access>
constexpr fields_of_t<T, Access> get_fields_of() {
    return {};
}

/// The fields of T including inherited ones: base classes first, then T.
/// A virtual base contributes once.
template <typename T, access_context Access = default_access>
constexpr all_fields_of_t<T, Access> get_all_fields_of() {
    return {};
}

/// One field by name: reflect::get_field_of<User, "id">().
/// A compile error if T declares no field with that name.
template <typename T, fixed_string Name>
constexpr auto get_field_of() {
    return field<detail::field_named(^^T, Name.view())>{};
}

/// One field by pointer to member: reflect::get_field_of<&User::id>().
///
/// Unlike the name form this survives a rename done by a refactoring tool.
/// It cannot reach bit-fields, reference members or private members, because
/// none of those have a pointer you can write down.
template <auto Pointer>
    requires std::is_member_object_pointer_v<decltype(Pointer)>
constexpr auto get_field_of() {
    using Class = decltype(detail::class_of_member(Pointer));
    return field<detail::field_at<Pointer>(fields_of_t<Class, unchecked_access>{})>{};
}

/// True if T declares a field with that name: has_field<User, "id">().
template <typename T, fixed_string Name>
consteval bool has_field() {
    for (std::meta::info member :
         std::meta::nonstatic_data_members_of(^^T, unchecked_access)) {
        if (std::meta::has_identifier(member) &&
            std::meta::identifier_of(member) == Name.view())
            return true;
    }
    return false;
}

/// The position of a field in get_fields_of<T>(), counted from 0.
/// A compile error if T declares no field with that name.
template <typename T, fixed_string Name>
consteval std::size_t field_index_of() {
    std::size_t index = 0;
    for (std::meta::info member :
         std::meta::nonstatic_data_members_of(^^T, unchecked_access)) {
        if (std::meta::has_identifier(member) &&
            std::meta::identifier_of(member) == Name.view())
            return index;
        ++index;
    }
    throw std::meta::exception(
        "reflect::field_index_of: no field with that name", ^^T);
}

/// The field types of T as a tuple type:
/// field_types_t<Point> is std::tuple<int, int>.
template <typename T, access_context Access = default_access>
using field_types_t = decltype(detail::field_types(fields_of_t<T, Access>{}));

/// Calls fn(field) for every field of T.
template <typename T, access_context Access = default_access, typename Fn>
constexpr void for_each_field(Fn&& fn) {
    fields_of_t<T, Access>::for_each(fn);
}

/// Calls fn(field, value) for every field of `object`. `value` is a reference
/// to the member inside `object`.
template <access_context Access = default_access, typename Object, typename Fn>
    requires std::is_class_v<std::remove_cvref_t<Object>>
constexpr void for_each_field(Object&& object, Fn&& fn) {
    fields_of_t<std::remove_cvref_t<Object>, Access>::for_each(
        [&](auto f) { fn(f, f.get(std::forward<Object>(object))); });
}

}  // namespace reflect
