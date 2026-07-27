#ifndef ASC_SPARSE_COORDINATE_H_
#define ASC_SPARSE_COORDINATE_H_

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

namespace asc {

template <typename Element>
concept SparseElement =
    std::is_arithmetic_v<std::remove_cv_t<Element>> &&
    !std::same_as<std::remove_cv_t<Element>, bool> &&
    !std::is_volatile_v<Element> &&
    std::is_trivially_copyable_v<std::remove_cv_t<Element>> &&
    std::is_trivially_destructible_v<std::remove_cv_t<Element>>;

enum class DuplicatePolicy : std::uint8_t {
  kReject = 0,
  kSum = 1,
};

enum class ExplicitZeroPolicy : std::uint8_t {
  kKeep = 0,
  kDrop = 1,
};

namespace internal_sparse_coordinate {

template <typename T>
struct IsCoreExtents : std::false_type {};

template <extent_t... StaticExtents>
struct IsCoreExtents<Extents<StaticExtents...>> : std::true_type {};

inline Status ValidateMemorySpace(MemorySpace space) {
  switch (space) {
    case MemorySpace::kHost:
    case MemorySpace::kPinnedHost:
    case MemorySpace::kDevice:
    case MemorySpace::kManaged:
      return Status::Ok();
  }
  return Status(ErrorCode::kInvalidArgument,
                "A sparse view has an invalid memory space");
}

template <std::size_t Rank>
Status ValidateShape(std::span<const extent_t, Rank> shape, nnz_t nnz) {
  bool has_zero_extent = false;
  for (extent_t extent : shape) {
    if (extent < 0) {
      return Status(ErrorCode::kShape, "A sparse extent cannot be negative");
    }
    has_zero_extent = has_zero_extent || extent == 0;
  }
  if (nnz < 0) {
    return Status(ErrorCode::kInvalidArgument,
                  "A sparse NNZ count cannot be negative");
  }
  if (has_zero_extent && nnz != 0) {
    return Status(ErrorCode::kShape,
                  "A zero-extent sparse domain cannot contain an entry");
  }
  if constexpr (Rank == 0) {
    if (nnz > 1) {
      return Status(ErrorCode::kInvalidArgument,
                    "A canonical rank-zero sparse object has at most one "
                    "entry");
    }
  }
  return Status::Ok();
}

inline Status ValidateAddressSpan(const void* pointer, std::size_t bytes,
                                  std::size_t alignment,
                                  const char* null_diagnostic,
                                  const char* alignment_diagnostic,
                                  const char* span_diagnostic) {
  if (bytes != 0 && pointer == nullptr) {
    return Status(ErrorCode::kMemoryAccess, null_diagnostic);
  }
  if (pointer == nullptr) {
    return Status::Ok();
  }
  const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(pointer);
  if (address % alignment != 0) {
    return Status(ErrorCode::kMemoryAccess, alignment_diagnostic);
  }
  if (bytes > std::numeric_limits<std::uintptr_t>::max() - address) {
    return Status(ErrorCode::kOverflow, span_diagnostic);
  }
  return Status::Ok();
}

inline bool ByteSpansOverlap(const void* left_pointer, std::size_t left_bytes,
                             const void* right_pointer,
                             std::size_t right_bytes) noexcept {
  if (left_bytes == 0 || right_bytes == 0) {
    return false;
  }
  const std::uintptr_t left = reinterpret_cast<std::uintptr_t>(left_pointer);
  const std::uintptr_t right = reinterpret_cast<std::uintptr_t>(right_pointer);
  return left < right + right_bytes && right < left + left_bytes;
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

template <std::size_t Rank>
bool CoordinatesEqual(std::span<const index_t, Rank> left,
                      std::span<const index_t, Rank> right) noexcept {
  return CompareCoordinates(left, right) == 0;
}

template <std::size_t Rank>
std::span<const index_t, Rank> CoordinateAt(const index_t* coordinates,
                                            nnz_t position) noexcept {
  if constexpr (Rank == 0) {
    static_cast<void>(coordinates);
    static_cast<void>(position);
    return std::span<const index_t, 0>();
  } else {
    const auto offset =
        static_cast<std::size_t>(position) * static_cast<std::size_t>(Rank);
    return std::span<const index_t, Rank>(coordinates + offset, Rank);
  }
}

template <std::size_t Rank>
std::span<index_t, Rank> MutableCoordinateAt(index_t* coordinates,
                                             nnz_t position) noexcept {
  if constexpr (Rank == 0) {
    static_cast<void>(coordinates);
    static_cast<void>(position);
    return std::span<index_t, 0>();
  } else {
    const auto offset =
        static_cast<std::size_t>(position) * static_cast<std::size_t>(Rank);
    return std::span<index_t, Rank>(coordinates + offset, Rank);
  }
}

template <std::integral Element>
Result<Element> AddValues(Element left, Element right) {
  return CheckedAdd(left, right);
}

template <std::floating_point Element>
Result<Element> AddValues(Element left, Element right) {
  return static_cast<Element>(left + right);
}

}  // namespace internal_sparse_coordinate

template <typename T>
concept SparseExtents = internal_sparse_coordinate::IsCoreExtents<T>::value;

template <SparseElement Element, SparseExtents ExtentsType>
class CoordinateArray;

namespace internal_sparse_coordinate {

template <SparseElement Element, SparseExtents ExtentsType>
struct ArrayFactory;
struct ViewAccess;

}  // namespace internal_sparse_coordinate

template <SparseElement Element, std::size_t Rank>
class CoordinateView {
 public:
  using element_type = Element;
  using value_type = std::remove_cv_t<Element>;
  using ShapeType = std::array<extent_t, Rank>;

