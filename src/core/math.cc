// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/core/math.cc
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#include "asc/core/math.h"

namespace asc {

template <>
int Pow<int>(int base, int exp) {
  ASC_VERIFY(exp >= 0, "exponential power must be non-negative");

  int result = 1;
  for (;;) {
    if (exp & 1) result *= base;
    exp >>= 1;
    if (!exp) break;
    base *= base;
  }
  return result;
}

}  // namespace asc
