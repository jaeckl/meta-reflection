// Input header for tests/codegen.cpp: classes a builder cannot be
// generated for. Each one has to produce an error at its own line.

#pragma once

#include <reflect/reflect.hpp>

namespace bad {

struct [[=reflect::generate_builder{}]] NoDefault {
    explicit NoDefault(int) {}
    int value = 0;
};

class [[=reflect::generate_builder{}]] HiddenRequired {
    [[=reflect::required]] int id = 0;
};

namespace {
struct [[=reflect::generate_builder{}]] Unreachable {
    int value = 0;
};

struct [[=reflect::registered]] Hidden {};
}  // namespace

}  // namespace bad
