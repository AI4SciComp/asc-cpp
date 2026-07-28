#ifndef ASC_SPARSE_COORDINATE_H_
#define ASC_SPARSE_COORDINATE_H_

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <utility>

#include "asc/core/contracts.h"
#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/expression/expression.h"
#include "asc/expression/writable.h"

namespace asc {

template <typename T>
concept SparseElement =
    std::same_as<T, std::remove_cv_t<T>> && std::is_arithmetic_v<T> &&
    !std::same_as<T, bool> && std::is_trivially_copyable_v<T> &&
    std::is_trivially_destructible_v<T>;

enum class DuplicatePolicy : std::uint8_t {
  kReject,
  kSum,
};

enum class ExplicitZeroPolicy : std::uint8_t {
  kKeep,
  kDrop,
};

enum class SparseCompressedFormat : std::uint8_t {
  kCsr,
  kCsc,
};

namespace internal_sparse_coordinate {

class ProviderAccess;

template <typename T>
struct IsExtents : std::false_type {};

template <extent_t... StaticExtents>
struct IsExtents<Extents<StaticExtents...>> : std::true_type {};

template <typename T>
inline constexpr bool kIsExtents =
    IsExtents<std::remove_cv_t<T>>::value && !std::is_const_v<T>;

inline Status ValidateSerialHost(const ExecutionContext& context,
                                 const char* operation) {
  if (context.backend() != Backend::kSerial) {
    return Status(ErrorCode::kUnsupported, operation);
  }
  if (!context.CanAccess(MemorySpace::kHost)) {
    return Status(ErrorCode::kMemoryAccess,
                  "Sparse CPU operations require host memory access");
  }
  return Status::Ok();
}

template <std::size_t Rank>
Result<extent_t> ValidateShape(std::span<const extent_t, Rank> extents) {
  bool empty = false;
  for (extent_t extent : extents) {
    if (extent < 0) {
      return Status(ErrorCode::kShape, "A sparse extent cannot be negative");
    }
    empty = empty || extent == 0;
  }
  extent_t logical_size = empty ? 0 : 1;
  if (!empty) {
    for (extent_t extent : extents) {
      auto product = CheckedMultiply(logical_size, extent);
      if (!product.ok()) {
        return product.status();
      }
      logical_size = *product;
    }
  }
  return logical_size;
}

template <std::size_t Rank>
Status ValidateCoordinate(std::span<const index_t, Rank> coordinate,
                          std::span<const extent_t, Rank> extents) {
  for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
    if (coordinate[dimension] < 0 ||
        coordinate[dimension] >= extents[dimension]) {
      return Status(ErrorCode::kIndex,
                    "A sparse coordinate is outside its extent");
    }
  }
  return Status::Ok();
}

template <std::size_t Rank>
int CompareCoordinates(std::span<const index_t, Rank> left,
                       std::span<const index_t, Rank> right) noexcept {
  for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
    if (left[dimension] < right[dimension]) {
      return -1;
    }
    if (left[dimension] > right[dimension]) {
      return 1;
    }
  }
  return 0;
}

inline Result<std::size_t> CoordinateElementCount(nnz_t count,
                                                  std::size_t rank) {
  if (count < 0) {
    return Status(ErrorCode::kInvalidArgument,
                  "A sparse entry count cannot be negative");
  }
  auto converted_count = CheckedCast<std::size_t>(count);
  if (!converted_count.ok()) {
    return converted_count.status();
  }
  return CheckedMultiply(*converted_count, rank);
}

template <typename Element>
Result<Element> AddDuplicate(Element left, Element right) {
  if constexpr (std::integral<Element>) {
    return CheckedAdd(left, right);
  }
  return static_cast<Element>(left + right);
}

inline bool ByteSpanContains(const void* begin, std::size_t size,
                             const void* address) noexcept {
  if (size == 0 || address == nullptr) {
    return false;
  }
  if (begin == nullptr) {
    return true;
  }
  const std::uintptr_t first = reinterpret_cast<std::uintptr_t>(begin);
  if (size > UINTPTR_MAX - first) {
    return true;
  }
  const std::uintptr_t candidate = reinterpret_cast<std::uintptr_t>(address);
  return candidate >= first && candidate < first + size;
}

inline Status ValidateAddressSpan(const void* data, std::size_t size) {
  if (size == 0) {
    return Status::Ok();
  }
  if (data == nullptr) {
    return Status(ErrorCode::kInvalidArgument,
                  "Nonempty sparse storage cannot be null");
  }
  const std::uintptr_t begin = reinterpret_cast<std::uintptr_t>(data);
  if (size > UINTPTR_MAX - begin) {
    return Status(ErrorCode::kOverflow,
                  "Sparse storage address range overflows uintptr_t");
  }
  return Status::Ok();
}

