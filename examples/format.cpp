// std::format for your own structs.
//
// One formatter, written once, covers every struct that opts in with an
// annotation.

#include <reflect/reflect.hpp>

#include <format>
#include <iostream>
#include <string>

// Marks a type as printable through std::format.
struct Formattable {};

// Hides a field from the output.
struct Secret {};

template <reflect::annotated_with<Formattable> T>
struct std::formatter<T> {
    constexpr auto parse(std::format_parse_context& context) { return context.begin(); }

    auto format(const T& object, std::format_context& context) const {
        auto out = std::format_to(context.out(), "{}{{", reflect::name_of<T>());
        bool first = true;
        reflect::for_each_field(object, [&](auto field, const auto& value) {
            out = std::format_to(out, "{}{}=", first ? "" : ", ", field.name());
            if constexpr (reflect::has_annotation<Secret>(field))
                out = std::format_to(out, "***");
            else
                out = std::format_to(out, "{}", value);
            first = false;
        });
        return std::format_to(out, "}}");
    }
};

struct [[=Formattable{}]] Point {
    int x;
    int y;
};

struct [[=Formattable{}]] Login {
    std::string user;
    [[=Secret{}]] std::string password;
    Point position;  // a formattable struct inside another one
};

int main() {
    Point point{3, 4};
    Login login{"ada", "hunter2", {1, 2}};

    std::cout << std::format("{}", point) << '\n';
    std::cout << std::format("{}", login) << '\n';
    std::cout << std::format("moved to {} at step {}", point, 7) << '\n';
}
