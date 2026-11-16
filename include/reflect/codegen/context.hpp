#pragma once

// Experimental: what a processor is handed to do its work.

#include "../access.hpp"
#include "../config.hpp"
#include "writer.hpp"

#include <meta>
#include <ostream>
#include <set>
#include <source_location>
#include <sstream>
#include <string>
#include <string_view>

namespace reflect::codegen {

/// The output of one generator run. A processor writes code through out(),
/// asks for the headers that code needs with include(), and reports
/// problems with error().
class context {
public:
    explicit context(std::ostream& errors) : writer_(body_), errors_(&errors) {}

    context(const context&) = delete;
    context& operator=(const context&) = delete;

    /// Where the generated code goes.
    source_writer& out() { return writer_; }

    /// Asks for an #include at the top of the generated file. Pass the
    /// header with its brackets or quotes: "<utility>". Asking twice is fine.
    void include(std::string_view header) { includes_.emplace(header); }

    /// Reports a problem with the class T. The message is printed like a
    /// compiler error, with the file and line where T is declared, and the
    /// generator fails.
    template <typename T>
    void error(std::string_view message) {
        constexpr std::source_location where = std::meta::source_location_of(^^T);
        report(where, message);
    }

    /// Reports a problem with a member: context.error(field, "...").
    template <descriptor D>
    void error(D, std::string_view message) {
        constexpr std::source_location where =
            std::meta::source_location_of(D::native());
        report(where, message);
    }

    /// True once error() has been called.
    bool failed() const { return failed_; }

    /// The code written so far, and the headers asked for, sorted.
    std::string body() const { return body_.str(); }
    const std::set<std::string>& includes() const { return includes_; }

private:
    void report(const std::source_location& where, std::string_view message) {
        *errors_ << where.file_name() << ':' << where.line() << ':'
                 << where.column() << ": error: " << message << '\n';
        failed_ = true;
    }

    std::ostringstream body_;
    source_writer writer_;
    std::set<std::string> includes_;
    std::ostream* errors_;
    bool failed_ = false;
};

}  // namespace reflect::codegen
