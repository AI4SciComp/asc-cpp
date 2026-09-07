#include <cuComplex.h>
#include <cublas_v2.h>
#include <cuda_runtime_api.h>
#include <driver_types.h>

#include <complex>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <type_traits>
#include <utility>

#include "../../core/cuda/cuda_internal.h"
#include "../../core/execution_internal.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/providers/cuda.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/providers/cuda.h"
#include "blas_level1_kernels_internal.h"
#include "context_internal.h"

namespace asc::internal_dense_cuda {
namespace {

template <typename Element>
using Vector = DenseBlasVectorView<Element>;

static_assert(sizeof(std::complex<float>) == sizeof(cuComplex));
static_assert(sizeof(std::complex<double>) == sizeof(cuDoubleComplex));

struct PreparedContext {
  PreparedContext(ContextState* context_state, void* execution_stream,
                  internal_core_cuda::DeviceGuard device_guard,
                  std::unique_lock<std::mutex> handle_lock) noexcept
      : state(context_state),
        stream(execution_stream),
        guard(std::move(device_guard)),
        lock(std::move(handle_lock)) {}

  PreparedContext(const PreparedContext&) = delete;
  PreparedContext& operator=(const PreparedContext&) = delete;
  PreparedContext(PreparedContext&&) noexcept = default;
  PreparedContext& operator=(PreparedContext&&) = delete;
  ~PreparedContext() = default;

