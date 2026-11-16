// reflect: a readable API on top of C++26 std::meta.
// https://github.com/jaeckl/meta-reflection
//
// This file is generated from the headers in include/reflect/ by
// tools/amalgamate.py. Do not edit it by hand.

#pragma once

// ---- config.hpp ------------------------------------------------------------

#define REFLECT_VERSION_MAJOR 0
#define REFLECT_VERSION_MINOR 1
#define REFLECT_VERSION_PATCH 0

// Fail early with a readable message instead of a wall of errors from <meta>.
#if !defined(__cpp_impl_reflection)
#error "reflect needs C++26 static reflection. With GCC 16 or newer compile with: -std=c++26 -freflection"
#endif

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <iosfwd>
#include <meta>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

// ---- access.hpp ------------------------------------------------------------

namespace reflect {

/// Decides which members a discovery function may see.
using access_context = std::meta::access_context;

/// Only what code at global scope may access.
inline constexpr access_context public_access = access_context::unprivileged();

/// Every declared member, private and protected included.
inline constexpr access_context unchecked_access = access_context::unchecked();

/// The policy used when a discovery function is called without one.
inline constexpr access_context default_access = unchecked_access;

/// What the calling code may access: public members from outside a class,
/// everything from inside one of its member functions.
consteval access_context current_access(
    access_context context = access_context::current()) {
    return context;
}

/// Satisfied by every proxy type: field, function, base, enumerator, member,
/// annotation and the rest.
template <typename D>
concept descriptor = requires {
    { D::native() } -> std::same_as<std::meta::info>;
};

}  // namespace reflect
// ---- detail/meta.hpp -------------------------------------------------------

// Helpers that work on raw std::meta::info. Not part of the public API.