  static Result<CoordinateView> Create(const index_t* coordinates,
                                       Element* values,
                                       std::span<const extent_t, Rank> shape,
                                       nnz_t nnz, MemorySpace space) {
    const Status space_status =
        internal_sparse_coordinate::ValidateMemorySpace(space);
    if (!space_status.ok()) {
      return space_status;
    }
    const Status shape_status =
        internal_sparse_coordinate::ValidateShape(shape, nnz);
    if (!shape_status.ok()) {
      return shape_status;
    }

    auto nnz_size = CheckedCast<std::size_t>(nnz);
    if (!nnz_size.ok()) {
      return nnz_size.status();
    }
    auto coordinate_count = CheckedMultiply(*nnz_size, Rank);
    if (!coordinate_count.ok()) {
      return Status(ErrorCode::kOverflow, "A coordinate count exceeds size_t");
    }
    auto coordinate_bytes = CheckedMultiply(*coordinate_count, sizeof(index_t));
    if (!coordinate_bytes.ok()) {
      return Status(ErrorCode::kOverflow,
                    "A coordinate byte span exceeds size_t");
    }
    auto value_bytes = CheckedMultiply(*nnz_size, sizeof(value_type));
    if (!value_bytes.ok()) {
      return Status(ErrorCode::kOverflow,
                    "A sparse value byte span exceeds size_t");
    }
    const Status coordinate_span_status =
        internal_sparse_coordinate::ValidateAddressSpan(
            coordinates, *coordinate_bytes, alignof(index_t),
            "A nonempty coordinate span cannot have a null pointer",
            "A coordinate pointer does not satisfy index alignment",
            "A coordinate address span exceeds uintptr_t");
    if (!coordinate_span_status.ok()) {
      return coordinate_span_status;
    }
    const Status value_span_status =
        internal_sparse_coordinate::ValidateAddressSpan(
            values, *value_bytes, alignof(value_type),
            "A nonempty sparse value span cannot have a null pointer",
            "A sparse value pointer does not satisfy element alignment",
            "A sparse value address span exceeds uintptr_t");
    if (!value_span_status.ok()) {
      return value_span_status;
    }
    if (internal_sparse_coordinate::ByteSpansOverlap(
            coordinates, *coordinate_bytes, values, *value_bytes)) {
      return Status(
          ErrorCode::kInvalidArgument,
          "Sparse coordinate structure and value spans must be disjoint");
    }

    ShapeType shape_values{};
    for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
      shape_values[dimension] = shape[dimension];
    }
    if (space == MemorySpace::kHost) {
      for (nnz_t position = 0; position < nnz; ++position) {
        const auto coordinate = internal_sparse_coordinate::CoordinateAt<Rank>(
            coordinates, position);
        for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
          if (coordinate[dimension] < 0 ||
              coordinate[dimension] >= shape_values[dimension]) {
            return Status(ErrorCode::kIndex,
                          "A sparse coordinate is outside its extent");
          }
        }
        if (position != 0) {
          const auto previous = internal_sparse_coordinate::CoordinateAt<Rank>(
              coordinates, position - 1);
          if (internal_sparse_coordinate::CompareCoordinates(previous,
                                                             coordinate) >= 0) {
            return Status(ErrorCode::kInvalidArgument,
                          "Canonical coordinates must be sorted and unique");
          }
        }
      }
    }
    auto alias = AliasToken::FromAddressSpan(values, *value_bytes);
    if (!alias.ok()) {
      return alias.status();
    }
    return CoordinateView(coordinates, values, shape_values, nnz, space, *alias,
                          space == MemorySpace::kHost);
  }

