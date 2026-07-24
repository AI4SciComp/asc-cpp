// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.

#ifndef ASC_ARRAY_LAYOUT_H_
#define ASC_ARRAY_LAYOUT_H_

#include <concepts>
#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <type_traits>
#include <utility>

#include "asc/array/extents.h"
#include "asc/core/contracts.h"
#include "asc/core/status.h"
#include "asc/core/types.h"

namespace asc {

/// @brief Canonical contiguous tensor storage order.
enum class LayoutOrder { kLeft, kRight };

namespace detail {

template <typename Value>
concept ArrayIndexValue =
    std::integral<Value> && !std::same_as<std::remove_cv_t<Value>, bool>;

inline Result<extent_t> CheckedArrayAdd(extent_t left, extent_t right,
                                        const char* diagnostic) {
  if (left < 0 || right < 0 ||
      left > std::numeric_limits<extent_t>::max() - right) {
    return Status(StatusCode::kOverflow, diagnostic);
  }
  return left + right;
}

inline Result<extent_t> CheckedArrayMultiply(extent_t left, extent_t right,
                                             const char* diagnostic) {
  if (left < 0 || right < 0 ||
      (right != 0 && left > std::numeric_limits<extent_t>::max() / right)) {
    return Status(StatusCode::kOverflow, diagnostic);
  }
  return left * right;
}

template <ArrayIndexValue Value>
Result<index_t> ToArrayIndex(Value value) {
  if constexpr (std::is_signed_v<Value>) {
    if (value < std::numeric_limits<index_t>::min()) {
      return Status(StatusCode::kOutOfRange,
                    "A tensor coordinate is below the canonical index range");
    }
  }

  if constexpr (std::is_unsigned_v<Value>) {
    using UnsignedIndex = std::make_unsigned_t<index_t>;
    if constexpr (sizeof(Value) >= sizeof(index_t)) {
      if (value >
          static_cast<UnsignedIndex>(std::numeric_limits<index_t>::max())) {
        return Status(StatusCode::kOutOfRange,
                      "A tensor coordinate exceeds the canonical index range");
      }
    }
  } else if constexpr (sizeof(Value) > sizeof(index_t)) {
    if (value > std::numeric_limits<index_t>::max()) {
      return Status(StatusCode::kOutOfRange,
                    "A tensor coordinate exceeds the canonical index range");
    }
  }
  return static_cast<index_t>(value);
}

template <typename Mapping, typename... Indices>
Result<index_t> TryArrayOffset(const Mapping& mapping, Indices... indices) {
  static_assert(sizeof...(Indices) == Mapping::Rank());
  std::array<index_t, Mapping::Rank()> coordinates{};
  if constexpr (Mapping::Rank() > 0) {
    std::size_t position = 0;
    Status status;
    const auto convert = [&](auto value) {
      if (!status.ok()) {
        return;
      }
      auto converted = ToArrayIndex(value);
      if (!converted.ok()) {
        status = converted.status();
        return;
      }
      coordinates[position++] = converted.value();
    };
    (convert(indices), ...);
    if (!status.ok()) {
      return status;
    }
  }
  return mapping.TryOffset(coordinates);
}

template <typename Mapping, typename... Indices>
index_t ContractArrayOffset(const Mapping& mapping, Indices... indices) {
  auto offset = TryArrayOffset(mapping, indices...);
  ASC_REQUIRE(offset.ok(), offset.status().message());
  return offset.value();
}

}  // namespace detail

/// @brief Checked contiguous mapping in left- or right-major order.
template <typename ExtentsT, LayoutOrder Order>
class ContiguousLayoutMapping {
 public:
  using ExtentsType = ExtentsT;

  static constexpr std::size_t Rank() noexcept { return ExtentsType::Rank(); }
  static constexpr bool IsAlwaysUnique() noexcept { return true; }
  static constexpr bool IsAlwaysExhaustive() noexcept { return true; }
  static constexpr bool IsAlwaysContiguous() noexcept { return true; }

