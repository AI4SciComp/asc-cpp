#ifndef ASC_SPARSE_COMPRESSED_H_
#define ASC_SPARSE_COMPRESSED_H_

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/expression/expression.h"
#include "asc/expression/writable.h"
#include "asc/sparse/coordinate.h"

namespace asc {

enum class SparseCompressedFormat : std::uint8_t {
  kCsr = 0,
  kCsc = 1,
};

template <SparseElement Element, SparseCompressedFormat Format>
class CompressedSparseView;

template <SparseElement Element, SparseCompressedFormat Format>
class CompressedSparseArray;

namespace internal_sparse_compressed {

template <SparseCompressedFormat Format>
inline constexpr bool kSupportedFormat =
    Format == SparseCompressedFormat::kCsr ||
    Format == SparseCompressedFormat::kCsc;

template <SparseCompressedFormat Format>
constexpr extent_t OuterExtent(const std::array<extent_t, 2>& shape) noexcept {
  static_assert(kSupportedFormat<Format>);
  if constexpr (Format == SparseCompressedFormat::kCsr) {
    return shape[0];
  } else {
    return shape[1];
  }
}

template <SparseCompressedFormat Format>
constexpr extent_t InnerExtent(const std::array<extent_t, 2>& shape) noexcept {
  static_assert(kSupportedFormat<Format>);
  if constexpr (Format == SparseCompressedFormat::kCsr) {
    return shape[1];
  } else {
    return shape[0];
  }
}

inline Status ValidateSerialHost(const ExecutionContext& context,
                                 MemorySpace space, const char* operation) {
  if (context.backend() != Backend::kSerial) {
    return Status(ErrorCode::kUnsupported, operation);
  }
  if (!context.CanAccess(space)) {
    return Status(ErrorCode::kMemoryAccess,
                  "Serial sparse conversion requires host memory");
  }
  return Status::Ok();
}

template <SparseElement Element, SparseCompressedFormat Format>
struct ArrayFactory;
struct ViewAccess;

}  // namespace internal_sparse_compressed

template <SparseElement Element, SparseCompressedFormat Format>
class CompressedSparseView {
  static_assert(internal_sparse_compressed::kSupportedFormat<Format>,
                "Compressed sparse format must be CSR or CSC");

 public:
  using element_type = Element;
  using value_type = std::remove_cv_t<Element>;
  using ShapeType = std::array<extent_t, 2>;
  static constexpr SparseCompressedFormat kFormat = Format;

