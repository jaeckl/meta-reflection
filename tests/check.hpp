#pragma once

// A deliberately tiny test helper, so the tests need nothing but the library.
// Most checks are static_asserts; CHECK covers what only exists at run time.

#include <cstdio>

namespace test {

inline int failures = 0;

inline int result() {
    if (failures == 0)
        std::puts("ok");
    return failures == 0 ? 0 : 1;
}

}  // namespace test

#define CHECK(expression)                                                  \
    do {                                                                   \
        if (!(expression)) {                                               \
            ++test::failures;                                              \
            std::printf("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__,   \
                        #expression);                                      \
        }                                                                  \
    } while (false)
