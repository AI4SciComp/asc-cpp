// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/casts.cc
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#include "asc/core/casts.h"

#include <bit>

namespace asc {

int SizeToInt(size_t x) {
  ASC_VERIFY(x <= static_cast<size_t>(std::numeric_limits<int>::max()),
                "size_t value is too big to fit into an int.");
  return static_cast<int>(x);
}

// Fast cast of 32-bit unsigned int to float in [0..1) interval.
// See: http://xoshiro.di.unimi.it/#remarks, "Generating uniform doubles in
// the unit interval"
float UInt32ToFloat(uint32_t x) {
  const uint32_t bits = 0x3f800000ul | (x >> 9);
  return std::bit_cast<float>(bits) - 1.0f;
}

// Fast cast of 64-bit unsigned int to double in [0..1) interval.
// See: http://xoshiro.di.unimi.it/#remarks, "Generating uniform doubles in
// the unit interval"
double UInt64ToDouble(uint64_t x) {
  const uint64_t bits = 0x3ff0000000000000ull | (x >> 12);
  return std::bit_cast<double>(bits) - 1.0;
}

}  // namespace asc
