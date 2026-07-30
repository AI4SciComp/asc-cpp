#ifndef ASC_SPARSE_COMPRESSED_H_
#define ASC_SPARSE_COMPRESSED_H_

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <span>
#include <type_traits>
#include <utility>

#include "asc/core/contracts.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/expression/expression.h"
#include "asc/expression/writable.h"
#include "asc/sparse/coordinate.h"

namespace asc {
namespace internal_sparse_compressed {

class ProviderAccess;

template <SparseCompressedFormat Format>
constexpr extent_t OuterExtent(std::span<const extent_t, 2> shape) noexcept {
  if constexpr (Format == SparseCompressedFormat::kCsr) {
    return shape[0];
  } else {
    return shape[1];
  }
}

template <SparseCompressedFormat Format>
constexpr extent_t InnerExtent(std::span<const extent_t, 2> shape) noexcept {
  if constexpr (Format == SparseCompressedFormat::kCsr) {
    return shape[1];
  } else {
    return shape[0];
  }
}

template <SparseCompressedFormat Format>
constexpr std::array<index_t, 2> Coordinate(extent_t outer,
                                            index_t inner) noexcept {
  if constexpr (Format == SparseCompressedFormat::kCsr) {
    return {outer, inner};
  } else {
    return {inner, outer};
  }
}

inline Result<std::size_t> OffsetCount(extent_t outer_extent) {
  auto count = CheckedAdd(outer_extent, extent_t{1});
  if (!count.ok()) {
    return count.status();
  }
  return CheckedCast<std::size_t>(*count);
}

}  // namespace internal_sparse_compressed

template <SparseViewElement Element, SparseCompressedFormat Format>
class CompressedSparseView {
 public:
  static_assert(Format == SparseCompressedFormat::kCsr ||
                    Format == SparseCompressedFormat::kCsc,
                "Compressed sparse format must be CSR or CSC");
  using element_type = Element;
  using value_type = std::remove_const_t<Element>;
  static constexpr SparseCompressedFormat kFormat = Format;
  static constexpr std::size_t kRank = 2;

  static Result<CompressedSparseView> Create(const nnz_t* outer_offsets,
                                             const index_t* inner_indices,
                                             Element* values,
                                             std::span<const extent_t, 2> shape,
                                             nnz_t nonzeros,
                                             MemorySpace memory_space) {
    auto logical_size = internal_sparse_coordinate::ValidateShape(shape);
    if (!logical_size.ok()) {
      return logical_size.status();
    }
    if (nonzeros < 0 || nonzeros > *logical_size) {
      return Status(ErrorCode::kShape,
                    "Sparse NNZ is incompatible with compressed shape");
    }
    auto offset_count = internal_sparse_compressed::OffsetCount(
        internal_sparse_compressed::OuterExtent<Format>(shape));
    if (!offset_count.ok()) {
      return offset_count.status();
    }
    auto offset_bytes = CheckedMultiply(*offset_count, sizeof(nnz_t));
    if (!offset_bytes.ok()) {
      return offset_bytes.status();
    }
    auto index_bytes = CheckedByteCount(nonzeros, sizeof(index_t));
    if (!index_bytes.ok()) {
      return index_bytes.status();
    }
    auto value_bytes = CheckedByteCount(nonzeros, sizeof(value_type));
    if (!value_bytes.ok()) {
      return value_bytes.status();
    }
    if (outer_offsets == nullptr ||
        (*index_bytes != 0 && inner_indices == nullptr) ||
        (*value_bytes != 0 && values == nullptr)) {
      return Status(ErrorCode::kInvalidArgument,
                    "Compressed sparse storage pointers are incomplete");
    }
    Status offset_span_status = internal_sparse_coordinate::ValidateAddressSpan(
        outer_offsets, *offset_bytes);
    if (!offset_span_status.ok()) {
      return offset_span_status;
    }
    Status index_span_status = internal_sparse_coordinate::ValidateAddressSpan(
        inner_indices, *index_bytes);
    if (!index_span_status.ok()) {
      return index_span_status;
    }
    Status value_span_status =
        internal_sparse_coordinate::ValidateAddressSpan(values, *value_bytes);
    if (!value_span_status.ok()) {
      return value_span_status;
    }
    if (internal_sparse_coordinate::ByteSpansOverlap(
            outer_offsets, *offset_bytes, inner_indices, *index_bytes) ||
        internal_sparse_coordinate::ByteSpansOverlap(
            outer_offsets, *offset_bytes, values, *value_bytes) ||
        internal_sparse_coordinate::ByteSpansOverlap(
            inner_indices, *index_bytes, values, *value_bytes)) {
      return Status(ErrorCode::kInvalidArgument,
                    "Compressed sparse storage spans cannot overlap");
    }
    std::array<extent_t, 2> copied_shape{shape[0], shape[1]};
    CompressedSparseView result(outer_offsets, inner_indices, values,
                                copied_shape, nonzeros, memory_space, false);
    if (memory_space == MemorySpace::kHost) {
      Status structure_status = result.ValidateCanonicalStructure();
      if (!structure_status.ok()) {
        return structure_status;
      }
      result.canonical_structure_trusted_ = true;
    }
    return result;
  }

