#pragma once

#include "access.hpp"
#include "config.hpp"
#include "detail/meta.hpp"
#include "list.hpp"

#include <array>
#include <cstddef>
#include <meta>
#include <optional>
#include <type_traits>
#include <utility>

namespace reflect {

/// One annotation on a declaration, for example the Column{7} in
/// `[[=Column{7}]] int id;`.
template <std::meta::info Info>
struct annotation {
    /// The annotation's type, without top-level const.
    using type = std::remove_const_t<typename [:std::meta::type_of(Info):]>;

    static consteval std::meta::info native() { return Info; }

    /// True if the annotation is of type A.
    template <typename A>
    static consteval bool is() {
        return std::meta::remove_const(std::meta::type_of(Info)) ==
               std::meta::remove_const(^^A);
    }

    /// The annotation value.
    static constexpr type value() { return std::meta::extract<type>(Info); }
};

/// The list type holding every annotation of one declaration.
template <std::meta::info Target>
using annotations_of_t =
    [:detail::make_list(^^annotation, detail::annotations(Target)):];

/// Annotation queries shared by the proxies that can carry annotations.
template <std::meta::info Info>
struct annotated {
    /// The raw reflection, for anything this library does not wrap.
    static consteval std::meta::info native() { return Info; }

    /// True if there is at least one annotation of type A.
    template <typename A>
    static consteval bool has_annotation() {
        return detail::annotation_count<A>(Info) != 0;
    }

    /// The one annotation of type A. A compile error if there is none or
    /// more than one.
    template <typename A>
    static consteval A get_annotation() {
        auto found = detail::annotations(Info, ^^A);
        if (found.size() != 1)
            throw std::meta::exception(
                "reflect::get_annotation: expected exactly one annotation of this type",
                Info);
        return std::meta::extract<A>(found[0]);
    }

    /// The annotation of type A, or std::nullopt if there is none. A compile
    /// error if there is more than one.
    template <typename A>
    static consteval std::optional<A> find_annotation() {
        auto found = detail::annotations(Info, ^^A);
        if (found.empty())
            return std::nullopt;
        if (found.size() != 1)
            throw std::meta::exception(
                "reflect::find_annotation: more than one annotation of this type",
                Info);
        return std::meta::extract<A>(found[0]);
    }

    /// Every annotation of type A, in declaration order. May be empty.
    template <typename A>
    static consteval std::array<A, detail::annotation_count<A>(Info)>
    get_annotations() {
        return detail::annotation_values<A>(
            Info, std::make_index_sequence<detail::annotation_count<A>(Info)>{});
    }

    /// Every annotation, whatever its type, as a list of annotation proxies.
    static constexpr auto annotations() { return annotations_of_t<Info>{}; }
};

// ---------------------------------------------------------------------------
// Free functions. Each has a form for a type, has_annotation<A, T>(), and a
// form for a proxy, has_annotation<A>(field). The annotation type comes first.
// ---------------------------------------------------------------------------

template <typename A, typename T>
consteval bool has_annotation() {
    return annotated<^^T>::template has_annotation<A>();
}
template <typename A, descriptor D>
consteval bool has_annotation(D) {
    return annotated<D::native()>::template has_annotation<A>();
}

template <typename A, typename T>
consteval A get_annotation() {
    return annotated<^^T>::template get_annotation<A>();
}
template <typename A, descriptor D>
consteval A get_annotation(D) {
    return annotated<D::native()>::template get_annotation<A>();
}

template <typename A, typename T>
consteval std::optional<A> find_annotation() {
    return annotated<^^T>::template find_annotation<A>();
}
template <typename A, descriptor D>
consteval std::optional<A> find_annotation(D) {
    return annotated<D::native()>::template find_annotation<A>();
}

template <typename A, typename T>
consteval auto get_annotations() {
    return annotated<^^T>::template get_annotations<A>();
}
template <typename A, descriptor D>
consteval auto get_annotations(D) {
    return annotated<D::native()>::template get_annotations<A>();
}

/// Every annotation on the type T, as a list of annotation proxies.
template <typename T>
constexpr annotations_of_t<^^T> get_annotations_of() {
    return {};
}
/// Every annotation on the entity behind a proxy.
template <descriptor D>
constexpr annotations_of_t<D::native()> get_annotations_of(D) {
    return {};
}

/// Calls fn(annotation) for every annotation on the type T.
template <typename T, typename Fn>
constexpr void for_each_annotation(Fn&& fn) {
    annotations_of_t<^^T>::for_each(fn);
}
/// Calls fn(annotation) for every annotation on the entity behind a proxy.
template <descriptor D, typename Fn>
constexpr void for_each_annotation(D, Fn&& fn) {
    annotations_of_t<D::native()>::for_each(fn);
}

/// Calls fn(value) for every annotation of type A on the type T.
template <typename A, typename T, typename Fn>
constexpr void for_each_annotation(Fn&& fn) {
    for (const A& value : annotated<^^T>::template get_annotations<A>())
        fn(value);
}
/// Calls fn(value) for every annotation of type A on the entity behind a proxy.
template <typename A, descriptor D, typename Fn>
constexpr void for_each_annotation(D, Fn&& fn) {
    for (const A& value : annotated<D::native()>::template get_annotations<A>())
        fn(value);
}

}  // namespace reflect
