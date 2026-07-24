// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.

#include "asc/linalg/capabilities.h"

namespace asc {

BackendKind LinalgCapabilities::GetBackend() const noexcept {
  return BackendKind::kSerial;
}

std::string_view LinalgCapabilities::GetProviderName() const noexcept {
  return "serial-reference";
}

bool LinalgCapabilities::IsDeterministic() const noexcept { return true; }

bool LinalgCapabilities::Supports(LinalgOperation operation) const noexcept {
  switch (operation) {
    case LinalgOperation::kCopy:
    case LinalgOperation::kScal:
    case LinalgOperation::kAxpy:
    case LinalgOperation::kDot:
    case LinalgOperation::kNrm2:
    case LinalgOperation::kGemv:
    case LinalgOperation::kGemm:
      return true;
  }
  return false;
}

Result<LinalgCapabilities> GetLinalgCapabilities(
    const ExecutionContext& context) {
  if (context.GetBackend() == BackendKind::kSerial) {
    if (!context.Supports(ExecutionCapability::kSynchronous) ||
        !context.Supports(ExecutionCapability::kHostMemory)) {
      return Status(StatusCode::kUnsupported,
                    "Canonical Linalg requires synchronous host execution");
    }
    return LinalgCapabilities();
  }

  if (context.GetFallbackPolicy() == FallbackPolicy::kDisallow) {
    return Status(StatusCode::kUnavailable,
                  "The requested Linalg execution provider is unavailable");
  }
  if (!context.Supports(ExecutionCapability::kSynchronous) ||
      !context.Supports(ExecutionCapability::kHostMemory)) {
    return Status(StatusCode::kUnsupported,
                  "Serial-reference fallback requires synchronous host "
                  "execution");
  }
  return LinalgCapabilities();
}

}  // namespace asc
