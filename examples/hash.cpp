// Hashing and equality for a struct, so it can be a key in unordered
// containers, without writing either by hand.

#include <reflect/reflect.hpp>

#include <cstddef>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>

// Combines the hashes of all fields.
template <typename T>
std::size_t hash_fields(const T& object) {
    std::size_t seed = 0;
    reflect::for_each_field(object, [&](auto, const auto& value) {
        std::size_t hash = std::hash<std::remove_cvref_t<decltype(value)>>{}(value);
        seed ^= hash + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
    });
    return seed;
}

// Function objects that work for any struct whose fields can be hashed and
// compared.
struct FieldHash {
    template <typename T>
    std::size_t operator()(const T& object) const {
        return hash_fields(object);
    }
};

struct FieldEqual {
    template <typename T>
    bool operator()(const T& left, const T& right) const {
        return reflect::tie(left) == reflect::tie(right);
    }
};

struct Coordinate {
    int x;
    int y;
};

struct Route {
    std::string from;
    std::string to;
};

int main() {
    std::unordered_set<Coordinate, FieldHash, FieldEqual> visited;
    visited.insert({1, 2});
    visited.insert({3, 4});
    visited.insert({1, 2});  // already there

    std::cout << visited.size() << " coordinates\n";
    std::cout << std::boolalpha << visited.contains({3, 4}) << ' '
              << visited.contains({4, 3}) << '\n';

    std::unordered_map<Route, int, FieldHash, FieldEqual> distance;
    distance[{"Berlin", "Paris"}] = 1050;
    distance[{"Paris", "Berlin"}] = 1050;
    distance[{"Berlin", "Paris"}] += 4;

    std::cout << distance.size() << " routes, Berlin to Paris: "
              << distance[{"Berlin", "Paris"}] << '\n';
}
