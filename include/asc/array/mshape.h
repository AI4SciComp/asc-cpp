// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/mshape.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_MSHAPE_H_
#define ASC_MSHAPE_H_

#include <initializer_list>
#include <limits>
#include <cstdint>
#include <type_traits>

#include "asc/core/error.h"
#include "asc/core/globals.h"

namespace asc {

template <int... Extents>
class MShape;

namespace details {

template <typename Sequence>
struct DShapeAlias;

template <int... Extents>
struct DShapeAlias<std::integer_sequence<int, Extents...>> {
  using Type = MShape<Extents...>;
};

}  // namespace details

template <int Rank>
using DShape = typename details::DShapeAlias<DynamicExtents<Rank>>::Type;

/// @brief Multi-dimensional shape with static or dynamic extents
///
/// MShape<Extents...> represents the shape of a multi-dimensional array.
/// Release-supported shapes are either fully static or fully dynamic:
/// - MShape<3, 4> for static compile-time extents
/// - DShape<2> / MShape<kDynamicExtent, kDynamicExtent> for runtime extents
///
/// @tparam Extents Extent for each dimension (static or kDynamicExtent)
///
/// @par Storage:
/// Fully dynamic shapes store one runtime extent per dimension. Fully static
/// shapes consume no meaningful runtime storage.
///
/// @par Usage Examples:
/// @code
/// // Fully static shape (3x4)
/// asc::MShape<3, 4> static_shape;
///
/// // Fully dynamic shape
/// asc::MShape<kDynamicExtent, kDynamicExtent> dyn_shape(3, 4);
///
/// // Mixed shapes such as MShape<3, kDynamicExtent> are intentionally
/// // unsupported. Use either MShape<3, 4> or DShape<2>.
/// @endcode
template <int... Extents>
class MShape {
 public:
  /// @brief Get the rank (number of dimensions)
  static ASC_HOST_DEVICE constexpr int GetRank() {
    return sizeof...(Extents);
  }

  /// @brief Get the number of dynamic extents
  static ASC_HOST_DEVICE constexpr int GetDynamicRank() {
    if constexpr (sizeof...(Extents) == 0) {
      return 0;  // Rank-0 shape has no dynamic extents
    } else {
      return ((Extents == kDynamicExtent ? 1 : 0) + ...);
    }
  }

  /// @brief Check if all extents are runtime dynamic
  static ASC_HOST_DEVICE constexpr bool IsDynamic() {
    return GetDynamicRank() == GetRank();
  }

  /// @brief Check if shape has only static extents
  static ASC_HOST_DEVICE constexpr bool IsStatic() {
    return GetDynamicRank() == 0;
  }

  /// @brief Check if this shape is supported by the release API.
  static ASC_HOST_DEVICE constexpr bool IsSupported() {
    return IsStatic() || IsDynamic();
  }

  /// @brief Get effective number of dimensions for array sizing
  static ASC_HOST_DEVICE constexpr int GetNDims() {
    return GetRank() > 0 ? GetRank() : 1;
  }

  /// @brief Get effective number of stored extents
  static ASC_HOST_DEVICE constexpr int GetNumDynamicExtents() {
    return GetDynamicRank() > 0 ? GetDynamicRank() : 1;
  }

  /// @brief Default constructor
  constexpr MShape() {
    ASC_STATIC_ASSERT(IsSupported(),
                         "Mixed static/dynamic MShape extents are unsupported. "
                         "Use a fully static MShape or a fully dynamic DShape.");
    ASC_STATIC_ASSERT(AreStaticExtentsValid(),
                         "Static MShape extents must be positive.");
    // Zero-initialize dynamic extents
    if constexpr (GetDynamicRank() > 0) {
      for (int i = 0; i < GetDynamicRank(); ++i) {
        extents_[i] = 0;
      }
    }
  }

  /// @brief Constructor for dynamic extents (variadic)
  /// @param extents Runtime values for each dynamic extent
  template <Integral... DExtents>
    requires(sizeof...(DExtents) == GetDynamicRank())
  constexpr explicit MShape(DExtents... extents) {
    ASC_STATIC_ASSERT(GetDynamicRank() > 0,
                         "MShape must have dynamic extents.");
    ASC_STATIC_ASSERT(IsSupported(),
                         "Mixed static/dynamic MShape extents are unsupported. "
                         "Use a fully static MShape or a fully dynamic DShape.");
    if constexpr (GetDynamicRank() > 0) {
      InitExtents(0, extents...);
    }
  }

