// Equality, ordering and a field-by-field diff for any struct.

#include <reflect/reflect.hpp>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

struct Employee {
    std::string name;
    std::string team;
    int level;
    double salary;
};

// Equality and ordering: tie() gives a tuple of references, and tuples
// already know how to compare.
template <typename T>
bool equal(const T& left, const T& right) {
    return reflect::tie(left) == reflect::tie(right);
}

template <typename T>
bool less(const T& left, const T& right) {
    return reflect::tie(left) < reflect::tie(right);
}

// Diff: walk two objects side by side.
template <typename T>
void print_changes(const T& before, const T& after) {
    reflect::for_each_field(before, after,
                            [](auto field, const auto& old_value, const auto& new_value) {
                                if (old_value != new_value)
                                    std::cout << "  " << field.name() << ": " << old_value
                                              << " -> " << new_value << '\n';
                            });
}

int main() {
    Employee before{"Ada", "Compilers", 3, 5200.0};
    Employee after = before;
    after.team = "Runtime";
    after.level = 4;

    std::cout << std::boolalpha;
    std::cout << "equal to itself: " << equal(before, before) << '\n';
    std::cout << "equal after edit: " << equal(before, after) << '\n';

    std::cout << "changes:\n";
    print_changes(before, after);

    std::vector<Employee> staff{
        {"Grace", "Compilers", 5, 7100.0},
        {"Ada", "Runtime", 4, 5200.0},
        {"Ada", "Compilers", 3, 5200.0},
    };
    std::ranges::sort(staff, less<Employee>);

    std::cout << "sorted:\n";
    for (const Employee& employee : staff)
        std::cout << "  " << employee.name << ", " << employee.team << '\n';
}
