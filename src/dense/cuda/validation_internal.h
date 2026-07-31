#ifndef ASC_SRC_DENSE_CUDA_VALIDATION_INTERNAL_H_
#define ASC_SRC_DENSE_CUDA_VALIDATION_INTERNAL_H_

#include <cuda_runtime_api.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "../../core/cuda/cuda_internal.h"
#include "asc/core/result.h"
#include "asc/core/types.h"
#include "asc/dense/providers/cuda.h"

namespace asc::internal_dense_cuda {

inline std::size_t ElementBytes(ElementKind kind) noexcept {
  return kind == ElementKind::kFloat ? sizeof(float) : sizeof(double);
}

inline Result<std::size_t> ViewBytes(const ViewDescriptor& view) {
  return CheckedMultiply(view.required_span_size,
                         ElementBytes(view.element_kind));
}

inline Status ValidateViewMetadata(const ViewDescriptor& view,
                                   ElementKind expected_kind,
                                   std::size_t maximum_rank) {
  if (view.element_kind != expected_kind) {
    return Status(ErrorCode::kInvalidArgument,
                  "CUDA Dense operands must have matching element types");
  }
  if (view.rank > maximum_rank || view.rank > view.extents.size()) {
    return Status(ErrorCode::kUnsupported,
                  "CUDA Dense operand rank is unsupported");
  }
  if (view.memory_space != MemorySpace::kDevice) {
    return Status(ErrorCode::kMemoryAccess,
                  "CUDA Dense operations require device memory");
  }

  extent_t logical_size = 1;
  bool empty = false;
  stride_t maximum_offset = 0;
  for (std::size_t dimension = 0; dimension < view.rank; ++dimension) {
    if (view.extents[dimension] < 0 || view.strides[dimension] < 0) {
      return Status(ErrorCode::kInvalidArgument,
                    "CUDA Dense extents and strides cannot be negative");
    }
    empty = empty || view.extents[dimension] == 0;
    if (!empty) {
      auto next_size = CheckedMultiply(logical_size, view.extents[dimension]);
      if (!next_size.ok()) {
        return next_size.status();
      }
      logical_size = *next_size;
    }
    if (view.extents[dimension] > 0) {
      auto contribution = CheckedMultiply<stride_t>(view.extents[dimension] - 1,
                                                    view.strides[dimension]);
      if (!contribution.ok()) {
        return contribution.status();
      }
      auto next_offset = CheckedAdd(maximum_offset, *contribution);
      if (!next_offset.ok()) {
        return next_offset.status();
      }
      maximum_offset = *next_offset;
    }
  }
  if (empty) {
    logical_size = 0;
  }
  if (logical_size != view.logical_size) {
    return Status(ErrorCode::kShape,
                  "CUDA Dense descriptor logical size is inconsistent");
  }

  std::size_t expected_span = 0;
  if (logical_size != 0) {
    auto span = CheckedAdd(maximum_offset, stride_t{1});
    if (!span.ok()) {
      return span.status();
    }
    auto converted_span = CheckedCast<std::size_t>(*span);
    if (!converted_span.ok()) {
      return converted_span.status();
    }
    expected_span = *converted_span;
  }
  if (expected_span != view.required_span_size) {
    return Status(ErrorCode::kShape,
                  "CUDA Dense descriptor span is inconsistent");
  }
  auto bytes = ViewBytes(view);
  if (!bytes.ok()) {
    return bytes.status();
  }
  if (*bytes != 0 && view.data == nullptr) {
    return Status(ErrorCode::kMemoryAccess,
                  "A nonempty CUDA Dense view has a null pointer");
  }
  if (*bytes != 0 && reinterpret_cast<std::uintptr_t>(view.data) %
                             ElementBytes(view.element_kind) !=
                         0) {
    return Status(ErrorCode::kMemoryAccess,
                  "A CUDA Dense view pointer is misaligned");
  }
  return Status::Ok();
}

inline Status ValidateDevicePointer(const ViewDescriptor& view,
                                    std::int32_t device) {
  if (view.logical_size == 0) {
    return Status::Ok();
  }
  cudaPointerAttributes attributes{};
  const cudaError_t error = cudaPointerGetAttributes(&attributes, view.data);
  if (error != cudaSuccess) {
    return internal_core_cuda::CudaStatus(
        error, ErrorCode::kMemoryAccess,
        "CUDA could not inspect a Dense operand pointer");
  }
  if (attributes.type != cudaMemoryTypeDevice || attributes.device != device) {
    return Status(ErrorCode::kMemoryAccess,
                  "A CUDA Dense operand is not on the context device");
  }
  return Status::Ok();
}

inline bool SameDescriptor(const ViewDescriptor& left,
                           const ViewDescriptor& right) noexcept {
  if (left.data != right.data || left.memory_space != right.memory_space ||
      left.element_kind != right.element_kind || left.rank != right.rank) {
    return false;
  }
  for (std::size_t dimension = 0; dimension < left.rank; ++dimension) {
    if (left.extents[dimension] != right.extents[dimension] ||
        left.strides[dimension] != right.strides[dimension]) {
      return false;
    }
  }
  return true;
}

inline Result<bool> ViewsOverlap(const ViewDescriptor& left,
                                 const ViewDescriptor& right) {
  auto left_bytes = ViewBytes(left);
  if (!left_bytes.ok()) {
    return left_bytes.status();
  }
  auto right_bytes = ViewBytes(right);
  if (!right_bytes.ok()) {
    return right_bytes.status();
  }
  if (*left_bytes == 0 || *right_bytes == 0) {
    return false;
  }
  const std::uintptr_t left_begin = reinterpret_cast<std::uintptr_t>(left.data);
  const std::uintptr_t right_begin =
      reinterpret_cast<std::uintptr_t>(right.data);
  if (left_begin > std::numeric_limits<std::uintptr_t>::max() - *left_bytes ||
      right_begin > std::numeric_limits<std::uintptr_t>::max() - *right_bytes) {
    return true;
  }
  return left_begin < right_begin + *right_bytes &&
         right_begin < left_begin + *left_bytes;
}

inline Status ValidateSameShape(const ViewDescriptor& left,
                                const ViewDescriptor& right) {
  if (left.rank != right.rank) {
    return Status(ErrorCode::kShape, "CUDA Dense operand ranks do not match");
  }
  for (std::size_t dimension = 0; dimension < left.rank; ++dimension) {
    if (left.extents[dimension] != right.extents[dimension]) {
      return Status(ErrorCode::kShape,
                    "CUDA Dense operand extents do not match");
    }
  }
  return Status::Ok();
}

inline Status ValidateMatrixMapping(const ViewDescriptor& matrix) {
  if (matrix.rank != 2) {
    return Status(ErrorCode::kShape, "A CUDA Dense matrix must have rank two");
  }
  if (matrix.layout_kind == DenseLayoutKind::kRight) {
    return Status(ErrorCode::kUnsupported,
                  "cuBLAS does not accept LayoutRight matrices");
  }
  const stride_t minimum_leading_dimension =
      std::max<stride_t>(1, matrix.extents[0]);
  if (matrix.strides[0] != 1 ||
      (matrix.logical_size != 0 &&
       matrix.strides[1] < minimum_leading_dimension)) {
    return Status(ErrorCode::kUnsupported,
                  "cuBLAS requires a column-major leading-dimension mapping");
  }
  return Status::Ok();
}

inline Status ValidateMatrixOperation(MatrixOperation operation) {
  switch (operation) {
    case MatrixOperation::kNone:
    case MatrixOperation::kTranspose:
    case MatrixOperation::kConjugateTranspose:
      return Status::Ok();
  }
  return Status(ErrorCode::kInvalidArgument,
                "MatrixOperation is not a recognized enumerator");
}

inline Result<int> ProviderExtentInteger(extent_t value, const char* message) {
  if (value < 0 ||
      value > static_cast<extent_t>(std::numeric_limits<int>::max())) {
    return Status(ErrorCode::kOverflow, message);
  }
  return static_cast<int>(value);
}

inline Result<int> ProviderStrideInteger(stride_t value, const char* message) {
  if (value <= 0 ||
      value > static_cast<stride_t>(std::numeric_limits<int>::max())) {
    return Status(ErrorCode::kOverflow, message);
  }
  return static_cast<int>(value);
}

}  // namespace asc::internal_dense_cuda

#endif  // ASC_SRC_DENSE_CUDA_VALIDATION_INTERNAL_H_