  static Result<CompressedSparseView> Create(
      std::span<const extent_t, 2> shape, std::span<const nnz_t> outer_offsets,
      std::span<const index_t> inner_indices, std::span<Element> values,
      MemorySpace space) {
    const Status space_status =
        internal_sparse_coordinate::ValidateMemorySpace(space);
    if (!space_status.ok()) {
      return space_status;
    }
    ShapeType shape_values{shape[0], shape[1]};
    for (extent_t extent : shape_values) {
      if (extent < 0) {
        return Status(ErrorCode::kShape,
                      "A compressed sparse extent cannot be negative");
      }
    }
    if (inner_indices.size() != values.size()) {
      return Status(
          ErrorCode::kInvalidArgument,
          "Compressed sparse inner-index and value lengths must match");
    }
    auto nnz = CheckedCast<nnz_t>(values.size());
    if (!nnz.ok()) {
      return nnz.status();
    }
    auto outer_count = CheckedAdd(
        internal_sparse_compressed::OuterExtent<Format>(shape_values),
        static_cast<extent_t>(1));
    if (!outer_count.ok()) {
      return Status(ErrorCode::kOverflow,
                    "A compressed sparse outer-offset length overflowed");
    }
    auto outer_count_size = CheckedCast<std::size_t>(*outer_count);
    if (!outer_count_size.ok()) {
      return outer_count_size.status();
    }
    if (outer_offsets.size() != *outer_count_size) {
      return Status(
          ErrorCode::kInvalidArgument,
          "Compressed sparse outer-offset length does not match the shape");
    }

    auto outer_bytes = CheckedMultiply(outer_offsets.size(), sizeof(nnz_t));
    if (!outer_bytes.ok()) {
      return Status(ErrorCode::kOverflow,
                    "An outer-offset byte span exceeds size_t");
    }
    auto inner_bytes = CheckedMultiply(inner_indices.size(), sizeof(index_t));
    if (!inner_bytes.ok()) {
      return Status(ErrorCode::kOverflow,
                    "An inner-index byte span exceeds size_t");
    }
    auto value_bytes = CheckedMultiply(values.size(), sizeof(value_type));
    if (!value_bytes.ok()) {
      return Status(ErrorCode::kOverflow,
                    "A sparse value byte span exceeds size_t");
    }
    const Status outer_span_status =
        internal_sparse_coordinate::ValidateAddressSpan(
            outer_offsets.data(), *outer_bytes, alignof(nnz_t),
            "A nonempty outer-offset span cannot have a null pointer",
            "An outer-offset pointer does not satisfy NNZ alignment",
            "An outer-offset address span exceeds uintptr_t");
    if (!outer_span_status.ok()) {
      return outer_span_status;
    }
    const Status inner_span_status =
        internal_sparse_coordinate::ValidateAddressSpan(
            inner_indices.data(), *inner_bytes, alignof(index_t),
            "A nonempty inner-index span cannot have a null pointer",
            "An inner-index pointer does not satisfy index alignment",
            "An inner-index address span exceeds uintptr_t");
    if (!inner_span_status.ok()) {
      return inner_span_status;
    }
    const Status value_span_status =
        internal_sparse_coordinate::ValidateAddressSpan(
            values.data(), *value_bytes, alignof(value_type),
            "A nonempty sparse value span cannot have a null pointer",
            "A sparse value pointer does not satisfy element alignment",
            "A sparse value address span exceeds uintptr_t");
    if (!value_span_status.ok()) {
      return value_span_status;
    }
    if (internal_sparse_coordinate::ByteSpansOverlap(
            outer_offsets.data(), *outer_bytes, inner_indices.data(),
            *inner_bytes) ||
        internal_sparse_coordinate::ByteSpansOverlap(
            outer_offsets.data(), *outer_bytes, values.data(), *value_bytes) ||
        internal_sparse_coordinate::ByteSpansOverlap(
            inner_indices.data(), *inner_bytes, values.data(), *value_bytes)) {
      return Status(
          ErrorCode::kInvalidArgument,
          "Compressed sparse structure and value spans must be pairwise "
          "disjoint");
    }

    if (space == MemorySpace::kHost) {
      if (outer_offsets.front() != 0) {
        return Status(ErrorCode::kInvalidArgument,
                      "The first compressed sparse outer offset must be zero");
      }
      if (outer_offsets.back() != *nnz) {
        return Status(
            ErrorCode::kInvalidArgument,
            "The final compressed sparse outer offset must equal NNZ");
      }
      const extent_t outer_extent =
          internal_sparse_compressed::OuterExtent<Format>(shape_values);
      const extent_t inner_extent =
          internal_sparse_compressed::InnerExtent<Format>(shape_values);
      for (extent_t outer = 0; outer < outer_extent; ++outer) {
        const nnz_t begin = outer_offsets[static_cast<std::size_t>(outer)];
        const nnz_t end = outer_offsets[static_cast<std::size_t>(outer + 1)];
        if (begin < 0 || end < begin || end > *nnz) {
          return Status(
              ErrorCode::kInvalidArgument,
              "Compressed sparse outer offsets must be nondecreasing and "
              "bounded by NNZ");
        }
        index_t previous = -1;
        for (nnz_t position = begin; position < end; ++position) {
          const index_t inner =
              inner_indices[static_cast<std::size_t>(position)];
          if (inner < 0 || inner >= inner_extent) {
            return Status(ErrorCode::kIndex,
                          "A compressed sparse inner index is out of range");
          }
          if (inner <= previous) {
            return Status(
                ErrorCode::kInvalidArgument,
                "Compressed sparse inner indices must be strictly increasing");
          }
          previous = inner;
        }
      }
    }

    auto alias = AliasToken::FromAddressSpan(values.data(), *value_bytes);
    if (!alias.ok()) {
      return alias.status();
    }
    return CompressedSparseView(outer_offsets.data(), inner_indices.data(),
                                values.data(), shape_values, *nnz, space,
                                *alias, space == MemorySpace::kHost);
  }

  template <SparseElement MutableElement>
    requires std::is_const_v<Element> && (!std::is_const_v<MutableElement>) &&
                 std::same_as<std::remove_const_t<Element>, MutableElement>
  constexpr CompressedSparseView(
      const CompressedSparseView<MutableElement, Format>& mutable_view) noexcept
      : outer_offsets_(mutable_view.outer_offsets_),
        inner_indices_(mutable_view.inner_indices_),
        values_(mutable_view.values_),
        shape_(mutable_view.shape_),
        nnz_(mutable_view.nnz_),
        space_(mutable_view.space_),
        alias_(mutable_view.alias_),
        trusted_provenance_(mutable_view.trusted_provenance_) {}

  [[nodiscard]] static constexpr rank_t rank() noexcept { return 2; }
  [[nodiscard]] constexpr const ShapeType& shape() const noexcept {
    return shape_;
  }
  [[nodiscard]] constexpr nnz_t nnz() const noexcept { return nnz_; }
  [[nodiscard]] constexpr MemorySpace space() const noexcept { return space_; }
  [[nodiscard]] constexpr AliasToken alias_token() const noexcept {
    return alias_;
  }
  [[nodiscard]] constexpr const nnz_t* outer_offset_data() const noexcept {
    return outer_offsets_;
  }
  [[nodiscard]] constexpr const index_t* inner_index_data() const noexcept {
    return inner_indices_;
  }
  [[nodiscard]] constexpr Element* value_data() const noexcept {
    return values_;
  }

