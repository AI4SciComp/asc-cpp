#include <cuComplex.h>
#include <cublas_v2.h>
#include <cuda_runtime_api.h>

#include <complex>
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
#include "blas_level2_kernels_internal.h"
#include "context_internal.h"

namespace asc::internal_dense_cuda {
namespace {

static_assert(sizeof(std::complex<float>) == sizeof(cuComplex));
static_assert(sizeof(std::complex<double>) == sizeof(cuDoubleComplex));

template <typename Element>
using Vector = DenseBlasVectorView<Element>;

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

  ContextState* state;
  void* stream;
  internal_core_cuda::DeviceGuard guard;
  std::unique_lock<std::mutex> lock;
};

Result<PreparedContext> Prepare(DenseCudaContext& context) {
  ContextState* state = Access::State(context);
  if (state == nullptr) {
    return Status(ErrorCode::kInvalidState,
                  "A moved-from DenseCudaContext cannot launch Level 2 BLAS");
  }
  const ExecutionContext& execution = context.execution_context();
  if (execution.backend() != Backend::kCuda ||
      execution.device().ordinal != state->device()) {
    return Status(ErrorCode::kInvalidState,
                  "Dense CUDA provider state does not match its context");
  }
  const auto& execution_state =
      internal_core_execution::Access::State(execution);
  if (execution_state == nullptr) {
    return Status(ErrorCode::kInvalidState,
                  "The Dense CUDA context has no provider state");
  }
  void* stream = execution_state->NativeExecutionHandle(Backend::kCuda);
  if (stream == nullptr) {
    return Status(ErrorCode::kUnavailable,
                  "The Dense CUDA context has no execution stream");
  }
  auto guard =
      internal_core_cuda::DeviceGuard::Create(execution.device().ordinal);
  if (!guard.ok()) {
    return guard.status();
  }
  return PreparedContext(state, stream, std::move(*guard), state->Lock());
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

bool IsValidTranspose(DenseBlasTranspose transpose) {
  return transpose == DenseBlasTranspose::kNone ||
         transpose == DenseBlasTranspose::kTranspose ||
         transpose == DenseBlasTranspose::kConjugateTranspose;
}

bool IsValidTriangle(DenseBlasTriangle triangle) {
  return triangle == DenseBlasTriangle::kUpper ||
         triangle == DenseBlasTriangle::kLower;
}

bool IsValidDiagonal(DenseBlasDiagonal diagonal) {
  return diagonal == DenseBlasDiagonal::kNonUnit ||
         diagonal == DenseBlasDiagonal::kUnit;
}

Status ValidateDeviceAddress(ConstMemoryView storage, std::int32_t device) {
  if (storage.size() == 0) {
    return Status::Ok();
  }
  cudaPointerAttributes attributes{};
  const cudaError_t error =
      cudaPointerGetAttributes(&attributes, storage.data());
  if (error != cudaSuccess) {
    return internal_core_cuda::CudaStatus(
        error, ErrorCode::kMemoryAccess,
        "CUDA could not inspect a Level 2 BLAS device pointer");
  }
  if (attributes.type != cudaMemoryTypeDevice || attributes.device != device) {
    return Status(ErrorCode::kMemoryAccess,
                  "A Level 2 BLAS operand is not on the context device");
  }
  return Status::Ok();
}

template <typename View>
Status ValidateDevice(const View& view, std::int32_t device) {
  if (view.memory_space() != MemorySpace::kDevice) {
    return Status(ErrorCode::kMemoryAccess,
                  "CUDA Level 2 BLAS requires device storage");
  }
  return ValidateDeviceAddress(view.reachable_storage(), device);
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
Element* SignedProviderPointer(Vector<Element> vector) {
  if (vector.increment() >= 0 || vector.size() <= 1) {
    return vector.data();
  }
  return reinterpret_cast<Element*>(
      const_cast<void*>(vector.reachable_storage().data()));
}

template <typename Element>
stride_t ProviderIncrement(Vector<Element> vector) {
  return vector.size() <= 1 ? stride_t{1} : vector.increment();
}

template <typename Element>
Level2ElementKind ElementKind() {
  if constexpr (std::same_as<Element, float>) {
    return Level2ElementKind::kRealFloat;
  } else if constexpr (std::same_as<Element, double>) {
    return Level2ElementKind::kRealDouble;
  } else if constexpr (std::same_as<Element, std::complex<float>>) {
    return Level2ElementKind::kComplexFloat;
  } else {
    return Level2ElementKind::kComplexDouble;
  }
}

template <typename Element>
Level2ComplexScalar Scalar(Element value) {
  if constexpr (DenseBlasComplex<Element>) {
    return {static_cast<double>(value.real()),
            static_cast<double>(value.imag())};
  } else {
    return {static_cast<double>(value), 0};
  }
}

template <typename Element>
Level2VectorDescriptor Describe(Vector<Element> vector) {
  return {const_cast<std::remove_const_t<Element>*>(vector.data()),
          vector.size(), vector.increment()};
}

template <typename Element>
Level2MatrixDescriptor Describe(DenseBlasMatrixView<Element> matrix,
                                bool upper = true) {
  return {const_cast<std::remove_const_t<Element>*>(matrix.data()),
          matrix.rows(),
          matrix.columns(),
          matrix.leading_dimension(),
          0,
          0,
          Level2StorageKind::kFull,
          upper};
}

template <typename Element>
Level2MatrixDescriptor Describe(DenseBlasBandMatrixView<Element> matrix) {
  return {const_cast<std::remove_const_t<Element>*>(matrix.data()),
          matrix.rows(),
          matrix.columns(),
          matrix.leading_dimension(),
          matrix.lower_bandwidth(),
          matrix.upper_bandwidth(),
          Level2StorageKind::kGeneralBand,
          true};
}

template <typename Element>
Level2MatrixDescriptor Describe(DenseBlasTriangularBandView<Element> matrix,
                                bool upper) {
  return {const_cast<std::remove_const_t<Element>*>(matrix.data()),
          matrix.order(),
          matrix.order(),
          matrix.leading_dimension(),
          matrix.bandwidth(),
          matrix.bandwidth(),
          Level2StorageKind::kTriangularBand,
          upper};
}

template <typename Element>
Level2MatrixDescriptor Describe(DenseBlasPackedMatrixView<Element> matrix,
                                bool upper) {
  return {const_cast<std::remove_const_t<Element>*>(matrix.data()),
          matrix.order(),
          matrix.order(),
          0,
          0,
          0,
          Level2StorageKind::kPacked,
          upper};
}

cublasOperation_t ProviderOperation(DenseBlasTranspose transpose) {
  if (transpose == DenseBlasTranspose::kNone) {
    return CUBLAS_OP_N;
  }
  if (transpose == DenseBlasTranspose::kTranspose) {
    return CUBLAS_OP_T;
  }
  return CUBLAS_OP_C;
}

cublasFillMode_t ProviderTriangle(DenseBlasTriangle triangle,
                                  DenseBlasLayout layout) {
  const bool upper = triangle == DenseBlasTriangle::kUpper;
  const bool provider_upper =
      layout == DenseBlasLayout::kColumnMajor ? upper : !upper;
  return provider_upper ? CUBLAS_FILL_MODE_UPPER : CUBLAS_FILL_MODE_LOWER;
}

cublasDiagType_t ProviderDiagonal(DenseBlasDiagonal diagonal) {
  return diagonal == DenseBlasDiagonal::kUnit ? CUBLAS_DIAG_UNIT
                                              : CUBLAS_DIAG_NON_UNIT;
}

template <typename Element>
cublasStatus_t ProviderScal(cublasHandle_t handle, extent_t size, Element beta,
                            Element* vector, stride_t increment) {
  if constexpr (std::same_as<Element, float>) {
    return cublasSscal_64(handle, size, &beta, vector, increment);
  } else if constexpr (std::same_as<Element, double>) {
    return cublasDscal_64(handle, size, &beta, vector, increment);
  } else if constexpr (std::same_as<Element, std::complex<float>>) {
    const cuComplex value = make_cuComplex(beta.real(), beta.imag());
    return cublasCscal_64(handle, size, &value, ProviderPointer(vector),
                          increment);
  } else {
    const cuDoubleComplex value =
        make_cuDoubleComplex(beta.real(), beta.imag());
    return cublasZscal_64(handle, size, &value, ProviderPointer(vector),
                          increment);
  }
}

template <typename Element>
cublasStatus_t ProviderGemv(cublasHandle_t handle, cublasOperation_t operation,
                            extent_t rows, extent_t columns, Element alpha,
                            const Element* matrix, stride_t leading_dimension,
                            const Element* input, stride_t input_increment,
                            Element beta, Element* output,
                            stride_t output_increment) {
  if constexpr (std::same_as<Element, float>) {
    return cublasSgemv_64(handle, operation, rows, columns, &alpha, matrix,
                          leading_dimension, input, input_increment, &beta,
                          output, output_increment);
  } else if constexpr (std::same_as<Element, double>) {
    return cublasDgemv_64(handle, operation, rows, columns, &alpha, matrix,
                          leading_dimension, input, input_increment, &beta,
                          output, output_increment);
  } else if constexpr (std::same_as<Element, std::complex<float>>) {
    const cuComplex provider_alpha = make_cuComplex(alpha.real(), alpha.imag());
    const cuComplex provider_beta = make_cuComplex(beta.real(), beta.imag());
    return cublasCgemv_64(handle, operation, rows, columns, &provider_alpha,
                          ProviderPointer(matrix), leading_dimension,
                          ProviderPointer(input), input_increment,
                          &provider_beta, ProviderPointer(output),
                          output_increment);
  } else {
    const cuDoubleComplex provider_alpha =
        make_cuDoubleComplex(alpha.real(), alpha.imag());
    const cuDoubleComplex provider_beta =
        make_cuDoubleComplex(beta.real(), beta.imag());
    return cublasZgemv_64(handle, operation, rows, columns, &provider_alpha,
                          ProviderPointer(matrix), leading_dimension,
                          ProviderPointer(input), input_increment,
                          &provider_beta, ProviderPointer(output),
                          output_increment);
  }
}

template <typename Element>
cublasStatus_t ProviderGbmv(cublasHandle_t handle, cublasOperation_t operation,
                            extent_t rows, extent_t columns, extent_t lower,
                            extent_t upper, Element alpha,
                            const Element* matrix, stride_t leading_dimension,
                            const Element* input, stride_t input_increment,
                            Element beta, Element* output,
                            stride_t output_increment) {
  if constexpr (std::same_as<Element, float>) {
    return cublasSgbmv_64(handle, operation, rows, columns, lower, upper,
                          &alpha, matrix, leading_dimension, input,
                          input_increment, &beta, output, output_increment);
  } else if constexpr (std::same_as<Element, double>) {
    return cublasDgbmv_64(handle, operation, rows, columns, lower, upper,
                          &alpha, matrix, leading_dimension, input,
                          input_increment, &beta, output, output_increment);
  } else if constexpr (std::same_as<Element, std::complex<float>>) {
    const cuComplex provider_alpha = make_cuComplex(alpha.real(), alpha.imag());
    const cuComplex provider_beta = make_cuComplex(beta.real(), beta.imag());
    return cublasCgbmv_64(handle, operation, rows, columns, lower, upper,
                          &provider_alpha, ProviderPointer(matrix),
                          leading_dimension, ProviderPointer(input),
                          input_increment, &provider_beta,
                          ProviderPointer(output), output_increment);
  } else {
    const cuDoubleComplex provider_alpha =
        make_cuDoubleComplex(alpha.real(), alpha.imag());
    const cuDoubleComplex provider_beta =
        make_cuDoubleComplex(beta.real(), beta.imag());
    return cublasZgbmv_64(handle, operation, rows, columns, lower, upper,
                          &provider_alpha, ProviderPointer(matrix),
                          leading_dimension, ProviderPointer(input),
                          input_increment, &provider_beta,
                          ProviderPointer(output), output_increment);
  }
}

template <typename Element, typename Matrix>
Result<CompletionEvent> GeneralMv(DenseCudaContext& context,
                                  DenseBlasTranspose transpose, Element alpha,
                                  Matrix matrix, Vector<const Element> input,
                                  Element beta, Vector<Element> output) {
  auto prepared = Prepare(context);
  if (!prepared.ok()) {
    return prepared.status();
  }
  if (!IsValidTranspose(transpose)) {
    return Status(ErrorCode::kInvalidArgument,
                  "A CUDA Level 2 transpose value is invalid");
  }
  const std::int32_t device = prepared->state->device();
  for (Status status :
       {ValidateDevice(matrix, device), ValidateDevice(input, device),
        ValidateDevice(output, device)}) {
    if (!status.ok()) {
      return status;
    }
  }
  const extent_t output_size =
      transpose == DenseBlasTranspose::kNone ? matrix.rows() : matrix.columns();
  const extent_t input_size =
      transpose == DenseBlasTranspose::kNone ? matrix.columns() : matrix.rows();
  if (input.size() != input_size || output.size() != output_size) {
    return Status(ErrorCode::kShape,
                  "CUDA matrix-vector operand shapes do not match");
  }
  if (Overlap(matrix.reachable_storage(), output.reachable_storage()) ||
      Overlap(input.reachable_storage(), output.reachable_storage())) {
    return Status(ErrorCode::kInvalidArgument,
                  "CUDA matrix-vector output overlaps a read operand");
  }
  if (output_size == 0) {
    return RecordOrDrain(context, *prepared);
  }
  if (input_size == 0) {
    return Finish(
        context, *prepared,
        LaunchLevel2ScaleVector(prepared->stream, ElementKind<Element>(),
                                Scalar(beta), Describe(output)));
  }
  if constexpr (DenseBlasComplex<Element>) {
    if (matrix.layout() == DenseBlasLayout::kRowMajor &&
        transpose == DenseBlasTranspose::kConjugateTranspose) {
      return Finish(context, *prepared,
                    LaunchLevel2ConjugateGemv(
                        prepared->stream, ElementKind<Element>(), Scalar(alpha),
                        Describe(matrix), Describe(input), Scalar(beta),
                        Describe(output)));
    }
  }

  extent_t provider_rows = matrix.rows();
  extent_t provider_columns = matrix.columns();
  extent_t provider_lower = 0;
  extent_t provider_upper = 0;
  cublasOperation_t operation = ProviderOperation(transpose);
  constexpr bool kBand = requires {
    matrix.lower_bandwidth();
    matrix.upper_bandwidth();
  };
  if constexpr (kBand) {
    provider_lower = matrix.lower_bandwidth();
    provider_upper = matrix.upper_bandwidth();
  }
  if (matrix.layout() == DenseBlasLayout::kRowMajor) {
    std::swap(provider_rows, provider_columns);
    if constexpr (kBand) {
      std::swap(provider_lower, provider_upper);
    }
    operation =
        transpose == DenseBlasTranspose::kNone ? CUBLAS_OP_T : CUBLAS_OP_N;
  }
  if constexpr (kBand) {
    return Finish(
        context, *prepared,
        ProviderGbmv(prepared->state->handle(), operation, provider_rows,
                     provider_columns, provider_lower, provider_upper, alpha,
                     matrix.data(), matrix.leading_dimension(),
                     SignedProviderPointer(input), ProviderIncrement(input),
                     beta, SignedProviderPointer(output),
                     ProviderIncrement(output)),
        "cuBLAS could not enqueue CudaGbmv");
  } else {
    return Finish(
        context, *prepared,
        ProviderGemv(prepared->state->handle(), operation, provider_rows,
                     provider_columns, alpha, matrix.data(),
                     matrix.leading_dimension(), SignedProviderPointer(input),
                     ProviderIncrement(input), beta,
                     SignedProviderPointer(output), ProviderIncrement(output)),
        "cuBLAS could not enqueue CudaGemv");
  }
}

template <typename Element>
cublasStatus_t ProviderSymv(cublasHandle_t handle, cublasFillMode_t triangle,
                            extent_t order, Element alpha,
                            const Element* matrix, stride_t leading_dimension,
                            const Element* input, stride_t input_increment,
                            Element beta, Element* output,
                            stride_t output_increment) {
  if constexpr (std::same_as<Element, float>) {
    return cublasSsymv_64(handle, triangle, order, &alpha, matrix,
                          leading_dimension, input, input_increment, &beta,
                          output, output_increment);
  } else {
    return cublasDsymv_64(handle, triangle, order, &alpha, matrix,
                          leading_dimension, input, input_increment, &beta,
                          output, output_increment);
  }
}

template <typename Element>
cublasStatus_t ProviderSbmv(cublasHandle_t handle, cublasFillMode_t triangle,
                            extent_t order, extent_t bandwidth, Element alpha,
                            const Element* matrix, stride_t leading_dimension,
                            const Element* input, stride_t input_increment,
                            Element beta, Element* output,
                            stride_t output_increment) {
  if constexpr (std::same_as<Element, float>) {
    return cublasSsbmv_64(handle, triangle, order, bandwidth, &alpha, matrix,
                          leading_dimension, input, input_increment, &beta,
                          output, output_increment);
  } else {
    return cublasDsbmv_64(handle, triangle, order, bandwidth, &alpha, matrix,
                          leading_dimension, input, input_increment, &beta,
                          output, output_increment);
  }
}

template <typename Element>
cublasStatus_t ProviderSpmv(cublasHandle_t handle, cublasFillMode_t triangle,
                            extent_t order, Element alpha,
                            const Element* matrix, const Element* input,
                            stride_t input_increment, Element beta,
                            Element* output, stride_t output_increment) {
  if constexpr (std::same_as<Element, float>) {
    return cublasSspmv_64(handle, triangle, order, &alpha, matrix, input,
                          input_increment, &beta, output, output_increment);
  } else {
    return cublasDspmv_64(handle, triangle, order, &alpha, matrix, input,
                          input_increment, &beta, output, output_increment);
  }
}

template <typename Element>
cublasStatus_t ProviderHemv(cublasHandle_t handle, cublasFillMode_t triangle,
                            extent_t order, Element alpha,
                            const Element* matrix, stride_t leading_dimension,
                            const Element* input, stride_t input_increment,
                            Element beta, Element* output,
                            stride_t output_increment) {
  if constexpr (std::same_as<Element, std::complex<float>>) {
    const cuComplex provider_alpha = make_cuComplex(alpha.real(), alpha.imag());
    const cuComplex provider_beta = make_cuComplex(beta.real(), beta.imag());
    return cublasChemv_64(
        handle, triangle, order, &provider_alpha, ProviderPointer(matrix),
        leading_dimension, ProviderPointer(input), input_increment,
        &provider_beta, ProviderPointer(output), output_increment);
  } else {
    const cuDoubleComplex provider_alpha =
        make_cuDoubleComplex(alpha.real(), alpha.imag());
    const cuDoubleComplex provider_beta =
        make_cuDoubleComplex(beta.real(), beta.imag());
    return cublasZhemv_64(
        handle, triangle, order, &provider_alpha, ProviderPointer(matrix),
        leading_dimension, ProviderPointer(input), input_increment,
        &provider_beta, ProviderPointer(output), output_increment);
  }
}

template <typename Element>
cublasStatus_t ProviderHbmv(cublasHandle_t handle, cublasFillMode_t triangle,
                            extent_t order, extent_t bandwidth, Element alpha,
                            const Element* matrix, stride_t leading_dimension,
                            const Element* input, stride_t input_increment,
                            Element beta, Element* output,
                            stride_t output_increment) {
  if constexpr (std::same_as<Element, std::complex<float>>) {
    const cuComplex provider_alpha = make_cuComplex(alpha.real(), alpha.imag());
    const cuComplex provider_beta = make_cuComplex(beta.real(), beta.imag());
    return cublasChbmv_64(handle, triangle, order, bandwidth, &provider_alpha,
                          ProviderPointer(matrix), leading_dimension,
                          ProviderPointer(input), input_increment,
                          &provider_beta, ProviderPointer(output),
                          output_increment);
  } else {
    const cuDoubleComplex provider_alpha =
        make_cuDoubleComplex(alpha.real(), alpha.imag());
    const cuDoubleComplex provider_beta =
        make_cuDoubleComplex(beta.real(), beta.imag());
    return cublasZhbmv_64(handle, triangle, order, bandwidth, &provider_alpha,
                          ProviderPointer(matrix), leading_dimension,
                          ProviderPointer(input), input_increment,
                          &provider_beta, ProviderPointer(output),
                          output_increment);
  }
}

template <typename Element>
cublasStatus_t ProviderHpmv(cublasHandle_t handle, cublasFillMode_t triangle,
                            extent_t order, Element alpha,
                            const Element* matrix, const Element* input,
                            stride_t input_increment, Element beta,
                            Element* output, stride_t output_increment) {
  if constexpr (std::same_as<Element, std::complex<float>>) {
    const cuComplex provider_alpha = make_cuComplex(alpha.real(), alpha.imag());
    const cuComplex provider_beta = make_cuComplex(beta.real(), beta.imag());
    return cublasChpmv_64(handle, triangle, order, &provider_alpha,
                          ProviderPointer(matrix), ProviderPointer(input),
                          input_increment, &provider_beta,
                          ProviderPointer(output), output_increment);
  } else {
    const cuDoubleComplex provider_alpha =
        make_cuDoubleComplex(alpha.real(), alpha.imag());
    const cuDoubleComplex provider_beta =
        make_cuDoubleComplex(beta.real(), beta.imag());
    return cublasZhpmv_64(handle, triangle, order, &provider_alpha,
                          ProviderPointer(matrix), ProviderPointer(input),
                          input_increment, &provider_beta,
                          ProviderPointer(output), output_increment);
  }
}

template <typename Element, typename Matrix>
Result<CompletionEvent> StructuredMv(DenseCudaContext& context,
                                     DenseBlasTriangle triangle, Element alpha,
                                     Matrix matrix, Vector<const Element> input,
                                     Element beta, Vector<Element> output) {
  auto prepared = Prepare(context);
  if (!prepared.ok()) {
    return prepared.status();
  }
  if (!IsValidTriangle(triangle)) {
    return Status(ErrorCode::kInvalidArgument,
                  "A CUDA structured triangle value is invalid");
  }
  const std::int32_t device = prepared->state->device();
  for (Status status :
       {ValidateDevice(matrix, device), ValidateDevice(input, device),
        ValidateDevice(output, device)}) {
    if (!status.ok()) {
      return status;
    }
  }
  const extent_t order = [&] {
    if constexpr (requires { matrix.order(); }) {
      return matrix.order();
    } else {
      return matrix.rows();
    }
  }();
  if constexpr (requires { matrix.columns(); }) {
    if (matrix.rows() != matrix.columns()) {
      return Status(ErrorCode::kShape,
                    "A CUDA structured matrix must be square");
    }
  }
  if (input.size() != order || output.size() != order) {
    return Status(ErrorCode::kShape,
                  "CUDA structured matrix-vector shapes do not match");
  }
  if (Overlap(matrix.reachable_storage(), output.reachable_storage()) ||
      Overlap(input.reachable_storage(), output.reachable_storage())) {
    return Status(ErrorCode::kInvalidArgument,
                  "CUDA structured output overlaps a read operand");
  }
  if (order == 0) {
    return RecordOrDrain(context, *prepared);
  }
  if constexpr (DenseBlasComplex<Element>) {
    if (matrix.layout() == DenseBlasLayout::kRowMajor) {
      return Finish(context, *prepared,
                    LaunchLevel2HermitianMv(
                        prepared->stream, ElementKind<Element>(), Scalar(alpha),
                        Describe(matrix, triangle == DenseBlasTriangle::kUpper),
                        Describe(input), Scalar(beta), Describe(output)));
    }
  }
  const cublasFillMode_t provider_triangle =
      ProviderTriangle(triangle, matrix.layout());
  cublasStatus_t provider_status = CUBLAS_STATUS_NOT_SUPPORTED;
  if constexpr (DenseBlasReal<Element>) {
    if constexpr (requires { matrix.bandwidth(); }) {
      provider_status = ProviderSbmv(
          prepared->state->handle(), provider_triangle, order,
          matrix.bandwidth(), alpha, matrix.data(), matrix.leading_dimension(),
          SignedProviderPointer(input), ProviderIncrement(input), beta,
          SignedProviderPointer(output), ProviderIncrement(output));
    } else if constexpr (requires { matrix.leading_dimension(); }) {
      provider_status = ProviderSymv(
          prepared->state->handle(), provider_triangle, order, alpha,
          matrix.data(), matrix.leading_dimension(),
          SignedProviderPointer(input), ProviderIncrement(input), beta,
          SignedProviderPointer(output), ProviderIncrement(output));
    } else {
      provider_status = ProviderSpmv(
          prepared->state->handle(), provider_triangle, order, alpha,
          matrix.data(), SignedProviderPointer(input), ProviderIncrement(input),
          beta, SignedProviderPointer(output), ProviderIncrement(output));
    }
  } else {
    if constexpr (requires { matrix.bandwidth(); }) {
      provider_status = ProviderHbmv(
          prepared->state->handle(), provider_triangle, order,
          matrix.bandwidth(), alpha, matrix.data(), matrix.leading_dimension(),
          SignedProviderPointer(input), ProviderIncrement(input), beta,
          SignedProviderPointer(output), ProviderIncrement(output));
    } else if constexpr (requires { matrix.leading_dimension(); }) {
      provider_status = ProviderHemv(
          prepared->state->handle(), provider_triangle, order, alpha,
          matrix.data(), matrix.leading_dimension(),
          SignedProviderPointer(input), ProviderIncrement(input), beta,
          SignedProviderPointer(output), ProviderIncrement(output));
    } else {
      provider_status = ProviderHpmv(
          prepared->state->handle(), provider_triangle, order, alpha,
          matrix.data(), SignedProviderPointer(input), ProviderIncrement(input),
          beta, SignedProviderPointer(output), ProviderIncrement(output));
    }
  }
  return Finish(context, *prepared, provider_status,
                "cuBLAS could not enqueue a structured matrix-vector call");
}

template <typename Element>
cublasStatus_t ProviderTrmv(cublasHandle_t handle, cublasFillMode_t triangle,
                            cublasOperation_t operation,
                            cublasDiagType_t diagonal, extent_t order,
                            const Element* matrix, stride_t leading_dimension,
                            Element* vector, stride_t increment) {
  if constexpr (std::same_as<Element, float>) {
    return cublasStrmv_64(handle, triangle, operation, diagonal, order, matrix,
                          leading_dimension, vector, increment);
  } else if constexpr (std::same_as<Element, double>) {
    return cublasDtrmv_64(handle, triangle, operation, diagonal, order, matrix,
                          leading_dimension, vector, increment);
  } else if constexpr (std::same_as<Element, std::complex<float>>) {
    return cublasCtrmv_64(handle, triangle, operation, diagonal, order,
                          ProviderPointer(matrix), leading_dimension,
                          ProviderPointer(vector), increment);
  } else {
    return cublasZtrmv_64(handle, triangle, operation, diagonal, order,
                          ProviderPointer(matrix), leading_dimension,
                          ProviderPointer(vector), increment);
  }
}

template <typename Element>
cublasStatus_t ProviderTbmv(cublasHandle_t handle, cublasFillMode_t triangle,
                            cublasOperation_t operation,
                            cublasDiagType_t diagonal, extent_t order,
                            extent_t bandwidth, const Element* matrix,
                            stride_t leading_dimension, Element* vector,
                            stride_t increment) {
  if constexpr (std::same_as<Element, float>) {
    return cublasStbmv_64(handle, triangle, operation, diagonal, order,
                          bandwidth, matrix, leading_dimension, vector,
                          increment);
  } else if constexpr (std::same_as<Element, double>) {
    return cublasDtbmv_64(handle, triangle, operation, diagonal, order,
                          bandwidth, matrix, leading_dimension, vector,
                          increment);
  } else if constexpr (std::same_as<Element, std::complex<float>>) {
    return cublasCtbmv_64(handle, triangle, operation, diagonal, order,
                          bandwidth, ProviderPointer(matrix), leading_dimension,
                          ProviderPointer(vector), increment);
  } else {
    return cublasZtbmv_64(handle, triangle, operation, diagonal, order,
                          bandwidth, ProviderPointer(matrix), leading_dimension,
                          ProviderPointer(vector), increment);
  }
}

template <typename Element>
cublasStatus_t ProviderTpmv(cublasHandle_t handle, cublasFillMode_t triangle,
                            cublasOperation_t operation,
                            cublasDiagType_t diagonal, extent_t order,
                            const Element* matrix, Element* vector,
                            stride_t increment) {
  if constexpr (std::same_as<Element, float>) {
    return cublasStpmv_64(handle, triangle, operation, diagonal, order, matrix,
                          vector, increment);
  } else if constexpr (std::same_as<Element, double>) {
    return cublasDtpmv_64(handle, triangle, operation, diagonal, order, matrix,
                          vector, increment);
  } else if constexpr (std::same_as<Element, std::complex<float>>) {
    return cublasCtpmv_64(handle, triangle, operation, diagonal, order,
                          ProviderPointer(matrix), ProviderPointer(vector),
                          increment);
  } else {
    return cublasZtpmv_64(handle, triangle, operation, diagonal, order,
                          ProviderPointer(matrix), ProviderPointer(vector),
                          increment);
  }
}

template <typename Element>
cublasStatus_t ProviderTrsv(cublasHandle_t handle, cublasFillMode_t triangle,
                            cublasOperation_t operation,
                            cublasDiagType_t diagonal, extent_t order,
                            const Element* matrix, stride_t leading_dimension,
                            Element* vector, stride_t increment) {
  if constexpr (std::same_as<Element, float>) {
    return cublasStrsv_64(handle, triangle, operation, diagonal, order, matrix,
                          leading_dimension, vector, increment);
  } else if constexpr (std::same_as<Element, double>) {
    return cublasDtrsv_64(handle, triangle, operation, diagonal, order, matrix,
                          leading_dimension, vector, increment);
  } else if constexpr (std::same_as<Element, std::complex<float>>) {
    return cublasCtrsv_64(handle, triangle, operation, diagonal, order,
                          ProviderPointer(matrix), leading_dimension,
                          ProviderPointer(vector), increment);
  } else {
    return cublasZtrsv_64(handle, triangle, operation, diagonal, order,
                          ProviderPointer(matrix), leading_dimension,
                          ProviderPointer(vector), increment);
  }
}

template <typename Element>
cublasStatus_t ProviderTbsv(cublasHandle_t handle, cublasFillMode_t triangle,
                            cublasOperation_t operation,
                            cublasDiagType_t diagonal, extent_t order,
                            extent_t bandwidth, const Element* matrix,
                            stride_t leading_dimension, Element* vector,
                            stride_t increment) {
  if constexpr (std::same_as<Element, float>) {
    return cublasStbsv_64(handle, triangle, operation, diagonal, order,
                          bandwidth, matrix, leading_dimension, vector,
                          increment);
  } else if constexpr (std::same_as<Element, double>) {
    return cublasDtbsv_64(handle, triangle, operation, diagonal, order,
                          bandwidth, matrix, leading_dimension, vector,
                          increment);
  } else if constexpr (std::same_as<Element, std::complex<float>>) {
    return cublasCtbsv_64(handle, triangle, operation, diagonal, order,
                          bandwidth, ProviderPointer(matrix), leading_dimension,
                          ProviderPointer(vector), increment);
  } else {
    return cublasZtbsv_64(handle, triangle, operation, diagonal, order,
                          bandwidth, ProviderPointer(matrix), leading_dimension,
                          ProviderPointer(vector), increment);
  }
}

template <typename Element>
cublasStatus_t ProviderTpsv(cublasHandle_t handle, cublasFillMode_t triangle,
                            cublasOperation_t operation,
                            cublasDiagType_t diagonal, extent_t order,
                            const Element* matrix, Element* vector,
                            stride_t increment) {
  if constexpr (std::same_as<Element, float>) {
    return cublasStpsv_64(handle, triangle, operation, diagonal, order, matrix,
                          vector, increment);
  } else if constexpr (std::same_as<Element, double>) {
    return cublasDtpsv_64(handle, triangle, operation, diagonal, order, matrix,
                          vector, increment);
  } else if constexpr (std::same_as<Element, std::complex<float>>) {
    return cublasCtpsv_64(handle, triangle, operation, diagonal, order,
                          ProviderPointer(matrix), ProviderPointer(vector),
                          increment);
  } else {
    return cublasZtpsv_64(handle, triangle, operation, diagonal, order,
                          ProviderPointer(matrix), ProviderPointer(vector),
                          increment);
  }
}

template <typename Element, typename Matrix>
Result<CompletionEvent> Triangular(DenseCudaContext& context,
                                   DenseBlasTriangle triangle,
                                   DenseBlasTranspose transpose,
                                   DenseBlasDiagonal diagonal, Matrix matrix,
                                   Vector<Element> vector, bool solve) {
  auto prepared = Prepare(context);
  if (!prepared.ok()) {
    return prepared.status();
  }
  if (!IsValidTriangle(triangle) || !IsValidTranspose(transpose) ||
      !IsValidDiagonal(diagonal)) {
    return Status(ErrorCode::kInvalidArgument,
                  "A CUDA triangular operation flag is invalid");
  }
  const std::int32_t device = prepared->state->device();
  for (Status status :
       {ValidateDevice(matrix, device), ValidateDevice(vector, device)}) {
    if (!status.ok()) {
      return status;
    }
  }
  const extent_t order = [&] {
    if constexpr (requires { matrix.order(); }) {
      return matrix.order();
    } else {
      return matrix.rows();
    }
  }();
  if constexpr (requires { matrix.columns(); }) {
    if (matrix.rows() != matrix.columns()) {
      return Status(ErrorCode::kShape,
                    "A CUDA triangular matrix must be square");
    }
  }
  if (vector.size() != order) {
    return Status(ErrorCode::kShape,
                  "A CUDA triangular matrix and vector have different orders");
  }
  if (Overlap(matrix.reachable_storage(), vector.reachable_storage())) {
    return Status(ErrorCode::kInvalidArgument,
                  "A CUDA triangular matrix overlaps its vector");
  }
  if (order == 0) {
    return RecordOrDrain(context, *prepared);
  }
  if constexpr (DenseBlasComplex<Element>) {
    if (matrix.layout() == DenseBlasLayout::kRowMajor &&
        transpose == DenseBlasTranspose::kConjugateTranspose) {
      return Finish(
          context, *prepared,
          LaunchLevel2ConjugateTriangular(
              prepared->stream, ElementKind<Element>(),
              Describe(matrix, triangle == DenseBlasTriangle::kUpper),
              Describe(vector), diagonal == DenseBlasDiagonal::kUnit, solve));
    }
  }

  cublasOperation_t operation = ProviderOperation(transpose);
  if (matrix.layout() == DenseBlasLayout::kRowMajor) {
    operation =
        transpose == DenseBlasTranspose::kNone ? CUBLAS_OP_T : CUBLAS_OP_N;
  }
  const cublasFillMode_t provider_triangle =
      ProviderTriangle(triangle, matrix.layout());
  const cublasDiagType_t provider_diagonal = ProviderDiagonal(diagonal);
  cublasStatus_t provider_status = CUBLAS_STATUS_NOT_SUPPORTED;
  if constexpr (requires { matrix.bandwidth(); }) {
    provider_status =
        solve ? ProviderTbsv(
                    prepared->state->handle(), provider_triangle, operation,
                    provider_diagonal, order, matrix.bandwidth(), matrix.data(),
                    matrix.leading_dimension(), SignedProviderPointer(vector),
                    ProviderIncrement(vector))
              : ProviderTbmv(
                    prepared->state->handle(), provider_triangle, operation,
                    provider_diagonal, order, matrix.bandwidth(), matrix.data(),
                    matrix.leading_dimension(), SignedProviderPointer(vector),
                    ProviderIncrement(vector));
  } else if constexpr (requires { matrix.leading_dimension(); }) {
    provider_status =
        solve ? ProviderTrsv(prepared->state->handle(), provider_triangle,
                             operation, provider_diagonal, order, matrix.data(),
                             matrix.leading_dimension(),
                             SignedProviderPointer(vector),
                             ProviderIncrement(vector))
              : ProviderTrmv(prepared->state->handle(), provider_triangle,
                             operation, provider_diagonal, order, matrix.data(),
                             matrix.leading_dimension(),
                             SignedProviderPointer(vector),
                             ProviderIncrement(vector));
  } else {
    provider_status =
        solve ? ProviderTpsv(prepared->state->handle(), provider_triangle,
                             operation, provider_diagonal, order, matrix.data(),
                             SignedProviderPointer(vector),
                             ProviderIncrement(vector))
              : ProviderTpmv(prepared->state->handle(), provider_triangle,
                             operation, provider_diagonal, order, matrix.data(),
                             SignedProviderPointer(vector),
                             ProviderIncrement(vector));
  }
  return Finish(context, *prepared, provider_status,
                "cuBLAS could not enqueue a triangular Level 2 call");
}

template <typename Element>
cublasStatus_t ProviderGer(cublasHandle_t handle, extent_t rows,
                           extent_t columns, Element alpha, const Element* x,
                           stride_t x_increment, const Element* y,
                           stride_t y_increment, Element* matrix,
                           stride_t leading_dimension, bool conjugate) {
  if constexpr (std::same_as<Element, float>) {
    return cublasSger_64(handle, rows, columns, &alpha, x, x_increment, y,
                         y_increment, matrix, leading_dimension);
  } else if constexpr (std::same_as<Element, double>) {
    return cublasDger_64(handle, rows, columns, &alpha, x, x_increment, y,
                         y_increment, matrix, leading_dimension);
  } else if constexpr (std::same_as<Element, std::complex<float>>) {
    const cuComplex provider_alpha = make_cuComplex(alpha.real(), alpha.imag());
    if (conjugate) {
      return cublasCgerc_64(handle, rows, columns, &provider_alpha,
                            ProviderPointer(x), x_increment, ProviderPointer(y),
                            y_increment, ProviderPointer(matrix),
                            leading_dimension);
    }
    return cublasCgeru_64(handle, rows, columns, &provider_alpha,
                          ProviderPointer(x), x_increment, ProviderPointer(y),
                          y_increment, ProviderPointer(matrix),
                          leading_dimension);
  } else {
    const cuDoubleComplex provider_alpha =
        make_cuDoubleComplex(alpha.real(), alpha.imag());
    if (conjugate) {
      return cublasZgerc_64(handle, rows, columns, &provider_alpha,
                            ProviderPointer(x), x_increment, ProviderPointer(y),
                            y_increment, ProviderPointer(matrix),
                            leading_dimension);
    }
    return cublasZgeru_64(handle, rows, columns, &provider_alpha,
                          ProviderPointer(x), x_increment, ProviderPointer(y),
                          y_increment, ProviderPointer(matrix),
                          leading_dimension);
  }
}

template <typename Element>
Result<CompletionEvent> GeneralRankUpdate(DenseCudaContext& context,
                                          Element alpha,
                                          Vector<const Element> x,
                                          Vector<const Element> y,
                                          DenseBlasMatrixView<Element> matrix,
                                          bool conjugate) {
  auto prepared = Prepare(context);
  if (!prepared.ok()) {
    return prepared.status();
  }
  const std::int32_t device = prepared->state->device();
  for (Status status : {ValidateDevice(x, device), ValidateDevice(y, device),
                        ValidateDevice(matrix, device)}) {
    if (!status.ok()) {
      return status;
    }
  }
  if (x.size() != matrix.rows() || y.size() != matrix.columns()) {
    return Status(ErrorCode::kShape,
                  "A CUDA general rank update has mismatched shapes");
  }
  if (Overlap(x.reachable_storage(), matrix.reachable_storage()) ||
      Overlap(y.reachable_storage(), matrix.reachable_storage())) {
    return Status(ErrorCode::kInvalidArgument,
                  "A CUDA rank-update matrix overlaps an input vector");
  }
  if (matrix.rows() == 0 || matrix.columns() == 0 || alpha == Element{0}) {
    return RecordOrDrain(context, *prepared);
  }
  if constexpr (DenseBlasComplex<Element>) {
    if (matrix.layout() == DenseBlasLayout::kRowMajor && conjugate) {
      return Finish(context, *prepared,
                    LaunchLevel2Gerc(prepared->stream, ElementKind<Element>(),
                                     Scalar(alpha), Describe(x), Describe(y),
                                     Describe(matrix)));
    }
  }
  extent_t rows = matrix.rows();
  extent_t columns = matrix.columns();
  auto provider_x = x;
  auto provider_y = y;
  if (matrix.layout() == DenseBlasLayout::kRowMajor) {
    std::swap(rows, columns);
    provider_x = y;
    provider_y = x;
  }
  return Finish(context, *prepared,
                ProviderGer(prepared->state->handle(), rows, columns, alpha,
                            SignedProviderPointer(provider_x),
                            ProviderIncrement(provider_x),
                            SignedProviderPointer(provider_y),
                            ProviderIncrement(provider_y), matrix.data(),
                            matrix.leading_dimension(), conjugate),
                "cuBLAS could not enqueue a general rank update");
}

template <typename Element, typename Alpha>
cublasStatus_t ProviderHer(cublasHandle_t handle, cublasFillMode_t triangle,
                           extent_t order, Alpha alpha, const Element* x,
                           stride_t increment, Element* matrix,
                           stride_t leading_dimension) {
  if constexpr (std::same_as<Element, std::complex<float>>) {
    return cublasCher_64(handle, triangle, order, &alpha, ProviderPointer(x),
                         increment, ProviderPointer(matrix), leading_dimension);
  } else {
    return cublasZher_64(handle, triangle, order, &alpha, ProviderPointer(x),
                         increment, ProviderPointer(matrix), leading_dimension);
  }
}

template <typename Element, typename Alpha>
cublasStatus_t ProviderHpr(cublasHandle_t handle, cublasFillMode_t triangle,
                           extent_t order, Alpha alpha, const Element* x,
                           stride_t increment, Element* matrix) {
  if constexpr (std::same_as<Element, std::complex<float>>) {
    return cublasChpr_64(handle, triangle, order, &alpha, ProviderPointer(x),
                         increment, ProviderPointer(matrix));
  } else {
    return cublasZhpr_64(handle, triangle, order, &alpha, ProviderPointer(x),
                         increment, ProviderPointer(matrix));
  }
}

template <typename Element>
cublasStatus_t ProviderHer2(cublasHandle_t handle, cublasFillMode_t triangle,
                            extent_t order, Element alpha, const Element* x,
                            stride_t x_increment, const Element* y,
                            stride_t y_increment, Element* matrix,
                            stride_t leading_dimension) {
  if constexpr (std::same_as<Element, std::complex<float>>) {
    const cuComplex provider_alpha = make_cuComplex(alpha.real(), alpha.imag());
    return cublasCher2_64(handle, triangle, order, &provider_alpha,
                          ProviderPointer(x), x_increment, ProviderPointer(y),
                          y_increment, ProviderPointer(matrix),
                          leading_dimension);
  } else {
    const cuDoubleComplex provider_alpha =
        make_cuDoubleComplex(alpha.real(), alpha.imag());
    return cublasZher2_64(handle, triangle, order, &provider_alpha,
                          ProviderPointer(x), x_increment, ProviderPointer(y),
                          y_increment, ProviderPointer(matrix),
                          leading_dimension);
  }
}

template <typename Element>
cublasStatus_t ProviderHpr2(cublasHandle_t handle, cublasFillMode_t triangle,
                            extent_t order, Element alpha, const Element* x,
                            stride_t x_increment, const Element* y,
                            stride_t y_increment, Element* matrix) {
  if constexpr (std::same_as<Element, std::complex<float>>) {
    const cuComplex provider_alpha = make_cuComplex(alpha.real(), alpha.imag());
    return cublasChpr2_64(handle, triangle, order, &provider_alpha,
                          ProviderPointer(x), x_increment, ProviderPointer(y),
                          y_increment, ProviderPointer(matrix));
  } else {
    const cuDoubleComplex provider_alpha =
        make_cuDoubleComplex(alpha.real(), alpha.imag());
    return cublasZhpr2_64(handle, triangle, order, &provider_alpha,
                          ProviderPointer(x), x_increment, ProviderPointer(y),
                          y_increment, ProviderPointer(matrix));
  }
}

template <typename Element>
cublasStatus_t ProviderSyr(cublasHandle_t handle, cublasFillMode_t triangle,
                           extent_t order, Element alpha, const Element* x,
                           stride_t increment, Element* matrix,
                           stride_t leading_dimension) {
  if constexpr (std::same_as<Element, float>) {
    return cublasSsyr_64(handle, triangle, order, &alpha, x, increment, matrix,
                         leading_dimension);
  } else {
    return cublasDsyr_64(handle, triangle, order, &alpha, x, increment, matrix,
                         leading_dimension);
  }
}

template <typename Element>
cublasStatus_t ProviderSpr(cublasHandle_t handle, cublasFillMode_t triangle,
                           extent_t order, Element alpha, const Element* x,
                           stride_t increment, Element* matrix) {
  if constexpr (std::same_as<Element, float>) {
    return cublasSspr_64(handle, triangle, order, &alpha, x, increment, matrix);
  } else {
    return cublasDspr_64(handle, triangle, order, &alpha, x, increment, matrix);
  }
}

template <typename Element>
cublasStatus_t ProviderSyr2(cublasHandle_t handle, cublasFillMode_t triangle,
                            extent_t order, Element alpha, const Element* x,
                            stride_t x_increment, const Element* y,
                            stride_t y_increment, Element* matrix,
                            stride_t leading_dimension) {
  if constexpr (std::same_as<Element, float>) {
    return cublasSsyr2_64(handle, triangle, order, &alpha, x, x_increment, y,
                          y_increment, matrix, leading_dimension);
  } else {
    return cublasDsyr2_64(handle, triangle, order, &alpha, x, x_increment, y,
                          y_increment, matrix, leading_dimension);
  }
}

template <typename Element>
cublasStatus_t ProviderSpr2(cublasHandle_t handle, cublasFillMode_t triangle,
                            extent_t order, Element alpha, const Element* x,
                            stride_t x_increment, const Element* y,
                            stride_t y_increment, Element* matrix) {
  if constexpr (std::same_as<Element, float>) {
    return cublasSspr2_64(handle, triangle, order, &alpha, x, x_increment, y,
                          y_increment, matrix);
  } else {
    return cublasDspr2_64(handle, triangle, order, &alpha, x, x_increment, y,
                          y_increment, matrix);
  }
}

template <bool kRankTwo, bool kHermitian, typename Element, typename Matrix,
          typename Alpha>
Result<CompletionEvent> StructuredRankUpdate(
    DenseCudaContext& context, DenseBlasTriangle triangle, Alpha alpha,
    Vector<const Element> x, Vector<const Element> y, Matrix matrix) {
  auto prepared = Prepare(context);
  if (!prepared.ok()) {
    return prepared.status();
  }
  if (!IsValidTriangle(triangle)) {
    return Status(ErrorCode::kInvalidArgument,
                  "A CUDA rank-update triangle value is invalid");
  }
  const std::int32_t device = prepared->state->device();
  for (Status status : {ValidateDevice(x, device), ValidateDevice(y, device),
                        ValidateDevice(matrix, device)}) {
    if (!status.ok()) {
      return status;
    }
  }
  const extent_t order = [&] {
    if constexpr (requires { matrix.order(); }) {
      return matrix.order();
    } else {
      return matrix.rows();
    }
  }();
  if constexpr (requires { matrix.columns(); }) {
    if (matrix.rows() != matrix.columns()) {
      return Status(ErrorCode::kShape,
                    "A CUDA structured update matrix must be square");
    }
  }
  if (x.size() != order || (kRankTwo && y.size() != order)) {
    return Status(ErrorCode::kShape,
                  "A CUDA structured update vector has the wrong length");
  }
  if (Overlap(x.reachable_storage(), matrix.reachable_storage()) ||
      (kRankTwo &&
       Overlap(y.reachable_storage(), matrix.reachable_storage()))) {
    return Status(ErrorCode::kInvalidArgument,
                  "A CUDA structured update overlaps an input vector");
  }
  if (order == 0 || alpha == Alpha{0}) {
    return RecordOrDrain(context, *prepared);
  }
  if constexpr (DenseBlasComplex<Element>) {
    if (kHermitian && matrix.layout() == DenseBlasLayout::kRowMajor) {
      Level2MatrixDescriptor descriptor =
          Describe(matrix, triangle == DenseBlasTriangle::kUpper);
      Level2ComplexScalar project_alpha;
      if constexpr (DenseBlasComplex<Alpha>) {
        project_alpha = Scalar(alpha);
      } else {
        project_alpha = {static_cast<double>(alpha), 0};
      }
      return Finish(context, *prepared,
                    LaunchLevel2HermitianUpdate(
                        prepared->stream, ElementKind<Element>(), project_alpha,
                        Describe(x), kRankTwo ? Describe(y) : Describe(x),
                        descriptor, kRankTwo));
    }
  }
  const cublasFillMode_t provider_triangle =
      ProviderTriangle(triangle, matrix.layout());
  cublasStatus_t provider_status = CUBLAS_STATUS_NOT_SUPPORTED;
  if constexpr (DenseBlasComplex<Element>) {
    if constexpr (kRankTwo) {
      if constexpr (requires { matrix.leading_dimension(); }) {
        provider_status =
            ProviderHer2(prepared->state->handle(), provider_triangle, order,
                         alpha, SignedProviderPointer(x), ProviderIncrement(x),
                         SignedProviderPointer(y), ProviderIncrement(y),
                         matrix.data(), matrix.leading_dimension());
      } else {
        provider_status = ProviderHpr2(
            prepared->state->handle(), provider_triangle, order, alpha,
            SignedProviderPointer(x), ProviderIncrement(x),
            SignedProviderPointer(y), ProviderIncrement(y), matrix.data());
      }
    } else {
      if constexpr (requires { matrix.leading_dimension(); }) {
        provider_status =
            ProviderHer(prepared->state->handle(), provider_triangle, order,
                        alpha, SignedProviderPointer(x), ProviderIncrement(x),
                        matrix.data(), matrix.leading_dimension());
      } else {
        provider_status = ProviderHpr(
            prepared->state->handle(), provider_triangle, order, alpha,
            SignedProviderPointer(x), ProviderIncrement(x), matrix.data());
      }
    }
  } else {
    if constexpr (kRankTwo) {
      if constexpr (requires { matrix.leading_dimension(); }) {
        provider_status =
            ProviderSyr2(prepared->state->handle(), provider_triangle, order,
                         alpha, SignedProviderPointer(x), ProviderIncrement(x),
                         SignedProviderPointer(y), ProviderIncrement(y),
                         matrix.data(), matrix.leading_dimension());
      } else {
        provider_status = ProviderSpr2(
            prepared->state->handle(), provider_triangle, order, alpha,
            SignedProviderPointer(x), ProviderIncrement(x),
            SignedProviderPointer(y), ProviderIncrement(y), matrix.data());
      }
    } else {
      if constexpr (requires { matrix.leading_dimension(); }) {
        provider_status =
            ProviderSyr(prepared->state->handle(), provider_triangle, order,
                        alpha, SignedProviderPointer(x), ProviderIncrement(x),
                        matrix.data(), matrix.leading_dimension());
      } else {
        provider_status = ProviderSpr(
            prepared->state->handle(), provider_triangle, order, alpha,
            SignedProviderPointer(x), ProviderIncrement(x), matrix.data());
      }
    }
  }
  return Finish(context, *prepared, provider_status,
                "cuBLAS could not enqueue a structured rank update");
}

}  // namespace
}  // namespace asc::internal_dense_cuda

namespace asc {

template <DenseBlasScalar Element>
Result<CompletionEvent> CudaGemv(DenseCudaContext& context,
                                 DenseBlasTranspose transpose, Element alpha,
                                 DenseBlasMatrixView<const Element> matrix,
                                 DenseBlasVectorView<const Element> input,
                                 Element beta,
                                 DenseBlasVectorView<Element> output) {
  return internal_dense_cuda::GeneralMv(context, transpose, alpha, matrix,
                                        input, beta, output);
}

template <DenseBlasScalar Element>
Result<CompletionEvent> CudaGbmv(DenseCudaContext& context,
                                 DenseBlasTranspose transpose, Element alpha,
                                 DenseBlasBandMatrixView<const Element> matrix,
                                 DenseBlasVectorView<const Element> input,
                                 Element beta,
                                 DenseBlasVectorView<Element> output) {
  return internal_dense_cuda::GeneralMv(context, transpose, alpha, matrix,
                                        input, beta, output);
}

template <DenseBlasComplex Element>
Result<CompletionEvent> CudaHemv(DenseCudaContext& context,
                                 DenseBlasTriangle triangle, Element alpha,
                                 DenseBlasMatrixView<const Element> matrix,
                                 DenseBlasVectorView<const Element> input,
                                 Element beta,
                                 DenseBlasVectorView<Element> output) {
  return internal_dense_cuda::StructuredMv(context, triangle, alpha, matrix,
                                           input, beta, output);
}

template <DenseBlasComplex Element>
Result<CompletionEvent> CudaHbmv(
    DenseCudaContext& context, DenseBlasTriangle triangle, Element alpha,
    DenseBlasTriangularBandView<const Element> matrix,
    DenseBlasVectorView<const Element> input, Element beta,
    DenseBlasVectorView<Element> output) {
  return internal_dense_cuda::StructuredMv(context, triangle, alpha, matrix,
                                           input, beta, output);
}

template <DenseBlasComplex Element>
Result<CompletionEvent> CudaHpmv(
    DenseCudaContext& context, DenseBlasTriangle triangle, Element alpha,
    DenseBlasPackedMatrixView<const Element> matrix,
    DenseBlasVectorView<const Element> input, Element beta,
    DenseBlasVectorView<Element> output) {
  return internal_dense_cuda::StructuredMv(context, triangle, alpha, matrix,
                                           input, beta, output);
}

template <DenseBlasReal Element>
Result<CompletionEvent> CudaSymv(DenseCudaContext& context,
                                 DenseBlasTriangle triangle, Element alpha,
                                 DenseBlasMatrixView<const Element> matrix,
                                 DenseBlasVectorView<const Element> input,
                                 Element beta,
                                 DenseBlasVectorView<Element> output) {
  return internal_dense_cuda::StructuredMv(context, triangle, alpha, matrix,
                                           input, beta, output);
}

template <DenseBlasReal Element>
Result<CompletionEvent> CudaSbmv(
    DenseCudaContext& context, DenseBlasTriangle triangle, Element alpha,
    DenseBlasTriangularBandView<const Element> matrix,
    DenseBlasVectorView<const Element> input, Element beta,
    DenseBlasVectorView<Element> output) {
  return internal_dense_cuda::StructuredMv(context, triangle, alpha, matrix,
                                           input, beta, output);
}

template <DenseBlasReal Element>
Result<CompletionEvent> CudaSpmv(
    DenseCudaContext& context, DenseBlasTriangle triangle, Element alpha,
    DenseBlasPackedMatrixView<const Element> matrix,
    DenseBlasVectorView<const Element> input, Element beta,
    DenseBlasVectorView<Element> output) {
  return internal_dense_cuda::StructuredMv(context, triangle, alpha, matrix,
                                           input, beta, output);
}

template <DenseBlasScalar Element>
Result<CompletionEvent> CudaTrmv(DenseCudaContext& context,
                                 DenseBlasTriangle triangle,
                                 DenseBlasTranspose transpose,
                                 DenseBlasDiagonal diagonal,
                                 DenseBlasMatrixView<const Element> matrix,
                                 DenseBlasVectorView<Element> vector) {
  return internal_dense_cuda::Triangular(context, triangle, transpose, diagonal,
                                         matrix, vector, false);
}

template <DenseBlasScalar Element>
Result<CompletionEvent> CudaTbmv(
    DenseCudaContext& context, DenseBlasTriangle triangle,
    DenseBlasTranspose transpose, DenseBlasDiagonal diagonal,
    DenseBlasTriangularBandView<const Element> matrix,
    DenseBlasVectorView<Element> vector) {
  return internal_dense_cuda::Triangular(context, triangle, transpose, diagonal,
                                         matrix, vector, false);
}

template <DenseBlasScalar Element>
Result<CompletionEvent> CudaTpmv(
    DenseCudaContext& context, DenseBlasTriangle triangle,
    DenseBlasTranspose transpose, DenseBlasDiagonal diagonal,
    DenseBlasPackedMatrixView<const Element> matrix,
    DenseBlasVectorView<Element> vector) {
  return internal_dense_cuda::Triangular(context, triangle, transpose, diagonal,
                                         matrix, vector, false);
}

template <DenseBlasScalar Element>
Result<CompletionEvent> CudaTrsv(DenseCudaContext& context,
                                 DenseBlasTriangle triangle,
                                 DenseBlasTranspose transpose,
                                 DenseBlasDiagonal diagonal,
                                 DenseBlasMatrixView<const Element> matrix,
                                 DenseBlasVectorView<Element> vector) {
  return internal_dense_cuda::Triangular(context, triangle, transpose, diagonal,
                                         matrix, vector, true);
}

template <DenseBlasScalar Element>
Result<CompletionEvent> CudaTbsv(
    DenseCudaContext& context, DenseBlasTriangle triangle,
    DenseBlasTranspose transpose, DenseBlasDiagonal diagonal,
    DenseBlasTriangularBandView<const Element> matrix,
    DenseBlasVectorView<Element> vector) {
  return internal_dense_cuda::Triangular(context, triangle, transpose, diagonal,
                                         matrix, vector, true);
}

template <DenseBlasScalar Element>
Result<CompletionEvent> CudaTpsv(
    DenseCudaContext& context, DenseBlasTriangle triangle,
    DenseBlasTranspose transpose, DenseBlasDiagonal diagonal,
    DenseBlasPackedMatrixView<const Element> matrix,
    DenseBlasVectorView<Element> vector) {
  return internal_dense_cuda::Triangular(context, triangle, transpose, diagonal,
                                         matrix, vector, true);
}

template <DenseBlasReal Element>
Result<CompletionEvent> CudaGer(DenseCudaContext& context, Element alpha,
                                DenseBlasVectorView<const Element> x,
                                DenseBlasVectorView<const Element> y,
                                DenseBlasMatrixView<Element> matrix) {
  return internal_dense_cuda::GeneralRankUpdate(context, alpha, x, y, matrix,
                                                false);
}

template <DenseBlasComplex Element>
Result<CompletionEvent> CudaGeru(DenseCudaContext& context, Element alpha,
                                 DenseBlasVectorView<const Element> x,
                                 DenseBlasVectorView<const Element> y,
                                 DenseBlasMatrixView<Element> matrix) {
  return internal_dense_cuda::GeneralRankUpdate(context, alpha, x, y, matrix,
                                                false);
}

template <DenseBlasComplex Element>
Result<CompletionEvent> CudaGerc(DenseCudaContext& context, Element alpha,
                                 DenseBlasVectorView<const Element> x,
                                 DenseBlasVectorView<const Element> y,
                                 DenseBlasMatrixView<Element> matrix) {
  return internal_dense_cuda::GeneralRankUpdate(context, alpha, x, y, matrix,
                                                true);
}

template <DenseBlasComplex Element>
Result<CompletionEvent> CudaHer(DenseCudaContext& context,
                                DenseBlasTriangle triangle,
                                DenseBlasRealType<Element> alpha,
                                DenseBlasVectorView<const Element> x,
                                DenseBlasMatrixView<Element> matrix) {
  return internal_dense_cuda::StructuredRankUpdate<false, true>(
      context, triangle, alpha, x, x, matrix);
}

template <DenseBlasComplex Element>
Result<CompletionEvent> CudaHpr(DenseCudaContext& context,
                                DenseBlasTriangle triangle,
                                DenseBlasRealType<Element> alpha,
                                DenseBlasVectorView<const Element> x,
                                DenseBlasPackedMatrixView<Element> matrix) {
  return internal_dense_cuda::StructuredRankUpdate<false, true>(
      context, triangle, alpha, x, x, matrix);
}

template <DenseBlasComplex Element>
Result<CompletionEvent> CudaHer2(DenseCudaContext& context,
                                 DenseBlasTriangle triangle, Element alpha,
                                 DenseBlasVectorView<const Element> x,
                                 DenseBlasVectorView<const Element> y,
                                 DenseBlasMatrixView<Element> matrix) {
  return internal_dense_cuda::StructuredRankUpdate<true, true>(
      context, triangle, alpha, x, y, matrix);
}

template <DenseBlasComplex Element>
Result<CompletionEvent> CudaHpr2(DenseCudaContext& context,
                                 DenseBlasTriangle triangle, Element alpha,
                                 DenseBlasVectorView<const Element> x,
                                 DenseBlasVectorView<const Element> y,
                                 DenseBlasPackedMatrixView<Element> matrix) {
  return internal_dense_cuda::StructuredRankUpdate<true, true>(
      context, triangle, alpha, x, y, matrix);
}

template <DenseBlasReal Element>
Result<CompletionEvent> CudaSyr(DenseCudaContext& context,
                                DenseBlasTriangle triangle, Element alpha,
                                DenseBlasVectorView<const Element> x,
                                DenseBlasMatrixView<Element> matrix) {
  return internal_dense_cuda::StructuredRankUpdate<false, false>(
      context, triangle, alpha, x, x, matrix);
}

template <DenseBlasReal Element>
Result<CompletionEvent> CudaSpr(DenseCudaContext& context,
                                DenseBlasTriangle triangle, Element alpha,
                                DenseBlasVectorView<const Element> x,
                                DenseBlasPackedMatrixView<Element> matrix) {
  return internal_dense_cuda::StructuredRankUpdate<false, false>(
      context, triangle, alpha, x, x, matrix);
}

template <DenseBlasReal Element>
Result<CompletionEvent> CudaSyr2(DenseCudaContext& context,
                                 DenseBlasTriangle triangle, Element alpha,
                                 DenseBlasVectorView<const Element> x,
                                 DenseBlasVectorView<const Element> y,
                                 DenseBlasMatrixView<Element> matrix) {
  return internal_dense_cuda::StructuredRankUpdate<true, false>(
      context, triangle, alpha, x, y, matrix);
}

template <DenseBlasReal Element>
Result<CompletionEvent> CudaSpr2(DenseCudaContext& context,
                                 DenseBlasTriangle triangle, Element alpha,
                                 DenseBlasVectorView<const Element> x,
                                 DenseBlasVectorView<const Element> y,
                                 DenseBlasPackedMatrixView<Element> matrix) {
  return internal_dense_cuda::StructuredRankUpdate<true, false>(
      context, triangle, alpha, x, y, matrix);
}

#define ASC_INSTANTIATE_CUDA_GENERAL(Type)                                    \
  template Result<CompletionEvent> CudaGemv<Type>(                            \
      DenseCudaContext&, DenseBlasTranspose, Type,                            \
      DenseBlasMatrixView<const Type>, DenseBlasVectorView<const Type>, Type, \
      DenseBlasVectorView<Type>);                                             \
  template Result<CompletionEvent> CudaGbmv<Type>(                            \
      DenseCudaContext&, DenseBlasTranspose, Type,                            \
      DenseBlasBandMatrixView<const Type>, DenseBlasVectorView<const Type>,   \
      Type, DenseBlasVectorView<Type>);                                       \
  template Result<CompletionEvent> CudaTrmv<Type>(                            \
      DenseCudaContext&, DenseBlasTriangle, DenseBlasTranspose,               \
      DenseBlasDiagonal, DenseBlasMatrixView<const Type>,                     \
      DenseBlasVectorView<Type>);                                             \
  template Result<CompletionEvent> CudaTbmv<Type>(                            \
      DenseCudaContext&, DenseBlasTriangle, DenseBlasTranspose,               \
      DenseBlasDiagonal, DenseBlasTriangularBandView<const Type>,             \
      DenseBlasVectorView<Type>);                                             \
  template Result<CompletionEvent> CudaTpmv<Type>(                            \
      DenseCudaContext&, DenseBlasTriangle, DenseBlasTranspose,               \
      DenseBlasDiagonal, DenseBlasPackedMatrixView<const Type>,               \
      DenseBlasVectorView<Type>);                                             \
  template Result<CompletionEvent> CudaTrsv<Type>(                            \
      DenseCudaContext&, DenseBlasTriangle, DenseBlasTranspose,               \
      DenseBlasDiagonal, DenseBlasMatrixView<const Type>,                     \
      DenseBlasVectorView<Type>);                                             \
  template Result<CompletionEvent> CudaTbsv<Type>(                            \
      DenseCudaContext&, DenseBlasTriangle, DenseBlasTranspose,               \
      DenseBlasDiagonal, DenseBlasTriangularBandView<const Type>,             \
      DenseBlasVectorView<Type>);                                             \
  template Result<CompletionEvent> CudaTpsv<Type>(                            \
      DenseCudaContext&, DenseBlasTriangle, DenseBlasTranspose,               \
      DenseBlasDiagonal, DenseBlasPackedMatrixView<const Type>,               \
      DenseBlasVectorView<Type>)

#define ASC_INSTANTIATE_CUDA_REAL(Type)                                       \
  ASC_INSTANTIATE_CUDA_GENERAL(Type);                                         \
  template Result<CompletionEvent> CudaSymv<Type>(                            \
      DenseCudaContext&, DenseBlasTriangle, Type,                             \
      DenseBlasMatrixView<const Type>, DenseBlasVectorView<const Type>, Type, \
      DenseBlasVectorView<Type>);                                             \
  template Result<CompletionEvent> CudaSbmv<Type>(                            \
      DenseCudaContext&, DenseBlasTriangle, Type,                             \
      DenseBlasTriangularBandView<const Type>,                                \
      DenseBlasVectorView<const Type>, Type, DenseBlasVectorView<Type>);      \
  template Result<CompletionEvent> CudaSpmv<Type>(                            \
      DenseCudaContext&, DenseBlasTriangle, Type,                             \
      DenseBlasPackedMatrixView<const Type>, DenseBlasVectorView<const Type>, \
      Type, DenseBlasVectorView<Type>);                                       \
  template Result<CompletionEvent> CudaGer<Type>(                             \
      DenseCudaContext&, Type, DenseBlasVectorView<const Type>,               \
      DenseBlasVectorView<const Type>, DenseBlasMatrixView<Type>);            \
  template Result<CompletionEvent> CudaSyr<Type>(                             \
      DenseCudaContext&, DenseBlasTriangle, Type,                             \
      DenseBlasVectorView<const Type>, DenseBlasMatrixView<Type>);            \
  template Result<CompletionEvent> CudaSpr<Type>(                             \
      DenseCudaContext&, DenseBlasTriangle, Type,                             \
      DenseBlasVectorView<const Type>, DenseBlasPackedMatrixView<Type>);      \
  template Result<CompletionEvent> CudaSyr2<Type>(                            \
      DenseCudaContext&, DenseBlasTriangle, Type,                             \
      DenseBlasVectorView<const Type>, DenseBlasVectorView<const Type>,       \
      DenseBlasMatrixView<Type>);                                             \
  template Result<CompletionEvent> CudaSpr2<Type>(                            \
      DenseCudaContext&, DenseBlasTriangle, Type,                             \
      DenseBlasVectorView<const Type>, DenseBlasVectorView<const Type>,       \
      DenseBlasPackedMatrixView<Type>)

#define ASC_INSTANTIATE_CUDA_COMPLEX(Type, Real)                              \
  ASC_INSTANTIATE_CUDA_GENERAL(Type);                                         \
  template Result<CompletionEvent> CudaHemv<Type>(                            \
      DenseCudaContext&, DenseBlasTriangle, Type,                             \
      DenseBlasMatrixView<const Type>, DenseBlasVectorView<const Type>, Type, \
      DenseBlasVectorView<Type>);                                             \
  template Result<CompletionEvent> CudaHbmv<Type>(                            \
      DenseCudaContext&, DenseBlasTriangle, Type,                             \
      DenseBlasTriangularBandView<const Type>,                                \
      DenseBlasVectorView<const Type>, Type, DenseBlasVectorView<Type>);      \
  template Result<CompletionEvent> CudaHpmv<Type>(                            \
      DenseCudaContext&, DenseBlasTriangle, Type,                             \
      DenseBlasPackedMatrixView<const Type>, DenseBlasVectorView<const Type>, \
      Type, DenseBlasVectorView<Type>);                                       \
  template Result<CompletionEvent> CudaGeru<Type>(                            \
      DenseCudaContext&, Type, DenseBlasVectorView<const Type>,               \
      DenseBlasVectorView<const Type>, DenseBlasMatrixView<Type>);            \
  template Result<CompletionEvent> CudaGerc<Type>(                            \
      DenseCudaContext&, Type, DenseBlasVectorView<const Type>,               \
      DenseBlasVectorView<const Type>, DenseBlasMatrixView<Type>);            \
  template Result<CompletionEvent> CudaHer<Type>(                             \
      DenseCudaContext&, DenseBlasTriangle, Real,                             \
      DenseBlasVectorView<const Type>, DenseBlasMatrixView<Type>);            \
  template Result<CompletionEvent> CudaHpr<Type>(                             \
      DenseCudaContext&, DenseBlasTriangle, Real,                             \
      DenseBlasVectorView<const Type>, DenseBlasPackedMatrixView<Type>);      \
  template Result<CompletionEvent> CudaHer2<Type>(                            \
      DenseCudaContext&, DenseBlasTriangle, Type,                             \
      DenseBlasVectorView<const Type>, DenseBlasVectorView<const Type>,       \
      DenseBlasMatrixView<Type>);                                             \
  template Result<CompletionEvent> CudaHpr2<Type>(                            \
      DenseCudaContext&, DenseBlasTriangle, Type,                             \
      DenseBlasVectorView<const Type>, DenseBlasVectorView<const Type>,       \
      DenseBlasPackedMatrixView<Type>)

ASC_INSTANTIATE_CUDA_REAL(float);
ASC_INSTANTIATE_CUDA_REAL(double);
ASC_INSTANTIATE_CUDA_COMPLEX(std::complex<float>, float);
ASC_INSTANTIATE_CUDA_COMPLEX(std::complex<double>, double);

#undef ASC_INSTANTIATE_CUDA_COMPLEX
#undef ASC_INSTANTIATE_CUDA_GENERAL
#undef ASC_INSTANTIATE_CUDA_REAL

}  // namespace asc
