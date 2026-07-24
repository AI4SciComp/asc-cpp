// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/globals.cc
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string>
#include <sstream>
#include "asc/core/globals.h"

namespace asc {

OutStream mout(std::cout);
OutStream merr(std::cerr);

namespace internal {

bool mout_initialized = false;
bool merr_initialized = false;

}  // namespace internal

void OutStream::Enable() {
  if (!IsEnabled()) {
    rdbuf(rdbuf_);
    tie(tie_);
  }
}

void OutStream::Disable() {
  if (IsEnabled()) {
    rdbuf_ = rdbuf(NULL);
    tie_ = tie(NULL);
  }
}

}  // namespace asc