  static Result<CompressedSparseView> Create(
      std::span<const nnz_t> outer_offsets,
      std::span<const index_t> inner_indices, std::span<Element> values,
      std::span<const extent_t, 2> shape, MemorySpace memory_space) {
    if (inner_indices.size() != values.size()) {
      return Status(ErrorCode::kShape,
                    "Compressed sparse indices and values differ in size");
    }
    auto nonzeros = CheckedCast<nnz_t>(values.size());
    if (!nonzeros.ok()) {
      return nonzeros.status();
    }
    auto expected_offsets = internal_sparse_compressed::OffsetCount(
        internal_sparse_compressed::OuterExtent<Format>(shape));
    if (!expected_offsets.ok()) {
      return expected_offsets.status();
    }
    if (outer_offsets.size() != *expected_offsets) {
      return Status(ErrorCode::kShape,
                    "Compressed sparse outer-offset count is incorrect");
    }
    return Create(outer_offsets.data(), inner_indices.data(), values.data(),
                  shape, *nonzeros, memory_space);
  }

  template <typename OtherElement>
    requires(std::is_const_v<Element> &&
             std::same_as<std::remove_const_t<OtherElement>, value_type> &&
             !std::is_const_v<OtherElement>)
  // Mutable-to-const views intentionally convert implicitly.
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr CompressedSparseView(
      const CompressedSparseView<OtherElement, Format>& other) noexcept
      : outer_offsets_(other.outer_offsets()),
        inner_indices_(other.inner_indices()),
        values_(other.values()),
        shape_(other.shape()),
        nonzeros_(other.nnz()),
        memory_space_(other.memory_space()),
        canonical_structure_trusted_(other.canonical_structure_trusted()) {}

  [[nodiscard]] static constexpr rank_t rank() noexcept { return 2; }

  [[nodiscard]] static constexpr SparseCompressedFormat format() noexcept {
    return Format;
  }

  [[nodiscard]] constexpr const std::array<extent_t, 2>& shape()
      const noexcept {
    return shape_;
  }

  [[nodiscard]] constexpr std::span<const extent_t, 2> extents()
      const noexcept {
    return shape_;
  }

  [[nodiscard]] constexpr extent_t rows() const noexcept { return shape_[0]; }

  [[nodiscard]] constexpr extent_t columns() const noexcept {
    return shape_[1];
  }

  [[nodiscard]] constexpr nnz_t nnz() const noexcept { return nonzeros_; }

  [[nodiscard]] constexpr const nnz_t* outer_offsets() const noexcept {
    return outer_offsets_;
  }

  [[nodiscard]] constexpr const index_t* inner_indices() const noexcept {
    return inner_indices_;
  }

  [[nodiscard]] constexpr Element* values() const noexcept { return values_; }

  [[nodiscard]] constexpr MemorySpace memory_space() const noexcept {
    return memory_space_;
  }

  [[nodiscard]] constexpr bool canonical_structure_trusted() const noexcept {
    return canonical_structure_trusted_;
  }

  template <SparseViewElement ReboundElement>
    requires std::same_as<std::remove_const_t<ReboundElement>, value_type>
  [[nodiscard]]
  Result<CompressedSparseView<ReboundElement, Format>> RebindValues(
      ReboundElement* values) const {
    auto value_bytes = CheckedByteCount(nonzeros_, sizeof(value_type));
    if (!value_bytes.ok()) {
      return value_bytes.status();
    }
    Status span_status =
        internal_sparse_coordinate::ValidateAddressSpan(values, *value_bytes);
    if (!span_status.ok()) {
      return span_status;
    }
    auto offset_count = internal_sparse_compressed::OffsetCount(
        internal_sparse_compressed::OuterExtent<Format>(extents()));
    if (!offset_count.ok()) {
      return offset_count.status();
    }
    auto offset_bytes = CheckedMultiply(*offset_count, sizeof(nnz_t));
    auto index_bytes = CheckedByteCount(nonzeros_, sizeof(index_t));
    if (!offset_bytes.ok()) {
      return offset_bytes.status();
    }
    if (!index_bytes.ok()) {
      return index_bytes.status();
    }
    if (internal_sparse_coordinate::ByteSpansOverlap(
            outer_offsets_, *offset_bytes, values, *value_bytes) ||
        internal_sparse_coordinate::ByteSpansOverlap(
            inner_indices_, *index_bytes, values, *value_bytes)) {
      return Status(
          ErrorCode::kInvalidArgument,
          "Rebound sparse values cannot overlap compressed structure");
    }
    if (values != values_ && internal_sparse_coordinate::ByteSpansOverlap(
                                 values_, *value_bytes, values, *value_bytes)) {
      return Status(
          ErrorCode::kInvalidArgument,
          "Rebound sparse values cannot partially overlap existing values");
    }
    return CompressedSparseView<ReboundElement, Format>(
        outer_offsets_, inner_indices_, values, shape_, nonzeros_,
        memory_space_, canonical_structure_trusted_);
  }

