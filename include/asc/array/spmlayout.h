// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/spmlayout.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_GENERIC_SPMLAYOUT_H_
#define ASC_GENERIC_SPMLAYOUT_H_

/// @file spmlayout.h
/// @brief Sparse layout policies for sparse multi-dimensional arrays
///
/// This module provides layout policies for sparse arrays, analogous to
/// LayoutLeft/LayoutRight/LayoutStride for dense arrays. Sparse layouts
/// determine the compression scheme (CSR vs CSC) and iteration order.
///
/// @par Sparse Layout Hierarchy:
/// - SparseLayoutLeft: Column-major compression (CSC format)
/// - SparseLayoutRight: Row-major compression (CSR format)
/// - SparseLayoutStride: Custom sparse layout (COO or custom formats)
///
/// @par Design Philosophy:
/// Similar to dense layouts, sparse layouts encapsulate:
/// - Compression strategy (CSR/CSC/COO)
/// - Iteration order
/// - Index mapping logic
/// - Storage format details
///
/// @par Default Layout:
/// - SparseLayoutLeft (CSC) when ASC_USE_ROW_MAJOR is not defined
/// - SparseLayoutRight (CSR) when ASC_USE_ROW_MAJOR is defined

#include <type_traits>
#include <vector>

#include "asc/core/error.h"
#include "asc/core/globals.h"
#include "asc/array/mshape.h"
#include "asc/array/uarray.h"

namespace asc {

/// @brief Base class for sparse layout maps (CRTP)
///
/// Provides common functionality for all sparse layout map classes:
/// - Shape storage
/// - Non-zero count tracking
/// - Common query methods
/// - Compression state
///
/// @tparam Shape The shape type
/// @tparam Derived The derived map class
template <typename Shape, typename Derived>
class SparseLayoutMapBase {
 public:
  ASC_STATIC_ASSERT(Shape::IsSupported(),
                       "Sparse layout maps support only fully static or fully "
                       "dynamic MShape extents.");

  /// @brief Get the rank (number of dimensions)
  static constexpr int GetRank() { return Shape::GetRank(); }

  /// @brief Get the number of dimensions
  static constexpr int GetNDims() { return Shape::GetNDims(); }

  // ========== Shape Accessors ==========

  /// @brief Get the shape object
  constexpr const Shape& GetShape() const { return shape_; }

  /// @brief Get extent in dimension dim
  constexpr int GetExtent(int dim) const {
    ASC_VERIFY(dim >= 0 && dim < GetRank(),
                  "Dimension index out of bounds");
    return shape_.GetExtent(dim);
  }

  /// @brief Get total size (product of all extents, dense size)
  constexpr int GetSize() const { return shape_.GetSize(); }

  // ========== Sparse-Specific Accessors ==========

  /// @brief Get number of non-zero elements
  int GetNNZ() const { return nnz_; }

  /// @brief Set number of non-zero elements
  void SetNNZ(int nnz) { nnz_ = nnz; }

  /// @brief Check if compressed
  bool IsCompressed() const { return compressed_; }

  /// @brief Set compression state
  void SetCompressed(bool compressed) { compressed_ = compressed; }

  /// @brief Get required memory size for sparse storage
  int GetRequiredSize() const {
    return static_cast<const Derived*>(this)->GetRequiredSizeImpl();
  }

 protected:
  Shape shape_;       ///< Multi-dimensional shape
  int nnz_;           ///< Number of non-zero elements
  bool compressed_;   ///< Whether the matrix is compressed

  constexpr SparseLayoutMapBase()
      : shape_(), nnz_(0), compressed_(false) {}

  constexpr explicit SparseLayoutMapBase(const Shape& shape)
      : shape_(shape), nnz_(0), compressed_(false) {}
};

// ==========================================================================
// SparseLayoutLeft: Compress last dimension
// ==========================================================================

/// @brief Left-major sparse layout (compress last dimension)
///
/// SparseLayoutLeft compresses the last dimension of N-D arrays:
/// - For 2D: CSC format (compress columns)
/// - For 3D+: Compress along last dimension
///
/// @par Storage Pattern:
/// - outer_ptr_: Pointers for the last dimension (size = last_extent + 1)
/// - inner_indices_: Array of (rank-1) index arrays, each of size nnz
///
/// @par Example (2D):
/// - outer_ptr_ = column pointers (ncols + 1)
/// - inner_indices_[0] = row indices (nnz)
///
/// @par Example (3D, shape [I, J, K]):
/// - outer_ptr_ = K-pointers (K + 1)
/// - inner_indices_[0] = I-indices (nnz)
/// - inner_indices_[1] = J-indices (nnz)
///
/// @par Best For:
/// - Last-dimension-oriented operations
/// - Column-major storage patterns
struct SparseLayoutLeft {
  /// Type alias for transpose layout (CSC → CSR)
  using TransposeLayout = struct SparseLayoutRight;

