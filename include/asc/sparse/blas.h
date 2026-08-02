#ifndef ASC_SPARSE_BLAS_H_
#define ASC_SPARSE_BLAS_H_

#include <algorithm>
#include <array>
#include <complex>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/expression/expression.h"
#include "asc/expression/writable.h"
#include "asc/sparse/compressed.h"
#include "asc/sparse/export.h"

namespace asc {

template <typename Element>
concept SparseBlasScalar =
    std::same_as<std::remove_cv_t<Element>, float> ||
    std::same_as<std::remove_cv_t<Element>, double> ||
    std::same_as<std::remove_cv_t<Element>, std::complex<float>> ||
    std::same_as<std::remove_cv_t<Element>, std::complex<double>>;

template <typename Element>
concept SparseBlasComplex =
    std::same_as<std::remove_cv_t<Element>, std::complex<float>> ||
    std::same_as<std::remove_cv_t<Element>, std::complex<double>>;

enum class SparseBlasConjugation : std::uint8_t {
  kUnconjugated,
  kConjugated,
};

enum class SparseBlasTranspose : std::uint8_t {
  kNone,
  kTranspose,
  kConjugateTranspose,
};

enum class SparseBlasLayout : std::uint8_t {
  kColumnMajor,
  kRowMajor,
};

enum class SparseBlasTriangle : std::uint8_t {
  kUpper,
  kLower,
};

enum class SparseBlasDiagonal : std::uint8_t {
  kNonUnit,
  kUnit,
};

namespace internal_sparse_standard_blas {

class ProviderAccess;

struct StorageBounds {
  const void* data = nullptr;
  std::size_t size = 0;
};

inline Result<StorageBounds> ValidateStorageBounds(
    const void* data, stride_t element_span, std::size_t element_size,
    std::size_t element_alignment, ConstMemoryView backing_storage) {
  if (element_span < 0 || !backing_storage.valid()) {
    return Status(ErrorCode::kInvalidArgument,
                  "A sparse BLAS storage descriptor is invalid");
  }
  if (element_span == 0) {
    return StorageBounds{};
  }
  if (data == nullptr || backing_storage.data() == nullptr) {
    return Status(ErrorCode::kMemoryAccess,
                  "Nonempty sparse BLAS storage requires backing memory");
  }
  const std::uintptr_t begin = reinterpret_cast<std::uintptr_t>(data);
  if (begin % element_alignment != 0) {
    return Status(ErrorCode::kInvalidArgument,
                  "A sparse BLAS pointer is not properly aligned");
  }
  auto byte_span = CheckedMultiply<stride_t>(
      element_span, static_cast<stride_t>(element_size));
  if (!byte_span.ok()) {
    return byte_span.status();
  }
  auto converted_span = CheckedCast<std::size_t>(*byte_span);
  if (!converted_span.ok()) {
    return converted_span.status();
  }
  if (*converted_span > std::numeric_limits<std::uintptr_t>::max() - begin) {
    return Status(ErrorCode::kOverflow,
                  "A sparse BLAS address calculation overflowed");
  }
  const std::uintptr_t end = begin + *converted_span;
  const std::uintptr_t backing_begin =
      reinterpret_cast<std::uintptr_t>(backing_storage.data());
  if (backing_storage.size() >
      std::numeric_limits<std::uintptr_t>::max() - backing_begin) {
    return Status(ErrorCode::kOverflow,
                  "A sparse BLAS backing span calculation overflowed");
  }
  const std::uintptr_t backing_end = backing_begin + backing_storage.size();
  if (begin < backing_begin || end > backing_end) {
    return Status(ErrorCode::kMemoryAccess,
                  "Sparse BLAS storage is outside its backing span");
  }
  return StorageBounds{data, *converted_span};
}

inline bool Overlap(ConstMemoryView left, ConstMemoryView right) noexcept {
  if (left.size() == 0 || right.size() == 0) {
    return false;
  }
  const auto left_begin = reinterpret_cast<std::uintptr_t>(left.data());
  const auto right_begin = reinterpret_cast<std::uintptr_t>(right.data());
  if (left.size() > UINTPTR_MAX - left_begin ||
      right.size() > UINTPTR_MAX - right_begin) {
    return true;
  }
  return left_begin < right_begin + right.size() &&
         right_begin < left_begin + left.size();
}

template <SparseBlasScalar Element>
Element MaybeConjugate(Element value, bool conjugate) noexcept {
  if constexpr (SparseBlasComplex<Element>) {
    return conjugate ? std::conj(value) : value;
  } else {
    return value;
  }
}

inline Status ValidateSerial(const ExecutionContext& context) {
  if (context.backend() != Backend::kSerial ||
      !context.CanAccess(MemorySpace::kHost)) {
    return Status(ErrorCode::kUnsupported,
                  "Sparse BLAS CPU operations require serial execution");
  }
  return Status::Ok();
}

inline Status ValidateConjugation(SparseBlasConjugation conjugation) {
  switch (conjugation) {
    case SparseBlasConjugation::kUnconjugated:
    case SparseBlasConjugation::kConjugated:
      return Status::Ok();
  }
  return Status(ErrorCode::kInvalidArgument,
                "A sparse BLAS conjugation value is invalid");
}

inline Status ValidateTranspose(SparseBlasTranspose transpose) {
  switch (transpose) {
    case SparseBlasTranspose::kNone:
    case SparseBlasTranspose::kTranspose:
    case SparseBlasTranspose::kConjugateTranspose:
      return Status::Ok();
  }
  return Status(ErrorCode::kInvalidArgument,
                "A sparse BLAS transpose value is invalid");
}

inline Status ValidateTriangle(SparseBlasTriangle triangle) {
  switch (triangle) {
    case SparseBlasTriangle::kUpper:
    case SparseBlasTriangle::kLower:
      return Status::Ok();
  }
  return Status(ErrorCode::kInvalidArgument,
                "A sparse BLAS triangle value is invalid");
}

inline Status ValidateDiagonal(SparseBlasDiagonal diagonal) {
  switch (diagonal) {
    case SparseBlasDiagonal::kNonUnit:
    case SparseBlasDiagonal::kUnit:
      return Status::Ok();
  }
  return Status(ErrorCode::kInvalidArgument,
                "A sparse BLAS diagonal value is invalid");
}

}  // namespace internal_sparse_standard_blas

template <typename Element>
class SparseBlasVectorView {
 public:
  using value_type = Element;
  using element_type = std::remove_const_t<Element>;

  static_assert(SparseBlasScalar<Element>,
                "SparseBlasVectorView requires a BLAS scalar");

