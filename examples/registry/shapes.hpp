// The classes of the program. [[=reflect::registered]] puts a class into
// the registry that the build generates; nothing else lists the shapes.

#pragma once

#include <reflect/reflect.hpp>

namespace shapes {

struct Shape {
    virtual ~Shape() = default;
    virtual double area() const = 0;
};

struct [[=reflect::registered]] Circle : Shape {
    double radius = 1;
    double area() const override { return 3.14159 * radius * radius; }
};

struct [[=reflect::registered]] Square : Shape {
    double side = 2;
    double area() const override { return side * side; }
};

struct [[=reflect::registered]] Triangle : Shape {
    double base = 3;
    double height = 2;
    double area() const override { return base * height / 2; }
};

}  // namespace shapes