inline bool ByteSpansOverlap(const void* left, std::size_t left_size,
                             const void* right,
                             std::size_t right_size) noexcept {
  if (left_size == 0 || right_size == 0) {
    return false;
  }
  const std::uintptr_t left_begin = reinterpret_cast<std::uintptr_t>(left);
  const std::uintptr_t right_begin = reinterpret_cast<std::uintptr_t>(right);
  if (left_size > UINTPTR_MAX - left_begin ||
      right_size > UINTPTR_MAX - right_begin) {
    return true;
  }
  return left_begin < right_begin + right_size &&
         right_begin < left_begin + left_size;
}

}  // namespace internal_sparse_coordinate

template <typename T>
concept SparseExtents = internal_sparse_coordinate::kIsExtents<T>;

template <typename T>
concept SparseViewElement =
    SparseElement<std::remove_const_t<T>> && !std::is_volatile_v<T>;

template <SparseViewElement Element, std::size_t Rank>
class CoordinateView {
 public:
  using element_type = Element;
  using value_type = std::remove_const_t<Element>;
  static constexpr std::size_t kRank = Rank;

  static Result<CoordinateView> Create(const index_t* coordinates,
                                       Element* values,
                                       std::span<const extent_t, Rank> extents,
                                       nnz_t nonzeros,
                                       MemorySpace memory_space) {
    auto logical_size = internal_sparse_coordinate::ValidateShape(extents);
    if (!logical_size.ok()) {
      return logical_size.status();
    }
    if (nonzeros < 0 || nonzeros > *logical_size) {
      return Status(ErrorCode::kShape,
                    "Sparse NNZ is incompatible with the coordinate shape");
    }
    auto coordinate_count =
        internal_sparse_coordinate::CoordinateElementCount(nonzeros, Rank);
    if (!coordinate_count.ok()) {
      return coordinate_count.status();
    }
    auto coordinate_bytes = CheckedMultiply(*coordinate_count, sizeof(index_t));
    if (!coordinate_bytes.ok()) {
      return coordinate_bytes.status();
    }
    auto value_bytes = CheckedByteCount(nonzeros, sizeof(value_type));
    if (!value_bytes.ok()) {
      return value_bytes.status();
    }
    if ((*coordinate_bytes != 0 && coordinates == nullptr) ||
        (*value_bytes != 0 && values == nullptr)) {
      return Status(ErrorCode::kInvalidArgument,
                    "Nonempty sparse view storage cannot be null");
    }
    Status coordinate_span_status =
        internal_sparse_coordinate::ValidateAddressSpan(coordinates,
                                                        *coordinate_bytes);
    if (!coordinate_span_status.ok()) {
      return coordinate_span_status;
    }
    Status value_span_status =
        internal_sparse_coordinate::ValidateAddressSpan(values, *value_bytes);
    if (!value_span_status.ok()) {
      return value_span_status;
    }
    if (internal_sparse_coordinate::ByteSpansOverlap(
            coordinates, *coordinate_bytes, values, *value_bytes)) {
      return Status(ErrorCode::kInvalidArgument,
                    "Sparse coordinate and value storage cannot overlap");
    }
    std::array<extent_t, Rank> shape{};
    std::copy(extents.begin(), extents.end(), shape.begin());
    CoordinateView result(coordinates, values, shape, nonzeros, memory_space,
                          false);
    if (memory_space == MemorySpace::kHost) {
      Status structure_status = result.ValidateCanonicalStructure();
      if (!structure_status.ok()) {
        return structure_status;
      }
      result.canonical_structure_trusted_ = true;
    }
    return result;
  }

  template <typename OtherElement>
    requires(std::is_const_v<Element> &&
             std::same_as<std::remove_const_t<OtherElement>, value_type> &&
             !std::is_const_v<OtherElement>)
  constexpr CoordinateView(
      const CoordinateView<OtherElement, Rank>& other) noexcept
      : coordinates_(other.coordinates()),
        values_(other.values()),
        extents_(other.shape()),
        nonzeros_(other.nnz()),
        memory_space_(other.memory_space()),
        canonical_structure_trusted_(other.canonical_structure_trusted()) {}

  [[nodiscard]] static constexpr rank_t rank() noexcept {
    return static_cast<rank_t>(Rank);
  }

  [[nodiscard]] constexpr std::span<const extent_t, Rank> extents()
      const noexcept {
    return extents_;
  }

