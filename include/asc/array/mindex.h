// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/mindex.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_MINDEX_H_
#define ASC_MINDEX_H_

#include <cstddef>
#include <type_traits>
#include <initializer_list>
#include <limits>
#include <utility>

#include "asc/core/error.h"
#include "asc/core/globals.h"
#include "asc/core/numeric.h"
#include "asc/array/mshape.h"
#include "asc/array/mlayout.h"
#include "asc/array/uarray.h"

namespace asc {

/// @brief Multi-dimensional index data class with layout support
///
/// MIndex stores both linear and multi-dimensional index representations
/// and keeps them synchronized using the Layout::Map for conversion.
///
/// @tparam Shape Multi-dimensional shape
/// @tparam Layout Memory layout policy
///
/// @par Usage:
/// @code
/// MShape<3, 4> shape;
/// LayoutLeft::Map<MShape<3, 4>> map(shape);
/// MIndex<MShape<3, 4>> idx(map);
///
/// idx.SetMulti(1, 2);
/// int linear = idx.GetOffset();  // 7
///
/// idx.SetOffset(5);
/// int i = idx[0], j = idx[1];    // (2, 1)
/// @endcode
template <typename Shape, typename Layout = DefaultLayout>
class MIndex {
 public:
  ASC_STATIC_ASSERT(Shape::IsSupported(),
                       "MIndex supports only fully static or fully dynamic "
                       "MShape extents.");

  using MapType = typename Layout::template Map<Shape>;

  static constexpr int GetRank() { return Shape::GetRank(); }
  static constexpr int GetNDims() { return Shape::GetNDims(); }

  // ========== Construction ==========

  /// @brief Default constructor - initializes to (0, 0, ...)
  constexpr MIndex() : map_(), offset_(0) {
    for (int i = 0; i < GetNDims(); ++i) {
      multi_index_[i] = 0;
    }
  }

  /// @brief Construct from layout map (PRIMARY CONSTRUCTOR)
  constexpr explicit MIndex(const MapType& map) : map_(map), offset_(0) {
    for (int i = 0; i < GetNDims(); ++i) {
      multi_index_[i] = 0;
    }
  }

  /// @brief Construct from shape (convenience, LayoutLeft/LayoutRight only)
  template <typename L = Layout>
    requires NonStrideLayout<L>
  constexpr explicit MIndex(const Shape& shape) : MIndex(MapType(shape)) {}

  /// @brief Construct from variadic extents (convenience, dynamic shapes only)
  template <Integral... Extents>
    requires(sizeof...(Extents) == GetRank() && NonStrideLayout<Layout>)
  constexpr explicit MIndex(Extents... extents)
      : MIndex(MapType(Shape(extents...))) {}

  // ========== Accessors ==========

  /// @brief Get the layout map
  /// @return Layout map object
  constexpr const MapType& GetMap() const { return map_; }

  /// @brief Get the shape object
  /// @return Shape object
  constexpr const Shape& GetShape() const { return map_.GetShape(); }

  /// @brief Get extent in dimension dim
  /// @param dim Dimension index (0 to GetRank()-1)
  /// @return Extent in dimension dim
  constexpr int GetExtent(int dim) const { return map_.GetExtent(dim); }

  /// @brief Get stride in dimension dim
  /// @param dim Dimension index (0 to GetRank()-1)
  /// @return Stride in dimension dim
  constexpr int GetStride(int dim) const { return map_.GetStride(dim); }

  /// @brief Get total size (product of all extents)
  /// @return Total size
  constexpr int GetSize() const { return map_.GetSize(); }

  /// @brief Get memory offset
  /// @return Memory offset
  constexpr int GetOffset() const { return offset_; }

  /// @brief Get multi-dimensional indices as array
  /// @return Multi-dimensional index array
  constexpr int* GetMultiIndex() { return multi_index_; }
  constexpr const int* GetMultiIndex() const { return multi_index_; }

  /// @brief Get multi-dimensional index at dimension dim
  /// @param dim Dimension index (0 to GetRank()-1)
  /// @return Multi-dimensional index at dimension dim
  constexpr int operator[](int dim) const {
    ASC_ASSERT(dim >= 0 && dim < GetNDims(),
                  "Dimension index out of bounds");
    return multi_index_[dim];
  }

  // ========== Mutators ==========

  /// @brief Set from memory offset
  /// @param offset Memory offset
  /// @param sync Whether to synchronize multi-dimensional indices
  constexpr void SetOffset(int offset, bool sync = true) {
    ASC_ASSERT(!sync || (offset >= 0 && offset <= GetSize()),
                  "Memory offset out of bounds");
    offset_ = offset;
    if (sync && offset < GetSize()) {
      map_.Unfold(offset, multi_index_);
    }
  }

  /// @brief Set from index tuple
  /// @tparam Indices Variadic index types
  /// @param indices Integral indices
  template <Integral... Indices>
  constexpr void SetMultiIndex(Indices... indices) {
    ASC_STATIC_ASSERT(sizeof...(indices) == GetNDims(),
                         "Index count mismatch");
    SetMultiIndexImpl(0, indices...);
    offset_ = VariadicFold();
  }

