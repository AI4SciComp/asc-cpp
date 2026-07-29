#include <cuda_runtime_api.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

#if defined(ASC_CPP_CUDA_RUNTIME_TEST_HOOKS)
#include <atomic>
#endif

#include "../execution_internal.h"
#include "asc/core/providers/cuda.h"
#include "cuda_internal.h"
#include "runtime_test_internal.h"

namespace asc {
namespace internal_core_cuda {

#if defined(ASC_CPP_CUDA_RUNTIME_TEST_HOOKS)
namespace {

std::atomic<CudaRuntimeFault> g_cuda_runtime_fault{CudaRuntimeFault::kNone};
std::atomic<std::size_t> g_cuda_runtime_drain_count{0};

}  // namespace

void SetCudaRuntimeFault(CudaRuntimeFault fault) noexcept {
  g_cuda_runtime_fault.store(fault, std::memory_order_release);
}

void ResetCudaRuntimeTestState() noexcept {
  g_cuda_runtime_fault.store(CudaRuntimeFault::kNone,
                             std::memory_order_release);
  g_cuda_runtime_drain_count.store(0, std::memory_order_release);
}

std::size_t CudaRuntimeDrainCount() noexcept {
  return g_cuda_runtime_drain_count.load(std::memory_order_acquire);
}
#endif

namespace {

#if defined(ASC_CPP_CUDA_RUNTIME_TEST_HOOKS)
bool ConsumeCudaRuntimeFault(CudaRuntimeFault fault) noexcept {
  CudaRuntimeFault expected = fault;
  return g_cuda_runtime_fault.compare_exchange_strong(
      expected, CudaRuntimeFault::kNone, std::memory_order_acq_rel);
}
#endif

cudaError_t CreateCompletionEvent(cudaEvent_t* event) noexcept {
#if defined(ASC_CPP_CUDA_RUNTIME_TEST_HOOKS)
  if (ConsumeCudaRuntimeFault(CudaRuntimeFault::kEventCreate)) {
    return cudaErrorUnknown;
  }
#endif
  return cudaEventCreateWithFlags(event, cudaEventDisableTiming);
}

cudaError_t RecordCompletionEvent(cudaEvent_t event,
                                  cudaStream_t stream) noexcept {
#if defined(ASC_CPP_CUDA_RUNTIME_TEST_HOOKS)
  if (ConsumeCudaRuntimeFault(CudaRuntimeFault::kEventRecord)) {
    return cudaErrorUnknown;
  }
#endif
  return cudaEventRecord(event, stream);
}

bool IsPowerOfTwo(std::size_t value) noexcept {
  return value != 0 && (value & (value - 1U)) == 0;
}

Status ValidateDeviceOrdinal(std::int32_t device) {
  if (device < 0) {
    return Status(ErrorCode::kInvalidArgument,
                  "A CUDA device ordinal cannot be negative");
  }
  auto count = CudaDeviceCount();
  if (!count.ok()) {
    return count.status();
  }
  if (device >= *count) {
    return Status(ErrorCode::kUnavailable,
                  "The requested CUDA device is unavailable", "cuda");
  }
  return Status::Ok();
}

bool RangesOverlap(const void* left, const void* right,
                   std::size_t bytes) noexcept {
  const std::uintptr_t left_begin = reinterpret_cast<std::uintptr_t>(left);
  const std::uintptr_t right_begin = reinterpret_cast<std::uintptr_t>(right);
  if (left_begin > UINTPTR_MAX - bytes || right_begin > UINTPTR_MAX - bytes) {
    return true;
  }
  return left_begin < right_begin + bytes && right_begin < left_begin + bytes;
}

void DrainStream(cudaStream_t stream) noexcept {
#if defined(ASC_CPP_CUDA_RUNTIME_TEST_HOOKS)
  g_cuda_runtime_drain_count.fetch_add(1, std::memory_order_acq_rel);
#endif
  static_cast<void>(cudaStreamSynchronize(stream));
}

Status ValidatePointer(const void* pointer, MemorySpace declared_space,
                       std::int32_t device) {
  cudaPointerAttributes attributes{};
  const cudaError_t error = cudaPointerGetAttributes(&attributes, pointer);
  if (error != cudaSuccess) {
    if (declared_space == MemorySpace::kHost &&
        error == cudaErrorInvalidValue) {
      static_cast<void>(cudaGetLastError());
      return Status::Ok();
    }
    return CudaStatus(error, ErrorCode::kMemoryAccess,
                      "CUDA could not inspect a memory pointer");
  }

  switch (declared_space) {
    case MemorySpace::kHost:
      if (attributes.type != cudaMemoryTypeUnregistered) {
        return Status(ErrorCode::kMemoryAccess,
                      "A pointer declared as host memory has CUDA attributes");
      }
      return Status::Ok();
    case MemorySpace::kPinnedHost:
      if (attributes.type != cudaMemoryTypeHost) {
        return Status(ErrorCode::kMemoryAccess,
                      "A pinned-host view is not CUDA-registered host memory");
      }
      return Status::Ok();
    case MemorySpace::kDevice:
      if (attributes.type != cudaMemoryTypeDevice ||
          attributes.device != device) {
        return Status(ErrorCode::kMemoryAccess,
                      "A device view is not on the execution-context device");
      }
      return Status::Ok();
    case MemorySpace::kManaged:
      if (attributes.type != cudaMemoryTypeManaged ||
          attributes.device != device) {
        return Status(
            ErrorCode::kMemoryAccess,
            "A managed view is not associated with the context device");
      }
      return Status::Ok();
  }
  return Status(ErrorCode::kInvalidArgument,
                "A memory view has an unrecognized memory space");
}

class CudaExecutionState;

class CudaCompletionState final
    : public internal_core_execution::CompletionState {
 public:
  CudaCompletionState(std::shared_ptr<const CudaExecutionState> execution,
                      cudaEvent_t event) noexcept
      : execution_(std::move(execution)), event_(event) {}
  ~CudaCompletionState() override;

  [[nodiscard]] Result<bool> Query() const override;
  [[nodiscard]] Status Wait() const override;

 private:
  std::shared_ptr<const CudaExecutionState> execution_;
  cudaEvent_t event_;
};

class CudaExecutionState final
    : public internal_core_execution::ExecutionState,
      public std::enable_shared_from_this<CudaExecutionState> {
 public:
  CudaExecutionState(std::int32_t device, cudaStream_t stream) noexcept
      : device_(device), stream_(stream) {}

  ~CudaExecutionState() override {
    auto guard = DeviceGuard::Create(device_);
    if (guard.ok()) {
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
            std::size_t byte_count) const override {
    auto guard = DeviceGuard::Create(device_);
    if (!guard.ok()) {
      return guard.status();
    }
    Status source_status =
        ValidatePointer(source.data(), source.space(), device_);
    if (!source_status.ok()) {
      return source_status;
    }
    Status destination_status =
        ValidatePointer(destination.data(), destination.space(), device_);
    if (!destination_status.ok()) {
      return destination_status;
    }
    if (source.data() == destination.data() &&
        source.space() == destination.space()) {
      return RecordEvent();
    }
    if (RangesOverlap(source.data(), destination.data(), byte_count)) {
      return Status(ErrorCode::kInvalidArgument,
                    "CUDA CopyBytes rejects partially overlapping views");
    }

    cudaEvent_t event = nullptr;
    cudaError_t error = CreateCompletionEvent(&event);
    if (error != cudaSuccess) {
      return CudaStatus(error, ErrorCode::kProvider,
                        "CUDA could not create a completion event");
    }
    error = cudaMemcpyAsync(destination.data(), source.data(), byte_count,
                            cudaMemcpyDefault, stream_);
    if (error != cudaSuccess) {
      const Status failure =
          CudaStatus(error, ErrorCode::kMemoryTransfer,
                     "CUDA could not enqueue an asynchronous copy");
      DrainStream(stream_);
      static_cast<void>(cudaEventDestroy(event));
      return failure;
    }
    error = RecordCompletionEvent(event, stream_);
    if (error != cudaSuccess) {
      const Status failure =
          CudaStatus(error, ErrorCode::kProvider,
                     "CUDA could not record a completion event");
      DrainStream(stream_);
      static_cast<void>(cudaEventDestroy(event));
      return failure;
    }
    return std::unique_ptr<internal_core_execution::CompletionState>(
        new CudaCompletionState(shared_from_this(), event));
  }

  [[nodiscard]] Result<
      std::unique_ptr<internal_core_execution::CompletionState>>
  RecordEvent() const override {
    auto guard = DeviceGuard::Create(device_);
    if (!guard.ok()) {
      return guard.status();
    }
    cudaEvent_t event = nullptr;
    cudaError_t error = CreateCompletionEvent(&event);
    if (error != cudaSuccess) {
      const Status failure =
          CudaStatus(error, ErrorCode::kProvider,
                     "CUDA could not create a completion event");
      DrainStream(stream_);
      return failure;
    }
    error = RecordCompletionEvent(event, stream_);
    if (error != cudaSuccess) {
      const Status failure =
          CudaStatus(error, ErrorCode::kProvider,
                     "CUDA could not record a completion event");
      DrainStream(stream_);
      static_cast<void>(cudaEventDestroy(event));
      return failure;
    }
    return std::unique_ptr<internal_core_execution::CompletionState>(
        new CudaCompletionState(shared_from_this(), event));
  }

  [[nodiscard]] void* NativeExecutionHandle(
      Backend backend) const noexcept override {
    return backend == Backend::kCuda ? static_cast<void*>(stream_) : nullptr;
  }

  [[nodiscard]] std::int32_t device() const noexcept { return device_; }

 private:
  std::int32_t device_;
  cudaStream_t stream_;
};

CudaCompletionState::~CudaCompletionState() {
  auto guard = DeviceGuard::Create(execution_->device());
  if (guard.ok()) {
    static_cast<void>(cudaEventDestroy(event_));
  }
}

Result<bool> CudaCompletionState::Query() const {
  auto guard = DeviceGuard::Create(execution_->device());
  if (!guard.ok()) {
    return guard.status();
  }
  const cudaError_t error = cudaEventQuery(event_);
  if (error == cudaSuccess) {
    return true;
  }
  if (error == cudaErrorNotReady) {
    return false;
  }
  return CudaStatus(error, ErrorCode::kProvider,
                    "CUDA could not query a completion event");
}

Status CudaCompletionState::Wait() const {
  auto guard = DeviceGuard::Create(execution_->device());
  if (!guard.ok()) {
    return guard.status();
  }
  const cudaError_t error = cudaEventSynchronize(event_);
  if (error != cudaSuccess) {
    return CudaStatus(error, ErrorCode::kProvider,
                      "CUDA could not wait for a completion event");
  }
  return Status::Ok();
}

}  // namespace
}  // namespace internal_core_cuda

Result<std::int32_t> CudaDeviceCount() {
  int count = 0;
  const cudaError_t error = cudaGetDeviceCount(&count);
  if (error != cudaSuccess) {
    return internal_core_cuda::CudaStatus(
        error, ErrorCode::kUnavailable,
        "CUDA could not enumerate available devices");
  }
  return static_cast<std::int32_t>(count);
}

CudaMemoryResource::CudaMemoryResource(std::int32_t device,
                                       MemorySpace memory_space) noexcept
    : device_(device), memory_space_(memory_space) {}

CudaMemoryResource::~CudaMemoryResource() = default;

Result<std::unique_ptr<CudaMemoryResource>> CudaMemoryResource::Create(
    std::int32_t device, MemorySpace memory_space) {
  if (memory_space != MemorySpace::kPinnedHost &&
      memory_space != MemorySpace::kDevice &&
      memory_space != MemorySpace::kManaged) {
    return Status(
        ErrorCode::kInvalidArgument,
        "CudaMemoryResource requires pinned-host, device, or managed memory");
  }
  Status device_status = internal_core_cuda::ValidateDeviceOrdinal(device);
  if (!device_status.ok()) {
    return device_status;
  }
  return std::unique_ptr<CudaMemoryResource>(
      new CudaMemoryResource(device, memory_space));
}

Result<void*> CudaMemoryResource::Allocate(std::size_t bytes,
                                           std::size_t alignment) {
  if (!internal_core_cuda::IsPowerOfTwo(alignment)) {
    return Status(ErrorCode::kInvalidArgument,
                  "CUDA allocation alignment must be a nonzero power of two");
  }
  if (bytes == 0) {
    return static_cast<void*>(nullptr);
  }
  auto guard = internal_core_cuda::DeviceGuard::Create(device_);
  if (!guard.ok()) {
    return guard.status();
  }

  void* pointer = nullptr;
  cudaError_t error = cudaSuccess;
  switch (memory_space_) {
    case MemorySpace::kPinnedHost:
      error = cudaHostAlloc(&pointer, bytes, cudaHostAllocDefault);
      break;
    case MemorySpace::kDevice:
      error = cudaMalloc(&pointer, bytes);
      break;
    case MemorySpace::kManaged:
      error = cudaMallocManaged(&pointer, bytes, cudaMemAttachGlobal);
      break;
    case MemorySpace::kHost:
      return Status(ErrorCode::kInvalidState,
                    "A CUDA resource has an invalid host memory space");
  }
  if (error != cudaSuccess) {
    return internal_core_cuda::CudaStatus(error, ErrorCode::kAllocation,
                                          "CUDA memory allocation failed");
  }
  if (reinterpret_cast<std::uintptr_t>(pointer) % alignment != 0) {
    if (memory_space_ == MemorySpace::kPinnedHost) {
      static_cast<void>(cudaFreeHost(pointer));
    } else {
      static_cast<void>(cudaFree(pointer));
    }
    return Status(ErrorCode::kAllocation,
                  "CUDA could not satisfy the requested allocation alignment",
                  "cuda");
  }
  return pointer;
}

void CudaMemoryResource::Deallocate(void* pointer, std::size_t bytes,
                                    std::size_t alignment) noexcept {
  static_cast<void>(bytes);
  static_cast<void>(alignment);
  if (pointer == nullptr) {
    return;
  }
  auto guard = internal_core_cuda::DeviceGuard::Create(device_);
  if (!guard.ok()) {
    return;
  }
  if (memory_space_ == MemorySpace::kPinnedHost) {
    static_cast<void>(cudaFreeHost(pointer));
  } else {
    static_cast<void>(cudaFree(pointer));
  }
}

Result<ExecutionContext> CreateCudaExecutionContext(std::int32_t device,
                                                    Determinism determinism) {
  switch (determinism) {
    case Determinism::kDeterministic:
    case Determinism::kBackendDefault:
      break;
    default:
      return Status(ErrorCode::kInvalidArgument,
                    "Determinism is not a recognized enumerator");
  }
  Status device_status = internal_core_cuda::ValidateDeviceOrdinal(device);
  if (!device_status.ok()) {
    return device_status;
  }
  auto guard = internal_core_cuda::DeviceGuard::Create(device);
  if (!guard.ok()) {
    return guard.status();
  }
  cudaStream_t stream = nullptr;
  const cudaError_t error =
      cudaStreamCreateWithFlags(&stream, cudaStreamNonBlocking);
  if (error != cudaSuccess) {
    return internal_core_cuda::CudaStatus(
        error, ErrorCode::kProvider,
        "CUDA could not create a nonblocking execution stream");
  }
  auto state =
      std::make_shared<internal_core_cuda::CudaExecutionState>(device, stream);
  return internal_core_execution::Access::MakeContext(
      Backend::kCuda, Device{.backend = Backend::kCuda, .ordinal = device},
      determinism, std::move(state));
}

Result<CompletionEvent> RecordCudaEvent(const ExecutionContext& context) {
  if (context.backend() != Backend::kCuda) {
    return Status(ErrorCode::kInvalidArgument,
                  "RecordCudaEvent requires a CUDA execution context");
  }
  const auto& state = internal_core_execution::Access::State(context);
  if (state == nullptr) {
    return Status(ErrorCode::kUnavailable,
                  "The CUDA execution context has no provider state");
  }
  auto event = state->RecordEvent();
  if (!event.ok()) {
    return event.status();
  }
  return internal_core_execution::Access::MakeEvent(std::move(*event));
}

}  // namespace asc
