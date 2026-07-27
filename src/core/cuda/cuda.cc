#include "asc/core/providers/cuda.h"

#include <cuda_runtime_api.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <utility>

#include "../execution_internal.h"
#include "provider_internal.h"

namespace asc {
namespace internal_core_cuda {
namespace {

Status CudaStatus(cudaError_t error, ErrorCode code, const char* operation) {
  if (error == cudaSuccess) {
    return Status::Ok();
  }
  // Consume the runtime thread's sticky error state when this returned error
  // is translated into asc::Status. A later valid operation must not observe
  // an error that this call has already reported.
  static_cast<void>(cudaGetLastError());
  return Status(code, std::string(operation) + ": " + cudaGetErrorString(error),
                "cuda", static_cast<std::int64_t>(error));
}

bool IsPowerOfTwo(std::size_t value) noexcept {
  return value != 0 && (value & (value - 1U)) == 0;
}

void ClearPriorCudaError() noexcept { static_cast<void>(cudaGetLastError()); }

struct RegisteredCudaAllocation {
  std::uintptr_t begin = 0;
  std::size_t bytes = 0;
  Device device;
  MemorySpace space = MemorySpace::kDevice;
  RegisteredCudaAllocation* next = nullptr;
};

std::mutex& AllocationRegistryMutex() {
  static std::mutex mutex;
  return mutex;
}

RegisteredCudaAllocation*& AllocationRegistryHead() {
  static RegisteredCudaAllocation* head = nullptr;
  return head;
}

bool RegisterCudaAllocation(void* pointer, std::size_t bytes, Device device,
                            MemorySpace space) noexcept {
  auto* allocation = new (std::nothrow) RegisteredCudaAllocation{
      .begin = reinterpret_cast<std::uintptr_t>(pointer),
      .bytes = bytes,
      .device = device,
      .space = space,
  };
  if (allocation == nullptr) {
    return false;
  }
  std::lock_guard lock(AllocationRegistryMutex());
  allocation->next = AllocationRegistryHead();
  AllocationRegistryHead() = allocation;
  return true;
}

void UnregisterCudaAllocation(const void* pointer) noexcept {
  const auto address = reinterpret_cast<std::uintptr_t>(pointer);
  RegisteredCudaAllocation* removed = nullptr;
  {
    std::lock_guard lock(AllocationRegistryMutex());
    auto** link = &AllocationRegistryHead();
    while (*link != nullptr) {
      if ((*link)->begin == address) {
        removed = *link;
        *link = removed->next;
        break;
      }
      link = &(*link)->next;
    }
  }
  delete removed;
}

Status ValidateRegisteredCudaRange(Device device, const void* pointer,
                                   std::size_t bytes, MemorySpace space) {
  const auto begin = reinterpret_cast<std::uintptr_t>(pointer);
  std::lock_guard lock(AllocationRegistryMutex());
  for (const RegisteredCudaAllocation* allocation = AllocationRegistryHead();
       allocation != nullptr; allocation = allocation->next) {
    if (begin < allocation->begin ||
        begin - allocation->begin >= allocation->bytes) {
      continue;
    }
    const std::size_t offset =
        static_cast<std::size_t>(begin - allocation->begin);
    if (bytes > allocation->bytes - offset) {
      return Status(
          ErrorCode::kMemoryAccess,
          "A CUDA memory view exceeds its registered allocation span");
    }
    if (allocation->device != device || allocation->space != space) {
      return Status(ErrorCode::kMemoryAccess,
                    "A CUDA memory view metadata disagrees with its registered "
                    "allocation");
    }
    break;
  }
  return Status::Ok();
}

class CudaExecutionState final
    : public internal_core_execution::ExecutionState,
      public std::enable_shared_from_this<CudaExecutionState> {
 public:
  CudaExecutionState(Device device, cudaStream_t stream) noexcept
      : device_(device), stream_(stream) {}

  ~CudaExecutionState() override {
    ClearPriorCudaError();
    auto guard = CudaDeviceGuard::Create(device_.ordinal);
    if (!guard.ok()) {
      return;
    }
    if (stream_ != nullptr) {
      static_cast<void>(cudaStreamDestroy(stream_));
    }
  }

  [[nodiscard]] bool CanAccess(MemorySpace space) const noexcept override {
    switch (space) {
      case MemorySpace::kHost:
      case MemorySpace::kPinnedHost:
      case MemorySpace::kDevice:
      case MemorySpace::kManaged:
        return true;
    }
    return false;
  }

  [[nodiscard]] Result<
      std::unique_ptr<internal_core_execution::CompletionState>>
  CopyBytes(MutableMemoryView destination, ConstMemoryView source,
            std::size_t byte_count) const override;

  [[nodiscard]] Device device() const noexcept { return device_; }
  [[nodiscard]] cudaStream_t stream() const noexcept { return stream_; }

 private:
  Device device_;
  cudaStream_t stream_;
};

class CudaCompletionState final
    : public internal_core_execution::CompletionState {
 public:
  CudaCompletionState(
      cudaEvent_t event,
      std::shared_ptr<const CudaExecutionState> execution_state) noexcept
      : event_(event), execution_state_(std::move(execution_state)) {}

  ~CudaCompletionState() override {
    ClearPriorCudaError();
    auto guard = CudaDeviceGuard::Create(execution_state_->device().ordinal);
    if (!guard.ok()) {
      return;
    }
    if (event_ != nullptr) {
      static_cast<void>(cudaEventDestroy(event_));
    }
  }

  [[nodiscard]] Result<bool> Query() const override {
    ClearPriorCudaError();
    auto guard = CudaDeviceGuard::Create(execution_state_->device().ordinal);
    if (!guard.ok()) {
      return guard.status();
    }
    const cudaError_t error = cudaEventQuery(event_);
    if (error == cudaSuccess) {
      return true;
    }
    if (error == cudaErrorNotReady) {
      // Not-ready is the successful pending-state result of Query(), not a
      // provider failure. Consume its sticky Runtime error before the device
      // guard restores the caller's current device.
      static_cast<void>(cudaGetLastError());
      return false;
    }
    return CudaStatus(error, ErrorCode::kProvider, "cudaEventQuery");
  }

  [[nodiscard]] Status Wait() const override {
    ClearPriorCudaError();
    auto guard = CudaDeviceGuard::Create(execution_state_->device().ordinal);
    if (!guard.ok()) {
      return guard.status();
    }
    return CudaStatus(cudaEventSynchronize(event_), ErrorCode::kProvider,
                      "cudaEventSynchronize");
  }

  [[nodiscard]] cudaEvent_t event() const noexcept { return event_; }
  [[nodiscard]] const std::shared_ptr<const CudaExecutionState>&
  execution_state() const noexcept {
    return execution_state_;
  }

 private:
  cudaEvent_t event_;
  std::shared_ptr<const CudaExecutionState> execution_state_;
};

Result<std::shared_ptr<const CudaExecutionState>> CudaState(
    const ExecutionContext& context) {
  if (context.backend() != Backend::kCuda) {
    return Status(ErrorCode::kInvalidArgument,
                  "A CUDA operation requires a CUDA execution context");
  }
  const auto& state = internal_core_execution::ExecutionAccess::State(context);
  auto cuda_state = std::dynamic_pointer_cast<const CudaExecutionState>(state);
  if (cuda_state == nullptr) {
    return Status(ErrorCode::kInvalidState,
                  "The CUDA execution context has invalid provider state");
  }
  return cuda_state;
}

Status ValidateCudaMemoryForDevice(Device device, const void* pointer,
                                   std::size_t bytes, MemorySpace space) {
  if (bytes == 0) {
    return Status::Ok();
  }
  if (pointer == nullptr) {
    return Status(ErrorCode::kMemoryAccess,
                  "A nonempty CUDA memory view has a null address");
  }
  if (space != MemorySpace::kHost && space != MemorySpace::kPinnedHost &&
      space != MemorySpace::kDevice && space != MemorySpace::kManaged) {
    return Status(ErrorCode::kInvalidArgument,
                  "A CUDA memory view has an invalid memory space");
  }
  auto guard = CudaDeviceGuard::Create(device.ordinal);
  if (!guard.ok()) {
    return guard.status();
  }
  cudaPointerAttributes attributes{};
  const cudaError_t error = cudaPointerGetAttributes(&attributes, pointer);
  if (space == MemorySpace::kHost && error == cudaErrorInvalidValue) {
    // CUDA versions/drivers may report an unregistered pageable host pointer
    // as invalid rather than returning cudaMemoryTypeUnregistered. Consume the
    // sticky runtime error because it is the expected classification result.
    static_cast<void>(cudaGetLastError());
    return Status::Ok();
  }
  if (error != cudaSuccess) {
    return CudaStatus(error, ErrorCode::kMemoryAccess,
                      "cudaPointerGetAttributes");
  }
  if (space == MemorySpace::kHost) {
    if (attributes.type == cudaMemoryTypeUnregistered) {
      return Status::Ok();
    }
    return Status(
        ErrorCode::kMemoryAccess,
        "A pageable-host view names CUDA-tracked memory with another space");
  }
  if (space == MemorySpace::kPinnedHost &&
      attributes.type != cudaMemoryTypeHost) {
    return Status(ErrorCode::kMemoryAccess,
                  "A pinned-host view does not name CUDA host memory");
  }
  if (space == MemorySpace::kDevice &&
      attributes.type != cudaMemoryTypeDevice) {
    return Status(ErrorCode::kMemoryAccess,
                  "A device view does not name CUDA device memory");
  }
  if (space == MemorySpace::kManaged &&
      attributes.type != cudaMemoryTypeManaged) {
    return Status(ErrorCode::kMemoryAccess,
                  "A managed view does not name CUDA managed memory");
  }
  if ((space == MemorySpace::kDevice || space == MemorySpace::kManaged) &&
      attributes.device != device.ordinal) {
    return Status(ErrorCode::kUnsupported,
                  "Cross-device CUDA memory access is not supported");
  }
  return ValidateRegisteredCudaRange(device, pointer, bytes, space);
}

Result<std::unique_ptr<CudaCompletionState>> CreateEventState(
    const std::shared_ptr<const CudaExecutionState>& state) {
  auto guard = CudaDeviceGuard::Create(state->device().ordinal);
  if (!guard.ok()) {
    return guard.status();
  }
  cudaEvent_t event = nullptr;
  cudaError_t error = cudaEventCreateWithFlags(&event, cudaEventDisableTiming);
  if (error != cudaSuccess) {
    return CudaStatus(error, ErrorCode::kProvider, "cudaEventCreateWithFlags");
  }
  return std::make_unique<CudaCompletionState>(event, state);
}

Status ValidateAddressSpan(const void* pointer, std::size_t bytes) {
  const auto begin = reinterpret_cast<std::uintptr_t>(pointer);
  if (bytes > std::numeric_limits<std::uintptr_t>::max() - begin) {
    return Status(ErrorCode::kOverflow,
                  "A CUDA memory view address span exceeds uintptr_t");
  }
  return Status::Ok();
}

bool RangesOverlap(const void* left, const void* right,
                   std::size_t bytes) noexcept {
  const auto left_begin = reinterpret_cast<std::uintptr_t>(left);
  const auto right_begin = reinterpret_cast<std::uintptr_t>(right);
  return left_begin < right_begin + bytes && right_begin < left_begin + bytes;
}

}  // namespace

class PendingCudaEventState {
 public:
  explicit PendingCudaEventState(
      std::unique_ptr<internal_core_execution::CompletionState>
          completion) noexcept
      : completion_(std::move(completion)) {}