  template <SparseElement MutableElement>
    requires std::is_const_v<Element> && (!std::is_const_v<MutableElement>) &&
                 std::same_as<std::remove_const_t<Element>, MutableElement>
  constexpr CoordinateView(
      const CoordinateView<MutableElement, Rank>& mutable_view) noexcept
      : coordinates_(mutable_view.coordinates_),
        values_(mutable_view.values_),
        shape_(mutable_view.shape_),
        nnz_(mutable_view.nnz_),
        space_(mutable_view.space_),
        alias_(mutable_view.alias_),
        trusted_provenance_(mutable_view.trusted_provenance_) {}

  [[nodiscard]] static constexpr rank_t rank() noexcept {
    static_assert(Rank <= std::numeric_limits<rank_t>::max());
    return static_cast<rank_t>(Rank);
  }
  [[nodiscard]] constexpr const ShapeType& shape() const noexcept {
    return shape_;
  }
  [[nodiscard]] constexpr nnz_t nnz() const noexcept { return nnz_; }
  [[nodiscard]] constexpr const index_t* coordinate_data() const noexcept {
    return coordinates_;
  }
  [[nodiscard]] constexpr Element* value_data() const noexcept {
    return values_;
  }
  [[nodiscard]] constexpr MemorySpace space() const noexcept { return space_; }
  [[nodiscard]] constexpr AliasToken alias_token() const noexcept {
    return alias_;
  }

  // Reuses this view's immutable canonical structure with a distinct exact
  // value span. Canonical provenance is preserved but does not imply that
  // asynchronous production of the structure has completed.
  template <SparseElement NewElement>
    requires std::same_as<std::remove_const_t<NewElement>, value_type>
  [[nodiscard]] Result<CoordinateView<NewElement, Rank>> RebindValues(
      std::span<NewElement> values) const {
    auto count = CheckedCast<std::size_t>(nnz_);
    if (!count.ok()) {
      return count.status();
    }
    if (values.size() != *count) {
      return Status(ErrorCode::kInvalidArgument,
                    "A rebound coordinate value span must have exact NNZ "
                    "length");
    }
    auto rebound = CoordinateView<NewElement, Rank>::Create(
        coordinates_, values.data(), shape_, nnz_, space_);
    if (!rebound.ok()) {
      return rebound.status();
    }
    rebound->trusted_provenance_ = trusted_provenance_;
    return std::move(*rebound);
  }

  [[nodiscard]] Result<std::span<const index_t, Rank>> CoordinateAt(
      nnz_t position) const {
    const Status access_status = ValidateStoredAccess(position);
    if (!access_status.ok()) {
      return access_status;
    }
    return internal_sparse_coordinate::CoordinateAt<Rank>(coordinates_,
                                                          position);
  }

  [[nodiscard]] Result<Element*> ValueAt(nnz_t position) const {
    const Status access_status = ValidateStoredAccess(position);
    if (!access_status.ok()) {
      return access_status;
    }
    return values_ + static_cast<std::size_t>(position);
  }