  static Result<SparseBlasVectorView> Create(Element* logical_first,
                                             extent_t size, stride_t increment,
                                             ConstMemoryView backing_storage) {
    if (size < 0 || increment == 0 || !backing_storage.valid()) {
      return Status(ErrorCode::kInvalidArgument,
                    "A sparse BLAS vector descriptor is invalid");
    }
    if (size == 0) {
      return SparseBlasVectorView(logical_first, size, increment,
                                  backing_storage, nullptr, 0);
    }
    if (logical_first == nullptr || backing_storage.data() == nullptr) {
      return Status(ErrorCode::kMemoryAccess,
                    "A nonempty sparse BLAS vector requires backing storage");
    }
    const std::uintptr_t logical_address =
        reinterpret_cast<std::uintptr_t>(logical_first);
    if (logical_address % alignof(element_type) != 0) {
      return Status(ErrorCode::kInvalidArgument,
                    "A sparse BLAS vector pointer is not aligned");
    }
    auto element_offset = CheckedMultiply<stride_t>(size - 1, increment);
    if (!element_offset.ok()) {
      return element_offset.status();
    }
    auto byte_offset = CheckedMultiply<stride_t>(
        *element_offset, static_cast<stride_t>(sizeof(element_type)));
    if (!byte_offset.ok()) {
      return byte_offset.status();
    }
    std::uintptr_t other_address = logical_address;
    if (*byte_offset >= 0) {
      const auto offset = static_cast<std::uintptr_t>(*byte_offset);
      if (offset > UINTPTR_MAX - logical_address) {
        return Status(ErrorCode::kOverflow,
                      "A sparse BLAS vector address overflowed");
      }
      other_address += offset;
    } else {
      if (*byte_offset == std::numeric_limits<stride_t>::min()) {
        return Status(ErrorCode::kOverflow,
                      "A sparse BLAS vector address overflowed");
      }
      const auto offset = static_cast<std::uintptr_t>(-*byte_offset);
      if (logical_address < offset) {
        return Status(ErrorCode::kOverflow,
                      "A sparse BLAS vector address underflowed");
      }
      other_address -= offset;
    }
    const std::uintptr_t begin = std::min(logical_address, other_address);
    const std::uintptr_t last = std::max(logical_address, other_address);
    if (sizeof(element_type) > UINTPTR_MAX - last) {
      return Status(ErrorCode::kOverflow,
                    "A sparse BLAS vector span overflowed");
    }
    const std::uintptr_t end = last + sizeof(element_type);
    const auto backing_begin =
        reinterpret_cast<std::uintptr_t>(backing_storage.data());
    if (backing_storage.size() > UINTPTR_MAX - backing_begin ||
        begin < backing_begin || end > backing_begin + backing_storage.size()) {
      return Status(ErrorCode::kMemoryAccess,
                    "A sparse BLAS vector is outside its backing span");
    }
    return SparseBlasVectorView(logical_first, size, increment, backing_storage,
                                // The checked address supports negative stride.
                                // NOLINTNEXTLINE(performance-no-int-to-ptr)
                                reinterpret_cast<const void*>(begin),
                                static_cast<std::size_t>(end - begin));
  }

  // Mutable-to-const views intentionally convert implicitly.
  // NOLINTNEXTLINE(google-explicit-constructor)
  operator SparseBlasVectorView<const element_type>() const noexcept
    requires(!std::is_const_v<Element>)
  {
    return SparseBlasVectorView<const element_type>(
        data_, size_, increment_, backing_storage_, reachable_data_,
        reachable_size_);
  }

  [[nodiscard]] Element* data() const noexcept { return data_; }
  [[nodiscard]] extent_t size() const noexcept { return size_; }
  [[nodiscard]] stride_t increment() const noexcept { return increment_; }
  [[nodiscard]] MemorySpace memory_space() const noexcept {
    return backing_storage_.space();
  }
  [[nodiscard]] ConstMemoryView backing_storage() const noexcept {
    return backing_storage_;
  }
  [[nodiscard]] ConstMemoryView reachable_storage() const noexcept {
    return {reachable_data_, reachable_size_, backing_storage_.space()};
  }

 private:
  template <typename>
  friend class SparseBlasVectorView;

  SparseBlasVectorView(Element* data, extent_t size, stride_t increment,
                       ConstMemoryView backing_storage,
                       const void* reachable_data,
                       std::size_t reachable_size) noexcept
      : data_(data),
        size_(size),
        increment_(increment),
        backing_storage_(backing_storage),
        reachable_data_(reachable_data),
        reachable_size_(reachable_size) {}

  Element* data_;
  extent_t size_;
  stride_t increment_;
  ConstMemoryView backing_storage_;
  const void* reachable_data_;
  std::size_t reachable_size_;
};

template <typename Element>
class SparseBlasIndexedVectorView {
 public:
  using value_type = Element;
  using element_type = std::remove_const_t<Element>;

  static_assert(SparseBlasScalar<Element>,
                "SparseBlasIndexedVectorView requires a BLAS scalar");

  static Result<SparseBlasIndexedVectorView> Create(
      const index_t* indices, Element* values, nnz_t nonzeros,
      extent_t dense_extent, ConstMemoryView index_storage,
      ConstMemoryView value_storage) {
    if (nonzeros < 0 || dense_extent < 0 || nonzeros > dense_extent ||
        index_storage.space() != value_storage.space()) {
      return Status(ErrorCode::kInvalidArgument,
                    "A sparse indexed-vector descriptor is invalid");
    }
    auto index_bytes = CheckedByteCount(nonzeros, sizeof(index_t));
    auto value_bytes = CheckedByteCount(nonzeros, sizeof(element_type));
    if (!index_bytes.ok()) {
      return index_bytes.status();
    }
    if (!value_bytes.ok()) {
      return value_bytes.status();
    }
    auto index_bounds = internal_sparse_standard_blas::ValidateStorageBounds(
        indices, static_cast<stride_t>(nonzeros), sizeof(index_t),
        alignof(index_t), index_storage);
    if (!index_bounds.ok()) {
      return index_bounds.status();
    }
    auto value_bounds = internal_sparse_standard_blas::ValidateStorageBounds(
        values, static_cast<stride_t>(nonzeros), sizeof(element_type),
        alignof(element_type), value_storage);
    if (!value_bounds.ok()) {
      return value_bounds.status();
    }
    if (internal_sparse_standard_blas::Overlap(
            {index_bounds->data, index_bounds->size, index_storage.space()},
            {value_bounds->data, value_bounds->size, value_storage.space()})) {
      return Status(ErrorCode::kInvalidArgument,
                    "Sparse indexed-vector indices and values overlap");
    }
    bool canonical = false;
    if (index_storage.space() == MemorySpace::kHost) {
      index_t previous = -1;
      for (nnz_t position = 0; position < nonzeros; ++position) {
        const index_t index = indices[static_cast<std::size_t>(position)];
        if (index <= previous || index < 0 || index >= dense_extent) {
          return Status(ErrorCode::kIndex,
                        "Sparse indexed-vector indices must be sorted, "
                        "unique, zero-based, and in range");
        }
        previous = index;
      }
      canonical = true;
    }
    return SparseBlasIndexedVectorView(indices, values, nonzeros, dense_extent,
                                       index_storage, value_storage, canonical);
  }

