// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/spmiterator.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_SPMITERATOR_H_
#define ASC_SPMITERATOR_H_

/// @file spmiterator.h
/// @brief Iterator for non-zero elements in sparse arrays
///
/// SparseMIterator provides iteration over non-zero elements in sparse
/// arrays. Unlike MIterator which iterates over all positions in a dense
/// array, SparseMIterator only visits stored non-zero elements.
///
/// @par Design:
/// - Iterates only over non-zero elements (not all logical positions)
/// - Provides access to both linear offset and multi-dimensional coordinates
/// - Layout-aware for cache-efficient traversal
///
/// @par Iteration Order:
/// - CSR (SparseLayoutRight): Row-major order
/// - CSC (SparseLayoutLeft): Column-major order
/// - COO (SparseLayoutStride): Storage order (typically sorted)
///
/// @par Usage:
/// @code
/// SparseMArray<double, DShape<2>, SparseLayoutRight> mat(DShape<2>(100,
/// 100));
/// // ... insert and finalize ...
///
/// auto& map = mat.GetMap();
/// SparseMIterator<DShape<2>, SparseLayoutRight> iter(map);
///
/// while (!iter.IsExhausted()) {
///   const auto& idx = iter.GetIndex();
///   int i = idx[0], j = idx[1];
///   int k = idx.GetNNZOffset();  // Position in values array
///   ++iter;
/// }
///
/// // Or use range-based for loop
/// for (const auto& idx : iter) {
///   std::cout << "(" << idx[0] << ", " << idx[1] << ")\n";
/// }
/// @endcode

#include "asc/core/error.h"
#include "asc/core/globals.h"
#include "asc/array/mshape.h"
#include "asc/array/spmlayout.h"
#include "asc/array/spmindex.h"

namespace asc {

/// @brief Iterator for non-zero elements in sparse arrays
///
/// SparseMIterator iterates over stored non-zero elements only,
/// providing efficient traversal of sparse data structures.
///
/// @tparam Shape Multi-dimensional shape
/// @tparam Layout Sparse layout policy (SparseLayoutLeft/Right/Stride)
///
/// @par Usage:
/// @code
/// SparseMArray<real_t, DShape<2>, SparseLayoutRight> csr_mat(DShape<2>(100,
/// 100));
/// // ... insert and finalize ...
///
/// auto& map = csr_mat.GetMap();
/// SparseMIterator<DShape<2>, SparseLayoutRight> iter(map);
///
/// while (!iter.IsExhausted()) {
///   const auto& idx = iter.GetIndex();
///   std::cout << "Element at (" << idx[0] << ", " << idx[1] << ")\n";
///   ++iter;
/// }
/// @endcode
template <typename Shape, typename Layout>
class SparseMIterator {
 public:
  using MapType = typename Layout::template Map<Shape>;
  using IndexType = SparseMIndex<Shape, Layout>;

  /// @brief Get the rank (number of dimensions)
  /// @return Rank of the multi-dimensional index
  static constexpr int GetRank() { return Shape::GetRank(); }

  // ========== Construction ==========

  /// @brief Default constructor
  constexpr SparseMIterator() : index_(), limit_(0), iter_count_(0) {}

  /// @brief Construct from sparse layout map (PRIMARY CONSTRUCTOR)
  /// @param map Sparse layout map object
  constexpr explicit SparseMIterator(const MapType& map)
      : index_(map), limit_(map.GetNNZ()), iter_count_(0) {}

  /// @brief Construct from sparse index object
  /// @param index Sparse multi-dimensional index object
  constexpr explicit SparseMIterator(const IndexType& index)
      : index_(index), limit_(index.GetNNZ()), iter_count_(0) {}

  // ========== Iterator State ==========

  /// @brief Reset iterator to the first non-zero element
  constexpr void Reset() {
    iter_count_ = 0;
    if (limit_ > 0) {
      index_.SetNNZOffset(0);
    }
  }

