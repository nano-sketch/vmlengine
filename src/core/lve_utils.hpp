#pragma once

#include <functional>

/**
 * general utilities for the vlm engine.
 * includes helpers for hashing and path management.
 */

#ifndef ENGINE_DIR
#define ENGINE_DIR "../"
#endif

namespace lve {

inline void hashCombine(std::size_t& seed) {}

template <typename T, typename... Rest>
void hashCombine(std::size_t& seed, const T& v, const Rest&... rest) {
  seed ^= std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  hashCombine(seed, rest...);
}

}  // namespace lve
