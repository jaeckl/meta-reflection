#include <reflect/reflect.hpp>

#include "check.hpp"

#include <string>
#include <vector>

struct Tag {
    int id;
};

class Shape {
public:
    virtual ~Shape() = default;
    virtual double area() const noexcept { return 1.5; }
    int scaled(int factor) { return factor * 2; }
    int scaled(double) { return -1; }
    int only_lvalue() & { return 3; }
    [[=Tag{3}]] int tagged(int a, const std::string& b) const {
        return a + int(b.size());
    }
    static int count() { return 5; }

    // None of these are "named functions".
    template <typename T>
    void generic(T) {}
    bool operator==(const Shape&) const { return true; }
    explicit operator bool() const { return true; }
    void removed() = delete;

protected:
    void guarded() {}

private:
    void internal() {}
};

// Named functions only, each overload on its own.
static_assert(reflect::get_functions_of<Shape>().size() == 8);
static_assert(reflect::get_functions_of<Shape, reflect::public_access>().size() == 6);
static_assert(reflect::get_functions_of<Shape>().names()[0] == "area");
static_assert(reflect::get_functions_of<Shape>().names()[1] == "scaled");
static_assert(reflect::get_functions_of<Shape>().names()[2] == "scaled");
static_assert(reflect::get_functions_of<Tag>().empty());

using Area = decltype(reflect::get_functions_of<Shape>().get<0>());
using ScaledInt = decltype(reflect::get_functions_of<Shape>().get<1>());
using ScaledDouble = decltype(reflect::get_functions_of<Shape>().get<2>());
using Tagged = decltype(reflect::get_functions_of<Shape>().get<4>());
using Count = decltype(reflect::get_functions_of<Shape>().get<5>());
using Guarded = decltype(reflect::get_functions_of<Shape>().get<6>());
using Internal = decltype(reflect::get_functions_of<Shape>().get<7>());

static_assert(Area::name() == "area");
static_assert(Area::arity() == 0);
static_assert(Area::is_const() && Area::is_virtual() && Area::is_noexcept());
static_assert(!Area::is_static() && Area::is_public());
static_assert(std::same_as<Area::return_type, double>);

static_assert(std::same_as<ScaledInt::parameter_type<0>, int>);
static_assert(std::same_as<ScaledDouble::parameter_type<0>, double>);
static_assert(!ScaledInt::is_const() && !ScaledInt::is_virtual());

static_assert(Tagged::arity() == 2);
static_assert(std::same_as<Tagged::parameter_type<1>, const std::string&>);
static_assert(reflect::has_annotation<Tag>(Tagged{}));
static_assert(reflect::get_annotation<Tag>(Tagged{}).id == 3);
static_assert(!reflect::has_annotation<Tag>(Area{}));
static_assert(reflect::get_functions_of<Shape>().with<Tag>().size() == 1);

static_assert(Count::is_static() && Count::name() == "count");
static_assert(Count::pointer() == &Shape::count);
static_assert(Guarded::is_protected());
static_assert(Internal::is_private());

int main() {
    Shape shape;
    const Shape constant;

    CHECK(Area::invoke(shape) == 1.5);
    CHECK(Area::invoke(constant) == 1.5);
    CHECK(ScaledInt::invoke(shape, 21) == 42);
    CHECK(ScaledDouble::invoke(shape, 21) == -1);
    CHECK(Tagged::invoke(constant, 1, "abc") == 4);
    CHECK(Count::invoke() == 5);
    CHECK((shape.*Tagged::pointer())(2, "ab") == 4);

    std::vector<std::string> names;
    reflect::for_each_function<Shape, reflect::public_access>(
        [&](auto function) { names.emplace_back(function.name()); });
    CHECK(names.size() == 6);
    CHECK(names.front() == "area" && names.back() == "count");

    // Call whatever takes no arguments and returns an int.
    int total = 0;
    reflect::for_each_function<Shape>([&](auto function) {
        using Function = decltype(function);
        if constexpr (function.arity() == 0 &&
                      std::same_as<typename Function::return_type, int>) {
            if constexpr (function.is_static())
                total += function.invoke();
            else
                total += function.invoke(shape);
        }
    });
    CHECK(total == 3 + 5);

    return test::result();
}
