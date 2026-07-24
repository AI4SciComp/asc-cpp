// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/spmindex.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_SPMINDEX_H_
#define ASC_SPMINDEX_H_

/// @file spmindex.h
/// @brief Multi-dimensional index for sparse arrays
///
/// SparseMIndex provides index tracking for non-zero elements in sparse
/// arrays. Unlike MIndex which tracks positions in dense arrays,
/// SparseMIndex tracks both the linear position in the sparse storage
/// and the multi-dimensional coordinates in the logical sparse array.
///
/// @par Design:
/// - Tracks position in non-zero value array (linear offset)
/// - Provides multi-dimensional coordinates for the non-zero element
/// - Layout-aware for efficient traversal (CSR/CSC/COO)
///
/// @par Usage:
/// @code
/// SparseMArray<double, DShape<2>, SparseLayoutRight> mat(DShape<2>(10, 10));
/// // ... insert elements ...
/// mat.Finalize();
///
/// auto& map = mat.GetMap();
/// SparseMIndex<DShape<2>, SparseLayoutRight> idx(map);
///
/// idx.SetNNZOffset(5);  // Go to 5th non-zero element
/// int i = idx[0], j = idx[1];  // Get multi-dimensional coordinates
/// @endcode

#include <cstddef>
#include <type_traits>

#include "asc/core/error.h"
#include "asc/core/globals.h"
#include "asc/array/mshape.h"
#include "asc/array/spmlayout.h"

namespace asc {

/// @brief Multi-dimensional index for sparse arrays
///
/// SparseMIndex stores both the linear offset in the non-zero value array
/// and the corresponding multi-dimensional coordinates in the logical array.
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
/// SparseMIndex<DShape<2>, SparseLayoutRight> idx(map);
///
/// idx.SetNNZOffset(0);
/// for (int k = 0; k < csr_mat.GetNNZ(); ++k) {
///   idx.SetNNZOffset(k);
///   std::cout << "(" << idx[0] << ", " << idx[1] << ")\n";
/// }
/// @endcode
template <typename Shape, typename Layout>
class SparseMIndex {
 public:
  using MapType = typename Layout::template Map<Shape>;

  static constexpr int GetRank() { return Shape::GetRank(); }
  static constexpr int GetNDims() { return Shape::GetNDims(); }

  // ========== Construction ==========

  /// @brief Default constructor - initializes to first non-zero element
  constexpr SparseMIndex() : map_(), nnz_offset_(0) {
    for (int i = 0; i < GetNDims(); ++i) {
      multi_index_[i] = 0;
    }
  }

  /// @brief Construct from sparse layout map (PRIMARY CONSTRUCTOR)
  constexpr explicit SparseMIndex(const MapType& map)
      : map_(map), nnz_offset_(0) {
    for (int i = 0; i < GetNDims(); ++i) {
      multi_index_[i] = 0;
    }
    // Initialize multi_index_ to first non-zero element's coordinates
    if (map_.GetNNZ() > 0) {
      UpdateMultiIndex();
    }
  }

  // ========== Accessors ==========

  /// @brief Get the sparse layout map
  /// @return Sparse layout map object
  constexpr const MapType& GetMap() const { return map_; }

  /// @brief Get the shape object
  /// @return Shape object
  constexpr const Shape& GetShape() const { return map_.GetShape(); }

  /// @brief Get extent in dimension dim
  /// @param dim Dimension index (0 to GetRank()-1)
  /// @return Extent in dimension dim
  constexpr int GetExtent(int dim) const { return map_.GetExtent(dim); }

  /// @brief Get number of non-zeros
  /// @return Total number of non-zero elements
  constexpr int GetNNZ() const { return map_.GetNNZ(); }

  /// @brief Get current position in non-zero value array
  /// @return Linear offset in non-zero value array
  constexpr int GetNNZOffset() const { return nnz_offset_; }

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

  /// @brief Set from non-zero element offset
  /// @param offset Linear offset in non-zero value array
  constexpr void SetNNZOffset(int offset) {
    ASC_ASSERT(offset >= 0 && offset < GetNNZ(),
                  "Non-zero offset out of bounds");
    nnz_offset_ = offset;
    UpdateMultiIndex();
  }

  /// @brief Set from multi-dimensional indices (finds matching non-zero)
  /// @tparam Indices Variadic index types
  /// @param indices Integral indices
  /// @return True if element exists, false otherwise
  template <Integral... Indices>
  constexpr bool SetMultiIndex(Indices... indices) {
    ASC_STATIC_ASSERT(sizeof...(indices) == GetNDims(),
                         "Index count mismatch");
    int temp_index[GetNDims()];
    int pos = 0;
    ((temp_index[pos++] = static_cast<int>(indices)), ...);

    return SetMultiIndexImpl(temp_index);
  }

