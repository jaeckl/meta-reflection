// Validation rules written next to the data they belong to.
//
// A rule is an annotation with a check() function. Adding a new kind of rule
// needs no change to the validator.

#include <reflect/reflect.hpp>

#include <iostream>
#include <string>
#include <vector>

struct Range {
    int min;
    int max;

    constexpr bool check(int value) const { return min <= value && value <= max; }
    std::string describe() const {
        return "must be between " + std::to_string(min) + " and " + std::to_string(max);
    }
};

struct NotEmpty {
    bool check(const std::string& value) const { return !value.empty(); }
    std::string describe() const { return "must not be empty"; }
};

struct MaxLength {
    std::size_t length;

    bool check(const std::string& value) const { return value.size() <= length; }
    std::string describe() const {
        return "must be at most " + std::to_string(length) + " characters";
    }
};

struct Signup {
    [[=NotEmpty{}, =MaxLength{12}]] std::string username;
    [[=Range{13, 120}]] int age = 0;
    [[=Range{0, 5}]] int newsletters = 0;
    std::string comment;  // no rules
};

// Runs every rule on every field and collects the messages.
template <typename T>
std::vector<std::string> validate(const T& object) {
    std::vector<std::string> errors;
    reflect::for_each_field(object, [&](auto field, const auto& value) {
        // `rule` is an annotation proxy; rule.value() is the Range, NotEmpty, ...
        reflect::for_each_annotation(field, [&](auto rule) {
            if (!rule.value().check(value))
                errors.push_back(std::string(field.name()) + " " +
                                 rule.value().describe());
        });
    });
    return errors;
}

// Stops at the first problem: enough for a yes/no answer.
template <typename T>
bool is_valid(const T& object) {
    return reflect::for_each_field_while(object, [](auto field, const auto& value) {
        return reflect::get_annotations_of(field).all_of(
            [&](auto rule) { return rule.value().check(value); });
    });
}

void report(const Signup& signup) {
    std::cout << '"' << signup.username << "\": "
              << (is_valid(signup) ? "valid" : "invalid") << '\n';
    for (const std::string& error : validate(signup))
        std::cout << "  " << error << '\n';
}

// A rule that only looks at constants can be enforced by the compiler.
static_assert(reflect::get_annotation<Range>(reflect::get_field_of<Signup, "age">())
                  .check(18));

int main() {
    report({"ada", 36, 1, ""});
    report({"", 7, 1, "hello"});
    report({"a_very_long_username", 30, 9, ""});
}
