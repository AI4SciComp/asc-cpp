// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/error.cc
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#include "asc/core/globals.h"
#include "asc/core/error.h"

namespace asc {

#ifdef ASC_USE_EXCEPTION
const char* ErrorException::what() const throw() { return msg.c_str(); }

static ErrorAction asc_error_action = kErrorThrow;
#else
static ErrorAction asc_error_action = kErrorAbort;
#endif

void SetErrorAction(ErrorAction action) {
  // Check if 'action' is valid.
  switch (action) {
    case kErrorAbort:
      break;
    case kErrorThrow:
#ifdef ASC_USE_EXCEPTION
      break;
#else
      Error(
          "SetErrorAction: kErrorThrow requires the build "
          "option ASC_USE_EXCEPTION=YES");
      return;
#endif
    default:
      asc::merr << "\n\nset_error_action: invalid action: " << action << '\n';
      Error();
      return;
  }
  asc_error_action = action;
}

ErrorAction GetErrorAction() { return asc_error_action; }

namespace internal {

// defined in globals.cc
extern bool mout_initialized;
extern bool merr_initialized;

}  // namespace internal

void Error(const char* msg) {
  std::ostream& os = internal::merr_initialized ? asc::merr : std::cerr;

  if (msg) {
    os << "\n\n" << msg << '\n';
  }

#ifdef ASC_USE_EXCEPTION
  if (asc_error_action == kErrorThrow) {
    throw ErrorException(msg);
  }
#endif
  std::abort();
}

void Warning(const char* msg) {
  std::ostream& os = internal::mout_initialized ? asc::mout : std::cout;
  if (msg) {
    os << "\n\n" << msg << std::endl;
  }
}

}  // namespace asc
