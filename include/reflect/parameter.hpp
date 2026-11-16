#pragma once

#include "annotation.hpp"
#include "config.hpp"
#include "function.hpp"

#include <array>
#include <cstddef>
#include <meta>
#include <string_view>
#include <utility>

namespace reflect {

/// One parameter of a member function.
///
/// Identified by the function and a position rather than by a reflection of
/// the parameter itself. GCC 16 crashes while writing debug information for
/// a template that has a parameter reflection as its argument.
template <std::meta::info Function, std::size_t Index>
    requires(std::meta::is_function(Function) &&
             Index < std::meta::parameters_of(Function).size())
struct parameter {
    static consteval std::meta::info native() {
        return std::meta::parameters_of(Function)[Index];
    }

    /// The parameter type as declared.
    using type =
        [:std::meta::type_of(std::meta::parameters_of(Function)[Index]):];

    /// Position in the parameter list, counted from 0.
    static consteval std::size_t index() { return Index; }

    /// False for an unnamed parameter, and for one whose name differs
    /// between two declarations of the function.
    static consteval bool has_name() {
        return std::meta::has_identifier(native());
    }
    static constexpr std::string_view name() {
        return std::meta::identifier_of(native());
    }

    static consteval bool has_default_argument() {
        return std::meta::has_default_argument(native());
    }

    template <typename A>
    static consteval bool has_annotation() {
        return annotated<native()>::template has_annotation<A>();
    }
    template <typename A>
    static consteval A get_annotation() {
        return annotated<native()>::template get_annotation<A>();
    }
    template <typename A>
    static consteval auto find_annotation() {
        return annotated<native()>::template find_annotation<A>();
    }
    template <typename A>
    static consteval auto get_annotations() {
        return annotated<native()>::template get_annotations<A>();
    }
    static constexpr auto annotations() {
        return annotations_of_t<native()>{};
    }
};

namespace detail {

template <std::meta::info Function, typename Fn, std::size_t... I>
constexpr void for_each_parameter(Fn& fn, std::index_sequence<I...>) {
    (static_cast<void>(fn(parameter<Function, I>{})), ...);
}

template <std::meta::info Function, std::size_t... I>
constexpr std::array<std::string_view, sizeof...(I)> parameter_names(
    std::index_sequence<I...>) {
    return {(parameter<Function, I>::has_name()
                 ? parameter<Function, I>::name()
                 : std::string_view{})...};
}

}  // namespace detail

/// A proxy for something that has parameters: a function or a constructor.
template <typename D>
concept has_parameters = descriptor<D> && std::meta::is_function(D::native());

/// Parameter I of a function or constructor: get_parameter_of<0>(function).
template <std::size_t I, has_parameters D>
constexpr parameter<D::native(), I> get_parameter_of(D) {
    return {};
}

/// Calls fn(parameter) for every parameter of a function or constructor,
/// left to right.
template <has_parameters D, typename Fn>
constexpr void for_each_parameter(D, Fn&& fn) {
    detail::for_each_parameter<D::native()>(
        fn, std::make_index_sequence<std::meta::parameters_of(D::native()).size()>{});
}

/// The parameter names of a function or constructor as a plain array. An
/// unnamed parameter gives an empty string.
template <has_parameters D>
constexpr auto get_parameter_names_of(D) {
    return detail::parameter_names<D::native()>(
        std::make_index_sequence<std::meta::parameters_of(D::native()).size()>{});
}

}  // namespace reflect
