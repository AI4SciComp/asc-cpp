// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/spmarray.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_GENERIC_SPMARRAY_H_
#define ASC_GENERIC_SPMARRAY_H_

/// @file spmarray.h
/// @brief Sparse multi-dimensional array with layout-based compression
///
/// This module provides the SparseMArray<T, Shape, Layout> class template,
/// a sparse array container following the same design pattern as DenseMArray.
///
/// @par Architecture:
/// SparseMArray<T, Shape, Layout> follows the same template pattern as
/// DenseMArray:
/// - T: Element type
/// - Shape: MShape for compile-time or runtime dimensionality
/// - Layout: SparseLayoutLeft (CSC), SparseLayoutRight (CSR), or
/// SparseLayoutStride (COO)
///
/// @par Design Philosophy:
/// - Consistent with DenseMArray template signature
/// - Layout policies encapsulate compression strategy (CSR/CSC/COO)
/// - Can be constructed from COO triplets, then compressed to CSR/CSC
/// - Inherits common operations from MObject via CRTP
///
/// @par Sparse Layouts:
/// - SparseLayoutLeft: Column-major compression (CSC format)
/// - SparseLayoutRight: Row-major compression (CSR format)
/// - SparseLayoutStride: Coordinate format (COO) for construction
/// - DefaultSparseLayout: SparseLayoutLeft or SparseLayoutRight based on
/// ASC_USE_ROW_MAJOR
///
/// @par Storage:
/// - Uses UArray for memory management (supports CPU/GPU)
/// - Layout::Map encapsulates format-specific storage (row_ptr/col_ptr/indices)
/// - Separate values_ array for non-zero values
///
/// @par Usage Pattern:
/// 1. Construct with SparseLayoutStride (COO) for easy insertion
/// 2. Call Compress() to convert to SparseLayoutLeft/Right (CSC/CSR)
/// 3. Use MObject interface for arithmetic operations
///
/// @par Example:
/// @code
/// // Construct sparse matrix in COO format
/// asc::SparseMArray<double, asc::DShape<2>, asc::SparseLayoutStride>
/// coo_mat(
///     asc::DShape<2>(100, 100));
/// coo_mat.Insert(10, 20, 3.14);
/// coo_mat.Insert(50, 30, 2.71);
/// coo_mat.Finalize();  // Sort triplets
///
/// // Convert to CSR for efficient operations
/// asc::SparseMArray<double, asc::DShape<2>, asc::SparseLayoutRight>
/// csr_mat =
///     coo_mat.ToLayout<asc::SparseLayoutRight>();
///
/// // Or use default sparse layout
/// asc::SparseMArray<double, asc::DShape<2>> mat(asc::DShape<2>(100,
/// 100));
/// @endcode

#include <algorithm>
#include <vector>
#include <utility>

#include "asc/core/globals.h"
#include "asc/core/error.h"
#include "asc/array/mobject.h"
#include "asc/array/forwards.h"
#include "asc/array/mshape.h"
#include "asc/array/spmlayout.h"
#include "asc/array/uarray.h"

namespace asc {

template <typename T, typename Shape>
using SpMArrayView = SparseMArray<T, Shape, SparseLayoutStride>;

// ==========================================================================
// Tuple Structures for Sparse Array Construction
// ==========================================================================

/// @brief N-dimensional tuple for sparse array construction
///
/// Stores indices in all dimensions plus a value.
/// Used for constructing sparse arrays from coordinate format.
///
/// @tparam T Element type
template <typename T>
struct SparseTuple {
  UArray<int> indices;  ///< Indices in all dimensions
  T value;              ///< Non-zero value

  /// @brief Default constructor
  SparseTuple() : indices(), value(T(0)) {}

  /// @brief Constructor with UArray indices and value
  /// @param idx UArray of indices (size = rank)
  /// @param v Value
  SparseTuple(const UArray<int>& idx, const T& v) : indices(idx), value(v) {}

  /// @brief Constructor with initializer list and value
  /// @param idx Initializer list of indices
  /// @param v Value
  SparseTuple(std::initializer_list<int> idx, const T& v)
      : indices(idx), value(v) {}