  [[nodiscard]] Result<nnz_t> OuterOffset(extent_t position) const {
    Status access_status = ValidateHostAccess();
    if (!access_status.ok()) {
      return access_status;
    }
    const extent_t outer_extent =
        internal_sparse_compressed::OuterExtent<Format>(extents());
    if (position < 0 || position > outer_extent) {
      return Status(ErrorCode::kIndex,
                    "Compressed outer-offset position is out of range");
    }
    return outer_offsets_[static_cast<std::size_t>(position)];
  }

  [[nodiscard]] Result<index_t> InnerIndex(nnz_t position) const {
    Status access_status = ValidateHostAccess();
    if (!access_status.ok()) {
      return access_status;
    }
    if (position < 0 || position >= nonzeros_) {
      return Status(ErrorCode::kIndex,
                    "Compressed stored-entry position is out of range");
    }
    return inner_indices_[static_cast<std::size_t>(position)];
  }

  [[nodiscard]] Result<Element*> AtStored(nnz_t position) const {
    Status access_status = ValidateHostAccess();
    if (!access_status.ok()) {
      return access_status;
    }
    if (position < 0 || position >= nonzeros_) {
      return Status(ErrorCode::kIndex,
                    "Compressed stored-entry position is out of range");
    }
    return values_ + static_cast<std::size_t>(position);
  }

  [[nodiscard]] Result<value_type> Lookup(
      std::span<const index_t, 2> coordinate) const {
    Status access_status = ValidateHostAccess();
    if (!access_status.ok()) {
      return access_status;
    }
    Status coordinate_status =
        internal_sparse_coordinate::ValidateCoordinate(coordinate, extents());
    if (!coordinate_status.ok()) {
      return coordinate_status;
    }
    const extent_t outer =
        Format == SparseCompressedFormat::kCsr ? coordinate[0] : coordinate[1];
    const index_t inner =
        Format == SparseCompressedFormat::kCsr ? coordinate[1] : coordinate[0];
    nnz_t first = outer_offsets_[static_cast<std::size_t>(outer)];
    nnz_t last = outer_offsets_[static_cast<std::size_t>(outer + 1)];
    while (first < last) {
      const nnz_t middle = first + (last - first) / 2;
      const index_t candidate =
          inner_indices_[static_cast<std::size_t>(middle)];
      if (candidate < inner) {
        first = middle + 1;
      } else {
        last = middle;
      }
    }
    if (first == outer_offsets_[static_cast<std::size_t>(outer + 1)] ||
        inner_indices_[static_cast<std::size_t>(first)] != inner) {
      return value_type{};
    }
    return values_[static_cast<std::size_t>(first)];
  }

  [[nodiscard]] constexpr ExpressionAliasMetadata ValueAlias() const noexcept {
    const std::size_t bytes =
        static_cast<std::size_t>(nonzeros_) * sizeof(value_type);
    return ExpressionAliasMetadata(values_, values_, bytes);
  }

  [[nodiscard]] constexpr bool SameDescriptor(
      const CompressedSparseView& other) const noexcept {
    return outer_offsets_ == other.outer_offsets_ &&
           inner_indices_ == other.inner_indices_ && values_ == other.values_ &&
           shape_ == other.shape_ && nonzeros_ == other.nonzeros_ &&
           memory_space_ == other.memory_space_;
  }

 private:
  template <SparseViewElement, SparseCompressedFormat>
  friend class CompressedSparseView;
  friend class internal_sparse_compressed::ProviderAccess;
  template <SparseElement, SparseCompressedFormat>
  friend class CompressedSparseArray;

  constexpr CompressedSparseView(const nnz_t* outer_offsets,
                                 const index_t* inner_indices, Element* values,
                                 std::array<extent_t, 2> shape, nnz_t nonzeros,
                                 MemorySpace memory_space,
                                 bool canonical_structure_trusted) noexcept
      : outer_offsets_(outer_offsets),
        inner_indices_(inner_indices),
        values_(values),
        shape_(shape),
        nonzeros_(nonzeros),
        memory_space_(memory_space),
        canonical_structure_trusted_(canonical_structure_trusted) {}

  [[nodiscard]] Status ValidateHostAccess() const {
    if (memory_space_ != MemorySpace::kHost) {
      return Status(ErrorCode::kMemoryAccess,
                    "Compressed sparse access requires host storage");
    }
    return Status::Ok();
  }

