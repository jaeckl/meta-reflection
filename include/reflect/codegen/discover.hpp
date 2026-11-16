#pragma once

// Experimental: finding the classes a code generator works on.

#include "../access.hpp"
#include "../config.hpp"
#include "../namespace.hpp"
#include "../string.hpp"

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <meta>
#include <string>
#include <string_view>
#include <vector>

namespace reflect::codegen {

/// The header files a generator reads its classes from:
/// inputs<"model.hpp", "orders.hpp">.
template <fixed_string... Files>
struct inputs {};

namespace detail {

// "a/./b/../c\\d" becomes "a/c/d". Paths are compared as text; the file
// system is not consulted.
constexpr std::string normalize_path(std::string_view path) {
    std::vector<std::string> parts;
    bool absolute = !path.empty() && (path[0] == '/' || path[0] == '\\');
    std::size_t start = 0;
    while (start <= path.size()) {
        std::size_t end = path.find_first_of("/\\", start);
        if (end == std::string_view::npos)
            end = path.size();
        std::string_view part = path.substr(start, end - start);
        if (part == ".." && !parts.empty() && parts.back() != "..")
            parts.pop_back();
        else if (!part.empty() && part != ".")
            parts.emplace_back(part);
        start = end + 1;
    }
    std::string result = absolute ? "/" : "";
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i != 0)
            result += '/';
        result += parts[i];
    }
    return result;
}

// Do two spellings name the same file? The compiler reports a header the way
// it was found, which may be relative or absolute, so one path may be the
// tail of the other: "model.hpp" and "/project/src/model.hpp".
constexpr bool same_file(std::string_view left, std::string_view right) {
    std::string a = normalize_path(left);
    std::string b = normalize_path(right);
    if (a.size() < b.size())
        std::swap(a, b);
    if (b.empty() || !a.ends_with(b))
        return false;
    if (a.size() == b.size())
        return true;
    // Only whole directory and file names count: "data_model.hpp" is not
    // "model.hpp".
    return b.front() != '/' && a[a.size() - b.size() - 1] == '/';
}

consteval bool declared_in(std::meta::info entity,
                           std::initializer_list<std::string_view> files) {
    std::string_view file = std::meta::source_location_of(entity).file_name();
    for (std::string_view wanted : files) {
        if (same_file(file, wanted))
            return true;
    }
    return false;
}

// The standard library and the implementation's own namespaces are large
// and never hold a class from an input file.
consteval bool is_skipped_namespace(std::meta::info scope) {
    if (!std::meta::has_identifier(scope))
        return false;
    std::string_view name = std::meta::identifier_of(scope);
    return name == "std" || name.starts_with("__");
}

consteval void collect_classes(std::meta::info scope,
                               std::initializer_list<std::string_view> files,
                               std::vector<std::meta::info>& found) {
    for (std::meta::info member : std::meta::members_of(scope, unchecked_access)) {
        if (std::meta::is_namespace(member)) {
            if (!std::meta::is_namespace_alias(member) && !is_skipped_namespace(member))
                collect_classes(member, files, found);
            continue;
        }
        if (!std::meta::is_type(member) || std::meta::is_type_alias(member))
            continue;
        if (!std::meta::is_class_type(member) || !std::meta::is_complete_type(member))
            continue;
        if (!declared_in(member, files) || std::ranges::contains(found, member))
            continue;
        found.push_back(member);
        collect_classes(member, files, found);  // classes nested in this one
    }
}

consteval std::meta::info classes_declared_in(
    std::initializer_list<std::string_view> files) {
    std::vector<std::meta::info> found;
    collect_classes(^^::, files, found);
    std::vector<std::meta::info> arguments;
    for (std::meta::info type : found)
        arguments.push_back(std::meta::reflect_constant(type));
    return std::meta::substitute(^^::reflect::detail::class_list, arguments);
}

consteval const char* enclosing_namespace(std::meta::info type) {
    std::string name;
    for (std::meta::info scope = std::meta::parent_of(type); scope != ^^::;
         scope = std::meta::parent_of(scope)) {
        if (!std::meta::is_namespace(scope) || !std::meta::has_identifier(scope))
            continue;
        std::string outer(std::meta::identifier_of(scope));
        name = name.empty() ? outer : outer + "::" + name;
    }
    return std::define_static_string(name);
}

consteval bool can_be_named(std::meta::info type) {
    if (!std::meta::has_identifier(type))
        return false;
    for (std::meta::info scope = std::meta::parent_of(type); scope != ^^::;
         scope = std::meta::parent_of(scope)) {
        bool nameable = std::meta::is_namespace(scope) || std::meta::is_type(scope);
        if (!nameable || !std::meta::has_identifier(scope))
            return false;
    }
    return true;
}

}  // namespace detail

/// Calls fn(std::type_identity<T>{}) for every class declared in one of the
/// given header files, whatever namespace it is in:
///
///     reflect::codegen::for_each_class_from<"model.hpp">(
///         []<typename T>(std::type_identity<T>) { ... });
///
/// Classes nested in other classes are included. Class templates, classes
/// that are only declared, and everything in namespace std are not. A file
/// is given the way it is included, or with more of its path in front.
template <fixed_string... Files, typename Fn>
constexpr void for_each_class_from(Fn&& fn) {
    using classes = [:detail::classes_declared_in({Files.view()...}):];
    classes::for_each(fn);
}

/// The namespace a type is declared in: "shop::detail", or "" for the
/// global namespace. Enclosing classes are not part of it.
template <typename T>
constexpr std::string_view namespace_of() {
    return detail::enclosing_namespace(^^T);
}

/// False for a type that code in another file cannot refer to: one in an
/// unnamed namespace or inside a function, or one without a name.
template <typename T>
consteval bool can_be_named() {
    return detail::can_be_named(^^T);
}

}  // namespace reflect::codegen