  [[nodiscard]] Result<Element*> Find(
      std::span<const index_t, Rank> coordinate) const {
    if (space_ != MemorySpace::kHost) {
      return Status(ErrorCode::kMemoryAccess,
                    "Sparse values are host-dereferenceable only in host "
                    "memory");
    }
    for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
      if (coordinate[dimension] < 0 ||
          coordinate[dimension] >= shape_[dimension]) {
        return Status(ErrorCode::kIndex,
                      "A sparse coordinate is outside its extent");
      }
    }
    nnz_t first = 0;
    nnz_t last = nnz_;
    while (first < last) {
      const nnz_t middle = first + (last - first) / 2;
      const int comparison = internal_sparse_coordinate::CompareCoordinates(
          internal_sparse_coordinate::CoordinateAt<Rank>(coordinates_, middle),
          coordinate);
      if (comparison < 0) {
        first = middle + 1;
      } else {
        last = middle;
      }
    }
    if (first == nnz_ ||
        !internal_sparse_coordinate::CoordinatesEqual(
            internal_sparse_coordinate::CoordinateAt<Rank>(coordinates_, first),
            coordinate)) {
      return static_cast<Element*>(nullptr);
    }
    return values_ + static_cast<std::size_t>(first);
  }

  template <SparseElement OtherElement>
    requires std::same_as<std::remove_cv_t<Element>,
                          std::remove_cv_t<OtherElement>>
  [[nodiscard]] constexpr bool IsExactView(
      const CoordinateView<OtherElement, Rank>& other) const noexcept {
    return coordinates_ == other.coordinate_data() &&
           values_ == other.value_data() && shape_ == other.shape() &&
           nnz_ == other.nnz() && space_ == other.space();
  }

 private:
  template <SparseElement, std::size_t>
  friend class CoordinateView;
  friend struct ExpressionAdapter<CoordinateView<Element, Rank>>;
  friend struct WritableExpressionAdapter<CoordinateView<Element, Rank>>;
  friend struct internal_sparse_coordinate::ViewAccess;

  [[nodiscard]] Status ValidateStoredAccess(nnz_t position) const {
    if (space_ != MemorySpace::kHost) {
      return Status(ErrorCode::kMemoryAccess,
                    "Sparse values are host-dereferenceable only in host "
                    "memory");
    }
    if (position < 0 || position >= nnz_) {
      return Status(ErrorCode::kIndex,
                    "A stored-entry position is outside the sparse view");
    }
    return Status::Ok();
  }

  [[nodiscard]] value_type ReadUnchecked(
      std::span<const index_t, Rank> coordinate) const noexcept {
    nnz_t first = 0;
    nnz_t last = nnz_;
    while (first < last) {
      const nnz_t middle = first + (last - first) / 2;
      const int comparison = internal_sparse_coordinate::CompareCoordinates(
          internal_sparse_coordinate::CoordinateAt<Rank>(coordinates_, middle),
          coordinate);
      if (comparison < 0) {
        first = middle + 1;
      } else {
        last = middle;
      }
    }
    if (first == nnz_ ||
        !internal_sparse_coordinate::CoordinatesEqual(
            internal_sparse_coordinate::CoordinateAt<Rank>(coordinates_, first),
            coordinate)) {
      return value_type{};
    }
    return values_[static_cast<std::size_t>(first)];
  }

  void WriteUnchecked(nnz_t position, value_type value) const noexcept
    requires(!std::is_const_v<Element>)
  {
    values_[static_cast<std::size_t>(position)] = value;
  }

  constexpr CoordinateView(const index_t* coordinates, Element* values,
                           ShapeType shape, nnz_t nnz, MemorySpace space,
                           AliasToken alias, bool trusted_provenance) noexcept
      : coordinates_(coordinates),
        values_(values),
        shape_(shape),
        nnz_(nnz),
        space_(space),
        alias_(alias),
        trusted_provenance_(trusted_provenance) {}

  const index_t* coordinates_;
  Element* values_;
  ShapeType shape_;
  nnz_t nnz_;
  MemorySpace space_;
  AliasToken alias_;
  bool trusted_provenance_ = false;
};

namespace internal_sparse_coordinate {

struct ViewAccess {
  template <SparseElement Element, std::size_t Rank>
  [[nodiscard]] static constexpr bool Trusted(
      CoordinateView<Element, Rank> view) noexcept {
    return view.trusted_provenance_;
  }

 private:
  template <SparseElement, SparseExtents>
  friend class ::asc::CoordinateArray;

  template <SparseElement Element, std::size_t Rank>
  static Result<CoordinateView<Element, Rank>> CreateTrusted(
      const index_t* coordinates, Element* values,
      std::span<const extent_t, Rank> shape, nnz_t nnz, MemorySpace space) {
    auto view = CoordinateView<Element, Rank>::Create(coordinates, values,
                                                      shape, nnz, space);
    if (!view.ok()) {
      return view.status();
    }
    return CoordinateView<Element, Rank>(view->coordinates_, view->values_,
                                         view->shape_, view->nnz_, view->space_,
                                         view->alias_, true);
  }
};

}  // namespace internal_sparse_coordinate

template <SparseElement Element, SparseExtents ExtentsType>
class CoordinateBuilder {
  static_assert(!std::is_const_v<Element>,
                "CoordinateBuilder owns mutable, non-const values");

 public:
  static constexpr std::size_t kRank = ExtentsType::kRank;

