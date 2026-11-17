// Creates objects from class names that are only known when the program
// runs. C++26 reflection cannot find a class by a name like that; the list
// of classes in shapes_registry.hpp, which the build generates from the
// annotations in shapes.hpp, makes it possible.
//
//     registry                       the three names below
//     registry shapes::Triangle      the names you give

#include "shapes_registry.hpp"

#include <iostream>
#include <memory>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

int main(int argc, char** argv) {
    // Every registered class. A new shape in shapes.hpp appears here
    // without a change to this file.
    std::cout << "known shapes:";
    reflect::for_each_registered([]<typename T>(std::type_identity<T>) {
        std::cout << ' ' << reflect::qualified_name_of<T>();
    });
    std::cout << '\n';

    // From a name to an object.
    std::vector<std::string> names(argv + 1, argv + argc);
    if (names.empty())
        names = {"shapes::Square", "shapes::Circle", "shapes::Hexagon"};
    for (const std::string& name : names) {
        std::unique_ptr<shapes::Shape> shape = reflect::create_registered<shapes::Shape>(name);
        if (shape)
            std::cout << name << ": area " << shape->area() << '\n';
        else
            std::cout << name << ": no such shape\n";
    }

    // The registered classes as the arguments of a template.
    using AnyShape = reflect::registered_classes<std::variant>;
    AnyShape any = shapes::Triangle{};
    double area = std::visit([](const auto& shape) { return shape.area(); }, any);
    std::cout << "a variant of " << std::variant_size_v<AnyShape>
              << " shapes, holding one with area " << area << '\n';
}
