#pragma once

// Experimental: the processor behind [[=reflect::registered]].

#include "../config.hpp"
#include "../registry.hpp"
#include "../type.hpp"
#include "context.hpp"
#include "discover.hpp"
#include "writer.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace reflect::codegen {

/// Writes the list of the classes that carry an annotation, for
/// reflect::for_each_registered, reflect::registered_classes and
/// reflect::create_registered:
///
///     reflect::codegen::registry_processor<>            [[=reflect::registered]]
///     reflect::codegen::registry_processor<Component>   [[=Component{}]]
///
/// The classes are listed in the order the generator finds them: header by
/// header, and in a header in the order of the declarations, with the
/// classes of a namespace kept together.
template <typename Annotation = registered_t>
class registry_processor {
    static_assert(can_be_named<Annotation>(),
                  "reflect::codegen::registry_processor: the annotation has to be "
                  "a class that generated code can name");

public:
    using annotation = Annotation;

    // Called for one class at a time: remember it.
    template <typename T>
    void process(context& context) {
        if (!can_be_named<T>()) {
            context.error<T>(
                "reflect::registered: the class is in an unnamed namespace or in "
                "a function, so generated code cannot refer to it");
            return;
        }
        classes_.emplace_back(qualified_name_of<T>());
    }

    // Called after the last class: write the list.
    void end(context& context) {
        // For the template that is specialized below. The single header
        // has the same name.
        context.include("<reflect/reflect.hpp>");
        source_writer& out = context.out();

        out.line("template <>");
        out.line("struct reflect::registry<", qualified_name_of<Annotation>(), "> {");
        {
            auto members = out.indented();
            out.line("template <template <typename...> typename Template>");
            if (classes_.empty()) {
                out.line("using apply = Template<>;");
            } else {
                out.line("using apply = Template<");
                auto arguments = out.indented();
                for (std::size_t i = 0; i < classes_.size(); ++i)
                    out.line(classes_[i], i + 1 < classes_.size() ? "," : ">;");
            }
        }
        out.line("};");
        out.blank();
    }

private:
    std::vector<std::string> classes_;
};

}  // namespace reflect::codegen
