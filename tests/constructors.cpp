#include <reflect/reflect.hpp>

#include "check.hpp"

#include <map>
#include <string>
#include <string_view>
#include <vector>

struct Doc {
    reflect::static_string text;
};

class User {
public:
    User() = default;
    explicit User(int id) : id_(id) {}
    User(int id, std::string name, [[=Doc{"in years"}]] int age = 30) noexcept
        : id_(id), name_(std::move(name)), age_(age) {}
    User(const User&) = default;
    User(User&&) = default;

    int id() const { return id_; }
    const std::string& name() const { return name_; }
    int age() const { return age_; }

private:
    User(std::string_view secret, bool) : name_(secret) {}

    int id_ = 0;
    std::string name_ = "?";
    int age_ = 0;
};

// Not an aggregate and not default constructible: out of reach for make.
class Connection {
public:
    Connection(std::string host, int port, bool tls)
        : host_(std::move(host)), port_(port), tls_(tls) {}

    const std::string& host() const { return host_; }
    int port() const { return port_; }
    bool tls() const { return tls_; }

private:
    std::string host_;
    int port_;
    bool tls_;
};

struct Fraction {
    int num;
    int den;
    constexpr Fraction(int n, int d) : num(n), den(d) {}
};

struct Point {
    int x = 0;
    int y = 0;
};

// Overloads that only differ in the parameter type.
struct Width {
    int which;
    Width(int) : which(1) {}
    Width(long) : which(2) {}
    Width(const std::string&) : which(3) {}
    Width(std::string&&) : which(4) {}
    Width(const int value, const char* const) : which(5 + value) {}
};

// Can be neither copied nor moved.
struct Pinned {
    int value;
    explicit Pinned(int v) : value(v) {}
    Pinned(const Pinned&) = delete;
    Pinned& operator=(const Pinned&) = delete;
};

struct Removed {
    Removed(int) = delete;
    Removed(double) {}
};

// --- discovery ---------------------------------------------------------------

// Default, explicit and the three-argument one, plus the private one. Copy
// and move are not listed.
static_assert(reflect::get_constructors_of<User>().size() == 4);
static_assert(reflect::get_constructors_of<User, reflect::public_access>().size() == 3);
static_assert(reflect::get_constructors_of<Connection>().size() == 1);
static_assert(reflect::get_constructors_of<Removed>().size() == 1);

// The default constructor the compiler declares is a constructor too.
static_assert(reflect::get_constructors_of<Point>().size() == 1);
static_assert(reflect::get_constructors_of<Point>().get<0>().is_default());

// --- lookup by parameter types -----------------------------------------------

constexpr auto by_default = reflect::get_constructor_of<User>();
constexpr auto by_id = reflect::get_constructor_of<User, int>();
constexpr auto by_all = reflect::get_constructor_of<User, int, std::string, int>();
constexpr auto hidden = reflect::get_constructor_of<User, std::string_view, bool>();

static_assert(by_default.arity() == 0 && by_default.is_default());
static_assert(by_id.arity() == 1 && by_id.is_explicit() && !by_id.is_default());
static_assert(by_all.arity() == 3 && !by_all.is_explicit());
static_assert(by_all.is_noexcept() && !by_id.is_noexcept());
static_assert(by_all.is_public() && hidden.is_private());
static_assert(!by_all.is_copy() && !by_all.is_move());
static_assert(std::same_as<decltype(by_all)::declaring_type, User>);
static_assert(std::same_as<decltype(by_all)::parameter_type<1>, std::string>);

// Top-level const is not part of a signature; references are.
static_assert(reflect::get_constructor_of<Width, int, const char*>().arity() == 2);
static_assert(!std::same_as<decltype(reflect::get_constructor_of<Width, const std::string&>()),
                            decltype(reflect::get_constructor_of<Width, std::string&&>())>);

// --- parameters --------------------------------------------------------------

static_assert(reflect::get_parameter_names_of(by_all)[1] == "name");
static_assert(reflect::get_parameter_of<2>(by_all).has_default_argument());
static_assert(reflect::get_parameter_of<2>(by_all).get_annotation<Doc>().text == "in years");
static_assert(!reflect::get_parameter_of<0>(by_all).has_annotation<Doc>());
static_assert(reflect::get_parameter_names_of(by_default).empty());