  /// @brief Validate all checked stride calculations and create a mapping.
  static Result<ContiguousLayoutMapping> Create(const ExtentsType& extents) {
    auto logical_size = extents.GetSize();
    if (!logical_size.ok()) {
      return logical_size.status();
    }

    std::array<stride_t, Rank()> strides{};
    if constexpr (Rank() > 0) {
      if constexpr (Order == LayoutOrder::kLeft) {
        strides[0] = 1;
        for (std::size_t dimension = 1; dimension < Rank(); ++dimension) {
          auto stride = detail::CheckedArrayMultiply(
              strides[dimension - 1], extents.GetExtent(dimension - 1),
              "A left-major tensor stride exceeds the canonical range");
          if (!stride.ok()) {
            return stride.status();
          }
          strides[dimension] = stride.value();
        }
      } else {
        strides[Rank() - 1] = 1;
        for (std::size_t dimension = Rank() - 1; dimension > 0; --dimension) {
          auto stride = detail::CheckedArrayMultiply(
              strides[dimension], extents.GetExtent(dimension),
              "A right-major tensor stride exceeds the canonical range");
          if (!stride.ok()) {
            return stride.status();
          }
          strides[dimension - 1] = stride.value();
        }
      }
    }
    return ContiguousLayoutMapping(extents, strides, logical_size.value());
  }

  const ExtentsType& GetExtents() const noexcept { return extents_; }
  extent_t GetExtent(std::size_t dimension) const {
    return extents_.GetExtent(dimension);
  }
  stride_t GetStride(std::size_t dimension) const {
    ASC_REQUIRE(dimension < Rank(), "Stride dimension is out of range");
    return strides_[dimension];
  }
  extent_t GetSize() const noexcept { return logical_size_; }
  extent_t GetRequiredSpan() const noexcept { return logical_size_; }
  constexpr bool IsUnique() const noexcept { return true; }
  constexpr bool IsExhaustive() const noexcept { return true; }
  constexpr bool IsContiguous() const noexcept { return true; }

  Result<index_t> TryOffset(
      const std::array<index_t, Rank()>& coordinates) const {
    extent_t offset = 0;
    for (std::size_t dimension = 0; dimension < Rank(); ++dimension) {
      if (coordinates[dimension] < 0 ||
          coordinates[dimension] >= GetExtent(dimension)) {
        return Status(StatusCode::kOutOfRange,
                      "A tensor coordinate is out of bounds");
      }
      auto term = detail::CheckedArrayMultiply(
          coordinates[dimension], strides_[dimension],
          "A tensor coordinate offset exceeds the canonical range");
      if (!term.ok()) {
        return term.status();
      }
      auto sum = detail::CheckedArrayAdd(
          offset, term.value(),
          "A tensor coordinate offset exceeds the canonical range");
      if (!sum.ok()) {
        return sum.status();
      }
      offset = sum.value();
    }
    return static_cast<index_t>(offset);
  }

  template <detail::ArrayIndexValue... Indices>
    requires(sizeof...(Indices) == Rank())
  Result<index_t> TryOffset(Indices... indices) const {
    return detail::TryArrayOffset(*this, indices...);
  }

  template <detail::ArrayIndexValue... Indices>
    requires(sizeof...(Indices) == Rank())
  index_t operator()(Indices... indices) const {
    return detail::ContractArrayOffset(*this, indices...);
  }

  index_t UncheckedOffset(
      const std::array<index_t, Rank()>& coordinates) const noexcept {
    index_t offset = 0;
    for (std::size_t dimension = 0; dimension < Rank(); ++dimension) {
      offset += coordinates[dimension] * strides_[dimension];
    }
    return offset;
  }

  template <detail::ArrayIndexValue... Indices>
    requires(sizeof...(Indices) == Rank())
  index_t UncheckedOffset(Indices... indices) const noexcept {
    const std::array<index_t, Rank()> coordinates{
        static_cast<index_t>(indices)...};
    return UncheckedOffset(coordinates);
  }

 private:
  constexpr ContiguousLayoutMapping(
      const ExtentsType& extents, const std::array<stride_t, Rank()>& strides,
      extent_t logical_size) noexcept
      : extents_(extents), strides_(strides), logical_size_(logical_size) {}

