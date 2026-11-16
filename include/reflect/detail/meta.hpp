#pragma once

// Helpers that work on raw std::meta::info. Not part of the public API.

#include "../config.hpp"

#include <array>
#include <cstddef>
#include <initializer_list>
#include <meta>
#include <utility>
#include <vector>

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