  static Result<CoordinateBuilder> Create(const ExtentsType& extents,
                                          nnz_t capacity,
                                          MemoryResource& resource) {
    if (resource.space() != MemorySpace::kHost) {
      return Status(
          ErrorCode::kUnsupported,
          "Milestone 4 coordinate construction supports host memory only");
    }
    if (capacity < 0) {
      return Status(ErrorCode::kInvalidArgument,
                    "A coordinate capacity cannot be negative");
    }
    const Status shape_status =
        internal_sparse_coordinate::ValidateShape(extents.values(), 0);
    if (!shape_status.ok()) {
      return shape_status;
    }
    auto capacity_size = CheckedCast<std::size_t>(capacity);
    if (!capacity_size.ok()) {
      return capacity_size.status();
    }
    auto coordinate_count = CheckedMultiply(*capacity_size, kRank);
    if (!coordinate_count.ok()) {
      return Status(ErrorCode::kOverflow,
                    "A coordinate capacity exceeds size_t");
    }
    auto coordinate_bytes = CheckedMultiply(*coordinate_count, sizeof(index_t));
    if (!coordinate_bytes.ok()) {
      return Status(ErrorCode::kOverflow,
                    "A coordinate capacity byte count exceeds size_t");
    }
    auto value_bytes = CheckedMultiply(*capacity_size, sizeof(Element));
    if (!value_bytes.ok()) {
      return Status(ErrorCode::kOverflow,
                    "A sparse value capacity byte count exceeds size_t");
    }
    auto coordinates =
        Buffer::Allocate(resource, *coordinate_bytes, alignof(index_t));
    if (!coordinates.ok()) {
      return coordinates.status();
    }
    auto values = Buffer::Allocate(resource, *value_bytes, alignof(Element));
    if (!values.ok()) {
      return values.status();
    }
    return CoordinateBuilder(extents, capacity, std::move(*coordinates),
                             std::move(*values), &resource);
  }

  CoordinateBuilder(const CoordinateBuilder&) = delete;
  CoordinateBuilder& operator=(const CoordinateBuilder&) = delete;

  CoordinateBuilder(CoordinateBuilder&& other) noexcept
      : extents_(std::move(other.extents_)),
        capacity_(std::exchange(other.capacity_, 0)),
        size_(std::exchange(other.size_, 0)),
        coordinates_(std::move(other.coordinates_)),
        values_(std::move(other.values_)),
        resource_(std::exchange(other.resource_, nullptr)) {}

  CoordinateBuilder& operator=(CoordinateBuilder&& other) noexcept {
    if (this != &other) {
      extents_ = std::move(other.extents_);
      capacity_ = std::exchange(other.capacity_, 0);
      size_ = std::exchange(other.size_, 0);
      coordinates_ = std::move(other.coordinates_);
      values_ = std::move(other.values_);
      resource_ = std::exchange(other.resource_, nullptr);
    }
    return *this;
  }

  ~CoordinateBuilder() = default;

  [[nodiscard]] const ExtentsType& extents() const noexcept { return extents_; }
  [[nodiscard]] nnz_t capacity() const noexcept { return capacity_; }
  [[nodiscard]] nnz_t size() const noexcept { return size_; }
  [[nodiscard]] MemoryResource* resource() const noexcept { return resource_; }

  Status Add(std::span<const index_t, kRank> coordinate, Element value) {
    if (resource_ == nullptr) {
      return Status(ErrorCode::kInvalidState,
                    "A moved-from coordinate builder cannot accept entries");
    }
    if (size_ == capacity_) {
      return Status(ErrorCode::kInvalidState,
                    "A coordinate builder has reached its declared capacity");
    }
    for (std::size_t dimension = 0; dimension < kRank; ++dimension) {
      if (coordinate[dimension] < 0 ||
          coordinate[dimension] >= extents_.values()[dimension]) {
        return Status(ErrorCode::kIndex,
                      "A coordinate is outside the builder extents");
      }
    }

    auto destination = MutableCoordinate(size_);
    for (std::size_t dimension = 0; dimension < kRank; ++dimension) {
      std::construct_at(destination.data() + dimension, coordinate[dimension]);
    }
    std::construct_at(MutableValues() + static_cast<std::size_t>(size_), value);
    ++size_;
    return Status::Ok();
  }

