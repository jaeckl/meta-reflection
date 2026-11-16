#pragma once

#include "config.hpp"
#include "detail/meta.hpp"

#include <array>
#include <cstddef>
#include <meta>
#include <string_view>
#include <tuple>
#include <type_traits>

namespace reflect {

/// What every discovery function returns: a compile-time list of proxies.
///
/// The elements are part of the type, so the object itself is empty. Each
/// element has its own type (field<^^User::id>, field<^^User::name>, ...),
/// which is why the list is walked with for_each or `template for` instead of
/// a normal loop.
template <template <std::meta::info> class Proxy, std::meta::info... Infos>
struct list {
    static constexpr std::size_t size() { return sizeof...(Infos); }
    static constexpr bool empty() { return sizeof...(Infos) == 0; }

    /// The element at position I.
    template <std::size_t I>
        requires(I < sizeof...(Infos))
    static constexpr auto get() {
        return Proxy<detail::nth(I, {Infos...})>{};
    }

    /// Calls fn(proxy) once per element, in declaration order.
    template <typename Fn>
    static constexpr void for_each(Fn&& fn) {
        (static_cast<void>(fn(Proxy<Infos>{})), ...);
    }

    template <typename A>
    using with_t = [:detail::make_list(
                         ^^Proxy,
                         detail::filter_by_annotation<A>({Infos...}, true)):];
    template <typename A>
    using without_t = [:detail::make_list(
                            ^^Proxy,
                            detail::filter_by_annotation<A>({Infos...}, false)):];

    /// Only the elements that carry an annotation of type A.
    template <typename A>
    static constexpr with_t<A> with() {
        return {};
    }

    /// Only the elements that carry no annotation of type A.
    template <typename A>
    static constexpr without_t<A> without() {
        return {};
    }

    /// The element names as a plain array, for ordinary run-time loops.
    static constexpr std::array<std::string_view, sizeof...(Infos)> names() {
        return {Proxy<Infos>::name()...};
    }
};

}  // namespace reflect

// The tuple protocol is what lets `template for` walk a list.
// Inside namespace std a plain `reflect` could one day mean std::reflect,
// hence the leading `::`.
template <template <std::meta::info> class Proxy, std::meta::info... Infos>
struct std::tuple_size<::reflect::list<Proxy, Infos...>>
    : std::integral_constant<std::size_t, sizeof...(Infos)> {};

template <std::size_t I, template <std::meta::info> class Proxy,
          std::meta::info... Infos>
struct std::tuple_element<I, ::reflect::list<Proxy, Infos...>> {
    using type = decltype(::reflect::list<Proxy, Infos...>::template get<I>());
};