  template <typename Shape>
  class Map : public SparseLayoutMapBase<Shape, Map<Shape>> {
   private:
    using Base = SparseLayoutMapBase<Shape, Map<Shape>>;
    friend Base;

   public:
    /// @brief Default constructor
    constexpr Map() : Base() {
      const int rank = Shape::GetRank();
      inner_indices_.resize(rank - 1);
    }

    /// @brief Construct from shape
    constexpr explicit Map(const Shape& shape) : Base(shape) {
      const int rank = Shape::GetRank();
      inner_indices_.resize(rank - 1);
    }

    /// @brief Get outer pointer array (last dimension, mutable)
    UArray<int>& GetOuterPtr() { return outer_ptr_; }

    /// @brief Get outer pointer array (last dimension, const)
    const UArray<int>& GetOuterPtr() const { return outer_ptr_; }

    /// @brief Get inner indices array for dimension dim (mutable)
    /// @param dim Dimension index (0 to rank-2)
    UArray<int>& GetInnerIndices(int dim) {
      ASC_VERIFY(dim >= 0 && dim < static_cast<int>(inner_indices_.size()),
                    "Dimension index out of bounds");
      return inner_indices_[dim];
    }

    /// @brief Get inner indices array for dimension dim (const)
    /// @param dim Dimension index (0 to rank-2)
    const UArray<int>& GetInnerIndices(int dim) const {
      ASC_VERIFY(dim >= 0 && dim < static_cast<int>(inner_indices_.size()),
                    "Dimension index out of bounds");
      return inner_indices_[dim];
    }

    /// @brief Get all inner indices arrays (const)
    const std::vector<UArray<int>>& GetInnerIndices() const {
      return inner_indices_;
    }

    /// @brief Get all inner indices arrays (mutable)
    std::vector<UArray<int>>& GetInnerIndices() {
      return inner_indices_;
    }

    // Backward compatibility aliases for 2D (CSC)
    UArray<int>& GetColPtr() { return outer_ptr_; }
    const UArray<int>& GetColPtr() const { return outer_ptr_; }
    UArray<int>& GetRowIndices() { return inner_indices_[0]; }
    const UArray<int>& GetRowIndices() const { return inner_indices_[0]; }

    /// @brief Initialize storage for compressed format
    /// @param nnz Number of non-zeros
    void InitializeStorage(int nnz) {
      const int rank = this->shape_.GetRank();
      const int outer_extent = this->shape_.GetExtent(rank - 1);

      outer_ptr_.SetSize(outer_extent + 1);
      for (int d = 0; d < rank - 1; ++d) {
        inner_indices_[d].SetSize(nnz);
      }

      this->nnz_ = nnz;
      this->compressed_ = true;
    }

    /// @brief Reserve capacity for non-zeros
    void Reserve(int capacity) {
      for (auto& indices : inner_indices_) {
        indices.Reserve(capacity);
      }
    }

    /// @brief Clear all storage
    void Clear() {
      outer_ptr_.DeleteAll();
      for (auto& indices : inner_indices_) {
        indices.DeleteAll();
      }
      this->nnz_ = 0;
      this->compressed_ = false;
    }

   private:
    using Base::shape_;
    using Base::nnz_;
    using Base::compressed_;

    UArray<int> outer_ptr_;                   ///< Outer pointers (last dimension)
    std::vector<UArray<int>> inner_indices_;  ///< Inner indices (rank-1 arrays)