  [[nodiscard]] Status ValidateCanonicalStructure() const {
    const extent_t outer_extent =
        internal_sparse_compressed::OuterExtent<Format>(extents());
    const extent_t inner_extent =
        internal_sparse_compressed::InnerExtent<Format>(extents());
    if (outer_offsets_[0] != 0) {
      return Status(ErrorCode::kInvalidArgument,
                    "Compressed offsets must begin at zero");
    }
    for (extent_t outer = 0; outer < outer_extent; ++outer) {
      const nnz_t begin = outer_offsets_[static_cast<std::size_t>(outer)];
      const nnz_t end = outer_offsets_[static_cast<std::size_t>(outer + 1)];
      if (begin < 0 || end < begin || end > nonzeros_) {
        return Status(ErrorCode::kInvalidArgument,
                      "Compressed offsets are not canonical");
      }
      for (nnz_t position = begin; position < end; ++position) {
        const index_t inner =
            inner_indices_[static_cast<std::size_t>(position)];
        if (inner < 0 || inner >= inner_extent) {
          return Status(ErrorCode::kIndex,
                        "Compressed inner index is outside its extent");
        }
        if (position != begin &&
            inner_indices_[static_cast<std::size_t>(position - 1)] >= inner) {
          return Status(ErrorCode::kInvalidArgument,
                        "Compressed inner indices must be strictly increasing");
        }
      }
    }
    if (outer_offsets_[static_cast<std::size_t>(outer_extent)] != nonzeros_) {
      return Status(ErrorCode::kInvalidArgument,
                    "Final compressed offset must equal NNZ");
    }
    return Status::Ok();
  }

  const nnz_t* outer_offsets_;
  const index_t* inner_indices_;
  Element* values_;
  std::array<extent_t, 2> shape_;
  nnz_t nonzeros_;
  MemorySpace memory_space_;
  bool canonical_structure_trusted_;
};

namespace internal_sparse_compressed {

class ProviderAccess {
 public:
  template <SparseViewElement Element, SparseCompressedFormat Format>
  static CompressedSparseView<Element, Format> MakeCanonicalView(
      const nnz_t* outer_offsets, const index_t* inner_indices, Element* values,
      std::array<extent_t, 2> shape, nnz_t nonzeros,
      MemorySpace memory_space) noexcept {
    return CompressedSparseView<Element, Format>(outer_offsets, inner_indices,
                                                 values, shape, nonzeros,
                                                 memory_space, true);
  }
};

}  // namespace internal_sparse_compressed

template <SparseElement Element, SparseCompressedFormat Format>
class CompressedSparseArray {
 public:
  static_assert(Format == SparseCompressedFormat::kCsr ||
                    Format == SparseCompressedFormat::kCsc,
                "Compressed sparse format must be CSR or CSC");
  using element_type = Element;
  static constexpr SparseCompressedFormat kFormat = Format;

  static Result<CompressedSparseArray> Create(
      MemoryResource& resource, std::span<const extent_t, 2> shape,
      std::span<const nnz_t> outer_offsets,
      std::span<const index_t> inner_indices, std::span<const Element> values) {
    if (resource.space() != MemorySpace::kHost) {
      return Status(ErrorCode::kUnsupported,
                    "Milestone 4 sparse owners require a host resource");
    }
    auto validated = CompressedSparseView<const Element, Format>::Create(
        outer_offsets, inner_indices, values, shape, MemorySpace::kHost);
    if (!validated.ok()) {
      return validated.status();
    }
    auto offset_bytes = CheckedMultiply(outer_offsets.size(), sizeof(nnz_t));
    if (!offset_bytes.ok()) {
      return offset_bytes.status();
    }
    auto index_bytes = CheckedMultiply(inner_indices.size(), sizeof(index_t));
    if (!index_bytes.ok()) {
      return index_bytes.status();
    }
    auto value_bytes = CheckedMultiply(values.size(), sizeof(Element));
    if (!value_bytes.ok()) {
      return value_bytes.status();
    }
    auto offset_buffer =
        Buffer::Allocate(resource, *offset_bytes, alignof(nnz_t));
    if (!offset_buffer.ok()) {
      return offset_buffer.status();
    }
    auto index_buffer =
        Buffer::Allocate(resource, *index_bytes, alignof(index_t));
    if (!index_buffer.ok()) {
      return index_buffer.status();
    }
    auto value_buffer =
        Buffer::Allocate(resource, *value_bytes, alignof(Element));
    if (!value_buffer.ok()) {
      return value_buffer.status();
    }
    std::copy(outer_offsets.begin(), outer_offsets.end(),
              static_cast<nnz_t*>(offset_buffer->data()));
    if (!inner_indices.empty()) {
      std::copy(inner_indices.begin(), inner_indices.end(),
                static_cast<index_t*>(index_buffer->data()));
    }
    if (!values.empty()) {
      std::copy(values.begin(), values.end(),
                static_cast<Element*>(value_buffer->data()));
    }
    std::array<extent_t, 2> copied_shape{shape[0], shape[1]};
    auto nonzeros = CheckedCast<nnz_t>(values.size());
    if (!nonzeros.ok()) {
      return nonzeros.status();
    }
    return CompressedSparseArray(
        &resource, std::move(*offset_buffer), std::move(*index_buffer),
        std::move(*value_buffer), copied_shape, *nonzeros);
  }

