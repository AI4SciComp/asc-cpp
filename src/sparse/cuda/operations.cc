#include <cuda_runtime_api.h>
#include <cusparse.h>

#include <algorithm>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <utility>

#include "../../core/cuda/cuda_internal.h"
#include "../../core/execution_internal.h"
#include "asc/core/contracts.h"
#include "asc/core/providers/cuda.h"
#include "asc/sparse/providers/cuda.h"
#include "context_internal.h"
#include "kernels_internal.h"

namespace asc::internal_sparse_cuda {
namespace {

std::size_t ElementBytes(ElementKind kind) {
  switch (kind) {
    case ElementKind::kFloat:
      return sizeof(float);
    case ElementKind::kDouble:
      return sizeof(double);
    case ElementKind::kComplexFloat:
      return sizeof(std::complex<float>);
    case ElementKind::kComplexDouble:
      return sizeof(std::complex<double>);
  }
  return 0;
}

std::size_t ElementAlignment(ElementKind kind) {
  switch (kind) {
    case ElementKind::kFloat:
      return alignof(float);
    case ElementKind::kDouble:
      return alignof(double);
    case ElementKind::kComplexFloat:
      return alignof(std::complex<float>);
    case ElementKind::kComplexDouble:
      return alignof(std::complex<double>);
  }
  return 1;
}

Result<void*> ValidateContext(SparseCudaContext& context) {
  ContextState* state = Access::State(context);
  if (state == nullptr) {
    return Status(ErrorCode::kInvalidState,
                  "A moved-from SparseCudaContext cannot launch work");
  }
  const ExecutionContext& execution = context.execution_context();
  if (execution.backend() != Backend::kCuda ||
      execution.device().ordinal != state->device()) {
    return Status(ErrorCode::kInvalidState,
                  "Sparse CUDA provider state does not match its context");
  }
  const auto& execution_state =
      internal_core_execution::Access::State(execution);
  if (execution_state == nullptr) {
    return Status(ErrorCode::kInvalidState,
                  "The sparse CUDA execution context has no provider state");
  }
  void* stream = execution_state->NativeExecutionHandle(Backend::kCuda);
  if (stream == nullptr) {
    return Status(ErrorCode::kUnavailable,
                  "The sparse CUDA execution context has no stream");
  }
  return stream;
}

Result<std::size_t> CheckedBytes(std::uint64_t count,
                                 std::size_t element_size) {
  auto bytes = CheckedMultiply<std::uint64_t>(
      count, static_cast<std::uint64_t>(element_size));
  if (!bytes.ok()) {
    return bytes.status();
  }
  return CheckedCast<std::size_t>(*bytes);
}

Status ValidateAddress(const void* pointer, std::size_t bytes,
                       std::size_t alignment, const char* null_message,
                       const char* alignment_message) {
  if (bytes == 0) {
    return Status::Ok();
  }
  if (pointer == nullptr) {
    return Status(ErrorCode::kInvalidArgument, null_message);
  }
  const auto address = reinterpret_cast<std::uintptr_t>(pointer);
  if (address % alignment != 0) {
    return Status(ErrorCode::kMemoryAccess, alignment_message);
  }
  if (bytes > UINTPTR_MAX - address) {
    return Status(ErrorCode::kOverflow,
                  "A sparse CUDA operand address span overflows");
  }
  return Status::Ok();
}

Status ValidateDevicePointer(const void* pointer, std::size_t bytes,
                             std::int32_t device, const char* message) {
  if (bytes == 0) {
    return Status::Ok();
  }
  cudaPointerAttributes attributes{};
  const cudaError_t error = cudaPointerGetAttributes(&attributes, pointer);
  if (error != cudaSuccess) {
    return internal_core_cuda::CudaStatus(error, ErrorCode::kMemoryAccess,
                                          message);
  }
  if (attributes.type != cudaMemoryTypeDevice || attributes.device != device) {
    return Status(ErrorCode::kMemoryAccess,
                  "A sparse CUDA operand is not on the context device");
  }
  return Status::Ok();
}

struct SparseBytes {
  std::size_t first = 0;
  std::size_t second = 0;
  std::size_t values = 0;
};

bool Overlap(const void* left, std::size_t left_bytes, const void* right,
             std::size_t right_bytes);

Result<SparseBytes> ValidateSparseMetadata(const SparseDescriptor& descriptor) {
  if (descriptor.rank != 0 && descriptor.extents == nullptr) {
    return Status(ErrorCode::kInvalidArgument,
                  "Sparse CUDA operand extents are missing");
  }
  bool empty = false;
  extent_t logical_size = 1;
  for (std::size_t dimension = 0; dimension < descriptor.rank; ++dimension) {
    if (descriptor.extents[dimension] < 0) {
      return Status(ErrorCode::kShape,
                    "A sparse CUDA extent cannot be negative");
    }
    empty = empty || descriptor.extents[dimension] == 0;
    if (!empty) {
      auto product =
          CheckedMultiply(logical_size, descriptor.extents[dimension]);
      if (!product.ok()) {
        return product.status();
      }
      logical_size = *product;
    }
  }
  if (empty) {
    logical_size = 0;
  }
  if (descriptor.nonzeros < 0 || descriptor.nonzeros > logical_size) {
    return Status(ErrorCode::kShape,
                  "Sparse CUDA NNZ is incompatible with its shape");
  }
  if (!descriptor.canonical_structure_trusted) {
    return Status(ErrorCode::kInvalidArgument,
                  "Sparse CUDA operations require trusted canonical structure");
  }

  auto nonzeros = CheckedCast<std::uint64_t>(descriptor.nonzeros);
  if (!nonzeros.ok()) {
    return nonzeros.status();
  }
  SparseBytes bytes;
  auto value_bytes =
      CheckedBytes(*nonzeros, ElementBytes(descriptor.element_kind));
  if (!value_bytes.ok()) {
    return value_bytes.status();
  }
  bytes.values = *value_bytes;
  if (descriptor.format == FormatKind::kCoordinate) {
    auto coordinate_count =
        CheckedMultiply<std::uint64_t>(*nonzeros, descriptor.rank);
    if (!coordinate_count.ok()) {
      return coordinate_count.status();
    }
    auto coordinate_bytes = CheckedBytes(*coordinate_count, sizeof(index_t));
    if (!coordinate_bytes.ok()) {
      return coordinate_bytes.status();
    }
    bytes.first = *coordinate_bytes;
  } else {
    if (descriptor.rank != 2) {
      return Status(ErrorCode::kShape,
                    "Compressed sparse CUDA operands require rank two");
    }
    const extent_t outer_extent = descriptor.format == FormatKind::kCsr
                                      ? descriptor.extents[0]
                                      : descriptor.extents[1];
    auto offset_count = CheckedAdd(outer_extent, extent_t{1});
    if (!offset_count.ok()) {
      return offset_count.status();
    }
    auto converted = CheckedCast<std::uint64_t>(*offset_count);
    if (!converted.ok()) {
      return converted.status();
    }
    auto offset_bytes = CheckedBytes(*converted, sizeof(nnz_t));
    auto index_bytes = CheckedBytes(*nonzeros, sizeof(index_t));
    if (!offset_bytes.ok()) {
      return offset_bytes.status();
    }
    if (!index_bytes.ok()) {
      return index_bytes.status();
    }
    bytes.first = *offset_bytes;
    bytes.second = *index_bytes;
  }

  Status first = ValidateAddress(descriptor.structure_first, bytes.first,
                                 descriptor.format == FormatKind::kCoordinate
                                     ? alignof(index_t)
                                     : alignof(nnz_t),
                                 "Sparse CUDA structure storage is incomplete",
                                 "Sparse CUDA structure storage is misaligned");
  if (!first.ok()) {
    return first;
  }
  Status second = ValidateAddress(descriptor.structure_second, bytes.second,
                                  alignof(index_t),
                                  "Sparse CUDA index storage is incomplete",
                                  "Sparse CUDA index storage is misaligned");
  if (!second.ok()) {
    return second;
  }
  Status values = ValidateAddress(descriptor.values, bytes.values,
                                  ElementAlignment(descriptor.element_kind),
                                  "Sparse CUDA value storage is incomplete",
                                  "Sparse CUDA value storage is misaligned");
  if (!values.ok()) {
    return values;
  }
  if (Overlap(descriptor.structure_first, bytes.first,
              descriptor.structure_second, bytes.second) ||
      Overlap(descriptor.structure_first, bytes.first, descriptor.values,
              bytes.values) ||
      Overlap(descriptor.structure_second, bytes.second, descriptor.values,
              bytes.values)) {
    return Status(ErrorCode::kInvalidArgument,
                  "Sparse CUDA descriptor storage spans cannot overlap");
  }
  return bytes;
}

Status ValidateSparseDevice(const SparseDescriptor& descriptor,
                            std::int32_t device) {
  if (descriptor.memory_space != MemorySpace::kDevice) {
    return Status(ErrorCode::kMemoryAccess,
                  "Sparse CUDA operations require device storage");
  }
  auto bytes = ValidateSparseMetadata(descriptor);
  if (!bytes.ok()) {
    return bytes.status();
  }
  for (const auto& span :
       {std::pair{descriptor.structure_first, bytes->first},
        std::pair{descriptor.structure_second, bytes->second},
        std::pair{descriptor.values, bytes->values}}) {
    Status pointer =
        ValidateDevicePointer(span.first, span.second, device,
                              "CUDA could not inspect sparse operand storage");
    if (!pointer.ok()) {
      return pointer;
    }
  }
  return Status::Ok();
}

Result<std::size_t> VectorBytes(const VectorDescriptor& vector) {
  if (vector.extent < 0 || vector.stride == 0) {
    return Status(ErrorCode::kInvalidArgument,
                  "Sparse CUDA vector metadata is invalid");
  }
  if (vector.extent == 0) {
    return std::size_t{0};
  }
  if (vector.stride == std::numeric_limits<stride_t>::min()) {
    return Status(ErrorCode::kOverflow,
                  "Sparse CUDA vector stride magnitude overflows");
  }
  const stride_t magnitude = vector.stride < 0 ? -vector.stride : vector.stride;
  auto last = CheckedMultiply<stride_t>(vector.extent - 1, magnitude);
  if (!last.ok()) {
    return last.status();
  }
  auto span = CheckedAdd(*last, stride_t{1});
  if (!span.ok()) {
    return span.status();
  }
  auto count = CheckedCast<std::uint64_t>(*span);
  if (!count.ok()) {
    return count.status();
  }
  return CheckedBytes(*count, ElementBytes(vector.element_kind));
}

Status ValidateVector(const VectorDescriptor& vector, ElementKind element_kind,
                      bool writable, std::int32_t device) {
  if (vector.element_kind != element_kind || vector.writable != writable) {
    return Status(ErrorCode::kInvalidArgument,
                  "Sparse CUDA vector type or mutability is incorrect");
  }
  if (vector.memory_space != MemorySpace::kDevice) {
    return Status(ErrorCode::kMemoryAccess,
                  "Sparse CUDA vectors require device storage");
  }
  auto bytes = VectorBytes(vector);
  if (!bytes.ok()) {
    return bytes.status();
  }
  Status address = ValidateAddress(
      vector.reachable_data == nullptr ? vector.data : vector.reachable_data,
      vector.reachable_size == 0 ? *bytes : vector.reachable_size,
      ElementAlignment(vector.element_kind),
      "A nonempty sparse CUDA vector cannot be null",
      "A sparse CUDA vector is misaligned");
  if (!address.ok()) {
    return address;
  }
  return ValidateDevicePointer(
      vector.reachable_data == nullptr ? vector.data : vector.reachable_data,
      vector.reachable_size == 0 ? *bytes : vector.reachable_size, device,
      "CUDA could not inspect a sparse vector");
}

bool Overlap(const void* left, std::size_t left_bytes, const void* right,
             std::size_t right_bytes) {
  if (left_bytes == 0 || right_bytes == 0) {
    return false;
  }
  const auto left_begin = reinterpret_cast<std::uintptr_t>(left);
  const auto right_begin = reinterpret_cast<std::uintptr_t>(right);
  if (left_bytes > UINTPTR_MAX - left_begin ||
      right_bytes > UINTPTR_MAX - right_begin) {
    return true;
  }
  return left_begin < right_begin + right_bytes &&
         right_begin < left_begin + left_bytes;
}

Status ValidateSpmvOperands(const SparseDescriptor& matrix,
                            const VectorDescriptor& input,
                            const VectorDescriptor& output,
                            std::int32_t device) {
  if (matrix.format != FormatKind::kCsr) {
    return Status(ErrorCode::kInvalidArgument,
                  "CUDA CSR SpMV requires CSR format");
  }
  Status matrix_status = ValidateSparseDevice(matrix, device);
  if (!matrix_status.ok()) {
    return matrix_status;
  }
  Status input_status =
      ValidateVector(input, matrix.element_kind, false, device);
  if (!input_status.ok()) {
    return input_status;
  }
  Status output_status =
      ValidateVector(output, matrix.element_kind, true, device);
  if (!output_status.ok()) {
    return output_status;
  }
  if (input.extent != matrix.extents[1] || output.extent != matrix.extents[0]) {
    return Status(ErrorCode::kShape,
                  "CUDA CSR SpMV vector lengths do not match the matrix");
  }
  auto matrix_bytes = ValidateSparseMetadata(matrix);
  auto input_bytes = VectorBytes(input);
  auto output_bytes = VectorBytes(output);
  if (!matrix_bytes.ok()) {
    return matrix_bytes.status();
  }
  if (!input_bytes.ok()) {
    return input_bytes.status();
  }
  if (!output_bytes.ok()) {
    return output_bytes.status();
  }
  if (Overlap(input.data, *input_bytes, output.data, *output_bytes) ||
      Overlap(matrix.structure_first, matrix_bytes->first, output.data,
              *output_bytes) ||
      Overlap(matrix.structure_second, matrix_bytes->second, output.data,
              *output_bytes) ||
      Overlap(matrix.values, matrix_bytes->values, output.data,
              *output_bytes)) {
    return Status(ErrorCode::kInvalidArgument,
                  "CUDA CSR SpMV rejects output operand overlap");
  }
  return Status::Ok();
}

cudaDataType DataType(ElementKind kind) {
  switch (kind) {
    case ElementKind::kFloat:
      return CUDA_R_32F;
    case ElementKind::kDouble:
      return CUDA_R_64F;
    case ElementKind::kComplexFloat:
      return CUDA_C_32F;
    case ElementKind::kComplexDouble:
      return CUDA_C_64F;
  }
  return CUDA_R_32F;
}

class SpmvDescriptors {
 public:
  ~SpmvDescriptors() {
    if (input_ != nullptr) {
      static_cast<void>(cusparseDestroyDnVec(input_));
    }
    if (output_ != nullptr) {
      static_cast<void>(cusparseDestroyDnVec(output_));
    }
    if (matrix_ != nullptr) {
      static_cast<void>(cusparseDestroySpMat(matrix_));
    }
  }