    /// @brief Get required memory size
    int GetRequiredSizeImpl() const {
      const int rank = shape_.GetRank();
      const int outer_extent = shape_.GetExtent(rank - 1);
      return (outer_extent + 1) + (rank - 1) * nnz_;
    }
  };
};

// ==========================================================================
// SparseLayoutRight: Compress first dimension
// ==========================================================================

/// @brief Right-major sparse layout (compress first dimension)
///
/// SparseLayoutRight compresses the first dimension of N-D arrays:
/// - For 2D: CSR format (compress rows)
/// - For 3D+: Compress along first dimension
///
/// @par Storage Pattern:
/// - outer_ptr_: Pointers for the first dimension (size = first_extent + 1)
/// - inner_indices_: Array of (rank-1) index arrays, each of size nnz
///
/// @par Example (2D):
/// - outer_ptr_ = row pointers (nrows + 1)
/// - inner_indices_[0] = column indices (nnz)
///
/// @par Example (3D, shape [I, J, K]):
/// - outer_ptr_ = I-pointers (I + 1)
/// - inner_indices_[0] = J-indices (nnz)
/// - inner_indices_[1] = K-indices (nnz)
///
/// @par Best For:
/// - First-dimension-oriented operations
/// - Row-major storage patterns
struct SparseLayoutRight {
  /// Type alias for transpose layout (CSR → CSC)
  using TransposeLayout = struct SparseLayoutLeft;

  template <typename Shape>
  class Map : public SparseLayoutMapBase<Shape, Map<Shape>> {
   private:
    using Base = SparseLayoutMapBase<Shape, Map<Shape>>;
    friend Base;

   public:
    /// @brief Default constructor
    constexpr Map() : Base() {
      const int rank = Shape::GetRank();
      inner_indices_.resize(rank - 1);
    }

    /// @brief Construct from shape
    constexpr explicit Map(const Shape& shape) : Base(shape) {
      const int rank = Shape::GetRank();
      inner_indices_.resize(rank - 1);
    }

    /// @brief Get outer pointer array (first dimension, mutable)
    UArray<int>& GetOuterPtr() { return outer_ptr_; }

    /// @brief Get outer pointer array (first dimension, const)
    const UArray<int>& GetOuterPtr() const { return outer_ptr_; }

    /// @brief Get inner indices array for dimension dim (mutable)
    /// @param dim Dimension index (0 to rank-2)
    UArray<int>& GetInnerIndices(int dim) {
      ASC_VERIFY(dim >= 0 && dim < static_cast<int>(inner_indices_.size()),
                    "Dimension index out of bounds");
      return inner_indices_[dim];
    }

    /// @brief Get inner indices array for dimension dim (const)
    /// @param dim Dimension index (0 to rank-2)
    const UArray<int>& GetInnerIndices(int dim) const {
      ASC_VERIFY(dim >= 0 && dim < static_cast<int>(inner_indices_.size()),
                    "Dimension index out of bounds");
      return inner_indices_[dim];
    }

    /// @brief Get all inner indices arrays (const)
    const std::vector<UArray<int>>& GetInnerIndices() const {
      return inner_indices_;
    }

    /// @brief Get all inner indices arrays (mutable)
    std::vector<UArray<int>>& GetInnerIndices() {
      return inner_indices_;
    }

    // Backward compatibility aliases for 2D (CSR)
    UArray<int>& GetRowPtr() { return outer_ptr_; }
    const UArray<int>& GetRowPtr() const { return outer_ptr_; }
    UArray<int>& GetColIndices() { return inner_indices_[0]; }
    const UArray<int>& GetColIndices() const { return inner_indices_[0]; }

    /// @brief Initialize storage for compressed format
    /// @param nnz Number of non-zeros
    void InitializeStorage(int nnz) {
      const int outer_extent = this->shape_.GetExtent(0);

      outer_ptr_.SetSize(outer_extent + 1);
      const int rank = this->shape_.GetRank();
      for (int d = 0; d < rank - 1; ++d) {
        inner_indices_[d].SetSize(nnz);
      }

      this->nnz_ = nnz;
      this->compressed_ = true;
    }

    /// @brief Reserve capacity for non-zeros
    void Reserve(int capacity) {
      for (auto& indices : inner_indices_) {
        indices.Reserve(capacity);
      }
    }

    /// @brief Clear all storage
    void Clear() {
      outer_ptr_.DeleteAll();
      for (auto& indices : inner_indices_) {
        indices.DeleteAll();
      }
      this->nnz_ = 0;
      this->compressed_ = false;
    }

   private:
    using Base::shape_;
    using Base::nnz_;
    using Base::compressed_;

    UArray<int> outer_ptr_;                   ///< Outer pointers (first dimension)
    std::vector<UArray<int>> inner_indices_;  ///< Inner indices (rank-1 arrays)