  // Mutable-to-const views intentionally convert implicitly.
  // NOLINTNEXTLINE(google-explicit-constructor)
  operator SparseBlasIndexedVectorView<const element_type>() const noexcept
    requires(!std::is_const_v<Element>)
  {
    return SparseBlasIndexedVectorView<const element_type>(
        indices_, values_, nonzeros_, dense_extent_, index_storage_,
        value_storage_, canonical_structure_trusted_);
  }

  [[nodiscard]] const index_t* indices() const noexcept { return indices_; }
  [[nodiscard]] Element* values() const noexcept { return values_; }
  [[nodiscard]] nnz_t nnz() const noexcept { return nonzeros_; }
  [[nodiscard]] extent_t dense_extent() const noexcept { return dense_extent_; }
  [[nodiscard]] MemorySpace memory_space() const noexcept {
    return value_storage_.space();
  }
  [[nodiscard]] ConstMemoryView index_storage() const noexcept {
    return index_storage_;
  }
  [[nodiscard]] ConstMemoryView value_storage() const noexcept {
    return value_storage_;
  }
  [[nodiscard]] bool canonical_structure_trusted() const noexcept {
    return canonical_structure_trusted_;
  }

 private:
  template <typename>
  friend class SparseBlasIndexedVectorView;
  friend class internal_sparse_standard_blas::ProviderAccess;

  SparseBlasIndexedVectorView(const index_t* indices, Element* values,
                              nnz_t nonzeros, extent_t dense_extent,
                              ConstMemoryView index_storage,
                              ConstMemoryView value_storage,
                              bool canonical_structure_trusted) noexcept
      : indices_(indices),
        values_(values),
        nonzeros_(nonzeros),
        dense_extent_(dense_extent),
        index_storage_(index_storage),
        value_storage_(value_storage),
        canonical_structure_trusted_(canonical_structure_trusted) {}

  const index_t* indices_;
  Element* values_;
  nnz_t nonzeros_;
  extent_t dense_extent_;
  ConstMemoryView index_storage_;
  ConstMemoryView value_storage_;
  bool canonical_structure_trusted_;
};

namespace internal_sparse_standard_blas {

class ProviderAccess {
 public:
  template <typename Element>
  static SparseBlasIndexedVectorView<Element> MakeCanonicalIndexedVector(
      const index_t* indices, Element* values, nnz_t nonzeros,
      extent_t dense_extent, ConstMemoryView index_storage,
      ConstMemoryView value_storage) noexcept {
    return SparseBlasIndexedVectorView<Element>(indices, values, nonzeros,
                                                dense_extent, index_storage,
                                                value_storage, true);
  }
};

}  // namespace internal_sparse_standard_blas

template <typename Element>
class SparseBlasMatrixView {
 public:
  using value_type = Element;
  using element_type = std::remove_const_t<Element>;

  static_assert(SparseBlasScalar<Element>,
                "SparseBlasMatrixView requires a BLAS scalar");

  static Result<SparseBlasMatrixView> Create(Element* data, extent_t rows,
                                             extent_t columns,
                                             SparseBlasLayout layout,
                                             stride_t leading_dimension,
                                             ConstMemoryView backing_storage) {
    if (rows < 0 || columns < 0 ||
        (layout != SparseBlasLayout::kColumnMajor &&
         layout != SparseBlasLayout::kRowMajor)) {
      return Status(ErrorCode::kInvalidArgument,
                    "A sparse BLAS dense-matrix descriptor is invalid");
    }
    const extent_t contiguous =
        layout == SparseBlasLayout::kColumnMajor ? rows : columns;
    if (leading_dimension < std::max<stride_t>(1, contiguous)) {
      return Status(ErrorCode::kInvalidArgument,
                    "A sparse BLAS leading dimension is too small");
    }
    stride_t span = 0;
    if (rows != 0 && columns != 0) {
      const extent_t major =
          layout == SparseBlasLayout::kColumnMajor ? columns : rows;
      auto leading_span =
          CheckedMultiply<stride_t>(major - 1, leading_dimension);
      if (!leading_span.ok()) {
        return leading_span.status();
      }
      auto total = CheckedAdd<stride_t>(*leading_span, contiguous);
      if (!total.ok()) {
        return total.status();
      }
      span = *total;
    }
    auto bounds = internal_sparse_standard_blas::ValidateStorageBounds(
        data, span, sizeof(element_type), alignof(element_type),
        backing_storage);
    if (!bounds.ok()) {
      return bounds.status();
    }
    return SparseBlasMatrixView(data, rows, columns, layout, leading_dimension,
                                backing_storage, *bounds);
  }

  // Mutable-to-const views intentionally convert implicitly.
  // NOLINTNEXTLINE(google-explicit-constructor)
  operator SparseBlasMatrixView<const element_type>() const noexcept
    requires(!std::is_const_v<Element>)
  {
    return SparseBlasMatrixView<const element_type>(data_, rows_, columns_,
                                                    layout_, leading_dimension_,
                                                    backing_storage_, bounds_);
  }

  [[nodiscard]] Element* data() const noexcept { return data_; }
  [[nodiscard]] extent_t rows() const noexcept { return rows_; }
  [[nodiscard]] extent_t columns() const noexcept { return columns_; }
  [[nodiscard]] SparseBlasLayout layout() const noexcept { return layout_; }
  [[nodiscard]] stride_t leading_dimension() const noexcept {
    return leading_dimension_;
  }
  [[nodiscard]] MemorySpace memory_space() const noexcept {
    return backing_storage_.space();
  }
  [[nodiscard]] ConstMemoryView reachable_storage() const noexcept {
    return {bounds_.data, bounds_.size, backing_storage_.space()};
  }
  [[nodiscard]] Element& operator()(extent_t row, extent_t column) const {
    const stride_t offset = layout_ == SparseBlasLayout::kColumnMajor
                                ? column * leading_dimension_ + row
                                : row * leading_dimension_ + column;
    return data_[static_cast<std::size_t>(offset)];
  }