// --- at compile time ---------------------------------------------------------

constexpr Fraction half = reflect::get_constructor_of<Fraction, int, int>().invoke(1, 2);
static_assert(half.num == 1 && half.den == 2);

constexpr Fraction third = reflect::construct<Fraction>(
    [](auto parameter) { return parameter.name() == "n" ? 1 : 3; });
static_assert(third.num == 1 && third.den == 3);

template <typename V>
V parse(const std::string& text) {
    if constexpr (std::same_as<V, bool>)
        return text == "true";
    else if constexpr (std::same_as<V, int>)
        return std::stoi(text);
    else
        return V(text);
}

int main() {
    // invoke runs exactly the constructor it was asked for.
    User ada = by_all.invoke(7, "Ada", 36);
    CHECK(ada.id() == 7 && ada.name() == "Ada" && ada.age() == 36);
    CHECK(by_id.invoke(5).id() == 5);
    CHECK(by_default.invoke().name() == "?");

    CHECK(reflect::get_constructor_of<Width, int>().invoke(1).which == 1);
    CHECK(reflect::get_constructor_of<Width, long>().invoke(1).which == 2);
    std::string text = "wide";
    CHECK(reflect::get_constructor_of<Width, const std::string&>().invoke(text).which == 3);
    CHECK(reflect::get_constructor_of<Width, std::string&&>().invoke(std::move(text)).which == 4);
    CHECK(reflect::get_constructor_of<Width, int, const char*>().invoke(1, "x").which == 6);
    CHECK(reflect::get_constructor_of<Removed, double>().arity() == 1);

    // A class that can be neither copied nor moved is built in place.
    Pinned pinned = reflect::get_constructor_of<Pinned, int>().invoke(4);
    CHECK(pinned.value == 4);
    Pinned again = reflect::construct<Pinned>([](auto) { return 9; });
    CHECK(again.value == 9);

    // construct asks for one argument per parameter, from left to right.
    std::map<std::string, std::string> input{
        {"host", "example.org"}, {"port", "443"}, {"tls", "true"}};
    std::vector<std::string> order;
    Connection connection = reflect::construct<Connection>([&](auto parameter) {
        order.emplace_back(parameter.name());
        return parse<typename decltype(parameter)::type>(input.at(order.back()));
    });
    CHECK(connection.host() == "example.org" && connection.port() == 443 &&
          connection.tls());
    CHECK((order == std::vector<std::string>{"host", "port", "tls"}));

    // With several constructors, say which one.
    User bob = reflect::construct(by_all, [](auto parameter) -> typename decltype(parameter)::type {
        if constexpr (std::same_as<typename decltype(parameter)::type, std::string>)
            return "Bob";
        else
            return parameter.index() == 0 ? 2 : 41;
    });
    CHECK(bob.id() == 2 && bob.name() == "Bob" && bob.age() == 41);

    // The callback may return anything that converts to the parameter type.
    User eve = reflect::construct(by_all, [](auto parameter) {
        if constexpr (std::same_as<typename decltype(parameter)::type, std::string>)
            return "Eve";
        else
            return short(3);
    });
    CHECK(eve.name() == "Eve" && eve.age() == 3);

    // Constructors as a list.
    std::vector<std::size_t> arities;
    int explicit_ones = 0;
    reflect::for_each_constructor<User, reflect::public_access>([&](auto constructor) {
        arities.push_back(constructor.arity());
        if constexpr (constructor.is_explicit())
            ++explicit_ones;
    });
    CHECK((arities == std::vector<std::size_t>{0, 1, 3}));
    CHECK(explicit_ones == 1);

    std::string signature;
    reflect::for_each_parameter(by_all, [&](auto parameter) {
        signature += parameter.name();
        signature += parameter.has_default_argument() ? "? " : " ";
    });
    CHECK(signature == "id name age? ");

    // The raw member view leads to the same proxy, copy and move included.
    int copies = 0, moves = 0, total = 0;
    reflect::for_each_member<User>([&](auto member) {
        if constexpr (member.kind() == reflect::member_kind::constructor) {
            ++total;
            copies += member.as_constructor().is_copy();
            moves += member.as_constructor().is_move();
        }
    });
    CHECK(total == 6 && copies == 1 && moves == 1);

    return test::result();
}