  /// @brief Set from multi-index
  /// @param index Multi-dimensional index array
  /// @param sync Whether to synchronize memory offset
  constexpr void SetMultiIndex(const int* index, bool sync = true) {
    for (int i = 0; i < GetNDims(); ++i) {
      multi_index_[i] = index[i];
    }
    if (sync) {
      offset_ = map_.Fold(multi_index_);
    }
  }

 private:
  // Grant MIterator access to private members for efficient iteration
  template <typename, typename>
  friend class MIterator;
  MapType map_;                  ///< Layout map (shape + strides + logic)
  int offset_;                   ///< Current memory offset
  int multi_index_[GetNDims()];  ///< Current multi-dimensional indices

  // Helper: Set single multi-dimensional index
  template <typename Idx>
  constexpr void SetMultiIndexImpl(int dim, Idx idx) {
    ASC_ASSERT(
        static_cast<int>(idx) >= 0 && static_cast<int>(idx) < GetExtent(dim),
        "Multi-dimensional index out of bounds");
    multi_index_[dim] = static_cast<int>(idx);
  }

  // Helper: Set multiple multi-dimensional indices
  template <typename First, typename... Rest>
  constexpr void SetMultiIndexImpl(int dim, First first, Rest... rest) {
    ASC_ASSERT(static_cast<int>(first) >= 0 &&
                      static_cast<int>(first) < GetExtent(dim),
                  "Multi-dimensional index out of bounds");
    multi_index_[dim] = static_cast<int>(first);
    SetMultiIndexImpl(dim + 1, rest...);
  }

  // Helper: Compute offset from multi-index
  constexpr int VariadicFold() const {
    return VariadicFoldImpl(std::make_index_sequence<GetNDims()>{});
  }
  template <std::size_t... Is>
  constexpr int VariadicFoldImpl(std::index_sequence<Is...>) const {
    return map_(multi_index_[Is]...);
  }
};

template <int... Extents>
using SIndex = MIndex<MShape<Extents...>>;

template <int Rank>
using DIndex = MIndex<DShape<Rank>>;

template <typename Shape, typename Layout = DefaultLayout>
class MIndexArray {
 public:
  ASC_STATIC_ASSERT(Shape::IsSupported(),
                       "MIndexArray supports only fully static or fully "
                       "dynamic MShape extents.");
  static_assert(std::is_same_v<Layout, LayoutLeft> ||
                    std::is_same_v<Layout, LayoutRight>,
                "MIndexArray supports LayoutLeft or LayoutRight.");

  using IndexType = MIndex<Shape, Layout>;
  using MapType = typename Layout::template Map<Shape>;

  MIndexArray() = default;

  explicit MIndexArray(const UArray<IndexType>& indices) : indices_(indices) {}

  static MIndexArray Box(const Shape& shape) {
    MIndexArray result;
    result.BuildBox(shape);
    return result;
  }

  static MIndexArray Box(int max_index) {
    MIndexArray result;
    const Shape shape = MakeUniformShape(max_index + 1);
    result.BuildBox(shape, max_index);
    return result;
  }

  static MIndexArray TotalDegree(const Shape& shape, int max_sum,
                                 int min_sum = 0) {
    MIndexArray result;
    result.BuildTotalDegree(shape, max_sum, min_sum);
    return result;
  }

  static MIndexArray TotalDegree(int max_sum, int min_sum = 0) {
    MIndexArray result;
    const Shape shape = MakeUniformShape(max_sum + 1);
    result.BuildTotalDegree(shape, max_sum, min_sum);
    return result;
  }

  int GetSize() const { return indices_.GetSize(); }
  int GetNIndices() const { return indices_.GetSize(); }
  const UArray<IndexType>& GetIndices() const { return indices_; }

  const IndexType& Get(int index) const {
    ASC_VERIFY(index >= 0 && index < GetSize(),
                  "MIndexArray index out of bounds");
    return indices_.HostRead()[index];
  }

  IndexType& Get(int index) {
    ASC_VERIFY(index >= 0 && index < GetSize(),
                  "MIndexArray index out of bounds");
    return indices_.HostReadWrite()[index];
  }

 private:
  UArray<IndexType> indices_;

  static Shape MakeUniformShape(int extent) {
    ASC_VERIFY(extent > 0, "MIndexArray extent must be positive");
    if constexpr (Shape::IsDynamic()) {
      int extents[Shape::GetRank()];
      for (int dim = 0; dim < Shape::GetRank(); ++dim) {
        extents[dim] = extent;
      }
      return Shape(extents);
    } else {
      Shape shape;
      for (int dim = 0; dim < Shape::GetRank(); ++dim) {
        ASC_VERIFY(shape.GetExtent(dim) >= extent,
                      "Static MIndexArray shape is too small");
      }
      return shape;
    }
  }

  void BuildBox(const Shape& shape) {
    ValidateShape(shape);
    const MapType map(shape);
    indices_.SetSize(shape.GetSize());
    IndexType* data = indices_.HostWrite();
    for (int offset = 0; offset < shape.GetSize(); ++offset) {
      data[offset] = IndexType(map);
      data[offset].SetOffset(offset);
    }
  }

