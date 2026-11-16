#include <reflect/reflect.hpp>

#include "check.hpp"

#include <array>
#include <concepts>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

struct Address {
    std::string city;
    int zip = 0;
};

struct Money {
    int cents = 0;
};

struct Entity {
    int id = 0;
};

struct Employee : Entity {
    std::string name;
    Address home;
    Address office;
    Money salary;
    std::array<int, 2> desk{};
    std::optional<int> badge;
};

class Sealed {
public:
    int open = 1;

private:
    int closed = 2;
};

struct Box {
    Sealed sealed;
    unsigned flag : 1 = 1;
};

struct Grid {
    struct Cell {
        int value = 0;
    };
    Cell a;
    Cell b;
    struct Row {
        Cell left;
        Cell right;
    } row;
};

// The walk works at compile time.
constexpr int sum_of(const Grid& grid) {
    int sum = 0;
    reflect::walk(grid, [&](reflect::field_path, auto, int value) { sum += value; });
    return sum;
}
static_assert(sum_of(Grid{{1}, {2}, {{3}, {4}}}) == 10);

constexpr std::size_t deepest(const Grid& grid) {
    std::size_t depth = 0;
    reflect::walk(grid, [&](reflect::field_path path, auto, int) {
        depth = path.size() > depth ? path.size() : depth;
    });
    return depth;
}
static_assert(deepest(Grid{}) == 3);

int main() {
    Employee employee{{7}, "Ada", {"London", 1000}, {"Paris", 7500}, {250000}, {4, 2}, 31};

    // The callback lists what it treats as a value; everything else is
    // walked into. Inherited fields come first.
    std::vector<std::string> lines;
    reflect::walk(employee, reflect::overloaded{
        [&](reflect::field_path path, auto, int value) {
            lines.push_back(path.to_string() + "=" + std::to_string(value));
        },
        [&](reflect::field_path path, auto, const std::string& value) {
            lines.push_back(path.to_string() + "=" + value);
        },
        // A struct the callback accepts is a value, not something to open.
        [&](reflect::field_path path, auto, const Money& value) {
            lines.push_back(path.to_string() + "=$" + std::to_string(value.cents / 100));
        },
        // Library types never leak their internals this way.
        [&]<std::size_t N>(reflect::field_path path, auto, const std::array<int, N>& value) {
            lines.push_back(path.to_string() + "=[" + std::to_string(value.size()) + "]");
        },
        [&](reflect::field_path path, auto, const std::optional<int>& value) {
            lines.push_back(path.to_string() + "=" + (value ? "set" : "unset"));
        },
    });
    CHECK((lines == std::vector<std::string>{
               "id=7", "name=Ada", "home.city=London", "home.zip=1000",
               "office.city=Paris", "office.zip=7500", "salary=$2500", "desk=[2]",
               "badge=set"}));

    // The path: depth, parts, the field's own name, and as text.
    std::vector<std::string> cities;
    reflect::walk(employee, reflect::overloaded{
        [&](reflect::field_path path, auto field, const std::string&) {
            if (path.size() == 2) {
                CHECK(path.back() == field.name());
                CHECK(path[1] == "city");
                cities.push_back(path.to_string("/"));
                std::ostringstream out;
                out << path;
                CHECK(out.str() == path.to_string());
            }
        },
        // Everything else that is not a class is ignored. Classes are left
        // to the walk.
        [](reflect::field_path, auto, const auto& value)
            requires(!std::is_class_v<std::remove_cvref_t<decltype(value)>>)
        {},
        [](reflect::field_path, auto, const std::array<int, 2>&) {},
        [](reflect::field_path, auto, const std::optional<int>&) {},
    });
    CHECK((cities == std::vector<std::string>{"home/city", "office/city"}));

    // Constrained generic lambdas work as well as overloads.
    int numbers = 0;
    reflect::walk(employee, reflect::overloaded{
        [&](reflect::field_path, auto, const std::integral auto&) { ++numbers; },
        [](reflect::field_path, auto, const std::string&) {},
        [](reflect::field_path, auto, const std::array<int, 2>&) {},
        [](reflect::field_path, auto, const std::optional<int>&) {},
    });
    CHECK(numbers == 4);  // id, home.zip, office.zip, salary.cents

    // Writing through the walk.
    reflect::walk(employee, reflect::overloaded{
        [](reflect::field_path, auto, int& value) { value = 0; },
        [](reflect::field_path, auto, std::string& value) { value.clear(); },
        [](reflect::field_path, auto, std::array<int, 2>&) {},
        [](reflect::field_path, auto, std::optional<int>& value) { value.reset(); },
    });
    CHECK(employee.id == 0 && employee.home.zip == 0 && employee.salary.cents == 0);
    CHECK(employee.name.empty() && employee.office.city.empty() && !employee.badge);

    // A const object hands out const values.
    const Employee& constant = employee;
    int constants = 0;
    reflect::walk(constant, [&](reflect::field_path, auto, auto& value) {
        static_assert(std::is_const_v<std::remove_reference_t<decltype(value)>>);
        ++constants;
    });
    CHECK(constants == 7);  // a callback that takes everything never descends

    // The access policy applies at every level; bit-fields arrive by value.
    Box box;
    std::vector<std::string> all, visible;
    reflect::walk(box, [&](reflect::field_path path, auto, std::integral auto value) {
        all.push_back(path.to_string() + "=" + std::to_string(value));
    });
    reflect::walk<reflect::public_access>(
        box, [&](reflect::field_path path, auto, std::integral auto value) {
            visible.push_back(path.to_string() + "=" + std::to_string(value));
        });
    CHECK((all == std::vector<std::string>{"sealed.open=1", "sealed.closed=2", "flag=1"}));
    CHECK((visible == std::vector<std::string>{"sealed.open=1", "flag=1"}));

    return test::result();
}