  /// @brief Get total number of non-zero elements to iterate
  /// @return Total number of non-zero elements
  constexpr int GetLimit() const { return limit_; }

  /// @brief Check if the iterator has exhausted all non-zero elements
  /// @return True if exhausted, false otherwise
  constexpr bool IsExhausted() const { return iter_count_ >= limit_; }

  // ========== Index Access ==========

  /// @brief Get the current sparse multi-dimensional index
  /// @return Current sparse multi-dimensional index object
  constexpr const IndexType& GetIndex() const { return index_; }
  constexpr IndexType& GetIndex() { return index_; }

  /// @brief Get multi-dimensional index at dimension dim
  /// @param dim Dimension index (0 to GetRank()-1)
  /// @return Multi-dimensional index at dimension dim
  constexpr int operator[](int dim) const { return index_[dim]; }

  /// @brief Get the current non-zero element offset
  /// @return Current offset in non-zero value array
  constexpr int GetNNZOffset() const { return index_.GetNNZOffset(); }

  // ========== Shape Access ==========

  /// @brief Get the shape object
  /// @return Shape object
  constexpr const Shape& GetShape() const { return index_.GetShape(); }

  /// @brief Get extent in dimension dim
  /// @param dim Dimension index (0 to GetRank()-1)
  /// @return Extent in dimension dim
  constexpr int GetExtent(int dim) const { return index_.GetExtent(dim); }

  /// @brief Get number of non-zeros
  /// @return Total number of non-zero elements
  constexpr int GetNNZ() const { return index_.GetNNZ(); }

  // ========== Iterator Operations ==========

  /// @brief Pre-increment the iterator to next non-zero element
  /// @return Reference to the incremented iterator
  constexpr SparseMIterator& operator++() {
    if (!IsExhausted()) {
      Increment();
    }
    return *this;
  }

  /// @brief Post-increment operator (deleted for performance)
  SparseMIterator operator++(int) = delete;

  /// @brief Dereference operator to access the current index
  /// @return Reference to the current sparse multi-dimensional index
  constexpr const IndexType& operator*() const { return index_; }
  constexpr IndexType& operator*() { return index_; }

  /// @brief Arrow operator to access members of the current index
  /// @return Pointer to the current sparse multi-dimensional index
  constexpr const IndexType* operator->() const { return &index_; }
  constexpr IndexType* operator->() { return &index_; }

  // ========== Comparison ==========

  /// @brief Equality comparison operator
  /// @param other Another SparseMIterator to compare with
  /// @return True if both iterators are at the same position
  constexpr bool operator==(const SparseMIterator& other) const {
    return iter_count_ == other.iter_count_;
  }

  /// @brief Inequality comparison operator
  /// @param other Another SparseMIterator to compare with
  /// @return True if both iterators are at different positions
  constexpr bool operator!=(const SparseMIterator& other) const {
    return iter_count_ != other.iter_count_;
  }

  // ========== Range-based For Loop Support ==========

  /// @brief Get iterator to the beginning for range-based for loops
  /// @return Iterator at the beginning
  constexpr SparseMIterator begin() const {
    SparseMIterator it(*this);
    it.Reset();
    return it;
  }

  /// @brief Get iterator to the end for range-based for loops
  /// @return Iterator at the end
  constexpr SparseMIterator end() const {
    SparseMIterator it(*this);
    it.iter_count_ = it.limit_;
    return it;
  }

 private:
  /// @brief Increment the iterator to next non-zero element
  constexpr void Increment() {
    iter_count_++;
    if (iter_count_ < limit_) {
      index_.SetNNZOffset(iter_count_);
    }
  }

  IndexType index_;  ///< Current sparse index (contains Map)
  int limit_;        ///< Total number of non-zeros to iterate
  int iter_count_;   ///< Current iteration position (0 to limit_-1)
};

}  // namespace asc

#endif  // ASC_SPMITERATOR_H_