  static Result<CompressedSparseArray> Create(
      MemoryResource& resource, extent_t rows, extent_t columns,
      std::span<const nnz_t> outer_offsets,
      std::span<const index_t> inner_indices, std::span<const Element> values) {
    const std::array<extent_t, 2> shape{rows, columns};
    return Create(resource, shape, outer_offsets, inner_indices, values);
  }

  CompressedSparseArray(const CompressedSparseArray&) = delete;
  CompressedSparseArray& operator=(const CompressedSparseArray&) = delete;
  CompressedSparseArray(CompressedSparseArray&&) noexcept = default;
  CompressedSparseArray& operator=(CompressedSparseArray&&) noexcept = default;
  ~CompressedSparseArray() = default;

  [[nodiscard]] bool valid() const noexcept {
    return resource_ != nullptr && outer_offsets_.valid() &&
           inner_indices_.valid() && values_.valid();
  }

  [[nodiscard]] const std::array<extent_t, 2>& shape() const noexcept {
    return shape_;
  }

  [[nodiscard]] extent_t rows() const noexcept { return shape_[0]; }

  [[nodiscard]] extent_t columns() const noexcept { return shape_[1]; }

  [[nodiscard]] nnz_t nnz() const noexcept { return nonzeros_; }

  [[nodiscard]] Result<CompressedSparseView<Element, Format>> view() {
    if (!valid()) {
      return Status(ErrorCode::kInvalidState,
                    "A moved-from CompressedSparseArray has no view");
    }
    return CompressedSparseView<Element, Format>(
        static_cast<const nnz_t*>(outer_offsets_.data()),
        static_cast<const index_t*>(inner_indices_.data()),
        static_cast<Element*>(values_.data()), shape_, nonzeros_,
        MemorySpace::kHost, true);
  }

  [[nodiscard]] Result<CompressedSparseView<const Element, Format>> view()
      const {
    if (!valid()) {
      return Status(ErrorCode::kInvalidState,
                    "A moved-from CompressedSparseArray has no view");
    }
    return CompressedSparseView<const Element, Format>(
        static_cast<const nnz_t*>(outer_offsets_.data()),
        static_cast<const index_t*>(inner_indices_.data()),
        static_cast<const Element*>(values_.data()), shape_, nonzeros_,
        MemorySpace::kHost, true);
  }

  template <SparseViewElement SourceElement>
  static Result<CompressedSparseArray> FromCoordinate(
      const ExecutionContext& context, CoordinateView<SourceElement, 2> source,
      MemoryResource& destination_resource) {
    static_assert(
        std::same_as<std::remove_const_t<SourceElement>, Element>,
        "Sparse conversion source and destination elements must match");
    Status context_status = internal_sparse_coordinate::ValidateSerialHost(
        context, "Sparse conversion requires serial execution");
    if (!context_status.ok()) {
      return context_status;
    }
    Status source_status = ValidateExpressionAccess(context, source);
    if (!source_status.ok()) {
      return source_status;
    }
    return AllocateAndFill(
        destination_resource, source.shape(), source.nnz(),
        [&](nnz_t* offsets, index_t* indices, Element* values) {
          const extent_t outer_extent =
              internal_sparse_compressed::OuterExtent<Format>(source.extents());
          nnz_t output = 0;
          offsets[0] = 0;
          for (extent_t outer = 0; outer < outer_extent; ++outer) {
            for (nnz_t position = 0; position < source.nnz(); ++position) {
              auto coordinate = source.Coordinate(position);
              if (!coordinate.ok()) {
                return coordinate.status();
              }
              const extent_t candidate_outer =
                  Format == SparseCompressedFormat::kCsr ? (*coordinate)[0]
                                                         : (*coordinate)[1];
              if (candidate_outer != outer) {
                continue;
              }
              indices[static_cast<std::size_t>(output)] =
                  Format == SparseCompressedFormat::kCsr ? (*coordinate)[1]
                                                         : (*coordinate)[0];
              auto value = source.AtStored(position);
              if (!value.ok()) {
                return value.status();
              }
              values[static_cast<std::size_t>(output)] = **value;
              ++output;
            }
            offsets[static_cast<std::size_t>(outer + 1)] = output;
          }
          return Status::Ok();
        });
  }