  /// @brief Comparison operator for sorting (lexicographic)
  bool operator<(const SparseTuple& other) const {
    const int size = indices.GetSize();
    const int other_size = other.indices.GetSize();
    if (size != other_size) {
      return size < other_size;
    }
    const int* data1 = indices.HostRead();
    const int* data2 = other.indices.HostRead();
    for (int i = 0; i < size; ++i) {
      if (data1[i] != data2[i]) {
        return data1[i] < data2[i];
      }
    }
    return false;
  }
};

// ==========================================================================
// SparseMArray Class Template
// ==========================================================================

/// @brief Sparse multi-dimensional array with layout-based compression
///
/// SparseMArray provides sparse storage with the same template signature
/// as DenseMArray, using Layout policies to determine compression format.
///
/// @tparam T Element type (must be trivial)
/// @tparam Shape Shape type (e.g., MShape<kDynamicExtent, kDynamicExtent>)
/// @tparam Layout Sparse layout policy (SparseLayoutLeft/Right/Stride)
///
/// @par Layout-Format Mapping:
/// - SparseLayoutLeft → Compress last dimension (CSC for 2D)
/// - SparseLayoutRight → Compress first dimension (CSR for 2D)
/// - SparseLayoutStride → COO (Coordinate) for arbitrary N-D
///
/// @note Supports arbitrary N-dimensional sparse arrays (rank >= 2).
template <typename T, typename Shape, typename Layout = DefaultSparseLayout>
class SparseMArray : public MObject<SparseMArray<T, Shape, Layout>> {
 public:
  /// Type aliases
  using ValueType = T;
  using ShapeType = Shape;
  using LayoutType = Layout;
  using MapType = typename Layout::template Map<Shape>;

  /// Friend declaration for MObject to access private members
  friend class MObject<SparseMArray<T, Shape, Layout>>;

  /// Friend declarations for format conversion
  template <typename U, typename S, typename L>
  friend class SparseMArray;

  // ========== Constructors and Destructor ==========

  /// @brief Default constructor
  SparseMArray() : map_() { InitializeEmptyCompressedStorage(); }

  /// @brief Constructor with shape
  /// @param shape Shape of the sparse array
  explicit SparseMArray(const Shape& shape) : map_(shape) {
    InitializeEmptyCompressedStorage();
  }

  /// @brief Constructor with shape and memory type
  /// @param shape Shape of the sparse array
  /// @param mt Memory type for allocation
  SparseMArray(const Shape& shape, MemoryType mt) : map_(shape) {
    InitializeEmptyCompressedStorage();
    values_ = UArray<T>(mt);
  }

  /// @brief Constructor with shape and capacity estimate
  /// @param shape Shape of the sparse array
  /// @param capacity Expected number of non-zeros
  SparseMArray(const Shape& shape, int capacity) : map_(shape) {
    InitializeEmptyCompressedStorage();
    Reserve(capacity);
  }

  /// @brief Construct sparse array with variadic extents
  /// @tparam Extents Integral types for extents
  /// @param extents Extent for each dimension
  template <Integral... Extents>
  explicit SparseMArray(Extents... extents);

  /// @brief Construct sparse array with memory type and variadic extents
  /// @tparam Extents Integral types for extents
  /// @param mt Memory type for allocation
  /// @param extents Extent for each dimension
  template <Integral... Extents>
  SparseMArray(MemoryType mt, Extents... extents);

  /// @brief Destructor
  ~SparseMArray() = default;

  // ========== Required MObject Interface ==========

  /// @brief Get total number of elements (dense size, including zeros)
  inline int GetSize() const { return map_.GetSize(); }

  /// @brief Get rank (number of dimensions)
  static constexpr int GetRank() { return Shape::GetRank(); }

  /// @brief Get extent along given dimension
  inline int GetExtent(int dim) const { return map_.GetExtent(dim); }

  /// @brief Get const reference to shape
  inline const Shape& GetShape() const { return map_.GetShape(); }

  // ========== Sparse-Specific Interface ==========

  /// @brief Get number of non-zero elements
  inline int GetNNZ() const { return map_.GetNNZ(); }

  /// @brief Get sparsity ratio (nnz / total_size)
  inline double GetSparsity() const {
    const int total = GetSize();
    return (total > 0) ? static_cast<double>(GetNNZ()) / total : 0.0;
  }

  /// @brief Check if matrix is in compressed format
  inline bool IsCompressed() const { return map_.IsCompressed(); }

  /// @brief Get const reference to layout map
  inline const MapType& GetMap() const { return map_; }

  /// @brief Reserve space for non-zero elements
  /// @param capacity Expected number of non-zeros
  void Reserve(int capacity);

  /// @brief Clear all elements
  void Clear();