  ExtentsType extents_;
  std::array<stride_t, Rank()> strides_{};
  extent_t logical_size_ = 0;
};

template <typename ExtentsType>
using LayoutLeftMapping =
    ContiguousLayoutMapping<ExtentsType, LayoutOrder::kLeft>;

template <typename ExtentsType>
using LayoutRightMapping =
    ContiguousLayoutMapping<ExtentsType, LayoutOrder::kRight>;

/// @brief Checked mapping with caller-supplied non-negative strides.
template <typename ExtentsT>
class LayoutStrideMapping {
 public:
  using ExtentsType = ExtentsT;

  static constexpr std::size_t Rank() noexcept { return ExtentsType::Rank(); }
  static constexpr bool IsAlwaysUnique() noexcept { return false; }
  static constexpr bool IsAlwaysExhaustive() noexcept { return false; }
  static constexpr bool IsAlwaysContiguous() noexcept { return false; }

  /// @brief Validate strides and backing-span arithmetic.
  static Result<LayoutStrideMapping> Create(
      const ExtentsType& extents,
      const std::array<stride_t, Rank()>& strides) {
    auto logical_size = extents.GetSize();
    if (!logical_size.ok()) {
      return logical_size.status();
    }
    for (stride_t stride : strides) {
      if (stride < 0) {
        return Status(StatusCode::kInvalidArgument,
                      "Canonical tensor strides must be non-negative");
      }
    }

    extent_t required_span = logical_size.value() == 0 ? 0 : 1;
    if (logical_size.value() != 0) {
      for (std::size_t dimension = 0; dimension < Rank(); ++dimension) {
        auto term = detail::CheckedArrayMultiply(
            extents.GetExtent(dimension) - 1, strides[dimension],
            "A strided tensor span exceeds the canonical range");
        if (!term.ok()) {
          return term.status();
        }
        auto sum = detail::CheckedArrayAdd(
            required_span, term.value(),
            "A strided tensor span exceeds the canonical range");
        if (!sum.ok()) {
          return sum.status();
        }
        required_span = sum.value();
      }
    }

    const bool unique = ProveUnique(extents, strides, logical_size.value());
    const bool contiguous =
        IsCanonicalContiguous(extents, strides, logical_size.value());
    return LayoutStrideMapping(extents, strides, logical_size.value(),
                               required_span, unique, contiguous);
  }

  const ExtentsType& GetExtents() const noexcept { return extents_; }
  extent_t GetExtent(std::size_t dimension) const {
    return extents_.GetExtent(dimension);
  }
  stride_t GetStride(std::size_t dimension) const {
    ASC_REQUIRE(dimension < Rank(), "Stride dimension is out of range");
    return strides_[dimension];
  }
  extent_t GetSize() const noexcept { return logical_size_; }
  extent_t GetRequiredSpan() const noexcept { return required_span_; }
  bool IsUnique() const noexcept { return unique_; }
  bool IsExhaustive() const noexcept { return contiguous_; }
  bool IsContiguous() const noexcept { return contiguous_; }

  Result<index_t> TryOffset(
      const std::array<index_t, Rank()>& coordinates) const {
    extent_t offset = 0;
    for (std::size_t dimension = 0; dimension < Rank(); ++dimension) {
      if (coordinates[dimension] < 0 ||
          coordinates[dimension] >= GetExtent(dimension)) {
        return Status(StatusCode::kOutOfRange,
                      "A tensor coordinate is out of bounds");
      }
      auto term = detail::CheckedArrayMultiply(
          coordinates[dimension], strides_[dimension],
          "A tensor coordinate offset exceeds the canonical range");
      if (!term.ok()) {
        return term.status();
      }
      auto sum = detail::CheckedArrayAdd(
          offset, term.value(),
          "A tensor coordinate offset exceeds the canonical range");
      if (!sum.ok()) {
        return sum.status();
      }
      offset = sum.value();
    }
    return static_cast<index_t>(offset);
  }

  template <detail::ArrayIndexValue... Indices>
    requires(sizeof...(Indices) == Rank())
  Result<index_t> TryOffset(Indices... indices) const {
    return detail::TryArrayOffset(*this, indices...);
  }

  template <detail::ArrayIndexValue... Indices>
    requires(sizeof...(Indices) == Rank())
  index_t operator()(Indices... indices) const {
    return detail::ContractArrayOffset(*this, indices...);
  }