  // Reuses this view's immutable canonical structure with a distinct exact
  // value span. Canonical provenance is preserved but does not imply that
  // asynchronous production of the structure has completed.
  template <SparseElement NewElement>
    requires std::same_as<std::remove_const_t<NewElement>, value_type>
  [[nodiscard]] Result<CompressedSparseView<NewElement, Format>> RebindValues(
      std::span<NewElement> values) const {
    auto count = CheckedCast<std::size_t>(nnz_);
    if (!count.ok()) {
      return count.status();
    }
    if (values.size() != *count) {
      return Status(ErrorCode::kInvalidArgument,
                    "A rebound compressed sparse value span must have exact "
                    "NNZ length");
    }
    const auto outer_count =
        static_cast<std::size_t>(
            internal_sparse_compressed::OuterExtent<Format>(shape_)) +
        1U;
    auto rebound = CompressedSparseView<NewElement, Format>::Create(
        shape_, std::span<const nnz_t>(outer_offsets_, outer_count),
        std::span<const index_t>(inner_indices_, *count), values, space_);
    if (!rebound.ok()) {
      return rebound.status();
    }
    rebound->trusted_provenance_ = trusted_provenance_;
    return std::move(*rebound);
  }

  [[nodiscard]] Result<Element*> ValueAt(nnz_t position) const {
    if (space_ != MemorySpace::kHost) {
      return Status(ErrorCode::kMemoryAccess,
                    "Sparse values are host-dereferenceable only in host "
                    "memory");
    }
    if (position < 0 || position >= nnz_) {
      return Status(ErrorCode::kIndex,
                    "A stored-entry position is outside the sparse view");
    }
    return values_ + static_cast<std::size_t>(position);
  }

  [[nodiscard]] Result<Element*> Find(index_t row, index_t column) const {
    if (space_ != MemorySpace::kHost) {
      return Status(ErrorCode::kMemoryAccess,
                    "Sparse values are host-dereferenceable only in host "
                    "memory");
    }
    if (row < 0 || row >= shape_[0] || column < 0 || column >= shape_[1]) {
      return Status(ErrorCode::kIndex,
                    "A sparse matrix coordinate is outside its shape");
    }
    const index_t outer = Format == SparseCompressedFormat::kCsr ? row : column;
    const index_t inner = Format == SparseCompressedFormat::kCsr ? column : row;
    const nnz_t position = FindPositionUnchecked(outer, inner);
    if (position == nnz_) {
      return static_cast<Element*>(nullptr);
    }
    return values_ + static_cast<std::size_t>(position);
  }

  template <SparseElement OtherElement>
    requires std::same_as<std::remove_cv_t<Element>,
                          std::remove_cv_t<OtherElement>>
  [[nodiscard]] constexpr bool IsExactView(
      const CompressedSparseView<OtherElement, Format>& other) const noexcept {
    return outer_offsets_ == other.outer_offset_data() &&
           inner_indices_ == other.inner_index_data() &&
           values_ == other.value_data() && shape_ == other.shape() &&
           nnz_ == other.nnz() && space_ == other.space();
  }

 private:
  template <SparseElement, SparseCompressedFormat>
  friend class CompressedSparseView;
  friend struct ExpressionAdapter<CompressedSparseView<Element, Format>>;
  friend struct WritableExpressionAdapter<
      CompressedSparseView<Element, Format>>;
  friend struct internal_sparse_compressed::ViewAccess;

  [[nodiscard]] nnz_t FindPositionUnchecked(index_t outer,
                                            index_t inner) const noexcept {
    nnz_t first = outer_offsets_[static_cast<std::size_t>(outer)];
    nnz_t last = outer_offsets_[static_cast<std::size_t>(outer + 1)];
    while (first < last) {
      const nnz_t middle = first + (last - first) / 2;
      const index_t value = inner_indices_[static_cast<std::size_t>(middle)];
      if (value < inner) {
        first = middle + 1;
      } else {
        last = middle;
      }
    }
    const nnz_t segment_end =
        outer_offsets_[static_cast<std::size_t>(outer + 1)];
    if (first == segment_end ||
        inner_indices_[static_cast<std::size_t>(first)] != inner) {
      return nnz_;
    }
    return first;
  }

