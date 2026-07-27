#ifndef ASC_DENSE_VIEW_H_
#define ASC_DENSE_VIEW_H_

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>

#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/layout.h"
#include "asc/expression/expression.h"
#include "asc/expression/writable.h"

namespace asc {

template <typename Element>
concept DenseElement =
    std::is_arithmetic_v<std::remove_cv_t<Element>> &&
    !std::same_as<std::remove_cv_t<Element>, bool> &&
    !std::is_volatile_v<Element> &&
    std::is_trivially_copyable_v<std::remove_cv_t<Element>> &&
    std::is_trivially_destructible_v<std::remove_cv_t<Element>>;

template <DenseElement Element, std::size_t Rank>
class DenseView {
 public:
  using element_type = Element;
  using value_type = std::remove_cv_t<Element>;
  using Mapping = DenseLayoutMapping<Rank>;
  using ShapeType = typename Mapping::ShapeType;

  static Result<DenseView> Create(Element* data, Mapping mapping,
                                  MemorySpace space) {
    return CreateWithAlias(data, mapping, space, AliasToken::FromIdentity(data),
                           false);
  }

  template <DenseElement MutableElement>
    requires std::is_const_v<Element> && (!std::is_const_v<MutableElement>) &&
                 std::same_as<std::remove_const_t<Element>, MutableElement>
  constexpr DenseView(
      const DenseView<MutableElement, Rank>& mutable_view) noexcept
      : data_(mutable_view.data_),
        mapping_(mutable_view.mapping_),
        space_(mutable_view.space_),
        alias_(mutable_view.alias_) {}

  [[nodiscard]] constexpr Element* data() const noexcept { return data_; }
  [[nodiscard]] constexpr const Mapping& mapping() const noexcept {
    return mapping_;
  }
  [[nodiscard]] constexpr const ShapeType& shape() const noexcept {
    return mapping_.shape();
  }
  [[nodiscard]] constexpr MemorySpace space() const noexcept { return space_; }
  [[nodiscard]] constexpr AliasToken alias_token() const noexcept {
    return alias_;
  }

  template <DenseElement OtherElement, std::size_t OtherRank>
  [[nodiscard]] bool MayOverlap(
      const DenseView<OtherElement, OtherRank>& other) const noexcept {
    if (mapping_.required_span_size() == 0 ||
        other.mapping().required_span_size() == 0) {
      return false;
    }
    const std::uintptr_t begin = reinterpret_cast<std::uintptr_t>(data_);
    const std::uintptr_t other_begin =
        reinterpret_cast<std::uintptr_t>(other.data());
    const auto span = static_cast<std::size_t>(mapping_.required_span_size());
    const auto other_span =
        static_cast<std::size_t>(other.mapping().required_span_size());
    const std::size_t bytes = span * sizeof(value_type);
    const std::size_t other_bytes =
        other_span *
        sizeof(typename DenseView<OtherElement, OtherRank>::value_type);
    return begin < other_begin + other_bytes && other_begin < begin + bytes;
  }

  [[nodiscard]] Result<Element*> At(
      std::span<const index_t, Rank> indices) const {
    if (space_ != MemorySpace::kHost) {
      return Status(ErrorCode::kMemoryAccess,
                    "Dense elements are host-dereferenceable only in host "
                    "memory");
    }
    auto offset = mapping_.Offset(indices);
    if (!offset.ok()) {
      return offset.status();
    }
    auto converted = CheckedCast<std::size_t>(*offset);
    if (!converted.ok()) {
      return converted.status();
    }
    return data_ + *converted;
  }

  [[nodiscard]] Result<DenseView> Subview(
      std::span<const index_t, Rank> offsets,
      std::span<const extent_t, Rank> extents) const {
    std::array<extent_t, Rank> subshape{};
    if constexpr (Rank > 0) {
      for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
        if (offsets[dimension] < 0) {
          return Status(ErrorCode::kIndex,
                        "A dense subview offset cannot be negative");
        }
        if (extents[dimension] < 0) {
          return Status(ErrorCode::kShape,
                        "A dense subview extent cannot be negative");
        }
        if (offsets[dimension] > mapping_.shape()[dimension] ||
            extents[dimension] >
                mapping_.shape()[dimension] - offsets[dimension]) {
          return Status(ErrorCode::kIndex,
                        "A dense subview region is outside its parent");
        }
        subshape[dimension] = extents[dimension];
      }
    }

    auto submapping =
        Mapping::Create(LayoutStride{}, subshape, mapping_.strides());
    if (!submapping.ok()) {
      return submapping.status();
    }

    Element* subdata = data_;
    if (submapping->logical_size() != 0) {
      auto offset = mapping_.Offset(offsets);
      if (!offset.ok()) {
        return offset.status();
      }
      auto converted = CheckedCast<std::size_t>(*offset);
      if (!converted.ok()) {
        return converted.status();
      }
      subdata += *converted;
    }
    return CreateWithAlias(subdata, *submapping, space_, alias_, true);
  }

  template <DenseElement OtherElement>
    requires std::same_as<std::remove_cv_t<Element>,
                          std::remove_cv_t<OtherElement>>
  [[nodiscard]] constexpr bool IsExactView(
      const DenseView<OtherElement, Rank>& other) const noexcept {
    return data_ == other.data() && mapping_.shape() == other.shape() &&
           mapping_.strides() == other.mapping().strides() &&
           space_ == other.space();
  }

 private:
  template <DenseElement, std::size_t>
  friend class DenseView;
  friend struct ExpressionAdapter<DenseView<Element, Rank>>;
  friend struct WritableExpressionAdapter<DenseView<Element, Rank>>;

