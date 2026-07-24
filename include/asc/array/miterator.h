// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/miterator.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_MITERATOR_H_
#define ASC_MITERATOR_H_

#include "asc/core/error.h"
#include "asc/core/globals.h"
#include "asc/array/mshape.h"
#include "asc/array/mlayout.h"
#include "asc/array/mindex.h"

namespace asc {

/// @brief Multi-dimensional index iterator with layout support
///
/// MIterator iterates over all indices in the order implied by the layout for
/// contiguous maps, and in canonical column-major index order for strided
/// views. It provides access to both linear and multi-dimensional indices at
/// each position.
///
/// @tparam Shape Multi-dimensional shape
/// @tparam Layout Memory layout policy
///
/// @par Iteration Order:
/// LayoutLeft uses column-major order, LayoutRight uses row-major order, and
/// LayoutStride uses canonical column-major index order with strided offsets.
///
/// @par Usage:
/// @code
/// MShape<2, 3> shape;
/// LayoutLeft::Map<MShape<2, 3>> map(shape);
/// MIterator<MShape<2, 3>> iter(map);
///
/// while (!iter.IsExhausted()) {
///   const auto& idx = iter.GetIndex();
///   int i = idx[0], j = idx[1];
///   int linear = idx.GetOffset();
///   ++iter;
/// }
/// @endcode
template <typename Shape, typename Layout = DefaultLayout>
class MIterator {
 public:
  using MapType = typename Layout::template Map<Shape>;
  using IndexType = MIndex<Shape, Layout>;

  /// @brief Get the rank (number of dimensions)
  /// @return Rank of the multi-dimensional index
  static constexpr int GetRank() { return Shape::GetRank(); }

  // ========== Construction ==========

  /// @brief Default constructor
  constexpr MIterator()
      : index_(), limit_(index_.GetMap().GetSize()), iter_count_(0) {}

  /// @brief Construct from layout map (PRIMARY CONSTRUCTOR)
  /// @param map Layout map object
  constexpr explicit MIterator(const MapType& map)
      : index_(map), limit_(map.GetSize()), iter_count_(0) {}

  /// @brief Construct from index object
  /// @param index Multi-dimensional index object
  constexpr explicit MIterator(const IndexType& index)
      : index_(index), limit_(index.GetMap().GetSize()), iter_count_(0) {}

  /// @brief Construct from shape (convenience, LayoutLeft/LayoutRight only)
  template <NonStrideLayout L = Layout>
  constexpr explicit MIterator(const Shape& shape)
      : MIterator(MapType(shape)) {}

  /// @brief Construct from variadic extents (convenience, dynamic shapes only)
  template <Integral... Extents, NonStrideLayout L = Layout>
    requires(sizeof...(Extents) == GetRank())
  constexpr explicit MIterator(Extents... extents)
      : MIterator(MapType(Shape(extents...))) {}

  // ========== Iterator State ==========

  /// @brief Reset iterator to the beginning
  constexpr void Reset() {
    index_.SetOffset(0);
    iter_count_ = 0;
  }

  /// @brief Get total number of iterations
  /// @return Total number of iterations
  constexpr int GetLimit() const { return limit_; }

  /// @brief Check if the iterator has exhausted all elements
  /// @return True if exhausted, false otherwise
  constexpr bool IsExhausted() const { return iter_count_ >= limit_; }

  // ========== Index Access ==========

  /// @brief Get the current multi-dimensional index
  /// @return Current multi-dimensional index object
  constexpr const IndexType& GetIndex() const { return index_; }
  constexpr IndexType& GetIndex() { return index_; }

  /// @brief Get multi-dimensional index at dimension dim
  /// @param dim Dimension index (0 to GetRank()-1)
  /// @return Multi-dimensional index at dimension dim
  constexpr int operator[](int dim) const { return index_[dim]; }

  /// @brief Get the current linear offset
  /// @return Current linear offset
  constexpr int GetOffset() const { return index_.GetOffset(); }

  // ========== Shape Access ==========