  /// @brief Constructor for dynamic extents (runtime array)
  /// @param extents Pointer to array of extent values
  constexpr explicit MShape(const int* extents) {
    ASC_STATIC_ASSERT(GetDynamicRank() > 0,
                         "MShape must have dynamic extents.");
    ASC_STATIC_ASSERT(IsSupported(),
                         "Mixed static/dynamic MShape extents are unsupported. "
                         "Use a fully static MShape or a fully dynamic DShape.");
    if constexpr (GetDynamicRank() > 0) {
      for (int i = 0; i < GetDynamicRank(); ++i) {
        SetDynamicExtent(i, extents[i]);
      }
    }
  }

  /// @brief Constructor for dynamic extents (initializer list)
  /// @param extents Initializer list of extent values
  constexpr MShape(std::initializer_list<int> extents) {
    ASC_STATIC_ASSERT(GetDynamicRank() > 0,
                         "MShape must have dynamic extents.");
    ASC_STATIC_ASSERT(IsSupported(),
                         "Mixed static/dynamic MShape extents are unsupported. "
                         "Use a fully static MShape or a fully dynamic DShape.");
    if constexpr (GetDynamicRank() > 0) {
      ASC_VERIFY(static_cast<int>(extents.size()) == GetDynamicRank(),
                    "Dynamic shape requires exactly "
                        << GetDynamicRank() << " extents, got "
                        << extents.size());
      int i = 0;
      for (int val : extents) {
        SetDynamicExtent(i++, val);
      }
    }
  }

  /// @brief Get extent in dimension dim
  /// @param dim Dimension index (0 to GetRank()-1)
  /// @return Extent in dimension dim
  ASC_HOST_DEVICE constexpr int GetExtent(int dim) const {
#if !defined(__CUDA_ARCH__)
    ASC_ASSERT(dim >= 0 && dim < GetRank(), "extent index out of bounds");
#endif
    if constexpr (GetRank() == 0) {
      return 1;  // Rank-0 shape: scalar with size 1
    } else if constexpr (IsDynamic()) {
      return extents_[dim];
    } else {
      constexpr int static_extents[GetRank()] = {Extents...};
      return static_extents[dim];
    }
  }

  /// @brief Get shape as an array of extents
  ASC_HOST_DEVICE constexpr int* GetExtents() { return extents_; }
  ASC_HOST_DEVICE constexpr const int* GetExtents() const {
    return extents_;
  }

  /// @brief Array-style access to extent
  ASC_HOST_DEVICE constexpr int operator[](int dim) const {
    return GetExtent(dim);
  }

  /// @brief Get total size (product of all extents)
  ASC_HOST_DEVICE constexpr int GetSize() const {
    int result = 1;
    for (int i = 0; i < GetRank(); ++i) {
      const int extent = GetExtent(i);
#if !defined(__CUDA_ARCH__)
      ASC_VERIFY(extent >= 0, "Shape extent must be non-negative.");
#endif
      if (extent != 0) {
#if !defined(__CUDA_ARCH__)
        ASC_VERIFY(result <= std::numeric_limits<int>::max() / extent,
                      "Shape size overflows int.");
#endif
      }
      result *= extent;
    }
    return result;
  }

 private:
  /// Storage for dynamic extents only
  int extents_[GetNumDynamicExtents()];

  static constexpr bool AreStaticExtentsValid() {
    if constexpr (sizeof...(Extents) == 0) {
      return true;
    } else {
      return ((Extents > 0 || Extents == kDynamicExtent) && ...);
    }
  }

  constexpr void SetDynamicExtent(int dim, int extent) {
    ASC_VERIFY(extent >= 0, "Dynamic extent must be non-negative: "
                                   << extent);
    extents_[dim] = extent;
  }

  template <Integral Extent>
  constexpr void SetDynamicExtentValue(int dim, Extent extent) {
    if constexpr (std::is_signed_v<Extent>) {
      ASC_VERIFY(extent >= 0, "Dynamic extent must be non-negative: "
                                     << extent);
    }
    ASC_VERIFY(
        static_cast<std::uintmax_t>(extent) <=
            static_cast<std::uintmax_t>(std::numeric_limits<int>::max()),
        "Dynamic extent exceeds int range: " << extent);
    extents_[dim] = static_cast<int>(extent);
  }

