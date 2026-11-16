#include <reflect/reflect.hpp>

#include "check.hpp"

#include <string>
#include <vector>

struct Calc {
    int over(int) { return 1; }
    int over(double) { return 2; }
    int over(const std::string&) { return 3; }
    int over(int, int) const { return 4; }

    int& value() { return stored; }
    const int& value() const { return stored; }

    double area() const noexcept { return 1.5; }
    int add(int a, int b) const { return a + b; }
    std::string take(std::string text) && { return text + "!"; }
    static int twice(int x) { return 2 * x; }
    virtual int virt() { return 6; }

    int stored = 7;
};

// --- all overloads of a name -------------------------------------------------

static_assert(reflect::get_functions_of<Calc, "over">().size() == 4);
static_assert(reflect::get_functions_of<Calc, "value">().size() == 2);
static_assert(reflect::get_functions_of<Calc, "add">().size() == 1);
static_assert(reflect::get_functions_of<Calc, "nope">().empty());

static_assert(reflect::has_function<Calc, "over">());
static_assert(reflect::has_function<Calc, "twice">());
static_assert(!reflect::has_function<Calc, "stored">());  // a field
static_assert(!reflect::has_function<Calc, "Over">());

// The access form still works next to the name form.
static_assert(reflect::get_functions_of<Calc>().size() == 11);
static_assert(reflect::get_functions_of<Calc, reflect::public_access>().size() == 11);

// The overloads are ordinary function proxies in an ordinary list.
static_assert(reflect::get_functions_of<Calc, "over">().count_if(
                  [](auto function) { return function.is_const(); }) == 1);
static_assert(reflect::get_functions_of<Calc, "over">()
                  .filter([](auto function) { return function.arity() == 2; })
                  .size() == 1);

// --- one overload by signature -----------------------------------------------

using OverInt = decltype(reflect::get_function_of<Calc, "over", int(int)>());
using OverDouble = decltype(reflect::get_function_of<Calc, "over", int(double)>());
static_assert(std::same_as<OverInt::parameter_type<0>, int>);
static_assert(std::same_as<OverDouble::parameter_type<0>, double>);
static_assert(!std::same_as<OverInt, OverDouble>);

static_assert(reflect::get_function_of<Calc, "over", int(const std::string&)>().arity() == 1);
static_assert(reflect::get_function_of<Calc, "over", int(int, int) const>().arity() == 2);
static_assert(reflect::get_function_of<Calc, "value", const int&() const>().is_const());
static_assert(!reflect::get_function_of<Calc, "value", int&()>().is_const());
// noexcept is not part of the signature you write.
static_assert(reflect::get_function_of<Calc, "area", double() const>().is_noexcept());
static_assert(reflect::get_function_of<Calc, "take", std::string(std::string) &&>().arity() == 1);
static_assert(reflect::get_function_of<Calc, "twice", int(int)>().is_static());

// --- a unique name needs no signature ----------------------------------------

static_assert(reflect::get_function_of<Calc, "add">().arity() == 2);
static_assert(reflect::get_function_of<Calc, "twice">().is_static());

// --- by pointer --------------------------------------------------------------

static_assert(reflect::get_function_of<&Calc::add>().name() == "add");
static_assert(reflect::get_function_of<&Calc::virt>().is_virtual());
using ByPointer =
    decltype(reflect::get_function_of<static_cast<int (Calc::*)(double)>(&Calc::over)>());
static_assert(std::same_as<ByPointer, OverDouble>);

int main() {
    Calc calc;
    const Calc constant;

    // invoke<"name">: ordinary overload resolution, including conversions.
    CHECK(reflect::invoke<"over">(calc, 42) == 1);
    CHECK(reflect::invoke<"over">(calc, 4.5) == 2);
    CHECK(reflect::invoke<"over">(calc, std::string("x")) == 3);
    CHECK(reflect::invoke<"over">(calc, "literal") == 3);
    CHECK(reflect::invoke<"over">(calc, 1, 2) == 4);
    CHECK(reflect::invoke<"over">(calc, 'a') == 1);     // promotion to int
    CHECK(reflect::invoke<"over">(calc, 1.5f) == 2);    // promotion to double
    CHECK(reflect::invoke<"over">(constant, 1, 2) == 4);

    // Return types and constness are those of the chosen overload.
    static_assert(std::same_as<decltype(reflect::invoke<"value">(calc)), int&>);
    static_assert(std::same_as<decltype(reflect::invoke<"value">(constant)), const int&>);
    reflect::invoke<"value">(calc) = 9;
    CHECK(calc.stored == 9);
    CHECK(reflect::invoke<"value">(constant) == 7);

    CHECK(reflect::invoke<"add">(constant, 2, 3) == 5);
    CHECK(reflect::invoke<"area">(calc) == 1.5);
    CHECK(reflect::invoke<"take">(Calc{}, std::string("moved")) == "moved!");
    CHECK(reflect::invoke<"twice">(calc, 21) == 42);
    CHECK(reflect::invoke<"virt">(calc) == 6);

    // A proxy picked by signature calls exactly that overload, even if a
    // different one would win overload resolution for the argument.
    CHECK(reflect::get_function_of<Calc, "over", int(double)>().invoke(calc, 1) == 2);
    CHECK(reflect::get_function_of<Calc, "over", int(int)>().invoke(calc, 1) == 1);
    CHECK(ByPointer::invoke(calc, 1) == 2);

    std::vector<std::size_t> arities;
    reflect::get_functions_of<Calc, "over">().for_each(
        [&](auto function) { arities.push_back(function.arity()); });
    CHECK((arities == std::vector<std::size_t>{1, 1, 1, 2}));

    return test::result();
}