  template <SparseViewElement SourceElement,
            SparseCompressedFormat SourceFormat>
    requires(SourceFormat != Format)
  static Result<CompressedSparseArray> FromCompressed(
      const ExecutionContext& context,
      CompressedSparseView<SourceElement, SourceFormat> source,
      MemoryResource& destination_resource) {
    static_assert(
        std::same_as<std::remove_const_t<SourceElement>, Element>,
        "Sparse conversion source and destination elements must match");
    Status context_status = internal_sparse_coordinate::ValidateSerialHost(
        context, "Sparse conversion requires serial execution");
    if (!context_status.ok()) {
      return context_status;
    }
    Status source_status = ValidateExpressionAccess(context, source);
    if (!source_status.ok()) {
      return source_status;
    }
    return AllocateAndFill(
        destination_resource, source.shape(), source.nnz(),
        [&](nnz_t* offsets, index_t* indices, Element* values) {
          const extent_t output_outer_extent =
              internal_sparse_compressed::OuterExtent<Format>(source.extents());
          nnz_t output = 0;
          offsets[0] = 0;
          for (extent_t output_outer = 0; output_outer < output_outer_extent;
               ++output_outer) {
            const extent_t source_outer_extent =
                internal_sparse_compressed::OuterExtent<SourceFormat>(
                    source.extents());
            for (extent_t source_outer = 0; source_outer < source_outer_extent;
                 ++source_outer) {
              auto begin = source.OuterOffset(source_outer);
              auto end = source.OuterOffset(source_outer + 1);
              if (!begin.ok()) {
                return begin.status();
              }
              if (!end.ok()) {
                return end.status();
              }
              for (nnz_t position = *begin; position < *end; ++position) {
                auto source_inner = source.InnerIndex(position);
                if (!source_inner.ok()) {
                  return source_inner.status();
                }
                const auto coordinate =
                    internal_sparse_compressed::Coordinate<SourceFormat>(
                        source_outer, *source_inner);
                const extent_t candidate_outer =
                    Format == SparseCompressedFormat::kCsr ? coordinate[0]
                                                           : coordinate[1];
                if (candidate_outer != output_outer) {
                  continue;
                }
                indices[static_cast<std::size_t>(output)] =
                    Format == SparseCompressedFormat::kCsr ? coordinate[1]
                                                           : coordinate[0];
                auto value = source.AtStored(position);
                if (!value.ok()) {
                  return value.status();
                }
                values[static_cast<std::size_t>(output)] = **value;
                ++output;
              }
            }
            offsets[static_cast<std::size_t>(output_outer + 1)] = output;
          }
          return Status::Ok();
        });
  }

 private:
  template <typename Fill>
  static Result<CompressedSparseArray> AllocateAndFill(
      MemoryResource& resource, std::span<const extent_t, 2> shape,
      nnz_t nonzeros, Fill&& fill) {
    if (resource.space() != MemorySpace::kHost) {
      return Status(ErrorCode::kUnsupported,
                    "Milestone 4 sparse conversions require a host resource");
    }
    auto offset_count = internal_sparse_compressed::OffsetCount(
        internal_sparse_compressed::OuterExtent<Format>(shape));
    if (!offset_count.ok()) {
      return offset_count.status();
    }
    auto offset_bytes = CheckedMultiply(*offset_count, sizeof(nnz_t));
    if (!offset_bytes.ok()) {
      return offset_bytes.status();
    }
    auto index_bytes = CheckedByteCount(nonzeros, sizeof(index_t));
    if (!index_bytes.ok()) {
      return index_bytes.status();
    }
    auto value_bytes = CheckedByteCount(nonzeros, sizeof(Element));
    if (!value_bytes.ok()) {
      return value_bytes.status();
    }
    auto offset_buffer =
        Buffer::Allocate(resource, *offset_bytes, alignof(nnz_t));
    if (!offset_buffer.ok()) {
      return offset_buffer.status();
    }
    auto index_buffer =
        Buffer::Allocate(resource, *index_bytes, alignof(index_t));
    if (!index_buffer.ok()) {
      return index_buffer.status();
    }
    auto value_buffer =
        Buffer::Allocate(resource, *value_bytes, alignof(Element));
    if (!value_buffer.ok()) {
      return value_buffer.status();
    }
    Status fill_status = fill(static_cast<nnz_t*>(offset_buffer->data()),
                              static_cast<index_t*>(index_buffer->data()),
                              static_cast<Element*>(value_buffer->data()));
    if (!fill_status.ok()) {
      return fill_status;
    }
    std::array<extent_t, 2> copied_shape{shape[0], shape[1]};
    return CompressedSparseArray(
        &resource, std::move(*offset_buffer), std::move(*index_buffer),
        std::move(*value_buffer), copied_shape, nonzeros);
  }

