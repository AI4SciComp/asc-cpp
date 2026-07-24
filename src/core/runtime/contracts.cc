// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.

#include <asc/core/contracts.h>

#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace asc {
namespace {

std::string_view ContractKindName(ContractKind kind) {
  switch (kind) {
    case ContractKind::kPrecondition:
      return "precondition";
    case ContractKind::kPostcondition:
      return "postcondition";
    case ContractKind::kInvariant:
      return "invariant";
  }
  return "contract";
}

}  // namespace

void ContractFailure(ContractKind kind, std::string_view expression,
                     std::string_view message,
                     const std::source_location& location) {
  std::ostringstream output;
  output << "ASC " << ContractKindName(kind) << " failed: (" << expression
         << ")";
  if (!message.empty()) {
    output << "\n --> " << message;
  }
  output << "\n ... in function: " << location.function_name()
         << "\n ... in file: " << location.file_name() << ':'
         << location.line() << '\n';
  const std::string text = output.str();

#ifdef ASC_USE_EXCEPTION
  throw ContractException(text);
#else
  std::fwrite(text.data(), sizeof(char), text.size(), stderr);
  std::abort();
#endif
}

}  // namespace asc