  [[nodiscard]] value_type ReadUnchecked(index_t row,
                                         index_t column) const noexcept {
    const index_t outer = Format == SparseCompressedFormat::kCsr ? row : column;
    const index_t inner = Format == SparseCompressedFormat::kCsr ? column : row;
    const nnz_t position = FindPositionUnchecked(outer, inner);
    if (position == nnz_) {
      return value_type{};
    }
    return values_[static_cast<std::size_t>(position)];
  }

  void WriteUnchecked(index_t row, index_t column,
                      value_type value) const noexcept
    requires(!std::is_const_v<Element>)
  {
    const index_t outer = Format == SparseCompressedFormat::kCsr ? row : column;
    const index_t inner = Format == SparseCompressedFormat::kCsr ? column : row;
    const nnz_t position = FindPositionUnchecked(outer, inner);
    if (position != nnz_) {
      values_[static_cast<std::size_t>(position)] = value;
    }
  }

  constexpr CompressedSparseView(const nnz_t* outer_offsets,
                                 const index_t* inner_indices, Element* values,
                                 ShapeType shape, nnz_t nnz, MemorySpace space,
                                 AliasToken alias,
                                 bool trusted_provenance) noexcept
      : outer_offsets_(outer_offsets),
        inner_indices_(inner_indices),
        values_(values),
        shape_(shape),
        nnz_(nnz),
        space_(space),
        alias_(alias),
        trusted_provenance_(trusted_provenance) {}

  const nnz_t* outer_offsets_;
  const index_t* inner_indices_;
  Element* values_;
  ShapeType shape_;
  nnz_t nnz_;
  MemorySpace space_;
  AliasToken alias_;
  bool trusted_provenance_ = false;
};

namespace internal_sparse_compressed {

struct ViewAccess {
  template <SparseElement Element, SparseCompressedFormat Format>
  [[nodiscard]] static constexpr bool Trusted(
      CompressedSparseView<Element, Format> view) noexcept {
    return view.trusted_provenance_;
  }

 private:
  template <SparseElement, SparseCompressedFormat>
  friend class ::asc::CompressedSparseArray;

  template <SparseElement Element, SparseCompressedFormat Format>
  static Result<CompressedSparseView<Element, Format>> CreateTrusted(
      std::span<const extent_t, 2> shape, std::span<const nnz_t> outer_offsets,
      std::span<const index_t> inner_indices, std::span<Element> values,
      MemorySpace space) {
    auto view = CompressedSparseView<Element, Format>::Create(
        shape, outer_offsets, inner_indices, values, space);
    if (!view.ok()) {
      return view.status();
    }
    return CompressedSparseView<Element, Format>(
        view->outer_offsets_, view->inner_indices_, view->values_, view->shape_,
        view->nnz_, view->space_, view->alias_, true);
  }
};

}  // namespace internal_sparse_compressed

template <SparseElement Element, SparseCompressedFormat Format>
class CompressedSparseArray {
  static_assert(internal_sparse_compressed::kSupportedFormat<Format>,
                "Compressed sparse format must be CSR or CSC");
  static_assert(!std::is_const_v<Element>,
                "CompressedSparseArray owns mutable, non-const values");

 public:
  using ViewType = CompressedSparseView<Element, Format>;
  using ConstViewType = CompressedSparseView<const Element, Format>;
  using ShapeType = std::array<extent_t, 2>;
  static constexpr SparseCompressedFormat kFormat = Format;

  static Result<CompressedSparseArray> Create(
      std::span<const extent_t, 2> shape, std::span<const nnz_t> outer_offsets,
      std::span<const index_t> inner_indices, std::span<const Element> values,
      MemoryResource& resource) {
    auto validation = ConstViewType::Create(shape, outer_offsets, inner_indices,
                                            values, resource.space());
    if (!validation.ok()) {
      return validation.status();
    }
    auto result =
        internal_sparse_compressed::ArrayFactory<Element, Format>::Create(
            validation->shape(), validation->nnz(), resource);
    if (!result.ok()) {
      return result.status();
    }
    auto* destination_outer =
        internal_sparse_compressed::ArrayFactory<Element, Format>::MutableOuter(
            *result);
    auto* destination_inner =
        internal_sparse_compressed::ArrayFactory<Element, Format>::MutableInner(
            *result);
    auto* destination_values = internal_sparse_compressed::ArrayFactory<
        Element, Format>::MutableValues(*result);
    for (std::size_t index = 0; index < outer_offsets.size(); ++index) {
      destination_outer[index] = outer_offsets[index];
    }
    for (std::size_t index = 0; index < inner_indices.size(); ++index) {
      destination_inner[index] = inner_indices[index];
      destination_values[index] = values[index];
    }
    return std::move(*result);
  }

  CompressedSparseArray(const CompressedSparseArray&) = delete;
  CompressedSparseArray& operator=(const CompressedSparseArray&) = delete;

