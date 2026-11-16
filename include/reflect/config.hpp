#pragma once

#define REFLECT_VERSION_MAJOR 0
#define REFLECT_VERSION_MINOR 1
#define REFLECT_VERSION_PATCH 0

// Fail early with a readable message instead of a wall of errors from <meta>.
#if !defined(__cpp_impl_reflection)
#error "reflect needs C++26 static reflection. With GCC 16 or newer compile with: -std=c++26 -freflection"
#endif
