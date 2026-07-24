// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.

#include <asc/core/execution_context.h>

#include <memory>
#include <utility>

namespace asc {
namespace detail {

bool SerialSupports(ExecutionCapability capability) noexcept;
Result<MemoryResourcePtr> GetSerialMemoryResource(MemorySpace space);
Status SynchronizeSerial();

class ExecutionContextState {
 public:
  explicit ExecutionContextState(ExecutionContextOptions options)
      : options_(std::move(options)) {}

  const ExecutionContextOptions& GetOptions() const noexcept {
    return options_;
  }

 private:
  ExecutionContextOptions options_;
};

}  // namespace detail
namespace {

bool IsValid(FallbackPolicy policy) noexcept {
  switch (policy) {
    case FallbackPolicy::kDisallow:
    case FallbackPolicy::kSameSpaceReference:
      return true;
  }
  return false;
}

bool IsValid(DeterminismPolicy policy) noexcept {
  switch (policy) {
    case DeterminismPolicy::kBestEffort:
    case DeterminismPolicy::kRequireDeterministic:
      return true;
  }
  return false;
}

}  // namespace

ExecutionContext ExecutionContext::Serial() {
  ExecutionContextOptions options;
  return ExecutionContext(
      std::make_shared<detail::ExecutionContextState>(std::move(options)));
}

Result<ExecutionContext> ExecutionContext::Create(
    const ExecutionContextOptions& options) {
  if (options.device_id < 0) {
    return Status(StatusCode::kInvalidArgument,
                  "Execution-context device identifier must be non-negative");
  }
  if (!IsValid(options.fallback)) {
    return Status(StatusCode::kInvalidArgument,
                  "Execution-context fallback policy is invalid");
  }
  if (!IsValid(options.determinism)) {
    return Status(StatusCode::kInvalidArgument,
                  "Execution-context determinism policy is invalid");
  }

  switch (options.backend) {
    case BackendKind::kSerial:
      if (options.device_id != 0) {
        return Status(StatusCode::kInvalidArgument,
                      "The serial provider accepts only device identifier zero");
      }
      return ExecutionContext(
          std::make_shared<detail::ExecutionContextState>(options));
    case BackendKind::kOpenMP:
      return Status::FromProvider(
          StatusCode::kUnavailable,
          "The canonical OpenMP provider is not available in Core M1",
          "openmp", 0);
    case BackendKind::kCuda:
      return Status::FromProvider(
          StatusCode::kUnavailable,
          "The canonical CUDA provider is not available in Core M1", "cuda",
          0);
  }
  return Status(StatusCode::kInvalidArgument,
                "Execution-context backend kind is invalid");
}

BackendKind ExecutionContext::GetBackend() const noexcept {
  return state_->GetOptions().backend;
}

int ExecutionContext::GetDeviceId() const noexcept {
  return state_->GetOptions().device_id;
}

FallbackPolicy ExecutionContext::GetFallbackPolicy() const noexcept {
  return state_->GetOptions().fallback;
}

DeterminismPolicy ExecutionContext::GetDeterminismPolicy() const noexcept {
  return state_->GetOptions().determinism;
}

bool ExecutionContext::Supports(ExecutionCapability capability) const noexcept {
  return detail::SerialSupports(capability);
}

Result<MemoryResourcePtr> ExecutionContext::GetMemoryResource(
    MemorySpace space) const {
  return detail::GetSerialMemoryResource(space);
}

Status ExecutionContext::Synchronize() const {
  return detail::SynchronizeSerial();
}

}  // namespace asc