  void BuildBox(const Shape& shape, int max_index) {
    ValidateShape(shape);
    ASC_VERIFY(max_index >= 0, "Maximum index must be nonnegative");
    for (int dim = 0; dim < Shape::GetRank(); ++dim) {
      ASC_VERIFY(shape.GetExtent(dim) > max_index,
                    "MIndexArray shape does not cover the box");
    }

    int n_indices = 1;
    const int extent = max_index + 1;
    for (int dim = 0; dim < Shape::GetRank(); ++dim) {
      CheckProduct(n_indices, extent);
      n_indices *= extent;
    }

    const MapType map(shape);
    indices_.SetSize(n_indices);
    UArray<int> values(Shape::GetNDims());
    int* multi = values.HostWrite();
    IndexType* data = indices_.HostWrite();
    for (int index = 0; index < n_indices; ++index) {
      UnfoldBox(index, extent, multi);
      data[index] = IndexType(map);
      data[index].SetMultiIndex(multi);
    }
  }

  void BuildTotalDegree(const Shape& shape, int max_sum, int min_sum) {
    ValidateShape(shape);
    ASC_VERIFY(min_sum >= 0, "Minimum index sum must be nonnegative");
    ASC_VERIFY(max_sum >= 0, "Maximum index sum must be nonnegative");
    ASC_VERIFY(min_sum <= max_sum,
                  "Minimum index sum must not exceed maximum index sum");
    for (int dim = 0; dim < Shape::GetRank(); ++dim) {
      ASC_VERIFY(shape.GetExtent(dim) > max_sum,
                    "MIndexArray shape does not cover total degree indices");
    }

    int n_indices = 0;
    for (int sum = min_sum; sum <= max_sum; ++sum) {
      const int count = Binomial(sum + Shape::GetRank() - 1,
                                 Shape::GetRank() - 1);
      CheckSum(n_indices, count);
      n_indices += count;
    }

    indices_.SetSize(n_indices);
    UArray<int> values(Shape::GetNDims());
    int* multi = values.HostWrite();
    int index = 0;
    const MapType map(shape);
    for (int sum = min_sum; sum <= max_sum; ++sum) {
      if constexpr (std::is_same_v<Layout, LayoutLeft>) {
        FillLeft(map, Shape::GetRank() - 1, sum, multi, &index);
      } else {
        FillRight(map, 0, sum, multi, &index);
      }
    }
    ASC_VERIFY(index == n_indices, "MIndexArray generation count mismatch");
  }

  static void ValidateShape(const Shape& shape) {
    ASC_VERIFY(Shape::GetRank() > 0, "MIndexArray shape must be nonempty");
    for (int dim = 0; dim < Shape::GetRank(); ++dim) {
      ASC_VERIFY(shape.GetExtent(dim) > 0,
                    "MIndexArray extents must be positive");
    }
  }

  static void UnfoldBox(int linear, int extent, int* multi) {
    int value = linear;
    if constexpr (std::is_same_v<Layout, LayoutLeft>) {
      for (int dim = 0; dim < Shape::GetRank(); ++dim) {
        multi[dim] = value % extent;
        value /= extent;
      }
    } else {
      for (int dim = Shape::GetRank() - 1; dim >= 0; --dim) {
        multi[dim] = value % extent;
        value /= extent;
      }
    }
  }

  void FillLeft(const MapType& map, int dim, int remaining, int* multi,
                int* index) {
    if (dim == 0) {
      multi[0] = remaining;
      WriteIndex(map, multi, (*index)++);
      return;
    }

    for (int value = 0; value <= remaining; ++value) {
      multi[dim] = value;
      FillLeft(map, dim - 1, remaining - value, multi, index);
    }
  }

  void FillRight(const MapType& map, int dim, int remaining, int* multi,
                 int* index) {
    if (dim == Shape::GetRank() - 1) {
      multi[dim] = remaining;
      WriteIndex(map, multi, (*index)++);
      return;
    }

    for (int value = 0; value <= remaining; ++value) {
      multi[dim] = value;
      FillRight(map, dim + 1, remaining - value, multi, index);
    }
  }

  void WriteIndex(const MapType& map, const int* multi, int index) {
    IndexType* data = indices_.HostReadWrite();
    data[index] = IndexType(map);
    data[index].SetMultiIndex(multi);
  }

  static void CheckProduct(int left, int right) {
    ASC_VERIFY(left >= 0 && right >= 0,
                  "MIndexArray product factors must be nonnegative");
    if (left == 0 || right == 0) return;
    ASC_VERIFY(left <= std::numeric_limits<int>::max() / right,
                  "MIndexArray size overflows int");
  }

  static void CheckSum(int left, int right) {
    ASC_VERIFY(left >= 0 && right >= 0,
                  "MIndexArray sum terms must be nonnegative");
    ASC_VERIFY(left <= std::numeric_limits<int>::max() - right,
                  "MIndexArray size overflows int");
  }
};

}  // namespace asc

#endif  // ASC_MINDEX_H_