 private:
  template <typename>
  friend class SparseBlasMatrixView;

  SparseBlasMatrixView(
      Element* data, extent_t rows, extent_t columns, SparseBlasLayout layout,
      stride_t leading_dimension, ConstMemoryView backing_storage,
      internal_sparse_standard_blas::StorageBounds bounds) noexcept
      : data_(data),
        rows_(rows),
        columns_(columns),
        layout_(layout),
        leading_dimension_(leading_dimension),
        backing_storage_(backing_storage),
        bounds_(bounds) {}

  Element* data_;
  extent_t rows_;
  extent_t columns_;
  SparseBlasLayout layout_;
  stride_t leading_dimension_;
  ConstMemoryView backing_storage_;
  internal_sparse_standard_blas::StorageBounds bounds_;
};

template <SparseBlasScalar Element, SparseCompressedFormat Format>
class SparseBlasTriangularView {
 public:
  static Result<SparseBlasTriangularView> Create(
      CompressedSparseView<const Element, Format> matrix,
      SparseBlasTriangle triangle, SparseBlasDiagonal diagonal) {
    const Status triangle_status =
        internal_sparse_standard_blas::ValidateTriangle(triangle);
    if (!triangle_status.ok()) {
      return triangle_status;
    }
    const Status diagonal_status =
        internal_sparse_standard_blas::ValidateDiagonal(diagonal);
    if (!diagonal_status.ok()) {
      return diagonal_status;
    }
    if (matrix.memory_space() != MemorySpace::kHost ||
        !matrix.canonical_structure_trusted()) {
      return Status(ErrorCode::kMemoryAccess,
                    "A triangular sparse descriptor requires canonical host "
                    "storage");
    }
    if (matrix.rows() != matrix.columns()) {
      return Status(ErrorCode::kShape,
                    "A triangular sparse matrix must be square");
    }
    for (extent_t outer = 0;
         outer <
         internal_sparse_compressed::OuterExtent<Format>(matrix.extents());
         ++outer) {
      const nnz_t begin = matrix.outer_offsets()[outer];
      const nnz_t end = matrix.outer_offsets()[outer + 1];
      for (nnz_t position = begin; position < end; ++position) {
        const index_t inner = matrix.inner_indices()[position];
        const auto coordinate =
            internal_sparse_compressed::Coordinate<Format>(outer, inner);
        const bool outside = triangle == SparseBlasTriangle::kLower
                                 ? coordinate[1] > coordinate[0]
                                 : coordinate[0] > coordinate[1];
        if (outside) {
          return Status(ErrorCode::kInvalidArgument,
                        "Sparse triangular storage contains an entry outside "
                        "the declared triangle");
        }
      }
    }
    if (diagonal == SparseBlasDiagonal::kNonUnit) {
      for (extent_t row = 0; row < matrix.rows(); ++row) {
        bool found = false;
        Element value{};
        for (extent_t outer = 0;
             outer <
             internal_sparse_compressed::OuterExtent<Format>(matrix.extents());
             ++outer) {
          const nnz_t begin = matrix.outer_offsets()[outer];
          const nnz_t end = matrix.outer_offsets()[outer + 1];
          for (nnz_t position = begin; position < end; ++position) {
            const auto coordinate =
                internal_sparse_compressed::Coordinate<Format>(
                    outer, matrix.inner_indices()[position]);
            if (coordinate[0] == row && coordinate[1] == row) {
              found = true;
              value = matrix.values()[position];
            }
          }
        }
        if (!found || value == Element{}) {
          return Status(ErrorCode::kInvalidArgument,
                        "A non-unit sparse triangular matrix is singular");
        }
      }
    }
    return SparseBlasTriangularView(matrix, triangle, diagonal);
  }

  [[nodiscard]] CompressedSparseView<const Element, Format> matrix() const {
    return matrix_;
  }
  [[nodiscard]] SparseBlasTriangle triangle() const noexcept {
    return triangle_;
  }
  [[nodiscard]] SparseBlasDiagonal diagonal() const noexcept {
    return diagonal_;
  }

 private:
  SparseBlasTriangularView(CompressedSparseView<const Element, Format> matrix,
                           SparseBlasTriangle triangle,
                           SparseBlasDiagonal diagonal) noexcept
      : matrix_(matrix), triangle_(triangle), diagonal_(diagonal) {}

