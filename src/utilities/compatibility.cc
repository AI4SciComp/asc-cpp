// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.

#include <cstdlib>

#include "asc/utilities/cli.h"
#include "asc/core/error.h"
#include "asc/core/globals.h"

namespace asc {
namespace detail {

[[noreturn]] void UtilitiesCompatibilityFailure(const Status& status) {
  ASC_VERIFY(false, status.message());
  std::abort();
}

}  // namespace detail

void OptionParser::Parse(int argc, const char* argv[]) {
  const Status status = TryParse(argc, argv);
  if (!status.ok()) {
    detail::UtilitiesCompatibilityFailure(status);
  }
}

void OptionParser::Parse(const std::string& filename) {
  const Status status = TryParseFile(filename);
  if (!status.ok()) {
    detail::UtilitiesCompatibilityFailure(status);
  }
}

void OptionParser::PrintHelp() const { PrintHelp(mout); }

void OptionParser::PrintUsage() const { PrintUsage(mout); }

}  // namespace asc