  cusparseSpMatDescr_t matrix_ = nullptr;
  cusparseDnVecDescr_t input_ = nullptr;
  cusparseDnVecDescr_t output_ = nullptr;
};

Status CreateSpmvDescriptors(const SparseDescriptor& matrix,
                             const VectorDescriptor& input,
                             const VectorDescriptor& output,
                             SpmvDescriptors& descriptors) {
  const cudaDataType data_type = DataType(matrix.element_kind);
  cusparseStatus_t status = cusparseCreateCsr(
      &descriptors.matrix_, matrix.extents[0], matrix.extents[1],
      matrix.nonzeros, const_cast<void*>(matrix.structure_first),
      const_cast<void*>(matrix.structure_second),
      const_cast<void*>(matrix.values), CUSPARSE_INDEX_64I, CUSPARSE_INDEX_64I,
      CUSPARSE_INDEX_BASE_ZERO, data_type);
  if (status != CUSPARSE_STATUS_SUCCESS) {
    return CusparseStatus(status, "cuSPARSE could not create a CSR descriptor");
  }
  status = cusparseCreateDnVec(&descriptors.input_, input.extent,
                               const_cast<void*>(input.data), data_type);
  if (status != CUSPARSE_STATUS_SUCCESS) {
    return CusparseStatus(
        status, "cuSPARSE could not create the input vector descriptor");
  }
  status = cusparseCreateDnVec(&descriptors.output_, output.extent,
                               const_cast<void*>(output.data), data_type);
  if (status != CUSPARSE_STATUS_SUCCESS) {
    return CusparseStatus(
        status, "cuSPARSE could not create the output vector descriptor");
  }
  return Status::Ok();
}

Result<std::size_t> QuerySpmvWorkspace(ContextState& state,
                                       const SparseDescriptor& matrix,
                                       const VectorDescriptor& input,
                                       const VectorDescriptor& output) {
  SpmvDescriptors descriptors;
  Status descriptor_status =
      CreateSpmvDescriptors(matrix, input, output, descriptors);
  if (!descriptor_status.ok()) {
    return descriptor_status;
  }
  float alpha_float = 1.0F;
  float beta_float = 0.0F;
  double alpha_double = 1.0;
  double beta_double = 0.0;
  const void* alpha = matrix.element_kind == ElementKind::kFloat
                          ? static_cast<const void*>(&alpha_float)
                          : static_cast<const void*>(&alpha_double);
  const void* beta = matrix.element_kind == ElementKind::kFloat
                         ? static_cast<const void*>(&beta_float)
                         : static_cast<const void*>(&beta_double);
  std::size_t workspace_size = 0;
  const cusparseStatus_t status = cusparseSpMV_bufferSize(
      state.handle(), CUSPARSE_OPERATION_NON_TRANSPOSE, alpha,
      descriptors.matrix_, descriptors.input_, beta, descriptors.output_,
      DataType(matrix.element_kind), CUSPARSE_SPMV_CSR_ALG2, &workspace_size);
  if (status != CUSPARSE_STATUS_SUCCESS) {
    return CusparseStatus(status,
                          "cuSPARSE could not query CSR SpMV workspace");
  }
  return workspace_size;
}

Status SameStructure(const SparseDescriptor& left,
                     const SparseDescriptor& right) {
  if (left.format != right.format || left.rank != right.rank ||
      left.nonzeros != right.nonzeros ||
      left.structure_first != right.structure_first ||
      left.structure_second != right.structure_second) {
    return Status(ErrorCode::kInvalidArgument,
                  "CUDA sparse evaluation requires identical structure");
  }
  for (std::size_t dimension = 0; dimension < left.rank; ++dimension) {
    if (left.extents[dimension] != right.extents[dimension]) {
      return Status(ErrorCode::kShape,
                    "CUDA sparse evaluation shapes do not match");
    }
  }
  return Status::Ok();
}

Status ValidateEvaluationOperand(const OperandDescriptor& operand,
                                 const SparseDescriptor& destination,
                                 std::int32_t device) {
  if (operand.kind == OperandKind::kScalar) {
    if (operand.view.element_kind != destination.element_kind) {
      return Status(ErrorCode::kInvalidArgument,
                    "CUDA sparse scalar type does not match destination");
    }
    return Status::Ok();
  }
  if (operand.kind != OperandKind::kView) {
    return Status(ErrorCode::kInvalidArgument,
                  "CUDA sparse operand kind is invalid");
  }
  if (operand.view.element_kind != destination.element_kind) {
    return Status(ErrorCode::kInvalidArgument,
                  "CUDA sparse value types do not match");
  }
  Status device_status = ValidateSparseDevice(operand.view, device);
  if (!device_status.ok()) {
    return device_status;
  }
  return SameStructure(operand.view, destination);
}

struct IndexedBytes {
  std::size_t indices = 0;
  std::size_t values = 0;
};

Result<IndexedBytes> ValidateIndexedMetadata(
    const IndexedVectorDescriptor& sparse) {
  if (sparse.nonzeros < 0 || sparse.dense_extent < 0 ||
      sparse.nonzeros > sparse.dense_extent ||
      !sparse.canonical_structure_trusted) {
    return Status(ErrorCode::kInvalidArgument,
                  "CUDA Sparse BLAS requires a canonical indexed vector");
  }
  auto count = CheckedCast<std::uint64_t>(sparse.nonzeros);
  if (!count.ok()) {
    return count.status();
  }
  auto index_bytes = CheckedBytes(*count, sizeof(index_t));
  auto value_bytes = CheckedBytes(*count, ElementBytes(sparse.element_kind));
  if (!index_bytes.ok()) {
    return index_bytes.status();
  }
  if (!value_bytes.ok()) {
    return value_bytes.status();
  }
  Status indices =
      ValidateAddress(sparse.indices, *index_bytes, alignof(index_t),
                      "A nonempty CUDA indexed vector requires indices",
                      "CUDA indexed-vector indices are misaligned");
  if (!indices.ok()) {
    return indices;
  }
  Status values = ValidateAddress(
      sparse.values, *value_bytes, ElementAlignment(sparse.element_kind),
      "A nonempty CUDA indexed vector requires values",
      "CUDA indexed-vector values are misaligned");
  if (!values.ok()) {
    return values;
  }
  if (Overlap(sparse.indices, *index_bytes, sparse.values, *value_bytes)) {
    return Status(ErrorCode::kInvalidArgument,
                  "CUDA indexed-vector indices and values overlap");
  }
  return IndexedBytes{*index_bytes, *value_bytes};
}

Result<IndexedBytes> ValidateIndexedDevice(
    const IndexedVectorDescriptor& sparse, bool writable, std::int32_t device) {
  if (sparse.memory_space != MemorySpace::kDevice ||
      sparse.writable != writable) {
    return Status(ErrorCode::kMemoryAccess,
                  "CUDA indexed-vector memory or mutability is invalid");
  }
  auto bytes = ValidateIndexedMetadata(sparse);
  if (!bytes.ok()) {
    return bytes.status();
  }
  Status indices =
      ValidateDevicePointer(sparse.indices, bytes->indices, device,
                            "CUDA could not inspect indexed-vector indices");
  if (!indices.ok()) {
    return indices;
  }
  Status values =
      ValidateDevicePointer(sparse.values, bytes->values, device,
                            "CUDA could not inspect indexed-vector values");
  if (!values.ok()) {
    return values;
  }
  return *bytes;
}

Result<std::size_t> ValidateMatrixDevice(const MatrixDescriptor& matrix,
                                         ElementKind kind, bool writable,
                                         std::int32_t device) {
  if (matrix.rows < 0 || matrix.columns < 0 || matrix.element_kind != kind ||
      matrix.writable != writable ||
      matrix.memory_space != MemorySpace::kDevice ||
      (matrix.layout != SparseBlasLayout::kColumnMajor &&
       matrix.layout != SparseBlasLayout::kRowMajor)) {
    return Status(ErrorCode::kInvalidArgument,
                  "A CUDA Sparse BLAS dense-matrix descriptor is invalid");
  }
  const extent_t contiguous = matrix.layout == SparseBlasLayout::kColumnMajor
                                  ? matrix.rows
                                  : matrix.columns;
  const bool vector_like =
      matrix.layout == SparseBlasLayout::kRowMajor && matrix.columns == 1;
  if ((!vector_like &&
       matrix.leading_dimension < std::max<stride_t>(1, contiguous)) ||
      (vector_like && matrix.leading_dimension == 0)) {
    return Status(ErrorCode::kInvalidArgument,
                  "A CUDA Sparse BLAS leading dimension is too small");
  }
  stride_t span = 0;
  if (matrix.rows != 0 && matrix.columns != 0) {
    const extent_t major = matrix.layout == SparseBlasLayout::kColumnMajor
                               ? matrix.columns
                               : matrix.rows;
    auto magnitude =
        matrix.leading_dimension < 0
            ? CheckedMultiply<stride_t>(matrix.leading_dimension, stride_t{-1})
            : Result<stride_t>(matrix.leading_dimension);
    if (!magnitude.ok()) {
      return magnitude.status();
    }
    auto leading = CheckedMultiply<stride_t>(major - 1, *magnitude);
    if (!leading.ok()) {
      return leading.status();
    }
    auto total = CheckedAdd<stride_t>(*leading, contiguous);
    if (!total.ok()) {
      return total.status();
    }
    span = *total;
  }
  auto count = CheckedCast<std::uint64_t>(span);
  if (!count.ok()) {
    return count.status();
  }
  auto bytes = CheckedBytes(*count, ElementBytes(kind));
  if (!bytes.ok()) {
    return bytes.status();
  }
  const void* reachable =
      matrix.reachable_data == nullptr ? matrix.data : matrix.reachable_data;
  const std::size_t reachable_size =
      matrix.reachable_size == 0 ? *bytes : matrix.reachable_size;
  Status address =
      ValidateAddress(reachable, reachable_size, ElementAlignment(kind),
                      "A nonempty CUDA Sparse BLAS matrix cannot be null",
                      "A CUDA Sparse BLAS matrix is misaligned");
  if (!address.ok()) {
    return address;
  }
  Status pointer = ValidateDevicePointer(
      reachable, reachable_size, device,
      "CUDA could not inspect a Sparse BLAS dense matrix");
  if (!pointer.ok()) {
    return pointer;
  }
  return reachable_size;
}

const void* ReachableData(const VectorDescriptor& vector) {
  return vector.reachable_data == nullptr ? vector.data : vector.reachable_data;
}

std::size_t ReachableSize(const VectorDescriptor& vector,
                          std::size_t calculated) {
  return vector.reachable_size == 0 ? calculated : vector.reachable_size;
}

const void* ReachableData(const MatrixDescriptor& matrix) {
  return matrix.reachable_data == nullptr ? matrix.data : matrix.reachable_data;
}

}  // namespace

Result<CloneBuffers> CloneCsrErased(SparseCudaContext& context,
                                    SparseDescriptor source,
                                    MemoryResource& resource) {
  auto stream = ValidateContext(context);
  if (!stream.ok()) {
    return stream.status();
  }
  if (source.format != FormatKind::kCsr ||
      source.memory_space != MemorySpace::kHost) {
    return Status(ErrorCode::kMemoryAccess,
                  "CudaCloneCsr requires canonical host CSR storage");
  }
  auto source_bytes = ValidateSparseMetadata(source);
  if (!source_bytes.ok()) {
    return source_bytes.status();
  }
  if (resource.space() != MemorySpace::kDevice) {
    return Status(ErrorCode::kMemoryAccess,
                  "CudaCloneCsr requires a device memory resource");
  }
  auto outer_offsets =
      Buffer::Allocate(resource, source_bytes->first, alignof(nnz_t));
  if (!outer_offsets.ok()) {
    return outer_offsets.status();
  }
  auto inner_indices =
      Buffer::Allocate(resource, source_bytes->second, alignof(index_t));
  if (!inner_indices.ok()) {
    return inner_indices.status();
  }
  auto values = Buffer::Allocate(resource, source_bytes->values,
                                 ElementAlignment(source.element_kind));
  if (!values.ok()) {
    return values.status();
  }
  auto guard = internal_core_cuda::DeviceGuard::Create(
      context.execution_context().device().ordinal);
  if (!guard.ok()) {
    return guard.status();
  }
  for (const auto& allocation :
       {std::pair{outer_offsets->data(), source_bytes->first},
        std::pair{inner_indices->data(), source_bytes->second},
        std::pair{values->data(), source_bytes->values}}) {
    Status pointer =
        ValidateDevicePointer(allocation.first, allocation.second,
                              context.execution_context().device().ordinal,
                              "CUDA could not inspect a cloned CSR allocation");
    if (!pointer.ok()) {
      return pointer;
    }
  }

  bool enqueued = false;
  const auto enqueue = [&](void* destination, const void* source_pointer,
                           std::size_t bytes) -> Status {
    if (bytes == 0) {
      return Status::Ok();
    }
    const cudaError_t error = cudaMemcpyAsync(
        destination, source_pointer, bytes, cudaMemcpyHostToDevice,
        static_cast<cudaStream_t>(*stream));
    if (error != cudaSuccess) {
      if (enqueued) {
        static_cast<void>(
            cudaStreamSynchronize(static_cast<cudaStream_t>(*stream)));
      }
      return internal_core_cuda::CudaStatus(
          error, ErrorCode::kMemoryTransfer,
          "CUDA could not enqueue a CSR clone copy");
    }
    enqueued = true;
    return Status::Ok();
  };
  Status offset_copy = enqueue(outer_offsets->data(), source.structure_first,
                               source_bytes->first);
  if (!offset_copy.ok()) {
    return offset_copy;
  }
  Status index_copy = enqueue(inner_indices->data(), source.structure_second,
                              source_bytes->second);
  if (!index_copy.ok()) {
    return index_copy;
  }
  Status value_copy =
      enqueue(values->data(), source.values, source_bytes->values);
  if (!value_copy.ok()) {
    return value_copy;
  }
  CompletionEvent completion =
      internal_core_execution::Access::MakeCompletedEvent();
  if (enqueued) {
    auto recorded = RecordCudaEvent(context.execution_context());
    if (!recorded.ok()) {
      static_cast<void>(
          cudaStreamSynchronize(static_cast<cudaStream_t>(*stream)));
      return recorded.status();
    }
    completion = std::move(*recorded);
  }
  return CloneBuffers{.outer_offsets = std::move(*outer_offsets),
                      .inner_indices = std::move(*inner_indices),
                      .values = std::move(*values),
                      .completion = std::move(completion)};
}

Result<IndexedCloneBuffers> CloneIndexedErased(SparseCudaContext& context,
                                               IndexedVectorDescriptor source,
                                               MemoryResource& resource) {
  auto stream = ValidateContext(context);
  if (!stream.ok()) {
    return stream.status();
  }
  if (source.memory_space != MemorySpace::kHost) {
    return Status(ErrorCode::kMemoryAccess,
                  "CudaCloneIndexedVector requires a canonical host indexed "
                  "vector");
  }
  auto source_bytes = ValidateIndexedMetadata(source);
  if (!source_bytes.ok()) {
    return source_bytes.status();
  }
  if (resource.space() != MemorySpace::kDevice) {
    return Status(ErrorCode::kMemoryAccess,
                  "CudaCloneIndexedVector requires device memory");
  }
  auto indices =
      Buffer::Allocate(resource, source_bytes->indices, alignof(index_t));
  if (!indices.ok()) {
    return indices.status();
  }
  auto values = Buffer::Allocate(resource, source_bytes->values,
                                 ElementAlignment(source.element_kind));
  if (!values.ok()) {
    return values.status();
  }
  auto guard = internal_core_cuda::DeviceGuard::Create(
      context.execution_context().device().ordinal);
  if (!guard.ok()) {
    return guard.status();
  }
  bool enqueued = false;
  const auto enqueue = [&](void* destination, const void* source_pointer,
                           std::size_t bytes) -> Status {
    if (bytes == 0) {
      return Status::Ok();
    }
    const cudaError_t error = cudaMemcpyAsync(
        destination, source_pointer, bytes, cudaMemcpyHostToDevice,
        static_cast<cudaStream_t>(*stream));
    if (error != cudaSuccess) {
      if (enqueued) {
        static_cast<void>(
            cudaStreamSynchronize(static_cast<cudaStream_t>(*stream)));
      }
      return internal_core_cuda::CudaStatus(
          error, ErrorCode::kMemoryTransfer,
          "CUDA could not enqueue an indexed-vector clone copy");
    }
    enqueued = true;
    return Status::Ok();
  };
  Status index_copy =
      enqueue(indices->data(), source.indices, source_bytes->indices);
  if (!index_copy.ok()) {
    return index_copy;
  }
  Status value_copy =
      enqueue(values->data(), source.values, source_bytes->values);
  if (!value_copy.ok()) {
    return value_copy;
  }
  CompletionEvent completion =
      internal_core_execution::Access::MakeCompletedEvent();
  if (enqueued) {
    auto recorded = RecordCudaEvent(context.execution_context());
    if (!recorded.ok()) {
      static_cast<void>(
          cudaStreamSynchronize(static_cast<cudaStream_t>(*stream)));
      return recorded.status();
    }
    completion = std::move(*recorded);
  }
  return IndexedCloneBuffers{.indices = std::move(*indices),
                             .values = std::move(*values),
                             .completion = std::move(completion)};
}

Result<std::size_t> CsrSpmvWorkspaceSizeErased(SparseCudaContext& context,
                                               SparseDescriptor matrix,
                                               VectorDescriptor input,
                                               VectorDescriptor output) {
  auto stream = ValidateContext(context);
  if (!stream.ok()) {
    return stream.status();
  }
  auto guard = internal_core_cuda::DeviceGuard::Create(
      context.execution_context().device().ordinal);
  if (!guard.ok()) {
    return guard.status();
  }
  Status operands = ValidateSpmvOperands(
      matrix, input, output, context.execution_context().device().ordinal);
  if (!operands.ok()) {
    return operands;
  }
  if (matrix.extents[0] == 0 || matrix.extents[1] == 0 || input.stride != 1 ||
      output.stride != 1) {
    return std::size_t{0};
  }
  ContextState* state = Access::State(context);
  std::lock_guard<std::mutex> lock(state->mutex());
  return QuerySpmvWorkspace(*state, matrix, input, output);
}

Result<CompletionEvent> CsrSpmvErased(SparseCudaContext& context, double alpha,
                                      SparseDescriptor matrix,
                                      VectorDescriptor input, double beta,
                                      VectorDescriptor output,
                                      MutableMemoryView workspace) {
  auto stream = ValidateContext(context);
  if (!stream.ok()) {
    return stream.status();
  }
  auto guard = internal_core_cuda::DeviceGuard::Create(
      context.execution_context().device().ordinal);
  if (!guard.ok()) {
    return guard.status();
  }
  Status operands = ValidateSpmvOperands(
      matrix, input, output, context.execution_context().device().ordinal);
  if (!operands.ok()) {
    return operands;
  }
  if (matrix.extents[0] == 0) {
    return internal_core_execution::Access::MakeCompletedEvent();
  }
  const bool project_kernel =
      matrix.extents[1] == 0 || input.stride != 1 || output.stride != 1;
  if (project_kernel) {
    if (workspace.size() != 0) {
      return Status(ErrorCode::kInvalidArgument,
                    "The strided CSR SpMV path requires zero workspace");
    }
    Status launch =
        LaunchStridedCsrSpmv(*stream, alpha, matrix, input, beta, output);
    if (!launch.ok()) {
      return launch;
    }
  } else {
    ContextState* state = Access::State(context);
    std::lock_guard<std::mutex> lock(state->mutex());
    auto required = QuerySpmvWorkspace(*state, matrix, input, output);
    if (!required.ok()) {
      return required.status();
    }
    if (workspace.space() != MemorySpace::kDevice ||
        workspace.size() < *required ||
        (*required != 0 && workspace.data() == nullptr)) {
      return Status(ErrorCode::kInvalidArgument,
                    "CUDA CSR SpMV workspace is missing or too small");
    }
    Status workspace_address =
        ValidateAddress(workspace.data(), *required, alignof(std::max_align_t),
                        "CUDA CSR SpMV workspace cannot be null",
                        "CUDA CSR SpMV workspace is misaligned");
    if (!workspace_address.ok()) {
      return workspace_address;
    }
    Status workspace_pointer =
        ValidateDevicePointer(workspace.data(), workspace.size(),
                              context.execution_context().device().ordinal,
                              "CUDA could not inspect CSR SpMV workspace");
    if (!workspace_pointer.ok()) {
      return workspace_pointer;
    }
    Status full_workspace_address = ValidateAddress(
        workspace.data(), workspace.size(), alignof(std::max_align_t),
        "CUDA CSR SpMV workspace cannot be null",
        "CUDA CSR SpMV workspace is misaligned");
    if (!full_workspace_address.ok()) {
      return full_workspace_address;
    }
    auto matrix_bytes = ValidateSparseMetadata(matrix);
    auto input_bytes = VectorBytes(input);
    auto output_bytes = VectorBytes(output);
    if (!matrix_bytes.ok()) {
      return matrix_bytes.status();
    }
    if (!input_bytes.ok()) {
      return input_bytes.status();
    }
    if (!output_bytes.ok()) {
      return output_bytes.status();
    }
    if (Overlap(workspace.data(), workspace.size(), matrix.structure_first,
                matrix_bytes->first) ||
        Overlap(workspace.data(), workspace.size(), matrix.structure_second,
                matrix_bytes->second) ||
        Overlap(workspace.data(), workspace.size(), matrix.values,
                matrix_bytes->values) ||
        Overlap(workspace.data(), workspace.size(), input.data, *input_bytes) ||
        Overlap(workspace.data(), workspace.size(), output.data,
                *output_bytes)) {
      return Status(ErrorCode::kInvalidArgument,
                    "CUDA CSR SpMV workspace overlaps an operand");
    }

    SpmvDescriptors descriptors;
    Status descriptor_status =
        CreateSpmvDescriptors(matrix, input, output, descriptors);
    if (!descriptor_status.ok()) {
      return descriptor_status;
    }
    float alpha_float = static_cast<float>(alpha);
    float beta_float = static_cast<float>(beta);
    double alpha_double = alpha;
    double beta_double = beta;
    const void* alpha_pointer = matrix.element_kind == ElementKind::kFloat
                                    ? static_cast<const void*>(&alpha_float)
                                    : static_cast<const void*>(&alpha_double);
    const void* beta_pointer = matrix.element_kind == ElementKind::kFloat
                                   ? static_cast<const void*>(&beta_float)
                                   : static_cast<const void*>(&beta_double);
    static_cast<void>(cudaGetLastError());
    const cusparseStatus_t provider_status = cusparseSpMV(
        state->handle(), CUSPARSE_OPERATION_NON_TRANSPOSE, alpha_pointer,
        descriptors.matrix_, descriptors.input_, beta_pointer,
        descriptors.output_, DataType(matrix.element_kind),
        CUSPARSE_SPMV_CSR_ALG2, workspace.data());
    if (provider_status != CUSPARSE_STATUS_SUCCESS) {
      static_cast<void>(
          cudaStreamSynchronize(static_cast<cudaStream_t>(*stream)));
      return CusparseStatus(provider_status,
                            "cuSPARSE could not enqueue deterministic CSR "
                            "SpMV");
    }
    const cudaError_t launch_error = cudaGetLastError();
    if (launch_error != cudaSuccess) {
      static_cast<void>(
          cudaStreamSynchronize(static_cast<cudaStream_t>(*stream)));
      return internal_core_cuda::CudaStatus(
          launch_error, ErrorCode::kProvider,
          "CUDA reported a CSR SpMV launch failure");
    }
  }
  auto completion = RecordCudaEvent(context.execution_context());
  if (!completion.ok()) {
    static_cast<void>(
        cudaStreamSynchronize(static_cast<cudaStream_t>(*stream)));
    return completion.status();
  }
  return completion;
}

Result<CompletionEvent> EvaluateErased(SparseCudaContext& context,
                                       PointwiseOperation operation,
                                       OperandDescriptor left,
                                       OperandDescriptor right,
                                       SparseDescriptor destination) {
  auto stream = ValidateContext(context);
  if (!stream.ok()) {
    return stream.status();
  }
  auto guard = internal_core_cuda::DeviceGuard::Create(
      context.execution_context().device().ordinal);
  if (!guard.ok()) {
    return guard.status();
  }
  Status destination_status = ValidateSparseDevice(
      destination, context.execution_context().device().ordinal);
  if (!destination_status.ok()) {
    return destination_status;
  }
  Status left_status = ValidateEvaluationOperand(
      left, destination, context.execution_context().device().ordinal);
  if (!left_status.ok()) {
    return left_status;
  }
  const bool binary = operation == PointwiseOperation::kAdd ||
                      operation == PointwiseOperation::kSubtract ||
                      operation == PointwiseOperation::kMultiply;
  if (binary) {
    Status right_status = ValidateEvaluationOperand(
        right, destination, context.execution_context().device().ordinal);
    if (!right_status.ok()) {
      return right_status;
    }
  }

  auto destination_bytes = ValidateSparseMetadata(destination);
  if (!destination_bytes.ok()) {
    return destination_bytes.status();
  }
  for (const OperandDescriptor* operand : {&left, binary ? &right : nullptr}) {
    if (operand == nullptr || operand->kind != OperandKind::kView) {
      continue;
    }
    auto operand_bytes = ValidateSparseMetadata(operand->view);
    if (!operand_bytes.ok()) {
      return operand_bytes.status();
    }
    if (operand->view.values != destination.values &&
        Overlap(operand->view.values, operand_bytes->values, destination.values,
                destination_bytes->values)) {
      return Status(ErrorCode::kInvalidArgument,
                    "CUDA sparse evaluation rejects partial value overlap");
    }
  }
  if (destination.nonzeros == 0) {
    return internal_core_execution::Access::MakeCompletedEvent();
  }
  Status launch =
      LaunchSparsePointwise(*stream, operation, left, right, destination);
  if (!launch.ok()) {
    return launch;
  }
  auto completion = RecordCudaEvent(context.execution_context());
  if (!completion.ok()) {
    static_cast<void>(
        cudaStreamSynchronize(static_cast<cudaStream_t>(*stream)));
    return completion.status();
  }
  return completion;
}

Result<CompletionEvent> StandardLevel1Erased(SparseCudaContext& context,
                                             StandardOperation operation,
                                             SparseBlasConjugation conjugation,
                                             ScalarValue alpha,
                                             IndexedVectorDescriptor sparse,
                                             VectorDescriptor dense,
                                             VectorDescriptor result) {
  Status enum_status =
      internal_sparse_standard_blas::ValidateConjugation(conjugation);
  if (!enum_status.ok()) {
    return enum_status;
  }
  auto stream = ValidateContext(context);
  if (!stream.ok()) {
    return stream.status();
  }
  auto guard = internal_core_cuda::DeviceGuard::Create(
      context.execution_context().device().ordinal);
  if (!guard.ok()) {
    return guard.status();
  }
  const std::int32_t device = context.execution_context().device().ordinal;
  const bool sparse_writable = operation == StandardOperation::kGather ||
                               operation == StandardOperation::kGatherZero;
  auto sparse_bytes = ValidateIndexedDevice(sparse, sparse_writable, device);
  if (!sparse_bytes.ok()) {
    return sparse_bytes.status();
  }
  const bool dense_writable = operation == StandardOperation::kAxpy ||
                              operation == StandardOperation::kGatherZero ||
                              operation == StandardOperation::kScatter;
  Status dense_status =
      ValidateVector(dense, sparse.element_kind, dense_writable, device);
  if (!dense_status.ok()) {
    return dense_status;
  }
  if (dense.extent != sparse.dense_extent) {
    return Status(ErrorCode::kShape,
                  "CUDA Sparse BLAS level-one vector extents do not match");
  }
  auto dense_bytes = VectorBytes(dense);
  if (!dense_bytes.ok()) {
    return dense_bytes.status();
  }
  const void* dense_data = ReachableData(dense);
  const std::size_t dense_size = ReachableSize(dense, *dense_bytes);
  if (operation != StandardOperation::kDot &&
      (Overlap(sparse.values, sparse_bytes->values, dense_data, dense_size) ||
       Overlap(sparse.indices, sparse_bytes->indices, dense_data,
               dense_size))) {
    return Status(ErrorCode::kInvalidArgument,
                  "CUDA Sparse BLAS level-one writable operands overlap");
  }
  if (operation == StandardOperation::kDot) {
    Status result_status =
        ValidateVector(result, sparse.element_kind, true, device);
    if (!result_status.ok()) {
      return result_status;
    }
    if (result.extent != 1) {
      return Status(ErrorCode::kShape,
                    "CUDA sparse dot requires one result element");
    }
    auto result_bytes = VectorBytes(result);
    if (!result_bytes.ok()) {
      return result_bytes.status();
    }
    const void* result_data = ReachableData(result);
    const std::size_t result_size = ReachableSize(result, *result_bytes);
    if (Overlap(result_data, result_size, dense_data, dense_size) ||
        Overlap(result_data, result_size, sparse.indices,
                sparse_bytes->indices) ||
        Overlap(result_data, result_size, sparse.values,
                sparse_bytes->values)) {
      return Status(ErrorCode::kInvalidArgument,
                    "CUDA sparse dot result overlaps an input");
    }
  }
  Status launch = LaunchStandardLevel1(*stream, operation, conjugation, alpha,
                                       sparse, dense, result);
  if (!launch.ok()) {
    return launch;
  }
  auto completion = RecordCudaEvent(context.execution_context());
  if (!completion.ok()) {
    static_cast<void>(
        cudaStreamSynchronize(static_cast<cudaStream_t>(*stream)));
    return completion.status();
  }
  return completion;
}

Result<CompletionEvent> StandardSpmvErased(SparseCudaContext& context,
                                           SparseBlasTranspose transpose,
                                           ScalarValue alpha,
                                           SparseDescriptor matrix,
                                           VectorDescriptor input,
                                           VectorDescriptor output) {
  Status enum_status =
      internal_sparse_standard_blas::ValidateTranspose(transpose);
  if (!enum_status.ok()) {
    return enum_status;
  }
  auto stream = ValidateContext(context);
  if (!stream.ok()) {
    return stream.status();
  }
  auto guard = internal_core_cuda::DeviceGuard::Create(
      context.execution_context().device().ordinal);
  if (!guard.ok()) {
    return guard.status();
  }
  const std::int32_t device = context.execution_context().device().ordinal;
  if (matrix.format != FormatKind::kCsr) {
    return Status(ErrorCode::kInvalidArgument,
                  "CUDA standardized SpMV requires explicit CSR storage");
  }
  Status matrix_status = ValidateSparseDevice(matrix, device);
  if (!matrix_status.ok()) {
    return matrix_status;
  }
  Status input_status =
      ValidateVector(input, matrix.element_kind, false, device);
  if (!input_status.ok()) {
    return input_status;
  }
  Status output_status =
      ValidateVector(output, matrix.element_kind, true, device);
  if (!output_status.ok()) {
    return output_status;
  }
  const extent_t input_extent = transpose == SparseBlasTranspose::kNone
                                    ? matrix.extents[1]
                                    : matrix.extents[0];
  const extent_t output_extent = transpose == SparseBlasTranspose::kNone
                                     ? matrix.extents[0]
                                     : matrix.extents[1];
  if (input.extent != input_extent || output.extent != output_extent) {
    return Status(ErrorCode::kShape,
                  "CUDA standardized SpMV vector extents do not match");
  }
  auto matrix_bytes = ValidateSparseMetadata(matrix);
  auto input_bytes = VectorBytes(input);
  auto output_bytes = VectorBytes(output);
  if (!matrix_bytes.ok()) {
    return matrix_bytes.status();
  }
  if (!input_bytes.ok()) {
    return input_bytes.status();
  }
  if (!output_bytes.ok()) {
    return output_bytes.status();
  }
  const void* output_data = ReachableData(output);
  const std::size_t output_size = ReachableSize(output, *output_bytes);
  if (Overlap(ReachableData(input), ReachableSize(input, *input_bytes),
              output_data, output_size) ||
      Overlap(matrix.structure_first, matrix_bytes->first, output_data,
              output_size) ||
      Overlap(matrix.structure_second, matrix_bytes->second, output_data,
              output_size) ||
      Overlap(matrix.values, matrix_bytes->values, output_data, output_size)) {
    return Status(ErrorCode::kInvalidArgument,
                  "CUDA standardized SpMV output overlaps an input");
  }
  Status launch =
      LaunchStandardSpmv(*stream, transpose, alpha, matrix, input, output);
  if (!launch.ok()) {
    return launch;
  }
  auto completion = RecordCudaEvent(context.execution_context());
  if (!completion.ok()) {
    static_cast<void>(
        cudaStreamSynchronize(static_cast<cudaStream_t>(*stream)));
    return completion.status();
  }
  return completion;
}

Result<CompletionEvent> StandardSpmmErased(SparseCudaContext& context,
                                           SparseBlasTranspose transpose,
                                           ScalarValue alpha,
                                           SparseDescriptor matrix,
                                           MatrixDescriptor input,
                                           MatrixDescriptor output) {
  Status enum_status =
      internal_sparse_standard_blas::ValidateTranspose(transpose);
  if (!enum_status.ok()) {
    return enum_status;
  }
  auto stream = ValidateContext(context);
  if (!stream.ok()) {
    return stream.status();
  }
  auto guard = internal_core_cuda::DeviceGuard::Create(
      context.execution_context().device().ordinal);
  if (!guard.ok()) {
    return guard.status();
  }
  const std::int32_t device = context.execution_context().device().ordinal;
  if (matrix.format != FormatKind::kCsr) {
    return Status(ErrorCode::kInvalidArgument,
                  "CUDA standardized SpMM requires explicit CSR storage");
  }
  Status matrix_status = ValidateSparseDevice(matrix, device);
  if (!matrix_status.ok()) {
    return matrix_status;
  }
  auto input_bytes =
      ValidateMatrixDevice(input, matrix.element_kind, false, device);
  auto output_bytes =
      ValidateMatrixDevice(output, matrix.element_kind, true, device);
  if (!input_bytes.ok()) {
    return input_bytes.status();
  }
  if (!output_bytes.ok()) {
    return output_bytes.status();
  }
  const extent_t inner = transpose == SparseBlasTranspose::kNone
                             ? matrix.extents[1]
                             : matrix.extents[0];
  const extent_t rows = transpose == SparseBlasTranspose::kNone
                            ? matrix.extents[0]
                            : matrix.extents[1];
  if (input.rows != inner || output.rows != rows ||
      input.columns != output.columns) {
    return Status(ErrorCode::kShape,
                  "CUDA standardized SpMM matrix extents do not match");
  }
  auto sparse_bytes = ValidateSparseMetadata(matrix);
  if (!sparse_bytes.ok()) {
    return sparse_bytes.status();
  }
  const void* output_data = ReachableData(output);
  if (Overlap(ReachableData(input), *input_bytes, output_data, *output_bytes) ||
      Overlap(matrix.structure_first, sparse_bytes->first, output_data,
              *output_bytes) ||
      Overlap(matrix.structure_second, sparse_bytes->second, output_data,
              *output_bytes) ||
      Overlap(matrix.values, sparse_bytes->values, output_data,
              *output_bytes)) {
    return Status(ErrorCode::kInvalidArgument,
                  "CUDA standardized SpMM output overlaps an input");
  }
  Status launch =
      LaunchStandardSpmm(*stream, transpose, alpha, matrix, input, output);
  if (!launch.ok()) {
    return launch;
  }
  auto completion = RecordCudaEvent(context.execution_context());
  if (!completion.ok()) {
    static_cast<void>(
        cudaStreamSynchronize(static_cast<cudaStream_t>(*stream)));
    return completion.status();
  }
  return completion;
}

Result<CompletionEvent> StandardTriangularSolveErased(
    SparseCudaContext& context, SparseBlasTranspose transpose,
    ScalarValue alpha, SparseDescriptor matrix, SparseBlasTriangle triangle,
    SparseBlasDiagonal diagonal, MatrixDescriptor right_hand_sides) {
  Status enum_status =
      internal_sparse_standard_blas::ValidateTranspose(transpose);
  if (!enum_status.ok()) {
    return enum_status;
  }
  enum_status = internal_sparse_standard_blas::ValidateTriangle(triangle);
  if (!enum_status.ok()) {
    return enum_status;
  }
  enum_status = internal_sparse_standard_blas::ValidateDiagonal(diagonal);
  if (!enum_status.ok()) {
    return enum_status;
  }
  auto stream = ValidateContext(context);
  if (!stream.ok()) {
    return stream.status();
  }
  auto guard = internal_core_cuda::DeviceGuard::Create(
      context.execution_context().device().ordinal);
  if (!guard.ok()) {
    return guard.status();
  }
  const std::int32_t device = context.execution_context().device().ordinal;
  if (matrix.format != FormatKind::kCsr ||
      matrix.extents[0] != matrix.extents[1]) {
    return Status(ErrorCode::kShape,
                  "CUDA triangular solve requires square CSR storage");
  }
  Status matrix_status = ValidateSparseDevice(matrix, device);
  if (!matrix_status.ok()) {
    return matrix_status;
  }
  auto rhs_bytes =
      ValidateMatrixDevice(right_hand_sides, matrix.element_kind, true, device);
  if (!rhs_bytes.ok()) {
    return rhs_bytes.status();
  }
  if (right_hand_sides.rows != matrix.extents[0]) {
    return Status(ErrorCode::kShape,
                  "CUDA triangular right-hand-side extent does not match");
  }
  auto sparse_bytes = ValidateSparseMetadata(matrix);
  if (!sparse_bytes.ok()) {
    return sparse_bytes.status();
  }
  const void* rhs_data = ReachableData(right_hand_sides);
  if (Overlap(matrix.structure_first, sparse_bytes->first, rhs_data,
              *rhs_bytes) ||
      Overlap(matrix.structure_second, sparse_bytes->second, rhs_data,
              *rhs_bytes) ||
      Overlap(matrix.values, sparse_bytes->values, rhs_data, *rhs_bytes)) {
    return Status(ErrorCode::kInvalidArgument,
                  "CUDA triangular right-hand sides overlap the matrix");
  }
  Status launch = LaunchStandardTriangularSolve(
      *stream, transpose, alpha, matrix, triangle, diagonal, right_hand_sides);
  if (!launch.ok()) {
    return launch;
  }
  auto completion = RecordCudaEvent(context.execution_context());
  if (!completion.ok()) {
    static_cast<void>(
        cudaStreamSynchronize(static_cast<cudaStream_t>(*stream)));
    return completion.status();
  }
  return completion;
}

}  // namespace asc::internal_sparse_cuda