  CompressedSparseView<const Element, Format> matrix_;
  SparseBlasTriangle triangle_;
  SparseBlasDiagonal diagonal_;
};

namespace internal_sparse_standard_blas {

template <SparseBlasScalar Element>
Status ValidateHostVector(SparseBlasVectorView<Element> vector) {
  if (vector.memory_space() != MemorySpace::kHost) {
    return Status(ErrorCode::kMemoryAccess,
                  "Sparse BLAS CPU vectors require host memory");
  }
  return Status::Ok();
}

template <SparseBlasScalar Element>
Status ValidateHostIndexed(SparseBlasIndexedVectorView<Element> vector) {
  if (vector.memory_space() != MemorySpace::kHost ||
      !vector.canonical_structure_trusted()) {
    return Status(ErrorCode::kMemoryAccess,
                  "Sparse BLAS CPU indexed vectors require canonical host "
                  "memory");
  }
  return Status::Ok();
}

template <SparseBlasScalar Element, SparseCompressedFormat Format,
          typename Function>
void ForEachStored(CompressedSparseView<const Element, Format> matrix,
                   Function function) {
  const extent_t outer_extent =
      internal_sparse_compressed::OuterExtent<Format>(matrix.extents());
  for (extent_t outer = 0; outer < outer_extent; ++outer) {
    const nnz_t begin = matrix.outer_offsets()[outer];
    const nnz_t end = matrix.outer_offsets()[outer + 1];
    for (nnz_t position = begin; position < end; ++position) {
      const auto coordinate = internal_sparse_compressed::Coordinate<Format>(
          outer, matrix.inner_indices()[position]);
      function(coordinate[0], coordinate[1], matrix.values()[position]);
    }
  }
}

template <SparseBlasScalar Element, SparseCompressedFormat Format>
Element OpValue(CompressedSparseView<const Element, Format> matrix,
                SparseBlasTranspose transpose, extent_t row, extent_t column) {
  Element result{};
  const bool exchanged = transpose != SparseBlasTranspose::kNone;
  const extent_t source_row = exchanged ? column : row;
  const extent_t source_column = exchanged ? row : column;
  ForEachStored(matrix, [&](extent_t candidate_row, extent_t candidate_column,
                            Element value) {
    if (candidate_row == source_row && candidate_column == source_column) {
      result = MaybeConjugate(
          value, transpose == SparseBlasTranspose::kConjugateTranspose);
    }
  });
  return result;
}

template <SparseBlasScalar Element>
Element& VectorAt(SparseBlasVectorView<Element> vector, extent_t index) {
  return vector.data()[index * vector.increment()];
}

template <SparseBlasScalar Element>
const Element& VectorAt(SparseBlasVectorView<const Element> vector,
                        extent_t index) {
  return vector.data()[index * vector.increment()];
}

template <SparseBlasScalar Element, SparseCompressedFormat Format>
Status ValidateCompressedHost(
    CompressedSparseView<const Element, Format> matrix) {
  if (matrix.memory_space() != MemorySpace::kHost ||
      !matrix.canonical_structure_trusted()) {
    return Status(ErrorCode::kMemoryAccess,
                  "Sparse BLAS CPU matrices require canonical host storage");
  }
  return Status::Ok();
}

template <SparseBlasScalar Element, SparseCompressedFormat Format>
bool MatrixOverlaps(CompressedSparseView<const Element, Format> matrix,
                    ConstMemoryView storage) {
  const extent_t outer_extent =
      internal_sparse_compressed::OuterExtent<Format>(matrix.extents());
  auto offset_count = CheckedAdd(outer_extent, extent_t{1});
  auto offset_bytes = offset_count.ok()
                          ? CheckedByteCount(*offset_count, sizeof(nnz_t))
                          : Result<std::size_t>(offset_count.status());
  auto index_bytes = CheckedByteCount(matrix.nnz(), sizeof(index_t));
  auto value_bytes = CheckedByteCount(matrix.nnz(), sizeof(Element));
  if (!offset_bytes.ok() || !index_bytes.ok() || !value_bytes.ok()) {
    return true;
  }
  return Overlap({matrix.outer_offsets(), *offset_bytes, matrix.memory_space()},
                 storage) ||
         Overlap({matrix.inner_indices(), *index_bytes, matrix.memory_space()},
                 storage) ||
         Overlap({matrix.values(), *value_bytes, matrix.memory_space()},
                 storage);
}

}  // namespace internal_sparse_standard_blas

template <SparseBlasScalar Element>
Result<Element> SparseDot(const ExecutionContext& context,
                          SparseBlasConjugation conjugation,
                          SparseBlasIndexedVectorView<const Element> sparse,
                          SparseBlasVectorView<const Element> dense) {
  Status enum_status =
      internal_sparse_standard_blas::ValidateConjugation(conjugation);
  if (!enum_status.ok()) {
    return enum_status;
  }
  Status status = internal_sparse_standard_blas::ValidateSerial(context);
  if (!status.ok()) {
    return status;
  }
  status = internal_sparse_standard_blas::ValidateHostIndexed(sparse);
  if (!status.ok()) {
    return status;
  }
  status = internal_sparse_standard_blas::ValidateHostVector(dense);
  if (!status.ok()) {
    return status;
  }
  if (dense.size() != sparse.dense_extent()) {
    return Status(ErrorCode::kShape, "Sparse dot vector extents do not match");
  }
  Element result{};
  for (nnz_t position = 0; position < sparse.nnz(); ++position) {
    const Element value = internal_sparse_standard_blas::MaybeConjugate(
        sparse.values()[position],
        conjugation == SparseBlasConjugation::kConjugated);
    result += value * internal_sparse_standard_blas::VectorAt(
                          dense, sparse.indices()[position]);
  }
  return result;
}

template <SparseBlasScalar Element>
Status SparseAxpy(const ExecutionContext& context, Element alpha,
                  SparseBlasIndexedVectorView<const Element> sparse,
                  SparseBlasVectorView<Element> dense) {
  Status status = internal_sparse_standard_blas::ValidateSerial(context);
  if (!status.ok()) {
    return status;
  }
  status = internal_sparse_standard_blas::ValidateHostIndexed(sparse);
  if (!status.ok()) {
    return status;
  }
  status = internal_sparse_standard_blas::ValidateHostVector(dense);
  if (!status.ok()) {
    return status;
  }
  if (dense.size() != sparse.dense_extent()) {
    return Status(ErrorCode::kShape, "Sparse axpy vector extents do not match");
  }
  if (internal_sparse_standard_blas::Overlap(sparse.value_storage(),
                                             dense.reachable_storage()) ||
      internal_sparse_standard_blas::Overlap(sparse.index_storage(),
                                             dense.reachable_storage())) {
    return Status(ErrorCode::kInvalidArgument,
                  "Sparse axpy writable operands overlap");
  }
  if (alpha == Element{}) {
    return Status::Ok();
  }
  for (nnz_t position = 0; position < sparse.nnz(); ++position) {
    internal_sparse_standard_blas::VectorAt(
        dense, sparse.indices()[position]) += alpha * sparse.values()[position];
  }
  return Status::Ok();
}

template <SparseBlasScalar Element>
Status SparseGather(const ExecutionContext& context,
                    SparseBlasVectorView<const Element> dense,
                    SparseBlasIndexedVectorView<Element> sparse) {
  Status status = internal_sparse_standard_blas::ValidateSerial(context);
  if (!status.ok()) {
    return status;
  }
  status = internal_sparse_standard_blas::ValidateHostVector(dense);
  if (!status.ok()) {
    return status;
  }
  status = internal_sparse_standard_blas::ValidateHostIndexed(sparse);
  if (!status.ok()) {
    return status;
  }
  if (dense.size() != sparse.dense_extent()) {
    return Status(ErrorCode::kShape,
                  "Sparse gather vector extents do not match");
  }
  if (internal_sparse_standard_blas::Overlap(dense.reachable_storage(),
                                             sparse.value_storage()) ||
      internal_sparse_standard_blas::Overlap(dense.reachable_storage(),
                                             sparse.index_storage())) {
    return Status(ErrorCode::kInvalidArgument,
                  "Sparse gather writable operands overlap");
  }
  for (nnz_t position = 0; position < sparse.nnz(); ++position) {
    sparse.values()[position] = internal_sparse_standard_blas::VectorAt(
        dense, sparse.indices()[position]);
  }
  return Status::Ok();
}

template <SparseBlasScalar Element>
Status SparseGatherZero(const ExecutionContext& context,
                        SparseBlasVectorView<Element> dense,
                        SparseBlasIndexedVectorView<Element> sparse) {
  Status status = internal_sparse_standard_blas::ValidateSerial(context);
  if (!status.ok()) {
    return status;
  }
  status = internal_sparse_standard_blas::ValidateHostVector(dense);
  if (!status.ok()) {
    return status;
  }
  status = internal_sparse_standard_blas::ValidateHostIndexed(sparse);
  if (!status.ok()) {
    return status;
  }
  if (dense.size() != sparse.dense_extent()) {
    return Status(ErrorCode::kShape,
                  "Sparse gather-zero vector extents do not match");
  }
  if (internal_sparse_standard_blas::Overlap(dense.reachable_storage(),
                                             sparse.value_storage()) ||
      internal_sparse_standard_blas::Overlap(dense.reachable_storage(),
                                             sparse.index_storage())) {
    return Status(ErrorCode::kInvalidArgument,
                  "Sparse gather-zero writable operands overlap");
  }
  for (nnz_t position = 0; position < sparse.nnz(); ++position) {
    Element& value = internal_sparse_standard_blas::VectorAt(
        dense, sparse.indices()[position]);
    sparse.values()[position] = value;
    value = Element{};
  }
  return Status::Ok();
}

template <SparseBlasScalar Element>
Status SparseScatter(const ExecutionContext& context,
                     SparseBlasIndexedVectorView<const Element> sparse,
                     SparseBlasVectorView<Element> dense) {
  Status status = internal_sparse_standard_blas::ValidateSerial(context);
  if (!status.ok()) {
    return status;
  }
  status = internal_sparse_standard_blas::ValidateHostIndexed(sparse);
  if (!status.ok()) {
    return status;
  }
  status = internal_sparse_standard_blas::ValidateHostVector(dense);
  if (!status.ok()) {
    return status;
  }
  if (dense.size() != sparse.dense_extent()) {
    return Status(ErrorCode::kShape,
                  "Sparse scatter vector extents do not match");
  }
  if (internal_sparse_standard_blas::Overlap(sparse.value_storage(),
                                             dense.reachable_storage()) ||
      internal_sparse_standard_blas::Overlap(sparse.index_storage(),
                                             dense.reachable_storage())) {
    return Status(ErrorCode::kInvalidArgument,
                  "Sparse scatter writable operands overlap");
  }
  for (nnz_t position = 0; position < sparse.nnz(); ++position) {
    internal_sparse_standard_blas::VectorAt(dense, sparse.indices()[position]) =
        sparse.values()[position];
  }
  return Status::Ok();
}

template <SparseBlasScalar Element, SparseCompressedFormat Format>
Status Spmv(const ExecutionContext& context, SparseBlasTranspose transpose,
            Element alpha, CompressedSparseView<const Element, Format> matrix,
            SparseBlasVectorView<const Element> input,
            SparseBlasVectorView<Element> output) {
  Status enum_status =
      internal_sparse_standard_blas::ValidateTranspose(transpose);
  if (!enum_status.ok()) {
    return enum_status;
  }
  Status status = internal_sparse_standard_blas::ValidateSerial(context);
  if (!status.ok()) {
    return status;
  }
  status = internal_sparse_standard_blas::ValidateCompressedHost(matrix);
  if (!status.ok()) {
    return status;
  }
  status = internal_sparse_standard_blas::ValidateHostVector(input);
  if (!status.ok()) {
    return status;
  }
  status = internal_sparse_standard_blas::ValidateHostVector(output);
  if (!status.ok()) {
    return status;
  }
  const extent_t input_extent = transpose == SparseBlasTranspose::kNone
                                    ? matrix.columns()
                                    : matrix.rows();
  const extent_t output_extent = transpose == SparseBlasTranspose::kNone
                                     ? matrix.rows()
                                     : matrix.columns();
  if (input.size() != input_extent || output.size() != output_extent) {
    return Status(ErrorCode::kShape,
                  "Sparse matrix-vector operand extents do not match");
  }
  if (internal_sparse_standard_blas::Overlap(input.reachable_storage(),
                                             output.reachable_storage()) ||
      internal_sparse_standard_blas::MatrixOverlaps(
          matrix, output.reachable_storage())) {
    return Status(ErrorCode::kInvalidArgument,
                  "Sparse matrix-vector input and output overlap");
  }
  if (alpha == Element{}) {
    return Status::Ok();
  }
  internal_sparse_standard_blas::ForEachStored(
      matrix, [&](extent_t row, extent_t column, Element value) {
        extent_t output_index = row;
        extent_t input_index = column;
        if (transpose != SparseBlasTranspose::kNone) {
          std::swap(output_index, input_index);
          value = internal_sparse_standard_blas::MaybeConjugate(
              value, transpose == SparseBlasTranspose::kConjugateTranspose);
        }
        internal_sparse_standard_blas::VectorAt(output, output_index) +=
            alpha * value *
            internal_sparse_standard_blas::VectorAt(input, input_index);
      });
  return Status::Ok();
}

template <SparseBlasScalar Element, SparseCompressedFormat Format>
Status Spmm(const ExecutionContext& context, SparseBlasTranspose transpose,
            Element alpha, CompressedSparseView<const Element, Format> matrix,
            SparseBlasMatrixView<const Element> input,
            SparseBlasMatrixView<Element> output) {
  Status enum_status =
      internal_sparse_standard_blas::ValidateTranspose(transpose);
  if (!enum_status.ok()) {
    return enum_status;
  }
  Status status = internal_sparse_standard_blas::ValidateSerial(context);
  if (!status.ok()) {
    return status;
  }
  status = internal_sparse_standard_blas::ValidateCompressedHost(matrix);
  if (!status.ok()) {
    return status;
  }
  if (input.memory_space() != MemorySpace::kHost ||
      output.memory_space() != MemorySpace::kHost) {
    return Status(ErrorCode::kMemoryAccess,
                  "Sparse matrix-matrix CPU operands require host memory");
  }
  const extent_t inner = transpose == SparseBlasTranspose::kNone
                             ? matrix.columns()
                             : matrix.rows();
  const extent_t rows = transpose == SparseBlasTranspose::kNone
                            ? matrix.rows()
                            : matrix.columns();
  if (input.rows() != inner || output.rows() != rows ||
      input.columns() != output.columns()) {
    return Status(ErrorCode::kShape,
                  "Sparse matrix-matrix operand extents do not match");
  }
  if (internal_sparse_standard_blas::Overlap(input.reachable_storage(),
                                             output.reachable_storage()) ||
      internal_sparse_standard_blas::MatrixOverlaps(
          matrix, output.reachable_storage())) {
    return Status(ErrorCode::kInvalidArgument,
                  "Sparse matrix-matrix input and output overlap");
  }
  if (alpha == Element{}) {
    return Status::Ok();
  }
  internal_sparse_standard_blas::ForEachStored(
      matrix, [&](extent_t row, extent_t column, Element value) {
        extent_t output_row = row;
        extent_t input_row = column;
        if (transpose != SparseBlasTranspose::kNone) {
          std::swap(output_row, input_row);
          value = internal_sparse_standard_blas::MaybeConjugate(
              value, transpose == SparseBlasTranspose::kConjugateTranspose);
        }
        for (extent_t rhs = 0; rhs < input.columns(); ++rhs) {
          output(output_row, rhs) += alpha * value * input(input_row, rhs);
        }
      });
  return Status::Ok();
}

template <SparseBlasScalar Element, SparseCompressedFormat Format>
Status SparseTriangularSolve(
    const ExecutionContext& context, SparseBlasTranspose transpose,
    Element alpha, SparseBlasTriangularView<Element, Format> triangular,
    SparseBlasVectorView<Element> right_hand_side) {
  Status enum_status =
      internal_sparse_standard_blas::ValidateTranspose(transpose);
  if (!enum_status.ok()) {
    return enum_status;
  }
  Status status = internal_sparse_standard_blas::ValidateSerial(context);
  if (!status.ok()) {
    return status;
  }
  status = internal_sparse_standard_blas::ValidateHostVector(right_hand_side);
  if (!status.ok()) {
    return status;
  }
  const auto matrix = triangular.matrix();
  if (right_hand_side.size() != matrix.rows()) {
    return Status(ErrorCode::kShape,
                  "Sparse triangular solve vector extent does not match");
  }
  if (internal_sparse_standard_blas::MatrixOverlaps(
          matrix, right_hand_side.reachable_storage())) {
    return Status(ErrorCode::kInvalidArgument,
                  "Sparse triangular right-hand side overlaps the matrix");
  }
  if (alpha == Element{}) {
    for (extent_t row = 0; row < matrix.rows(); ++row) {
      internal_sparse_standard_blas::VectorAt(right_hand_side, row) = Element{};
    }
    return Status::Ok();
  }
  const bool transposed = transpose != SparseBlasTranspose::kNone;
  const bool lower = transposed
                         ? triangular.triangle() == SparseBlasTriangle::kUpper
                         : triangular.triangle() == SparseBlasTriangle::kLower;
  for (extent_t step = 0; step < matrix.rows(); ++step) {
    const extent_t row = lower ? step : matrix.rows() - 1 - step;
    Element value =
        alpha * internal_sparse_standard_blas::VectorAt(right_hand_side, row);
    for (extent_t column = 0; column < matrix.columns(); ++column) {
      if ((lower && column >= row) || (!lower && column <= row)) {
        continue;
      }
      value -= internal_sparse_standard_blas::OpValue(matrix, transpose, row,
                                                      column) *
               internal_sparse_standard_blas::VectorAt(right_hand_side, column);
    }
    if (triangular.diagonal() == SparseBlasDiagonal::kNonUnit) {
      value /=
          internal_sparse_standard_blas::OpValue(matrix, transpose, row, row);
    }
    internal_sparse_standard_blas::VectorAt(right_hand_side, row) = value;
  }
  return Status::Ok();
}

template <SparseBlasScalar Element, SparseCompressedFormat Format>
Status SparseTriangularSolveMultiple(
    const ExecutionContext& context, SparseBlasTranspose transpose,
    Element alpha, SparseBlasTriangularView<Element, Format> triangular,
    SparseBlasMatrixView<Element> right_hand_sides) {
  Status enum_status =
      internal_sparse_standard_blas::ValidateTranspose(transpose);
  if (!enum_status.ok()) {
    return enum_status;
  }
  Status status = internal_sparse_standard_blas::ValidateSerial(context);
  if (!status.ok()) {
    return status;
  }
  const auto matrix = triangular.matrix();
  if (right_hand_sides.memory_space() != MemorySpace::kHost) {
    return Status(ErrorCode::kMemoryAccess,
                  "Sparse triangular solve requires host right-hand sides");
  }
  if (right_hand_sides.rows() != matrix.rows()) {
    return Status(ErrorCode::kShape,
                  "Sparse triangular solve matrix extent does not match");
  }
  if (internal_sparse_standard_blas::MatrixOverlaps(
          matrix, right_hand_sides.reachable_storage())) {
    return Status(ErrorCode::kInvalidArgument,
                  "Sparse triangular right-hand sides overlap the matrix");
  }
  if (alpha == Element{}) {
    for (extent_t rhs = 0; rhs < right_hand_sides.columns(); ++rhs) {
      for (extent_t row = 0; row < matrix.rows(); ++row) {
        right_hand_sides(row, rhs) = Element{};
      }
    }
    return Status::Ok();
  }
  const bool transposed = transpose != SparseBlasTranspose::kNone;
  const bool lower = transposed
                         ? triangular.triangle() == SparseBlasTriangle::kUpper
                         : triangular.triangle() == SparseBlasTriangle::kLower;
  for (extent_t rhs = 0; rhs < right_hand_sides.columns(); ++rhs) {
    for (extent_t step = 0; step < matrix.rows(); ++step) {
      const extent_t row = lower ? step : matrix.rows() - 1 - step;
      Element value = alpha * right_hand_sides(row, rhs);
      for (extent_t column = 0; column < matrix.columns(); ++column) {
        if ((lower && column >= row) || (!lower && column <= row)) {
          continue;
        }
        value -= internal_sparse_standard_blas::OpValue(matrix, transpose, row,
                                                        column) *
                 // The stored matrix column selects a right-hand-side row.
                 // NOLINTNEXTLINE(readability-suspicious-call-argument)
                 right_hand_sides(column, rhs);
      }
      if (triangular.diagonal() == SparseBlasDiagonal::kNonUnit) {
        value /=
            internal_sparse_standard_blas::OpValue(matrix, transpose, row, row);
      }
      right_hand_sides(row, rhs) = value;
    }
  }
  return Status::Ok();
}

}  // namespace asc