  /// @brief Helper: Initialize single dynamic extent
  template <typename Extent>
  constexpr void InitExtents(int dim, Extent extent) {
    if constexpr (GetDynamicRank() > 0) {
      SetDynamicExtentValue(dim, extent);
    }
  }

  /// @brief Helper: Initialize multiple dynamic extents recursively
  template <typename First, typename... Rest>
  constexpr void InitExtents(int dim, First first, Rest... rest) {
    if constexpr (GetDynamicRank() > 0) {
      SetDynamicExtentValue(dim, first);
      InitExtents(dim + 1, rest...);
    }
  }
};

template <typename T>
struct IsMShape : std::false_type {};

template <int... Extents>
struct IsMShape<MShape<Extents...>> : std::true_type {};

template <typename Shape>
concept StaticShape = std::decay_t<Shape>::IsStatic();

template <typename Shape>
concept DynamicShape = std::decay_t<Shape>::IsDynamic();

template <typename Shape>
concept SupportedShape = std::decay_t<Shape>::IsSupported();

template <typename Shape>
concept HighRankShape = (std::decay_t<Shape>::GetRank() > 1);

template <typename Shape, int Rank>
concept AlignedShape = std::decay_t<Shape>::IsStatic() ||
                       (std::decay_t<Shape>::GetDynamicRank() == Rank);

/// @brief Implementation details for shape manipulations
namespace details {

/// @brief Extract Nth element from parameter pack
template <int N, int First, int... Rest>
struct NthExtent {
  static constexpr int kValue = NthExtent<N - 1, Rest...>::kValue;
};

template <int First, int... Rest>
struct NthExtent<0, First, Rest...> {
  static constexpr int kValue = First;
};

/// @brief Reverse shape extents at compile time
template <typename S, typename Indices>
struct TransposeShapeImpl;

template <int... Extents, size_t... Is>
struct TransposeShapeImpl<MShape<Extents...>, std::index_sequence<Is...>> {
  using Type =
      MShape<NthExtent<sizeof...(Extents) - 1 - Is, Extents...>::kValue...>;
};

/// @brief Permute shape extents at compile time
template <typename S, typename Indices>
struct PermuteShapeImpl;

template <int... Extents, size_t... Is>
struct PermuteShapeImpl<MShape<Extents...>, std::index_sequence<Is...>> {
  using Type = MShape<NthExtent<Is, Extents...>::kValue...>;
};

}  // namespace details

/// @brief Reverse shape dimensions at compile time
template <typename Shape>
using TransposedShape = typename details::TransposeShapeImpl<
    Shape, std::make_index_sequence<Shape::GetRank()>>::Type;

/// @brief Permute shape dimensions at compile time
template <typename Shape, size_t... Is>
using PermutedShape =
    typename details::PermuteShapeImpl<Shape, std::index_sequence<Is...>>::Type;

/// @brief Create a reversed shape object
template <typename Shape>
inline TransposedShape<Shape> CreateReverseShape(const Shape& shape) {
  if constexpr (Shape::IsStatic()) {
    return TransposedShape<Shape>{};
  } else {
    constexpr int kRank = Shape::GetRank();
    TransposedShape<Shape> rev_shape;
    for (int i = 0; i < kRank; ++i) {
      rev_shape.GetExtents()[i] = shape.GetExtent(kRank - 1 - i);
    }
    return rev_shape;
  }
}

/// @brief Create a permuted shape object
template <typename Shape, size_t... Is>
inline PermutedShape<Shape, Is...> CreatePermuteShape(const Shape& shape) {
  if constexpr (Shape::IsStatic()) {
    return PermutedShape<Shape, Is...>{};
  } else {
    PermutedShape<Shape, Is...> perm_shape;
    constexpr int kRank = Shape::GetRank();
    constexpr size_t perm_indices[] = {Is...};
    for (int i = 0; i < kRank; ++i) {
      perm_shape.GetExtents()[i] = shape.GetExtent(perm_indices[i]);
    }
    return perm_shape;
  }
}

}  // namespace asc

#endif  // ASC_MSHAPE_H_
