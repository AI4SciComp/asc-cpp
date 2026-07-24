// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/random/sobol.cc
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#include "asc/random/sobol.h"

namespace asc {

void InitializeSobolDirectionNumbers(
    int64_t (&v)[kSobolDimMax2][kSobolLogMax]) {
#include "sobol_data.inc"  // NOLINT(build/include)
}

}  // namespace asc
