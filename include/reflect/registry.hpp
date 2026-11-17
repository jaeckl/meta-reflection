#pragma once

// Experimental: the classes of a project that carry an annotation, found
// by a code generator and used by ordinary code.

#include "config.hpp"
#include "type.hpp"

#include <concepts>
#include <cstddef>
#include <memory>
#include <string_view>
#include <type_traits>

namespace reflect {

/// Marks a class for the registry:
///
///     struct [[=reflect::registered]] Circle : Shape { ... };
///
/// The annotation does nothing by itself. A build that runs
/// reflect::codegen::registry_processor<> collects the marked classes in
/// a generated header, and the functions below read them from there.
struct registered_t {};
inline constexpr registered_t registered{};

/// The classes that carry the annotation `Annotation`. The library only
/// declares this template. The definition is written by the registry
/// processor into the generated header, so the functions below need that
/// header to be included.
template <typename Annotation>
struct registry;

namespace detail {

template <typename Annotation>
concept has_registry = requires { sizeof(registry<Annotation>); };

template <typename... Types>
struct registered_pack {
    template <typename Fn>
    static constexpr void for_each(Fn& fn) {
        (static_cast<void>(fn(type_info<^^Types>{})), ...);
    }
};

}  // namespace detail

/// A template filled with the registered classes, in the order the
/// generator found them:
///
///     using AnyShape = reflect::registered_classes<std::variant>;
///     // std::variant<shop::Circle, shop::Square>
///
/// The second argument is the annotation, for a registry of your own:
/// reflect::registered_classes<std::variant, Component>.
template <template <typename...> typename Template, typename Annotation = registered_t>
using registered_classes = typename registry<Annotation>::template apply<Template>;

/// Calls fn(type_info<...>{}) for every registered class:
///
///     reflect::for_each_registered([](auto cls) {
///         std::cout << cls.qualified_name() << '\n';
///     });
///
/// For a registry of your own: reflect::for_each_registered<Component>(fn).
template <typename Annotation = registered_t, typename Fn>
constexpr void for_each_registered(Fn&& fn) {
    static_assert(detail::has_registry<Annotation>,
                  "reflect::for_each_registered: there is no registry for this "
                  "annotation here; include the header that "
                  "reflect::codegen::registry_processor generated for it");
    if constexpr (detail::has_registry<Annotation>) {
        using classes = registered_classes<detail::registered_pack, Annotation>;
        classes::for_each(fn);
    }
}

/// Creates an object of the registered class with this name, which is only
/// known when the program runs:
///
///     std::unique_ptr<Shape> shape = reflect::create_registered<Shape>("shop::Circle");
///
/// The name is the one reflect::qualified_name_of gives. The result is
/// empty if no registered class has that name, or if the class is not a
/// `Base` or has no default constructor.
///
/// For a registry of your own: reflect::create_registered<Shape, Component>(name).
template <typename Base, typename Annotation = registered_t>
std::unique_ptr<Base> create_registered(std::string_view name) {
    std::unique_ptr<Base> result;
    for_each_registered<Annotation>([&](auto cls) {
        using T = typename decltype(cls)::type;
        if constexpr (std::derived_from<T, Base> && std::default_initializable<T>) {
            static_assert(std::same_as<T, Base> || std::has_virtual_destructor_v<Base>,
                          "reflect::create_registered: the base class needs a "
                          "virtual destructor");
            if (!result && name == cls.qualified_name())
                result = std::make_unique<T>();
        }
    });
    return result;
}

}  // namespace reflect
