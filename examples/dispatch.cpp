// Call member functions by a name that arrives at run time.
//
// The usual shape of a small RPC layer, a scripting bridge or a console:
// a string comes in, the matching member function runs.

#include <reflect/reflect.hpp>

#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

// Marks the functions that may be called from outside.
struct Command {
    reflect::static_string help;
};

class Thermostat {
public:
    [[=Command{"current target temperature"}]] double target() const { return target_; }

    [[=Command{"set the target temperature"}]] void set_target(double degrees) {
        target_ = degrees;
    }

    [[=Command{"change the target by a number of steps of a given size"}]]
    double nudge(int steps, double step_size) {
        target_ += steps * step_size;
        return target_;
    }

    [[=Command{"switch to a named mode"}]] std::string mode(std::string name) {
        mode_ = name;
        return "mode is now " + mode_;
    }

    void recalibrate() {}  // no Command annotation: not callable by name

private:
    double target_ = 20.0;
    std::string mode_ = "auto";
};

template <typename T>
T from_word(const std::string& word) {
    T value{};
    std::istringstream(word) >> value;
    return value;
}

// The type to parse argument I into: the parameter type without const and &.
template <typename Function, std::size_t I>
using argument_t = std::remove_cvref_t<typename Function::template parameter_type<I>>;

// Calls `function` on `object` with arguments parsed from words[1...] and
// prints the result.
template <typename Object, typename Function, std::size_t... I>
void call(Object& object, Function function, const std::vector<std::string>& words,
          std::index_sequence<I...>) {
    if constexpr (std::is_void_v<typename Function::return_type>) {
        function.invoke(object, from_word<argument_t<Function, I>>(words[I + 1])...);
        std::cout << "ok\n";
    } else {
        std::cout << function.invoke(object,
                                     from_word<argument_t<Function, I>>(words[I + 1])...)
                  << '\n';
    }
}

template <typename Object>
void run(Object& object, const std::string& line) {
    std::vector<std::string> words;
    std::istringstream stream(line);
    for (std::string word; stream >> word;)
        words.push_back(word);
    if (words.empty())
        return;

    std::cout << "> " << line << '\n';

    bool found = false;
    // with<Command>() keeps only the functions that carry the annotation.
    reflect::get_functions_of<Object>().template with<Command>().for_each(
        [&](auto function) {
            if (found || function.name() != words[0])
                return;
            found = true;
            if (words.size() - 1 != function.arity()) {
                std::cout << function.name() << " takes " << function.arity()
                          << " argument(s)\n";
                return;
            }
            call(object, function, words, std::make_index_sequence<function.arity()>{});
        });

    if (!found)
        std::cout << "unknown command " << words[0] << '\n';
}

template <typename Object>
void print_commands() {
    reflect::get_functions_of<Object>().template with<Command>().for_each(
        [](auto function) {
            std::cout << "  " << function.name() << '(';
            reflect::for_each_parameter(function, [](auto parameter) {
                if constexpr (parameter.index() != 0)
                    std::cout << ", ";
                std::cout << parameter.name();
            });
            std::cout << ")  " << reflect::get_annotation<Command>(function).help << '\n';
        });
}

int main() {
    std::cout << "commands:\n";
    print_commands<Thermostat>();

    Thermostat thermostat;
    run(thermostat, "target");
    run(thermostat, "set_target 21.5");
    run(thermostat, "nudge 3 0.5");
    run(thermostat, "mode eco");
    run(thermostat, "nudge 3");
    run(thermostat, "recalibrate");
}
