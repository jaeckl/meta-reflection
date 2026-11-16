#pragma once

// Experimental: writing source code with indentation.

#include "../config.hpp"

#include <ostream>
#include <string>
#include <string_view>

namespace reflect::codegen {

/// Writes source code line by line and keeps track of the indentation:
///
///     out.line("class ", name, " {");
///     {
///         auto body = out.indented();
///         out.line("int value;");
///     }
///     out.line("};");
class source_writer {
public:
    explicit source_writer(std::ostream& out) : out_(&out) {}

    source_writer(const source_writer&) = delete;
    source_writer& operator=(const source_writer&) = delete;

    /// One line: the indentation, the parts one after the other, a newline.
    /// A part is anything that can be written to a stream.
    template <typename... Parts>
    source_writer& line(const Parts&... parts) {
        for (int i = 0; i < depth_; ++i)
            *out_ << "    ";
        static_cast<void>((*out_ << ... << parts));
        *out_ << '\n';
        return *this;
    }

    /// An empty line.
    source_writer& blank() {
        *out_ << '\n';
        return *this;
    }

    /// Indents the lines written while the returned object is alive.
    class indentation {
    public:
        explicit indentation(source_writer& writer) : writer_(&writer) {
            ++writer_->depth_;
        }
        indentation(const indentation&) = delete;
        indentation& operator=(const indentation&) = delete;
        ~indentation() { --writer_->depth_; }

    private:
        source_writer* writer_;
    };

    [[nodiscard]] indentation indented() { return indentation(*this); }

private:
    std::ostream* out_;
    int depth_ = 0;
};

/// Opens a namespace and closes it again at the end of the scope. Does
/// nothing for an empty name, the global namespace.
///
///     reflect::codegen::namespace_scope scope(out, "shop::detail");
class namespace_scope {
public:
    namespace_scope(source_writer& out, std::string_view name)
        : out_(&out), name_(name) {
        if (!name_.empty())
            out_->line("namespace ", name_, " {").blank();
    }
    namespace_scope(const namespace_scope&) = delete;
    namespace_scope& operator=(const namespace_scope&) = delete;
    ~namespace_scope() {
        if (!name_.empty())
            out_->line("}  // namespace ", name_).blank();
    }

private:
    source_writer* out_;
    std::string name_;
};

}  // namespace reflect::codegen
