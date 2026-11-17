// Input header for tests/registry.cpp: classes for two registries.

#pragma once

#include <reflect/reflect.hpp>

#include <string>

// Annotations of the project, each with a registry of its own.
struct Component {};
struct Unused {};

namespace shapes {

struct Shape {
    virtual ~Shape() = default;
    virtual std::string name() const = 0;
};

struct [[=reflect::registered]] Circle : Shape {
    std::string name() const override { return "circle"; }
};

struct [[=reflect::registered, =Component{}]] Square : Shape {
    std::string name() const override { return "square"; }
};

// Registered, but it cannot be created without arguments.
struct [[=reflect::registered]] Polygon : Shape {
    explicit Polygon(int sides) : sides(sides) {}
    std::string name() const override { return "polygon"; }
    int sides;
};

// Registered, but abstract.
struct [[=reflect::registered]] Outline : Shape {};

// Registered, but not a Shape.
struct [[=reflect::registered]] Canvas {
    int width = 80;
};

struct Unmarked : Shape {
    std::string name() const override { return "unmarked"; }
};

namespace detail {
struct [[=Component{}]] Helper {};
}  // namespace detail

}  // namespace shapes
