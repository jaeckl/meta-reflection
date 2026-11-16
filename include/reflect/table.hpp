#pragma once

// Field metadata as plain data, for code that wants an ordinary array and an
// ordinary loop instead of templates.

#include "access.hpp"
#include "config.hpp"
#include "field.hpp"
#include "list.hpp"

#include <array>
#include <cstddef>
#include <meta>
#include <string_view>

namespace reflect {

/// What is known about one field without knowing its type.
struct field_info {
    std::string_view name;       ///< empty for an unnamed member
    std::string_view type_name;  ///< as the compiler prints it; for display only
    std::size_t index;           ///< position in get_fields_of<T>()
    std::size_t offset;          ///< bytes from the start of the object
    std::size_t size;            ///< sizeof the member; 0 for a bit-field
    bool is_public;
};

namespace detail {

template <std::meta::info Info>
constexpr field_info describe_field(std::size_t index) {
    return {
        std::meta::has_identifier(Info) ? std::meta::identifier_of(Info)
                                        : std::string_view{},
        std::meta::display_string_of(std::meta::type_of(Info)),
        index,
        std::meta::offset_of(Info).bytes,
        std::meta::is_bit_field(Info) ? 0 : std::meta::size_of(Info),
        std::meta::is_public(Info),
    };
}

template <std::meta::info... Infos>
constexpr std::array<field_info, sizeof...(Infos)> field_table(
    list<field, Infos...>) {
    [[maybe_unused]] std::size_t index = 0;
    return {describe_field<Infos>(index++)...};
}

}  // namespace detail

/// One field_info per field of T, in declaration order.
///
///     static constexpr auto table = reflect::field_table<Packet>();
///     for (const reflect::field_info& field : table) ...
template <typename T, access_context Access = default_access>
constexpr auto field_table() {
    return detail::field_table(fields_of_t<T, Access>{});
}

}  // namespace reflect
