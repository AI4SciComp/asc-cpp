// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.

#include <asc/core/execution_context.h>

namespace asc::detail {

bool SerialSupports(ExecutionCapability capability) noexcept {
  switch (capability) {
    case ExecutionCapability::kSynchronous:
    case ExecutionCapability::kHostMemory:
      return true;
    case ExecutionCapability::kAsynchronous:
    case ExecutionCapability::kPinnedHostMemory:
    case ExecutionCapability::kDeviceMemory:
    case ExecutionCapability::kManagedMemory:
      return false;
  }
  return false;
}

Result<MemoryResourcePtr> GetSerialMemoryResource(MemorySpace space) {
  if (space == MemorySpace::kHost) {
    return GetHostMemoryResource();
  }
  return Status::FromProvider(
      StatusCode::kUnsupported,
      "The serial provider does not implement the requested memory space",
      "serial", 0);
}

Status SynchronizeSerial() { return Status::Ok(); }

}  // namespace asc::detail
