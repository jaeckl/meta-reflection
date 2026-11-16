#pragma once

#include "access.hpp"
#include "annotation.hpp"
#include "config.hpp"
#include "detail/meta.hpp"
#include "field.hpp"
#include "function.hpp"
#include "list.hpp"

#include <meta>
#include <string_view>

namespace reflect {

enum class member_kind {
    field,                ///< non-static data member
    static_field,         ///< static data member
    function,             ///< non-static member function with a name
    static_function,      ///< static member function
    constructor,
    destructor,
    operator_function,    ///< operator==, operator(), ...
    conversion_function,  ///< operator bool, ...
    type,                 ///< nested class, enum or alias
    template_,            ///< member template of any kind
    other,
};

/// Any member of a class, exactly as the language lists it.
///
/// This is the unfiltered view. It includes constructors, operators, nested
/// types, member templates and the special members the compiler declares
/// itself. Use get_fields_of / get_functions_of for the common cases.
template <std::meta::info Info>
struct member : annotated<Info> {
    static consteval member_kind kind() {
        if (std::meta::is_nonstatic_data_member(Info))
            return member_kind::field;
        if (std::meta::is_variable(Info))
            return member_kind::static_field;
        if (std::meta::is_constructor(Info))
            return member_kind::constructor;
        if (std::meta::is_destructor(Info))
            return member_kind::destructor;
        if (std::meta::is_operator_function(Info))
            return member_kind::operator_function;
        if (std::meta::is_conversion_function(Info))
            return member_kind::conversion_function;
        if (std::meta::is_function(Info))
            return std::meta::is_static_member(Info)
                       ? member_kind::static_function
                       : member_kind::function;
        if (std::meta::is_type(Info))
            return member_kind::type;
        if (std::meta::is_template(Info))
            return member_kind::template_;
        return member_kind::other;
    }

    static consteval bool is_field() { return kind() == member_kind::field; }
    /// True for every function, including constructors and operators.
    static consteval bool is_function() { return std::meta::is_function(Info); }

    /// False for constructors, operators, unnamed members and the like.
    static consteval bool has_name() { return std::meta::has_identifier(Info); }
    /// Only valid if has_name().
    static constexpr std::string_view name() {
        return std::meta::identifier_of(Info);
    }
    /// Always available. The text is compiler specific; use it for
    /// diagnostics, not as a key.
    static constexpr std::string_view display_name() {
        return std::meta::display_string_of(Info);
    }

    static consteval bool is_public() { return std::meta::is_public(Info); }
    static consteval bool is_protected() { return std::meta::is_protected(Info); }
    static consteval bool is_private() { return std::meta::is_private(Info); }

    /// The same member as a field proxy. Only if is_field().
    static constexpr auto as_field()
        requires(std::meta::is_nonstatic_data_member(Info))
    {
        return field<Info>{};
    }

    /// The same member as a function proxy. Only if is_function().
    static constexpr auto as_function()
        requires(std::meta::is_function(Info))
    {
        return function<Info>{};
    }
};

template <typename T, access_context Access = default_access>
using members_of_t =
    [:detail::make_list(^^member, std::meta::members_of(^^T, Access)):];

/// Every member of T, unfiltered.
template <typename T, access_context Access = default_access>
constexpr members_of_t<T, Access> get_members_of() {
    return {};
}

/// Calls fn(member) for every member of T.
template <typename T, access_context Access = default_access, typename Fn>
constexpr void for_each_member(Fn&& fn) {
    members_of_t<T, Access>::for_each(fn);
}

}  // namespace reflect
