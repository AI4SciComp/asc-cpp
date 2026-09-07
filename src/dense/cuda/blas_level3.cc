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
#include "blas_level2_kernels_internal.h"
#include "context_internal.h"

namespace asc::internal_dense_cuda {
namespace {

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
  ContextState* state = Access::State(context);
  if (state == nullptr) {
    return Status(ErrorCode::kInvalidState,
                  "A moved-from DenseCudaContext cannot launch Level 3 BLAS");
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

bool IsValidSide(DenseBlasSide side) {
  return side == DenseBlasSide::kLeft || side == DenseBlasSide::kRight;
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
        "CUDA could not inspect a Level 3 BLAS device pointer");
  }
  if (attributes.type != cudaMemoryTypeDevice || attributes.device != device) {
    return Status(ErrorCode::kMemoryAccess,
                  "A Level 3 BLAS operand is not on the context device");
  }
  return Status::Ok();
}

template <typename View>
Status ValidateDevice(const View& view, std::int32_t device) {
  if (view.memory_space() != MemorySpace::kDevice) {
    return Status(ErrorCode::kMemoryAccess,
                  "CUDA Level 3 BLAS requires device storage");
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

template <typename... Views>
bool SameLayout(const Views&... views) {
  const DenseBlasLayout layouts[] = {views.layout()...};
  for (std::size_t index = 1; index < sizeof...(Views); ++index) {
    if (layouts[index] != layouts[0]) {
      return false;
    }
  }
  return true;
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
Element* MatrixPointer(DenseBlasMatrixView<Element> matrix, index_t row,
                       index_t column) {
  const stride_t offset = matrix.layout() == DenseBlasLayout::kColumnMajor
                              ? column * matrix.leading_dimension() + row
                              : row * matrix.leading_dimension() + column;
  return matrix.data() + offset;
}

template <typename Element>
Status ScaleRows(void* stream, Element beta,
                 DenseBlasMatrixView<Element> matrix, bool triangular,
                 DenseBlasTriangle triangle, bool hermitian) {
  for (index_t row = 0; row < matrix.rows(); ++row) {
    index_t begin = 0;
    index_t end = matrix.columns();
    if (triangular) {
      begin = triangle == DenseBlasTriangle::kUpper ? row : 0;
      end = triangle == DenseBlasTriangle::kUpper ? matrix.columns() : row + 1;
    }
    if (begin < end) {
      const stride_t increment = matrix.layout() == DenseBlasLayout::kRowMajor
                                     ? stride_t{1}
                                     : matrix.leading_dimension();
      Status status = LaunchLevel2ScaleVector(
          stream, ElementKind<Element>(), Scalar(beta),
          {MatrixPointer(matrix, row, begin), end - begin, increment});
      if (!status.ok()) {
        return status;
      }
    }
    if constexpr (DenseBlasComplex<Element>) {
      if (hermitian && row < matrix.columns()) {
        using Real = DenseBlasRealType<Element>;
        auto* diagonal =
            reinterpret_cast<Real*>(MatrixPointer(matrix, row, row));
        const cudaError_t error = cudaMemsetAsync(
            diagonal + 1, 0, sizeof(Real), static_cast<cudaStream_t>(stream));
        if (error != cudaSuccess) {
          return internal_core_cuda::CudaStatus(
              error, ErrorCode::kProvider,
              "CUDA could not clear a Hermitian diagonal component");
        }
      }
    }
  }
  return Status::Ok();
}

template <typename Element>
Status ScaleMatrix(void* stream, Element beta,
                   DenseBlasMatrixView<Element> matrix) {
  return ScaleRows(stream, beta, matrix, false, DenseBlasTriangle::kUpper,
                   false);
}

template <typename Element>
Status ScaleTriangle(void* stream, Element beta,
                     DenseBlasMatrixView<Element> matrix,
                     DenseBlasTriangle triangle, bool hermitian) {
  return ScaleRows(stream, beta, matrix, true, triangle, hermitian);
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

cublasSideMode_t ProviderSide(DenseBlasSide side, DenseBlasLayout layout) {
  const bool left = side == DenseBlasSide::kLeft;
  const bool provider_left =
      layout == DenseBlasLayout::kColumnMajor ? left : !left;
  return provider_left ? CUBLAS_SIDE_LEFT : CUBLAS_SIDE_RIGHT;
}

cublasDiagType_t ProviderDiagonal(DenseBlasDiagonal diagonal) {
  return diagonal == DenseBlasDiagonal::kUnit ? CUBLAS_DIAG_UNIT
                                              : CUBLAS_DIAG_NON_UNIT;
}

template <typename Element>
cublasStatus_t ProviderGemm(cublasHandle_t handle,
                            cublasOperation_t left_operation,
                            cublasOperation_t right_operation, extent_t rows,
                            extent_t columns, extent_t inner, Element alpha,
                            const Element* left, stride_t left_ld,
                            const Element* right, stride_t right_ld,
                            Element beta, Element* output, stride_t output_ld) {
  if constexpr (std::same_as<Element, float>) {
    return cublasSgemm_64(handle, left_operation, right_operation, rows,
                          columns, inner, &alpha, left, left_ld, right,
                          right_ld, &beta, output, output_ld);
  } else if constexpr (std::same_as<Element, double>) {
    return cublasDgemm_64(handle, left_operation, right_operation, rows,
                          columns, inner, &alpha, left, left_ld, right,
                          right_ld, &beta, output, output_ld);
  } else if constexpr (std::same_as<Element, std::complex<float>>) {
    const cuComplex provider_alpha = make_cuComplex(alpha.real(), alpha.imag());
    const cuComplex provider_beta = make_cuComplex(beta.real(), beta.imag());
    return cublasCgemm_64(
        handle, left_operation, right_operation, rows, columns, inner,
        &provider_alpha, ProviderPointer(left), left_ld, ProviderPointer(right),
        right_ld, &provider_beta, ProviderPointer(output), output_ld);
  } else {
    const cuDoubleComplex provider_alpha =
        make_cuDoubleComplex(alpha.real(), alpha.imag());
    const cuDoubleComplex provider_beta =
        make_cuDoubleComplex(beta.real(), beta.imag());
    return cublasZgemm_64(
        handle, left_operation, right_operation, rows, columns, inner,
        &provider_alpha, ProviderPointer(left), left_ld, ProviderPointer(right),
        right_ld, &provider_beta, ProviderPointer(output), output_ld);
  }
}

template <typename Element>
cublasStatus_t ProviderSymm(cublasHandle_t handle, cublasSideMode_t side,
                            cublasFillMode_t triangle, extent_t rows,
                            extent_t columns, Element alpha,
                            const Element* structured, stride_t structured_ld,
                            const Element* other, stride_t other_ld,
                            Element beta, Element* output, stride_t output_ld) {
  if constexpr (std::same_as<Element, float>) {
    return cublasSsymm_64(handle, side, triangle, rows, columns, &alpha,
                          structured, structured_ld, other, other_ld, &beta,
                          output, output_ld);
  } else if constexpr (std::same_as<Element, double>) {
    return cublasDsymm_64(handle, side, triangle, rows, columns, &alpha,
                          structured, structured_ld, other, other_ld, &beta,
                          output, output_ld);
  } else if constexpr (std::same_as<Element, std::complex<float>>) {
    const cuComplex provider_alpha = make_cuComplex(alpha.real(), alpha.imag());
    const cuComplex provider_beta = make_cuComplex(beta.real(), beta.imag());
    return cublasCsymm_64(handle, side, triangle, rows, columns,
                          &provider_alpha, ProviderPointer(structured),
                          structured_ld, ProviderPointer(other), other_ld,
                          &provider_beta, ProviderPointer(output), output_ld);
  } else {
    const cuDoubleComplex provider_alpha =
        make_cuDoubleComplex(alpha.real(), alpha.imag());
    const cuDoubleComplex provider_beta =
        make_cuDoubleComplex(beta.real(), beta.imag());
    return cublasZsymm_64(handle, side, triangle, rows, columns,
                          &provider_alpha, ProviderPointer(structured),
                          structured_ld, ProviderPointer(other), other_ld,
                          &provider_beta, ProviderPointer(output), output_ld);
  }
}

template <DenseBlasComplex Element>
cublasStatus_t ProviderHemm(cublasHandle_t handle, cublasSideMode_t side,
                            cublasFillMode_t triangle, extent_t rows,
                            extent_t columns, Element alpha,
                            const Element* structured, stride_t structured_ld,
                            const Element* other, stride_t other_ld,
                            Element beta, Element* output, stride_t output_ld) {
  if constexpr (std::same_as<Element, std::complex<float>>) {
    const cuComplex provider_alpha = make_cuComplex(alpha.real(), alpha.imag());
    const cuComplex provider_beta = make_cuComplex(beta.real(), beta.imag());
    return cublasChemm_64(handle, side, triangle, rows, columns,
                          &provider_alpha, ProviderPointer(structured),
                          structured_ld, ProviderPointer(other), other_ld,
                          &provider_beta, ProviderPointer(output), output_ld);
  } else {
    const cuDoubleComplex provider_alpha =
        make_cuDoubleComplex(alpha.real(), alpha.imag());
    const cuDoubleComplex provider_beta =
        make_cuDoubleComplex(beta.real(), beta.imag());
    return cublasZhemm_64(handle, side, triangle, rows, columns,
                          &provider_alpha, ProviderPointer(structured),
                          structured_ld, ProviderPointer(other), other_ld,
                          &provider_beta, ProviderPointer(output), output_ld);
  }
}

template <typename Element>
cublasStatus_t ProviderSyrk(cublasHandle_t handle, cublasFillMode_t triangle,
                            cublasOperation_t operation, extent_t order,
                            extent_t inner, Element alpha, const Element* input,
                            stride_t input_ld, Element beta, Element* output,
                            stride_t output_ld) {
  if constexpr (std::same_as<Element, float>) {
    return cublasSsyrk_64(handle, triangle, operation, order, inner, &alpha,
                          input, input_ld, &beta, output, output_ld);
  } else if constexpr (std::same_as<Element, double>) {
    return cublasDsyrk_64(handle, triangle, operation, order, inner, &alpha,
                          input, input_ld, &beta, output, output_ld);
  } else if constexpr (std::same_as<Element, std::complex<float>>) {
    const cuComplex provider_alpha = make_cuComplex(alpha.real(), alpha.imag());
    const cuComplex provider_beta = make_cuComplex(beta.real(), beta.imag());
    return cublasCsyrk_64(handle, triangle, operation, order, inner,
                          &provider_alpha, ProviderPointer(input), input_ld,
                          &provider_beta, ProviderPointer(output), output_ld);
  } else {
    const cuDoubleComplex provider_alpha =
        make_cuDoubleComplex(alpha.real(), alpha.imag());
    const cuDoubleComplex provider_beta =
        make_cuDoubleComplex(beta.real(), beta.imag());
    return cublasZsyrk_64(handle, triangle, operation, order, inner,
                          &provider_alpha, ProviderPointer(input), input_ld,
                          &provider_beta, ProviderPointer(output), output_ld);
  }
}

template <DenseBlasComplex Element>
cublasStatus_t ProviderHerk(cublasHandle_t handle, cublasFillMode_t triangle,
                            cublasOperation_t operation, extent_t order,
                            extent_t inner, DenseBlasRealType<Element> alpha,
                            const Element* input, stride_t input_ld,
                            DenseBlasRealType<Element> beta, Element* output,
                            stride_t output_ld) {
  if constexpr (std::same_as<Element, std::complex<float>>) {
    return cublasCherk_64(handle, triangle, operation, order, inner, &alpha,
                          ProviderPointer(input), input_ld, &beta,
                          ProviderPointer(output), output_ld);
  } else {
    return cublasZherk_64(handle, triangle, operation, order, inner, &alpha,
                          ProviderPointer(input), input_ld, &beta,
                          ProviderPointer(output), output_ld);
  }
}

template <typename Element>
cublasStatus_t ProviderSyr2k(cublasHandle_t handle, cublasFillMode_t triangle,
                             cublasOperation_t operation, extent_t order,
                             extent_t inner, Element alpha, const Element* left,
                             stride_t left_ld, const Element* right,
                             stride_t right_ld, Element beta, Element* output,
                             stride_t output_ld) {
  if constexpr (std::same_as<Element, float>) {
    return cublasSsyr2k_64(handle, triangle, operation, order, inner, &alpha,
                           left, left_ld, right, right_ld, &beta, output,
                           output_ld);
  } else if constexpr (std::same_as<Element, double>) {
    return cublasDsyr2k_64(handle, triangle, operation, order, inner, &alpha,
                           left, left_ld, right, right_ld, &beta, output,
                           output_ld);
  } else if constexpr (std::same_as<Element, std::complex<float>>) {
    const cuComplex provider_alpha = make_cuComplex(alpha.real(), alpha.imag());
    const cuComplex provider_beta = make_cuComplex(beta.real(), beta.imag());
    return cublasCsyr2k_64(handle, triangle, operation, order, inner,
                           &provider_alpha, ProviderPointer(left), left_ld,
                           ProviderPointer(right), right_ld, &provider_beta,
                           ProviderPointer(output), output_ld);
  } else {
    const cuDoubleComplex provider_alpha =
        make_cuDoubleComplex(alpha.real(), alpha.imag());
    const cuDoubleComplex provider_beta =
        make_cuDoubleComplex(beta.real(), beta.imag());
    return cublasZsyr2k_64(handle, triangle, operation, order, inner,
                           &provider_alpha, ProviderPointer(left), left_ld,
                           ProviderPointer(right), right_ld, &provider_beta,
                           ProviderPointer(output), output_ld);
  }
}

template <DenseBlasComplex Element>
cublasStatus_t ProviderHer2k(cublasHandle_t handle, cublasFillMode_t triangle,
                             cublasOperation_t operation, extent_t order,
                             extent_t inner, Element alpha, const Element* left,
                             stride_t left_ld, const Element* right,
                             stride_t right_ld, DenseBlasRealType<Element> beta,
                             Element* output, stride_t output_ld) {
  if constexpr (std::same_as<Element, std::complex<float>>) {
    const cuComplex provider_alpha = make_cuComplex(alpha.real(), alpha.imag());
    return cublasCher2k_64(handle, triangle, operation, order, inner,
                           &provider_alpha, ProviderPointer(left), left_ld,
                           ProviderPointer(right), right_ld, &beta,
                           ProviderPointer(output), output_ld);
  } else {
    const cuDoubleComplex provider_alpha =
        make_cuDoubleComplex(alpha.real(), alpha.imag());
    return cublasZher2k_64(handle, triangle, operation, order, inner,
                           &provider_alpha, ProviderPointer(left), left_ld,
                           ProviderPointer(right), right_ld, &beta,
                           ProviderPointer(output), output_ld);
  }
}

template <typename Element>
cublasStatus_t ProviderTrmm(cublasHandle_t handle, cublasSideMode_t side,
                            cublasFillMode_t triangle,
                            cublasOperation_t operation,
                            cublasDiagType_t diagonal, extent_t rows,
                            extent_t columns, Element alpha,
                            const Element* triangular, stride_t triangular_ld,
                            Element* matrix, stride_t matrix_ld) {
  if constexpr (std::same_as<Element, float>) {
    return cublasStrmm_64(handle, side, triangle, operation, diagonal, rows,
                          columns, &alpha, triangular, triangular_ld, matrix,
                          matrix_ld, matrix, matrix_ld);
  } else if constexpr (std::same_as<Element, double>) {
    return cublasDtrmm_64(handle, side, triangle, operation, diagonal, rows,
                          columns, &alpha, triangular, triangular_ld, matrix,
                          matrix_ld, matrix, matrix_ld);
  } else if constexpr (std::same_as<Element, std::complex<float>>) {
    const cuComplex provider_alpha = make_cuComplex(alpha.real(), alpha.imag());
    return cublasCtrmm_64(handle, side, triangle, operation, diagonal, rows,
                          columns, &provider_alpha, ProviderPointer(triangular),
                          triangular_ld, ProviderPointer(matrix), matrix_ld,
                          ProviderPointer(matrix), matrix_ld);
  } else {
    const cuDoubleComplex provider_alpha =
        make_cuDoubleComplex(alpha.real(), alpha.imag());
    return cublasZtrmm_64(handle, side, triangle, operation, diagonal, rows,
                          columns, &provider_alpha, ProviderPointer(triangular),
                          triangular_ld, ProviderPointer(matrix), matrix_ld,
                          ProviderPointer(matrix), matrix_ld);
  }
}

template <typename Element>
cublasStatus_t ProviderTrsm(cublasHandle_t handle, cublasSideMode_t side,
                            cublasFillMode_t triangle,
                            cublasOperation_t operation,
                            cublasDiagType_t diagonal, extent_t rows,
                            extent_t columns, Element alpha,
                            const Element* triangular, stride_t triangular_ld,
                            Element* matrix, stride_t matrix_ld) {
  if constexpr (std::same_as<Element, float>) {
    return cublasStrsm_64(handle, side, triangle, operation, diagonal, rows,
                          columns, &alpha, triangular, triangular_ld, matrix,
                          matrix_ld);
  } else if constexpr (std::same_as<Element, double>) {
    return cublasDtrsm_64(handle, side, triangle, operation, diagonal, rows,
                          columns, &alpha, triangular, triangular_ld, matrix,
                          matrix_ld);
  } else if constexpr (std::same_as<Element, std::complex<float>>) {
    const cuComplex provider_alpha = make_cuComplex(alpha.real(), alpha.imag());
    return cublasCtrsm_64(handle, side, triangle, operation, diagonal, rows,
                          columns, &provider_alpha, ProviderPointer(triangular),
                          triangular_ld, ProviderPointer(matrix), matrix_ld);
  } else {
    const cuDoubleComplex provider_alpha =
        make_cuDoubleComplex(alpha.real(), alpha.imag());
    return cublasZtrsm_64(handle, side, triangle, operation, diagonal, rows,
                          columns, &provider_alpha, ProviderPointer(triangular),
                          triangular_ld, ProviderPointer(matrix), matrix_ld);
  }
}

template <typename Element>
Status ValidateThreeMatrices(std::int32_t device,
                             DenseBlasMatrixView<const Element> left,
                             DenseBlasMatrixView<const Element> right,
                             DenseBlasMatrixView<Element> output) {
  for (Status status :
       {ValidateDevice(left, device), ValidateDevice(right, device),
        ValidateDevice(output, device)}) {
    if (!status.ok()) {
      return status;
    }
  }
  if (!SameLayout(left, right, output)) {
    return Status(ErrorCode::kInvalidArgument,
                  "CUDA Level 3 operands must use one common layout");
  }
  if (Overlap(left.reachable_storage(), output.reachable_storage()) ||
      Overlap(right.reachable_storage(), output.reachable_storage())) {
    return Status(ErrorCode::kInvalidArgument,
                  "CUDA Level 3 output overlaps a read operand");
  }
  return Status::Ok();
}

template <typename Element>
Result<CompletionEvent> GemmImpl(DenseCudaContext& context,
                                 DenseBlasTranspose left_transpose,
                                 DenseBlasTranspose right_transpose,
                                 Element alpha,
                                 DenseBlasMatrixView<const Element> left,
                                 DenseBlasMatrixView<const Element> right,
                                 Element beta,
                                 DenseBlasMatrixView<Element> output) {
  auto prepared = Prepare(context);
  if (!prepared.ok()) {
    return prepared.status();
  }
  if (!IsValidTranspose(left_transpose) || !IsValidTranspose(right_transpose)) {
    return Status(ErrorCode::kInvalidArgument,
                  "A CUDA Gemm transpose value is invalid");
  }
  Status status =
      ValidateThreeMatrices(prepared->state->device(), left, right, output);
  if (!status.ok()) {
    return status;
  }
  const extent_t rows = left_transpose == DenseBlasTranspose::kNone
                            ? left.rows()
                            : left.columns();
  const extent_t inner = left_transpose == DenseBlasTranspose::kNone
                             ? left.columns()
                             : left.rows();
  const extent_t right_inner = right_transpose == DenseBlasTranspose::kNone
                                   ? right.rows()
                                   : right.columns();
  const extent_t columns = right_transpose == DenseBlasTranspose::kNone
                               ? right.columns()
                               : right.rows();
  if (inner != right_inner || output.rows() != rows ||
      output.columns() != columns) {
    return Status(ErrorCode::kShape, "CUDA Gemm operand shapes do not match");
  }
  if (rows == 0 || columns == 0) {
    return RecordOrDrain(context, *prepared);
  }
  if (alpha == Element{0} || inner == 0) {
    return Finish(context, *prepared,
                  ScaleMatrix(prepared->stream, beta, output));
  }
  cublasOperation_t provider_left = ProviderOperation(left_transpose);
  cublasOperation_t provider_right = ProviderOperation(right_transpose);
  const Element* provider_left_data = left.data();
  const Element* provider_right_data = right.data();
  stride_t provider_left_ld = left.leading_dimension();
  stride_t provider_right_ld = right.leading_dimension();
  extent_t provider_rows = rows;
  extent_t provider_columns = columns;
  if (output.layout() == DenseBlasLayout::kRowMajor) {
    std::swap(provider_left, provider_right);
    std::swap(provider_left_data, provider_right_data);
    std::swap(provider_left_ld, provider_right_ld);
    std::swap(provider_rows, provider_columns);
  }
  return Finish(context, *prepared,
                ProviderGemm(prepared->state->handle(), provider_left,
                             provider_right, provider_rows, provider_columns,
                             inner, alpha, provider_left_data, provider_left_ld,
                             provider_right_data, provider_right_ld, beta,
                             output.data(), output.leading_dimension()),
                "cuBLAS could not enqueue CudaGemm");
}

template <typename Element>
Result<CompletionEvent> StructuredImpl(
    DenseCudaContext& context, DenseBlasSide side, DenseBlasTriangle triangle,
    Element alpha, DenseBlasMatrixView<const Element> structured,
    DenseBlasMatrixView<const Element> other, Element beta,
    DenseBlasMatrixView<Element> output, bool hermitian) {
  auto prepared = Prepare(context);
  if (!prepared.ok()) {
    return prepared.status();
  }
  if (!IsValidSide(side) || !IsValidTriangle(triangle)) {
    return Status(ErrorCode::kInvalidArgument,
                  "A CUDA structured Level 3 flag is invalid");
  }
  Status status = ValidateThreeMatrices(prepared->state->device(), structured,
                                        other, output);
  if (!status.ok()) {
    return status;
  }
  if (structured.rows() != structured.columns() ||
      other.rows() != output.rows() || other.columns() != output.columns()) {
    return Status(ErrorCode::kShape,
                  "CUDA structured Level 3 shapes do not match");
  }
  const extent_t order =
      side == DenseBlasSide::kLeft ? output.rows() : output.columns();
  if (structured.rows() != order) {
    return Status(ErrorCode::kShape,
                  "The CUDA structured matrix order does not match its side");
  }
  if (output.rows() == 0 || output.columns() == 0) {
    return RecordOrDrain(context, *prepared);
  }
  if (alpha == Element{0}) {
    return Finish(context, *prepared,
                  ScaleMatrix(prepared->stream, beta, output));
  }
  const cublasSideMode_t provider_side = ProviderSide(side, output.layout());
  const cublasFillMode_t provider_triangle =
      ProviderTriangle(triangle, output.layout());
  const extent_t provider_rows =
      output.layout() == DenseBlasLayout::kColumnMajor ? output.rows()
                                                       : output.columns();
  const extent_t provider_columns =
      output.layout() == DenseBlasLayout::kColumnMajor ? output.columns()
                                                       : output.rows();
  cublasStatus_t provider_status = CUBLAS_STATUS_NOT_SUPPORTED;
  if constexpr (DenseBlasComplex<Element>) {
    provider_status =
        hermitian ? ProviderHemm(prepared->state->handle(), provider_side,
                                 provider_triangle, provider_rows,
                                 provider_columns, alpha, structured.data(),
                                 structured.leading_dimension(), other.data(),
                                 other.leading_dimension(), beta, output.data(),
                                 output.leading_dimension())
                  : ProviderSymm(prepared->state->handle(), provider_side,
                                 provider_triangle, provider_rows,
                                 provider_columns, alpha, structured.data(),
                                 structured.leading_dimension(), other.data(),
                                 other.leading_dimension(), beta, output.data(),
                                 output.leading_dimension());
  } else {
    provider_status = ProviderSymm(
        prepared->state->handle(), provider_side, provider_triangle,
        provider_rows, provider_columns, alpha, structured.data(),
        structured.leading_dimension(), other.data(), other.leading_dimension(),
        beta, output.data(), output.leading_dimension());
  }
  return Finish(context, *prepared, provider_status,
                "cuBLAS could not enqueue a structured Level 3 multiply");
}

template <typename Element>
Status ValidateRankK(std::int32_t device, DenseBlasTriangle triangle,
                     DenseBlasMatrixView<const Element> left,
                     DenseBlasMatrixView<const Element> right,
                     DenseBlasMatrixView<Element> output) {
  if (!IsValidTriangle(triangle)) {
    return Status(ErrorCode::kInvalidArgument,
                  "A CUDA rank-k triangle value is invalid");
  }
  Status status = ValidateThreeMatrices(device, left, right, output);
  if (!status.ok()) {
    return status;
  }
  if (left.rows() != right.rows() || left.columns() != right.columns() ||
      output.rows() != output.columns()) {
    return Status(ErrorCode::kShape, "CUDA rank-k operand shapes do not match");
  }
  return Status::Ok();
}

template <typename Element>
Result<CompletionEvent> SyrkImpl(DenseCudaContext& context,
                                 DenseBlasTriangle triangle,
                                 DenseBlasTranspose transpose, Element alpha,
                                 DenseBlasMatrixView<const Element> left,
                                 DenseBlasMatrixView<const Element> right,
                                 Element beta,
                                 DenseBlasMatrixView<Element> output,
                                 bool rank_two) {
  auto prepared = Prepare(context);
  if (!prepared.ok()) {
    return prepared.status();
  }
  if (!IsValidTranspose(transpose) ||
      (DenseBlasComplex<Element> &&
       transpose == DenseBlasTranspose::kConjugateTranspose)) {
    return Status(ErrorCode::kInvalidArgument,
                  "A CUDA symmetric rank-k transpose value is invalid");
  }
  Status status =
      ValidateRankK(prepared->state->device(), triangle, left, right, output);
  if (!status.ok()) {
    return status;
  }
  const extent_t order =
      transpose == DenseBlasTranspose::kNone ? left.rows() : left.columns();
  const extent_t inner =
      transpose == DenseBlasTranspose::kNone ? left.columns() : left.rows();
  if (output.rows() != order) {
    return Status(ErrorCode::kShape,
                  "A CUDA symmetric rank-k output has the wrong order");
  }
  if (order == 0) {
    return RecordOrDrain(context, *prepared);
  }
  if (alpha == Element{0} || inner == 0) {
    return Finish(
        context, *prepared,
        ScaleTriangle(prepared->stream, beta, output, triangle, false));
  }
  cublasOperation_t operation = ProviderOperation(transpose);
  if (output.layout() == DenseBlasLayout::kRowMajor) {
    operation =
        transpose == DenseBlasTranspose::kNone ? CUBLAS_OP_T : CUBLAS_OP_N;
  }
  const cublasFillMode_t provider_triangle =
      ProviderTriangle(triangle, output.layout());
  const cublasStatus_t provider_status =
      rank_two ? ProviderSyr2k(prepared->state->handle(), provider_triangle,
                               operation, order, inner, alpha, left.data(),
                               left.leading_dimension(), right.data(),
                               right.leading_dimension(), beta, output.data(),
                               output.leading_dimension())
               : ProviderSyrk(prepared->state->handle(), provider_triangle,
                              operation, order, inner, alpha, left.data(),
                              left.leading_dimension(), beta, output.data(),
                              output.leading_dimension());
  return Finish(context, *prepared, provider_status,
                "cuBLAS could not enqueue a symmetric rank-k call");
}

template <DenseBlasComplex Element>
Result<CompletionEvent> HerkImpl(DenseCudaContext& context,
                                 DenseBlasTriangle triangle,
                                 DenseBlasTranspose transpose, Element alpha,
                                 DenseBlasMatrixView<const Element> left,
                                 DenseBlasMatrixView<const Element> right,
                                 DenseBlasRealType<Element> beta,
                                 DenseBlasMatrixView<Element> output,
                                 bool rank_two) {
  auto prepared = Prepare(context);
  if (!prepared.ok()) {
    return prepared.status();
  }
  if (transpose != DenseBlasTranspose::kNone &&
      transpose != DenseBlasTranspose::kConjugateTranspose) {
    return Status(ErrorCode::kInvalidArgument,
                  "A CUDA Hermitian rank-k transpose value is invalid");
  }
  Status status =
      ValidateRankK(prepared->state->device(), triangle, left, right, output);
  if (!status.ok()) {
    return status;
  }
  const extent_t order =
      transpose == DenseBlasTranspose::kNone ? left.rows() : left.columns();
  const extent_t inner =
      transpose == DenseBlasTranspose::kNone ? left.columns() : left.rows();
  if (output.rows() != order) {
    return Status(ErrorCode::kShape,
                  "A CUDA Hermitian rank-k output has the wrong order");
  }
  if (order == 0) {
    return RecordOrDrain(context, *prepared);
  }
  if (alpha == Element{0} || inner == 0) {
    return Finish(context, *prepared,
                  ScaleTriangle(prepared->stream, Element{beta, 0}, output,
                                triangle, true));
  }
  cublasOperation_t operation = ProviderOperation(transpose);
  const Element* provider_left = left.data();
  const Element* provider_right = right.data();
  stride_t provider_left_ld = left.leading_dimension();
  stride_t provider_right_ld = right.leading_dimension();
  if (output.layout() == DenseBlasLayout::kRowMajor) {
    operation =
        transpose == DenseBlasTranspose::kNone ? CUBLAS_OP_C : CUBLAS_OP_N;
    if (rank_two) {
      std::swap(provider_left, provider_right);
      std::swap(provider_left_ld, provider_right_ld);
    }
  }
  const cublasFillMode_t provider_triangle =
      ProviderTriangle(triangle, output.layout());
  cublasStatus_t provider_status = CUBLAS_STATUS_NOT_SUPPORTED;
  if (rank_two) {
    provider_status = ProviderHer2k(
        prepared->state->handle(), provider_triangle, operation, order, inner,
        alpha, provider_left, provider_left_ld, provider_right,
        provider_right_ld, beta, output.data(), output.leading_dimension());
  } else {
    provider_status = ProviderHerk(
        prepared->state->handle(), provider_triangle, operation, order, inner,
        static_cast<DenseBlasRealType<Element>>(alpha.real()), provider_left,
        provider_left_ld, beta, output.data(), output.leading_dimension());
  }
  return Finish(context, *prepared, provider_status,
                "cuBLAS could not enqueue a Hermitian rank-k call");
}

template <typename Element>
Result<CompletionEvent> TriangularImpl(
    DenseCudaContext& context, DenseBlasSide side, DenseBlasTriangle triangle,
    DenseBlasTranspose transpose, DenseBlasDiagonal diagonal, Element alpha,
    DenseBlasMatrixView<const Element> triangular,
    DenseBlasMatrixView<Element> matrix, bool solve) {
  auto prepared = Prepare(context);
  if (!prepared.ok()) {
    return prepared.status();
  }
  if (!IsValidSide(side) || !IsValidTriangle(triangle) ||
      !IsValidTranspose(transpose) || !IsValidDiagonal(diagonal)) {
    return Status(ErrorCode::kInvalidArgument,
                  "A CUDA triangular Level 3 flag is invalid");
  }
  for (Status status : {ValidateDevice(triangular, prepared->state->device()),
                        ValidateDevice(matrix, prepared->state->device())}) {
    if (!status.ok()) {
      return status;
    }
  }
  if (!SameLayout(triangular, matrix)) {
    return Status(ErrorCode::kInvalidArgument,
                  "CUDA triangular operands must use one common layout");
  }
  if (triangular.rows() != triangular.columns()) {
    return Status(ErrorCode::kShape, "A CUDA triangular matrix must be square");
  }
  const extent_t order =
      side == DenseBlasSide::kLeft ? matrix.rows() : matrix.columns();
  if (triangular.rows() != order) {
    return Status(ErrorCode::kShape,
                  "The CUDA triangular order does not match its side");
  }
  if (Overlap(triangular.reachable_storage(), matrix.reachable_storage())) {
    return Status(ErrorCode::kInvalidArgument,
                  "A CUDA triangular input overlaps its mutable matrix");
  }
  if (matrix.rows() == 0 || matrix.columns() == 0) {
    return RecordOrDrain(context, *prepared);
  }
  if (alpha == Element{0}) {
    return Finish(context, *prepared,
                  ScaleMatrix(prepared->stream, Element{0}, matrix));
  }
  const cublasSideMode_t provider_side = ProviderSide(side, matrix.layout());
  const cublasFillMode_t provider_triangle =
      ProviderTriangle(triangle, matrix.layout());
  const extent_t provider_rows =
      matrix.layout() == DenseBlasLayout::kColumnMajor ? matrix.rows()
                                                       : matrix.columns();
  const extent_t provider_columns =
      matrix.layout() == DenseBlasLayout::kColumnMajor ? matrix.columns()
                                                       : matrix.rows();
  const cublasStatus_t provider_status =
      solve ? ProviderTrsm(prepared->state->handle(), provider_side,
                           provider_triangle, ProviderOperation(transpose),
                           ProviderDiagonal(diagonal), provider_rows,
                           provider_columns, alpha, triangular.data(),
                           triangular.leading_dimension(), matrix.data(),
                           matrix.leading_dimension())
            : ProviderTrmm(prepared->state->handle(), provider_side,
                           provider_triangle, ProviderOperation(transpose),
                           ProviderDiagonal(diagonal), provider_rows,
                           provider_columns, alpha, triangular.data(),
                           triangular.leading_dimension(), matrix.data(),
                           matrix.leading_dimension());
  return Finish(context, *prepared, provider_status,
                "cuBLAS could not enqueue a triangular Level 3 call");
}

}  // namespace
}  // namespace asc::internal_dense_cuda

namespace asc {

template <DenseBlasScalar Element>
Result<CompletionEvent> CudaGemm(DenseCudaContext& context,
                                 DenseBlasTranspose left_transpose,
                                 DenseBlasTranspose right_transpose,
                                 Element alpha,
                                 DenseBlasMatrixView<const Element> left,
                                 DenseBlasMatrixView<const Element> right,
                                 Element beta,
                                 DenseBlasMatrixView<Element> output) {
  return internal_dense_cuda::GemmImpl(context, left_transpose, right_transpose,
                                       alpha, left, right, beta, output);
}

template <DenseBlasScalar Element>
Result<CompletionEvent> CudaSymm(DenseCudaContext& context, DenseBlasSide side,
                                 DenseBlasTriangle triangle, Element alpha,
                                 DenseBlasMatrixView<const Element> symmetric,
                                 DenseBlasMatrixView<const Element> other,
                                 Element beta,
                                 DenseBlasMatrixView<Element> output) {
  return internal_dense_cuda::StructuredImpl(
      context, side, triangle, alpha, symmetric, other, beta, output, false);
}

template <DenseBlasComplex Element>
Result<CompletionEvent> CudaHemm(DenseCudaContext& context, DenseBlasSide side,
                                 DenseBlasTriangle triangle, Element alpha,
                                 DenseBlasMatrixView<const Element> hermitian,
                                 DenseBlasMatrixView<const Element> other,
                                 Element beta,
                                 DenseBlasMatrixView<Element> output) {
  return internal_dense_cuda::StructuredImpl(
      context, side, triangle, alpha, hermitian, other, beta, output, true);
}

template <DenseBlasScalar Element>
Result<CompletionEvent> CudaSyrk(DenseCudaContext& context,
                                 DenseBlasTriangle triangle,
                                 DenseBlasTranspose transpose, Element alpha,
                                 DenseBlasMatrixView<const Element> input,
                                 Element beta,
                                 DenseBlasMatrixView<Element> output) {
  return internal_dense_cuda::SyrkImpl(context, triangle, transpose, alpha,
                                       input, input, beta, output, false);
}

template <DenseBlasComplex Element>
Result<CompletionEvent> CudaHerk(DenseCudaContext& context,
                                 DenseBlasTriangle triangle,
                                 DenseBlasTranspose transpose,
                                 DenseBlasRealType<Element> alpha,
                                 DenseBlasMatrixView<const Element> input,
                                 DenseBlasRealType<Element> beta,
                                 DenseBlasMatrixView<Element> output) {
  return internal_dense_cuda::HerkImpl(context, triangle, transpose,
                                       Element{alpha, 0}, input, input, beta,
                                       output, false);
}

template <DenseBlasScalar Element>
Result<CompletionEvent> CudaSyr2k(DenseCudaContext& context,
                                  DenseBlasTriangle triangle,
                                  DenseBlasTranspose transpose, Element alpha,
                                  DenseBlasMatrixView<const Element> left,
                                  DenseBlasMatrixView<const Element> right,
                                  Element beta,
                                  DenseBlasMatrixView<Element> output) {
  return internal_dense_cuda::SyrkImpl(context, triangle, transpose, alpha,
                                       left, right, beta, output, true);
}

template <DenseBlasComplex Element>
Result<CompletionEvent> CudaHer2k(DenseCudaContext& context,
                                  DenseBlasTriangle triangle,
                                  DenseBlasTranspose transpose, Element alpha,
                                  DenseBlasMatrixView<const Element> left,
                                  DenseBlasMatrixView<const Element> right,
                                  DenseBlasRealType<Element> beta,
                                  DenseBlasMatrixView<Element> output) {
  return internal_dense_cuda::HerkImpl(context, triangle, transpose, alpha,
                                       left, right, beta, output, true);
}

template <DenseBlasScalar Element>
Result<CompletionEvent> CudaTrmm(DenseCudaContext& context, DenseBlasSide side,
                                 DenseBlasTriangle triangle,
                                 DenseBlasTranspose transpose,
                                 DenseBlasDiagonal diagonal, Element alpha,
                                 DenseBlasMatrixView<const Element> triangular,
                                 DenseBlasMatrixView<Element> matrix) {
  return internal_dense_cuda::TriangularImpl(context, side, triangle, transpose,
                                             diagonal, alpha, triangular,
                                             matrix, false);
}

template <DenseBlasScalar Element>
Result<CompletionEvent> CudaTrsm(DenseCudaContext& context, DenseBlasSide side,
                                 DenseBlasTriangle triangle,
                                 DenseBlasTranspose transpose,
                                 DenseBlasDiagonal diagonal, Element alpha,
                                 DenseBlasMatrixView<const Element> triangular,
                                 DenseBlasMatrixView<Element> matrix) {
  return internal_dense_cuda::TriangularImpl(context, side, triangle, transpose,
                                             diagonal, alpha, triangular,
                                             matrix, true);
}

#define ASC_INSTANTIATE_CUDA_LEVEL3(Type)                                      \
  template Result<CompletionEvent> CudaGemm<Type>(                             \
      DenseCudaContext&, DenseBlasTranspose, DenseBlasTranspose, Type,         \
      DenseBlasMatrixView<const Type>, DenseBlasMatrixView<const Type>, Type,  \
      DenseBlasMatrixView<Type>);                                              \
  template Result<CompletionEvent> CudaSymm<Type>(                             \
      DenseCudaContext&, DenseBlasSide, DenseBlasTriangle, Type,               \
      DenseBlasMatrixView<const Type>, DenseBlasMatrixView<const Type>, Type,  \
      DenseBlasMatrixView<Type>);                                              \
  template Result<CompletionEvent> CudaSyrk<Type>(                             \
      DenseCudaContext&, DenseBlasTriangle, DenseBlasTranspose, Type,          \
      DenseBlasMatrixView<const Type>, Type, DenseBlasMatrixView<Type>);       \
  template Result<CompletionEvent> CudaSyr2k<Type>(                            \
      DenseCudaContext&, DenseBlasTriangle, DenseBlasTranspose, Type,          \
      DenseBlasMatrixView<const Type>, DenseBlasMatrixView<const Type>, Type,  \
      DenseBlasMatrixView<Type>);                                              \
  template Result<CompletionEvent> CudaTrmm<Type>(                             \
      DenseCudaContext&, DenseBlasSide, DenseBlasTriangle, DenseBlasTranspose, \
      DenseBlasDiagonal, Type, DenseBlasMatrixView<const Type>,                \
      DenseBlasMatrixView<Type>);                                              \
  template Result<CompletionEvent> CudaTrsm<Type>(                             \
      DenseCudaContext&, DenseBlasSide, DenseBlasTriangle, DenseBlasTranspose, \
      DenseBlasDiagonal, Type, DenseBlasMatrixView<const Type>,                \
      DenseBlasMatrixView<Type>)

#define ASC_INSTANTIATE_CUDA_LEVEL3_COMPLEX(Type, Real)                       \
  ASC_INSTANTIATE_CUDA_LEVEL3(Type);                                          \
  template Result<CompletionEvent> CudaHemm<Type>(                            \
      DenseCudaContext&, DenseBlasSide, DenseBlasTriangle, Type,              \
      DenseBlasMatrixView<const Type>, DenseBlasMatrixView<const Type>, Type, \
      DenseBlasMatrixView<Type>);                                             \
  template Result<CompletionEvent> CudaHerk<Type>(                            \
      DenseCudaContext&, DenseBlasTriangle, DenseBlasTranspose, Real,         \
      DenseBlasMatrixView<const Type>, Real, DenseBlasMatrixView<Type>);      \
  template Result<CompletionEvent> CudaHer2k<Type>(                           \
      DenseCudaContext&, DenseBlasTriangle, DenseBlasTranspose, Type,         \
      DenseBlasMatrixView<const Type>, DenseBlasMatrixView<const Type>, Real, \
      DenseBlasMatrixView<Type>)

ASC_INSTANTIATE_CUDA_LEVEL3(float);
ASC_INSTANTIATE_CUDA_LEVEL3(double);
ASC_INSTANTIATE_CUDA_LEVEL3_COMPLEX(std::complex<float>, float);
ASC_INSTANTIATE_CUDA_LEVEL3_COMPLEX(std::complex<double>, double);

#undef ASC_INSTANTIATE_CUDA_LEVEL3
#undef ASC_INSTANTIATE_CUDA_LEVEL3_COMPLEX

}  // namespace asc
