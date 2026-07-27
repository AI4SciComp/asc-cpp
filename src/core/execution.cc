#include "asc/core/execution.h"

#include <cstring>
#include <memory>
#include <utility>

#include "execution_internal.h"

namespace asc {

const char* BackendName(Backend backend) noexcept {
  switch (backend) {
    case Backend::kSerial:
      return "serial";
    case Backend::kCuda:
      return "cuda";
    case Backend::kHip:
      return "hip";
    case Backend::kSycl:
      return "sycl";
  }
  return "unknown";
}

Device Device::Serial() noexcept {
  return Device{.backend = Backend::kSerial, .ordinal = 0};
}

ExecutionContext ExecutionContext::Serial() noexcept {
  return ExecutionContext(Backend::kSerial, Device::Serial(),
                          Determinism::kDeterministic);
}

Result<ExecutionContext> ExecutionContext::Create(Backend backend,
                                                  Device device,
                                                  Determinism determinism) {
  switch (backend) {
    case Backend::kSerial:
    case Backend::kCuda:
    case Backend::kHip:
    case Backend::kSycl:
      break;
    default:
      return Status(ErrorCode::kInvalidArgument,
                    "Backend is not a recognized enumerator");
  }
  switch (determinism) {
    case Determinism::kDeterministic:
    case Determinism::kBackendDefault:
      break;
    default:
      return Status(ErrorCode::kInvalidArgument,
                    "Determinism is not a recognized enumerator");
  }
  if (device.backend != backend) {
    return Status(ErrorCode::kInvalidArgument,
                  "Device backend does not match requested backend");
  }
  if (device.ordinal < 0) {
    return Status(ErrorCode::kInvalidArgument,
                  "Device ordinal cannot be negative");
  }
  if (backend == Backend::kSerial) {
    if (device.ordinal != 0) {
      return Status(ErrorCode::kUnavailable,
                    "Only serial CPU device ordinal zero is available");
    }
    return ExecutionContext(backend, device, determinism);
  }
  if (backend == Backend::kCuda) {
    return Status(ErrorCode::kUnavailable,
                  "CUDA contexts require the optional core CUDA factory");
  }
  return Status(ErrorCode::kUnsupported,
                "The requested backend has no approved core facet");
}

bool ExecutionContext::CanAccess(MemorySpace space) const noexcept {
  if (backend_ == Backend::kSerial) {
    return space == MemorySpace::kHost;
  }
  return state_ != nullptr && state_->CanAccess(space);
}

ExecutionContext::ExecutionContext(
    Backend backend, Device device, Determinism determinism,
    std::shared_ptr<const internal_core_execution::ExecutionState>
        state) noexcept
    : backend_(backend),
      device_(device),
      determinism_(determinism),
      state_(std::move(state)) {}

CompletionEvent::CompletionEvent(bool complete) noexcept
    : valid_(true), complete_(complete) {}

CompletionEvent::CompletionEvent(
    std::unique_ptr<internal_core_execution::CompletionState> state) noexcept
    : valid_(state != nullptr), state_(std::move(state)) {}

CompletionEvent::CompletionEvent(CompletionEvent&& other) noexcept
    : valid_(std::exchange(other.valid_, false)),
      complete_(std::exchange(other.complete_, false)),
      state_(std::move(other.state_)) {}

CompletionEvent& CompletionEvent::operator=(CompletionEvent&& other) noexcept {
  if (this != &other) {
    valid_ = std::exchange(other.valid_, false);
    complete_ = std::exchange(other.complete_, false);
    state_ = std::move(other.state_);
  }
  return *this;
}

CompletionEvent::~CompletionEvent() = default;

Result<bool> CompletionEvent::Query() const {
  if (!valid_) {
    return Status(ErrorCode::kInvalidState,
                  "A moved-from CompletionEvent cannot be queried");
  }
  if (complete_) {
    return true;
  }
  if (state_ == nullptr) {
    return Status(ErrorCode::kInternal,
                  "A pending CompletionEvent has no provider state");
  }
  auto queried = state_->Query();
  if (!queried.ok()) {
    return queried.status();
  }
  complete_ = *queried;
  return *queried;
}

Status CompletionEvent::Wait() const {
  if (!valid_) {
    return Status(ErrorCode::kInvalidState,
                  "A moved-from CompletionEvent cannot be waited on");
  }
  if (complete_) {
    return Status::Ok();
  }
  if (state_ == nullptr) {
    return Status(ErrorCode::kInternal,
                  "A pending CompletionEvent has no provider state");
  }
  const Status status = state_->Wait();
  if (!status.ok()) {
    return status;
  }
  complete_ = true;
  return Status::Ok();
}

Result<CompletionEvent> CopyBytes(const ExecutionContext& context,
                                  MutableMemoryView destination,
                                  ConstMemoryView source,
                                  std::size_t byte_count) {
  if (byte_count > source.size() || byte_count > destination.size()) {
    return Status(ErrorCode::kMemoryAccess,
                  "Copy byte count exceeds a memory view");
  }
  if (!source.valid() || !destination.valid()) {
    return Status(ErrorCode::kMemoryAccess,
                  "A nonempty memory view has a null address");
  }
  if (!context.CanAccess(source.space()) ||
      !context.CanAccess(destination.space())) {
    if (context.backend() == Backend::kSerial) {
      return Status(ErrorCode::kUnsupported,
                    "Serial execution can copy only host memory");
    }
    return Status(ErrorCode::kMemoryAccess,
                  "The execution context cannot access a memory view");
  }
  if (byte_count == 0) {
    return CompletionEvent(true);
  }
  if (context.backend() == Backend::kSerial) {
    if (source.data() == destination.data() &&
        source.space() == destination.space()) {
      return CompletionEvent(true);
    }
    std::memmove(destination.data(), source.data(), byte_count);
    return CompletionEvent(true);
  }
  if (context.state_ == nullptr) {
    return Status(ErrorCode::kUnavailable,
                  "The execution context has no provider state");
  }
  auto state = context.state_->CopyBytes(destination, source, byte_count);
  if (!state.ok()) {
    return state.status();
  }
  return CompletionEvent(std::move(*state));
}

}  // namespace asc