  static Result<DenseView> CreateWithAlias(Element* data, Mapping mapping,
                                           MemorySpace space, AliasToken alias,
                                           bool preserve_alias) {
    switch (space) {
      case MemorySpace::kHost:
      case MemorySpace::kPinnedHost:
      case MemorySpace::kDevice:
      case MemorySpace::kManaged:
        break;
      default:
        return Status(ErrorCode::kInvalidArgument,
                      "A dense view has an invalid memory space");
    }
    if (mapping.required_span_size() != 0 && data == nullptr) {
      return Status(ErrorCode::kMemoryAccess,
                    "A nonempty dense view cannot have a null pointer");
    }
    if constexpr (!std::is_const_v<Element>) {
      if (!mapping.is_unique()) {
        return Status(ErrorCode::kInvalidArgument,
                      "A mutable dense view requires a proven-unique mapping");
      }
    }
    auto span_size = CheckedCast<std::size_t>(mapping.required_span_size());
    if (!span_size.ok()) {
      return span_size.status();
    }
    auto bytes = CheckedMultiply(*span_size, sizeof(value_type));
    if (!bytes.ok()) {
      return Status(ErrorCode::kOverflow,
                    "A dense view byte span exceeds size_t");
    }
    if (mapping.required_span_size() >
        static_cast<extent_t>(std::numeric_limits<std::ptrdiff_t>::max())) {
      return Status(ErrorCode::kOverflow,
                    "A dense view element span exceeds ptrdiff_t");
    }
    if (data != nullptr &&
        *bytes > std::numeric_limits<std::uintptr_t>::max() -
                     reinterpret_cast<std::uintptr_t>(data)) {
      return Status(ErrorCode::kOverflow,
                    "A dense view address span exceeds uintptr_t");
    }
    if (!preserve_alias) {
      auto address_span = AliasToken::FromAddressSpan(data, *bytes);
      if (!address_span.ok()) {
        return address_span.status();
      }
      alias = *address_span;
    }
    return DenseView(data, mapping, space, alias);
  }

  [[nodiscard]] value_type ReadUnchecked(
      std::span<const index_t, Rank> indices) const noexcept {
    extent_t offset = 0;
    if constexpr (Rank > 0) {
      for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
        offset += indices[dimension] * mapping_.strides()[dimension];
      }
    }
    return data_[static_cast<std::size_t>(offset)];
  }

  void WriteUnchecked(std::span<const index_t, Rank> indices,
                      value_type value) const noexcept
    requires(!std::is_const_v<Element>)
  {
    extent_t offset = 0;
    if constexpr (Rank > 0) {
      for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
        offset += indices[dimension] * mapping_.strides()[dimension];
      }
    }
    data_[static_cast<std::size_t>(offset)] = value;
  }

  constexpr DenseView(Element* data, Mapping mapping, MemorySpace space,
                      AliasToken alias) noexcept
      : data_(data), mapping_(mapping), space_(space), alias_(alias) {}

  Element* data_;
  Mapping mapping_;
  MemorySpace space_;
  AliasToken alias_;
};

template <DenseElement Element, std::size_t Rank>
struct ExpressionAdapter<DenseView<Element, Rank>> {
  using value_type = std::remove_cv_t<Element>;
  static_assert(Rank <= std::numeric_limits<rank_t>::max());
  static constexpr rank_t kRank = static_cast<rank_t>(Rank);
  static constexpr ExpressionOperationCategory kOperationCategory =
      ExpressionOperationCategory::kTerminal;
  static constexpr SparsityEffect kSparsityEffect =
      SparsityEffect::kStructurePreserving;

  [[nodiscard]] static constexpr std::array<extent_t, Rank> Shape(
      const DenseView<Element, Rank>& view) noexcept {
    return view.shape();
  }

  [[nodiscard]] static value_type Read(
      const DenseView<Element, Rank>& view,
      std::span<const index_t, Rank> indices) noexcept {
    return view.ReadUnchecked(indices);
  }

  [[nodiscard]] static constexpr bool MayAlias(
      const DenseView<Element, Rank>& view, AliasToken alias) noexcept {
    return AliasTokensMayOverlap(view.alias_token(), alias);
  }
};

template <DenseElement Element, std::size_t Rank>
struct ExpressionPlacementAdapter<DenseView<Element, Rank>> {
  [[nodiscard]] static constexpr MemorySpace Space(
      const DenseView<Element, Rank>& view) noexcept {
    return view.space();
  }
};

template <DenseElement Element, std::size_t Rank>
  requires(!std::is_const_v<Element>)
struct WritableExpressionAdapter<DenseView<Element, Rank>> {
  using value_type = Element;
  static_assert(Rank <= std::numeric_limits<rank_t>::max());
  static constexpr rank_t kRank = static_cast<rank_t>(Rank);

  [[nodiscard]] static constexpr std::array<extent_t, Rank> Shape(
      const DenseView<Element, Rank>& view) noexcept {
    return view.shape();
  }

  [[nodiscard]] static constexpr AliasToken Alias(
      const DenseView<Element, Rank>& view) noexcept {
    return view.alias_token();
  }

  static void Write(DenseView<Element, Rank>& view,
                    std::span<const index_t, Rank> indices,
                    value_type value) noexcept {
    view.WriteUnchecked(indices, value);
  }
};

static_assert(std::is_trivially_copyable_v<DenseView<float, 1>>);
static_assert(std::is_trivially_copyable_v<DenseView<const float, 1>>);

}  // namespace asc

#endif  // ASC_DENSE_VIEW_H_
