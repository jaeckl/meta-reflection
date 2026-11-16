// A processor of this project: writes operator<< for every class marked
// [[=Printable{}]].
//
// A processor is a class with a `process` function template. The build
// calls it once for every class that carries the annotation named by
// `annotation`.

#pragma once

#include "printable.hpp"

#include <reflect/codegen.hpp>

#include <string_view>

struct print_processor {
    using annotation = Printable;

    template <typename T>
    static void process(reflect::codegen::context& context) {
        std::string_view type = reflect::qualified_name_of<T>();

        if (reflect::get_fields_of<T, reflect::public_access>().empty()) {
            // Printed like a compiler error, at the line of the class.
            context.error<T>("Printable: the class has no public fields to print");
            return;
        }

        context.include("<ostream>");
        reflect::codegen::source_writer& out = context.out();

        // The operator goes into the namespace of the class, where the
        // compiler looks for it.
        reflect::codegen::namespace_scope scope(out, reflect::codegen::namespace_of<T>());

        out.line("inline std::ostream& operator<<(std::ostream& out, const ", type,
                 "& value) {");
        {
            auto body = out.indented();
            out.line("out << \"", reflect::name_of<T>(), "{\";");
            bool first = true;
            reflect::for_each_field<T, reflect::public_access>([&](auto field) {
                out.line("out << \"", first ? "" : ", ", field.name(), "=\" << value.",
                         field.name(), ";");
                first = false;
            });
            out.line("return out << \"}\";");
        }
        out.line("}");
        out.blank();
    }
};