  std::unique_ptr<internal_core_execution::CompletionState> completion_;
};

CudaDeviceGuard::CudaDeviceGuard(std::int32_t original_device,
                                 bool restore) noexcept
    : original_device_(original_device), restore_(restore) {}

Result<CudaDeviceGuard> CudaDeviceGuard::Create(
    const ExecutionContext& context) {
  if (context.backend() != Backend::kCuda ||
      context.device().backend != Backend::kCuda) {
    return Status(ErrorCode::kInvalidArgument,
                  "A CUDA device guard requires a CUDA context");
  }
  return Create(context.device().ordinal);
}

Result<CudaDeviceGuard> CudaDeviceGuard::Create(std::int32_t device_ordinal) {
  int original_device = 0;
  cudaError_t error = cudaGetDevice(&original_device);
  if (error != cudaSuccess) {
    return CudaStatus(error, ErrorCode::kProvider, "cudaGetDevice");
  }
  const bool restore = original_device != device_ordinal;
  if (restore) {
    error = cudaSetDevice(device_ordinal);
    if (error != cudaSuccess) {
      return CudaStatus(error, ErrorCode::kUnavailable, "cudaSetDevice");
    }
  }
  return CudaDeviceGuard(original_device, restore);
}

CudaDeviceGuard::CudaDeviceGuard(CudaDeviceGuard&& other) noexcept
    : original_device_(other.original_device_),
      restore_(std::exchange(other.restore_, false)) {}

CudaDeviceGuard::~CudaDeviceGuard() {
  if (restore_) {
    static_cast<void>(cudaSetDevice(original_device_));
  }
}

PendingCudaEvent::PendingCudaEvent(
    std::unique_ptr<PendingCudaEventState> state) noexcept
    : state_(std::move(state)) {}

Result<PendingCudaEvent> PendingCudaEvent::Create(
    const ExecutionContext& execution_context) {
  auto cuda_state = CudaState(execution_context);
  if (!cuda_state.ok()) {
    return cuda_state.status();
  }
  auto completion = CreateEventState(*cuda_state);
  if (!completion.ok()) {
    return completion.status();
  }
  std::unique_ptr<internal_core_execution::CompletionState> completion_state =
      std::move(*completion);
  return PendingCudaEvent(
      std::make_unique<PendingCudaEventState>(std::move(completion_state)));
}

PendingCudaEvent::PendingCudaEvent(PendingCudaEvent&& other) noexcept = default;
PendingCudaEvent& PendingCudaEvent::operator=(
    PendingCudaEvent&& other) noexcept = default;
PendingCudaEvent::~PendingCudaEvent() = default;

Result<CompletionEvent> PendingCudaEvent::Record() {
  if (state_ == nullptr || state_->completion_ == nullptr) {
    return Status(ErrorCode::kInvalidState,
                  "A moved or recorded pending CUDA event is invalid");
  }
  auto* cuda_completion =
      static_cast<CudaCompletionState*>(state_->completion_.get());
  const auto& execution_state = cuda_completion->execution_state();
  auto guard = CudaDeviceGuard::Create(execution_state->device().ordinal);
  if (!guard.ok()) {
    return guard.status();
  }
  const cudaError_t pending_error = cudaGetLastError();
  if (pending_error != cudaSuccess) {
    // Work was submitted before Record() was called. Drain this stream before
    // returning without the event that would otherwise track that work.
    static_cast<void>(cudaStreamSynchronize(execution_state->stream()));
    return CudaStatus(pending_error, ErrorCode::kProvider,
                      "CUDA error before cudaEventRecord");
  }
  const cudaError_t error =
      cudaEventRecord(cuda_completion->event(), execution_state->stream());
  if (error != cudaSuccess) {
    static_cast<void>(cudaStreamSynchronize(execution_state->stream()));
    return CudaStatus(error, ErrorCode::kProvider, "cudaEventRecord");
  }
  std::unique_ptr<internal_core_execution::CompletionState> completion =
      std::move(state_->completion_);
  state_.reset();
  return internal_core_execution::CompletionAccess::Make(std::move(completion));
}

Result<void*> CudaStreamHandle(const ExecutionContext& context) {
  auto state = CudaState(context);
  if (!state.ok()) {
    return state.status();
  }
  return reinterpret_cast<void*>((*state)->stream());
}

Status ValidateCudaMemory(const ExecutionContext& context, const void* pointer,
                          std::size_t bytes, MemorySpace space) {
  if (context.backend() != Backend::kCuda ||
      context.device().backend != Backend::kCuda) {
    return Status(ErrorCode::kInvalidArgument,
                  "CUDA memory validation requires a CUDA context");
  }
  return ValidateCudaMemoryForDevice(context.device(), pointer, bytes, space);
}

Result<std::unique_ptr<internal_core_execution::CompletionState>>
CudaExecutionState::CopyBytes(MutableMemoryView destination,
                              ConstMemoryView source,
                              std::size_t byte_count) const {
  ClearPriorCudaError();
  const Status source_span_status =
      ValidateAddressSpan(source.data(), byte_count);
  if (!source_span_status.ok()) {
    return source_span_status;
  }
  const Status destination_span_status =
      ValidateAddressSpan(destination.data(), byte_count);
  if (!destination_span_status.ok()) {
    return destination_span_status;
  }
  const Status source_status = ValidateCudaMemoryForDevice(
      device_, source.data(), byte_count, source.space());
  if (!source_status.ok()) {
    return source_status;
  }
  const Status destination_status = ValidateCudaMemoryForDevice(
      device_, destination.data(), byte_count, destination.space());
  if (!destination_status.ok()) {
    return destination_status;
  }
  if (source.data() == destination.data() &&
      source.space() == destination.space()) {
    auto completion = CreateEventState(shared_from_this());
    if (!completion.ok()) {
      return completion.status();
    }
    auto guard = CudaDeviceGuard::Create(device_.ordinal);
    if (!guard.ok()) {
      return guard.status();
    }
    // A prior operation's sticky thread-local error is not attributable to
    // this event submission.
    static_cast<void>(cudaGetLastError());
    const cudaError_t error = cudaEventRecord((*completion)->event(), stream_);
    if (error != cudaSuccess) {
      static_cast<void>(cudaStreamSynchronize(stream_));
      return CudaStatus(error, ErrorCode::kProvider, "cudaEventRecord");
    }
    return std::unique_ptr<internal_core_execution::CompletionState>(
        std::move(*completion));
  }
  if (RangesOverlap(source.data(), destination.data(), byte_count)) {
    return Status(ErrorCode::kInvalidArgument,
                  "Partially overlapping CUDA copies are not supported");
  }
  auto guard = CudaDeviceGuard::Create(device_.ordinal);
  if (!guard.ok()) {
    return guard.status();
  }
  auto completion = CreateEventState(shared_from_this());
  if (!completion.ok()) {
    return completion.status();
  }
  // Isolate this submission from a prior operation's sticky thread-local
  // runtime error. Post-submission checks below are attributable to this call.
  static_cast<void>(cudaGetLastError());
  const cudaError_t error =
      cudaMemcpyAsync(destination.data(), source.data(), byte_count,
                      cudaMemcpyDefault, stream_);
  if (error != cudaSuccess) {
    // CUDA Runtime calls may report an earlier asynchronous failure after the
    // new operation was accepted. Drain the affected stream before returning
    // without a completion event.
    static_cast<void>(cudaStreamSynchronize(stream_));
    return CudaStatus(error, ErrorCode::kMemoryTransfer, "cudaMemcpyAsync");
  }
  const cudaError_t submission_error = cudaGetLastError();
  if (submission_error != cudaSuccess) {
    static_cast<void>(cudaStreamSynchronize(stream_));
    return CudaStatus(submission_error, ErrorCode::kMemoryTransfer,
                      "CUDA error after cudaMemcpyAsync");
  }
  const cudaError_t record_error =
      cudaEventRecord((*completion)->event(), stream_);
  if (record_error != cudaSuccess) {
    // A failure after enqueue must not leave live work without a completion
    // handle. This failure-only wait is stream-scoped, never device-wide.
    static_cast<void>(cudaStreamSynchronize(stream_));
    return CudaStatus(record_error, ErrorCode::kProvider, "cudaEventRecord");
  }
  return std::unique_ptr<internal_core_execution::CompletionState>(
      std::move(*completion));
}

}  // namespace internal_core_cuda

Result<std::int32_t> CudaDeviceCount() {
  internal_core_cuda::ClearPriorCudaError();
  int count = 0;
  const cudaError_t error = cudaGetDeviceCount(&count);
  if (error != cudaSuccess) {
    return internal_core_cuda::CudaStatus(error, ErrorCode::kUnavailable,
                                          "cudaGetDeviceCount");
  }
  return static_cast<std::int32_t>(count);
}

CudaMemoryResource::CudaMemoryResource(Device device,
                                       MemorySpace memory_space) noexcept
    : device_(device), memory_space_(memory_space) {}

CudaMemoryResource::~CudaMemoryResource() = default;

Result<std::unique_ptr<CudaMemoryResource>> CudaMemoryResource::Create(
    Device device, MemorySpace memory_space) {
  if (device.backend != Backend::kCuda || device.ordinal < 0) {
    return Status(ErrorCode::kInvalidArgument,
                  "A CUDA memory resource requires a CUDA device");
  }
  if (memory_space != MemorySpace::kPinnedHost &&
      memory_space != MemorySpace::kDevice &&
      memory_space != MemorySpace::kManaged) {
    return Status(
        ErrorCode::kInvalidArgument,
        "A CUDA memory resource supports pinned, device, or managed memory");
  }
  auto count = CudaDeviceCount();
  if (!count.ok()) {
    return count.status();
  }
  if (device.ordinal >= *count) {
    return Status(ErrorCode::kUnavailable,
                  "The requested CUDA device is unavailable");
  }
  return std::unique_ptr<CudaMemoryResource>(
      new CudaMemoryResource(device, memory_space));
}

Result<void*> CudaMemoryResource::Allocate(std::size_t bytes,
                                           std::size_t alignment) {
  internal_core_cuda::ClearPriorCudaError();
  if (!internal_core_cuda::IsPowerOfTwo(alignment)) {
    return Status(ErrorCode::kInvalidArgument,
                  "Allocation alignment must be a nonzero power of two");
  }
  if (bytes == 0) {
    return static_cast<void*>(nullptr);
  }
  auto guard = internal_core_cuda::CudaDeviceGuard::Create(device_.ordinal);
  if (!guard.ok()) {
    return guard.status();
  }
  void* pointer = nullptr;
  cudaError_t error = cudaSuccess;
  if (memory_space_ == MemorySpace::kPinnedHost) {
    error = cudaHostAlloc(&pointer, bytes, cudaHostAllocDefault);
  } else if (memory_space_ == MemorySpace::kDevice) {
    error = cudaMalloc(&pointer, bytes);
  } else {
    error = cudaMallocManaged(&pointer, bytes, cudaMemAttachGlobal);
  }
  if (error != cudaSuccess) {
    return internal_core_cuda::CudaStatus(error, ErrorCode::kAllocation,
                                          "CUDA allocation");
  }
  if (reinterpret_cast<std::uintptr_t>(pointer) % alignment != 0) {
    if (memory_space_ == MemorySpace::kPinnedHost) {
      static_cast<void>(cudaFreeHost(pointer));
    } else {
      static_cast<void>(cudaFree(pointer));
    }
    return Status(ErrorCode::kAllocation,
                  "CUDA allocation does not satisfy requested alignment");
  }
  if (!internal_core_cuda::RegisterCudaAllocation(pointer, bytes, device_,
                                                  memory_space_)) {
    if (memory_space_ == MemorySpace::kPinnedHost) {
      static_cast<void>(cudaFreeHost(pointer));
    } else {
      static_cast<void>(cudaFree(pointer));
    }
    return Status(ErrorCode::kAllocation,
                  "CUDA allocation registry storage is unavailable");
  }
  return pointer;
}

void CudaMemoryResource::Deallocate(void* pointer, std::size_t bytes,
                                    std::size_t alignment) noexcept {
  internal_core_cuda::ClearPriorCudaError();
  static_cast<void>(bytes);
  static_cast<void>(alignment);
  if (pointer == nullptr) {
    return;
  }
  auto guard = internal_core_cuda::CudaDeviceGuard::Create(device_.ordinal);
  if (!guard.ok()) {
    return;
  }
  internal_core_cuda::UnregisterCudaAllocation(pointer);
  if (memory_space_ == MemorySpace::kPinnedHost) {
    static_cast<void>(cudaFreeHost(pointer));
  } else {
    static_cast<void>(cudaFree(pointer));
  }
}

Result<ExecutionContext> CreateCudaExecutionContext(Device device,
                                                    Determinism determinism) {
  internal_core_cuda::ClearPriorCudaError();
  if (device.backend != Backend::kCuda || device.ordinal < 0) {
    return Status(ErrorCode::kInvalidArgument,
                  "A CUDA context requires a CUDA device");
  }
  switch (determinism) {
    case Determinism::kDeterministic:
    case Determinism::kBackendDefault:
      break;
    default:
      return Status(ErrorCode::kInvalidArgument,
                    "Determinism is not a recognized enumerator");
  }
  auto count = CudaDeviceCount();
  if (!count.ok()) {
    return count.status();
  }
  if (device.ordinal >= *count) {
    return Status(ErrorCode::kUnavailable,
                  "The requested CUDA device is unavailable");
  }
  auto guard = internal_core_cuda::CudaDeviceGuard::Create(device.ordinal);
  if (!guard.ok()) {
    return guard.status();
  }
  cudaStream_t stream = nullptr;
  const cudaError_t error =
      cudaStreamCreateWithFlags(&stream, cudaStreamNonBlocking);
  if (error != cudaSuccess) {
    return internal_core_cuda::CudaStatus(error, ErrorCode::kProvider,
                                          "cudaStreamCreateWithFlags");
  }
  auto state =
      std::make_shared<internal_core_cuda::CudaExecutionState>(device, stream);
  return internal_core_execution::ExecutionAccess::Make(
      Backend::kCuda, device, determinism, std::move(state));
}

Result<CompletionEvent> RecordCudaEvent(const ExecutionContext& context) {
  internal_core_cuda::ClearPriorCudaError();
  auto state = internal_core_cuda::CudaState(context);
  if (!state.ok()) {
    return state.status();
  }
  auto event = internal_core_cuda::CreateEventState(*state);
  if (!event.ok()) {
    return event.status();
  }
  auto guard =
      internal_core_cuda::CudaDeviceGuard::Create(context.device().ordinal);
  if (!guard.ok()) {
    return guard.status();
  }
  const cudaError_t error =
      cudaEventRecord((*event)->event(), (*state)->stream());
  if (error != cudaSuccess) {
    static_cast<void>(cudaStreamSynchronize((*state)->stream()));
    return internal_core_cuda::CudaStatus(error, ErrorCode::kProvider,
                                          "cudaEventRecord");
  }
  return internal_core_execution::CompletionAccess::Make(
      std::unique_ptr<internal_core_execution::CompletionState>(
          std::move(*event)));
}

}  // namespace asc