  Result<CoordinateArray<Element, ExtentsType>> Finalize(
      const ExecutionContext& context, DuplicatePolicy duplicate_policy,
      ExplicitZeroPolicy zero_policy) {
    if (resource_ == nullptr) {
      return Status(ErrorCode::kInvalidState,
                    "A moved-from coordinate builder cannot be finalized");
    }
    if (context.backend() != Backend::kSerial) {
      return Status(
          ErrorCode::kUnsupported,
          "Milestone 4 coordinate finalization requires serial execution");
    }
    if (!context.CanAccess(resource_->space())) {
      return Status(ErrorCode::kMemoryAccess,
                    "Serial coordinate finalization requires host memory");
    }
    switch (duplicate_policy) {
      case DuplicatePolicy::kReject:
      case DuplicatePolicy::kSum:
        break;
      default:
        return Status(ErrorCode::kInvalidArgument,
                      "DuplicatePolicy is not a recognized enumerator");
    }
    switch (zero_policy) {
      case ExplicitZeroPolicy::kKeep:
      case ExplicitZeroPolicy::kDrop:
        break;
      default:
        return Status(ErrorCode::kInvalidArgument,
                      "ExplicitZeroPolicy is not a recognized enumerator");
    }

    const Status duplicate_status =
        ValidateDuplicatesBeforeMutation(duplicate_policy);
    if (!duplicate_status.ok()) {
      return duplicate_status;
    }

    StableSort();
    const nnz_t finalized_size = CompactSorted(duplicate_policy, zero_policy);
    auto result = CoordinateArray<Element, ExtentsType>(
        extents_, finalized_size, std::move(coordinates_), std::move(values_),
        resource_);
    capacity_ = 0;
    size_ = 0;
    resource_ = nullptr;
    return result;
  }

 private:
  friend class CoordinateArray<Element, ExtentsType>;

  [[nodiscard]] index_t* MutableCoordinates() noexcept {
    return static_cast<index_t*>(coordinates_.data());
  }
  [[nodiscard]] Element* MutableValues() noexcept {
    return static_cast<Element*>(values_.data());
  }
  [[nodiscard]] std::span<index_t, kRank> MutableCoordinate(
      nnz_t position) noexcept {
    return internal_sparse_coordinate::MutableCoordinateAt<kRank>(
        MutableCoordinates(), position);
  }
  [[nodiscard]] std::span<const index_t, kRank> Coordinate(
      nnz_t position) const noexcept {
    return internal_sparse_coordinate::CoordinateAt<kRank>(
        static_cast<const index_t*>(coordinates_.data()), position);
  }

  Status ValidateDuplicatesBeforeMutation(
      DuplicatePolicy duplicate_policy) const {
    for (nnz_t position = 0; position < size_; ++position) {
      bool has_earlier_equal = false;
      for (nnz_t earlier = 0; earlier < position; ++earlier) {
        if (internal_sparse_coordinate::CoordinatesEqual(
                Coordinate(earlier), Coordinate(position))) {
          has_earlier_equal = true;
          break;
        }
      }
      if (has_earlier_equal) {
        continue;
      }
      Element combined = static_cast<const Element*>(
          values_.data())[static_cast<std::size_t>(position)];
      for (nnz_t later = position + 1; later < size_; ++later) {
        if (!internal_sparse_coordinate::CoordinatesEqual(Coordinate(position),
                                                          Coordinate(later))) {
          continue;
        }
        if (duplicate_policy == DuplicatePolicy::kReject) {
          return Status(ErrorCode::kInvalidArgument,
                        "Coordinate finalization rejected a duplicate entry");
        }
        auto sum = internal_sparse_coordinate::AddValues(
            combined, static_cast<const Element*>(
                          values_.data())[static_cast<std::size_t>(later)]);
        if (!sum.ok()) {
          return Status(
              ErrorCode::kOverflow,
              "Integral duplicate summation overflowed during finalization");
        }
        combined = *sum;
      }
    }
    return Status::Ok();
  }

  void StableSort() noexcept {
    Element* const values = MutableValues();
    for (nnz_t position = 1; position < size_; ++position) {
      std::array<index_t, kRank> coordinate{};
      const auto source = Coordinate(position);
      for (std::size_t dimension = 0; dimension < kRank; ++dimension) {
        coordinate[dimension] = source[dimension];
      }
      const Element value = values[static_cast<std::size_t>(position)];
      nnz_t insertion = position;
      while (insertion > 0 &&
             internal_sparse_coordinate::CompareCoordinates(
                 Coordinate(insertion - 1),
                 std::span<const index_t, kRank>(coordinate)) > 0) {
        const auto previous = Coordinate(insertion - 1);
        auto destination = MutableCoordinate(insertion);
        for (std::size_t dimension = 0; dimension < kRank; ++dimension) {
          destination[dimension] = previous[dimension];
        }
        values[static_cast<std::size_t>(insertion)] =
            values[static_cast<std::size_t>(insertion - 1)];
        --insertion;
      }
      auto destination = MutableCoordinate(insertion);
      for (std::size_t dimension = 0; dimension < kRank; ++dimension) {
        destination[dimension] = coordinate[dimension];
      }
      values[static_cast<std::size_t>(insertion)] = value;
    }
  }