  /// @brief Set from multi-dimensional index array
  /// @param index Multi-dimensional index array
  /// @return True if element exists, false otherwise
  constexpr bool SetMultiIndex(const int* index) {
    return SetMultiIndexImpl(index);
  }

 private:
  // Grant SparseMIterator access to private members
  template <typename, typename>
  friend class SparseMIterator;

  MapType map_;                  ///< Sparse layout map
  int nnz_offset_;               ///< Current offset in non-zero value array
  int multi_index_[GetNDims()];  ///< Current multi-dimensional indices

  /// @brief Update multi-dimensional indices from current nnz_offset_
  constexpr void UpdateMultiIndex() {
    if constexpr (std::is_same<Layout, SparseLayoutRight>::value) {
      // CSR format: row_ptr[i] <= k < row_ptr[i+1] means row i
      const auto& row_ptr = map_.GetRowPtr();
      const auto& col_indices = map_.GetColIndices();
      const int* row_ptr_data = row_ptr.HostRead();
      const int* col_data = col_indices.HostRead();

      const int nrows = GetExtent(0);
      int row = 0;
      for (int i = 0; i < nrows; ++i) {
        if (nnz_offset_ >= row_ptr_data[i] && nnz_offset_ < row_ptr_data[i + 1]) {
          row = i;
          break;
        }
      }

      multi_index_[0] = row;
      multi_index_[1] = col_data[nnz_offset_];

    } else if constexpr (std::is_same<Layout, SparseLayoutLeft>::value) {
      // CSC format: col_ptr[j] <= k < col_ptr[j+1] means column j
      const auto& col_ptr = map_.GetColPtr();
      const auto& row_indices = map_.GetRowIndices();
      const int* col_ptr_data = col_ptr.HostRead();
      const int* row_data = row_indices.HostRead();

      const int ncols = GetExtent(1);
      int col = 0;
      for (int j = 0; j < ncols; ++j) {
        if (nnz_offset_ >= col_ptr_data[j] && nnz_offset_ < col_ptr_data[j + 1]) {
          col = j;
          break;
        }
      }

      multi_index_[0] = row_data[nnz_offset_];
      multi_index_[1] = col;

    } else if constexpr (std::is_same<Layout, SparseLayoutStride>::value) {
      // COO format: read indices directly
      const int rank = GetRank();
      for (int d = 0; d < rank; ++d) {
        const auto& dim_indices = map_.GetIndices(d);
        const int* dim_data = dim_indices.HostRead();
        multi_index_[d] = dim_data[nnz_offset_];
      }
    }
  }

  /// @brief Search for element with given multi-dimensional indices
  /// @param index Multi-dimensional index array
  /// @return True if element found, false otherwise
  constexpr bool SetMultiIndexImpl(const int* index) {
    const int nnz = GetNNZ();

    if constexpr (std::is_same<Layout, SparseLayoutRight>::value) {
      // CSR format: search in row
      const auto& row_ptr = map_.GetRowPtr();
      const auto& col_indices = map_.GetColIndices();
      const int* row_ptr_data = row_ptr.HostRead();
      const int* col_data = col_indices.HostRead();

      const int row = index[0];
      const int col = index[1];

      for (int k = row_ptr_data[row]; k < row_ptr_data[row + 1]; ++k) {
        if (col_data[k] == col) {
          nnz_offset_ = k;
          multi_index_[0] = row;
          multi_index_[1] = col;
          return true;
        }
      }
      return false;

    } else if constexpr (std::is_same<Layout, SparseLayoutLeft>::value) {
      // CSC format: search in column
      const auto& col_ptr = map_.GetColPtr();
      const auto& row_indices = map_.GetRowIndices();
      const int* col_ptr_data = col_ptr.HostRead();
      const int* row_data = row_indices.HostRead();

      const int row = index[0];
      const int col = index[1];

      for (int k = col_ptr_data[col]; k < col_ptr_data[col + 1]; ++k) {
        if (row_data[k] == row) {
          nnz_offset_ = k;
          multi_index_[0] = row;
          multi_index_[1] = col;
          return true;
        }
      }
      return false;

    } else if constexpr (std::is_same<Layout, SparseLayoutStride>::value) {
      // COO format: linear search
      const int rank = GetRank();

      for (int k = 0; k < nnz; ++k) {
        bool match = true;
        for (int d = 0; d < rank; ++d) {
          const auto& dim_indices = map_.GetIndices(d);
          const int* dim_data = dim_indices.HostRead();
          if (dim_data[k] != index[d]) {
            match = false;
            break;
          }
        }
        if (match) {
          nnz_offset_ = k;
          for (int d = 0; d < rank; ++d) {
            multi_index_[d] = index[d];
          }
          return true;
        }
      }
      return false;
    }
  }
};

}  // namespace asc

#endif  // ASC_SPMINDEX_H_