  /// @brief Insert a non-zero element with N-dimensional indices
  /// @param indices UArray of indices (size must equal rank)
  /// @param value Value to insert
  /// @note Only supported for SparseLayoutStride (COO format)
  void Insert(const UArray<int>& indices, const T& value);

  /// @brief Insert a non-zero element (2D convenience method)
  /// @param row Row index
  /// @param col Column index
  /// @param value Value to insert
  /// @note Only supported for SparseLayoutStride (COO format)
  void Insert(int row, int col, const T& value);

  /// @brief Set from tuples (N-dimensional interface)
  /// @tparam Iterator SparseTuple iterator type
  /// @param begin Iterator to first tuple
  /// @param end Iterator past last tuple
  template <typename Iterator>
  void SetFromTuples(Iterator begin, Iterator end);

  /// @brief Finalize matrix after insertion (sort and compress)
  void Finalize();

  /// @brief Compress the sparse matrix (layout-specific)
  void Compress();

  /// @brief Convert to different sparse layout
  /// @tparam TargetLayout Target sparse layout type
  /// @return New sparse array with target layout
  template <typename TargetLayout>
  SparseMArray<T, Shape, TargetLayout> ToLayout() const;

  /// @brief Get raw access to values array
  inline const UArray<T>& GetValues() const { return values_; }

  /// @brief Get mutable access to values array
  inline UArray<T>& GetValues() { return values_; }

  // ========== Sparse Operations ==========

  /// @brief Transpose the sparse matrix (2D only)
  /// @return Transposed sparse matrix with swapped layout
  /// @note For CSR → CSC and vice versa. COO transposes in-place.
  SparseMArray<T, Shape, typename Layout::TransposeLayout> Transpose() const;

  /// @brief Extract outer dimension slice (layout-dependent)
  /// @param idx Index of the outer dimension
  /// @return Sparse slice in COO format
  /// @note For CSR: extracts row; for CSC: extracts column
  SparseMArray<T, DShape<2>, SparseLayoutStride> GetOuter(int idx) const;

  /// @brief Extract range of outer dimension slices (layout-dependent)
  /// @param start Start index (inclusive)
  /// @param end End index (exclusive)
  /// @return Sparse subarray in COO format
  /// @note For CSR: extracts rows [start, end); for CSC: extracts columns
  /// [start, end)
  SparseMArray<T, Shape, SparseLayoutStride> SliceOuter(int start,
                                                        int end) const;

  /// @brief Extract subarray with range slicing along all dimensions
  /// @param ranges Array of {start, end} pairs for each dimension
  /// @return Sparse subarray in COO format with adjusted shape
  /// @note All dimensions must have rank matching ranges.GetSize()
  SparseMArray<T, Shape, SparseLayoutStride> Slice(
      const UArray<int>& ranges) const;

  /// @brief Extract diagonal elements
  /// @return Dense array of diagonal elements
  UArray<T> GetDiagonal() const;

  /// @brief Set diagonal elements from array
  /// @param diag Array of diagonal values
  /// @note Size must match min(nrows, ncols)
  void SetDiagonal(const UArray<T>& diag);

  // ========== Element Access ==========

  /// @brief Read-only element access with N-dimensional indices
  /// @param indices Array of indices (size must equal rank)
  /// @return Element value at given indices (returns 0 if not stored)
  T At(const UArray<int>& indices) const;

  /// @brief Read-only element access with N-dimensional indices
  /// @param indices Pointer to rank indices
  /// @return Element value at given indices (returns 0 if not stored)
  T At(const int* indices) const;

  /// @brief Read-only element access with variadic indices
  /// @tparam Indices Integral index types
  /// @param indices Variadic integral indices
  /// @return Element value at given indices (returns 0 if not stored)
  template <Integral... Indices>
  T At(Indices... indices) const;

  /// @brief Check if element exists in sparse structure
  /// @param indices Array of indices (size must equal rank)
  /// @return True if element is stored (non-zero), false otherwise
  bool Contains(const UArray<int>& indices) const;

  /// @brief Check if element exists in sparse structure
  /// @param indices Pointer to rank indices
  /// @return True if element is stored, false otherwise
  bool Contains(const int* indices) const;

  /// @brief Check if element exists with variadic indices
  /// @tparam Indices Integral index types
  /// @param indices Variadic integral indices
  /// @return True if element is stored, false otherwise
  template <Integral... Indices>
  bool Contains(Indices... indices) const;