  nnz_t CompactSorted(DuplicatePolicy duplicate_policy,
                      ExplicitZeroPolicy zero_policy) noexcept {
    Element* const values = MutableValues();
    nnz_t output = 0;
    nnz_t input = 0;
    while (input < size_) {
      const nnz_t first = input;
      Element value = values[static_cast<std::size_t>(input)];
      ++input;
      if (duplicate_policy == DuplicatePolicy::kSum) {
        while (input < size_ && internal_sparse_coordinate::CoordinatesEqual(
                                    Coordinate(first), Coordinate(input))) {
          value = *internal_sparse_coordinate::AddValues(
              value, values[static_cast<std::size_t>(input)]);
          ++input;
        }
      }
      if (zero_policy == ExplicitZeroPolicy::kDrop && value == Element{}) {
        continue;
      }
      const auto source = Coordinate(first);
      auto destination = MutableCoordinate(output);
      for (std::size_t dimension = 0; dimension < kRank; ++dimension) {
        destination[dimension] = source[dimension];
      }
      values[static_cast<std::size_t>(output)] = value;
      ++output;
    }
    return output;
  }

  CoordinateBuilder(ExtentsType extents, nnz_t capacity, Buffer coordinates,
                    Buffer values, MemoryResource* resource) noexcept
      : extents_(std::move(extents)),
        capacity_(capacity),
        coordinates_(std::move(coordinates)),
        values_(std::move(values)),
        resource_(resource) {}

  ExtentsType extents_;
  nnz_t capacity_ = 0;
  nnz_t size_ = 0;
  Buffer coordinates_;
  Buffer values_;
  MemoryResource* resource_ = nullptr;
};

template <SparseElement Element, SparseExtents ExtentsType>
class CoordinateArray {
  static_assert(!std::is_const_v<Element>,
                "CoordinateArray owns mutable, non-const values");

 public:
  static constexpr std::size_t kRank = ExtentsType::kRank;
  using ViewType = CoordinateView<Element, kRank>;
  using ConstViewType = CoordinateView<const Element, kRank>;

  CoordinateArray(const CoordinateArray&) = delete;
  CoordinateArray& operator=(const CoordinateArray&) = delete;

  CoordinateArray(CoordinateArray&& other) noexcept
      : extents_(std::move(other.extents_)),
        nnz_(std::exchange(other.nnz_, 0)),
        coordinates_(std::move(other.coordinates_)),
        values_(std::move(other.values_)),
        resource_(std::exchange(other.resource_, nullptr)) {}

  CoordinateArray& operator=(CoordinateArray&& other) noexcept {
    if (this != &other) {
      extents_ = std::move(other.extents_);
      nnz_ = std::exchange(other.nnz_, 0);
      coordinates_ = std::move(other.coordinates_);
      values_ = std::move(other.values_);
      resource_ = std::exchange(other.resource_, nullptr);
    }
    return *this;
  }

  ~CoordinateArray() = default;

  [[nodiscard]] const ExtentsType& extents() const noexcept { return extents_; }
  [[nodiscard]] nnz_t nnz() const noexcept { return nnz_; }
  [[nodiscard]] MemoryResource* resource() const noexcept { return resource_; }

  [[nodiscard]] Result<ViewType> view() {
    if (resource_ == nullptr) {
      return Status(ErrorCode::kInvalidState,
                    "A moved-from CoordinateArray cannot produce a view");
    }
    return internal_sparse_coordinate::ViewAccess::CreateTrusted(
        static_cast<const index_t*>(coordinates_.data()),
        static_cast<Element*>(values_.data()), extents_.values(), nnz_,
        resource_->space());
  }

  [[nodiscard]] Result<ConstViewType> view() const {
    if (resource_ == nullptr) {
      return Status(ErrorCode::kInvalidState,
                    "A moved-from CoordinateArray cannot produce a view");
    }
    return internal_sparse_coordinate::ViewAccess::CreateTrusted(
        static_cast<const index_t*>(coordinates_.data()),
        static_cast<const Element*>(values_.data()), extents_.values(), nnz_,
        resource_->space());
  }

 private:
  friend class CoordinateBuilder<Element, ExtentsType>;
  friend struct internal_sparse_coordinate::ArrayFactory<Element, ExtentsType>;

  CoordinateArray(ExtentsType extents, nnz_t nnz, Buffer coordinates,
                  Buffer values, MemoryResource* resource) noexcept
      : extents_(std::move(extents)),
        nnz_(nnz),
        coordinates_(std::move(coordinates)),
        values_(std::move(values)),
        resource_(resource) {}

