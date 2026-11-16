#pragma once

// Experimental: the processor behind [[=reflect::generate_builder{}]].

#include "../access.hpp"
#include "../annotation.hpp"
#include "../builder.hpp"
#include "../config.hpp"
#include "../field.hpp"
#include "../type.hpp"
#include "context.hpp"
#include "discover.hpp"
#include "writer.hpp"

#include <concepts>
#include <string>
#include <string_view>
#include <type_traits>

namespace reflect::codegen {

/// Writes a builder class for every class marked
/// [[=reflect::generate_builder{}]]:
///
///     shop::User user = shop::UserBuilder().with_id(7).with_name("Ada").build();
///
/// - One setter per public field, fields of base classes included.
/// - build() throws std::logic_error if a field marked
///   [[=reflect::required]] was not set.
/// - The builder is declared in the namespace of the class.
struct builder_processor {
    using annotation = generate_builder;

    template <typename T>
    static void process(context& context) {
        if (!can_be_named<T>()) {
            context.error<T>(
                "reflect::generate_builder: the class is in an unnamed namespace "
                "or in a function, so generated code cannot refer to it");
            return;
        }
        if (!std::default_initializable<T>) {
            context.error<T>(
                "reflect::generate_builder: the class needs a default constructor");
            return;
        }

        // A required field the builder cannot set would make build() fail
        // every time.
        bool usable = true;
        all_fields_of_t<T, unchecked_access>::for_each([&](auto field) {
            if constexpr (field.template has_annotation<required_t>()) {
                if (!field.is_public() || !can_set(field)) {
                    context.error(field,
                                  "reflect::generate_builder: a required field has "
                                  "to be public and assignable");
                    usable = false;
                }
            }
        });
        if (!usable)
            return;

        std::string type(qualified_name_of<T>());
        std::string builder = std::string(name_of<T>()) + "Builder";
        std::string_view prefix = get_annotation<generate_builder, T>().prefix;
        bool any_required = false;

        context.include("<utility>");
        source_writer& out = context.out();
        namespace_scope scope(out, namespace_of<T>());

        out.line("class ", builder, " {");
        out.line("public:");
        {
            auto members = out.indented();
            for_each_settable_field<T>([&](auto field) {
                // decltype(shop::User::name) is the type of the field,
                // whatever it is, without having to print its name.
                out.line(builder, "& ", prefix, field.name(), "(decltype(", type,
                         "::", field.name(), ") value) {");
                {
                    auto body = out.indented();
                    out.line("object_.", field.name(), " = std::move(value);");
                    if constexpr (field.template has_annotation<required_t>()) {
                        out.line("has_", field.name(), "_ = true;");
                        any_required = true;
                    }
                    out.line("return *this;");
                }
                out.line("}");
            });
            out.line(type, " build() {");
            {
                auto body = out.indented();
                for_each_settable_field<T>([&](auto field) {
                    if constexpr (field.template has_annotation<required_t>()) {
                        out.line("if (!has_", field.name(), "_)");
                        auto statement = out.indented();
                        out.line("throw std::logic_error(\"", builder,
                                 ": the required field '", field.name(),
                                 "' was not set\");");
                    }
                });
                out.line("return std::move(object_);");
            }
            out.line("}");
        }
        out.blank();
        out.line("private:");
        {
            auto members = out.indented();
            out.line(type, " object_{};");
            for_each_settable_field<T>([&](auto field) {
                if constexpr (field.template has_annotation<required_t>())
                    out.line("bool has_", field.name(), "_ = false;");
            });
        }
        out.line("};");
        out.blank();

        if (any_required)
            context.include("<stdexcept>");
    }

private:
    // A const or reference member cannot be assigned after construction.
    template <typename Field>
    static consteval bool can_set(Field) {
        using type = typename Field::type;
        return std::is_assignable_v<type&, type>;
    }

    // The generated code is ordinary code: it reaches public fields only.
    template <typename T, typename Fn>
    static void for_each_settable_field(Fn&& fn) {
        all_fields_of_t<T, public_access>::for_each([&](auto field) {
            if constexpr (field.has_name() && !field.is_bit_field() && can_set(field))
                fn(field);
        });
    }
};

}  // namespace reflect::codegen
