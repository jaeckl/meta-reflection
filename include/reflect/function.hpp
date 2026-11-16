#pragma once

#include "access.hpp"
#include "annotation.hpp"
#include "config.hpp"
#include "detail/meta.hpp"
#include "list.hpp"
#include "string.hpp"

#include <concepts>
#include <cstddef>
#include <meta>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace reflect {

/// A member function, static or not. Each overload is its own function.
template <std::meta::info Info>
    requires(std::meta::is_function(Info))
struct function : annotated<Info> {
    using return_type = [:std::meta::return_type_of(Info):];

    /// The type of parameter I, as declared.
    template <std::size_t I>
    using parameter_type =
        [:std::meta::type_of(std::meta::parameters_of(Info)[I]):];

    static constexpr std::string_view name() {
        return std::meta::identifier_of(Info);
    }

    /// The number of parameters.
    static consteval std::size_t arity() {
        return std::meta::parameters_of(Info).size();
    }

    static consteval bool is_static() {
        return std::meta::is_static_member(Info);
    }
    static consteval bool is_const() { return std::meta::is_const(Info); }
    static consteval bool is_virtual() { return std::meta::is_virtual(Info); }
    static consteval bool is_noexcept() { return std::meta::is_noexcept(Info); }
    static consteval bool is_public() { return std::meta::is_public(Info); }
    static consteval bool is_protected() { return std::meta::is_protected(Info); }
    static consteval bool is_private() { return std::meta::is_private(Info); }

    /// Calls the function on `object`.
    template <typename Object, typename... Args>
        requires(!std::meta::is_static_member(Info))
    static constexpr decltype(auto) invoke(Object&& object, Args&&... args) {
        return std::forward<Object>(object).[:Info:](std::forward<Args>(args)...);
    }

    /// Calls a static member function.
    template <typename... Args>
        requires(std::meta::is_static_member(Info))
    static constexpr decltype(auto) invoke(Args&&... args) {
        return [:Info:](std::forward<Args>(args)...);
    }

    /// Pointer to member function, or a plain function pointer for a static one.
    static constexpr auto pointer() { return &[:Info:]; }
};

namespace detail {

// Named functions only: no constructors, destructors, operators or
// conversion functions, and nothing that was deleted.
consteval std::vector<std::meta::info> functions_of(std::meta::info type,
                                                    access_context access) {
    std::vector<std::meta::info> functions;
    for (std::meta::info member : std::meta::members_of(type, access)) {
        if (std::meta::is_function(member) &&
            std::meta::has_identifier(member) && !std::meta::is_deleted(member))
            functions.push_back(member);
    }
    return functions;
}

consteval std::vector<std::meta::info> functions_named(std::meta::info type,
                                                       std::string_view name) {
    std::vector<std::meta::info> functions;
    for (std::meta::info function : functions_of(type, unchecked_access)) {
        if (std::meta::identifier_of(function) == name)
            functions.push_back(function);
    }
    return functions;
}

consteval std::meta::info function_named(std::meta::info type,
                                         std::string_view name) {
    std::vector<std::meta::info> found = functions_named(type, name);
    if (found.empty())
        throw std::meta::exception(
            "reflect::get_function_of: no function with that name", type);
    if (found.size() > 1)
        throw std::meta::exception(
            "reflect::get_function_of: the name is overloaded; add a "
            "signature, for example get_function_of<T, \"name\", void(int)>()",
            type);
    return found[0];
}

// Return type, parameter types, const and the & / && qualifier have to match.
// noexcept is not part of the comparison.
consteval bool has_signature(std::meta::info function,
                             std::meta::info signature) {
    std::meta::info type = std::meta::type_of(function);
    std::vector<std::meta::info> have = std::meta::parameters_of(type);
    std::vector<std::meta::info> want = std::meta::parameters_of(signature);
    if (have.size() != want.size())
        return false;
    for (std::size_t i = 0; i < have.size(); ++i) {
        if (have[i] != want[i])
            return false;
    }
    return std::meta::return_type_of(type) ==
               std::meta::return_type_of(signature) &&
           std::meta::is_const(type) == std::meta::is_const(signature) &&
           std::meta::is_lvalue_reference_qualified(type) ==
               std::meta::is_lvalue_reference_qualified(signature) &&
           std::meta::is_rvalue_reference_qualified(type) ==
               std::meta::is_rvalue_reference_qualified(signature);
}

consteval std::meta::info function_with_signature(std::meta::info type,
                                                  std::string_view name,
                                                  std::meta::info signature) {
    if (!std::meta::is_function_type(signature))
        throw std::meta::exception(
            "reflect::get_function_of: the third argument has to be a "
            "function type such as int(int) const",
            signature);
    for (std::meta::info function : functions_named(type, name)) {
        if (has_signature(function, signature))
            return function;
    }
    throw std::meta::exception(
        "reflect::get_function_of: no overload with that signature", type);
}

template <typename Class, typename Function>
Class class_of_member_function(Function Class::*);

// Two pointers are the same pointer if they are the same template argument.
// Comparing them with == says the same, but with -fsanitize=undefined GCC 16
// does not accept that comparison in a constant expression.
template <auto Pointer>
struct pointer_tag {};

template <auto Pointer, std::meta::info Info>
consteval bool is_function_pointer_to() {
    if constexpr (std::meta::is_static_member(Info)) {
        return false;
    } else {
        return std::same_as<pointer_tag<&[:Info:]>, pointer_tag<Pointer>>;
    }
}

template <auto Pointer, std::meta::info... Infos>
consteval std::meta::info function_at(list<function, Infos...>) {
    std::meta::info result{};
    bool found = ((is_function_pointer_to<Pointer, Infos>()
                       ? (result = Infos, true)
                       : false) ||
                  ...);
    if (!found)
        throw std::meta::exception(
            "reflect::get_function_of: the pointer does not name a member "
            "function declared by that class",
            ^^void);
    return result;
}

// reflect::invoke<"name"> hands the choice of overload to the compiler. For
// that it needs a real overload set: one call operator per overload, each
// with exactly the parameters of the function it forwards to.

template <std::meta::info Function>
consteval std::meta::info object_parameter_of() {
    std::meta::info type = std::meta::parent_of(Function);
    if (std::meta::is_const(Function))
        type = std::meta::add_const(type);
    return std::meta::is_rvalue_reference_qualified(Function)
               ? std::meta::add_rvalue_reference(type)
               : std::meta::add_lvalue_reference(type);
}

template <std::meta::info Function,
          typename Indices =
              std::make_index_sequence<function<Function>::arity()>>
struct thunk;

template <std::meta::info Function, std::size_t... I>
struct thunk<Function, std::index_sequence<I...>> {
    using object_type = [:object_parameter_of<Function>():];