namespace asc {
namespace internal_sparse_blas {

template <typename Element>
struct ReadableVectorDescriptor {
  const void* object;
  Element (*read)(const void*, index_t);
};

template <typename Element>
struct WritableVectorDescriptor {
  void* object;
  Element (*read)(const void*, index_t);
  void (*write)(void*, index_t, Element);
};

ASC_SPARSE_EXPORT Status SpmvReference(float alpha, CsrView<const float> matrix,
                                       ReadableVectorDescriptor<float> input,
                                       float beta,
                                       WritableVectorDescriptor<float> output);

ASC_SPARSE_EXPORT Status SpmvReference(double alpha,
                                       CsrView<const double> matrix,
                                       ReadableVectorDescriptor<double> input,
                                       double beta,
                                       WritableVectorDescriptor<double> output);

template <typename Element>
concept SpmvElement =
    std::same_as<Element, float> || std::same_as<Element, double>;

template <typename Element>
bool MatrixStructureMayOverlap(CsrView<const Element> matrix,
                               ExpressionAliasMetadata output_alias) noexcept {
  if (!output_alias.has_byte_span() || output_alias.size() == 0) {
    return false;
  }
  auto offset_count = internal_sparse_compressed::OffsetCount(matrix.rows());
  auto index_bytes = CheckedByteCount(matrix.nnz(), sizeof(index_t));
  if (!offset_count.ok() || !index_bytes.ok()) {
    return true;
  }
  auto offset_bytes = CheckedMultiply(*offset_count, sizeof(nnz_t));
  if (!offset_bytes.ok()) {
    return true;
  }
  const ExpressionAliasMetadata offset_alias(
      matrix.outer_offsets(), matrix.outer_offsets(), *offset_bytes);
  const ExpressionAliasMetadata index_alias(
      matrix.inner_indices(), matrix.inner_indices(), *index_bytes);
  return internal_expression_writable::ByteSpansOverlap(offset_alias,
                                                        output_alias) ||
         internal_expression_writable::ByteSpansOverlap(index_alias,
                                                        output_alias);
}

template <typename Element, PlacedReadableExpression Input,
          WritableExpression Output>
  requires(SpmvElement<Element> && kExpressionRank<Input> == 1 &&
           kExpressionRank<Output> == 1 &&
           std::same_as<ExpressionValue<Input>, Element> &&
           std::same_as<ExpressionValue<Output>, Element>)
Status ValidateSpmv(const ExecutionContext& context, Element /*alpha*/,
                    CsrView<const Element> matrix, const Input& input,
                    Element /*beta*/, const Output& output) {
  Status context_status = internal_sparse_coordinate::ValidateSerialHost(
      context, "Sparse SpMV requires serial execution");
  if (!context_status.ok()) {
    return context_status;
  }
  Status matrix_status = ValidateExpressionAccess(context, matrix);
  if (!matrix_status.ok()) {
    return matrix_status;
  }
  Status input_status = ValidateExpressionAccess(context, input);
  if (!input_status.ok()) {
    return input_status;
  }
  Status output_status = ValidateWritableExpressionAccess(context, output);
  if (!output_status.ok()) {
    return output_status;
  }
  if (ExpressionSpace(input) != MemorySpace::kHost ||
      ExpressionSpace(output) != MemorySpace::kHost ||
      matrix.memory_space() != MemorySpace::kHost) {
    return Status(ErrorCode::kMemoryAccess,
                  "Sparse SpMV requires host operands");
  }
  if (!WritableExpressionIsUnique(output)) {
    return Status(ErrorCode::kInvalidArgument,
                  "Sparse SpMV requires a unique output mapping");
  }
  const auto input_shape = ExpressionShape(input);
  const auto output_shape = WritableExpressionShape(output);
  if (input_shape[0] != matrix.columns() || output_shape[0] != matrix.rows()) {
    return Status(ErrorCode::kShape,
                  "Sparse SpMV vector lengths do not match the matrix");
  }
  const ExpressionAliasMetadata output_alias = WritableExpressionAlias(output);
  if (ExpressionMayOverlap(input, output_alias) ||
      ExpressionMayOverlap(matrix, output_alias) ||
      internal_sparse_blas::MatrixStructureMayOverlap(matrix, output_alias)) {
    return Status(ErrorCode::kInvalidArgument,
                  "Sparse SpMV rejects output operand overlap");
  }
  return Status::Ok();
}

}  // namespace internal_sparse_blas

template <SparseViewElement MatrixElement, PlacedReadableExpression Input,
          WritableExpression Output>
  requires(
      internal_sparse_blas::SpmvElement<std::remove_const_t<MatrixElement>> &&
      kExpressionRank<Input> == 1 && kExpressionRank<Output> == 1 &&
      std::same_as<ExpressionValue<Input>,
                   std::remove_const_t<MatrixElement>> &&
      std::same_as<ExpressionValue<Output>, std::remove_const_t<MatrixElement>>)
Status Spmv(const ExecutionContext& context,
            std::remove_const_t<MatrixElement> alpha,
            CsrView<MatrixElement> matrix, const Input& input,
            std::remove_const_t<MatrixElement> beta, Output& output) {
  using Element = std::remove_const_t<MatrixElement>;
  const CsrView<const Element> const_matrix = matrix;
  Status validation_status = internal_sparse_blas::ValidateSpmv(
      context, alpha, const_matrix, input, beta, output);
  if (!validation_status.ok()) {
    return validation_status;
  }

  const internal_sparse_blas::ReadableVectorDescriptor<Element>
      input_descriptor{
          .object = &input,
          .read =
              [](const void* object, index_t index) {
                const auto& operand = *static_cast<const Input*>(object);
                const std::array<index_t, 1> coordinate{index};
                return ExpressionRead(operand,
                                      std::span<const index_t, 1>(coordinate));
              },
      };
  internal_sparse_blas::WritableVectorDescriptor<Element> output_descriptor{
      .object = &output,
      .read =
          [](const void* object, index_t index) {
            const auto& operand = *static_cast<const Output*>(object);
            const std::array<index_t, 1> coordinate{index};
            return ExpressionRead(operand,
                                  std::span<const index_t, 1>(coordinate));
          },
      .write =
          [](void* object, index_t index, Element value) {
            auto& operand = *static_cast<Output*>(object);
            const std::array<index_t, 1> coordinate{index};
            WriteExpression(operand, std::span<const index_t, 1>(coordinate),
                            value);
          },
  };
  return internal_sparse_blas::SpmvReference(
      alpha, const_matrix, input_descriptor, beta, output_descriptor);
}

}  // namespace asc

#endif  // ASC_SPARSE_BLAS_H_