  ContextState* state;
  void* stream;
  internal_core_cuda::DeviceGuard guard;
  std::unique_lock<std::mutex> lock;
};

Result<PreparedContext> Prepare(DenseCudaContext& context) {
  ContextState* dense_state = Access::State(context);
  if (dense_state == nullptr) {
    return Status(ErrorCode::kInvalidState,
                  "A moved-from DenseCudaContext cannot launch BLAS work");
  }
  const ExecutionContext& execution = context.execution_context();
  if (execution.backend() != Backend::kCuda ||
      execution.device().ordinal != dense_state->device()) {
    return Status(ErrorCode::kInvalidState,
                  "Dense CUDA provider state does not match its context");
  }
  const auto& execution_state =
      internal_core_execution::Access::State(execution);
  if (execution_state == nullptr) {
    return Status(ErrorCode::kInvalidState,
                  "The Dense CUDA execution context has no provider state");
  }
  void* stream = execution_state->NativeExecutionHandle(Backend::kCuda);
  if (stream == nullptr) {
    return Status(ErrorCode::kUnavailable,
                  "The Dense CUDA execution context has no stream");
  }
  auto guard =
      internal_core_cuda::DeviceGuard::Create(execution.device().ordinal);
  if (!guard.ok()) {
    return guard.status();
  }
  return PreparedContext(dense_state, stream, std::move(*guard),
                         dense_state->Lock());
}

void DrainStream(void* stream) noexcept {
  static_cast<void>(cudaStreamSynchronize(static_cast<cudaStream_t>(stream)));
}

Result<CompletionEvent> RecordOrDrain(DenseCudaContext& context,
                                      PreparedContext& prepared) {
  auto completion = RecordCudaEvent(context.execution_context());
  if (!completion.ok()) {
    DrainStream(prepared.stream);
    return completion.status();
  }
  return completion;
}

Result<CompletionEvent> Finish(DenseCudaContext& context,
                               PreparedContext& prepared,
                               cublasStatus_t provider_status,
                               const char* message) {
  if (provider_status != CUBLAS_STATUS_SUCCESS) {
    Status status = CublasStatus(provider_status, message);
    DrainStream(prepared.stream);
    return status;
  }
  return RecordOrDrain(context, prepared);
}

Result<CompletionEvent> Finish(DenseCudaContext& context,
                               PreparedContext& prepared,
                               Status launch_status) {
  if (!launch_status.ok()) {
    DrainStream(prepared.stream);
    return launch_status;
  }
  return RecordOrDrain(context, prepared);
}

template <typename Call>
Result<CompletionEvent> DevicePointerCall(DenseCudaContext& context,
                                          PreparedContext& prepared,
                                          const char* message, Call call) {
  cublasStatus_t status = cublasSetPointerMode(prepared.state->handle(),
                                               CUBLAS_POINTER_MODE_DEVICE);
  if (status != CUBLAS_STATUS_SUCCESS) {
    return Finish(context, prepared, status,
                  "cuBLAS could not select device scalar pointer mode");
  }
  const cublasStatus_t call_status = call();
  status =
      cublasSetPointerMode(prepared.state->handle(), CUBLAS_POINTER_MODE_HOST);
  if (call_status != CUBLAS_STATUS_SUCCESS) {
    return Finish(context, prepared, call_status, message);
  }
  return Finish(context, prepared, status,
                "cuBLAS could not restore host scalar pointer mode");
}

template <typename Element>
constexpr std::size_t ProviderAlignment() {
  using Value = std::remove_const_t<Element>;
  if constexpr (std::same_as<Value, std::complex<float>>) {
    return alignof(cuComplex);
  } else if constexpr (std::same_as<Value, std::complex<double>>) {
    return alignof(cuDoubleComplex);
  } else {
    return alignof(Value);
  }
}

Status ValidateDeviceAddress(const void* pointer, std::int32_t device,
                             const char* message) {
  cudaPointerAttributes attributes{};
  const cudaError_t error = cudaPointerGetAttributes(&attributes, pointer);
  if (error != cudaSuccess) {
    return internal_core_cuda::CudaStatus(error, ErrorCode::kMemoryAccess,
                                          message);
  }
  if (attributes.type != cudaMemoryTypeDevice || attributes.device != device) {
    return Status(ErrorCode::kMemoryAccess,
                  "A CUDA BLAS operand is not on the context device");
  }
  return Status::Ok();
}

template <typename Element>
Status ValidateDevice(Vector<Element> vector, std::int32_t device) {
  if (vector.memory_space() != MemorySpace::kDevice) {
    return Status(ErrorCode::kMemoryAccess,
                  "CUDA BLAS requires device storage");
  }
  if (vector.size() == 0) {
    return Status::Ok();
  }
  if (vector.data() == nullptr) {
    return Status(ErrorCode::kMemoryAccess,
                  "A nonempty CUDA BLAS vector has a null pointer");
  }
  const ConstMemoryView reachable = vector.reachable_storage();
  if (reinterpret_cast<std::uintptr_t>(reachable.data()) %
          ProviderAlignment<Element>() !=
      0) {
    return Status(ErrorCode::kMemoryAccess,
                  "A CUDA BLAS vector is not provider-aligned");
  }
  return ValidateDeviceAddress(
      reachable.data(), device,
      "CUDA could not inspect a BLAS vector device pointer");
}

template <typename Element>
Status ValidateExactSize(Vector<Element> vector, extent_t size,
                         std::int32_t device, const char* message) {
  Status status = ValidateDevice(vector, device);
  if (!status.ok()) {
    return status;
  }
  if (vector.size() != size) {
    return Status(ErrorCode::kShape, message);
  }
  return Status::Ok();
}

template <typename LeftElement, typename RightElement>
Status ValidatePair(Vector<LeftElement> left, Vector<RightElement> right,
                    std::int32_t device) {
  Status left_status = ValidateDevice(left, device);
  if (!left_status.ok()) {
    return left_status;
  }
  Status right_status = ValidateDevice(right, device);
  if (!right_status.ok()) {
    return right_status;
  }
  if (left.size() != right.size()) {
    return Status(ErrorCode::kShape, "CUDA BLAS vector sizes do not match");
  }
  return Status::Ok();
}

template <typename LeftElement, typename RightElement>
bool SameDescriptor(Vector<LeftElement> left, Vector<RightElement> right) {
  return static_cast<const void*>(left.data()) ==
             static_cast<const void*>(right.data()) &&
         left.size() == right.size() && left.increment() == right.increment() &&
         left.memory_space() == right.memory_space();
}

template <typename LeftElement, typename RightElement>
bool Overlap(Vector<LeftElement> left, Vector<RightElement> right) {
  const ConstMemoryView left_span = left.reachable_storage();
  const ConstMemoryView right_span = right.reachable_storage();
  if (left_span.size() == 0 || right_span.size() == 0) {
    return false;
  }
  const auto left_begin = reinterpret_cast<std::uintptr_t>(left_span.data());
  const auto right_begin = reinterpret_cast<std::uintptr_t>(right_span.data());
  return left_begin < right_begin + right_span.size() &&
         right_begin < left_begin + left_span.size();
}

bool Overlap(ConstMemoryView left, ConstMemoryView right) {
  if (left.size() == 0 || right.size() == 0) {
    return false;
  }
  const auto left_begin = reinterpret_cast<std::uintptr_t>(left.data());
  const auto right_begin = reinterpret_cast<std::uintptr_t>(right.data());
  if (left_begin > std::numeric_limits<std::uintptr_t>::max() - left.size() ||
      right_begin > std::numeric_limits<std::uintptr_t>::max() - right.size()) {
    return true;
  }
  return left_begin < right_begin + right.size() &&
         right_begin < left_begin + left.size();
}

template <typename Element>
auto ProviderPointer(Element* pointer) {
  using Value = std::remove_const_t<Element>;
  if constexpr (std::same_as<Value, std::complex<float>>) {
    using Provider = std::conditional_t<std::is_const_v<Element>,
                                        const cuComplex, cuComplex>;
    return reinterpret_cast<Provider*>(pointer);
  } else if constexpr (std::same_as<Value, std::complex<double>>) {
    using Provider = std::conditional_t<std::is_const_v<Element>,
                                        const cuDoubleComplex, cuDoubleComplex>;
    return reinterpret_cast<Provider*>(pointer);
  } else {
    return pointer;
  }
}

template <typename Element>
auto PositiveProviderPointer(Vector<Element> vector) {
  if (vector.increment() >= 0 || vector.size() <= 1) {
    return vector.data();
  }
  return const_cast<Element*>(
      static_cast<const Element*>(vector.reachable_storage().data()));
}

template <typename Element>
Element* SignedProviderPointer(Vector<Element> vector) {
  if (vector.increment() >= 0 || vector.size() <= 1) {
    return vector.data();
  }
  return const_cast<Element*>(
      static_cast<const Element*>(vector.reachable_storage().data()));
}

template <typename Element>
stride_t ProviderIncrement(Vector<Element> vector) {
  return vector.size() <= 1 ? stride_t{1} : vector.increment();
}

template <typename Element>
stride_t PositiveProviderIncrement(Vector<Element> vector) {
  if (vector.size() <= 1) {
    return stride_t{1};
  }
  return vector.increment() < 0 ? -vector.increment() : vector.increment();
}

template <typename Element>
cublasStatus_t ProviderRotg(cublasHandle_t handle, Element* a, Element* b,
                            Element* c, Element* s) {
  if constexpr (std::same_as<Element, float>) {
    return cublasSrotg(handle, a, b, c, s);
  } else {
    return cublasDrotg(handle, a, b, c, s);
  }
}

template <typename Element>
cublasStatus_t ProviderRotmg(cublasHandle_t handle, Element* d1, Element* d2,
                             Element* x1, const Element* y1,
                             Element* parameters) {
  if constexpr (std::same_as<Element, float>) {
    return cublasSrotmg(handle, d1, d2, x1, y1, parameters);
  } else {
    return cublasDrotmg(handle, d1, d2, x1, y1, parameters);
  }
}

template <typename Element, typename Real>
cublasStatus_t ProviderRot(cublasHandle_t handle, extent_t size, Element* x,
                           stride_t x_increment, Element* y,
                           stride_t y_increment, const Real* c, const Real* s) {
  if constexpr (std::same_as<Element, float>) {
    return cublasSrot_64(handle, size, x, x_increment, y, y_increment, c, s);
  } else if constexpr (std::same_as<Element, double>) {
    return cublasDrot_64(handle, size, x, x_increment, y, y_increment, c, s);
  } else if constexpr (std::same_as<Element, std::complex<float>>) {
    return cublasCsrot_64(handle, size, ProviderPointer(x), x_increment,
                          ProviderPointer(y), y_increment, c, s);
  } else {
    return cublasZdrot_64(handle, size, ProviderPointer(x), x_increment,
                          ProviderPointer(y), y_increment, c, s);
  }
}

template <typename Element>
cublasStatus_t ProviderRotm(cublasHandle_t handle, extent_t size, Element* x,
                            stride_t x_increment, Element* y,
                            stride_t y_increment, const Element* parameters) {
  if constexpr (std::same_as<Element, float>) {
    return cublasSrotm_64(handle, size, x, x_increment, y, y_increment,
                          parameters);
  } else {
    return cublasDrotm_64(handle, size, x, x_increment, y, y_increment,
                          parameters);
  }
}

template <typename Element>
cublasStatus_t ProviderSwap(cublasHandle_t handle, extent_t size, Element* x,
                            stride_t x_increment, Element* y,
                            stride_t y_increment) {
  if constexpr (std::same_as<Element, float>) {
    return cublasSswap_64(handle, size, x, x_increment, y, y_increment);
  } else if constexpr (std::same_as<Element, double>) {
    return cublasDswap_64(handle, size, x, x_increment, y, y_increment);
  } else if constexpr (std::same_as<Element, std::complex<float>>) {
    return cublasCswap_64(handle, size, ProviderPointer(x), x_increment,
                          ProviderPointer(y), y_increment);
  } else {
    return cublasZswap_64(handle, size, ProviderPointer(x), x_increment,
                          ProviderPointer(y), y_increment);
  }
}

template <typename Alpha, typename Element>
cublasStatus_t ProviderScal(cublasHandle_t handle, extent_t size, Alpha alpha,
                            Element* destination, stride_t increment) {
  if constexpr (std::same_as<Element, float>) {
    return cublasSscal_64(handle, size, &alpha, destination, increment);
  } else if constexpr (std::same_as<Element, double>) {
    return cublasDscal_64(handle, size, &alpha, destination, increment);
  } else if constexpr (std::same_as<Element, std::complex<float>> &&
                       std::same_as<Alpha, float>) {
    return cublasCsscal_64(handle, size, &alpha, ProviderPointer(destination),
                           increment);
  } else if constexpr (std::same_as<Element, std::complex<double>> &&
                       std::same_as<Alpha, double>) {
    return cublasZdscal_64(handle, size, &alpha, ProviderPointer(destination),
                           increment);
  } else if constexpr (std::same_as<Element, std::complex<float>>) {
    const cuComplex provider_alpha = make_cuComplex(alpha.real(), alpha.imag());
    return cublasCscal_64(handle, size, &provider_alpha,
                          ProviderPointer(destination), increment);
  } else {
    const cuDoubleComplex provider_alpha =
        make_cuDoubleComplex(alpha.real(), alpha.imag());
    return cublasZscal_64(handle, size, &provider_alpha,
                          ProviderPointer(destination), increment);
  }
}

template <typename Element>
cublasStatus_t ProviderCopy(cublasHandle_t handle, extent_t size,
                            const Element* source, stride_t source_increment,
                            Element* destination,
                            stride_t destination_increment) {
  if constexpr (std::same_as<Element, float>) {
    return cublasScopy_64(handle, size, source, source_increment, destination,
                          destination_increment);
  } else if constexpr (std::same_as<Element, double>) {
    return cublasDcopy_64(handle, size, source, source_increment, destination,
                          destination_increment);
  } else if constexpr (std::same_as<Element, std::complex<float>>) {
    return cublasCcopy_64(handle, size, ProviderPointer(source),
                          source_increment, ProviderPointer(destination),
                          destination_increment);
  } else {
    return cublasZcopy_64(handle, size, ProviderPointer(source),
                          source_increment, ProviderPointer(destination),
                          destination_increment);
  }
}

template <typename Element>
cublasStatus_t ProviderAxpy(cublasHandle_t handle, extent_t size, Element alpha,
                            const Element* source, stride_t source_increment,
                            Element* destination,
                            stride_t destination_increment) {
  if constexpr (std::same_as<Element, float>) {
    return cublasSaxpy_64(handle, size, &alpha, source, source_increment,
                          destination, destination_increment);
  } else if constexpr (std::same_as<Element, double>) {
    return cublasDaxpy_64(handle, size, &alpha, source, source_increment,
                          destination, destination_increment);
  } else if constexpr (std::same_as<Element, std::complex<float>>) {
    const cuComplex provider_alpha = make_cuComplex(alpha.real(), alpha.imag());
    return cublasCaxpy_64(handle, size, &provider_alpha,
                          ProviderPointer(source), source_increment,
                          ProviderPointer(destination), destination_increment);
  } else {
    const cuDoubleComplex provider_alpha =
        make_cuDoubleComplex(alpha.real(), alpha.imag());
    return cublasZaxpy_64(handle, size, &provider_alpha,
                          ProviderPointer(source), source_increment,
                          ProviderPointer(destination), destination_increment);
  }
}

template <typename Element>
cublasStatus_t ProviderDot(cublasHandle_t handle, extent_t size,
                           const Element* left, stride_t left_increment,
                           const Element* right, stride_t right_increment,
                           Element* result) {
  if constexpr (std::same_as<Element, float>) {
    return cublasSdot_64(handle, size, left, left_increment, right,
                         right_increment, result);
  } else {
    return cublasDdot_64(handle, size, left, left_increment, right,
                         right_increment, result);
  }
}

template <typename Element, bool Conjugate>
cublasStatus_t ProviderComplexDot(cublasHandle_t handle, extent_t size,
                                  const Element* left, stride_t left_increment,
                                  const Element* right,
                                  stride_t right_increment, Element* result) {
  if constexpr (std::same_as<Element, std::complex<float>>) {
    if constexpr (Conjugate) {
      return cublasCdotc_64(handle, size, ProviderPointer(left), left_increment,
                            ProviderPointer(right), right_increment,
                            ProviderPointer(result));
    } else {
      return cublasCdotu_64(handle, size, ProviderPointer(left), left_increment,
                            ProviderPointer(right), right_increment,
                            ProviderPointer(result));
    }
  } else {
    if constexpr (Conjugate) {
      return cublasZdotc_64(handle, size, ProviderPointer(left), left_increment,
                            ProviderPointer(right), right_increment,
                            ProviderPointer(result));
    } else {
      return cublasZdotu_64(handle, size, ProviderPointer(left), left_increment,
                            ProviderPointer(right), right_increment,
                            ProviderPointer(result));
    }
  }
}

template <typename Element, typename Real>
cublasStatus_t ProviderNrm2(cublasHandle_t handle, extent_t size,
                            const Element* operand, stride_t increment,
                            Real* result) {
  if constexpr (std::same_as<Element, float>) {
    return cublasSnrm2_64(handle, size, operand, increment, result);
  } else if constexpr (std::same_as<Element, double>) {
    return cublasDnrm2_64(handle, size, operand, increment, result);
  } else if constexpr (std::same_as<Element, std::complex<float>>) {
    return cublasScnrm2_64(handle, size, ProviderPointer(operand), increment,
                           result);
  } else {
    return cublasDznrm2_64(handle, size, ProviderPointer(operand), increment,
                           result);
  }
}

template <typename Element, typename Real>
cublasStatus_t ProviderAsum(cublasHandle_t handle, extent_t size,
                            const Element* operand, stride_t increment,
                            Real* result) {
  if constexpr (std::same_as<Element, float>) {
    return cublasSasum_64(handle, size, operand, increment, result);
  } else if constexpr (std::same_as<Element, double>) {
    return cublasDasum_64(handle, size, operand, increment, result);
  } else if constexpr (std::same_as<Element, std::complex<float>>) {
    return cublasScasum_64(handle, size, ProviderPointer(operand), increment,
                           result);
  } else {
    return cublasDzasum_64(handle, size, ProviderPointer(operand), increment,
                           result);
  }
}

template <typename Element>
cublasStatus_t ProviderIamax(cublasHandle_t handle, extent_t size,
                             const Element* operand, stride_t increment,
                             index_t* result) {
  if constexpr (std::same_as<Element, float>) {
    return cublasIsamax_64(handle, size, operand, increment, result);
  } else if constexpr (std::same_as<Element, double>) {
    return cublasIdamax_64(handle, size, operand, increment, result);
  } else if constexpr (std::same_as<Element, std::complex<float>>) {
    return cublasIcamax_64(handle, size, ProviderPointer(operand), increment,
                           result);
  } else {
    return cublasIzamax_64(handle, size, ProviderPointer(operand), increment,
                           result);
  }
}

template <typename Element>
Result<CompletionEvent> RealRotgImpl(DenseCudaContext& context,
                                     Vector<Element> a, Vector<Element> b,
                                     Vector<Element> c, Vector<Element> s) {
  auto prepared = Prepare(context);
  if (!prepared.ok()) {
    return prepared.status();
  }
  const std::int32_t device = prepared->state->device();
  for (const Vector<Element> vector : {a, b, c, s}) {
    Status status = ValidateExactSize(vector, 1, device,
                                      "CudaRotg operands must be scalar");
    if (!status.ok()) {
      return status;
    }
  }
  if (Overlap(a, b) || Overlap(a, c) || Overlap(a, s) || Overlap(b, c) ||
      Overlap(b, s) || Overlap(c, s)) {
    return Status(ErrorCode::kInvalidArgument,
                  "CudaRotg rejects overlapping scalar operands");
  }
  return DevicePointerCall(
      context, *prepared, "cuBLAS could not enqueue CudaRotg", [&] {
        return ProviderRotg(prepared->state->handle(), a.data(), b.data(),
                            c.data(), s.data());
      });
}

template <typename Real>
Result<CompletionEvent> ComplexRotgImpl(DenseCudaContext& context,
                                        Vector<std::complex<Real>> a,
                                        Vector<const std::complex<Real>> b,
                                        Vector<Real> c,
                                        Vector<std::complex<Real>> s) {
  auto prepared = Prepare(context);
  if (!prepared.ok()) {
    return prepared.status();
  }
  const std::int32_t device = prepared->state->device();
  for (const Status& status :
       {ValidateExactSize(a, 1, device, "CudaRotg operands must be scalar"),
        ValidateExactSize(b, 1, device, "CudaRotg operands must be scalar"),
        ValidateExactSize(c, 1, device, "CudaRotg operands must be scalar"),
        ValidateExactSize(s, 1, device, "CudaRotg operands must be scalar")}) {
    if (!status.ok()) {
      return status;
    }
  }
  if (Overlap(a, b) || Overlap(a, c) || Overlap(a, s) || Overlap(b, c) ||
      Overlap(b, s) || Overlap(c, s)) {
    return Status(ErrorCode::kInvalidArgument,
                  "CudaRotg rejects overlapping scalar operands");
  }
  return Finish(context, *prepared,
                LaunchComplexRotg(prepared->stream, a.data(), b.data(),
                                  c.data(), s.data()));
}

template <typename Element>
Result<CompletionEvent> RotmgImpl(DenseCudaContext& context, Vector<Element> d1,
                                  Vector<Element> d2, Vector<Element> x1,
                                  Vector<const Element> y1,
                                  Vector<Element> parameters) {
  auto prepared = Prepare(context);
  if (!prepared.ok()) {
    return prepared.status();
  }
  const std::int32_t device = prepared->state->device();
  for (const Status& status :
       {ValidateExactSize(d1, 1, device,
                          "CudaRotmg scalar operands must have size one"),
        ValidateExactSize(d2, 1, device,
                          "CudaRotmg scalar operands must have size one"),
        ValidateExactSize(x1, 1, device,
                          "CudaRotmg scalar operands must have size one"),
        ValidateExactSize(y1, 1, device,
                          "CudaRotmg scalar operands must have size one"),
        ValidateExactSize(parameters, 5, device,
                          "CudaRotmg parameters must have size five")}) {
    if (!status.ok()) {
      return status;
    }
  }
  if (parameters.increment() != 1) {
    return Status(ErrorCode::kInvalidArgument,
                  "CudaRotmg parameters must be contiguous");
  }
  if (Overlap(d1, d2) || Overlap(d1, x1) || Overlap(d1, y1) ||
      Overlap(d1, parameters) || Overlap(d2, x1) || Overlap(d2, y1) ||
      Overlap(d2, parameters) || Overlap(x1, y1) || Overlap(x1, parameters) ||
      Overlap(y1, parameters)) {
    return Status(ErrorCode::kInvalidArgument,
                  "CudaRotmg rejects overlapping operands");
  }
  return DevicePointerCall(
      context, *prepared, "cuBLAS could not enqueue CudaRotmg", [&] {
        return ProviderRotmg(prepared->state->handle(), d1.data(), d2.data(),
                             x1.data(), y1.data(), parameters.data());
      });
}

template <typename Element, typename Real>
Result<CompletionEvent> RotImpl(DenseCudaContext& context, Vector<Element> x,
                                Vector<Element> y, Real c, Real s) {
  auto prepared = Prepare(context);
  if (!prepared.ok()) {
    return prepared.status();
  }
  Status pair_status = ValidatePair(x, y, prepared->state->device());
  if (!pair_status.ok()) {
    return pair_status;
  }
  if (Overlap(x, y)) {
    return Status(ErrorCode::kInvalidArgument,
                  "CudaRot rejects overlapping vectors");
  }
  if (x.size() == 0) {
    return RecordCudaEvent(context.execution_context());
  }
  return Finish(
      context, *prepared,
      ProviderRot(prepared->state->handle(), x.size(), SignedProviderPointer(x),
                  ProviderIncrement(x), SignedProviderPointer(y),
                  ProviderIncrement(y), &c, &s),
      "cuBLAS could not enqueue CudaRot");
}

template <typename Element>
Result<CompletionEvent> RotmImpl(DenseCudaContext& context, Vector<Element> x,
                                 Vector<Element> y,
                                 Vector<const Element> parameters) {
  auto prepared = Prepare(context);
  if (!prepared.ok()) {
    return prepared.status();
  }
  const std::int32_t device = prepared->state->device();
  Status pair_status = ValidatePair(x, y, device);
  if (!pair_status.ok()) {
    return pair_status;
  }
  Status parameter_status = ValidateExactSize(
      parameters, 5, device, "CudaRotm parameters must have size five");
  if (!parameter_status.ok()) {
    return parameter_status;
  }
  if (parameters.increment() != 1) {
    return Status(ErrorCode::kInvalidArgument,
                  "CudaRotm parameters must be contiguous");
  }
  if (Overlap(x, y) || Overlap(x, parameters) || Overlap(y, parameters)) {
    return Status(ErrorCode::kInvalidArgument,
                  "CudaRotm rejects overlapping operands");
  }
  if (x.size() == 0) {
    return RecordCudaEvent(context.execution_context());
  }
  return DevicePointerCall(
      context, *prepared, "cuBLAS could not enqueue CudaRotm", [&] {
        return ProviderRotm(prepared->state->handle(), x.size(),
                            SignedProviderPointer(x), ProviderIncrement(x),
                            SignedProviderPointer(y), ProviderIncrement(y),
                            parameters.data());
      });
}

template <typename Element>
Result<CompletionEvent> SwapImpl(DenseCudaContext& context, Vector<Element> x,
                                 Vector<Element> y) {
  auto prepared = Prepare(context);
  if (!prepared.ok()) {
    return prepared.status();
  }
  Status pair_status = ValidatePair(x, y, prepared->state->device());
  if (!pair_status.ok()) {
    return pair_status;
  }
  if (SameDescriptor(x, y) || x.size() == 0) {
    return RecordCudaEvent(context.execution_context());
  }
  if (Overlap(x, y)) {
    return Status(ErrorCode::kInvalidArgument,
                  "CudaSwap rejects partially overlapping vectors");
  }
  return Finish(context, *prepared,
                ProviderSwap(prepared->state->handle(), x.size(),
                             SignedProviderPointer(x), ProviderIncrement(x),
                             SignedProviderPointer(y), ProviderIncrement(y)),
                "cuBLAS could not enqueue CudaSwap");
}

template <typename Alpha, typename Element>
Result<CompletionEvent> ScalImpl(DenseCudaContext& context, Alpha alpha,
                                 Vector<Element> destination) {
  auto prepared = Prepare(context);
  if (!prepared.ok()) {
    return prepared.status();
  }
  Status status = ValidateDevice(destination, prepared->state->device());
  if (!status.ok()) {
    return status;
  }
  if (destination.size() == 0) {
    return RecordCudaEvent(context.execution_context());
  }
  return Finish(context, *prepared,
                ProviderScal(prepared->state->handle(), destination.size(),
                             alpha, PositiveProviderPointer(destination),
                             PositiveProviderIncrement(destination)),
                "cuBLAS could not enqueue CudaScal");
}

template <typename Element>
Result<CompletionEvent> CopyImpl(DenseCudaContext& context,
                                 Vector<const Element> source,
                                 Vector<Element> destination) {
  auto prepared = Prepare(context);
  if (!prepared.ok()) {
    return prepared.status();
  }
  Status pair_status =
      ValidatePair(source, destination, prepared->state->device());
  if (!pair_status.ok()) {
    return pair_status;
  }
  if (SameDescriptor(source, destination) || source.size() == 0) {
    return RecordCudaEvent(context.execution_context());
  }
  if (Overlap(source, destination)) {
    return Status(ErrorCode::kInvalidArgument,
                  "CudaCopy rejects partially overlapping vectors");
  }
  return Finish(
      context, *prepared,
      ProviderCopy(prepared->state->handle(), source.size(),
                   SignedProviderPointer(source), ProviderIncrement(source),
                   SignedProviderPointer(destination),
                   ProviderIncrement(destination)),
      "cuBLAS could not enqueue CudaCopy");
}

template <typename Element>
Result<CompletionEvent> AxpyImpl(DenseCudaContext& context, Element alpha,
                                 Vector<const Element> source,
                                 Vector<Element> destination) {
  auto prepared = Prepare(context);
  if (!prepared.ok()) {
    return prepared.status();
  }
  Status pair_status =
      ValidatePair(source, destination, prepared->state->device());
  if (!pair_status.ok()) {
    return pair_status;
  }
  if (Overlap(source, destination) && !SameDescriptor(source, destination)) {
    return Status(ErrorCode::kInvalidArgument,
                  "CudaAxpy rejects partially overlapping vectors");
  }
  if (source.size() == 0 || alpha == Element{0}) {
    return RecordCudaEvent(context.execution_context());
  }
  return Finish(
      context, *prepared,
      ProviderAxpy(prepared->state->handle(), source.size(), alpha,
                   SignedProviderPointer(source), ProviderIncrement(source),
                   SignedProviderPointer(destination),
                   ProviderIncrement(destination)),
      "cuBLAS could not enqueue CudaAxpy");
}

template <typename Input, typename Output>
Status ValidateReduction(Vector<const Input> left, Vector<const Input> right,
                         Vector<Output> result, std::int32_t device) {
  Status pair_status = ValidatePair(left, right, device);
  if (!pair_status.ok()) {
    return pair_status;
  }
  Status result_status = ValidateExactSize(
      result, 1, device, "A CUDA BLAS reduction result must have size one");
  if (!result_status.ok()) {
    return result_status;
  }
  if (Overlap(left, result) || Overlap(right, result)) {
    return Status(ErrorCode::kInvalidArgument,
                  "A CUDA BLAS reduction result cannot overlap an input");
  }
  return Status::Ok();
}

template <typename Input, typename Output>
Status ValidateReduction(Vector<const Input> operand, Vector<Output> result,
                         std::int32_t device) {
  Status operand_status = ValidateDevice(operand, device);
  if (!operand_status.ok()) {
    return operand_status;
  }
  Status result_status = ValidateExactSize(
      result, 1, device, "A CUDA BLAS reduction result must have size one");
  if (!result_status.ok()) {
    return result_status;
  }
  if (Overlap(operand, result)) {
    return Status(ErrorCode::kInvalidArgument,
                  "A CUDA BLAS reduction result cannot overlap its input");
  }
  return Status::Ok();
}

template <typename Element>
Result<CompletionEvent> DotImpl(DenseCudaContext& context,
                                Vector<const Element> left,
                                Vector<const Element> right,
                                Vector<Element> result) {
  auto prepared = Prepare(context);
  if (!prepared.ok()) {
    return prepared.status();
  }
  Status status =
      ValidateReduction(left, right, result, prepared->state->device());
  if (!status.ok()) {
    return status;
  }
  if (left.size() == 0) {
    return Finish(context, *prepared,
                  LaunchSetScalar(prepared->stream, Element{0}, result.data()));
  }
  return DevicePointerCall(
      context, *prepared, "cuBLAS could not enqueue CudaDot", [&] {
        return ProviderDot(prepared->state->handle(), left.size(),
                           SignedProviderPointer(left), ProviderIncrement(left),
                           SignedProviderPointer(right),
                           ProviderIncrement(right), result.data());
      });
}

Result<CompletionEvent> SdsdotImpl(DenseCudaContext& context, float bias,
                                   Vector<const float> left,
                                   Vector<const float> right,
                                   Vector<float> result) {
  auto prepared = Prepare(context);
  if (!prepared.ok()) {
    return prepared.status();
  }
  Status status =
      ValidateReduction(left, right, result, prepared->state->device());
  if (!status.ok()) {
    return status;
  }
  if (left.size() == 0) {
    return Finish(context, *prepared,
                  LaunchSetScalar(prepared->stream, bias, result.data()));
  }
  return Finish(context, *prepared,
                LaunchSdsdot(prepared->stream, bias, left.size(), left.data(),
                             left.increment(), right.data(), right.increment(),
                             result.data()));
}

Result<CompletionEvent> DsdotImpl(DenseCudaContext& context,
                                  Vector<const float> left,
                                  Vector<const float> right,
                                  Vector<double> result) {
  auto prepared = Prepare(context);
  if (!prepared.ok()) {
    return prepared.status();
  }
  Status status =
      ValidateReduction(left, right, result, prepared->state->device());
  if (!status.ok()) {
    return status;
  }
  if (left.size() == 0) {
    return Finish(context, *prepared,
                  LaunchSetScalar(prepared->stream, 0.0, result.data()));
  }
  return Finish(
      context, *prepared,
      LaunchDsdot(prepared->stream, left.size(), left.data(), left.increment(),
                  right.data(), right.increment(), result.data()));
}

template <typename Element, bool Conjugate>
Result<CompletionEvent> ComplexDotImpl(DenseCudaContext& context,
                                       Vector<const Element> left,
                                       Vector<const Element> right,
                                       Vector<Element> result) {
  auto prepared = Prepare(context);
  if (!prepared.ok()) {
    return prepared.status();
  }
  Status status =
      ValidateReduction(left, right, result, prepared->state->device());
  if (!status.ok()) {
    return status;
  }
  if (left.size() == 0) {
    return Finish(
        context, *prepared,
        LaunchSetScalar(prepared->stream, Element{0, 0}, result.data()));
  }
  return DevicePointerCall(
      context, *prepared, "cuBLAS could not enqueue complex CudaDot", [&] {
        return ProviderComplexDot<Element, Conjugate>(
            prepared->state->handle(), left.size(), SignedProviderPointer(left),
            ProviderIncrement(left), SignedProviderPointer(right),
            ProviderIncrement(right), result.data());
      });
}

template <typename Element, typename Real>
Result<CompletionEvent> Nrm2Impl(DenseCudaContext& context,
                                 Vector<const Element> operand,
                                 Vector<Real> result) {
  auto prepared = Prepare(context);
  if (!prepared.ok()) {
    return prepared.status();
  }
  Status status = ValidateReduction(operand, result, prepared->state->device());
  if (!status.ok()) {
    return status;
  }
  if (operand.size() == 0) {
    return Finish(context, *prepared,
                  LaunchSetScalar(prepared->stream, Real{0}, result.data()));
  }
  return DevicePointerCall(
      context, *prepared, "cuBLAS could not enqueue CudaNrm2", [&] {
        return ProviderNrm2(prepared->state->handle(), operand.size(),
                            PositiveProviderPointer(operand),
                            PositiveProviderIncrement(operand), result.data());
      });
}

template <typename Element, typename Real>
Result<CompletionEvent> AsumImpl(DenseCudaContext& context,
                                 Vector<const Element> operand,
                                 Vector<Real> result) {
  auto prepared = Prepare(context);
  if (!prepared.ok()) {
    return prepared.status();
  }
  Status status = ValidateReduction(operand, result, prepared->state->device());
  if (!status.ok()) {
    return status;
  }
  if (operand.size() == 0) {
    return Finish(context, *prepared,
                  LaunchSetScalar(prepared->stream, Real{0}, result.data()));
  }
  return DevicePointerCall(
      context, *prepared, "cuBLAS could not enqueue CudaAsum", [&] {
        return ProviderAsum(prepared->state->handle(), operand.size(),
                            PositiveProviderPointer(operand),
                            PositiveProviderIncrement(operand), result.data());
      });
}

Status ValidateWorkspace(MutableMemoryView workspace, std::int32_t device) {
  if (!workspace.valid() || workspace.data() == nullptr ||
      workspace.size() < sizeof(index_t)) {
    return Status(ErrorCode::kInvalidArgument,
                  "CudaIamax workspace must hold one provider index");
  }
  if (workspace.space() != MemorySpace::kDevice) {
    return Status(ErrorCode::kMemoryAccess,
                  "CudaIamax workspace must use device storage");
  }
  if (reinterpret_cast<std::uintptr_t>(workspace.data()) % alignof(index_t) !=
      0) {
    return Status(ErrorCode::kMemoryAccess,
                  "CudaIamax workspace is not index-aligned");
  }
  return ValidateDeviceAddress(
      workspace.data(), device,
      "CUDA could not inspect the CudaIamax workspace pointer");
}

template <typename Element>
Result<CompletionEvent> IamaxImpl(DenseCudaContext& context,
                                  Vector<const Element> operand,
                                  Vector<index_t> result,
                                  MutableMemoryView workspace) {
  auto prepared = Prepare(context);
  if (!prepared.ok()) {
    return prepared.status();
  }
  const std::int32_t device = prepared->state->device();
  Status status = ValidateReduction(operand, result, device);
  if (!status.ok()) {
    return status;
  }
  status = ValidateWorkspace(workspace, device);
  if (!status.ok()) {
    return status;
  }
  const ConstMemoryView workspace_span(workspace.data(), workspace.size(),
                                       workspace.space());
  if (Overlap(operand.reachable_storage(), workspace_span) ||
      Overlap(result.reachable_storage(), workspace_span)) {
    return Status(ErrorCode::kInvalidArgument,
                  "CudaIamax workspace cannot overlap an operand");
  }
  if (operand.size() == 0) {
    return Finish(
        context, *prepared,
        LaunchSetScalar(prepared->stream, index_t{-1}, result.data()));
  }
  auto* provider_index = static_cast<index_t*>(workspace.data());
  if (operand.increment() < 0) {
    Status launch_status =
        LaunchNegativeIamax(prepared->stream, operand.size(), operand.data(),
                            operand.increment(), provider_index);
    if (!launch_status.ok()) {
      return Finish(context, *prepared, launch_status);
    }
    return Finish(
        context, *prepared,
        LaunchIamaxIndexConversion(prepared->stream, provider_index,
                                   operand.size(), false, result.data()));
  }
  cublasStatus_t pointer_status = cublasSetPointerMode(
      prepared->state->handle(), CUBLAS_POINTER_MODE_DEVICE);
  if (pointer_status != CUBLAS_STATUS_SUCCESS) {
    return Finish(context, *prepared, pointer_status,
                  "cuBLAS could not select device scalar pointer mode");
  }
  const cublasStatus_t iamax_status =
      ProviderIamax(prepared->state->handle(), operand.size(),
                    PositiveProviderPointer(operand),
                    PositiveProviderIncrement(operand), provider_index);
  const cublasStatus_t restore_status =
      cublasSetPointerMode(prepared->state->handle(), CUBLAS_POINTER_MODE_HOST);
  if (iamax_status != CUBLAS_STATUS_SUCCESS) {
    return Finish(context, *prepared, iamax_status,
                  "cuBLAS could not enqueue CudaIamax");
  }
  if (restore_status != CUBLAS_STATUS_SUCCESS) {
    return Finish(context, *prepared, restore_status,
                  "cuBLAS could not restore host scalar pointer mode");
  }
  return Finish(context, *prepared,
                LaunchIamaxIndexConversion(
                    prepared->stream, provider_index, operand.size(),
                    operand.increment() < 0, result.data()));
}

}  // namespace

}  // namespace asc::internal_dense_cuda