  CompressedSparseArray(CompressedSparseArray&& other) noexcept
      : shape_(other.shape_),
        nnz_(std::exchange(other.nnz_, 0)),
        outer_offsets_(std::move(other.outer_offsets_)),
        inner_indices_(std::move(other.inner_indices_)),
        values_(std::move(other.values_)),
        resource_(std::exchange(other.resource_, nullptr)) {}

  CompressedSparseArray& operator=(CompressedSparseArray&& other) noexcept {
    if (this != &other) {
      shape_ = other.shape_;
      nnz_ = std::exchange(other.nnz_, 0);
      outer_offsets_ = std::move(other.outer_offsets_);
      inner_indices_ = std::move(other.inner_indices_);
      values_ = std::move(other.values_);
      resource_ = std::exchange(other.resource_, nullptr);
    }
    return *this;
  }

  ~CompressedSparseArray() = default;

  [[nodiscard]] constexpr const ShapeType& shape() const noexcept {
    return shape_;
  }
  [[nodiscard]] nnz_t nnz() const noexcept { return nnz_; }
  [[nodiscard]] MemoryResource* resource() const noexcept { return resource_; }

  [[nodiscard]] Result<ViewType> view() {
    if (resource_ == nullptr) {
      return Status(ErrorCode::kInvalidState,
                    "A moved-from CompressedSparseArray cannot produce a view");
    }
    return internal_sparse_compressed::ViewAccess::CreateTrusted<Element,
                                                                 Format>(
        shape_,
        std::span<const nnz_t>(static_cast<const nnz_t*>(outer_offsets_.data()),
                               OuterCount()),
        std::span<const index_t>(
            static_cast<const index_t*>(inner_indices_.data()),
            static_cast<std::size_t>(nnz_)),
        std::span<Element>(static_cast<Element*>(values_.data()),
                           static_cast<std::size_t>(nnz_)),
        resource_->space());
  }

  [[nodiscard]] Result<ConstViewType> view() const {
    if (resource_ == nullptr) {
      return Status(ErrorCode::kInvalidState,
                    "A moved-from CompressedSparseArray cannot produce a view");
    }
    return internal_sparse_compressed::ViewAccess::CreateTrusted<const Element,
                                                                 Format>(
        shape_,
        std::span<const nnz_t>(static_cast<const nnz_t*>(outer_offsets_.data()),
                               OuterCount()),
        std::span<const index_t>(
            static_cast<const index_t*>(inner_indices_.data()),
            static_cast<std::size_t>(nnz_)),
        std::span<const Element>(static_cast<const Element*>(values_.data()),
                                 static_cast<std::size_t>(nnz_)),
        resource_->space());
  }

 private:
  friend struct internal_sparse_compressed::ArrayFactory<Element, Format>;

  [[nodiscard]] std::size_t OuterCount() const noexcept {
    return static_cast<std::size_t>(
        internal_sparse_compressed::OuterExtent<Format>(shape_) + 1);
  }

  CompressedSparseArray(ShapeType shape, nnz_t nnz, Buffer outer_offsets,
                        Buffer inner_indices, Buffer values,
                        MemoryResource* resource) noexcept
      : shape_(shape),
        nnz_(nnz),
        outer_offsets_(std::move(outer_offsets)),
        inner_indices_(std::move(inner_indices)),
        values_(std::move(values)),
        resource_(resource) {}

  ShapeType shape_{};
  nnz_t nnz_ = 0;
  Buffer outer_offsets_;
  Buffer inner_indices_;
  Buffer values_;
  MemoryResource* resource_ = nullptr;
};

namespace internal_sparse_compressed {

template <SparseElement Element, SparseCompressedFormat Format>
struct ArrayFactory {
  using Array = CompressedSparseArray<Element, Format>;
  using ShapeType = typename Array::ShapeType;

  static Result<Array> Create(const ShapeType& shape, nnz_t nnz,
                              MemoryResource& resource) {
    if (resource.space() != MemorySpace::kHost) {
      return Status(
          ErrorCode::kUnsupported,
          "Milestone 4 compressed sparse ownership supports host memory only");
    }
    auto array = AllocateStorage(shape, nnz, resource);
    if (!array.ok()) {
      return array.status();
    }
    auto outer_count =
        CheckedAdd(OuterExtent<Format>(shape), static_cast<extent_t>(1));
    if (!outer_count.ok()) {
      return outer_count.status();
    }
    auto outer_count_size = CheckedCast<std::size_t>(*outer_count);
    if (!outer_count_size.ok()) {
      return outer_count_size.status();
    }
    auto nnz_size = CheckedCast<std::size_t>(nnz);
    if (!nnz_size.ok()) {
      return nnz_size.status();
    }
    auto* outer_data = MutableOuter(*array);
    for (std::size_t index = 0; index < *outer_count_size; ++index) {
      std::construct_at(outer_data + index, static_cast<nnz_t>(0));
    }
    auto* inner_data = MutableInner(*array);
    auto* value_data = MutableValues(*array);
    for (std::size_t index = 0; index < *nnz_size; ++index) {
      std::construct_at(inner_data + index, static_cast<index_t>(0));
      std::construct_at(value_data + index, Element{});
    }
    return std::move(*array);
  }