  ExtentsType extents_;
  nnz_t nnz_ = 0;
  Buffer coordinates_;
  Buffer values_;
  MemoryResource* resource_ = nullptr;
};

namespace internal_sparse_coordinate {

template <SparseElement Element, SparseExtents ExtentsType>
struct ArrayFactory {
  static Result<CoordinateArray<Element, ExtentsType>> AllocateStorage(
      const ExtentsType& extents, nnz_t nnz, MemoryResource& resource) {
    if (nnz < 0 || nnz > extents.logical_size()) {
      return Status(ErrorCode::kInvalidArgument,
                    "A coordinate owner NNZ count is outside its shape");
    }
    auto count = CheckedCast<std::size_t>(nnz);
    if (!count.ok()) {
      return count.status();
    }
    auto coordinate_count = CheckedMultiply(*count, ExtentsType::kRank);
    if (!coordinate_count.ok()) {
      return Status(ErrorCode::kOverflow,
                    "A coordinate owner coordinate count overflowed");
    }
    auto coordinate_bytes = CheckedMultiply(*coordinate_count, sizeof(index_t));
    if (!coordinate_bytes.ok()) {
      return Status(ErrorCode::kOverflow,
                    "A coordinate owner coordinate bytes overflowed");
    }
    auto value_bytes = CheckedMultiply(*count, sizeof(Element));
    if (!value_bytes.ok()) {
      return Status(ErrorCode::kOverflow,
                    "A coordinate owner value bytes overflowed");
    }
    auto coordinates =
        Buffer::Allocate(resource, *coordinate_bytes, alignof(index_t));
    if (!coordinates.ok()) {
      return coordinates.status();
    }
    auto values = Buffer::Allocate(resource, *value_bytes, alignof(Element));
    if (!values.ok()) {
      return values.status();
    }
    return CoordinateArray<Element, ExtentsType>(
        extents, nnz, std::move(*coordinates), std::move(*values), &resource);
  }
};

}  // namespace internal_sparse_coordinate

template <SparseElement Element, std::size_t Rank>
struct ExpressionAdapter<CoordinateView<Element, Rank>> {
  using value_type = std::remove_cv_t<Element>;
  static_assert(Rank <= std::numeric_limits<rank_t>::max());
  static constexpr rank_t kRank = static_cast<rank_t>(Rank);
  static constexpr ExpressionOperationCategory kOperationCategory =
      ExpressionOperationCategory::kTerminal;
  static constexpr SparsityEffect kSparsityEffect =
      SparsityEffect::kStructurePreserving;

  [[nodiscard]] static constexpr std::array<extent_t, Rank> Shape(
      const CoordinateView<Element, Rank>& view) noexcept {
    return view.shape();
  }

  [[nodiscard]] static value_type Read(
      const CoordinateView<Element, Rank>& view,
      std::span<const index_t, Rank> coordinate) noexcept {
    return view.ReadUnchecked(coordinate);
  }

  [[nodiscard]] static constexpr bool MayAlias(
      const CoordinateView<Element, Rank>& view, AliasToken alias) noexcept {
    return AliasTokensMayOverlap(view.alias_token(), alias);
  }
};

template <SparseElement Element, std::size_t Rank>
struct ExpressionPlacementAdapter<CoordinateView<Element, Rank>> {
  [[nodiscard]] static constexpr MemorySpace Space(
      const CoordinateView<Element, Rank>& view) noexcept {
    return view.space();
  }
};

template <SparseElement Element, std::size_t Rank>
  requires(!std::is_const_v<Element>)
struct WritableExpressionAdapter<CoordinateView<Element, Rank>> {
  using value_type = Element;
  static_assert(Rank <= std::numeric_limits<rank_t>::max());
  static constexpr rank_t kRank = static_cast<rank_t>(Rank);

  [[nodiscard]] static constexpr std::array<extent_t, Rank> Shape(
      const CoordinateView<Element, Rank>& view) noexcept {
    return view.shape();
  }

  [[nodiscard]] static constexpr AliasToken Alias(
      const CoordinateView<Element, Rank>& view) noexcept {
    return view.alias_token();
  }

  static void Write(CoordinateView<Element, Rank>& view,
                    std::span<const index_t, Rank> coordinate,
                    value_type value) noexcept {
    auto found = view.Find(coordinate);
    if (found.ok() && *found != nullptr) {
      **found = value;
    }
  }
};

static_assert(std::is_trivially_copyable_v<CoordinateView<float, 1>>);
static_assert(std::is_trivially_copyable_v<CoordinateView<const float, 1>>);

}  // namespace asc

#endif  // ASC_SPARSE_COORDINATE_H_