  /// @brief Get the shape object
  /// @return Shape object
  constexpr const Shape& GetShape() const { return index_.GetShape(); }

  /// @brief Get extent in dimension dim
  /// @param dim Dimension index (0 to GetRank()-1)
  /// @return Extent in dimension dim
  constexpr int GetExtent(int dim) const { return index_.GetExtent(dim); }

  // ========== Iterator Operations ==========

  /// @brief Pre-increment the iterator
  /// @return Reference to the incremented iterator
  constexpr MIterator& operator++() {
    if (!IsExhausted()) {
      Increment();
    }
    return *this;
  }

  /// @brief Post-increment operator (deleted for performance)
  MIterator operator++(int) = delete;

  /// @brief Dereference operator to access the current index
  /// @return Reference to the current multi-dimensional index
  constexpr const IndexType& operator*() const { return index_; }
  constexpr IndexType& operator*() { return index_; }

  /// @brief Arrow operator to access members of the current index
  /// @return Pointer to the current multi-dimensional index
  constexpr const IndexType* operator->() const { return &index_; }
  constexpr IndexType* operator->() { return &index_; }

  // ========== Comparison ==========

  /// @brief Equality comparison operator
  /// @param other Another MIterator to compare with
  /// @return True if both iterators are at the same position
  constexpr bool operator==(const MIterator& other) const {
    return iter_count_ == other.iter_count_;
  }

  /// @brief Inequality comparison operator
  /// @param other Another MIterator to compare with
  /// @return True if both iterators are at different positions
  constexpr bool operator!=(const MIterator& other) const {
    return iter_count_ != other.iter_count_;
  }

  // ========== Range-based For Loop Support ==========

  /// @brief Get iterator to the beginning for range-based for loops
  /// @return Iterator at the beginning
  constexpr MIterator begin() const {
    MIterator it(*this);
    it.Reset();
    return it;
  }

  /// @brief Get iterator to the end for range-based for loops
  /// @return Iterator at the end
  constexpr MIterator end() const {
    MIterator it(*this);
    it.iter_count_ = it.limit_;
    return it;
  }

 private:
  /// @brief Increment the iterator according to layout
  constexpr void Increment() {
    int* multi_index = index_.GetMultiIndex();
    const Shape& shape = index_.GetShape();
    MapType map = index_.GetMap();

    iter_count_++;

    if constexpr (std::is_same_v<Layout, LayoutLeft>) {
      // Column-major: first dimension varies fastest
      for (int dim = 0; dim < Shape::GetRank(); ++dim) {
        if (multi_index[dim] + 1 < shape.GetExtent(dim)) {
          multi_index[dim]++;
          index_.SetOffset(map.Fold(multi_index), false);
          return;
        } else {
          multi_index[dim] = 0;
        }
      }
    } else if constexpr (std::is_same_v<Layout, LayoutRight>) {
      // Row-major: last dimension varies fastest
      for (int dim = Shape::GetRank() - 1; dim >= 0; --dim) {
        if (multi_index[dim] + 1 < shape.GetExtent(dim)) {
          multi_index[dim]++;
          index_.SetOffset(map.Fold(multi_index), false);
          return;
        } else {
          multi_index[dim] = 0;
        }
      }
    } else {
      // Strided views iterate in canonical column-major index order while
      // computing offsets through the stride map.
      for (int dim = 0; dim < Shape::GetRank(); ++dim) {
        if (multi_index[dim] + 1 < shape.GetExtent(dim)) {
          multi_index[dim]++;
          index_.SetOffset(map.Fold(multi_index), false);
          return;
        } else {
          multi_index[dim] = 0;
        }
      }
    }
  }

  IndexType index_;  ///< Current index (contains Map)
  int limit_;        ///< Total size to iterate
  int iter_count_;   ///< Current iteration position (0 to limit_-1)
};

template <int... Extents>
using SIterator = MIterator<MShape<Extents...>>;

template <int Rank>
using DIterator = MIterator<DShape<Rank>>;

}  // namespace asc

#endif  // ASC_MITERATOR_H_