  static Result<Array> AllocateStorage(const ShapeType& shape, nnz_t nnz,
                                       MemoryResource& resource) {
    const Status shape_status =
        internal_sparse_coordinate::ValidateShape<2>(shape, nnz);
    if (!shape_status.ok()) {
      return shape_status;
    }
    auto outer_count =
        CheckedAdd(OuterExtent<Format>(shape), static_cast<extent_t>(1));
    if (!outer_count.ok()) {
      return outer_count.status();
    }
    auto outer_bytes = CheckedByteCount(*outer_count, sizeof(nnz_t));
    if (!outer_bytes.ok()) {
      return outer_bytes.status();
    }
    auto inner_bytes = CheckedByteCount(nnz, sizeof(index_t));
    if (!inner_bytes.ok()) {
      return inner_bytes.status();
    }
    auto value_bytes = CheckedByteCount(nnz, sizeof(Element));
    if (!value_bytes.ok()) {
      return value_bytes.status();
    }
    auto outer_offsets =
        Buffer::Allocate(resource, *outer_bytes, alignof(nnz_t));
    if (!outer_offsets.ok()) {
      return outer_offsets.status();
    }
    auto inner_indices =
        Buffer::Allocate(resource, *inner_bytes, alignof(index_t));
    if (!inner_indices.ok()) {
      return inner_indices.status();
    }
    auto values = Buffer::Allocate(resource, *value_bytes, alignof(Element));
    if (!values.ok()) {
      return values.status();
    }

    return Array(shape, nnz, std::move(*outer_offsets),
                 std::move(*inner_indices), std::move(*values), &resource);
  }

