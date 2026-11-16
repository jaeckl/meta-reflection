#include <reflect/reflect.hpp>

#include "check.hpp"

#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

struct Skip {};

struct Record {
    int id = 1;
    std::string name = "n";
    [[=Skip{}]] double weight = 2.5;
    int count = 4;

private:
    long hidden = 0;
};

struct Nothing {};

constexpr auto fields = reflect::get_fields_of<Record>();
constexpr auto none = reflect::get_fields_of<Nothing>();

constexpr auto is_int = [](auto field) {
    return std::same_as<typename decltype(field)::type, int>;
};
constexpr auto is_public = [](auto field) { return field.is_public(); };

// --- questions ---------------------------------------------------------------

static_assert(fields.any_of(is_int));
static_assert(!fields.all_of(is_int));
static_assert(!fields.none_of(is_int));
static_assert(fields.count_if(is_int) == 2);
static_assert(fields.count_if(is_public) == 4);
static_assert(fields.none_of([](auto field) { return field.is_bit_field(); }));

// The usual conventions for an empty list.
static_assert(!none.any_of(is_int));
static_assert(none.all_of(is_int));
static_assert(none.none_of(is_int));
static_assert(none.count_if(is_int) == 0);

// --- filter ------------------------------------------------------------------

static_assert(fields.filter(is_int).size() == 2);
static_assert(fields.filter(is_int).names()[1] == "count");
static_assert(fields.filter(is_public).filter(is_int).size() == 2);
static_assert(fields.filter([](auto) { return false; }).empty());
static_assert(none.filter(is_int).empty());

// A filtered list is an ordinary list again.
static_assert(std::same_as<decltype(fields.filter(is_int)),
                           reflect::list<reflect::field, ^^Record::id, ^^Record::count>>);
static_assert(fields.filter(is_public).without<Skip>().size() == 3);
static_assert(std::same_as<decltype(fields.filter(is_int).get<0>()),
                           reflect::field<^^Record::id>>);

// Works for every kind of list.
enum class Level { Low = 1, Mid = 5, High = 9 };
static_assert(reflect::get_enumerators_of<Level>()
                  .filter([](auto e) { return e.value() > Level::Low; })
                  .size() == 2);
static_assert(reflect::get_enumerators_of<Level>().any_of(
    [](auto e) { return e.name() == "Mid"; }));

// --- map ---------------------------------------------------------------------

static_assert(std::get<1>(fields.map([](auto field) { return field.name(); })) == "name");
static_assert(std::tuple_size_v<decltype(fields.map(is_int))> == 5);
static_assert(std::get<0>(fields.map([](auto field) { return field.offset(); })) == 0);

// --- structured use ----------------------------------------------------------

using Fields = reflect::fields_of_t<Record>;
static_assert(std::same_as<Fields, std::remove_const_t<decltype(fields)>>);
static_assert(std::tuple_size_v<Fields> == 5);
static_assert(std::same_as<std::tuple_element_t<2, Fields>,
                           reflect::field<^^Record::weight>>);

int main() {
    Record record;

    // Predicates used for a question may capture and read run-time state.
    int threshold = 3;
    bool any_large = reflect::get_fields_of<Record>().filter(is_int).any_of(
        [&](auto field) { return field.get(record) > threshold; });
    CHECK(any_large);

    threshold = 10;
    CHECK(reflect::get_fields_of<Record>().filter(is_int).none_of(
        [&](auto field) { return field.get(record) > threshold; }));

    // for_each_while stops at the first false.
    std::vector<std::string> visited;
    bool completed = fields.for_each_while([&](auto field) {
        visited.emplace_back(field.name());
        return field.name() != "name";
    });
    CHECK(!completed);
    CHECK((visited == std::vector<std::string>{"id", "name"}));

    visited.clear();
    CHECK(fields.for_each_while([&](auto field) {
        visited.emplace_back(field.name());
        return true;
    }));
    CHECK(visited.size() == 5);
    CHECK(none.for_each_while([](auto) { return false; }));

    // map can pull the values out of an object.
    auto values = fields.filter(is_public).map(
        [&](auto field) { return field.get(record); });
    static_assert(std::same_as<decltype(values),
                               std::tuple<int, std::string, double, int>>);
    CHECK(std::get<2>(values) == 2.5);

    // A list can be walked with template for, and taken apart by index.
    int ints = 0;
    template for (auto field : fields.filter(is_int))
        ints += field.get(record);
    CHECK(ints == 5);

    return test::result();
}
