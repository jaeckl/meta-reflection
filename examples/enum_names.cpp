// Enum to text and back, without a hand-written switch.

#include <reflect/reflect.hpp>

#include <iostream>
#include <string>

enum class Color { Red, Green, Blue };

int main() {
    // Value to name.
    std::cout << reflect::enum_name(Color::Green) << '\n';

    // Name to value. The result is empty for an unknown name.
    for (std::string text : {"Blue", "Purple"}) {
        if (auto color = reflect::enum_cast<Color>(text))
            std::cout << text << " -> " << static_cast<int>(*color) << '\n';
        else
            std::cout << text << " is not a Color\n";
    }

    // Every value, for a menu or a help text.
    std::cout << reflect::name_of<Color>() << " has "
              << reflect::enum_count<Color>() << " values:";
    for (Color color : reflect::enum_values<Color>())
        std::cout << ' ' << reflect::enum_name(color);
    std::cout << '\n';
}
