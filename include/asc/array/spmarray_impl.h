// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/spmarray_impl.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_GENERIC_SPMARRAY_IMPL_H_
#define ASC_GENERIC_SPMARRAY_IMPL_H_

/// @file spmarray_impl.h
/// @brief Implementation of SparseMArray methods
///
/// This file contains the implementations of SparseMArray member functions.
/// It should be included at the end of spmarray.h.

#include <algorithm>
#include <unordered_map>
#include <vector>

#include "asc/array/spmarray.h"

namespace asc {

namespace sparse_detail {

struct IndexVectorHash {
  std::size_t operator()(const std::vector<int>& key) const {
    std::size_t seed = key.size();
    for (int value : key) {
      const std::size_t x = static_cast<std::size_t>(value);
      seed ^= x + 0x9e3779b9u + (seed << 6) + (seed >> 2);
    }
    return seed;
  }
};

}  // namespace sparse_detail

// ==========================================================================
// Constructor Implementations
// ==========================================================================

template <typename T, typename Shape, typename Layout>
template <Integral... Extents>
SparseMArray<T, Shape, Layout>::SparseMArray(Extents... extents)
    : map_(Shape(extents...)) {
  static_assert(sizeof...(Extents) == Shape::GetRank(),
                "Number of extents must match shape rank");
  InitializeEmptyCompressedStorage();
}

template <typename T, typename Shape, typename Layout>
template <Integral... Extents>
SparseMArray<T, Shape, Layout>::SparseMArray(MemoryType mt, Extents... extents)
    : map_(Shape(extents...)) {
  static_assert(sizeof...(Extents) == Shape::GetRank(),
                "Number of extents must match shape rank");
  InitializeEmptyCompressedStorage();
  values_ = UArray<T>(mt);
}

// ==========================================================================
// Sparse-Specific Interface Implementation
// ==========================================================================

template <typename T, typename Shape, typename Layout>
void SparseMArray<T, Shape, Layout>::Reserve(int capacity) {
  values_.Reserve(capacity);
  map_.Reserve(capacity);
}

template <typename T, typename Shape, typename Layout>
void SparseMArray<T, Shape, Layout>::Clear() {
  values_.DeleteAll();
  map_.Clear();
}

template <typename T, typename Shape, typename Layout>
void SparseMArray<T, Shape, Layout>::Insert(const UArray<int>& indices,
                                            const T& value) {
  const int rank = GetRank();
  ASC_VERIFY(indices.GetSize() == rank,
                "Indices size must match array rank");

  // Validate indices
  const int* idx_data = indices.HostRead();
  for (int d = 0; d < rank; ++d) {
    ASC_VERIFY(idx_data[d] >= 0 && idx_data[d] < GetExtent(d),
                  "Index out of bounds");
  }

  // Only SparseLayoutStride (COO) supports direct insertion
  if constexpr (std::is_same<Layout, SparseLayoutStride>::value) {
    const int nnz = map_.GetNNZ();
    for (int d = 0; d < rank; ++d) {
      map_.GetIndices(d).Append(idx_data[d]);
    }
    values_.Append(value);
    map_.SetNNZ(nnz + 1);
  } else {
    ASC_VERIFY(
        false,
        "Direct insertion only supported for SparseLayoutStride (COO). "
        "Use SetFromTuples() or convert from COO format.");
  }
}

template <typename T, typename Shape, typename Layout>
void SparseMArray<T, Shape, Layout>::Insert(int row, int col, const T& value) {
  ASC_VERIFY(GetRank() == 2, "2D Insert() requires rank == 2");
  UArray<int> indices({row, col});
  Insert(indices, value);
}

template <typename T, typename Shape, typename Layout>
template <typename Iterator>
void SparseMArray<T, Shape, Layout>::SetFromTuples(Iterator begin,
                                                   Iterator end) {
  // Clear existing data
  Clear();

  // Count tuples
  const int num_tuples = std::distance(begin, end);
  if (num_tuples == 0) {
    if constexpr (std::is_same<Layout, SparseLayoutStride>::value) {
      map_.SetNNZ(0);
    } else {
      const int rank = GetRank();
      const int outer_extent =
          std::is_same<Layout, SparseLayoutRight>::value
              ? GetExtent(0)
              : GetExtent(rank - 1);
      auto& outer_ptr = map_.GetOuterPtr();
      outer_ptr.SetSize(outer_extent + 1);
      for (int i = 0; i <= outer_extent; ++i) {
        outer_ptr[i] = 0;
      }
      if constexpr (std::is_same<Layout, SparseLayoutRight>::value) {
        for (int d = 1; d < rank; ++d) {
          map_.GetInnerIndices(d - 1).SetSize(0);
        }
      } else {
        for (int d = 0; d < rank - 1; ++d) {
          map_.GetInnerIndices(d).SetSize(0);
        }
      }
      map_.SetNNZ(0);
      map_.SetCompressed(true);
    }
    return;
  }

  const int rank = GetRank();

  // For COO format, directly append tuples
  if constexpr (std::is_same<Layout, SparseLayoutStride>::value) {
    Reserve(num_tuples);
    for (auto it = begin; it != end; ++it) {
      ASC_VERIFY(it->indices.GetSize() == rank,
                    "Tuple indices size must match array rank");
      const int* idx_data = it->indices.HostRead();
      for (int d = 0; d < rank; ++d) {
        map_.GetIndices(d).Append(idx_data[d]);
      }
      values_.Append(it->value);
    }
    map_.SetNNZ(num_tuples);
  } else {
    // For compressed formats, convert from tuples
    std::vector<SparseTuple<T>> tuples(begin, end);
    std::sort(tuples.begin(), tuples.end());

    // Allocate storage
    Reserve(num_tuples);
    values_.SetSize(num_tuples);

    // Build compressed format
    if constexpr (std::is_same<Layout, SparseLayoutRight>::value) {
      // Compress first dimension
      const int outer_extent = GetExtent(0);
      auto& outer_ptr = map_.GetOuterPtr();
      outer_ptr.SetSize(outer_extent + 1);

      // Count non-zeros per outer index
      for (int i = 0; i <= outer_extent; ++i) {
        outer_ptr[i] = 0;
      }
      for (const auto& t : tuples) {
        outer_ptr[t.indices[0] + 1]++;
      }

      // Cumulative sum
      for (int i = 1; i <= outer_extent; ++i) {
        outer_ptr[i] += outer_ptr[i - 1];
      }

      // Fill inner indices and values
      for (int d = 1; d < rank; ++d) {
        map_.GetInnerIndices(d - 1).SetSize(num_tuples);
      }

      for (const auto& t : tuples) {
        const int idx = outer_ptr[t.indices[0]]++;
        for (int d = 1; d < rank; ++d) {
          map_.GetInnerIndices(d - 1)[idx] = t.indices[d];
        }
        values_[idx] = t.value;
      }

      // Restore outer pointers
      for (int i = outer_extent; i > 0; --i) {
        outer_ptr[i] = outer_ptr[i - 1];
      }
      outer_ptr[0] = 0;

    } else if constexpr (std::is_same<Layout, SparseLayoutLeft>::value) {
      // Compress last dimension - sort by last index first
      std::sort(tuples.begin(), tuples.end(),
                [rank](const SparseTuple<T>& a, const SparseTuple<T>& b) {
                  // Compare last dimension first, then rest
                  if (a.indices[rank - 1] != b.indices[rank - 1]) {
                    return a.indices[rank - 1] < b.indices[rank - 1];
                  }
                  // UArray has implicit pointer conversions, so comparing the
                  // containers directly would compare allocation addresses.
                  // Compare the remaining coordinates explicitly to keep each
                  // CSC column in canonical, increasing index order.
                  for (int d = 0; d < rank - 1; ++d) {
                    if (a.indices[d] != b.indices[d]) {
                      return a.indices[d] < b.indices[d];
                    }
                  }
                  return false;
                });

      const int outer_extent = GetExtent(rank - 1);
      auto& outer_ptr = map_.GetOuterPtr();
      outer_ptr.SetSize(outer_extent + 1);

      // Count non-zeros per outer index (last dimension)
      for (int i = 0; i <= outer_extent; ++i) {
        outer_ptr[i] = 0;
      }
      for (const auto& t : tuples) {
        outer_ptr[t.indices[rank - 1] + 1]++;
      }

      // Cumulative sum
      for (int i = 1; i <= outer_extent; ++i) {
        outer_ptr[i] += outer_ptr[i - 1];
      }

      // Fill inner indices (first rank-1 dimensions) and values
      for (int d = 0; d < rank - 1; ++d) {
        map_.GetInnerIndices(d).SetSize(num_tuples);
      }

      for (const auto& t : tuples) {
        const int idx = outer_ptr[t.indices[rank - 1]]++;
        for (int d = 0; d < rank - 1; ++d) {
          map_.GetInnerIndices(d)[idx] = t.indices[d];
        }
        values_[idx] = t.value;
      }

      // Restore outer pointers
      for (int i = outer_extent; i > 0; --i) {
        outer_ptr[i] = outer_ptr[i - 1];
      }
      outer_ptr[0] = 0;
    }

    map_.SetNNZ(num_tuples);
    map_.SetCompressed(true);
  }
}

template <typename T, typename Shape, typename Layout>
void SparseMArray<T, Shape, Layout>::Finalize() {
  if constexpr (std::is_same<Layout, SparseLayoutStride>::value) {
    // Sort COO tuples
    const int nnz = GetNNZ();
    if (nnz == 0) return;

    const int rank = GetRank();

    // Create index array for sorting
    UArray<int> sort_indices(nnz);
    int* sort_data = sort_indices.HostWrite();
    for (int i = 0; i < nnz; ++i) {
      sort_data[i] = i;
    }

    // Get references to index arrays
    std::vector<const UArray<int>*> index_refs(rank);
    for (int d = 0; d < rank; ++d) {
      index_refs[d] = &map_.GetIndices(d);
    }

    // Sort indices based on lexicographic ordering of all dimensions
    std::sort(sort_data, sort_data + nnz, [&](int i, int j) {
      for (int d = 0; d < rank; ++d) {
        const int idx_i = (*index_refs[d])[i];
        const int idx_j = (*index_refs[d])[j];
        if (idx_i != idx_j) {
          return idx_i < idx_j;
        }
      }
      return false;  // Equal indices
    });

    // Reorder all arrays
    std::vector<UArray<int>> new_indices(rank);
    for (int d = 0; d < rank; ++d) {
      new_indices[d].SetSize(nnz);
    }
    UArray<T> new_values(nnz);

    const int* sort_read = sort_indices.HostRead();
    for (int i = 0; i < nnz; ++i) {
      const int old_idx = sort_read[i];
      for (int d = 0; d < rank; ++d) {
        new_indices[d][i] = map_.GetIndices(d)[old_idx];
      }
      new_values[i] = values_[old_idx];
    }

    // Sum duplicates
    std::vector<UArray<int>> merged_indices(rank);
    UArray<T> merged_values;

    int write_pos = 0;
    for (int read_pos = 0; read_pos < nnz; ++read_pos) {
      // Check if this entry has same indices as previous entry
      bool is_duplicate = false;
      if (read_pos > 0) {
        is_duplicate = true;
        for (int d = 0; d < rank; ++d) {
          if (new_indices[d][read_pos] != new_indices[d][read_pos - 1]) {
            is_duplicate = false;
            break;
          }
        }
      }

      if (is_duplicate) {
        // Sum with previous entry
        merged_values[write_pos - 1] += new_values[read_pos];
      } else {
        // New unique entry
        if (write_pos >= merged_values.GetSize()) {
          // Resize arrays if needed
          for (int d = 0; d < rank; ++d) {
            merged_indices[d].SetSize(write_pos + 1);
          }
          merged_values.SetSize(write_pos + 1);
        }
        for (int d = 0; d < rank; ++d) {
          merged_indices[d][write_pos] = new_indices[d][read_pos];
        }
        merged_values[write_pos] = new_values[read_pos];
        write_pos++;
      }
    }

    // Remove zeros
    int final_pos = 0;
    for (int i = 0; i < write_pos; ++i) {
      if (merged_values[i] != T(0)) {
        if (final_pos != i) {
          for (int d = 0; d < rank; ++d) {
            merged_indices[d][final_pos] = merged_indices[d][i];
          }
          merged_values[final_pos] = merged_values[i];
        }
        final_pos++;
      }
    }

    // Resize to final size
    for (int d = 0; d < rank; ++d) {
      merged_indices[d].SetSize(final_pos);
    }
    merged_values.SetSize(final_pos);

    // Update map and values
    for (int d = 0; d < rank; ++d) {
      map_.GetIndices(d) = merged_indices[d];
    }
    values_ = merged_values;
    map_.SetNNZ(final_pos);

    map_.SetCompressed(true);
  }
}

template <typename T, typename Shape, typename Layout>
void SparseMArray<T, Shape, Layout>::Compress() {
  Finalize();
}

// ==========================================================================
// Format Conversion Implementation
// ==========================================================================

template <typename T, typename Shape, typename Layout>
template <typename TargetLayout>
SparseMArray<T, Shape, TargetLayout> SparseMArray<T, Shape, Layout>::ToLayout()
    const {
  const int rank = GetRank();
  const int nnz = GetNNZ();

  // Create COO tuples from current format
  std::vector<SparseTuple<T>> tuples;
  tuples.reserve(nnz);

  if constexpr (std::is_same_v<Layout, SparseLayoutStride>) {
    // From COO - extract indices
    for (int k = 0; k < nnz; ++k) {
      UArray<int> indices(rank);
      int* idx_data = indices.HostWrite();
      for (int d = 0; d < rank; ++d) {
        idx_data[d] = map_.GetIndices(d)[k];
      }
      tuples.emplace_back(indices, values_[k]);
    }
  } else if constexpr (std::is_same_v<Layout, SparseLayoutRight>) {
    // From compressed first dimension
    const auto& outer_ptr = map_.GetOuterPtr();
    const int outer_extent = GetExtent(0);

    for (int i = 0; i < outer_extent; ++i) {
      for (int k = outer_ptr[i]; k < outer_ptr[i + 1]; ++k) {
        UArray<int> indices(rank);
        int* idx_data = indices.HostWrite();
        idx_data[0] = i;
        for (int d = 1; d < rank; ++d) {
          idx_data[d] = map_.GetInnerIndices(d - 1)[k];
        }
        tuples.emplace_back(indices, values_[k]);
      }
    }
  } else if constexpr (std::is_same_v<Layout, SparseLayoutLeft>) {
    // From compressed last dimension
    const auto& outer_ptr = map_.GetOuterPtr();
    const int outer_extent = GetExtent(rank - 1);

    for (int j = 0; j < outer_extent; ++j) {
      for (int k = outer_ptr[j]; k < outer_ptr[j + 1]; ++k) {
        UArray<int> indices(rank);
        int* idx_data = indices.HostWrite();
        for (int d = 0; d < rank - 1; ++d) {
          idx_data[d] = map_.GetInnerIndices(d)[k];
        }
        idx_data[rank - 1] = j;
        tuples.emplace_back(indices, values_[k]);
      }
    }
  }

  // Build target format
  SparseMArray<T, Shape, TargetLayout> result(GetShape());
  result.SetFromTuples(tuples.begin(), tuples.end());
  return result;
}

// ==========================================================================
// Sparse Operations Implementation
// ==========================================================================

template <typename T, typename Shape, typename Layout>
SparseMArray<T, Shape, typename Layout::TransposeLayout>
SparseMArray<T, Shape, Layout>::Transpose() const {
  ASC_VERIFY(GetRank() == 2, "Transpose only supports 2D matrices");

  const int nrows = GetExtent(0);
  const int ncols = GetExtent(1);
  const int nnz = GetNNZ();

  // Create transposed shape (swap dimensions)
  Shape transposed_shape(ncols, nrows);

  // Convert to COO, swap indices, then convert to target layout
  std::vector<SparseTuple<T>> tuples;
  tuples.reserve(nnz);

  if constexpr (std::is_same<Layout, SparseLayoutStride>::value) {
    // COO format: directly swap row and column indices
    const auto& row_indices = map_.GetIndices(0);
    const auto& col_indices = map_.GetIndices(1);

    for (int k = 0; k < nnz; ++k) {
      // Swap: (i,j) → (j,i)
      UArray<int> indices(2);
      indices[0] = col_indices[k];
      indices[1] = row_indices[k];
      tuples.emplace_back(indices, values_[k]);
    }
  } else if constexpr (std::is_same<Layout, SparseLayoutRight>::value) {
    // CSR format
    const auto& row_ptr = map_.GetRowPtr();
    const auto& col_indices = map_.GetColIndices();

    for (int i = 0; i < nrows; ++i) {
      for (int k = row_ptr[i]; k < row_ptr[i + 1]; ++k) {
        const int j = col_indices[k];
        // Swap: (i,j) → (j,i)
        UArray<int> indices(2);
        indices[0] = j;
        indices[1] = i;
        tuples.emplace_back(indices, values_[k]);
      }
    }
  } else if constexpr (std::is_same<Layout, SparseLayoutLeft>::value) {
    // CSC format
    const auto& col_ptr = map_.GetColPtr();
    const auto& row_indices = map_.GetRowIndices();

    for (int j = 0; j < ncols; ++j) {
      for (int k = col_ptr[j]; k < col_ptr[j + 1]; ++k) {
        const int i = row_indices[k];
        // Swap: (i,j) → (j,i)
        UArray<int> indices(2);
        indices[0] = j;
        indices[1] = i;
        tuples.emplace_back(indices, values_[k]);
      }
    }
  }

  // Build result in transposed layout
  SparseMArray<T, Shape, typename Layout::TransposeLayout> result(
      transposed_shape);
  result.SetFromTuples(tuples.begin(), tuples.end());
  return result;
}

template <typename T, typename Shape, typename Layout>
SparseMArray<T, DShape<2>, SparseLayoutStride>
SparseMArray<T, Shape, Layout>::GetOuter(int idx) const {
  ASC_VERIFY(GetRank() == 2, "GetOuter only supports 2D matrices");

  if constexpr (std::is_same<Layout, SparseLayoutRight>::value) {
    // CSR format - extract row (outer dimension = rows)
    ASC_VERIFY(idx >= 0 && idx < GetExtent(0), "Row index out of bounds");

    const int ncols = GetExtent(1);
    DShape<2> vec_shape({1, ncols});  // Row vector shape
    SparseMArray<T, DShape<2>, SparseLayoutStride> result(vec_shape);

    const auto& row_ptr = map_.GetRowPtr();
    const auto& col_indices = map_.GetColIndices();
    const int start = row_ptr[idx];
    const int end = row_ptr[idx + 1];
    const int row_nnz = end - start;

    if (row_nnz > 0) {
      result.Reserve(row_nnz);
      for (int k = start; k < end; ++k) {
        const int col = col_indices[k];
        result.Insert(0, col, values_[k]);
      }
      result.Finalize();
    }

    return result;
  } else if constexpr (std::is_same<Layout, SparseLayoutLeft>::value) {
    // CSC format - extract column (outer dimension = columns)
    ASC_VERIFY(idx >= 0 && idx < GetExtent(1),
                  "Column index out of bounds");

    const int nrows = GetExtent(0);
    DShape<2> vec_shape({nrows, 1});  // Column vector shape
    SparseMArray<T, DShape<2>, SparseLayoutStride> result(vec_shape);

    const auto& col_ptr = map_.GetColPtr();
    const auto& row_indices = map_.GetRowIndices();
    const int start = col_ptr[idx];
    const int end = col_ptr[idx + 1];
    const int col_nnz = end - start;

    if (col_nnz > 0) {
      result.Reserve(col_nnz);
      for (int k = start; k < end; ++k) {
        const int row = row_indices[k];
        result.Insert(row, 0, values_[k]);
      }
      result.Finalize();
    }

    return result;
  } else {
    // COO format - extract row by default
    ASC_VERIFY(idx >= 0 && idx < GetExtent(0), "Row index out of bounds");

    const int ncols = GetExtent(1);
    DShape<2> vec_shape({1, ncols});
    SparseMArray<T, DShape<2>, SparseLayoutStride> result(vec_shape);

    const auto& row_indices = map_.GetIndices(0);
    const auto& col_indices = map_.GetIndices(1);
    const int nnz = GetNNZ();

    for (int k = 0; k < nnz; ++k) {
      if (row_indices[k] == idx) {
        result.Insert(0, col_indices[k], values_[k]);
      }
    }
    result.Finalize();

    return result;
  }
}

template <typename T, typename Shape, typename Layout>
SparseMArray<T, Shape, SparseLayoutStride>
SparseMArray<T, Shape, Layout>::SliceOuter(int start, int end) const {
  const int rank = GetRank();

  if constexpr (std::is_same<Layout, SparseLayoutRight>::value) {
    // CSR format - slice rows
    ASC_VERIFY(start >= 0 && start <= GetExtent(0),
                  "Start index out of bounds");
    ASC_VERIFY(end >= start && end <= GetExtent(0),
                  "End index out of bounds");

    // Create new shape with sliced outer dimension
    Shape new_shape = GetShape();
    if constexpr (Shape::IsDynamic()) {
      std::vector<int> new_extents(rank);
      for (int d = 0; d < rank; ++d) {
        new_extents[d] = GetExtent(d);
      }
      new_extents[0] = end - start;
      new_shape = Shape(new_extents.data());
    } else {
      // Static shape - reconstruct with new first dimension
      new_shape = GetShape();
      // Note: For static shapes, this is a limitation
    }

    SparseMArray<T, Shape, SparseLayoutStride> result(new_shape);

    const auto& row_ptr = map_.GetRowPtr();
    const auto& inner_indices = map_.GetInnerIndices();
    const T* val_data = values_.HostRead();

    // Extract elements from rows [start, end)
    for (int i = start; i < end; ++i) {
      for (int k = row_ptr[i]; k < row_ptr[i + 1]; ++k) {
        UArray<int> indices(rank);
        int* idx_data = indices.HostWrite();
        idx_data[0] = i - start;  // Adjust row index
        for (int d = 1; d < rank; ++d) {
          idx_data[d] = inner_indices[d - 1][k];
        }
        result.Insert(indices, val_data[k]);
      }
    }
    result.Finalize();
    return result;

  } else if constexpr (std::is_same<Layout, SparseLayoutLeft>::value) {
    // CSC format - slice columns
    ASC_VERIFY(start >= 0 && start <= GetExtent(rank - 1),
                  "Start index out of bounds");
    ASC_VERIFY(end >= start && end <= GetExtent(rank - 1),
                  "End index out of bounds");

    // Create new shape with sliced outer dimension (last dimension)
    Shape new_shape = GetShape();
    if constexpr (Shape::IsDynamic()) {
      std::vector<int> new_extents(rank);
      for (int d = 0; d < rank; ++d) {
        new_extents[d] = GetExtent(d);
      }
      new_extents[rank - 1] = end - start;
      new_shape = Shape(new_extents.data());
    }

    SparseMArray<T, Shape, SparseLayoutStride> result(new_shape);

    const auto& col_ptr = map_.GetOuterPtr();
    const auto& inner_indices = map_.GetInnerIndices();
    const T* val_data = values_.HostRead();

    // Extract elements from columns [start, end)
    for (int j = start; j < end; ++j) {
      for (int k = col_ptr[j]; k < col_ptr[j + 1]; ++k) {
        UArray<int> indices(rank);
        int* idx_data = indices.HostWrite();
        for (int d = 0; d < rank - 1; ++d) {
          idx_data[d] = inner_indices[d][k];
        }
        idx_data[rank - 1] = j - start;  // Adjust column index
        result.Insert(indices, val_data[k]);
      }
    }
    result.Finalize();
    return result;

  } else {
    // COO format - slice first dimension by default
    ASC_VERIFY(start >= 0 && start <= GetExtent(0),
                  "Start index out of bounds");
    ASC_VERIFY(end >= start && end <= GetExtent(0),
                  "End index out of bounds");

    Shape new_shape = GetShape();
    if constexpr (Shape::IsDynamic()) {
      std::vector<int> new_extents(rank);
      for (int d = 0; d < rank; ++d) {
        new_extents[d] = GetExtent(d);
      }
      new_extents[0] = end - start;
      new_shape = Shape(new_extents.data());
    }

    SparseMArray<T, Shape, SparseLayoutStride> result(new_shape);

    const auto& indices_arrays = map_.GetIndices();
    const T* val_data = values_.HostRead();
    const int nnz = GetNNZ();

    // Filter elements by first dimension
    for (int k = 0; k < nnz; ++k) {
      const int idx0 = indices_arrays[0][k];
      if (idx0 >= start && idx0 < end) {
        UArray<int> indices(rank);
        int* idx_data = indices.HostWrite();
        idx_data[0] = idx0 - start;  // Adjust index
        for (int d = 1; d < rank; ++d) {
          idx_data[d] = indices_arrays[d][k];
        }
        result.Insert(indices, val_data[k]);
      }
    }
    result.Finalize();
    return result;
  }
}

template <typename T, typename Shape, typename Layout>
SparseMArray<T, Shape, SparseLayoutStride>
SparseMArray<T, Shape, Layout>::Slice(const UArray<int>& ranges) const {
  const int rank = GetRank();
  ASC_VERIFY(ranges.GetSize() == 2 * rank,
                "Ranges must contain start and end for each dimension");

  const int* range_data = ranges.HostRead();

  // Build new shape and validate ranges
  std::vector<int> new_extents(rank);
  for (int d = 0; d < rank; ++d) {
    const int start = range_data[2 * d];
    const int end = range_data[2 * d + 1];
    ASC_VERIFY(start >= 0 && start <= GetExtent(d),
                  "Start index out of bounds for dimension");
    ASC_VERIFY(end >= start && end <= GetExtent(d),
                  "End index out of bounds for dimension");
    new_extents[d] = end - start;
  }

  Shape new_shape;
  if constexpr (Shape::IsDynamic()) {
    new_shape = Shape(new_extents.data());
  } else {
    new_shape = GetShape();
  }

  SparseMArray<T, Shape, SparseLayoutStride> result(new_shape);

  // Convert to COO for easy filtering
  auto coo = ToLayout<SparseLayoutStride>();
  const int nnz = coo.GetNNZ();
  const T* val_data = coo.GetValues().HostRead();

  // Filter elements that fall within all dimension ranges
  for (int k = 0; k < nnz; ++k) {
    bool in_range = true;
    UArray<int> new_indices(rank);
    int* new_idx_data = new_indices.HostWrite();

    for (int d = 0; d < rank; ++d) {
      const int idx = coo.GetMap().GetIndices(d)[k];
      const int start = range_data[2 * d];
      const int end = range_data[2 * d + 1];

      if (idx < start || idx >= end) {
        in_range = false;
        break;
      }
      new_idx_data[d] = idx - start;  // Adjust index to new coordinate system
    }

    if (in_range) {
      result.Insert(new_indices, val_data[k]);
    }
  }

  result.Finalize();
  return result;
}

template <typename T, typename Shape, typename Layout>
UArray<T> SparseMArray<T, Shape, Layout>::GetDiagonal() const {
  ASC_VERIFY(GetRank() == 2, "GetDiagonal only supports 2D matrices");

  const int nrows = GetExtent(0);
  const int ncols = GetExtent(1);
  const int diag_size = (nrows < ncols) ? nrows : ncols;

  UArray<T> diag(diag_size);
  T* diag_data = diag.HostWrite();

  // Initialize to zero
  for (int i = 0; i < diag_size; ++i) {
    diag_data[i] = T(0);
  }

  const int nnz = GetNNZ();

  if constexpr (std::is_same<Layout, SparseLayoutRight>::value) {
    // CSR format
    const auto& row_ptr = map_.GetRowPtr();
    const auto& col_indices = map_.GetColIndices();
    const T* val_data = values_.HostRead();

    for (int i = 0; i < diag_size; ++i) {
      for (int k = row_ptr[i]; k < row_ptr[i + 1]; ++k) {
        if (col_indices[k] == i) {
          diag_data[i] = val_data[k];
          break;
        }
      }
    }
  } else if constexpr (std::is_same<Layout, SparseLayoutLeft>::value) {
    // CSC format
    const auto& col_ptr = map_.GetColPtr();
    const auto& row_indices = map_.GetRowIndices();
    const T* val_data = values_.HostRead();

    for (int j = 0; j < diag_size; ++j) {
      for (int k = col_ptr[j]; k < col_ptr[j + 1]; ++k) {
        if (row_indices[k] == j) {
          diag_data[j] = val_data[k];
          break;
        }
      }
    }
  } else if constexpr (std::is_same<Layout, SparseLayoutStride>::value) {
    // COO format
    const auto& row_indices = map_.GetIndices(0);
    const auto& col_indices = map_.GetIndices(1);
    const T* val_data = values_.HostRead();

    for (int k = 0; k < nnz; ++k) {
      const int i = row_indices[k];
      const int j = col_indices[k];
      if (i == j && i < diag_size) {
        diag_data[i] = val_data[k];
      }
    }
  }

  return diag;
}

template <typename T, typename Shape, typename Layout>
void SparseMArray<T, Shape, Layout>::SetDiagonal(const UArray<T>& diag) {
  ASC_VERIFY(GetRank() == 2, "SetDiagonal only supports 2D matrices");

  const int nrows = GetExtent(0);
  const int ncols = GetExtent(1);
  const int diag_size = (nrows < ncols) ? nrows : ncols;

  ASC_VERIFY(diag.GetSize() == diag_size,
                "Diagonal size must match min(nrows, ncols)");

  const T* diag_data = diag.HostRead();
  const int nnz = GetNNZ();

  if constexpr (std::is_same<Layout, SparseLayoutRight>::value) {
    // CSR format
    const auto& row_ptr = map_.GetRowPtr();
    const auto& col_indices = map_.GetColIndices();
    T* val_data = values_.HostWrite();

    for (int i = 0; i < diag_size; ++i) {
      bool found = false;
      for (int k = row_ptr[i]; k < row_ptr[i + 1]; ++k) {
        if (col_indices[k] == i) {
          val_data[k] = diag_data[i];
          found = true;
          break;
        }
      }
      ASC_VERIFY(found || diag_data[i] == T(0),
                    "Cannot set non-zero diagonal element in non-existing "
                    "sparse position");
    }
  } else if constexpr (std::is_same<Layout, SparseLayoutLeft>::value) {
    // CSC format
    const auto& col_ptr = map_.GetColPtr();
    const auto& row_indices = map_.GetRowIndices();
    T* val_data = values_.HostWrite();

    for (int j = 0; j < diag_size; ++j) {
      bool found = false;
      for (int k = col_ptr[j]; k < col_ptr[j + 1]; ++k) {
        if (row_indices[k] == j) {
          val_data[k] = diag_data[j];
          found = true;
          break;
        }
      }
      ASC_VERIFY(found || diag_data[j] == T(0),
                    "Cannot set non-zero diagonal element in non-existing "
                    "sparse position");
    }
  } else if constexpr (std::is_same<Layout, SparseLayoutStride>::value) {
    // COO format
    const auto& row_indices = map_.GetIndices(0);
    const auto& col_indices = map_.GetIndices(1);
    T* val_data = values_.HostWrite();

    UArray<bool> found(diag_size);
    bool* found_data = found.HostWrite();
    for (int i = 0; i < diag_size; ++i) {
      found_data[i] = false;
    }

    for (int k = 0; k < nnz; ++k) {
      const int i = row_indices[k];
      const int j = col_indices[k];
      if (i == j && i < diag_size) {
        val_data[k] = diag_data[i];
        found_data[i] = true;
      }
    }

    const bool* found_read = found.HostRead();
    for (int i = 0; i < diag_size; ++i) {
      ASC_VERIFY(found_read[i] || diag_data[i] == T(0),
                    "Cannot set non-zero diagonal element in non-existing "
                    "sparse position");
    }
  }
}

// ==========================================================================
// Element Access Implementation
// ==========================================================================

template <typename T, typename Shape, typename Layout>
int SparseMArray<T, Shape, Layout>::FindNNZOffset(const int* indices) const {
  ASC_VERIFY(indices != nullptr, "Sparse index pointer must not be null");
  for (int d = 0; d < GetRank(); ++d) {
    ASC_VERIFY(indices[d] >= 0 && indices[d] < GetExtent(d),
                  "Index out of bounds");
  }

  const int nnz = GetNNZ();
  if (nnz == 0) {
    return -1;
  }

  constexpr int kRank = Shape::GetRank();

  if constexpr (std::is_same<Layout, SparseLayoutRight>::value) {
    const int* outer_ptr = map_.GetOuterPtr().HostRead();
    const int* inner_indices[kRank - 1];
    for (int d = 1; d < kRank; ++d) {
      inner_indices[d - 1] = map_.GetInnerIndices(d - 1).HostRead();
    }

    const int outer = indices[0];
    for (int k = outer_ptr[outer]; k < outer_ptr[outer + 1]; ++k) {
      bool match = true;
      for (int d = 1; d < kRank; ++d) {
        if (inner_indices[d - 1][k] != indices[d]) {
          match = false;
          break;
        }
      }
      if (match) return k;
    }
    return -1;

  } else if constexpr (std::is_same<Layout, SparseLayoutLeft>::value) {
    const int* outer_ptr = map_.GetOuterPtr().HostRead();
    const int* inner_indices[kRank - 1];
    for (int d = 0; d < kRank - 1; ++d) {
      inner_indices[d] = map_.GetInnerIndices(d).HostRead();
    }

    const int outer = indices[kRank - 1];
    for (int k = outer_ptr[outer]; k < outer_ptr[outer + 1]; ++k) {
      bool match = true;
      for (int d = 0; d < kRank - 1; ++d) {
        if (inner_indices[d][k] != indices[d]) {
          match = false;
          break;
        }
      }
      if (match) return k;
    }
    return -1;

  } else if constexpr (std::is_same<Layout, SparseLayoutStride>::value) {
    const int* dim_indices[kRank];
    for (int d = 0; d < kRank; ++d) {
      dim_indices[d] = map_.GetIndices(d).HostRead();
    }

    for (int k = 0; k < nnz; ++k) {
      bool match = true;
      for (int d = 0; d < kRank; ++d) {
        if (dim_indices[d][k] != indices[d]) {
          match = false;
          break;
        }
      }
      if (match) return k;
    }
    return -1;
  }
}

template <typename T, typename Shape, typename Layout>
T SparseMArray<T, Shape, Layout>::At(const UArray<int>& indices) const {
  ASC_VERIFY(indices.GetSize() == GetRank(),
                "Number of indices must match array rank");
  return At(indices.HostRead());
}

template <typename T, typename Shape, typename Layout>
T SparseMArray<T, Shape, Layout>::At(const int* indices) const {
  const int offset = FindNNZOffset(indices);
  if (offset < 0) return T(0);
  return values_.HostRead()[offset];
}

template <typename T, typename Shape, typename Layout>
template <Integral... Indices>
T SparseMArray<T, Shape, Layout>::At(Indices... indices) const {
  constexpr int num_indices = sizeof...(Indices);
  ASC_STATIC_ASSERT(num_indices == GetRank(),
                       "Number of indices must match array rank");

  int index_array[GetRank()] = {static_cast<int>(indices)...};
  return At(index_array);
}

template <typename T, typename Shape, typename Layout>
bool SparseMArray<T, Shape, Layout>::Contains(const UArray<int>& indices) const {
  ASC_VERIFY(indices.GetSize() == GetRank(),
                "Number of indices must match array rank");
  return Contains(indices.HostRead());
}

template <typename T, typename Shape, typename Layout>
bool SparseMArray<T, Shape, Layout>::Contains(const int* indices) const {
  return FindNNZOffset(indices) >= 0;
}

template <typename T, typename Shape, typename Layout>
template <Integral... Indices>
bool SparseMArray<T, Shape, Layout>::Contains(Indices... indices) const {
  constexpr int num_indices = sizeof...(Indices);
  ASC_STATIC_ASSERT(num_indices == GetRank(),
                       "Number of indices must match array rank");

  int index_array[GetRank()] = {static_cast<int>(indices)...};
  return Contains(index_array);
}

// ==========================================================================
// Reduction Operations Implementation
// ==========================================================================

template <typename T, typename Shape, typename Layout>
T SparseMArray<T, Shape, Layout>::Sum() const {
  const int nnz = GetNNZ();
  if (nnz == 0) {
    return T(0);
  }

  const T* val_data = values_.HostRead();

  T sum = T(0);
  for (int k = 0; k < nnz; ++k) {
    sum += val_data[k];
  }
  return sum;
}

template <typename T, typename Shape, typename Layout>
T SparseMArray<T, Shape, Layout>::Max() const {
  const int nnz = GetNNZ();
  ASC_VERIFY(nnz > 0, "Cannot compute max of empty sparse array");

  const T* val_data = values_.HostRead();

  T max_val = std::abs(val_data[0]);
  for (int k = 1; k < nnz; ++k) {
    const T abs_val = std::abs(val_data[k]);
    if (abs_val > max_val) {
      max_val = abs_val;
    }
  }
  return max_val;
}

template <typename T, typename Shape, typename Layout>
T SparseMArray<T, Shape, Layout>::Min() const {
  const int nnz = GetNNZ();
  ASC_VERIFY(nnz > 0, "Cannot compute min of empty sparse array");

  const T* val_data = values_.HostRead();

  T min_val = std::abs(val_data[0]);
  for (int k = 1; k < nnz; ++k) {
    const T abs_val = std::abs(val_data[k]);
    if (abs_val < min_val) {
      min_val = abs_val;
    }
  }
  return min_val;
}

template <typename T, typename Shape, typename Layout>
T SparseMArray<T, Shape, Layout>::Norm(int p) const {
  const int nnz = GetNNZ();
  if (nnz == 0) {
    return T(0);
  }

  ASC_VERIFY(p > 0, "Norm parameter p must be positive");

  const T* val_data = values_.HostRead();

  if (p == 1) {
    T sum = T(0);
    for (int k = 0; k < nnz; ++k) {
      sum += std::abs(val_data[k]);
    }
    return sum;
  } else if (p == 2) {
    T sum_sq = T(0);
    for (int k = 0; k < nnz; ++k) {
      sum_sq += val_data[k] * val_data[k];
    }
    return std::sqrt(sum_sq);
  } else {
    T sum = T(0);
    for (int k = 0; k < nnz; ++k) {
      sum += std::pow(std::abs(val_data[k]), static_cast<T>(p));
    }
    return std::pow(sum, T(1) / static_cast<T>(p));
  }
}

// ==========================================================================
// Utility Functions Implementation
// ==========================================================================

template <typename T, typename Shape, typename Layout>
void SparseMArray<T, Shape, Layout>::Compact() {
  const int nnz = GetNNZ();
  if (nnz == 0) {
    return;
  }

  if constexpr (std::is_same<Layout, SparseLayoutRight>::value) {
    // CSR format
    auto& row_ptr = map_.GetRowPtr();
    auto& col_indices = map_.GetColIndices();

    const int* row_ptr_read = row_ptr.HostRead();
    int* col_data = col_indices.HostWrite();
    T* val_data = values_.HostWrite();

    const int nrows = GetExtent(0);

    // Save old row_ptr values before modifying
    UArray<int> new_row_ptr(nrows + 1);
    int* new_row_ptr_data = new_row_ptr.HostWrite();

    int write_pos = 0;

    for (int i = 0; i < nrows; ++i) {
      const int row_start = row_ptr_read[i];
      const int row_end = row_ptr_read[i + 1];
      new_row_ptr_data[i] = write_pos;

      for (int k = row_start; k < row_end; ++k) {
        if (val_data[k] != T(0)) {
          if (write_pos != k) {
            col_data[write_pos] = col_data[k];
            val_data[write_pos] = val_data[k];
          }
          write_pos++;
        }
      }
    }
    new_row_ptr_data[nrows] = write_pos;

    // Copy new row_ptr back
    int* row_ptr_write = row_ptr.HostWrite();
    for (int i = 0; i <= nrows; ++i) {
      row_ptr_write[i] = new_row_ptr_data[i];
    }

    col_indices.SetSize(write_pos);
    values_.SetSize(write_pos);
    map_.SetNNZ(write_pos);

  } else if constexpr (std::is_same<Layout, SparseLayoutLeft>::value) {
    // CSC format
    auto& col_ptr = map_.GetColPtr();
    auto& row_indices = map_.GetRowIndices();

    const int* col_ptr_read = col_ptr.HostRead();
    int* row_data = row_indices.HostWrite();
    T* val_data = values_.HostWrite();

    const int ncols = GetExtent(1);

    // Save old col_ptr values before modifying
    UArray<int> new_col_ptr(ncols + 1);
    int* new_col_ptr_data = new_col_ptr.HostWrite();

    int write_pos = 0;

    for (int j = 0; j < ncols; ++j) {
      const int col_start = col_ptr_read[j];
      const int col_end = col_ptr_read[j + 1];
      new_col_ptr_data[j] = write_pos;

      for (int k = col_start; k < col_end; ++k) {
        if (val_data[k] != T(0)) {
          if (write_pos != k) {
            row_data[write_pos] = row_data[k];
            val_data[write_pos] = val_data[k];
          }
          write_pos++;
        }
      }
    }
    new_col_ptr_data[ncols] = write_pos;

    // Copy new col_ptr back
    int* col_ptr_write = col_ptr.HostWrite();
    for (int j = 0; j <= ncols; ++j) {
      col_ptr_write[j] = new_col_ptr_data[j];
    }

    row_indices.SetSize(write_pos);
    values_.SetSize(write_pos);
    map_.SetNNZ(write_pos);

  } else if constexpr (std::is_same<Layout, SparseLayoutStride>::value) {
    // COO format
    const int rank = GetRank();
    std::vector<UArray<int>*> indices_ptrs(rank);
    std::vector<int*> indices_data(rank);

    for (int d = 0; d < rank; ++d) {
      indices_ptrs[d] = &map_.GetIndices(d);
      indices_data[d] = indices_ptrs[d]->HostWrite();
    }

    T* val_data = values_.HostWrite();

    int write_pos = 0;
    for (int k = 0; k < nnz; ++k) {
      if (val_data[k] != T(0)) {
        if (write_pos != k) {
          for (int d = 0; d < rank; ++d) {
            indices_data[d][write_pos] = indices_data[d][k];
          }
          val_data[write_pos] = val_data[k];
        }
        write_pos++;
      }
    }

    for (int d = 0; d < rank; ++d) {
      indices_ptrs[d]->SetSize(write_pos);
    }
    values_.SetSize(write_pos);

    // Update NNZ in the map
    map_.SetNNZ(write_pos);
  }
}

template <typename T, typename Shape, typename Layout>
void SparseMArray<T, Shape, Layout>::Fill(T value) {
  const int nnz = GetNNZ();
  if (nnz == 0) {
    return;
  }

  const bool use_dev = values_.UseDevice();
  T* val_data = values_.Write(use_dev);
  const T fill_value = value;

  ASC_FORALL_SWITCH(use_dev, k, nnz, {
    val_data[k] = fill_value;
  });
}

template <typename T, typename Shape, typename Layout>
void SparseMArray<T, Shape, Layout>::ShrinkToFit() {
  const int nnz = GetNNZ();

  if constexpr (std::is_same<Layout, SparseLayoutRight>::value) {
    // CSR format
    auto& col_indices = map_.GetColIndices();
    col_indices.SetSize(nnz);

  } else if constexpr (std::is_same<Layout, SparseLayoutLeft>::value) {
    // CSC format
    auto& row_indices = map_.GetRowIndices();
    row_indices.SetSize(nnz);

  } else if constexpr (std::is_same<Layout, SparseLayoutStride>::value) {
    // COO format
    const int rank = GetRank();
    for (int d = 0; d < rank; ++d) {
      auto& indices = map_.GetIndices(d);
      indices.SetSize(nnz);
    }
  }

  values_.SetSize(nnz);
}

// ==========================================================================
// Compound Assignment Operators Implementation
// ==========================================================================

template <typename T, typename Shape, typename Layout>
SparseMArray<T, Shape, Layout>& SparseMArray<T, Shape, Layout>::operator+=(
    const SparseMArray<T, Shape, Layout>& other) {
  // Check shapes match
  ASC_VERIFY(GetRank() == other.GetRank(), "Ranks must match");
  for (int d = 0; d < GetRank(); ++d) {
    ASC_VERIFY(GetExtent(d) == other.GetExtent(d),
                  "Shapes must match for element-wise addition");
  }

  // Convert both to COO format for easy merging
  auto coo1 = ToLayout<SparseLayoutStride>();
  auto coo2 = other.ToLayout<SparseLayoutStride>();

  const int nnz1 = coo1.GetNNZ();
  const int nnz2 = coo2.GetNNZ();
  const int rank = GetRank();

  // Create tuples for merge
  std::vector<SparseTuple<T>> tuples;
  tuples.reserve(nnz1 + nnz2);

  // Add first matrix tuples
  for (int k = 0; k < nnz1; ++k) {
    UArray<int> indices(rank);
    int* idx_data = indices.HostWrite();
    for (int d = 0; d < rank; ++d) {
      idx_data[d] = coo1.GetMap().GetIndices(d)[k];
    }
    tuples.emplace_back(indices, coo1.GetValues()[k]);
  }

  // Add second matrix tuples
  for (int k = 0; k < nnz2; ++k) {
    UArray<int> indices(rank);
    int* idx_data = indices.HostWrite();
    for (int d = 0; d < rank; ++d) {
      idx_data[d] = coo2.GetMap().GetIndices(d)[k];
    }
    tuples.emplace_back(indices, coo2.GetValues()[k]);
  }

  // Sort and merge duplicates
  std::sort(tuples.begin(), tuples.end());

  std::vector<SparseTuple<T>> merged;
  merged.reserve(tuples.size());

  for (size_t i = 0; i < tuples.size(); ++i) {
    if (i == 0 || !(tuples[i].indices == tuples[i - 1].indices)) {
      merged.push_back(tuples[i]);
    } else {
      // Duplicate index - add values
      merged.back().value += tuples[i].value;
    }
  }

  // Remove zeros
  merged.erase(
      std::remove_if(merged.begin(), merged.end(),
                     [](const SparseTuple<T>& t) { return t.value == T(0); }),
      merged.end());

  // Rebuild this array with merged data
  if constexpr (std::is_same<Layout, SparseLayoutStride>::value) {
    Clear();
    SetFromTuples(merged.begin(), merged.end());
  } else {
    // Convert back to original layout
    SparseMArray<T, Shape, SparseLayoutStride> coo_result(GetShape());
    coo_result.SetFromTuples(merged.begin(), merged.end());
    *this = coo_result.template ToLayout<Layout>();
  }

  return *this;
}

template <typename T, typename Shape, typename Layout>
SparseMArray<T, Shape, Layout>& SparseMArray<T, Shape, Layout>::operator-=(
    const SparseMArray<T, Shape, Layout>& other) {
  // Check shapes match
  ASC_VERIFY(GetRank() == other.GetRank(), "Ranks must match");
  for (int d = 0; d < GetRank(); ++d) {
    ASC_VERIFY(GetExtent(d) == other.GetExtent(d),
                  "Shapes must match for element-wise subtraction");
  }

  // Convert both to COO format for easy merging
  auto coo1 = ToLayout<SparseLayoutStride>();
  auto coo2 = other.ToLayout<SparseLayoutStride>();

  const int nnz1 = coo1.GetNNZ();
  const int nnz2 = coo2.GetNNZ();
  const int rank = GetRank();

  // Create tuples for merge
  std::vector<SparseTuple<T>> tuples;
  tuples.reserve(nnz1 + nnz2);

  // Add first matrix tuples
  for (int k = 0; k < nnz1; ++k) {
    UArray<int> indices(rank);
    int* idx_data = indices.HostWrite();
    for (int d = 0; d < rank; ++d) {
      idx_data[d] = coo1.GetMap().GetIndices(d)[k];
    }
    tuples.emplace_back(indices, coo1.GetValues()[k]);
  }

  // Add second matrix tuples (negated)
  for (int k = 0; k < nnz2; ++k) {
    UArray<int> indices(rank);
    int* idx_data = indices.HostWrite();
    for (int d = 0; d < rank; ++d) {
      idx_data[d] = coo2.GetMap().GetIndices(d)[k];
    }
    tuples.emplace_back(indices, -coo2.GetValues()[k]);
  }

  // Sort and merge duplicates
  std::sort(tuples.begin(), tuples.end());

  std::vector<SparseTuple<T>> merged;
  merged.reserve(tuples.size());

  for (size_t i = 0; i < tuples.size(); ++i) {
    if (i == 0 || !(tuples[i].indices == tuples[i - 1].indices)) {
      merged.push_back(tuples[i]);
    } else {
      // Duplicate index - add values (second is already negated)
      merged.back().value += tuples[i].value;
    }
  }

  // Remove zeros
  merged.erase(
      std::remove_if(merged.begin(), merged.end(),
                     [](const SparseTuple<T>& t) { return t.value == T(0); }),
      merged.end());

  // Rebuild this array with merged data
  if constexpr (std::is_same<Layout, SparseLayoutStride>::value) {
    Clear();
    SetFromTuples(merged.begin(), merged.end());
  } else {
    // Convert back to original layout
    SparseMArray<T, Shape, SparseLayoutStride> coo_result(GetShape());
    coo_result.SetFromTuples(merged.begin(), merged.end());
    *this = coo_result.template ToLayout<Layout>();
  }

  return *this;
}

template <typename T, typename Shape, typename Layout>
SparseMArray<T, Shape, Layout>& SparseMArray<T, Shape, Layout>::operator*=(
    const SparseMArray<T, Shape, Layout>& other) {
  // Check shapes match
  ASC_VERIFY(GetRank() == other.GetRank(), "Ranks must match");
  for (int d = 0; d < GetRank(); ++d) {
    ASC_VERIFY(GetExtent(d) == other.GetExtent(d),
                  "Shapes must match for element-wise multiplication");
  }

  // Convert both to COO format
  auto coo1 = ToLayout<SparseLayoutStride>();
  auto coo2 = other.ToLayout<SparseLayoutStride>();

  const int nnz1 = coo1.GetNNZ();
  const int nnz2 = coo2.GetNNZ();
  const int rank = GetRank();

  // Build index map for second matrix for fast lookup
  std::unordered_map<std::vector<int>, T, sparse_detail::IndexVectorHash>
      value_map;
  value_map.reserve(static_cast<std::size_t>(nnz2));

  for (int k = 0; k < nnz2; ++k) {
    std::vector<int> key(rank);
    for (int d = 0; d < rank; ++d) {
      key[d] = coo2.GetMap().GetIndices(d)[k];
    }
    value_map[key] = coo2.GetValues()[k];
  }

  // Multiply matching elements
  std::vector<SparseTuple<T>> tuples;
  tuples.reserve((nnz1 < nnz2) ? nnz1 : nnz2);

  for (int k = 0; k < nnz1; ++k) {
    std::vector<int> key(rank);
    for (int d = 0; d < rank; ++d) {
      key[d] = coo1.GetMap().GetIndices(d)[k];
    }

    auto it = value_map.find(key);
    if (it != value_map.end()) {
      const T product = coo1.GetValues()[k] * it->second;
      if (product != T(0)) {
        UArray<int> indices(rank);
        int* idx_data = indices.HostWrite();
        for (int d = 0; d < rank; ++d) {
          idx_data[d] = key[d];
        }
        tuples.emplace_back(indices, product);
      }
    }
  }

  // Rebuild this array with result
  if constexpr (std::is_same<Layout, SparseLayoutStride>::value) {
    Clear();
    SetFromTuples(tuples.begin(), tuples.end());
  } else {
    // Convert back to original layout
    SparseMArray<T, Shape, SparseLayoutStride> coo_result(GetShape());
    coo_result.SetFromTuples(tuples.begin(), tuples.end());
    *this = coo_result.template ToLayout<Layout>();
  }

  return *this;
}

template <typename T, typename Shape, typename Layout>
SparseMArray<T, Shape, Layout>& SparseMArray<T, Shape, Layout>::operator/=(
    const SparseMArray<T, Shape, Layout>& other) {
  // Check shapes match
  ASC_VERIFY(GetRank() == other.GetRank(), "Ranks must match");
  for (int d = 0; d < GetRank(); ++d) {
    ASC_VERIFY(GetExtent(d) == other.GetExtent(d),
                  "Shapes must match for element-wise division");
  }

  // Convert both to COO format
  auto coo1 = ToLayout<SparseLayoutStride>();
  auto coo2 = other.ToLayout<SparseLayoutStride>();

  const int nnz1 = coo1.GetNNZ();
  const int nnz2 = coo2.GetNNZ();
  const int rank = GetRank();

  // Build index map for second matrix for fast lookup
  std::unordered_map<std::vector<int>, T, sparse_detail::IndexVectorHash>
      value_map;
  value_map.reserve(static_cast<std::size_t>(nnz2));

  for (int k = 0; k < nnz2; ++k) {
    std::vector<int> key(rank);
    for (int d = 0; d < rank; ++d) {
      key[d] = coo2.GetMap().GetIndices(d)[k];
    }
    ASC_VERIFY(coo2.GetValues()[k] != T(0),
                  "Division by zero in sparse element-wise division");
    value_map[key] = coo2.GetValues()[k];
  }

  // Divide matching elements
  std::vector<SparseTuple<T>> tuples;
  tuples.reserve(nnz1);

  for (int k = 0; k < nnz1; ++k) {
    std::vector<int> key(rank);
    for (int d = 0; d < rank; ++d) {
      key[d] = coo1.GetMap().GetIndices(d)[k];
    }

    auto it = value_map.find(key);
    ASC_VERIFY(it != value_map.end(),
                  "Division by implicit zero in sparse element-wise division");
    const T quotient = coo1.GetValues()[k] / it->second;
    if (quotient != T(0)) {
      UArray<int> indices(rank);
      int* idx_data = indices.HostWrite();
      for (int d = 0; d < rank; ++d) {
        idx_data[d] = key[d];
      }
      tuples.emplace_back(indices, quotient);
    }
  }

  // Rebuild this array with result
  if constexpr (std::is_same<Layout, SparseLayoutStride>::value) {
    Clear();
    SetFromTuples(tuples.begin(), tuples.end());
  } else {
    // Convert back to original layout
    SparseMArray<T, Shape, SparseLayoutStride> coo_result(GetShape());
    coo_result.SetFromTuples(tuples.begin(), tuples.end());
    *this = coo_result.template ToLayout<Layout>();
  }

  return *this;
}

// Scalar compound assignment operators
template <typename T, typename Shape, typename Layout>
template <Arithmetic U>
SparseMArray<T, Shape, Layout>& SparseMArray<T, Shape, Layout>::operator+=(
    const U& scalar) {
  const int nnz = GetNNZ();
  const bool use_dev = values_.UseDevice();
  T* val_data = values_.Write(use_dev);
  const T scalar_t = static_cast<T>(scalar);

  ASC_FORALL_SWITCH(use_dev, k, nnz, {
    val_data[k] += scalar_t;
  });

  return *this;
}

template <typename T, typename Shape, typename Layout>
template <Arithmetic U>
SparseMArray<T, Shape, Layout>& SparseMArray<T, Shape, Layout>::operator-=(
    const U& scalar) {
  const int nnz = GetNNZ();
  const bool use_dev = values_.UseDevice();
  T* val_data = values_.Write(use_dev);
  const T scalar_t = static_cast<T>(scalar);

  ASC_FORALL_SWITCH(use_dev, k, nnz, {
    val_data[k] -= scalar_t;
  });

  return *this;
}

template <typename T, typename Shape, typename Layout>
template <Arithmetic U>
SparseMArray<T, Shape, Layout>& SparseMArray<T, Shape, Layout>::operator*=(
    const U& scalar) {
  const int nnz = GetNNZ();
  const bool use_dev = values_.UseDevice();
  T* val_data = values_.Write(use_dev);
  const T scalar_t = static_cast<T>(scalar);

  ASC_FORALL_SWITCH(use_dev, k, nnz, {
    val_data[k] *= scalar_t;
  });

  return *this;
}

template <typename T, typename Shape, typename Layout>
template <Arithmetic U>
SparseMArray<T, Shape, Layout>& SparseMArray<T, Shape, Layout>::operator/=(
    const U& scalar) {
  ASC_VERIFY(scalar != U(0), "Division by zero");
  const int nnz = GetNNZ();
  const bool use_dev = values_.UseDevice();
  T* val_data = values_.Write(use_dev);
  const T scalar_t = static_cast<T>(scalar);

  ASC_FORALL_SWITCH(use_dev, k, nnz, {
    val_data[k] /= scalar_t;
  });

  return *this;
}

// Unary negation operator
template <typename T, typename Shape, typename Layout>
SparseMArray<T, Shape, Layout> SparseMArray<T, Shape, Layout>::operator-()
    const {
  SparseMArray<T, Shape, Layout> result(*this);
  const int nnz = result.GetNNZ();
  const bool use_dev = result.values_.UseDevice();
  T* val_data = result.values_.Write(use_dev);

  ASC_FORALL_SWITCH(use_dev, k, nnz, {
    val_data[k] = -val_data[k];
  });

  return result;
}

}  // namespace asc

#endif  // ASC_GENERIC_SPMARRAY_IMPL_H_