  CompressedSparseArray(MemoryResource* resource, Buffer outer_offsets,
                        Buffer inner_indices, Buffer values,
                        std::array<extent_t, 2> shape, nnz_t nonzeros) noexcept
      : resource_(resource),
        outer_offsets_(std::move(outer_offsets)),
        inner_indices_(std::move(inner_indices)),
        values_(std::move(values)),
        shape_(shape),
        nonzeros_(nonzeros) {}

  MemoryResource* resource_;
  Buffer outer_offsets_;
  Buffer inner_indices_;
  Buffer values_;
  std::array<extent_t, 2> shape_;
  nnz_t nonzeros_;
};

template <typename Element>
using CsrView = CompressedSparseView<Element, SparseCompressedFormat::kCsr>;

template <typename Element>
using CscView = CompressedSparseView<Element, SparseCompressedFormat::kCsc>;

template <SparseElement Element>
using CsrArray = CompressedSparseArray<Element, SparseCompressedFormat::kCsr>;

template <SparseElement Element>
using CscArray = CompressedSparseArray<Element, SparseCompressedFormat::kCsc>;

template <typename Element, SparseCompressedFormat Format>
struct ExpressionAdapter<CompressedSparseView<Element, Format>> {
  using value_type = std::remove_const_t<Element>;
  static constexpr rank_t rank = 2;
  static constexpr SparsityEffect sparsity_effect =
      SparsityEffect::kStructurePreserving;
  static constexpr ExpressionOperation operation =
      ExpressionOperation::kTerminal;

  static constexpr std::array<extent_t, 2> Shape(
      const CompressedSparseView<Element, Format>& view) noexcept {
    return view.shape();
  }

  static value_type Read(const CompressedSparseView<Element, Format>& view,
                         std::span<const index_t, 2> coordinate) {
    auto value = view.Lookup(coordinate);
    ASC_CHECK_MESSAGE(value.ok(),
                      "Sparse expression read requires valid host coordinates");
    return *value;
  }

  static bool MayAlias(const CompressedSparseView<Element, Format>& view,
                       AliasToken token) noexcept {
    const auto value_alias = view.ValueAlias();
    if (token.identity() == value_alias.identity() ||
        internal_sparse_coordinate::ByteSpanContains(
            value_alias.data(), value_alias.size(), token.identity())) {
      return true;
    }
    auto offset_count = internal_sparse_compressed::OffsetCount(
        internal_sparse_compressed::OuterExtent<Format>(view.extents()));
    if (!offset_count.ok()) {
      return true;
    }
    auto offset_bytes = CheckedMultiply(*offset_count, sizeof(nnz_t));
    auto index_bytes = CheckedByteCount(view.nnz(), sizeof(index_t));
    if (!offset_bytes.ok() || !index_bytes.ok()) {
      return true;
    }
    return internal_sparse_coordinate::ByteSpanContains(
               view.outer_offsets(), *offset_bytes, token.identity()) ||
           internal_sparse_coordinate::ByteSpanContains(
               view.inner_indices(), *index_bytes, token.identity());
  }

  static Status ValidateAccess(
      const CompressedSparseView<Element, Format>& view,
      const ExecutionContext& context) {
    Status context_status = internal_sparse_coordinate::ValidateSerialHost(
        context, "Sparse expression access requires serial execution");
    if (!context_status.ok()) {
      return context_status;
    }
    if (view.memory_space() != MemorySpace::kHost) {
      return Status(ErrorCode::kMemoryAccess,
                    "Sparse expression access requires host storage");
    }
    return Status::Ok();
  }
};

template <typename Element, SparseCompressedFormat Format>
struct ExpressionPlacementAdapter<CompressedSparseView<Element, Format>> {
  static constexpr MemorySpace Space(
      const CompressedSparseView<Element, Format>& view) noexcept {
    return view.memory_space();
  }

  static constexpr ExpressionAliasMetadata Alias(
      const CompressedSparseView<Element, Format>& view) noexcept {
    return view.ValueAlias();
  }
};

template <typename Element, SparseCompressedFormat Format>
  requires(!std::is_const_v<Element>)
struct WritableExpressionAdapter<CompressedSparseView<Element, Format>> {
  using value_type = std::remove_const_t<Element>;

  static constexpr std::array<extent_t, 2> Shape(
      const CompressedSparseView<Element, Format>& view) noexcept {
    return view.shape();
  }

  static constexpr ExpressionAliasMetadata Alias(
      const CompressedSparseView<Element, Format>& view) noexcept {
    return view.ValueAlias();
  }

  static constexpr bool IsUnique(
      const CompressedSparseView<Element, Format>& /*view*/) noexcept {
    return true;
  }

