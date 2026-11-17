// The registry: the generated list of the classes that carry an
// annotation, and the functions of the library that read it. The build
// generates registry_model_generated.hpp from registry_model.hpp.

#include "registry_model_generated.hpp"

#include <reflect/reflect.hpp>

#include "check.hpp"

#include <concepts>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

// --- the classes, as a type --------------------------------------------------

static_assert(std::same_as<reflect::registered_classes<std::tuple>,
                           std::tuple<shapes::Circle, shapes::Square, shapes::Polygon,
                                      shapes::Outline, shapes::Canvas>>);

// A registry for an annotation of the project.
static_assert(std::same_as<reflect::registered_classes<std::tuple, Component>,
                           std::tuple<shapes::Square, shapes::detail::Helper>>);

// An annotation that no class carries has an empty registry.
static_assert(std::same_as<reflect::registered_classes<std::tuple, Unused>, std::tuple<>>);

// --- one call per class ------------------------------------------------------

constexpr int count_registered() {
    int count = 0;
    reflect::for_each_registered([&](auto) { ++count; });
    return count;
}
static_assert(count_registered() == 5);

constexpr int count_unused() {
    int count = 0;
    reflect::for_each_registered<Unused>([&](auto) { ++count; });
    return count;
}
static_assert(count_unused() == 0);

int main() {
    std::vector<std::string_view> names;
    reflect::for_each_registered([&](auto cls) {
        names.push_back(cls.qualified_name());
    });
    CHECK((names == std::vector<std::string_view>{"shapes::Circle", "shapes::Square",
                                                  "shapes::Polygon", "shapes::Outline",
                                                  "shapes::Canvas"}));

    std::vector<std::string_view> components;
    reflect::for_each_registered<Component>([&](auto cls) {
        components.push_back(cls.name());
    });
    CHECK((components == std::vector<std::string_view>{"Square", "Helper"}));

    // --- an object from a name known at run time ------------------------------

    std::string wanted = "shapes::";
    wanted += "Square";
    std::unique_ptr<shapes::Shape> shape = reflect::create_registered<shapes::Shape>(wanted);
    CHECK(shape != nullptr);
    CHECK(shape && shape->name() == "square");
    CHECK(reflect::create_registered<shapes::Shape>("shapes::Circle")->name() == "circle");

    // No such class, or the name without its namespace.
    CHECK(reflect::create_registered<shapes::Shape>("shapes::Triangle") == nullptr);
    CHECK(reflect::create_registered<shapes::Shape>("Circle") == nullptr);
    CHECK(reflect::create_registered<shapes::Shape>("") == nullptr);
    // A class that is not registered.
    CHECK(reflect::create_registered<shapes::Shape>("shapes::Unmarked") == nullptr);
    // No default constructor, abstract, not a Shape.
    CHECK(reflect::create_registered<shapes::Shape>("shapes::Polygon") == nullptr);
    CHECK(reflect::create_registered<shapes::Shape>("shapes::Outline") == nullptr);
    CHECK(reflect::create_registered<shapes::Shape>("shapes::Canvas") == nullptr);

    // The class itself as the base: no virtual destructor needed.
    std::unique_ptr<shapes::Canvas> canvas =
        reflect::create_registered<shapes::Canvas>("shapes::Canvas");
    CHECK(canvas && canvas->width == 80);
    CHECK(reflect::create_registered<shapes::Canvas>("shapes::Circle") == nullptr);

    // The other registry holds other classes.
    CHECK(reflect::create_registered<shapes::Shape, Component>("shapes::Square") != nullptr);
    CHECK(reflect::create_registered<shapes::Shape, Component>("shapes::Circle") == nullptr);
    CHECK(reflect::create_registered<shapes::Shape, Unused>("shapes::Circle") == nullptr);

    // --- the classes in a template of your choice -----------------------------

    using Part = reflect::registered_classes<std::variant, Component>;
    Part part = shapes::Square{};
    CHECK(std::holds_alternative<shapes::Square>(part));
    CHECK(std::variant_size_v<Part> == 2);

    return test::result();
}
