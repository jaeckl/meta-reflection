#pragma once

// Helpers that work on raw std::meta::info. Not part of the public API.

#include "../config.hpp"

#include <cstddef>
#include <initializer_list>
#include <meta>
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