  index_t UncheckedOffset(
      const std::array<index_t, Rank()>& coordinates) const noexcept {
    index_t offset = 0;
    for (std::size_t dimension = 0; dimension < Rank(); ++dimension) {
      offset += coordinates[dimension] * strides_[dimension];
    }
    return offset;
  }

  template <detail::ArrayIndexValue... Indices>
    requires(sizeof...(Indices) == Rank())
  index_t UncheckedOffset(Indices... indices) const noexcept {
    const std::array<index_t, Rank()> coordinates{
        static_cast<index_t>(indices)...};
    return UncheckedOffset(coordinates);
  }

 private:
  struct ActiveDimension {
    stride_t stride;
    extent_t extent;
  };

  static bool ProveUnique(const ExtentsType& extents,
                          const std::array<stride_t, Rank()>& strides,
                          extent_t logical_size) {
    if (logical_size == 0) {
      return true;
    }
    std::array<ActiveDimension, Rank()> active{};
    std::size_t active_count = 0;
    for (std::size_t dimension = 0; dimension < Rank(); ++dimension) {
      if (extents.GetExtent(dimension) > 1) {
        active[active_count++] =
            ActiveDimension{strides[dimension], extents.GetExtent(dimension)};
      }
    }
    std::sort(active.begin(), active.begin() + active_count,
              [](const ActiveDimension& left, const ActiveDimension& right) {
                return left.stride < right.stride;
              });

    extent_t covered_span = 1;
    for (std::size_t index = 0; index < active_count; ++index) {
      if (active[index].stride == 0 || active[index].stride < covered_span) {
        return false;
      }
      auto term = detail::CheckedArrayMultiply(
          active[index].extent - 1, active[index].stride,
          "A uniqueness proof exceeded the canonical range");
      if (!term.ok()) {
        return false;
      }
      auto next_span = detail::CheckedArrayAdd(
          covered_span, term.value(),
          "A uniqueness proof exceeded the canonical range");
      if (!next_span.ok()) {
        return false;
      }
      covered_span = next_span.value();
    }
    return true;
  }

  static bool IsCanonicalContiguous(
      const ExtentsType& extents,
      const std::array<stride_t, Rank()>& strides, extent_t logical_size) {
    if (logical_size == 0) {
      return true;
    }

    extent_t expected = 1;
    bool left_contiguous = true;
    for (std::size_t dimension = 0; dimension < Rank(); ++dimension) {
      const extent_t extent = extents.GetExtent(dimension);
      if (extent > 1 && strides[dimension] != expected) {
        left_contiguous = false;
        break;
      }
      if (extent > 1) {
        auto next = detail::CheckedArrayMultiply(
            expected, extent, "A contiguity check exceeded the canonical range");
        if (!next.ok()) {
          return false;
        }
        expected = next.value();
      }
    }
    if (left_contiguous) {
      return true;
    }

    expected = 1;
    for (std::size_t dimension = Rank(); dimension > 0; --dimension) {
      const std::size_t current = dimension - 1;
      const extent_t extent = extents.GetExtent(current);
      if (extent > 1 && strides[current] != expected) {
        return false;
      }
      if (extent > 1) {
        auto next = detail::CheckedArrayMultiply(
            expected, extent, "A contiguity check exceeded the canonical range");
        if (!next.ok()) {
          return false;
        }
        expected = next.value();
      }
    }
    return true;
  }

  constexpr LayoutStrideMapping(
      const ExtentsType& extents, const std::array<stride_t, Rank()>& strides,
      extent_t logical_size, extent_t required_span, bool unique,
      bool contiguous) noexcept
      : extents_(extents),
        strides_(strides),
        logical_size_(logical_size),
        required_span_(required_span),
        unique_(unique),
        contiguous_(contiguous) {}

  ExtentsType extents_;
  std::array<stride_t, Rank()> strides_{};
  extent_t logical_size_ = 0;
  extent_t required_span_ = 0;
  bool unique_ = false;
  bool contiguous_ = false;
};

}  // namespace asc

#endif  // ASC_ARRAY_LAYOUT_H_
