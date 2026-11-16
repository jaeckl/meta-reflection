// Command line options from a plain struct.
//
// Adding an option means adding one member. The parser, the help text and
// the "current values" dump all follow from the struct.
//
// Try:  command_line --jobs=8 --mode=Fast --verbose

#include <reflect/reflect.hpp>

#include <charconv>
#include <iostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

struct Help {
    reflect::static_string text;
};

enum class Mode { Fast, Safe };

struct Options {
    [[=Help{"number of worker threads"}]] int jobs = 1;
    [[=Help{"file to write the result to"}]] std::string output = "out.txt";
    [[=Help{"Fast or Safe"}]] Mode mode = Mode::Safe;
    [[=Help{"print what is going on"}]] bool verbose = false;
};

// Text to value, for the member types an options struct may use.
template <typename T>
bool from_text(std::string_view text, T& value) {
    if constexpr (std::is_same_v<T, bool>) {
        value = text.empty() || text == "true";
        return text.empty() || text == "true" || text == "false";
    } else if constexpr (std::is_enum_v<T>) {
        auto parsed = reflect::enum_cast<T>(text);
        if (parsed)
            value = *parsed;
        return parsed.has_value();
    } else if constexpr (std::is_arithmetic_v<T>) {
        const char* end = text.data() + text.size();
        auto result = std::from_chars(text.data(), end, value);
        return result.ec == std::errc{} && result.ptr == end;
    } else {
        value = T(text);
        return true;
    }
}

template <typename T>
bool parse(const std::vector<std::string_view>& arguments, T& options) {
    for (std::string_view argument : arguments) {
        if (!argument.starts_with("--")) {
            std::cout << "expected --name=value, got " << argument << '\n';
            return false;
        }
        argument.remove_prefix(2);
        std::size_t equals = argument.find('=');
        std::string_view name = argument.substr(0, equals);
        std::string_view text =
            equals == std::string_view::npos ? "" : argument.substr(equals + 1);

        // The name is only known at run time; visit_field finds the member.
        bool accepted = false;
        bool known = reflect::visit_field(options, name, [&](auto, auto& value) {
            accepted = from_text(text, value);
        });
        if (!known) {
            std::cout << "unknown option --" << name << '\n';
            return false;
        }
        if (!accepted) {
            std::cout << "bad value for --" << name << ": '" << text << "'\n";
            return false;
        }
    }
    return true;
}

template <typename T>
void print_help() {
    std::cout << "options:\n";
    reflect::for_each_field<T>([](auto field) {
        std::cout << "  --" << field.name();
        if constexpr (reflect::has_annotation<Help>(field))
            std::cout << "  " << reflect::get_annotation<Help>(field).text;
        std::cout << '\n';
    });
}

template <typename T>
void print_values(const T& options) {
    reflect::for_each_field(options, [](auto field, const auto& value) {
        std::cout << "  " << field.name() << " = ";
        if constexpr (std::is_enum_v<typename decltype(field)::type>)
            std::cout << reflect::enum_name(value);
        else
            std::cout << std::boolalpha << value;
        std::cout << '\n';
    });
}

int main(int argc, char** argv) {
    std::vector<std::string_view> arguments(argv + 1, argv + argc);
    if (arguments.empty())
        arguments = {"--jobs=8", "--mode=Fast", "--verbose"};  // demo input

    print_help<Options>();

    Options options;
    if (!parse(arguments, options))
        return 1;

    std::cout << "values:\n";
    print_values(options);

    // What a typo looks like.
    Options untouched;
    parse({"--job=8"}, untouched);
    parse({"--mode=Quick"}, untouched);
}
