// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.

#ifndef ASC_ARRAY_EXTENTS_H_
#define ASC_ARRAY_EXTENTS_H_

#include <concepts>
#include <array>
#include <cstddef>
#include <limits>
#include <type_traits>
#include <utility>

#include "asc/core/contracts.h"
#include "asc/core/status.h"
#include "asc/core/types.h"

namespace asc {

namespace detail {

template <typename Value>
concept ArrayExtentValue =
    std::integral<Value> && !std::same_as<std::remove_cv_t<Value>, bool>;

template <ArrayExtentValue Value>
Result<extent_t> ToArrayExtent(Value value) {
  if constexpr (std::is_signed_v<Value>) {
    if (value < 0) {
      return Status(StatusCode::kInvalidArgument,
                    "A dynamic extent must be non-negative");
    }
  }

  using UnsignedValue = std::make_unsigned_t<Value>;
  const auto unsigned_value = static_cast<UnsignedValue>(value);
  if constexpr (sizeof(UnsignedValue) >= sizeof(extent_t)) {
    using UnsignedExtent = std::make_unsigned_t<extent_t>;
    if (unsigned_value >
        static_cast<UnsignedExtent>(std::numeric_limits<extent_t>::max())) {
      return Status(StatusCode::kOverflow,
                    "A dynamic extent exceeds the canonical extent range");
    }
  }
  return static_cast<extent_t>(unsigned_value);
}

}  // namespace detail

/// @brief Fixed-rank tensor extents with mixed static and dynamic dimensions.
///
/// Static template arguments are non-negative extents or `dynamic_extent`.
/// Runtime storage contains only the dynamic dimensions.
template <extent_t... StaticExtents>
class Extents {
  static_assert(((StaticExtents >= 0 || StaticExtents == dynamic_extent) &&
                 ...),
                "Static extents must be non-negative or dynamic_extent");

 public:
  /// @brief Number of tensor dimensions.
  static constexpr std::size_t Rank() noexcept {
    return sizeof...(StaticExtents);
  }

  /// @brief Number of dimensions whose extents are supplied at runtime.
  static constexpr std::size_t DynamicRank() noexcept {
    return ((StaticExtents == dynamic_extent ? 1U : 0U) + ... + 0U);
  }

  /// @brief Static extent or `dynamic_extent` for a dimension.
  static constexpr extent_t StaticExtent(std::size_t dimension) {
    if (dimension >= Rank()) {
      ContractFailure(ContractKind::kPrecondition, "dimension < Rank()",
                      "Extent dimension is out of range");
    }
    constexpr std::array<extent_t, Rank()> kStaticExtents{StaticExtents...};
    return kStaticExtents[dimension];
  }

  /// @brief Construct extents with every dynamic dimension set to zero.
  constexpr Extents() = default;

  /// @brief Validate and construct the dynamic dimensions in dimension order.
  template <detail::ArrayExtentValue... Values>
    requires(sizeof...(Values) == DynamicRank())
  static Result<Extents> Create(Values... values) {
    Extents result;
    if constexpr (DynamicRank() > 0) {
      std::size_t dynamic_index = 0;
      Status status;
      const auto assign = [&](auto value) {
        if (!status.ok()) {
          return;
        }
        auto converted = detail::ToArrayExtent(value);
        if (!converted.ok()) {
          status = converted.status();
          return;
        }
        result.dynamic_extents_[dynamic_index++] = converted.value();
      };
      (assign(values), ...);
      if (!status.ok()) {
        return status;
      }
    }
    return result;
  }

  /// @brief Runtime extent of a dimension.
  constexpr extent_t GetExtent(std::size_t dimension) const {
    if (dimension >= Rank()) {
      ContractFailure(ContractKind::kPrecondition, "dimension < Rank()",
                      "Extent dimension is out of range");
    }
    constexpr std::array<extent_t, Rank()> kStaticExtents{StaticExtents...};
    if (kStaticExtents[dimension] != dynamic_extent) {
      return kStaticExtents[dimension];
    }

    std::size_t dynamic_index = 0;
    for (std::size_t current = 0; current < dimension; ++current) {
      if (kStaticExtents[current] == dynamic_extent) {
        ++dynamic_index;
      }
    }
    return dynamic_extents_[dynamic_index];
  }

  /// @brief Checked product of all extents.
  Result<extent_t> GetSize() const {
    for (std::size_t dimension = 0; dimension < Rank(); ++dimension) {
      if (GetExtent(dimension) == 0) {
        return extent_t{0};
      }
    }

    extent_t size = 1;
    for (std::size_t dimension = 0; dimension < Rank(); ++dimension) {
      const extent_t extent = GetExtent(dimension);
      if (size > std::numeric_limits<extent_t>::max() / extent) {
        return Status(StatusCode::kOverflow,
                      "Tensor logical size exceeds the canonical extent range");
      }
      size *= extent;
    }
    return size;
  }

  /// @brief Compare all logical extents.
  friend constexpr bool operator==(const Extents& left,
                                   const Extents& right) noexcept = default;

 private:
  std::array<extent_t, DynamicRank()> dynamic_extents_{};
};

namespace detail {

template <std::size_t... Indices>
auto MakeDynamicTensorExtents(std::index_sequence<Indices...>)
    -> Extents<((void)Indices, dynamic_extent)...>;

}  // namespace detail

/// @brief Fixed-rank extents with every dimension supplied at runtime.
template <std::size_t Rank>
using DynamicTensorExtents =
    decltype(detail::MakeDynamicTensorExtents(std::make_index_sequence<Rank>{}));

}  // namespace asc

#endif  // ASC_ARRAY_EXTENTS_H_