  static void Write(CompressedSparseView<Element, Format>& view,
                    std::span<const index_t, 2> coordinate, value_type value) {
    const extent_t outer =
        Format == SparseCompressedFormat::kCsr ? coordinate[0] : coordinate[1];
    const index_t inner =
        Format == SparseCompressedFormat::kCsr ? coordinate[1] : coordinate[0];
    auto begin = view.OuterOffset(outer);
    auto end = view.OuterOffset(outer + 1);
    ASC_CHECK_MESSAGE(begin.ok() && end.ok(),
                      "Sparse writable coordinate must be valid");
    for (nnz_t position = *begin; position < *end; ++position) {
      auto stored_inner = view.InnerIndex(position);
      ASC_CHECK_MESSAGE(stored_inner.ok(),
                        "Sparse writable structure must be accessible");
      if (*stored_inner == inner) {
        auto stored_value = view.AtStored(position);
        ASC_CHECK_MESSAGE(stored_value.ok(),
                          "Sparse writable value must be accessible");
        **stored_value = value;
        return;
      }
    }
    ASC_CHECK_MESSAGE(false,
                      "Sparse writable protocol requires a stored coordinate");
  }

  static Status ValidateAccess(
      const CompressedSparseView<Element, Format>& view,
      const ExecutionContext& context) {
    return ExpressionAdapter<
        CompressedSparseView<Element, Format>>::ValidateAccess(view, context);
  }
};

template <SparseViewElement SourceElement>
Result<CsrArray<std::remove_const_t<SourceElement>>> ConvertToCsr(
    const ExecutionContext& context, CoordinateView<SourceElement, 2> source,
    MemoryResource& destination_resource) {
  using Element = std::remove_const_t<SourceElement>;
  return CsrArray<Element>::FromCoordinate(context, source,
                                           destination_resource);
}

template <SparseViewElement SourceElement>
Result<CscArray<std::remove_const_t<SourceElement>>> ConvertToCsc(
    const ExecutionContext& context, CoordinateView<SourceElement, 2> source,
    MemoryResource& destination_resource) {
  using Element = std::remove_const_t<SourceElement>;
  return CscArray<Element>::FromCoordinate(context, source,
                                           destination_resource);
}

template <SparseViewElement SourceElement>
Result<CsrArray<std::remove_const_t<SourceElement>>> ConvertToCsr(
    const ExecutionContext& context, CscView<SourceElement> source,
    MemoryResource& destination_resource) {
  using Element = std::remove_const_t<SourceElement>;
  return CsrArray<Element>::FromCompressed(context, source,
                                           destination_resource);
}

template <SparseViewElement SourceElement>
Result<CscArray<std::remove_const_t<SourceElement>>> ConvertToCsc(
    const ExecutionContext& context, CsrView<SourceElement> source,
    MemoryResource& destination_resource) {
  using Element = std::remove_const_t<SourceElement>;
  return CscArray<Element>::FromCompressed(context, source,
                                           destination_resource);
}

template <SparseViewElement Element, SparseCompressedFormat Format>
Result<CoordinateArray<std::remove_const_t<Element>,
                       Extents<kDynamicExtent, kDynamicExtent>>>
ConvertToCoordinate(const ExecutionContext& context,
                    CompressedSparseView<Element, Format> source,
                    MemoryResource& destination_resource) {
  using Value = std::remove_const_t<Element>;
  using DynamicExtents = Extents<kDynamicExtent, kDynamicExtent>;
  Status context_status = internal_sparse_coordinate::ValidateSerialHost(
      context, "Sparse conversion requires serial execution");
  if (!context_status.ok()) {
    return context_status;
  }
  Status source_status = ValidateExpressionAccess(context, source);
  if (!source_status.ok()) {
    return source_status;
  }
  auto extents = DynamicExtents::Create(source.rows(), source.columns());
  if (!extents.ok()) {
    return extents.status();
  }
  auto builder = CoordinateBuilder<Value, DynamicExtents>::Create(
      destination_resource, *extents, source.nnz());
  if (!builder.ok()) {
    return builder.status();
  }
  const extent_t outer_extent =
      internal_sparse_compressed::OuterExtent<Format>(source.extents());
  for (extent_t outer = 0; outer < outer_extent; ++outer) {
    auto begin = source.OuterOffset(outer);
    auto end = source.OuterOffset(outer + 1);
    if (!begin.ok()) {
      return begin.status();
    }
    if (!end.ok()) {
      return end.status();
    }
    for (nnz_t position = *begin; position < *end; ++position) {
      auto inner = source.InnerIndex(position);
      auto value = source.AtStored(position);
      if (!inner.ok()) {
        return inner.status();
      }
      if (!value.ok()) {
        return value.status();
      }
      const auto coordinate =
          internal_sparse_compressed::Coordinate<Format>(outer, *inner);
      Status add_status = builder->Add(coordinate, **value);
      if (!add_status.ok()) {
        return add_status;
      }
    }
  }
  return std::move(*builder).Finalize(context, DuplicatePolicy::kReject,
                                      ExplicitZeroPolicy::kKeep);
}

static_assert(std::is_trivially_copyable_v<CsrView<float>>);

}  // namespace asc

#endif  // ASC_SPARSE_COMPRESSED_H_
