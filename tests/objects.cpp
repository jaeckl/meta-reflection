#include <reflect/reflect.hpp>

#include "check.hpp"

#include <map>
#include <string>
#include <tuple>
#include <vector>

struct Point {
    int x = 0;
    int y = 0;
};

struct Person {
    std::string name;
    int age = 0;
    double score = 0;
};

class Account {
public:
    int number() const { return number_; }
    const std::string& owner() const { return owner_; }

private:
    int number_ = 0;
    std::string owner_;
};

struct Fixed {
    const int id;
    std::string label;
};

struct Packed {
    unsigned low : 4 = 0;
    unsigned high : 4 = 0;
};

// --- tie ---------------------------------------------------------------------

constexpr bool equal(const Point& a, const Point& b) {
    return reflect::tie(a) == reflect::tie(b);
}
static_assert(equal({1, 2}, {1, 2}));
static_assert(!equal({1, 2}, {1, 3}));

constexpr bool less(const Point& a, const Point& b) {
    return reflect::tie(a) < reflect::tie(b);
}
static_assert(less({1, 2}, {1, 3}));
static_assert(less({0, 9}, {1, 0}));
static_assert(!less({1, 2}, {1, 2}));

// --- make --------------------------------------------------------------------

static_assert(reflect::make<Point>([](auto field) {
                  return field.name() == "x" ? 3 : 4;
              }).y == 4);

int main() {
    // tie gives references into the object.
    Point point{1, 2};
    const Point constant{1, 2};
    static_assert(std::same_as<decltype(reflect::tie(point)), std::tuple<int&, int&>>);
    static_assert(std::same_as<decltype(reflect::tie(constant)),
                               std::tuple<const int&, const int&>>);
    std::get<0>(reflect::tie(point)) = 10;
    CHECK(point.x == 10);
    reflect::tie(point) = std::tuple{7, 8};
    CHECK(point.x == 7 && point.y == 8);

    auto [x, y] = reflect::tie(point);
    CHECK(x == 7 && y == 8);

    // Ordering for free.
    Person a{"Ada", 36, 1.5};
    Person b{"Ada", 37, 0.5};
    CHECK(reflect::tie(a) < reflect::tie(b));
    CHECK(reflect::tie(a) != reflect::tie(b));

    // With an access policy only the visible fields take part.
    Account hidden;
    CHECK(std::tuple_size_v<decltype(reflect::tie(hidden))> == 2);
    CHECK(std::tuple_size_v<decltype(reflect::tie<reflect::public_access>(hidden))> == 0);

    // Bit-fields are copied.
    Packed packed;
    packed.high = 3;
    static_assert(std::same_as<decltype(reflect::tie(packed)),
                               std::tuple<unsigned, unsigned>>);
    CHECK(std::get<1>(reflect::tie(packed)) == 3);

    // Two objects at once: which fields differ?
    std::vector<std::string> changed;
    reflect::for_each_field(a, b, [&](auto field, const auto& left, const auto& right) {
        if (left != right)
            changed.emplace_back(field.name());
    });
    CHECK((changed == std::vector<std::string>{"age", "score"}));

    // ... and copy only those.
    reflect::for_each_field(a, std::as_const(b),
                            [](auto, auto& target, const auto& source) {
                                if (target != source)
                                    target = source;
                            });
    CHECK(reflect::tie(a) == reflect::tie(b));

    // Early exit: validate until the first bad field.
    Person invalid{"", -1, 0};
    std::string first_error;
    int visited = 0;
    bool valid = reflect::for_each_field_while(
        invalid, [&](auto field, const auto& value) {
            ++visited;
            if (value == std::remove_cvref_t<decltype(value)>{}) {
                first_error = field.name();
                return false;
            }
            return true;
        });
    CHECK(!valid);
    CHECK(first_error == "name" && visited == 1);
    CHECK(reflect::for_each_field_while(a, [](auto, const auto&) { return true; }));

    // A field picked by a run-time name.
    Person person{"Ada", 36, 1.5};
    std::string text;
    auto to_text = [&](auto, const auto& value) {
        if constexpr (std::same_as<std::remove_cvref_t<decltype(value)>, std::string>)
            text = value;
        else
            text = std::to_string(value);
    };
    std::string wanted = "age";
    CHECK(reflect::visit_field(person, wanted, to_text));
    CHECK(text == "36");
    CHECK(reflect::visit_field(person, "name", to_text));
    CHECK(text == "Ada");
    CHECK(!reflect::visit_field(person, "missing", to_text));
    CHECK(text == "Ada");

    // visit_field can write as well.
    CHECK(reflect::visit_field(person, "age", [](auto, auto& value) {
        if constexpr (std::is_arithmetic_v<std::remove_reference_t<decltype(value)>>)
            value = 40;
    }));
    CHECK(person.age == 40);

    // Private fields are skipped under public_access.
    Account account;
    CHECK(reflect::visit_field(account, "number_", [](auto, auto&) {}));
    CHECK(!reflect::visit_field<reflect::public_access>(account, "number_",
                                                        [](auto, auto&) {}));

    // make: fill a struct from key/value input.
    std::map<std::string, std::string> input{
        {"name", "Grace"}, {"age", "45"}, {"score", "9.5"}};
    std::vector<std::string> order;
    auto parse = [&](auto field) -> typename decltype(field)::type {
        using Type = typename decltype(field)::type;
        order.emplace_back(field.name());
        const std::string& raw = input.at(std::string(field.name()));
        if constexpr (std::same_as<Type, std::string>)
            return raw;
        else if constexpr (std::same_as<Type, int>)
            return std::stoi(raw);
        else
            return std::stod(raw);
    };
    Person made = reflect::make<Person>(parse);
    CHECK(made.name == "Grace" && made.age == 45 && made.score == 9.5);
    CHECK((order == std::vector<std::string>{"name", "age", "score"}));

    // A class with private fields is default constructed and then assigned.
    Account made_account = reflect::make<Account>(
        [](auto field) -> typename decltype(field)::type {
            using Type = typename decltype(field)::type;
            if constexpr (std::same_as<Type, int>)
                return 12;
            else
                return "Ada";
        });
    CHECK(made_account.number() == 12 && made_account.owner() == "Ada");

    // An aggregate is brace-initialized, so const members are fine.
    Fixed fixed = reflect::make<Fixed>(
        [](auto field) -> std::remove_const_t<typename decltype(field)::type> {
            using Type = std::remove_const_t<typename decltype(field)::type>;
            if constexpr (std::same_as<Type, int>)
                return 5;
            else
                return "five";
        });
    CHECK(fixed.id == 5 && fixed.label == "five");

    return test::result();
}