  // ========== Reduction Operations ==========

  /// @brief Sum of all non-zero elements
  /// @return Sum of all stored values
  T Sum() const;

  /// @brief Maximum absolute value
  /// @return Maximum absolute value among non-zero elements
  T Max() const;

  /// @brief Minimum absolute value (among non-zeros)
  /// @return Minimum absolute value among non-zero elements
  T Min() const;

  /// @brief Lp norm of sparse array
  /// @param p Norm parameter (1, 2, or other positive integer)
  /// @return Lp norm value
  T Norm(int p = 2) const;

  // ========== Utility Functions ==========

  /// @brief Remove explicit zeros from storage
  /// @note Compacts the sparse structure by removing stored zeros
  void Compact();

  /// @brief Fill all non-zero entries with a value
  /// @param value Value to fill
  void Fill(T value);

  /// @brief Minimize memory usage
  /// @note Shrinks capacity to match actual number of non-zeros
  void ShrinkToFit();

  // ========== Compound Assignment Operators ==========

  /// @brief Element-wise addition with another sparse array
  /// @param other Sparse array to add
  /// @return Reference to this array
  SparseMArray& operator+=(const SparseMArray<T, Shape, Layout>& other);

  /// @brief Element-wise subtraction with another sparse array
  /// @param other Sparse array to subtract
  /// @return Reference to this array
  SparseMArray& operator-=(const SparseMArray<T, Shape, Layout>& other);

  /// @brief Element-wise multiplication (Hadamard product) with another sparse
  /// array
  /// @param other Sparse array to multiply
  /// @return Reference to this array
  SparseMArray& operator*=(const SparseMArray<T, Shape, Layout>& other);

  /// @brief Element-wise division with another sparse array
  /// @param other Sparse array to divide by
  /// @return Reference to this array
  SparseMArray& operator/=(const SparseMArray<T, Shape, Layout>& other);

  /// @brief Add scalar to all non-zero elements
  /// @param scalar Scalar value to add
  /// @return Reference to this array
  template <Arithmetic U>
  SparseMArray& operator+=(const U& scalar);

  /// @brief Subtract scalar from all non-zero elements
  /// @param scalar Scalar value to subtract
  /// @return Reference to this array
  template <Arithmetic U>
  SparseMArray& operator-=(const U& scalar);

  /// @brief Multiply all non-zero elements by scalar
  /// @param scalar Scalar value to multiply by
  /// @return Reference to this array
  template <Arithmetic U>
  SparseMArray& operator*=(const U& scalar);

  /// @brief Divide all non-zero elements by scalar
  /// @param scalar Scalar value to divide by
  /// @return Reference to this array
  template <Arithmetic U>
  SparseMArray& operator/=(const U& scalar);

  // ========== Unary Operators ==========

  /// @brief Unary negation operator
  /// @return New sparse array with negated elements
  SparseMArray operator-() const;

 private:
  /// @brief Give an empty compressed array a valid outer-pointer structure.
  void InitializeEmptyCompressedStorage() {
    if constexpr (!std::is_same_v<Layout, SparseLayoutStride>) {
      map_.InitializeStorage(0);
      auto& outer_ptr = map_.GetOuterPtr();
      for (int i = 0; i < outer_ptr.GetSize(); ++i) {
        outer_ptr[i] = 0;
      }
    }
  }

  /// @brief Find stored offset for an index tuple.
  /// @param indices Pointer to rank indices
  /// @return Stored non-zero offset, or -1 if absent
  int FindNNZOffset(const int* indices) const;

  MapType map_;       ///< Layout map
  UArray<T> values_;  ///< Non-zero values

  ASC_STATIC_ASSERT(Trivial<T>,
                       "SparseMArray element type T must be a trivial type");
  ASC_STATIC_ASSERT(IsMShape<Shape>::value, "Shape must be an MShape type");
  ASC_STATIC_ASSERT(Shape::IsSupported(),
                       "SparseMArray supports only fully static or fully "
                       "dynamic MShape extents.");
  ASC_STATIC_ASSERT(IsSparseLayout<Layout>::value,
                       "Layout must be a valid sparse layout type");
  ASC_STATIC_ASSERT(Shape::GetRank() >= 2,
                       "SparseMArray requires rank >= 2");
};

}  // namespace asc

// Include implementation
#include "asc/array/spmarray_impl.h"

#endif  // ASC_GENERIC_SPMARRAY_H_