    /// @brief Get required memory size
    int GetRequiredSizeImpl() const {
      const int outer_extent = shape_.GetExtent(0);
      const int rank = shape_.GetRank();
      return (outer_extent + 1) + (rank - 1) * nnz_;
    }
  };
};

// ==========================================================================
// SparseLayoutStride: N-dimensional COO sparse layout
// ==========================================================================

/// @brief Custom sparse layout (N-dimensional COO format)
///
/// SparseLayoutStride implements Coordinate (COO) format for N-D arrays:
/// - indices_[d][k] = index in dimension d of k-th non-zero
/// - values_[k] = value of k-th non-zero
///
/// @par Storage Arrays:
/// - indices_: vector of rank UArray<int>, each of size nnz
///
/// @par Example (2D):
/// - indices_[0] = row indices (nnz)
/// - indices_[1] = column indices (nnz)
///
/// @par Example (3D):
/// - indices_[0] = I-indices (nnz)
/// - indices_[1] = J-indices (nnz)
/// - indices_[2] = K-indices (nnz)
///
/// @par Best For:
/// - Incremental construction
/// - Easy format conversion
/// - Unstructured sparse patterns
struct SparseLayoutStride {
  /// Type alias for transpose layout (COO transposes in-place)
  using TransposeLayout = SparseLayoutStride;

  template <typename Shape>
  class Map : public SparseLayoutMapBase<Shape, Map<Shape>> {
   private:
    using Base = SparseLayoutMapBase<Shape, Map<Shape>>;
    friend Base;

   public:
    /// @brief Default constructor
    constexpr Map() : Base() {
      const int rank = Shape::GetRank();
      indices_.resize(rank);
    }

    /// @brief Construct from shape
    constexpr explicit Map(const Shape& shape) : Base(shape) {
      const int rank = Shape::GetRank();
      indices_.resize(rank);
    }

    /// @brief Get indices array for dimension dim (mutable)
    /// @param dim Dimension index (0 to rank-1)
    UArray<int>& GetIndices(int dim) {
      ASC_VERIFY(dim >= 0 && dim < static_cast<int>(indices_.size()),
                    "Dimension index out of bounds");
      return indices_[dim];
    }

    /// @brief Get indices array for dimension dim (const)
    /// @param dim Dimension index (0 to rank-1)
    const UArray<int>& GetIndices(int dim) const {
      ASC_VERIFY(dim >= 0 && dim < static_cast<int>(indices_.size()),
                    "Dimension index out of bounds");
      return indices_[dim];
    }

    /// @brief Get all indices arrays (const)
    const std::vector<UArray<int>>& GetIndices() const {
      return indices_;
    }

    /// @brief Get all indices arrays (mutable)
    std::vector<UArray<int>>& GetIndices() {
      return indices_;
    }

    // Backward compatibility aliases for 2D
    UArray<int>& GetRowIndices() { return indices_[0]; }
    const UArray<int>& GetRowIndices() const { return indices_[0]; }
    UArray<int>& GetColIndices() { return indices_[1]; }
    const UArray<int>& GetColIndices() const { return indices_[1]; }

    /// @brief Initialize storage for COO format
    /// @param nnz Number of non-zeros
    void InitializeStorage(int nnz) {
      const int rank = this->shape_.GetRank();
      for (int d = 0; d < rank; ++d) {
        indices_[d].SetSize(nnz);
      }
      this->nnz_ = nnz;
      this->compressed_ = false;
    }

    /// @brief Reserve capacity for non-zeros
    void Reserve(int capacity) {
      for (auto& idx_array : indices_) {
        idx_array.Reserve(capacity);
      }
    }

    /// @brief Clear all storage
    void Clear() {
      for (auto& idx_array : indices_) {
        idx_array.DeleteAll();
      }
      this->nnz_ = 0;
      this->compressed_ = false;
    }

   private:
    using Base::shape_;
    using Base::nnz_;
    using Base::compressed_;

    std::vector<UArray<int>> indices_;  ///< Indices for all dimensions (COO)

    /// @brief Get required memory size
    int GetRequiredSizeImpl() const {
      const int rank = shape_.GetRank();
      return rank * nnz_;  // rank index arrays
    }
  };
};

#ifdef ASC_USE_ROW_MAJOR
using DefaultSparseLayout = SparseLayoutRight;
#else
using DefaultSparseLayout = SparseLayoutLeft;
#endif

template <typename T>
struct IsSparseLayout : std::false_type {};

template <>
struct IsSparseLayout<SparseLayoutLeft> : std::true_type {};

template <>
struct IsSparseLayout<SparseLayoutRight> : std::true_type {};

template <>
struct IsSparseLayout<SparseLayoutStride> : std::true_type {};

template <typename T>
concept SparseLayoutLike = IsSparseLayout<T>::value;

}  // namespace asc

#endif  // ASC_GENERIC_SPMLAYOUT_H_