namespace asc {

using internal_dense_cuda::Vector;

Result<CompletionEvent> CudaRotg(DenseCudaContext& context, Vector<float> a,
                                 Vector<float> b, Vector<float> c,
                                 Vector<float> s) {
  return internal_dense_cuda::RealRotgImpl(context, a, b, c, s);
}

Result<CompletionEvent> CudaRotg(DenseCudaContext& context, Vector<double> a,
                                 Vector<double> b, Vector<double> c,
                                 Vector<double> s) {
  return internal_dense_cuda::RealRotgImpl(context, a, b, c, s);
}

Result<CompletionEvent> CudaRotg(DenseCudaContext& context,
                                 Vector<std::complex<float>> a,
                                 Vector<const std::complex<float>> b,
                                 Vector<float> c,
                                 Vector<std::complex<float>> s) {
  return internal_dense_cuda::ComplexRotgImpl(context, a, b, c, s);
}

Result<CompletionEvent> CudaRotg(DenseCudaContext& context,
                                 Vector<std::complex<double>> a,
                                 Vector<const std::complex<double>> b,
                                 Vector<double> c,
                                 Vector<std::complex<double>> s) {
  return internal_dense_cuda::ComplexRotgImpl(context, a, b, c, s);
}

Result<CompletionEvent> CudaRotmg(DenseCudaContext& context, Vector<float> d1,
                                  Vector<float> d2, Vector<float> x1,
                                  Vector<const float> y1,
                                  Vector<float> parameters) {
  return internal_dense_cuda::RotmgImpl(context, d1, d2, x1, y1, parameters);
}

Result<CompletionEvent> CudaRotmg(DenseCudaContext& context, Vector<double> d1,
                                  Vector<double> d2, Vector<double> x1,
                                  Vector<const double> y1,
                                  Vector<double> parameters) {
  return internal_dense_cuda::RotmgImpl(context, d1, d2, x1, y1, parameters);
}

Result<CompletionEvent> CudaRot(DenseCudaContext& context, Vector<float> x,
                                Vector<float> y, float c, float s) {
  return internal_dense_cuda::RotImpl(context, x, y, c, s);
}

Result<CompletionEvent> CudaRot(DenseCudaContext& context, Vector<double> x,
                                Vector<double> y, double c, double s) {
  return internal_dense_cuda::RotImpl(context, x, y, c, s);
}

Result<CompletionEvent> CudaRot(DenseCudaContext& context,
                                Vector<std::complex<float>> x,
                                Vector<std::complex<float>> y, float c,
                                float s) {
  return internal_dense_cuda::RotImpl(context, x, y, c, s);
}

Result<CompletionEvent> CudaRot(DenseCudaContext& context,
                                Vector<std::complex<double>> x,
                                Vector<std::complex<double>> y, double c,
                                double s) {
  return internal_dense_cuda::RotImpl(context, x, y, c, s);
}

Result<CompletionEvent> CudaRotm(DenseCudaContext& context, Vector<float> x,
                                 Vector<float> y,
                                 Vector<const float> parameters) {
  return internal_dense_cuda::RotmImpl(context, x, y, parameters);
}

Result<CompletionEvent> CudaRotm(DenseCudaContext& context, Vector<double> x,
                                 Vector<double> y,
                                 Vector<const double> parameters) {
  return internal_dense_cuda::RotmImpl(context, x, y, parameters);
}

Result<CompletionEvent> CudaSwap(DenseCudaContext& context, Vector<float> x,
                                 Vector<float> y) {
  return internal_dense_cuda::SwapImpl(context, x, y);
}

Result<CompletionEvent> CudaSwap(DenseCudaContext& context, Vector<double> x,
                                 Vector<double> y) {
  return internal_dense_cuda::SwapImpl(context, x, y);
}

Result<CompletionEvent> CudaSwap(DenseCudaContext& context,
                                 Vector<std::complex<float>> x,
                                 Vector<std::complex<float>> y) {
  return internal_dense_cuda::SwapImpl(context, x, y);
}

Result<CompletionEvent> CudaSwap(DenseCudaContext& context,
                                 Vector<std::complex<double>> x,
                                 Vector<std::complex<double>> y) {
  return internal_dense_cuda::SwapImpl(context, x, y);
}

Result<CompletionEvent> CudaScal(DenseCudaContext& context, float alpha,
                                 Vector<float> destination) {
  return internal_dense_cuda::ScalImpl(context, alpha, destination);
}

Result<CompletionEvent> CudaScal(DenseCudaContext& context, double alpha,
                                 Vector<double> destination) {
  return internal_dense_cuda::ScalImpl(context, alpha, destination);
}

Result<CompletionEvent> CudaScal(DenseCudaContext& context,
                                 std::complex<float> alpha,
                                 Vector<std::complex<float>> destination) {
  return internal_dense_cuda::ScalImpl(context, alpha, destination);
}

Result<CompletionEvent> CudaScal(DenseCudaContext& context,
                                 std::complex<double> alpha,
                                 Vector<std::complex<double>> destination) {
  return internal_dense_cuda::ScalImpl(context, alpha, destination);
}

Result<CompletionEvent> CudaScal(DenseCudaContext& context, float alpha,
                                 Vector<std::complex<float>> destination) {
  return internal_dense_cuda::ScalImpl(context, alpha, destination);
}

Result<CompletionEvent> CudaScal(DenseCudaContext& context, double alpha,
                                 Vector<std::complex<double>> destination) {
  return internal_dense_cuda::ScalImpl(context, alpha, destination);
}

Result<CompletionEvent> CudaCopy(DenseCudaContext& context,
                                 Vector<const float> source,
                                 Vector<float> destination) {
  return internal_dense_cuda::CopyImpl(context, source, destination);
}

Result<CompletionEvent> CudaCopy(DenseCudaContext& context,
                                 Vector<const double> source,
                                 Vector<double> destination) {
  return internal_dense_cuda::CopyImpl(context, source, destination);
}

Result<CompletionEvent> CudaCopy(DenseCudaContext& context,
                                 Vector<const std::complex<float>> source,
                                 Vector<std::complex<float>> destination) {
  return internal_dense_cuda::CopyImpl(context, source, destination);
}

Result<CompletionEvent> CudaCopy(DenseCudaContext& context,
                                 Vector<const std::complex<double>> source,
                                 Vector<std::complex<double>> destination) {
  return internal_dense_cuda::CopyImpl(context, source, destination);
}

Result<CompletionEvent> CudaAxpy(DenseCudaContext& context, float alpha,
                                 Vector<const float> source,
                                 Vector<float> destination) {
  return internal_dense_cuda::AxpyImpl(context, alpha, source, destination);
}

Result<CompletionEvent> CudaAxpy(DenseCudaContext& context, double alpha,
                                 Vector<const double> source,
                                 Vector<double> destination) {
  return internal_dense_cuda::AxpyImpl(context, alpha, source, destination);
}

Result<CompletionEvent> CudaAxpy(DenseCudaContext& context,
                                 std::complex<float> alpha,
                                 Vector<const std::complex<float>> source,
                                 Vector<std::complex<float>> destination) {
  return internal_dense_cuda::AxpyImpl(context, alpha, source, destination);
}

Result<CompletionEvent> CudaAxpy(DenseCudaContext& context,
                                 std::complex<double> alpha,
                                 Vector<const std::complex<double>> source,
                                 Vector<std::complex<double>> destination) {
  return internal_dense_cuda::AxpyImpl(context, alpha, source, destination);
}

Result<CompletionEvent> CudaDot(DenseCudaContext& context,
                                Vector<const float> left,
                                Vector<const float> right,
                                Vector<float> result) {
  return internal_dense_cuda::DotImpl(context, left, right, result);
}

Result<CompletionEvent> CudaDot(DenseCudaContext& context,
                                Vector<const double> left,
                                Vector<const double> right,
                                Vector<double> result) {
  return internal_dense_cuda::DotImpl(context, left, right, result);
}

Result<CompletionEvent> CudaDot(DenseCudaContext& context, float bias,
                                Vector<const float> left,
                                Vector<const float> right,
                                Vector<float> result) {
  return internal_dense_cuda::SdsdotImpl(context, bias, left, right, result);
}

Result<CompletionEvent> CudaDot(DenseCudaContext& context,
                                DenseBlasDotAccumulation accumulation,
                                Vector<const float> left,
                                Vector<const float> right,
                                Vector<double> result) {
  if (accumulation != DenseBlasDotAccumulation::kDouble) {
    return Status(ErrorCode::kInvalidArgument,
                  "The CUDA BLAS dot accumulation mode is not recognized");
  }
  return internal_dense_cuda::DsdotImpl(context, left, right, result);
}

Result<CompletionEvent> CudaDotu(DenseCudaContext& context,
                                 Vector<const std::complex<float>> left,
                                 Vector<const std::complex<float>> right,
                                 Vector<std::complex<float>> result) {
  return internal_dense_cuda::ComplexDotImpl<std::complex<float>, false>(
      context, left, right, result);
}

Result<CompletionEvent> CudaDotu(DenseCudaContext& context,
                                 Vector<const std::complex<double>> left,
                                 Vector<const std::complex<double>> right,
                                 Vector<std::complex<double>> result) {
  return internal_dense_cuda::ComplexDotImpl<std::complex<double>, false>(
      context, left, right, result);
}

Result<CompletionEvent> CudaDotc(DenseCudaContext& context,
                                 Vector<const std::complex<float>> left,
                                 Vector<const std::complex<float>> right,
                                 Vector<std::complex<float>> result) {
  return internal_dense_cuda::ComplexDotImpl<std::complex<float>, true>(
      context, left, right, result);
}

Result<CompletionEvent> CudaDotc(DenseCudaContext& context,
                                 Vector<const std::complex<double>> left,
                                 Vector<const std::complex<double>> right,
                                 Vector<std::complex<double>> result) {
  return internal_dense_cuda::ComplexDotImpl<std::complex<double>, true>(
      context, left, right, result);
}

Result<CompletionEvent> CudaNrm2(DenseCudaContext& context,
                                 Vector<const float> operand,
                                 Vector<float> result) {
  return internal_dense_cuda::Nrm2Impl(context, operand, result);
}

Result<CompletionEvent> CudaNrm2(DenseCudaContext& context,
                                 Vector<const double> operand,
                                 Vector<double> result) {
  return internal_dense_cuda::Nrm2Impl(context, operand, result);
}

Result<CompletionEvent> CudaNrm2(DenseCudaContext& context,
                                 Vector<const std::complex<float>> operand,
                                 Vector<float> result) {
  return internal_dense_cuda::Nrm2Impl(context, operand, result);
}

Result<CompletionEvent> CudaNrm2(DenseCudaContext& context,
                                 Vector<const std::complex<double>> operand,
                                 Vector<double> result) {
  return internal_dense_cuda::Nrm2Impl(context, operand, result);
}

Result<CompletionEvent> CudaAsum(DenseCudaContext& context,
                                 Vector<const float> operand,
                                 Vector<float> result) {
  return internal_dense_cuda::AsumImpl(context, operand, result);
}

Result<CompletionEvent> CudaAsum(DenseCudaContext& context,
                                 Vector<const double> operand,
                                 Vector<double> result) {
  return internal_dense_cuda::AsumImpl(context, operand, result);
}

Result<CompletionEvent> CudaAsum(DenseCudaContext& context,
                                 Vector<const std::complex<float>> operand,
                                 Vector<float> result) {
  return internal_dense_cuda::AsumImpl(context, operand, result);
}

Result<CompletionEvent> CudaAsum(DenseCudaContext& context,
                                 Vector<const std::complex<double>> operand,
                                 Vector<double> result) {
  return internal_dense_cuda::AsumImpl(context, operand, result);
}

Result<CompletionEvent> CudaIamax(DenseCudaContext& context,
                                  Vector<const float> operand,
                                  Vector<index_t> result,
                                  MutableMemoryView provider_workspace) {
  return internal_dense_cuda::IamaxImpl(context, operand, result,
                                        provider_workspace);
}

Result<CompletionEvent> CudaIamax(DenseCudaContext& context,
                                  Vector<const double> operand,
                                  Vector<index_t> result,
                                  MutableMemoryView provider_workspace) {
  return internal_dense_cuda::IamaxImpl(context, operand, result,
                                        provider_workspace);
}

Result<CompletionEvent> CudaIamax(DenseCudaContext& context,
                                  Vector<const std::complex<float>> operand,
                                  Vector<index_t> result,
                                  MutableMemoryView provider_workspace) {
  return internal_dense_cuda::IamaxImpl(context, operand, result,
                                        provider_workspace);
}

Result<CompletionEvent> CudaIamax(DenseCudaContext& context,
                                  Vector<const std::complex<double>> operand,
                                  Vector<index_t> result,
                                  MutableMemoryView provider_workspace) {
  return internal_dense_cuda::IamaxImpl(context, operand, result,
                                        provider_workspace);
}

}  // namespace asc
