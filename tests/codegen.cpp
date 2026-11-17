#include <reflect/codegen.hpp>

#include "check.hpp"
#include "codegen_bad_model.hpp"
#include "codegen_model.hpp"

#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace codegen = reflect::codegen;

// A class of this file, not of an input header.
struct [[=Printable{}]] Local {};

// Paths.
static_assert(codegen::detail::normalize_path("a/./b/../c") == "a/c");
static_assert(codegen::detail::normalize_path("/x//y/") == "/x/y");
static_assert(codegen::detail::normalize_path("C:\\work\\src\\model.hpp") == "C:/work/src/model.hpp");
static_assert(codegen::detail::normalize_path("../up/file.hpp") == "../up/file.hpp");
static_assert(codegen::detail::same_file("model.hpp", "model.hpp"));
static_assert(codegen::detail::same_file("/project/src/model.hpp", "model.hpp"));
static_assert(codegen::detail::same_file("model.hpp", "/project/src/model.hpp"));
static_assert(codegen::detail::same_file("/project/src/../src/model.hpp", "src/model.hpp"));
static_assert(!codegen::detail::same_file("/project/src/data_model.hpp", "model.hpp"));
static_assert(!codegen::detail::same_file("/project/a/model.hpp", "b/model.hpp"));
static_assert(!codegen::detail::same_file("/project/src/model.hpp", "/src/model.hpp"));
static_assert(!codegen::detail::same_file("model.hpp", ""));

// Names.
static_assert(codegen::namespace_of<TopLevel>() == "");
static_assert(codegen::namespace_of<shop::User>() == "shop");
static_assert(codegen::namespace_of<shop::detail::Deep>() == "shop::detail");
static_assert(codegen::namespace_of<shop::Outer::Nested>() == "shop");
static_assert(codegen::can_be_named<shop::User>());
static_assert(codegen::can_be_named<shop::Outer::Nested>());
static_assert(!codegen::can_be_named<bad::Unreachable>());
static_assert(!codegen::can_be_named<int>());

// A processor without `annotation` is called for every class.
struct list_processor {
    template <typename T>
    static void process(codegen::context& context) {
        context.out().line("// ", reflect::qualified_name_of<T>());
    }
};

// A processor with one is called for the classes that carry it.
struct print_processor {
    using annotation = Printable;

    template <typename T>
    static void process(codegen::context& context) {
        context.include("<ostream>");
        context.include("<ostream>");  // asking twice is fine
        context.out().line("void print(const ", reflect::qualified_name_of<T>(), "&);");
    }
};

struct failing_processor {
    using annotation = Printable;

    template <typename T>
    static void process(codegen::context& context) {
        context.error<T>("not today");
    }
};

// A processor is an object: it can count in process() and report in end().
struct counting_processor {
    using annotation = Printable;

    void begin(codegen::context& context) { context.out().line("// begin"); }

    template <typename T>
    void process(codegen::context&) {
        ++count_;
    }

    void end(codegen::context& context) {
        context.out().line("// end: ", count_, " classes");
    }

private:
    int count_ = 0;
};

// begin() and end() are called even if no class carries the annotation,
// and they may be static.
struct Unused {};

struct idle_processor {
    using annotation = Unused;

    template <typename T>
    static void process(codegen::context& context) {
        context.out().line("// never");
    }

    static void end(codegen::context& context) { context.out().line("// idle"); }
};

// end() can refuse the input as well.
struct refusing_processor {
    using annotation = Printable;

    template <typename T>
    void process(codegen::context&) {}

    void end(codegen::context& context) {
        context.error<shop::Plain>("refused at the end");
    }
};

bool contains(const std::string& text, std::string_view part) {
    return text.find(part) != std::string::npos;
}