  static nnz_t* MutableOuter(Array& array) noexcept {
    return static_cast<nnz_t*>(array.outer_offsets_.data());
  }
  static index_t* MutableInner(Array& array) noexcept {
    return static_cast<index_t*>(array.inner_indices_.data());
  }
  static Element* MutableValues(Array& array) noexcept {
    return static_cast<Element*>(array.values_.data());
  }
};

template <SparseElement Element, SparseCompressedFormat Format>
Status ValidateConversionSource(const ExecutionContext& context,
                                CompressedSparseView<Element, Format> source) {
  return ValidateSerialHost(
      context, source.space(),
      "Milestone 4 sparse conversions require serial execution");
}

template <SparseElement Element, std::size_t Rank>
Status ValidateConversionSource(const ExecutionContext& context,
                                CoordinateView<Element, Rank> source) {
  return ValidateSerialHost(
      context, source.space(),
      "Milestone 4 sparse conversions require serial execution");
}

template <SparseCompressedFormat DestinationFormat, SparseElement Element>
Result<CompressedSparseArray<std::remove_cv_t<Element>, DestinationFormat>>
CoordinateToCompressed(const ExecutionContext& context,
                       CoordinateView<Element, 2> source,
                       MemoryResource& destination_resource) {
  using Scalar = std::remove_cv_t<Element>;
  const Status source_status = ValidateConversionSource(context, source);
  if (!source_status.ok()) {
    return source_status;
  }
  auto destination = ArrayFactory<Scalar, DestinationFormat>::Create(
      source.shape(), source.nnz(), destination_resource);
  if (!destination.ok()) {
    return destination.status();
  }
  nnz_t* const offsets =
      ArrayFactory<Scalar, DestinationFormat>::MutableOuter(*destination);
  index_t* const indices =
      ArrayFactory<Scalar, DestinationFormat>::MutableInner(*destination);
  Scalar* const values =
      ArrayFactory<Scalar, DestinationFormat>::MutableValues(*destination);
  const extent_t outer_extent = OuterExtent<DestinationFormat>(source.shape());

  nnz_t output = 0;
  offsets[0] = 0;
  for (index_t outer = 0; outer < outer_extent; ++outer) {
    for (nnz_t position = 0; position < source.nnz(); ++position) {
      const auto coordinate = internal_sparse_coordinate::CoordinateAt<2>(
          source.coordinate_data(), position);
      const index_t source_outer =
          DestinationFormat == SparseCompressedFormat::kCsr ? coordinate[0]
                                                            : coordinate[1];
      if (source_outer != outer) {
        continue;
      }
      const index_t source_inner =
          DestinationFormat == SparseCompressedFormat::kCsr ? coordinate[1]
                                                            : coordinate[0];
      indices[static_cast<std::size_t>(output)] = source_inner;
      values[static_cast<std::size_t>(output)] =
          source.value_data()[static_cast<std::size_t>(position)];
      ++output;
    }
    offsets[static_cast<std::size_t>(outer + 1)] = output;
  }
  return std::move(*destination);
}

template <SparseCompressedFormat DestinationFormat,
          SparseCompressedFormat SourceFormat, SparseElement Element>
Result<CompressedSparseArray<std::remove_cv_t<Element>, DestinationFormat>>
CompressedToCompressed(const ExecutionContext& context,
                       CompressedSparseView<Element, SourceFormat> source,
                       MemoryResource& destination_resource) {
  static_assert(DestinationFormat != SourceFormat);
  using Scalar = std::remove_cv_t<Element>;
  const Status source_status = ValidateConversionSource(context, source);
  if (!source_status.ok()) {
    return source_status;
  }
  auto destination = ArrayFactory<Scalar, DestinationFormat>::Create(
      source.shape(), source.nnz(), destination_resource);
  if (!destination.ok()) {
    return destination.status();
  }
  nnz_t* const destination_offsets =
      ArrayFactory<Scalar, DestinationFormat>::MutableOuter(*destination);
  index_t* const destination_indices =
      ArrayFactory<Scalar, DestinationFormat>::MutableInner(*destination);
  Scalar* const destination_values =
      ArrayFactory<Scalar, DestinationFormat>::MutableValues(*destination);
  const extent_t destination_outer_extent =
      OuterExtent<DestinationFormat>(source.shape());
  const extent_t source_outer_extent =
      OuterExtent<SourceFormat>(source.shape());
  const nnz_t* const source_offsets = source.outer_offset_data();
  const index_t* const source_indices = source.inner_index_data();
  Element* const source_values = source.value_data();

  nnz_t output = 0;
  destination_offsets[0] = 0;
  for (index_t destination_outer = 0;
       destination_outer < destination_outer_extent; ++destination_outer) {
    for (index_t source_outer = 0; source_outer < source_outer_extent;
         ++source_outer) {
      const nnz_t begin =
          source_offsets[static_cast<std::size_t>(source_outer)];
      const nnz_t end =
          source_offsets[static_cast<std::size_t>(source_outer + 1)];
      for (nnz_t position = begin; position < end; ++position) {
        if (source_indices[static_cast<std::size_t>(position)] !=
            destination_outer) {
          continue;
        }
        destination_indices[static_cast<std::size_t>(output)] = source_outer;
        destination_values[static_cast<std::size_t>(output)] =
            source_values[static_cast<std::size_t>(position)];
        ++output;
      }
    }
    destination_offsets[static_cast<std::size_t>(destination_outer + 1)] =
        output;
  }
  return std::move(*destination);
}

}  // namespace internal_sparse_compressed

template <SparseElement Element, SparseCompressedFormat Format>
struct ExpressionAdapter<CompressedSparseView<Element, Format>> {
  using value_type = std::remove_cv_t<Element>;
  static constexpr rank_t kRank = 2;
  static constexpr ExpressionOperationCategory kOperationCategory =
      ExpressionOperationCategory::kTerminal;
  static constexpr SparsityEffect kSparsityEffect =
      SparsityEffect::kStructurePreserving;

  [[nodiscard]] static constexpr std::array<extent_t, 2> Shape(
      const CompressedSparseView<Element, Format>& view) noexcept {
    return view.shape();
  }

  [[nodiscard]] static value_type Read(
      const CompressedSparseView<Element, Format>& view,
      std::span<const index_t, 2> coordinate) noexcept {
    return view.ReadUnchecked(coordinate[0], coordinate[1]);
  }

  [[nodiscard]] static constexpr bool MayAlias(
      const CompressedSparseView<Element, Format>& view,
      AliasToken alias) noexcept {
    return AliasTokensMayOverlap(view.alias_token(), alias);
  }
};

template <SparseElement Element, SparseCompressedFormat Format>
struct ExpressionPlacementAdapter<CompressedSparseView<Element, Format>> {
  [[nodiscard]] static constexpr MemorySpace Space(
      const CompressedSparseView<Element, Format>& view) noexcept {
    return view.space();
  }
};

template <SparseElement Element, SparseCompressedFormat Format>
  requires(!std::is_const_v<Element>)
struct WritableExpressionAdapter<CompressedSparseView<Element, Format>> {
  using value_type = Element;
  static constexpr rank_t kRank = 2;

  [[nodiscard]] static constexpr std::array<extent_t, 2> Shape(
      const CompressedSparseView<Element, Format>& view) noexcept {
    return view.shape();
  }

  [[nodiscard]] static constexpr AliasToken Alias(
      const CompressedSparseView<Element, Format>& view) noexcept {
    return view.alias_token();
  }