    constexpr decltype(auto) operator()(
        object_type object,
        typename function<Function>::template parameter_type<I>... args) const {
        return static_cast<object_type>(object).[:Function:](
            static_cast<typename function<Function>::template parameter_type<I>&&>(
                args)...);
    }
};

template <std::meta::info... Functions>
struct overload_set : thunk<Functions>... {
    using thunk<Functions>::operator()...;
};

template <std::meta::info... Functions>
constexpr overload_set<Functions...> make_overload_set(
    list<function, Functions...>) {
    return {};
}

}  // namespace detail

template <typename T, access_context Access = default_access>
using functions_of_t =
    [:detail::make_list(^^function, detail::functions_of(^^T, Access)):];

template <typename T, fixed_string Name>
using functions_named_t =
    [:detail::make_list(^^function,
                        detail::functions_named(^^T, Name.view())):];

/// The named member functions T declares itself, in declaration order.
template <typename T, access_context Access = default_access>
constexpr functions_of_t<T, Access> get_functions_of() {
    return {};
}

/// Every overload with that name: get_functions_of<Shape, "scaled">().
/// Empty if there is none.
template <typename T, fixed_string Name>
constexpr functions_named_t<T, Name> get_functions_of() {
    return {};
}

/// True if T declares a member function with that name.
template <typename T, fixed_string Name>
consteval bool has_function() {
    return !detail::functions_named(^^T, Name.view()).empty();
}

/// The one function with that name: get_function_of<User, "age">().
/// A compile error if there is none, or if the name is overloaded.
template <typename T, fixed_string Name>
constexpr auto get_function_of() {
    return function<detail::function_named(^^T, Name.view())>{};
}

/// One overload, picked by its signature:
///
///     get_function_of<Shape, "scaled", int(double)>()
///     get_function_of<Buffer, "data", const char*() const>()
///
/// The signature is written the way the function is declared, including a
/// trailing const, & or &&. noexcept is left out.
template <typename T, fixed_string Name, typename Signature>
constexpr auto get_function_of() {
    return function<detail::function_with_signature(^^T, Name.view(),
                                                    ^^Signature)>{};
}

/// One function by pointer to member: get_function_of<&User::age>().
/// For an overloaded function the pointer needs a cast that picks one.
template <auto Pointer>
    requires std::is_member_function_pointer_v<decltype(Pointer)>
constexpr auto get_function_of() {
    using Class = decltype(detail::class_of_member_function(Pointer));
    return function<detail::function_at<Pointer>(
        functions_of_t<Class, unchecked_access>{})>{};
}

/// Calls the member function with that name and lets the compiler pick the
/// overload, exactly as object.name(args...) would:
///
///     reflect::invoke<"scaled">(shape, 2.5)
///
/// Differences to a direct call: default arguments are not applied, only
/// functions declared by the class itself are considered, and a by-value
/// parameter costs one extra move.
template <fixed_string Name, typename Object, typename... Args>
constexpr decltype(auto) invoke(Object&& object, Args&&... args) {
    using Overloads = functions_named_t<std::remove_cvref_t<Object>, Name>;
    static_assert(!Overloads::empty(),
                  "reflect::invoke: no member function with that name");
    return detail::make_overload_set(Overloads{})(std::forward<Object>(object),
                                                 std::forward<Args>(args)...);
}

/// Calls fn(function) for every named member function of T.
template <typename T, access_context Access = default_access, typename Fn>
constexpr void for_each_function(Fn&& fn) {
    functions_of_t<T, Access>::for_each(fn);
}

}  // namespace reflect