int main() {
    // Discovery: the classes of the listed header, wherever they are.
    std::vector<std::string_view> found;
    codegen::for_each_class_from<"codegen_model.hpp">([&]<typename T>(std::type_identity<T>) {
        found.push_back(reflect::qualified_name_of<T>());
    });
    CHECK((found == std::vector<std::string_view>{
                        "Printable", "TopLevel", "shop::Entity", "shop::User", "shop::Plain",
                        "shop::detail::Deep", "shop::v2::Versioned", "shop::Outer",
                        "shop::Outer::Nested"}));

    int both = 0;
    codegen::for_each_class_from<"codegen_model.hpp", "codegen_bad_model.hpp">(
        [&](auto) { ++both; });
    CHECK(both == 13);

    int none = 0;
    codegen::for_each_class_from<"no_such_file.hpp">([&](auto) { ++none; });
    codegen::for_each_class_from<>([&](auto) { ++none; });
    CHECK(none == 0);

    // The writer.
    {
        std::ostringstream text;
        codegen::source_writer out(text);
        out.line("class A {");
        {
            auto body = out.indented();
            out.line("int ", "value", " = ", 7, ";");
            {
                auto deeper = out.indented();
                out.line("x");
            }
            out.line("y");
        }
        out.line("};").blank();
        {
            codegen::namespace_scope scope(out, "a::b");
            out.line("int z;");
        }
        {
            codegen::namespace_scope global(out, "");
            out.line("int g;");
        }
        CHECK(text.str() == "class A {\n"
                            "    int value = 7;\n"
                            "        x\n"
                            "    y\n"
                            "};\n"
                            "\n"
                            "namespace a::b {\n"
                            "\n"
                            "int z;\n"
                            "}  // namespace a::b\n"
                            "\n"
                            "int g;\n");
    }

    // Processors, in the order they are listed.
    {
        std::ostringstream out, errors;
        bool ok = codegen::generate<codegen::inputs<"codegen_model.hpp">, print_processor,
                                    list_processor>(out, errors);
        CHECK(ok);
        CHECK(errors.str().empty());
        CHECK(out.str() == "// Generated by reflect. Do not edit.\n"
                           "\n"
                           "#pragma once\n"
                           "\n"
                           "#include \"codegen_model.hpp\"\n"
                           "\n"
                           "#include <ostream>\n"
                           "\n"
                           "void print(const TopLevel&);\n"
                           "void print(const shop::User&);\n"
                           "void print(const shop::v2::Versioned&);\n"
                           "void print(const shop::Outer::Nested&);\n"
                           "// Printable\n"
                           "// TopLevel\n"
                           "// shop::Entity\n"
                           "// shop::User\n"
                           "// shop::Plain\n"
                           "// shop::detail::Deep\n"
                           "// shop::v2::Versioned\n"
                           "// shop::Outer\n"
                           "// shop::Outer::Nested\n");
    }

    // begin() and end(), and a fresh processor object for every run.
    for (int run = 0; run < 2; ++run) {
        std::ostringstream out, errors;
        bool ok = codegen::generate<codegen::inputs<"codegen_model.hpp">, counting_processor,
                                    idle_processor>(out, errors);
        CHECK(ok);
        CHECK(contains(out.str(), "#include \"codegen_model.hpp\"\n"
                                  "\n"
                                  "// begin\n"
                                  "// end: 4 classes\n"
                                  "// idle\n"));
    }
    {
        std::ostringstream out, errors;
        bool ok = codegen::generate<codegen::inputs<"codegen_model.hpp">,
                                    refusing_processor>(out, errors);
        CHECK(!ok);
        CHECK(out.str().empty());
        CHECK(contains(errors.str(), ": error: refused at the end\n"));
    }

    // The registry processor, for an annotation of the project.
    {
        std::ostringstream out, errors;
        bool ok = codegen::generate<codegen::inputs<"codegen_model.hpp">,
                                    codegen::registry_processor<Printable>>(out, errors);
        CHECK(ok);
        CHECK(errors.str().empty());
        CHECK(out.str() == "// Generated by reflect. Do not edit.\n"
                           "\n"
                           "#pragma once\n"
                           "\n"
                           "#include \"codegen_model.hpp\"\n"
                           "\n"
                           "#include <reflect/reflect.hpp>\n"
                           "\n"
                           "template <>\n"
                           "struct reflect::registry<Printable> {\n"
                           "    template <template <typename...> typename Template>\n"
                           "    using apply = Template<\n"
                           "        TopLevel,\n"
                           "        shop::User,\n"
                           "        shop::v2::Versioned,\n"
                           "        shop::Outer::Nested>;\n"
                           "};\n");
    }
    // Without a class the registry exists and is empty.
    {
        std::ostringstream out, errors;
        bool ok = codegen::generate<codegen::inputs<"codegen_model.hpp">,
                                    codegen::registry_processor<>>(out, errors);
        CHECK(ok);
        CHECK(contains(out.str(), "struct reflect::registry<reflect::registered_t> {\n"
                                  "    template <template <typename...> typename Template>\n"
                                  "    using apply = Template<>;\n"
                                  "};\n"));
    }
    // A class that generated code cannot name is an error.
    {
        std::ostringstream out, errors;
        bool ok = codegen::generate<codegen::inputs<"codegen_bad_model.hpp">,
                                    codegen::registry_processor<>>(out, errors);
        CHECK(!ok);
        CHECK(out.str().empty());
        CHECK(contains(errors.str(), "codegen_bad_model.hpp:24:"));
        CHECK(contains(errors.str(), ": error: reflect::registered: the class is in an unnamed namespace"));
    }

    // The builder processor.
    {
        std::ostringstream out, errors;
        bool ok = codegen::generate<codegen::inputs<"codegen_model.hpp">,
                                    codegen::builder_processor>(out, errors);
        CHECK(ok);
        CHECK(errors.str().empty());
        std::string text = out.str();
        CHECK(contains(text, "#include <stdexcept>\n#include <utility>\n"));
        CHECK(contains(text, "namespace shop {\n\nclass UserBuilder {\npublic:\n"));
        // Inherited, required.
        CHECK(contains(text, "    UserBuilder& with_id(decltype(shop::User::id) value) {\n"
                             "        object_.id = std::move(value);\n"
                             "        has_id_ = true;\n"
                             "        return *this;\n"
                             "    }\n"));
        // Not required.
        CHECK(contains(text, "    UserBuilder& with_age(decltype(shop::User::age) value) {\n"
                             "        object_.age = std::move(value);\n"
                             "        return *this;\n"
                             "    }\n"));
        CHECK(contains(text, "    shop::User build() {\n"
                             "        if (!has_id_)\n"
                             "            throw std::logic_error(\"UserBuilder: the required field 'id' was not set\");\n"
                             "        if (!has_name_)\n"
                             "            throw std::logic_error(\"UserBuilder: the required field 'name' was not set\");\n"
                             "        return std::move(object_);\n"
                             "    }\n"));
        CHECK(contains(text, "private:\n"
                             "    shop::User object_{};\n"
                             "    bool has_id_ = false;\n"
                             "    bool has_name_ = false;\n"
                             "};\n"));
        // No setter for what generated code cannot assign.
        CHECK(!contains(text, "with_fixed"));
        CHECK(!contains(text, "with_flag"));
        CHECK(!contains(text, "with_secret"));
        // The prefix option and a nested namespace.
        CHECK(contains(text, "namespace shop::detail {\n\nclass DeepBuilder {\n"));
        CHECK(contains(text, "    DeepBuilder& set_depth(decltype(shop::detail::Deep::depth) value) {\n"));
        CHECK(contains(text, "}  // namespace shop::detail\n"));
        // Nothing for classes without the annotation.
        CHECK(!contains(text, "PlainBuilder"));
        CHECK(!contains(text, "TopLevelBuilder"));
    }

    // Errors: reported at the declaration, and nothing is written.
    {
        std::ostringstream out, errors;
        bool ok = codegen::generate<codegen::inputs<"codegen_bad_model.hpp">,
                                    codegen::builder_processor>(out, errors);
        CHECK(!ok);
        CHECK(out.str().empty());
        std::string text = errors.str();
        CHECK(contains(text, "codegen_bad_model.hpp:10:"));
        CHECK(contains(text, ": error: reflect::generate_builder: the class needs a default constructor\n"));
        CHECK(contains(text, "codegen_bad_model.hpp:16:"));
        CHECK(contains(text, ": error: reflect::generate_builder: a required field has to be public and assignable\n"));
        CHECK(contains(text, "codegen_bad_model.hpp:20:"));
        CHECK(contains(text, "unnamed namespace"));
    }
    {
        std::ostringstream out, errors;
        bool ok = codegen::generate<codegen::inputs<"codegen_model.hpp">, list_processor,
                                    failing_processor>(out, errors);
        CHECK(!ok);
        CHECK(out.str().empty());
        CHECK(contains(errors.str(), "codegen_model.hpp:12:"));
        CHECK(contains(errors.str(), ": error: not today\n"));
    }

    return test::result();
}