  static void Write(CompressedSparseView<Element, Format>& view,
                    std::span<const index_t, 2> coordinate,
                    value_type value) noexcept {
    view.WriteUnchecked(coordinate[0], coordinate[1], value);
  }
};

template <SparseElement Element>
using CsrView = CompressedSparseView<Element, SparseCompressedFormat::kCsr>;

template <SparseElement Element>
using CscView = CompressedSparseView<Element, SparseCompressedFormat::kCsc>;

template <SparseElement Element>
using CsrArray = CompressedSparseArray<Element, SparseCompressedFormat::kCsr>;

template <SparseElement Element>
using CscArray = CompressedSparseArray<Element, SparseCompressedFormat::kCsc>;

template <SparseElement Element>
Result<CsrArray<std::remove_cv_t<Element>>> ToCsr(
    const ExecutionContext& context, CoordinateView<Element, 2> source,
    MemoryResource& destination_resource) {
  return internal_sparse_compressed::CoordinateToCompressed<
      SparseCompressedFormat::kCsr>(context, source, destination_resource);
}

template <SparseElement Element>
Result<CscArray<std::remove_cv_t<Element>>> ToCsc(
    const ExecutionContext& context, CoordinateView<Element, 2> source,
    MemoryResource& destination_resource) {
  return internal_sparse_compressed::CoordinateToCompressed<
      SparseCompressedFormat::kCsc>(context, source, destination_resource);
}

template <SparseElement Element>
Result<CscArray<std::remove_cv_t<Element>>> ToCsc(
    const ExecutionContext& context, CsrView<Element> source,
    MemoryResource& destination_resource) {
  return internal_sparse_compressed::CompressedToCompressed<
      SparseCompressedFormat::kCsc>(context, source, destination_resource);
}

template <SparseElement Element>
Result<CsrArray<std::remove_cv_t<Element>>> ToCsr(
    const ExecutionContext& context, CscView<Element> source,
    MemoryResource& destination_resource) {
  return internal_sparse_compressed::CompressedToCompressed<
      SparseCompressedFormat::kCsr>(context, source, destination_resource);
}

template <SparseElement Element, SparseCompressedFormat Format>
Result<CoordinateArray<std::remove_cv_t<Element>,
                       Extents<kDynamicExtent, kDynamicExtent>>>
ToCoordinate(const ExecutionContext& context,
             CompressedSparseView<Element, Format> source,
             MemoryResource& destination_resource) {
  using Scalar = std::remove_cv_t<Element>;
  using MatrixExtents = Extents<kDynamicExtent, kDynamicExtent>;
  const Status source_status =
      internal_sparse_compressed::ValidateConversionSource(context, source);
  if (!source_status.ok()) {
    return source_status;
  }
  auto extents = MatrixExtents::Create(source.shape()[0], source.shape()[1]);
  if (!extents.ok()) {
    return extents.status();
  }
  auto builder = CoordinateBuilder<Scalar, MatrixExtents>::Create(
      *extents, source.nnz(), destination_resource);
  if (!builder.ok()) {
    return builder.status();
  }

  const nnz_t* const offsets = source.outer_offset_data();
  const index_t* const indices = source.inner_index_data();
  Element* const values = source.value_data();
  if constexpr (Format == SparseCompressedFormat::kCsr) {
    for (index_t row = 0; row < source.shape()[0]; ++row) {
      const nnz_t begin = offsets[static_cast<std::size_t>(row)];
      const nnz_t end = offsets[static_cast<std::size_t>(row + 1)];
      for (nnz_t position = begin; position < end; ++position) {
        const std::array<index_t, 2> coordinate{
            row, indices[static_cast<std::size_t>(position)]};
        const Status add_status = builder->Add(
            coordinate, values[static_cast<std::size_t>(position)]);
        if (!add_status.ok()) {
          return add_status;
        }
      }
    }
  } else {
    for (index_t row = 0; row < source.shape()[0]; ++row) {
      for (index_t column = 0; column < source.shape()[1]; ++column) {
        const nnz_t begin = offsets[static_cast<std::size_t>(column)];
        const nnz_t end = offsets[static_cast<std::size_t>(column + 1)];
        for (nnz_t position = begin; position < end; ++position) {
          if (indices[static_cast<std::size_t>(position)] != row) {
            continue;
          }
          const std::array<index_t, 2> coordinate{row, column};
          const Status add_status = builder->Add(
              coordinate, values[static_cast<std::size_t>(position)]);
          if (!add_status.ok()) {
            return add_status;
          }
        }
      }
    }
  }
  return builder->Finalize(context, DuplicatePolicy::kReject,
                           ExplicitZeroPolicy::kKeep);
}

static_assert(std::is_trivially_copyable_v<CsrView<float>>);
static_assert(std::is_trivially_copyable_v<CsrView<const float>>);

}  // namespace asc

#endif  // ASC_SPARSE_COMPRESSED_H_