namespace reflect {

template <template <std::meta::info> class Proxy, std::meta::info... Infos>
struct list;

namespace detail {

// Turns reflections into the type list<Proxy, items...>.
//
// ^^list has to be written out here: inside the class the name refers to the
// current specialization, and substitute() rejects that.
consteval std::meta::info make_list(std::meta::info proxy_template,
                                    const std::vector<std::meta::info>& items) {
    std::vector<std::meta::info> arguments;
    arguments.push_back(proxy_template);
    for (std::meta::info item : items)
        arguments.push_back(std::meta::reflect_constant(item));
    return std::meta::substitute(^^list, arguments);
}

// The list type of the items whose flag in `keep` is set.
consteval std::meta::info make_filtered_list(
    std::meta::info proxy_template, std::initializer_list<std::meta::info> items,
    std::initializer_list<bool> keep) {
    std::vector<std::meta::info> kept;
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (keep.begin()[i])
            kept.push_back(items.begin()[i]);
    }
    return make_list(proxy_template, kept);
}

consteval std::meta::info nth(std::size_t index,
                              std::initializer_list<std::meta::info> items) {
    return items.begin()[index];
}

// std::meta::annotations_of throws for entities that cannot carry an
// annotation, a member template for one. Those simply have none.
consteval bool can_have_annotations(std::meta::info target) {
    return std::meta::is_type(target) || std::meta::is_type_alias(target) ||
           std::meta::is_variable(target) || std::meta::is_function(target) ||
           std::meta::is_function_parameter(target) ||
           std::meta::is_namespace(target) || std::meta::is_enumerator(target) ||
           std::meta::is_base(target) ||
           std::meta::is_nonstatic_data_member(target);
}

consteval std::vector<std::meta::info> annotations(std::meta::info target) {
    if (!can_have_annotations(target))
        return {};
    return std::meta::annotations_of(target);
}

consteval std::vector<std::meta::info> annotations(std::meta::info target,
                                                   std::meta::info type) {
    if (!can_have_annotations(target))
        return {};
    return std::meta::annotations_of_with_type(target, type);
}

template <typename A>
consteval std::size_t annotation_count(std::meta::info target) {
    return annotations(target, ^^A).size();
}

// A pack expansion rather than a loop, so that A needs no default constructor.
template <typename A, std::size_t... I>
consteval std::array<A, sizeof...(I)> annotation_values(
    std::meta::info target, std::index_sequence<I...>) {
    [[maybe_unused]] std::vector<std::meta::info> found = annotations(target, ^^A);
    return {std::meta::extract<A>(found[I])...};
}

// The annotations of type `annotation` on the nearest class that has any:
// `type` itself first, then its base classes, depth first and in the order
// they are declared. Empty if neither the type nor a base class has one.
consteval std::vector<std::meta::info> nearest_annotations(
    std::meta::info type, std::meta::info annotation,
    std::meta::access_context access) {
    std::vector<std::meta::info> found = annotations(type, annotation);
    if (!found.empty())
        return found;
    if (!std::meta::is_class_type(type) || !std::meta::is_complete_type(type))
        return found;
    for (std::meta::info base : std::meta::bases_of(type, access)) {
        found = nearest_annotations(std::meta::type_of(base), annotation, access);
        if (!found.empty())
            return found;
    }
    return found;
}

template <typename A>
consteval std::vector<std::meta::info> filter_by_annotation(
    const std::vector<std::meta::info>& items, bool keep_annotated) {
    std::vector<std::meta::info> kept;
    for (std::meta::info item : items) {
        bool annotated = !annotations(item, ^^A).empty();
        if (annotated == keep_annotated)
            kept.push_back(item);
    }
    return kept;
}

}  // namespace detail
}  // namespace reflect
// ---- list.hpp --------------------------------------------------------------

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

    /// Like for_each, but stops as soon as fn returns false.
    /// Returns true if every element was visited.
    template <typename Fn>
    static constexpr bool for_each_while(Fn&& fn) {
        return (static_cast<bool>(fn(Proxy<Infos>{})) && ...);
    }

    /// True if pred(proxy) is true for at least one element.
    template <typename Pred>
    static constexpr bool any_of(Pred&& pred) {
        return (static_cast<bool>(pred(Proxy<Infos>{})) || ...);
    }

    /// True if pred(proxy) is true for every element. True for an empty list.
    template <typename Pred>
    static constexpr bool all_of(Pred&& pred) {
        return (static_cast<bool>(pred(Proxy<Infos>{})) && ...);
    }

    /// True if pred(proxy) is false for every element.
    template <typename Pred>
    static constexpr bool none_of(Pred&& pred) {
        return !any_of(pred);
    }

    /// The number of elements for which pred(proxy) is true.
    template <typename Pred>
    static constexpr std::size_t count_if(Pred&& pred) {
        return (std::size_t{0} + ... +
                (static_cast<bool>(pred(Proxy<Infos>{})) ? 1u : 0u));
    }

    /// A new list with the elements for which pred(proxy) is true.
    ///
    /// The result is a type, so the answer has to be known at compile time:
    /// pred may only look at the proxy, and a lambda must not capture.
    template <typename Pred>
    static constexpr auto filter([[maybe_unused]] Pred pred) {
        using Result = [:detail::make_filtered_list(
                             ^^Proxy, {Infos...},
                             {static_cast<bool>(pred(Proxy<Infos>{}))...}):];
        return Result{};
    }

    /// Calls fn(proxy) for every element and returns the results as a tuple.
    template <typename Fn>
    static constexpr auto map(Fn&& fn) {
        return std::tuple{fn(Proxy<Infos>{})...};
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
// ---- annotation.hpp --------------------------------------------------------

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

/// The annotation of type A, or `fallback` if there is none:
///
///     int width = reflect::get_annotation_or<Width>(field, Width{10}).value;
template <typename A, typename T>
consteval A get_annotation_or(A fallback) {
    std::optional<A> found = annotated<^^T>::template find_annotation<A>();
    return found ? *found : fallback;
}
template <typename A, descriptor D>
consteval A get_annotation_or(D, A fallback) {
    std::optional<A> found = annotated<D::native()>::template find_annotation<A>();
    return found ? *found : fallback;
}

template <typename A, typename T>
consteval auto get_annotations() {
    return annotated<^^T>::template get_annotations<A>();
}
template <typename A, descriptor D>
consteval auto get_annotations(D) {
    return annotated<D::native()>::template get_annotations<A>();
}

// ---------------------------------------------------------------------------
// Annotations of a class or of one of its base classes.
//
// An annotation belongs to the class it is written on; a derived class does
// not have it. These functions look further: at T first, then at its base
// classes, depth first and in the order they are declared. The nearest class
// that carries an annotation of type A answers, so a derived class can
// replace what a base class says.
// ---------------------------------------------------------------------------

/// True if T or one of its base classes has an annotation of type A.
template <typename A, typename T, access_context Access = default_access>
consteval bool has_inherited_annotation() {
    return !detail::nearest_annotations(^^T, ^^A, Access).empty();
}

/// The annotation of type A on T or, if T has none, on the nearest base
/// class that has one. A compile error if there is none, or if that class
/// has more than one.
template <typename A, typename T, access_context Access = default_access>
consteval A get_inherited_annotation() {
    auto found = detail::nearest_annotations(^^T, ^^A, Access);
    if (found.empty())
        throw std::meta::exception(
            "reflect::get_inherited_annotation: neither the class nor a base "
            "class has an annotation of this type",
            ^^T);
    if (found.size() != 1)
        throw std::meta::exception(
            "reflect::get_inherited_annotation: more than one annotation of "
            "this type on the nearest class that has one",
            ^^T);
    return std::meta::extract<A>(found[0]);
}

/// Like get_inherited_annotation, but std::nullopt if there is none.
template <typename A, typename T, access_context Access = default_access>
consteval std::optional<A> find_inherited_annotation() {
    auto found = detail::nearest_annotations(^^T, ^^A, Access);
    if (found.empty())
        return std::nullopt;
    if (found.size() != 1)
        throw std::meta::exception(
            "reflect::find_inherited_annotation: more than one annotation of "
            "this type on the nearest class that has one",
            ^^T);
    return std::meta::extract<A>(found[0]);
}

/// Like get_inherited_annotation, but `fallback` if there is none.
template <typename A, typename T, access_context Access = default_access>
consteval A get_inherited_annotation_or(A fallback) {
    std::optional<A> found = find_inherited_annotation<A, T, Access>();
    return found ? *found : fallback;
}

/// A type that carries an annotation of type A. Lets a function accept only
/// types that opted in:
///
///     template <reflect::annotated_with<Serializable> T>
///     void save(const T& object);
template <typename T, typename A>
concept annotated_with = has_annotation<A, T>();

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
// ---- base.hpp --------------------------------------------------------------

namespace reflect {

/// A direct base class of a type.
template <std::meta::info Info>
    requires(std::meta::is_base(Info))
struct base : annotated<Info> {
    /// The base class itself. Pass it back in to keep going:
    /// reflect::get_fields_of<typename decltype(base)::type>().
    using type = [:std::meta::type_of(Info):];

    static consteval bool is_virtual() { return std::meta::is_virtual(Info); }
    static consteval bool is_public() { return std::meta::is_public(Info); }
    static consteval bool is_protected() { return std::meta::is_protected(Info); }
    static consteval bool is_private() { return std::meta::is_private(Info); }

    /// The base class part of `object`.
    template <typename Object>
    static constexpr decltype(auto) get(Object&& object) {
        return (std::forward<Object>(object).[:Info:]);
    }
};

template <typename T, access_context Access = default_access>
using bases_of_t =
    [:detail::make_list(^^base, std::meta::bases_of(^^T, Access)):];

/// The direct base classes of T, in declaration order.
template <typename T, access_context Access = default_access>
constexpr bases_of_t<T, Access> get_bases_of() {
    return {};
}

/// Calls fn(base) for every direct base class of T.
template <typename T, access_context Access = default_access, typename Fn>
constexpr void for_each_base(Fn&& fn) {
    bases_of_t<T, Access>::for_each(fn);
}

}  // namespace reflect
// ---- string.hpp ------------------------------------------------------------

namespace reflect {

/// Text that can be stored in an annotation.
///
/// An annotation value has to be a structural type, which rules out
/// std::string and std::string_view. static_string keeps the characters in
/// static storage and stays structural:
///
///     struct Rename { reflect::static_string value; };
///     struct User { [[=Rename{"display name"}]] std::string name; };
struct static_string {
    const char* data;
    std::size_t size;

    consteval static_string(const char* text)
        : data(std::define_static_string(std::string_view(text))),
          size(std::string_view(text).size()) {}

    consteval static_string(std::string_view text)
        : data(std::define_static_string(text)), size(text.size()) {}

    constexpr std::string_view view() const { return {data, size}; }
    constexpr const char* c_str() const { return data; }
    constexpr operator std::string_view() const { return view(); }

    friend constexpr bool operator==(static_string left, static_string right) {
        return left.view() == right.view();
    }

    /// std::cout << annotation.text
    template <typename Traits>
    friend std::basic_ostream<char, Traits>& operator<<(
        std::basic_ostream<char, Traits>& out, static_string text) {
        return out << text.view();
    }
};

/// A string literal usable as a template argument, as in
/// reflect::get_field_of<User, "id">().
template <std::size_t N>
struct fixed_string {
    char text[N]{};

    constexpr fixed_string(const char (&literal)[N]) {
        std::copy_n(literal, N, text);
    }

    constexpr std::string_view view() const { return {text, N - 1}; }
};

}  // namespace reflect
// ---- field.hpp -------------------------------------------------------------

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
// ---- builder.hpp -----------------------------------------------------------

// Experimental: a builder for any class, without generated code.

namespace reflect {

/// Marks a field that a reflect::builder has to be given a value for:
///
///     [[=reflect::required]] std::string name;
struct required_t {};
inline constexpr required_t required{};

/// Asks the code generator for a builder class, UserBuilder for a class
/// User. Every setter is called `prefix` plus the name of its field:
///
///     struct [[=reflect::generate_builder{}]] User { ... };
///     struct [[=reflect::generate_builder{"set_"}]] Order { ... };
///
/// Read by reflect::codegen::builder_processor; without a generator run the
/// annotation does nothing.
struct generate_builder {
    static_string prefix = "with_";
};

namespace detail {

// The field called `name` in `type` or, if it declares none, in one of its
// base classes. The null reflection if there is none.
consteval std::meta::info find_field_in_hierarchy(std::meta::info type,
                                                  std::string_view name) {
    for (std::meta::info member :
         std::meta::nonstatic_data_members_of(type, unchecked_access)) {
        if (std::meta::has_identifier(member) &&
            std::meta::identifier_of(member) == name)
            return member;
    }
    for (std::meta::info base : std::meta::bases_of(type, unchecked_access)) {
        std::meta::info found =
            find_field_in_hierarchy(std::meta::type_of(base), name);
        if (found != std::meta::info{})
            return found;
    }
    return std::meta::info{};
}

consteval std::meta::info builder_field(std::meta::info type,
                                        std::string_view name) {
    std::meta::info found = find_field_in_hierarchy(type, name);
    if (found == std::meta::info{})
        throw std::meta::exception(
            "reflect::builder::set: no field with that name", type);
    return found;
}

// Throws, with the name of the field in the message, if a field marked
// [[=reflect::required]] is not among those that were set.
consteval bool check_required_fields(std::meta::info type,
                                     std::initializer_list<std::meta::info> set) {
    for (std::meta::info member : all_fields_of(type, unchecked_access)) {
        if (annotations(member, ^^required_t).empty())
            continue;
        bool was_set = false;
        for (std::meta::info given : set)
            was_set = was_set || given == member;
        if (!was_set)
            throw std::meta::exception(
                std::string("reflect::builder::build: the required field '") +
                    std::string(std::meta::identifier_of(member)) +
                    "' was not set",
                type);
    }
    return true;
}

}  // namespace detail

/// Builds a T step by step, one named field at a time:
///
///     User user = reflect::builder<User>()
///                     .set<"name">("Ada")
///                     .set<"id">(7)
///                     .build();
///
/// The names are checked while compiling. So is the rule that every field
/// marked [[=reflect::required]] has been set before build(), and that no
/// field is set twice. The price for those two checks is that every set()
/// returns a builder of a new type: keep the calls in one chain, or store
/// the steps in `auto` variables.
///
/// Fields that are not set keep the value a default-constructed T gives
/// them. T therefore needs a default constructor.
template <typename T, std::meta::info... Set>
class builder {
    static_assert(std::default_initializable<T>,
                  "reflect::builder: the class needs a default constructor");

public:
    constexpr builder() = default;

    /// Gives the field called Name a value. Fields of base classes count.
    template <fixed_string Name, typename Value>
    [[nodiscard]] constexpr auto set(this builder self, Value&& value) {
        constexpr std::meta::info target = detail::builder_field(^^T, Name.view());
        static_assert(!((Set == target) || ...),
                      "reflect::builder::set: this field was already set");
        static_assert(
            std::is_assignable_v<typename field<target>::type&, Value&&>,
            "reflect::builder::set: the value cannot be assigned to this field");
        field<target>::set(self.object_, std::forward<Value>(value));
        return builder<T, Set..., target>(std::move(self.object_));
    }

    /// The finished object. A compile error if a required field is missing.
    [[nodiscard]] constexpr T build(this builder self) {
        static_assert(detail::check_required_fields(^^T, {Set...}));
        return std::move(self.object_);
    }

private:
    template <typename, std::meta::info...>
    friend class builder;

    constexpr explicit builder(T&& object) : object_(std::move(object)) {}

    T object_{};
};

}  // namespace reflect
// ---- function.hpp ----------------------------------------------------------

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

template <auto Pointer, std::meta::info Info>
consteval bool is_function_pointer_to() {
    if constexpr (std::meta::is_static_member(Info)) {
        return false;
    } else if constexpr (!std::same_as<decltype(&[:Info:]), decltype(Pointer)>) {
        return false;
    } else {
        return &[:Info:] == Pointer;
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
// ---- parameter.hpp ---------------------------------------------------------

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
// ---- constructor.hpp -------------------------------------------------------

namespace reflect {

namespace detail {

// A constructor cannot be spliced the way a member function can, so calling
// one specific constructor needs a detour: a function with exactly the
// parameters of that constructor. Arguments convert at its boundary, and the
// call inside then matches the constructor exactly.
template <std::meta::info Constructor,
          typename Indices = std::make_index_sequence<
              std::meta::parameters_of(Constructor).size()>>
struct constructor_thunk;

template <std::meta::info Constructor, std::size_t... I>
struct constructor_thunk<Constructor, std::index_sequence<I...>> {
    using type = [:std::meta::parent_of(Constructor):];

    static constexpr type call(
        typename parameter<Constructor, I>::type... arguments) {
        return type(
            static_cast<typename parameter<Constructor, I>::type&&>(arguments)...);
    }
};

}  // namespace detail

/// A constructor of a class.
template <std::meta::info Info>
    requires(std::meta::is_constructor(Info))
struct constructor : annotated<Info> {
    /// The class the constructor belongs to, which is what it builds.
    using declaring_type = [:std::meta::parent_of(Info):];

    /// The type of parameter I, as declared.
    template <std::size_t I>
    using parameter_type =
        [:std::meta::type_of(std::meta::parameters_of(Info)[I]):];

    /// The number of parameters.
    static consteval std::size_t arity() {
        return std::meta::parameters_of(Info).size();
    }

    /// True if the constructor can be called without arguments.
    static consteval bool is_default() {
        return std::meta::is_default_constructor(Info);
    }
    static consteval bool is_copy() { return std::meta::is_copy_constructor(Info); }
    static consteval bool is_move() { return std::meta::is_move_constructor(Info); }
    static consteval bool is_explicit() { return std::meta::is_explicit(Info); }
    static consteval bool is_noexcept() { return std::meta::is_noexcept(Info); }
    static consteval bool is_public() { return std::meta::is_public(Info); }
    static consteval bool is_protected() { return std::meta::is_protected(Info); }
    static consteval bool is_private() { return std::meta::is_private(Info); }

    /// Builds an object with exactly this constructor. The arguments are
    /// converted to the parameter types as in an ordinary call; which
    /// constructor runs is already decided.
    ///
    /// Only for public constructors. Reflection can read a private field,
    /// but it cannot call a private constructor.
    template <typename... Args>
        requires(sizeof...(Args) == std::meta::parameters_of(Info).size())
    static constexpr declaring_type invoke(Args&&... args) {
        static_assert(std::meta::is_public(Info),
                      "reflect::constructor::invoke: only a public constructor "
                      "can be called");
        return detail::constructor_thunk<Info>::call(std::forward<Args>(args)...);
    }
};

namespace detail {

// Copy and move constructors are left out: they are not a way to build an
// object from its parts. So is anything deleted.
consteval std::vector<std::meta::info> constructors_of(std::meta::info type,
                                                       access_context access) {
    std::vector<std::meta::info> constructors;
    for (std::meta::info member : std::meta::members_of(type, access)) {
        if (std::meta::is_constructor(member) &&
            !std::meta::is_copy_constructor(member) &&
            !std::meta::is_move_constructor(member) &&
            !std::meta::is_deleted(member))
            constructors.push_back(member);
    }
    return constructors;
}

// The parameter type the way a caller would write it: no alias, and no
// top-level const, which is not part of a signature.
consteval std::meta::info plain_parameter_type(std::meta::info type) {
    return std::meta::remove_cv(std::meta::dealias(type));
}

consteval std::meta::info constructor_with(
    std::meta::info type, const std::vector<std::meta::info>& wanted) {
    for (std::meta::info candidate : constructors_of(type, unchecked_access)) {
        std::vector<std::meta::info> parameters = std::meta::parameters_of(candidate);
        if (parameters.size() != wanted.size())
            continue;
        bool same = true;
        for (std::size_t i = 0; i < parameters.size(); ++i) {
            if (plain_parameter_type(std::meta::type_of(parameters[i])) !=
                plain_parameter_type(wanted[i]))
                same = false;
        }
        if (same)
            return candidate;
    }
    throw std::meta::exception(
        "reflect::get_constructor_of: no constructor with these parameter types",
        type);
}

// The constructor reflect::construct<T>(fn) uses: the only public one that
// takes arguments.
consteval std::meta::info only_constructor(std::meta::info type) {
    std::meta::info found{};
    std::size_t count = 0;
    for (std::meta::info candidate : constructors_of(type, public_access)) {
        if (std::meta::parameters_of(candidate).empty())
            continue;
        found = candidate;
        ++count;
    }
    if (count != 1)
        throw std::meta::exception(
            "reflect::construct: the class needs exactly one public constructor "
            "that takes arguments; otherwise pick one, for example "
            "construct(get_constructor_of<T, int, std::string>(), fn)",
            type);
    return found;
}

template <std::meta::info Constructor, typename Fn, std::size_t... I>
constexpr typename constructor<Constructor>::declaring_type construct(
    Fn& fn, std::index_sequence<I...>) {
    // The arguments of a call are evaluated in no particular order, the
    // elements of a braced list from left to right. A callback that reads
    // from a stream depends on that.
    [[maybe_unused]] std::tuple<decltype(fn(parameter<Constructor, I>{}))...> arguments{
        fn(parameter<Constructor, I>{})...};
    return constructor<Constructor>::invoke(std::get<I>(std::move(arguments))...);
}

}  // namespace detail

template <typename T, access_context Access = default_access>
using constructors_of_t =
    [:detail::make_list(^^constructor, detail::constructors_of(^^T, Access)):];

/// The constructors of T, in declaration order, without the copy and the
/// move constructor. Includes the default constructor the compiler declares.
template <typename T, access_context Access = default_access>
constexpr constructors_of_t<T, Access> get_constructors_of() {
    return {};
}

/// One constructor, picked by its parameter types:
///
///     get_constructor_of<User, int, std::string>()   // User(int, std::string)
///     get_constructor_of<User>()                     // User()
///
/// A compile error if T has no such constructor.
template <typename T, typename... Parameters>
constexpr auto get_constructor_of() {
    return constructor<detail::constructor_with(^^T, {^^Parameters...})>{};
}

/// Calls fn(constructor) for every constructor of T.
template <typename T, access_context Access = default_access, typename Fn>
constexpr void for_each_constructor(Fn&& fn) {
    constructors_of_t<T, Access>::for_each(fn);
}

/// Builds an object by asking fn(parameter) for every argument of a
/// constructor, from left to right:
///
///     reflect::construct(reflect::get_constructor_of<User, int, std::string>(),
///                        [&](auto parameter) { return read(parameter.name()); });
template <std::meta::info Constructor, typename Fn>
constexpr typename constructor<Constructor>::declaring_type construct(
    constructor<Constructor>, Fn&& fn) {
    return detail::construct<Constructor>(
        fn, std::make_index_sequence<constructor<Constructor>::arity()>{});
}

/// Like the form above, for a class that has exactly one public constructor
/// that takes arguments: reflect::construct<Connection>(fn).
///
/// This is how to build a class that reflect::make cannot: one that is not
/// an aggregate and has no default constructor.
template <typename T, typename Fn>
    requires std::is_class_v<T>
constexpr T construct(Fn&& fn) {
    return detail::construct<detail::only_constructor(^^T)>(
        fn, std::make_index_sequence<
                std::meta::parameters_of(detail::only_constructor(^^T)).size()>{});
}

}  // namespace reflect
// ---- enumerator.hpp --------------------------------------------------------

namespace reflect {

/// One enumerator of an enumeration.
template <std::meta::info Info>
    requires(std::meta::is_enumerator(Info))
struct enumerator : annotated<Info> {
    /// The enumeration type.
    using type = [:std::meta::type_of(Info):];

    static constexpr std::string_view name() {
        return std::meta::identifier_of(Info);
    }
    static constexpr type value() { return std::meta::extract<type>(Info); }
};

template <typename E>
    requires std::is_enum_v<E>
using enumerators_of_t =
    [:detail::make_list(^^enumerator, std::meta::enumerators_of(^^E)):];

/// The enumerators of E, in declaration order.
template <typename E>
    requires std::is_enum_v<E>
constexpr enumerators_of_t<E> get_enumerators_of() {
    return {};
}

/// Calls fn(enumerator) for every enumerator of E.
template <typename E, typename Fn>
    requires std::is_enum_v<E>
constexpr void for_each_enumerator(Fn&& fn) {
    enumerators_of_t<E>::for_each(fn);
}

namespace detail {

template <typename E, std::meta::info... Infos>
constexpr std::array<E, sizeof...(Infos)> enum_values(list<enumerator, Infos...>) {
    return {enumerator<Infos>::value()...};
}

}  // namespace detail

/// The number of enumerators of E.
template <typename E>
    requires std::is_enum_v<E>
constexpr std::size_t enum_count() {
    return enumerators_of_t<E>::size();
}

/// All values of E as an array, in declaration order.
template <typename E>
    requires std::is_enum_v<E>
constexpr std::array<E, enumerators_of_t<E>::size()> enum_values() {
    return detail::enum_values<E>(enumerators_of_t<E>{});
}

/// All enumerator names of E as an array, in declaration order.
template <typename E>
    requires std::is_enum_v<E>
constexpr std::array<std::string_view, enumerators_of_t<E>::size()> enum_names() {
    return enumerators_of_t<E>::names();
}

/// The name of an enumerator: enum_name(Role::Admin) == "Admin".
/// Empty if no enumerator has that value. If several enumerators share the
/// value, the first one declared wins.
template <typename E>
    requires std::is_enum_v<E>
constexpr std::string_view enum_name(E value) {
    std::string_view result;
    for_each_enumerator<E>([&](auto e) {
        if (result.empty() && e.value() == value)
            result = e.name();
    });
    return result;
}

/// The enumerator with that name, or nullopt. The match is exact and case
/// sensitive.
template <typename E>
    requires std::is_enum_v<E>
constexpr std::optional<E> enum_cast(std::string_view name) {
    std::optional<E> result;
    for_each_enumerator<E>([&](auto e) {
        if (!result && e.name() == name)
            result = e.value();
    });
    return result;
}

}  // namespace reflect
// ---- static_field.hpp ------------------------------------------------------

namespace reflect {

/// A static data member. Like field, but there is no object to pass in.
template <std::meta::info Info>
    requires(std::meta::is_variable(Info) && std::meta::is_static_member(Info))
struct static_field : annotated<Info> {
    /// The declared type of the member. Keeps const.
    using type = [:std::meta::type_of(Info):];
    /// The class that declares the member.
    using declaring_type = [:std::meta::parent_of(Info):];

    static constexpr std::string_view name() {
        return std::meta::identifier_of(Info);
    }

    static consteval bool is_public() { return std::meta::is_public(Info); }
    static consteval bool is_protected() { return std::meta::is_protected(Info); }
    static consteval bool is_private() { return std::meta::is_private(Info); }

    /// A reference to the variable.
    static constexpr type& get() { return [:Info:]; }

    template <typename Value>
    static constexpr void set(Value&& value) {
        [:Info:] = std::forward<Value>(value);
    }

    /// A plain pointer to the variable.
    static constexpr auto pointer() { return &[:Info:]; }
};

template <typename T, access_context Access = default_access>
using static_fields_of_t =
    [:detail::make_list(^^static_field,
                        std::meta::static_data_members_of(^^T, Access)):];

/// The static data members T declares, in declaration order.
template <typename T, access_context Access = default_access>
constexpr static_fields_of_t<T, Access> get_static_fields_of() {
    return {};
}

/// Calls fn(static_field) for every static data member of T.
template <typename T, access_context Access = default_access, typename Fn>
constexpr void for_each_static_field(Fn&& fn) {
    static_fields_of_t<T, Access>::for_each(fn);
}

}  // namespace reflect
// ---- member.hpp ------------------------------------------------------------

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

    /// The same member as a static field proxy. Only for static data members.
    static constexpr auto as_static_field()
        requires(std::meta::is_variable(Info))
    {
        return static_field<Info>{};
    }

    /// The same member as a function proxy. Only if is_function().
    static constexpr auto as_function()
        requires(std::meta::is_function(Info))
    {
        return function<Info>{};
    }

    /// The same member as a constructor proxy. Only for constructors.
    static constexpr auto as_constructor()
        requires(std::meta::is_constructor(Info))
    {
        return constructor<Info>{};
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
// ---- namespace.hpp ---------------------------------------------------------

// Experimental: the classes of a namespace.

namespace reflect {

namespace detail {

template <std::meta::info... Types>
struct class_list {
    template <typename Fn>
    static constexpr void for_each(Fn& fn) {
        (static_cast<void>(fn(std::type_identity<typename [:Types:]>{})), ...);
    }
};

consteval std::meta::info classes_in(std::meta::info scope) {
    std::vector<std::meta::info> arguments;
    for (std::meta::info member : std::meta::members_of(scope, unchecked_access)) {
        // An alias would report its class a second time.
        if (!std::meta::is_type(member) || std::meta::is_type_alias(member))
            continue;
        if (!std::meta::is_class_type(member) || !std::meta::is_complete_type(member))
            continue;
        arguments.push_back(std::meta::reflect_constant(member));
    }
    return std::meta::substitute(^^class_list, arguments);
}

}  // namespace detail

/// Calls fn(std::type_identity<T>{}) for every class T declared directly in
/// a namespace:
///
///     reflect::for_each_class_in<^^model>([]<typename T>(std::type_identity<T>) {
///         if constexpr (reflect::has_annotation<Entity, T>())
///             std::cout << reflect::name_of<T>() << '\n';
///     });
///
/// Only complete, non-template classes are reported, and nested namespaces
/// are not searched. The compiler knows the classes it has seen so far in
/// the current source file: call this after the headers that declare them.
template <std::meta::info Namespace, typename Fn>
    requires(std::meta::is_namespace(Namespace))
constexpr void for_each_class_in(Fn&& fn) {
    using classes = [:detail::classes_in(Namespace):];
    classes::for_each(fn);
}

}  // namespace reflect
// ---- object.hpp ------------------------------------------------------------

// Helpers that work on whole objects rather than on one member.

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
// ---- table.hpp -------------------------------------------------------------

// Field metadata as plain data, for code that wants an ordinary array and an
// ordinary loop instead of templates.

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
// ---- type.hpp --------------------------------------------------------------

namespace reflect {

namespace detail {

// "outer::inner::Name", built from the enclosing namespaces and classes.
consteval const char* qualified_name(std::meta::info type) {
    if (!std::meta::has_identifier(type))
        return std::define_static_string(std::meta::display_string_of(type));
    std::string name(std::meta::identifier_of(type));
    for (std::meta::info scope = std::meta::parent_of(type); scope != ^^::;
         scope = std::meta::parent_of(scope)) {
        // A function, for a local class, or an unnamed namespace: the type
        // cannot be named from outside such a scope, so the scopes are
        // left out.
        bool nameable = std::meta::is_namespace(scope) || std::meta::is_type(scope);
        if (!nameable || !std::meta::has_identifier(scope))
            break;
        name = std::string(std::meta::identifier_of(scope)) + "::" + name;
    }
    return std::define_static_string(name);
}

}  // namespace detail

/// The name of a type: "User" for struct User.
///
/// A type without a plain identifier (int, Box<int>, a pointer) gets the
/// compiler's display text instead, which differs between compilers.
template <typename T>
constexpr std::string_view name_of() {
    return std::meta::has_identifier(^^T) ? std::meta::identifier_of(^^T)
                                         : std::meta::display_string_of(^^T);
}

/// The name of a type with the namespaces and classes around it:
/// "shop::User" for struct User in namespace shop. This is the name to
/// write in generated code.
///
/// Like name_of, a type without a plain identifier gets the compiler's
/// display text.
template <typename T>
constexpr std::string_view qualified_name_of() {
    return detail::qualified_name(^^T);
}

}  // namespace reflect
// ---- walk.hpp --------------------------------------------------------------

// Walking an object together with the objects nested inside it.

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