  [[nodiscard]] constexpr const std::array<extent_t, Rank>& shape()
      const noexcept {
    return extents_;
  }

  [[nodiscard]] constexpr nnz_t nnz() const noexcept { return nonzeros_; }

  [[nodiscard]] constexpr const index_t* coordinates() const noexcept {
    return coordinates_;
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
  [[nodiscard]] Result<CoordinateView<ReboundElement, Rank>> RebindValues(
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
    auto coordinate_count =
        internal_sparse_coordinate::CoordinateElementCount(nonzeros_, Rank);
    if (!coordinate_count.ok()) {
      return coordinate_count.status();
    }
    auto coordinate_bytes = CheckedMultiply(*coordinate_count, sizeof(index_t));
    if (!coordinate_bytes.ok()) {
      return coordinate_bytes.status();
    }
    if (internal_sparse_coordinate::ByteSpansOverlap(
            coordinates_, *coordinate_bytes, values, *value_bytes)) {
      return Status(ErrorCode::kInvalidArgument,
                    "Rebound sparse values cannot overlap coordinates");
    }
    if (values != values_ && internal_sparse_coordinate::ByteSpansOverlap(
                                 values_, *value_bytes, values, *value_bytes)) {
      return Status(
          ErrorCode::kInvalidArgument,
          "Rebound sparse values cannot partially overlap existing values");
    }
    return CoordinateView<ReboundElement, Rank>(coordinates_, values, extents_,
                                                nonzeros_, memory_space_,
                                                canonical_structure_trusted_);
  }

  [[nodiscard]] Result<std::span<const index_t, Rank>> Coordinate(
      nnz_t position) const {
    Status access_status = ValidateHostAccess();
    if (!access_status.ok()) {
      return access_status;
    }
    if (position < 0 || position >= nonzeros_) {
      return Status(ErrorCode::kIndex,
                    "Sparse stored-entry position is out of range");
    }
    if constexpr (Rank == 0) {
      return std::span<const index_t, 0>(coordinates_, 0);
    } else {
      const std::size_t offset = static_cast<std::size_t>(position) * Rank;
      return std::span<const index_t, Rank>(coordinates_ + offset, Rank);
    }
  }

  [[nodiscard]] Result<Element*> AtStored(nnz_t position) const {
    Status access_status = ValidateHostAccess();
    if (!access_status.ok()) {
      return access_status;
    }
    if (position < 0 || position >= nonzeros_) {
      return Status(ErrorCode::kIndex,
                    "Sparse stored-entry position is out of range");
    }
    return values_ + static_cast<std::size_t>(position);
  }

  [[nodiscard]] Result<value_type> Lookup(
      std::span<const index_t, Rank> coordinate) const {
    Status access_status = ValidateHostAccess();
    if (!access_status.ok()) {
      return access_status;
    }
    Status coordinate_status =
        internal_sparse_coordinate::ValidateCoordinate(coordinate, extents());
    if (!coordinate_status.ok()) {
      return coordinate_status;
    }
    nnz_t first = 0;
    nnz_t last = nonzeros_;
    while (first < last) {
      const nnz_t middle = first + (last - first) / 2;
      const std::size_t offset = static_cast<std::size_t>(middle) * Rank;
      const index_t* coordinate_pointer = coordinates_;
      if constexpr (Rank != 0) {
        coordinate_pointer += offset;
      }
      const int comparison = internal_sparse_coordinate::CompareCoordinates(
          std::span<const index_t, Rank>(coordinate_pointer, Rank), coordinate);
      if (comparison < 0) {
        first = middle + 1;
      } else {
        last = middle;
      }
    }
    if (first == nonzeros_) {
      return value_type{};
    }
    const std::size_t offset = static_cast<std::size_t>(first) * Rank;
    const index_t* coordinate_pointer = coordinates_;
    if constexpr (Rank != 0) {
      coordinate_pointer += offset;
    }
    if (internal_sparse_coordinate::CompareCoordinates(
            std::span<const index_t, Rank>(coordinate_pointer, Rank),
            coordinate) != 0) {
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
      const CoordinateView& other) const noexcept {
    return coordinates_ == other.coordinates_ && values_ == other.values_ &&
           extents_ == other.extents_ && nonzeros_ == other.nonzeros_ &&
           memory_space_ == other.memory_space_;
  }

 private:
  template <SparseViewElement, std::size_t>
  friend class CoordinateView;
  friend class internal_sparse_coordinate::ProviderAccess;
  template <SparseElement, SparseExtents>
  friend class CoordinateArray;

  constexpr CoordinateView(const index_t* coordinates, Element* values,
                           std::array<extent_t, Rank> extents, nnz_t nonzeros,
                           MemorySpace memory_space,
                           bool canonical_structure_trusted) noexcept
      : coordinates_(coordinates),
        values_(values),
        extents_(extents),
        nonzeros_(nonzeros),
        memory_space_(memory_space),
        canonical_structure_trusted_(canonical_structure_trusted) {}

  [[nodiscard]] Status ValidateHostAccess() const {
    if (memory_space_ != MemorySpace::kHost) {
      return Status(ErrorCode::kMemoryAccess,
                    "Sparse coordinate access requires host storage");
    }
    return Status::Ok();
  }

  [[nodiscard]] Status ValidateCanonicalStructure() const {
    for (nnz_t position = 0; position < nonzeros_; ++position) {
      const std::size_t offset = static_cast<std::size_t>(position) * Rank;
      const index_t* coordinate_pointer = coordinates_;
      if constexpr (Rank != 0) {
        coordinate_pointer += offset;
      }
      const auto coordinate =
          std::span<const index_t, Rank>(coordinate_pointer, Rank);
      Status coordinate_status =
          internal_sparse_coordinate::ValidateCoordinate(coordinate, extents());
      if (!coordinate_status.ok()) {
        return coordinate_status;
      }
      if (position != 0) {
        const std::size_t previous_offset =
            static_cast<std::size_t>(position - 1) * Rank;
        const index_t* previous_pointer = coordinates_;
        if constexpr (Rank != 0) {
          previous_pointer += previous_offset;
        }
        if (internal_sparse_coordinate::CompareCoordinates(
                std::span<const index_t, Rank>(previous_pointer, Rank),
                coordinate) >= 0) {
          return Status(ErrorCode::kInvalidArgument,
                        "Finalized coordinates must be strictly "
                        "lexicographically sorted");
        }
      }
    }
    return Status::Ok();
  }

  const index_t* coordinates_;
  Element* values_;
  std::array<extent_t, Rank> extents_;
  nnz_t nonzeros_;
  MemorySpace memory_space_;
  bool canonical_structure_trusted_;
};

namespace internal_sparse_coordinate {

class ProviderAccess {
 public:
  template <SparseViewElement Element, std::size_t Rank>
  static CoordinateView<Element, Rank> MakeCanonicalView(
      const index_t* coordinates, Element* values,
      std::array<extent_t, Rank> extents, nnz_t nonzeros,
      MemorySpace memory_space) noexcept {
    return CoordinateView<Element, Rank>(coordinates, values, extents, nonzeros,
                                         memory_space, true);
  }
};

}  // namespace internal_sparse_coordinate

template <SparseElement Element, SparseExtents ExtentsType>
class CoordinateBuilder;

template <SparseElement Element, SparseExtents ExtentsType>
class CoordinateArray {
 public:
  using element_type = Element;
  using extents_type = ExtentsType;
  static constexpr std::size_t kRank = ExtentsType::kRank;

  static Result<CoordinateArray> Create(MemoryResource& resource,
                                        ExtentsType extents,
                                        std::span<const index_t> coordinates,
                                        std::span<const Element> values) {
    if (resource.space() != MemorySpace::kHost) {
      return Status(ErrorCode::kUnsupported,
                    "Milestone 4 sparse owners require a host resource");
    }
    auto nonzeros = CheckedCast<nnz_t>(values.size());
    if (!nonzeros.ok()) {
      return nonzeros.status();
    }
    auto coordinate_count =
        internal_sparse_coordinate::CoordinateElementCount(*nonzeros, kRank);
    if (!coordinate_count.ok()) {
      return coordinate_count.status();
    }
    if (coordinates.size() != *coordinate_count) {
      return Status(ErrorCode::kShape,
                    "Coordinate and value storage sizes do not agree");
    }
    std::array<extent_t, kRank> shape{};
    std::copy(extents.values().begin(), extents.values().end(), shape.begin());
    auto validated = CoordinateView<const Element, kRank>::Create(
        coordinates.data(), values.data(), shape, *nonzeros,
        MemorySpace::kHost);
    if (!validated.ok()) {
      return validated.status();
    }
    auto coordinate_bytes =
        CheckedMultiply(coordinates.size(), sizeof(index_t));
    if (!coordinate_bytes.ok()) {
      return coordinate_bytes.status();
    }
    auto value_bytes = CheckedMultiply(values.size(), sizeof(Element));
    if (!value_bytes.ok()) {
      return value_bytes.status();
    }
    auto coordinate_buffer =
        Buffer::Allocate(resource, *coordinate_bytes, alignof(index_t));
    if (!coordinate_buffer.ok()) {
      return coordinate_buffer.status();
    }
    auto value_buffer =
        Buffer::Allocate(resource, *value_bytes, alignof(Element));
    if (!value_buffer.ok()) {
      return value_buffer.status();
    }
    if (!coordinates.empty()) {
      std::copy(coordinates.begin(), coordinates.end(),
                static_cast<index_t*>(coordinate_buffer->data()));
    }
    if (!values.empty()) {
      std::copy(values.begin(), values.end(),
                static_cast<Element*>(value_buffer->data()));
    }
    return CoordinateArray(&resource, std::move(*coordinate_buffer),
                           std::move(*value_buffer), std::move(extents),
                           *nonzeros);
  }

  CoordinateArray(const CoordinateArray&) = delete;
  CoordinateArray& operator=(const CoordinateArray&) = delete;
  CoordinateArray(CoordinateArray&&) noexcept = default;
  CoordinateArray& operator=(CoordinateArray&&) noexcept = default;
  ~CoordinateArray() = default;

  [[nodiscard]] bool valid() const noexcept {
    return resource_ != nullptr && coordinates_.valid() && values_.valid();
  }

  [[nodiscard]] const ExtentsType& extents() const noexcept { return extents_; }

  [[nodiscard]] nnz_t nnz() const noexcept { return nonzeros_; }

  [[nodiscard]] Result<CoordinateView<Element, kRank>> view() {
    if (!valid()) {
      return Status(ErrorCode::kInvalidState,
                    "A moved-from CoordinateArray has no view");
    }
    return CoordinateView<Element, kRank>(
        static_cast<const index_t*>(coordinates_.data()),
        static_cast<Element*>(values_.data()), Shape(), nonzeros_,
        MemorySpace::kHost, true);
  }

  [[nodiscard]] Result<CoordinateView<const Element, kRank>> view() const {
    if (!valid()) {
      return Status(ErrorCode::kInvalidState,
                    "A moved-from CoordinateArray has no view");
    }
    return CoordinateView<const Element, kRank>(
        static_cast<const index_t*>(coordinates_.data()),
        static_cast<const Element*>(values_.data()), Shape(), nonzeros_,
        MemorySpace::kHost, true);
  }

 private:
  friend class CoordinateBuilder<Element, ExtentsType>;

  CoordinateArray(MemoryResource* resource, Buffer coordinates, Buffer values,
                  ExtentsType extents, nnz_t nonzeros) noexcept
      : resource_(resource),
        coordinates_(std::move(coordinates)),
        values_(std::move(values)),
        extents_(std::move(extents)),
        nonzeros_(nonzeros) {}

  [[nodiscard]] std::array<extent_t, kRank> Shape() const noexcept {
    std::array<extent_t, kRank> result{};
    if constexpr (kRank != 0) {
      std::copy(extents_.values().begin(), extents_.values().end(),
                result.begin());
    }
    return result;
  }

  MemoryResource* resource_;
  Buffer coordinates_;
  Buffer values_;
  ExtentsType extents_;
  nnz_t nonzeros_;
};

template <SparseElement Element, SparseExtents ExtentsType>
class CoordinateBuilder {
 public:
  static constexpr std::size_t kRank = ExtentsType::kRank;

  static Result<CoordinateBuilder> Create(MemoryResource& resource,
                                          ExtentsType extents, nnz_t capacity) {
    if (resource.space() != MemorySpace::kHost) {
      return Status(ErrorCode::kUnsupported,
                    "Milestone 4 sparse builders require a host resource");
    }
    if (capacity < 0) {
      return Status(ErrorCode::kInvalidArgument,
                    "CoordinateBuilder capacity cannot be negative");
    }
    if (extents.logical_size() == 0 && capacity != 0) {
      return Status(ErrorCode::kShape,
                    "An empty sparse shape cannot have entry capacity");
    }
    auto coordinate_count =
        internal_sparse_coordinate::CoordinateElementCount(capacity, kRank);
    if (!coordinate_count.ok()) {
      return coordinate_count.status();
    }
    auto coordinate_bytes = CheckedMultiply(*coordinate_count, sizeof(index_t));
    if (!coordinate_bytes.ok()) {
      return coordinate_bytes.status();
    }
    auto value_bytes = CheckedByteCount(capacity, sizeof(Element));
    if (!value_bytes.ok()) {
      return value_bytes.status();
    }
    auto coordinate_buffer =
        Buffer::Allocate(resource, *coordinate_bytes, alignof(index_t));
    if (!coordinate_buffer.ok()) {
      return coordinate_buffer.status();
    }
    auto value_buffer =
        Buffer::Allocate(resource, *value_bytes, alignof(Element));
    if (!value_buffer.ok()) {
      return value_buffer.status();
    }
    return CoordinateBuilder(&resource, std::move(*coordinate_buffer),
                             std::move(*value_buffer), std::move(extents),
                             capacity);
  }

  CoordinateBuilder(const CoordinateBuilder&) = delete;
  CoordinateBuilder& operator=(const CoordinateBuilder&) = delete;
  CoordinateBuilder(CoordinateBuilder&&) noexcept = default;
  CoordinateBuilder& operator=(CoordinateBuilder&&) noexcept = default;
  ~CoordinateBuilder() = default;

  [[nodiscard]] bool valid() const noexcept {
    return resource_ != nullptr && coordinates_.valid() && values_.valid();
  }

  [[nodiscard]] nnz_t size() const noexcept { return size_; }

  [[nodiscard]] nnz_t capacity() const noexcept { return capacity_; }

  [[nodiscard]] const ExtentsType& extents() const noexcept { return extents_; }

  Status Add(std::span<const index_t, kRank> coordinate, Element value) {
    if (!valid()) {
      return Status(ErrorCode::kInvalidState,
                    "A moved-from CoordinateBuilder cannot accept entries");
    }
    Status coordinate_status = internal_sparse_coordinate::ValidateCoordinate(
        coordinate, extents_.values());
    if (!coordinate_status.ok()) {
      return coordinate_status;
    }
    if (size_ == capacity_) {
      return Status(ErrorCode::kInvalidState,
                    "CoordinateBuilder capacity is exhausted");
    }
    const std::size_t position = static_cast<std::size_t>(size_);
    if constexpr (kRank != 0) {
      std::copy(coordinate.begin(), coordinate.end(),
                CoordinateDestination(position));
    }
    ValueData()[position] = value;
    ++size_;
    return Status::Ok();
  }

  [[nodiscard]] Result<CoordinateArray<Element, ExtentsType>> Finalize(
      const ExecutionContext& context, DuplicatePolicy duplicate_policy,
      ExplicitZeroPolicy zero_policy) && {
    if (!valid()) {
      return Status(ErrorCode::kInvalidState,
                    "A moved-from CoordinateBuilder cannot be finalized");
    }
    Status context_status = internal_sparse_coordinate::ValidateSerialHost(
        context, "Sparse finalization requires serial execution");
    if (!context_status.ok()) {
      return context_status;
    }
    if (duplicate_policy != DuplicatePolicy::kReject &&
        duplicate_policy != DuplicatePolicy::kSum) {
      return Status(ErrorCode::kInvalidArgument,
                    "Unknown sparse duplicate policy");
    }
    if (zero_policy != ExplicitZeroPolicy::kKeep &&
        zero_policy != ExplicitZeroPolicy::kDrop) {
      return Status(ErrorCode::kInvalidArgument,
                    "Unknown sparse explicit-zero policy");
    }

    Status preflight_status = PreflightDuplicates(duplicate_policy);
    if (!preflight_status.ok()) {
      return preflight_status;
    }

    StableSort();
    const nnz_t finalized_size = Compact(duplicate_policy, zero_policy);
    size_ = finalized_size;
    CoordinateArray<Element, ExtentsType> result(
        resource_, std::move(coordinates_), std::move(values_),
        std::move(extents_), size_);
    resource_ = nullptr;
    capacity_ = 0;
    size_ = 0;
    return result;
  }

 private:
  CoordinateBuilder(MemoryResource* resource, Buffer coordinates, Buffer values,
                    ExtentsType extents, nnz_t capacity) noexcept
      : resource_(resource),
        coordinates_(std::move(coordinates)),
        values_(std::move(values)),
        extents_(std::move(extents)),
        capacity_(capacity) {}

  [[nodiscard]] index_t* CoordinateData() noexcept {
    return static_cast<index_t*>(coordinates_.data());
  }

  [[nodiscard]] const index_t* CoordinateData() const noexcept {
    return static_cast<const index_t*>(coordinates_.data());
  }

  [[nodiscard]] Element* ValueData() noexcept {
    return static_cast<Element*>(values_.data());
  }

  [[nodiscard]] const Element* ValueData() const noexcept {
    return static_cast<const Element*>(values_.data());
  }

  [[nodiscard]] index_t* CoordinateDestination(std::size_t position) noexcept {
    if constexpr (kRank == 0) {
      return CoordinateData();
    } else {
      return CoordinateData() + position * kRank;
    }
  }

  [[nodiscard]] std::span<const index_t, kRank> CoordinateAt(
      nnz_t position) const noexcept {
    const index_t* pointer = CoordinateData();
    if constexpr (kRank != 0) {
      pointer += static_cast<std::size_t>(position) * kRank;
    }
    return std::span<const index_t, kRank>(pointer, kRank);
  }

  [[nodiscard]] Status PreflightDuplicates(
      DuplicatePolicy duplicate_policy) const {
    for (nnz_t first = 0; first < size_; ++first) {
      bool was_seen = false;
      for (nnz_t earlier = 0; earlier < first; ++earlier) {
        if (internal_sparse_coordinate::CompareCoordinates(
                CoordinateAt(first), CoordinateAt(earlier)) == 0) {
          was_seen = true;
          break;
        }
      }
      if (was_seen) {
        continue;
      }
      Element sum = ValueData()[static_cast<std::size_t>(first)];
      bool duplicate = false;
      for (nnz_t later = first + 1; later < size_; ++later) {
        if (internal_sparse_coordinate::CompareCoordinates(
                CoordinateAt(first), CoordinateAt(later)) != 0) {
          continue;
        }
        duplicate = true;
        if (duplicate_policy == DuplicatePolicy::kSum) {
          auto next = internal_sparse_coordinate::AddDuplicate(
              sum, ValueData()[static_cast<std::size_t>(later)]);
          if (!next.ok()) {
            return next.status();
          }
          sum = *next;
        }
      }
      if (duplicate && duplicate_policy == DuplicatePolicy::kReject) {
        return Status(ErrorCode::kInvalidArgument,
                      "Duplicate sparse coordinates were rejected");
      }
    }
    return Status::Ok();
  }

  void StableSort() noexcept {
    for (nnz_t position = 1; position < size_; ++position) {
      const std::size_t source = static_cast<std::size_t>(position);
      std::array<index_t, kRank> coordinate{};
      if constexpr (kRank != 0) {
        std::copy_n(CoordinateDestination(source), kRank, coordinate.begin());
      }
      const Element value = ValueData()[source];
      nnz_t insertion = position;
      while (insertion > 0) {
        const std::size_t previous = static_cast<std::size_t>(insertion - 1);
        if (internal_sparse_coordinate::CompareCoordinates(
                CoordinateAt(static_cast<nnz_t>(previous)),
                std::span<const index_t, kRank>(coordinate)) <= 0) {
          break;
        }
        if constexpr (kRank != 0) {
          std::copy_n(
              CoordinateDestination(previous), kRank,
              CoordinateDestination(static_cast<std::size_t>(insertion)));
        }
        ValueData()[static_cast<std::size_t>(insertion)] =
            ValueData()[previous];
        --insertion;
      }
      if constexpr (kRank != 0) {
        std::copy(coordinate.begin(), coordinate.end(),
                  CoordinateDestination(static_cast<std::size_t>(insertion)));
      }
      ValueData()[static_cast<std::size_t>(insertion)] = value;
    }
  }

  [[nodiscard]] nnz_t Compact(DuplicatePolicy duplicate_policy,
                              ExplicitZeroPolicy zero_policy) noexcept {
    nnz_t output = 0;
    nnz_t input = 0;
    while (input < size_) {
      const nnz_t group_begin = input;
      Element value = ValueData()[static_cast<std::size_t>(input++)];
      while (input < size_ &&
             internal_sparse_coordinate::CompareCoordinates(
                 CoordinateAt(group_begin), CoordinateAt(input)) == 0) {
        if (duplicate_policy == DuplicatePolicy::kSum) {
          if constexpr (std::integral<Element>) {
            value = static_cast<Element>(
                value + ValueData()[static_cast<std::size_t>(input)]);
          } else {
            value += ValueData()[static_cast<std::size_t>(input)];
          }
        }
        ++input;
      }
      if (zero_policy == ExplicitZeroPolicy::kDrop && value == Element{}) {
        continue;
      }
      const std::size_t destination = static_cast<std::size_t>(output);
      const std::size_t source = static_cast<std::size_t>(group_begin);
      if (destination != source) {
        if constexpr (kRank != 0) {
          std::copy_n(CoordinateDestination(source), kRank,
                      CoordinateDestination(destination));
        }
      }
      ValueData()[destination] = value;
      ++output;
    }
    return output;
  }

  MemoryResource* resource_;
  Buffer coordinates_;
  Buffer values_;
  ExtentsType extents_;
  nnz_t capacity_;
  nnz_t size_ = 0;
};

template <typename Element, std::size_t Rank>
struct ExpressionAdapter<CoordinateView<Element, Rank>> {
  using value_type = std::remove_const_t<Element>;
  static constexpr rank_t rank = static_cast<rank_t>(Rank);
  static constexpr SparsityEffect sparsity_effect =
      SparsityEffect::kStructurePreserving;
  static constexpr ExpressionOperation operation =
      ExpressionOperation::kTerminal;

  static constexpr std::array<extent_t, Rank> Shape(
      const CoordinateView<Element, Rank>& view) noexcept {
    return view.shape();
  }

  static value_type Read(const CoordinateView<Element, Rank>& view,
                         std::span<const index_t, Rank> coordinate) {
    auto value = view.Lookup(coordinate);
    ASC_CHECK_MESSAGE(value.ok(),
                      "Sparse expression read requires valid host coordinates");
    return *value;
  }

  static bool MayAlias(const CoordinateView<Element, Rank>& view,
                       AliasToken token) noexcept {
    const auto value_alias = view.ValueAlias();
    if (token.identity() == value_alias.identity() ||
        internal_sparse_coordinate::ByteSpanContains(
            value_alias.data(), value_alias.size(), token.identity())) {
      return true;
    }
    auto coordinate_count =
        internal_sparse_coordinate::CoordinateElementCount(view.nnz(), Rank);
    if (!coordinate_count.ok()) {
      return true;
    }
    auto coordinate_bytes = CheckedMultiply(*coordinate_count, sizeof(index_t));
    if (!coordinate_bytes.ok()) {
      return true;
    }
    return internal_sparse_coordinate::ByteSpanContains(
        view.coordinates(), *coordinate_bytes, token.identity());
  }

  static Status ValidateAccess(const CoordinateView<Element, Rank>& view,
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

template <typename Element, std::size_t Rank>
struct ExpressionPlacementAdapter<CoordinateView<Element, Rank>> {
  static constexpr MemorySpace Space(
      const CoordinateView<Element, Rank>& view) noexcept {
    return view.memory_space();
  }

  static constexpr ExpressionAliasMetadata Alias(
      const CoordinateView<Element, Rank>& view) noexcept {
    return view.ValueAlias();
  }
};

template <typename Element, std::size_t Rank>
  requires(!std::is_const_v<Element>)
struct WritableExpressionAdapter<CoordinateView<Element, Rank>> {
  using value_type = std::remove_const_t<Element>;

  static constexpr std::array<extent_t, Rank> Shape(
      const CoordinateView<Element, Rank>& view) noexcept {
    return view.shape();
  }

  static constexpr bool IsUnique(
      const CoordinateView<Element, Rank>&) noexcept {
    return true;
  }

  static constexpr ExpressionAliasMetadata Alias(
      const CoordinateView<Element, Rank>& view) noexcept {
    return view.ValueAlias();
  }

  static void Write(CoordinateView<Element, Rank>& view,
                    std::span<const index_t, Rank> coordinate,
                    value_type value) {
    auto existing = view.Lookup(coordinate);
    ASC_CHECK_MESSAGE(existing.ok(),
                      "Sparse expression write requires valid coordinates");
    // Sparse evaluators write only stored coordinates. Locate the entry
    // without exposing structural mutation through the writable protocol.
    for (nnz_t position = 0; position < view.nnz(); ++position) {
      auto stored_coordinate = view.Coordinate(position);
      ASC_CHECK_MESSAGE(stored_coordinate.ok(),
                        "Sparse stored coordinate must be accessible");
      if (internal_sparse_coordinate::CompareCoordinates(*stored_coordinate,
                                                         coordinate) == 0) {
        auto stored_value = view.AtStored(position);
        ASC_CHECK_MESSAGE(stored_value.ok(),
                          "Sparse stored value must be accessible");
        **stored_value = value;
        return;
      }
    }
    ASC_CHECK_MESSAGE(false,
                      "Sparse writable protocol requires a stored coordinate");
  }

  static Status ValidateAccess(const CoordinateView<Element, Rank>& view,
                               const ExecutionContext& context) {
    return ExpressionAdapter<CoordinateView<Element, Rank>>::ValidateAccess(
        view, context);
  }
};

static_assert(std::is_trivially_copyable_v<CoordinateView<float, 1>>);

}  // namespace asc

#endif  // ASC_SPARSE_COORDINATE_H_
