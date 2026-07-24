// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: tests/generic/unit_test_spmarray.cc
// Unit tests for N-dimensional sparse arrays (SparseMArray)
// ============================================================================

#include <gtest/gtest.h>
#include <vector>
#include <algorithm>

#include "common/common.h"
#include "asc/array/marray.h"
#include "asc/array/spmindex.h"
#include "asc/array/spmiterator.h"

namespace asc {

// ============================================================================
// SparseMArray Basic Tests - Constructors and Properties
// ============================================================================

template <typename MemType>
using SparseMArrayTest = BaseTest<MemType>;

TYPED_TEST_SUITE(SparseMArrayTest, AllMemoryTypes);

TYPED_TEST(SparseMArrayTest, COO_DefaultConstruction) {
  SpDMatrix<real_t, SparseLayoutStride> mat(DShape<2>(5, 5));

  EXPECT_EQ(mat.GetRank(), 2);
  EXPECT_EQ(mat.GetExtent(0), 5);
  EXPECT_EQ(mat.GetExtent(1), 5);
  EXPECT_EQ(mat.GetSize(), 25);
  EXPECT_EQ(mat.GetNNZ(), 0);
  EXPECT_FALSE(mat.IsCompressed());
}

TYPED_TEST(SparseMArrayTest, VariadicConstructor_2D) {
  SparseMArray<real_t, DShape<2>, SparseLayoutStride> mat(5, 7);

  EXPECT_EQ(mat.GetRank(), 2);
  EXPECT_EQ(mat.GetExtent(0), 5);
  EXPECT_EQ(mat.GetExtent(1), 7);
  EXPECT_EQ(mat.GetSize(), 35);
  EXPECT_EQ(mat.GetNNZ(), 0);
  EXPECT_FALSE(mat.IsCompressed());
}

TYPED_TEST(SparseMArrayTest, VariadicConstructor_3D) {
  SparseMArray<real_t, DShape<3>, SparseLayoutStride> tensor(3, 4, 5);

  EXPECT_EQ(tensor.GetRank(), 3);
  EXPECT_EQ(tensor.GetExtent(0), 3);
  EXPECT_EQ(tensor.GetExtent(1), 4);
  EXPECT_EQ(tensor.GetExtent(2), 5);
  EXPECT_EQ(tensor.GetSize(), 60);
  EXPECT_EQ(tensor.GetNNZ(), 0);
  EXPECT_FALSE(tensor.IsCompressed());
}

TEST(SparseMArrayMemoryTypeTest, VariadicConstructorWithMemoryType_CPU) {
  // Test variadic constructor with CPU memory type
  SparseMArray<real_t, DShape<2>, SparseLayoutStride> mat(MemoryType::kHost, 4, 6);

  EXPECT_EQ(mat.GetRank(), 2);
  EXPECT_EQ(mat.GetExtent(0), 4);
  EXPECT_EQ(mat.GetExtent(1), 6);
  EXPECT_EQ(mat.GetSize(), 24);
  EXPECT_EQ(mat.GetNNZ(), 0);
}

TYPED_TEST(SparseMArrayTest, COO_InsertElements) {
  SpDMatrix<real_t, SparseLayoutStride> mat(DShape<2>(3, 3));

  mat.Insert(UArray<int>{0, 0}, 1.0);
  mat.Insert(UArray<int>{1, 1}, 2.0);
  mat.Insert(UArray<int>{2, 2}, 3.0);

  EXPECT_EQ(mat.GetNNZ(), 3);
  EXPECT_FALSE(mat.IsCompressed());
}

TYPED_TEST(SparseMArrayTest, COO_Insert2DConvenience) {
  SpDMatrix<real_t, SparseLayoutStride> mat(DShape<2>(3, 3));

  mat.Insert(0, 0, 1.0);
  mat.Insert(1, 1, 2.0);
  mat.Insert(2, 2, 3.0);

  EXPECT_EQ(mat.GetNNZ(), 3);
}

TYPED_TEST(SparseMArrayTest, COO_SetFromTuples) {
  SpDMatrix<real_t, SparseLayoutStride> mat(DShape<2>(3, 3));

  std::vector<SparseTuple<real_t>> tuples;
  tuples.emplace_back(UArray<int>{0, 0}, 1.0);
  tuples.emplace_back(UArray<int>{1, 1}, 2.0);
  tuples.emplace_back(UArray<int>{2, 2}, 3.0);
  tuples.emplace_back(UArray<int>{0, 2}, 4.0);

  mat.SetFromTuples(tuples.begin(), tuples.end());

  EXPECT_EQ(mat.GetNNZ(), 4);

  // Verify values using sparse accessors
  const auto& values = mat.GetValues();
  const real_t* val_data = values.HostRead();
  EXPECT_REAL_EQ(val_data[0], 1.0);
  EXPECT_REAL_EQ(val_data[1], 2.0);
  EXPECT_REAL_EQ(val_data[2], 3.0);
  EXPECT_REAL_EQ(val_data[3], 4.0);
}

TYPED_TEST(SparseMArrayTest, COO_InitializerListInterface) {
  SpDMatrix<real_t, SparseLayoutStride> mat(DShape<2>(3, 3));

  std::vector<SparseTuple<real_t>> tuples;
  tuples.emplace_back(std::initializer_list<int>{0, 0}, 1.0);
  tuples.emplace_back(std::initializer_list<int>{1, 1}, 2.0);
  tuples.emplace_back(std::initializer_list<int>{2, 2}, 3.0);

  mat.SetFromTuples(tuples.begin(), tuples.end());

  EXPECT_EQ(mat.GetNNZ(), 3);
}

TYPED_TEST(SparseMArrayTest, COO_Finalize) {
  SpDMatrix<real_t, SparseLayoutStride> mat(DShape<2>(3, 3));

  // Insert in non-sorted order
  mat.Insert(2, 2, 3.0);
  mat.Insert(0, 0, 1.0);
  mat.Insert(1, 1, 2.0);

  mat.Finalize();

  EXPECT_EQ(mat.GetNNZ(), 3);
  EXPECT_TRUE(mat.IsCompressed());
}

TYPED_TEST(SparseMArrayTest, COO_FinalizeSumsDuplicates) {
  SpDMatrix<real_t, SparseLayoutStride> mat(DShape<2>(3, 3));

  // Insert duplicate entries at same location
  mat.Insert(1, 1, 2.0);
  mat.Insert(0, 0, 1.0);
  mat.Insert(1, 1, 3.0);  // Duplicate at (1,1)
  mat.Insert(2, 2, 4.0);
  mat.Insert(1, 1, 5.0);  // Another duplicate at (1,1)

  mat.Finalize();

  // Should have 3 unique entries after merging
  EXPECT_EQ(mat.GetNNZ(), 3);
  EXPECT_TRUE(mat.IsCompressed());

  // Verify that duplicates were summed: (1,1) should be 2+3+5=10
  const auto& row_indices = mat.GetMap().GetIndices(0);
  const auto& col_indices = mat.GetMap().GetIndices(1);
  const auto& values = mat.GetValues();
  const real_t* val_data = values.HostRead();

  bool found_11 = false;
  for (int k = 0; k < mat.GetNNZ(); ++k) {
    if (row_indices[k] == 1 && col_indices[k] == 1) {
      EXPECT_REAL_EQ(val_data[k], 10.0);  // 2+3+5=10
      found_11 = true;
    }
  }
  EXPECT_TRUE(found_11);
}

TYPED_TEST(SparseMArrayTest, COO_FinalizeRemovesZeros) {
  SpDMatrix<real_t, SparseLayoutStride> mat(DShape<2>(3, 3));

  // Insert entries that sum to zero
  mat.Insert(1, 1, 5.0);
  mat.Insert(0, 0, 1.0);
  mat.Insert(1, 1, -5.0);  // Cancels with previous (1,1)
  mat.Insert(2, 2, 3.0);

  mat.Finalize();

  // Should have 2 entries after removing zero
  EXPECT_EQ(mat.GetNNZ(), 2);
  EXPECT_TRUE(mat.IsCompressed());

  // Verify (1,1) was removed
  const auto& row_indices = mat.GetMap().GetIndices(0);
  const auto& col_indices = mat.GetMap().GetIndices(1);

  for (int k = 0; k < mat.GetNNZ(); ++k) {
    EXPECT_FALSE(row_indices[k] == 1 && col_indices[k] == 1);
  }
}

// ============================================================================
// CSR Format (SparseLayoutRight) Tests
// ============================================================================

TYPED_TEST(SparseMArrayTest, CSR_Construction) {
  SpDMatrix<real_t, SparseLayoutRight> mat(DShape<2>(3, 4));

  EXPECT_EQ(mat.GetRank(), 2);
  EXPECT_EQ(mat.GetExtent(0), 3);
  EXPECT_EQ(mat.GetExtent(1), 4);
  EXPECT_EQ(mat.GetNNZ(), 0);
}

TYPED_TEST(SparseMArrayTest, CSR_SetFromTuples) {
  SpDMatrix<real_t, SparseLayoutRight> mat(DShape<2>(3, 4));

  std::vector<SparseTuple<real_t>> tuples;
  tuples.emplace_back(UArray<int>{0, 1}, 1.0);
  tuples.emplace_back(UArray<int>{0, 3}, 2.0);
  tuples.emplace_back(UArray<int>{1, 2}, 3.0);
  tuples.emplace_back(UArray<int>{2, 0}, 4.0);

  mat.SetFromTuples(tuples.begin(), tuples.end());

  EXPECT_EQ(mat.GetNNZ(), 4);
  EXPECT_TRUE(mat.IsCompressed());
}

TYPED_TEST(SparseMArrayTest, CSR_RowPointers) {
  SpDMatrix<real_t, SparseLayoutRight> mat(DShape<2>(3, 4));

  std::vector<SparseTuple<real_t>> tuples;
  tuples.emplace_back(UArray<int>{0, 1}, 1.0);
  tuples.emplace_back(UArray<int>{0, 3}, 2.0);
  tuples.emplace_back(UArray<int>{1, 2}, 3.0);
  tuples.emplace_back(UArray<int>{2, 0}, 4.0);

  mat.SetFromTuples(tuples.begin(), tuples.end());

  const auto& row_ptr = mat.GetMap().GetRowPtr();
  const int* ptr_data = row_ptr.HostRead();

  // Row 0 has 2 elements, row 1 has 1 element, row 2 has 1 element
  EXPECT_EQ(ptr_data[0], 0);
  EXPECT_EQ(ptr_data[1], 2);
  EXPECT_EQ(ptr_data[2], 3);
  EXPECT_EQ(ptr_data[3], 4);
}

// ============================================================================
// CSC Format (SparseLayoutLeft) Tests
// ============================================================================

TYPED_TEST(SparseMArrayTest, CSC_Construction) {
  SpDMatrix<real_t, SparseLayoutLeft> mat(DShape<2>(3, 4));

  EXPECT_EQ(mat.GetRank(), 2);
  EXPECT_EQ(mat.GetExtent(0), 3);
  EXPECT_EQ(mat.GetExtent(1), 4);
  EXPECT_EQ(mat.GetNNZ(), 0);
}

TYPED_TEST(SparseMArrayTest, CSC_SetFromTuples) {
  SpDMatrix<real_t, SparseLayoutLeft> mat(DShape<2>(3, 4));

  std::vector<SparseTuple<real_t>> tuples;
  tuples.emplace_back(UArray<int>{0, 1}, 1.0);
  tuples.emplace_back(UArray<int>{2, 1}, 2.0);
  tuples.emplace_back(UArray<int>{1, 2}, 3.0);
  tuples.emplace_back(UArray<int>{0, 3}, 4.0);

  mat.SetFromTuples(tuples.begin(), tuples.end());

  EXPECT_EQ(mat.GetNNZ(), 4);
  EXPECT_TRUE(mat.IsCompressed());
}

TYPED_TEST(SparseMArrayTest, CSC_ColumnPointers) {
  SpDMatrix<real_t, SparseLayoutLeft> mat(DShape<2>(3, 4));

  std::vector<SparseTuple<real_t>> tuples;
  tuples.emplace_back(UArray<int>{0, 1}, 1.0);
  tuples.emplace_back(UArray<int>{2, 1}, 2.0);
  tuples.emplace_back(UArray<int>{1, 2}, 3.0);
  tuples.emplace_back(UArray<int>{0, 3}, 4.0);

  mat.SetFromTuples(tuples.begin(), tuples.end());

  const auto& col_ptr = mat.GetMap().GetColPtr();
  const int* ptr_data = col_ptr.HostRead();

  // Col 0: 0 elements, col 1: 2 elements, col 2: 1 element, col 3: 1 element
  EXPECT_EQ(ptr_data[0], 0);
  EXPECT_EQ(ptr_data[1], 0);
  EXPECT_EQ(ptr_data[2], 2);
  EXPECT_EQ(ptr_data[3], 3);
  EXPECT_EQ(ptr_data[4], 4);
}

// ============================================================================
// Format Conversion Tests
// ============================================================================

TYPED_TEST(SparseMArrayTest, ConvertCOOtoCSR) {
  SpDMatrix<real_t, SparseLayoutStride> coo_mat(DShape<2>(3, 3));

  std::vector<SparseTuple<real_t>> tuples;
  tuples.emplace_back(UArray<int>{0, 0}, 1.0);
  tuples.emplace_back(UArray<int>{1, 1}, 2.0);
  tuples.emplace_back(UArray<int>{2, 2}, 3.0);
  coo_mat.SetFromTuples(tuples.begin(), tuples.end());

  auto csr_mat = coo_mat.ToLayout<SparseLayoutRight>();

  EXPECT_EQ(csr_mat.GetNNZ(), 3);
  EXPECT_TRUE(csr_mat.IsCompressed());

  // Verify values using sparse accessors
  const auto& values = csr_mat.GetValues();
  const real_t* val_data = values.HostRead();
  EXPECT_REAL_EQ(val_data[0], 1.0);
  EXPECT_REAL_EQ(val_data[1], 2.0);
  EXPECT_REAL_EQ(val_data[2], 3.0);
}

TYPED_TEST(SparseMArrayTest, ConvertCOOtoCSC) {
  SpDMatrix<real_t, SparseLayoutStride> coo_mat(DShape<2>(3, 3));

  std::vector<SparseTuple<real_t>> tuples;
  tuples.emplace_back(UArray<int>{0, 0}, 1.0);
  tuples.emplace_back(UArray<int>{1, 1}, 2.0);
  tuples.emplace_back(UArray<int>{2, 2}, 3.0);
  coo_mat.SetFromTuples(tuples.begin(), tuples.end());

  auto csc_mat = coo_mat.ToLayout<SparseLayoutLeft>();

  EXPECT_EQ(csc_mat.GetNNZ(), 3);
  EXPECT_TRUE(csc_mat.IsCompressed());

  // Verify values using sparse accessors
  const auto& values = csc_mat.GetValues();
  const real_t* val_data = values.HostRead();
  EXPECT_REAL_EQ(val_data[0], 1.0);
  EXPECT_REAL_EQ(val_data[1], 2.0);
  EXPECT_REAL_EQ(val_data[2], 3.0);
}

TYPED_TEST(SparseMArrayTest, ConvertCSRtoCSC) {
  SpDMatrix<real_t, SparseLayoutRight> csr_mat(DShape<2>(3, 3));

  std::vector<SparseTuple<real_t>> tuples;
  tuples.emplace_back(UArray<int>{0, 0}, 1.0);
  tuples.emplace_back(UArray<int>{0, 2}, 4.0);
  tuples.emplace_back(UArray<int>{1, 1}, 2.0);
  tuples.emplace_back(UArray<int>{2, 2}, 3.0);
  csr_mat.SetFromTuples(tuples.begin(), tuples.end());

  auto csc_mat = csr_mat.ToLayout<SparseLayoutLeft>();

  EXPECT_EQ(csc_mat.GetNNZ(), 4);
  EXPECT_TRUE(csc_mat.IsCompressed());

  // Verify values using sparse accessors
  const auto& values = csc_mat.GetValues();
  // Values are reordered in CSC format, but count should match
  EXPECT_EQ(values.GetSize(), 4);
}

// ============================================================================
// 3D Sparse Tensor Tests
// ============================================================================

TYPED_TEST(SparseMArrayTest, COO_3D_DefaultConstruction) {
  SparseMArray<real_t, DShape<3>, SparseLayoutStride> tensor(
      DShape<3>(2, 3, 4));

  EXPECT_EQ(tensor.GetRank(), 3);
  EXPECT_EQ(tensor.GetExtent(0), 2);
  EXPECT_EQ(tensor.GetExtent(1), 3);
  EXPECT_EQ(tensor.GetExtent(2), 4);
  EXPECT_EQ(tensor.GetSize(), 24);
  EXPECT_EQ(tensor.GetNNZ(), 0);
}

TYPED_TEST(SparseMArrayTest, COO_3D_InsertElements) {
  SparseMArray<real_t, DShape<3>, SparseLayoutStride> tensor(
      DShape<3>(2, 3, 4));

  tensor.Insert(UArray<int>{0, 0, 0}, 1.0);
  tensor.Insert(UArray<int>{1, 1, 1}, 2.0);
  tensor.Insert(UArray<int>{1, 2, 3}, 3.0);

  EXPECT_EQ(tensor.GetNNZ(), 3);
}

TYPED_TEST(SparseMArrayTest, COO_3D_SetFromTuples) {
  SparseMArray<real_t, DShape<3>, SparseLayoutStride> tensor(
      DShape<3>(2, 3, 4));

  std::vector<SparseTuple<real_t>> tuples;
  tuples.emplace_back(UArray<int>{0, 0, 0}, 1.0);
  tuples.emplace_back(UArray<int>{1, 1, 1}, 2.0);
  tuples.emplace_back(UArray<int>{1, 2, 3}, 3.0);

  tensor.SetFromTuples(tuples.begin(), tuples.end());

  EXPECT_EQ(tensor.GetNNZ(), 3);

  // Verify values using sparse accessors
  const auto& values = tensor.GetValues();
  const real_t* val_data = values.HostRead();
  EXPECT_REAL_EQ(val_data[0], 1.0);
  EXPECT_REAL_EQ(val_data[1], 2.0);
  EXPECT_REAL_EQ(val_data[2], 3.0);
}

TYPED_TEST(SparseMArrayTest, COO_3D_Finalize) {
  SparseMArray<real_t, DShape<3>, SparseLayoutStride> tensor(
      DShape<3>(2, 3, 4));

  tensor.Insert(UArray<int>{1, 2, 3}, 3.0);
  tensor.Insert(UArray<int>{0, 0, 0}, 1.0);
  tensor.Insert(UArray<int>{1, 1, 1}, 2.0);

  tensor.Finalize();

  EXPECT_EQ(tensor.GetNNZ(), 3);
  EXPECT_TRUE(tensor.IsCompressed());
}

TYPED_TEST(SparseMArrayTest, CompressFirst_3D_SetFromTuples) {
  SparseMArray<real_t, DShape<3>, SparseLayoutRight> tensor(DShape<3>(2, 3, 4));

  std::vector<SparseTuple<real_t>> tuples;
  tuples.emplace_back(UArray<int>{0, 0, 0}, 1.0);
  tuples.emplace_back(UArray<int>{0, 1, 2}, 2.0);
  tuples.emplace_back(UArray<int>{1, 2, 3}, 3.0);

  tensor.SetFromTuples(tuples.begin(), tuples.end());

  EXPECT_EQ(tensor.GetNNZ(), 3);
  EXPECT_TRUE(tensor.IsCompressed());
}

TYPED_TEST(SparseMArrayTest, CompressFirst_3D_Structure) {
  SparseMArray<real_t, DShape<3>, SparseLayoutRight> tensor(DShape<3>(2, 3, 4));

  std::vector<SparseTuple<real_t>> tuples;
  tuples.emplace_back(UArray<int>{0, 0, 0}, 1.0);
  tuples.emplace_back(UArray<int>{0, 1, 2}, 2.0);
  tuples.emplace_back(UArray<int>{1, 2, 3}, 3.0);

  tensor.SetFromTuples(tuples.begin(), tuples.end());

  const auto& outer_ptr = tensor.GetMap().GetOuterPtr();
  const int* ptr_data = outer_ptr.HostRead();

  // First dimension has extent 2, so outer_ptr has size 3
  EXPECT_EQ(ptr_data[0], 0);
  EXPECT_EQ(ptr_data[1], 2);  // First slab has 2 elements
  EXPECT_EQ(ptr_data[2], 3);  // Second slab has 1 element
}

TYPED_TEST(SparseMArrayTest, CompressLast_3D_SetFromTuples) {
  SparseMArray<real_t, DShape<3>, SparseLayoutLeft> tensor(DShape<3>(2, 3, 4));

  std::vector<SparseTuple<real_t>> tuples;
  tuples.emplace_back(UArray<int>{0, 0, 0}, 1.0);
  tuples.emplace_back(UArray<int>{1, 1, 1}, 2.0);
  tuples.emplace_back(UArray<int>{1, 2, 3}, 3.0);

  tensor.SetFromTuples(tuples.begin(), tuples.end());

  EXPECT_EQ(tensor.GetNNZ(), 3);
  EXPECT_TRUE(tensor.IsCompressed());
}

TYPED_TEST(SparseMArrayTest, CompressLast_3D_Structure) {
  SparseMArray<real_t, DShape<3>, SparseLayoutLeft> tensor(DShape<3>(2, 3, 4));

  std::vector<SparseTuple<real_t>> tuples;
  tuples.emplace_back(UArray<int>{0, 0, 0}, 1.0);
  tuples.emplace_back(UArray<int>{1, 1, 1}, 2.0);
  tuples.emplace_back(UArray<int>{1, 2, 3}, 3.0);

  tensor.SetFromTuples(tuples.begin(), tuples.end());

  const auto& outer_ptr = tensor.GetMap().GetOuterPtr();
  const int* ptr_data = outer_ptr.HostRead();

  // Last dimension has extent 4, so outer_ptr has size 5
  EXPECT_EQ(ptr_data[0], 0);
  EXPECT_EQ(ptr_data[1], 1);  // k=0 has 1 element
  EXPECT_EQ(ptr_data[2], 2);  // k=1 has 1 element
  EXPECT_EQ(ptr_data[3], 2);  // k=2 has 0 elements
  EXPECT_EQ(ptr_data[4], 3);  // k=3 has 1 element
}

TYPED_TEST(SparseMArrayTest, Convert3D_COOtoCompressFirst) {
  SparseMArray<real_t, DShape<3>, SparseLayoutStride> coo_tensor(
      DShape<3>(2, 3, 4));

  std::vector<SparseTuple<real_t>> tuples;
  tuples.emplace_back(UArray<int>{0, 0, 0}, 1.0);
  tuples.emplace_back(UArray<int>{1, 1, 1}, 2.0);

  coo_tensor.SetFromTuples(tuples.begin(), tuples.end());

  auto compressed = coo_tensor.ToLayout<SparseLayoutRight>();

  EXPECT_EQ(compressed.GetNNZ(), 2);
  EXPECT_TRUE(compressed.IsCompressed());
}

TYPED_TEST(SparseMArrayTest, Convert3D_COOtoCompressLast) {
  SparseMArray<real_t, DShape<3>, SparseLayoutStride> coo_tensor(
      DShape<3>(2, 3, 4));

  std::vector<SparseTuple<real_t>> tuples;
  tuples.emplace_back(UArray<int>{0, 0, 0}, 1.0);
  tuples.emplace_back(UArray<int>{1, 1, 1}, 2.0);

  coo_tensor.SetFromTuples(tuples.begin(), tuples.end());

  auto compressed = coo_tensor.ToLayout<SparseLayoutLeft>();

  EXPECT_EQ(compressed.GetNNZ(), 2);
  EXPECT_TRUE(compressed.IsCompressed());
}

TYPED_TEST(SparseMArrayTest, Convert3D_CompressFirstToCompressLast) {
  SparseMArray<real_t, DShape<3>, SparseLayoutRight> tensor1(
      DShape<3>(2, 3, 4));

  std::vector<SparseTuple<real_t>> tuples;
  tuples.emplace_back(UArray<int>{0, 0, 0}, 1.0);
  tuples.emplace_back(UArray<int>{1, 1, 1}, 2.0);

  tensor1.SetFromTuples(tuples.begin(), tuples.end());

  auto tensor2 = tensor1.ToLayout<SparseLayoutLeft>();

  EXPECT_EQ(tensor2.GetNNZ(), 2);
  EXPECT_TRUE(tensor2.IsCompressed());
}

// ============================================================================
// Utility Tests
// ============================================================================

TYPED_TEST(SparseMArrayTest, Sparsity) {
  SpDMatrix<real_t, SparseLayoutStride> mat(DShape<2>(10, 10));

  std::vector<SparseTuple<real_t>> tuples;
  for (int i = 0; i < 5; ++i) {
    tuples.emplace_back(UArray<int>{i, i}, 1.0);
  }
  mat.SetFromTuples(tuples.begin(), tuples.end());

  EXPECT_REAL_EQ(mat.GetSparsity(), 0.05);
}

TYPED_TEST(SparseMArrayTest, Clear) {
  SpDMatrix<real_t, SparseLayoutStride> mat(DShape<2>(5, 5));

  mat.Insert(0, 0, 1.0);
  mat.Insert(1, 1, 2.0);

  EXPECT_EQ(mat.GetNNZ(), 2);

  mat.Clear();

  EXPECT_EQ(mat.GetNNZ(), 0);
}

TYPED_TEST(SparseMArrayTest, Reserve) {
  SpDMatrix<real_t, SparseLayoutStride> mat(DShape<2>(10, 10));

  mat.Reserve(100);

  for (int i = 0; i < 10; ++i) {
    mat.Insert(i, i, static_cast<real_t>(i));
  }

  EXPECT_EQ(mat.GetNNZ(), 10);
}

TYPED_TEST(SparseMArrayTest, EmptyMatrix) {
  SpDMatrix<real_t, SparseLayoutStride> mat(DShape<2>(5, 5));

  std::vector<SparseTuple<real_t>> tuples;
  mat.SetFromTuples(tuples.begin(), tuples.end());

  EXPECT_EQ(mat.GetNNZ(), 0);
  EXPECT_REAL_EQ(mat.GetSparsity(), 0.0);
}

TYPED_TEST(SparseMArrayTest, EmptyCompressedMatrixCanAccumulate) {
  SpDMatrix<real_t, SparseLayoutStride> coo(DShape<2>(3, 3));
  auto empty = coo.ToLayout<DefaultSparseLayout>();

  SpDMatrix<real_t> diagonal(DShape<2>(3, 3));
  std::vector<SparseTuple<real_t>> tuples;
  tuples.emplace_back(UArray<int>{1, 1}, 2.0);
  diagonal.SetFromTuples(tuples.begin(), tuples.end());

  empty += diagonal;
  EXPECT_EQ(empty.GetNNZ(), 1);
  EXPECT_REAL_EQ(empty.At(1, 1), 2.0);
  EXPECT_REAL_EQ(empty.At(0, 0), 0.0);
}

TYPED_TEST(SparseMArrayTest, EmptyTensor) {
  SparseMArray<real_t, DShape<3>, SparseLayoutStride> tensor(
      DShape<3>(2, 3, 4));

  std::vector<SparseTuple<real_t>> tuples;
  tensor.SetFromTuples(tuples.begin(), tuples.end());

  EXPECT_EQ(tensor.GetNNZ(), 0);
  EXPECT_REAL_EQ(tensor.GetSparsity(), 0.0);
}

// ============================================================================
// Sparse Operations Tests
// ============================================================================

TYPED_TEST(SparseMArrayTest, Transpose_CSR) {
  // Create CSR matrix
  SpDMatrix<real_t, SparseLayoutRight> mat(DShape<2>(3, 4));
  std::vector<SparseTuple<real_t>> tuples;
  tuples.emplace_back(UArray<int>{0, 1}, 1.0);
  tuples.emplace_back(UArray<int>{1, 0}, 2.0);
  tuples.emplace_back(UArray<int>{1, 3}, 3.0);
  tuples.emplace_back(UArray<int>{2, 2}, 4.0);
  mat.SetFromTuples(tuples.begin(), tuples.end());

  // Transpose to CSC
  auto mat_t = mat.Transpose();

  EXPECT_EQ(mat_t.GetExtent(0), 4);  // Transposed dimensions
  EXPECT_EQ(mat_t.GetExtent(1), 3);
  EXPECT_EQ(mat_t.GetNNZ(), 4);

  // Verify that transposed positions contain correct values
  // Convert to COO for easy verification
  auto mat_t_coo = mat_t.template ToLayout<SparseLayoutStride>();
  const auto& values_t = mat_t_coo.GetValues();
  const real_t* val_data = values_t.HostRead();
  const auto& row_idx = mat_t_coo.GetMap().GetIndices(0);
  const auto& col_idx = mat_t_coo.GetMap().GetIndices(1);

  // Check that all expected transposed entries exist
  bool found_01 = false, found_10 = false, found_22 = false, found_31 = false;
  for (int k = 0; k < mat_t_coo.GetNNZ(); ++k) {
    if (row_idx[k] == 0 && col_idx[k] == 1) {
      EXPECT_REAL_EQ(val_data[k], 2.0);  // (1,0) → (0,1)
      found_01 = true;
    } else if (row_idx[k] == 1 && col_idx[k] == 0) {
      EXPECT_REAL_EQ(val_data[k], 1.0);  // (0,1) → (1,0)
      found_10 = true;
    } else if (row_idx[k] == 2 && col_idx[k] == 2) {
      EXPECT_REAL_EQ(val_data[k], 4.0);  // (2,2) → (2,2)
      found_22 = true;
    } else if (row_idx[k] == 3 && col_idx[k] == 1) {
      EXPECT_REAL_EQ(val_data[k], 3.0);  // (1,3) → (3,1)
      found_31 = true;
    }
  }
  EXPECT_TRUE(found_01 && found_10 && found_22 && found_31);
}

TYPED_TEST(SparseMArrayTest, Transpose_COO) {
  // Create COO matrix
  SpDMatrix<real_t, SparseLayoutStride> mat(DShape<2>(2, 3));
  mat.Insert(0, 1, 1.5);
  mat.Insert(1, 2, 2.5);

  auto mat_t = mat.Transpose();

  EXPECT_EQ(mat_t.GetExtent(0), 3);  // Transposed dimensions
  EXPECT_EQ(mat_t.GetExtent(1), 2);
  EXPECT_EQ(mat_t.GetNNZ(), 2);

  // Verify indices are swapped
  const auto& row_indices = mat_t.GetMap().GetIndices(0);
  const auto& col_indices = mat_t.GetMap().GetIndices(1);
  EXPECT_EQ(row_indices[0], 1);  // (0,1) → (1,0)
  EXPECT_EQ(col_indices[0], 0);
  EXPECT_EQ(row_indices[1], 2);  // (1,2) → (2,1)
  EXPECT_EQ(col_indices[1], 1);
}

TYPED_TEST(SparseMArrayTest, GetOuter_CSR) {
  // Create CSR matrix - GetOuter extracts rows
  SpDMatrix<real_t, SparseLayoutRight> mat(DShape<2>(3, 4));
  std::vector<SparseTuple<real_t>> tuples;
  tuples.emplace_back(UArray<int>{0, 1}, 1.0);
  tuples.emplace_back(UArray<int>{1, 0}, 2.0);
  tuples.emplace_back(UArray<int>{1, 3}, 3.0);
  tuples.emplace_back(UArray<int>{2, 2}, 4.0);
  mat.SetFromTuples(tuples.begin(), tuples.end());

  // Extract row 1 (outer dimension for CSR) - returns 1×4 row vector
  auto row1 = mat.GetOuter(1);

  EXPECT_EQ(row1.GetExtent(0), 1);  // Row dimension
  EXPECT_EQ(row1.GetExtent(1), 4);  // Column dimension
  EXPECT_EQ(row1.GetNNZ(), 2);      // Two non-zeros in row 1

  // Verify values
  const auto& values = row1.GetValues();
  const real_t* val_data = values.HostRead();
  EXPECT_REAL_EQ(val_data[0], 2.0);
  EXPECT_REAL_EQ(val_data[1], 3.0);
}

TYPED_TEST(SparseMArrayTest, GetOuter_CSC) {
  // Create CSC matrix - GetOuter extracts columns
  SpDMatrix<real_t, SparseLayoutLeft> mat(DShape<2>(3, 4));
  std::vector<SparseTuple<real_t>> tuples;
  tuples.emplace_back(UArray<int>{0, 1}, 1.0);
  tuples.emplace_back(UArray<int>{1, 1}, 2.0);
  tuples.emplace_back(UArray<int>{2, 2}, 3.0);
  mat.SetFromTuples(tuples.begin(), tuples.end());

  // Extract column 1 (outer dimension for CSC) - returns 3×1 column vector
  auto col1 = mat.GetOuter(1);

  EXPECT_EQ(col1.GetExtent(0), 3);  // Row dimension
  EXPECT_EQ(col1.GetExtent(1), 1);  // Column dimension
  EXPECT_EQ(col1.GetNNZ(), 2);      // Two non-zeros in column 1

  // Verify values
  const auto& values = col1.GetValues();
  const real_t* val_data = values.HostRead();
  EXPECT_REAL_EQ(val_data[0], 1.0);
  EXPECT_REAL_EQ(val_data[1], 2.0);
}

TYPED_TEST(SparseMArrayTest, GetDiagonal_CSR) {
  // Create CSR matrix with diagonal elements
  SpDMatrix<real_t, SparseLayoutRight> mat(DShape<2>(4, 4));
  std::vector<SparseTuple<real_t>> tuples;
  tuples.emplace_back(UArray<int>{0, 0}, 1.0);
  tuples.emplace_back(UArray<int>{1, 1}, 2.0);
  tuples.emplace_back(UArray<int>{2, 2}, 3.0);
  tuples.emplace_back(UArray<int>{0, 2}, 4.0);  // Off-diagonal
  mat.SetFromTuples(tuples.begin(), tuples.end());

  auto diag = mat.GetDiagonal();

  EXPECT_EQ(diag.GetSize(), 4);
  const real_t* diag_data = diag.HostRead();
  EXPECT_REAL_EQ(diag_data[0], 1.0);
  EXPECT_REAL_EQ(diag_data[1], 2.0);
  EXPECT_REAL_EQ(diag_data[2], 3.0);
  EXPECT_REAL_EQ(diag_data[3], 0.0);  // Missing diagonal element
}

TYPED_TEST(SparseMArrayTest, SetDiagonal_CSR) {
  // Create CSR matrix with diagonal elements
  SpDMatrix<real_t, SparseLayoutRight> mat(DShape<2>(3, 3));
  std::vector<SparseTuple<real_t>> tuples;
  tuples.emplace_back(UArray<int>{0, 0}, 1.0);
  tuples.emplace_back(UArray<int>{1, 1}, 2.0);
  tuples.emplace_back(UArray<int>{2, 2}, 3.0);
  mat.SetFromTuples(tuples.begin(), tuples.end());

  // Set new diagonal values
  UArray<real_t> new_diag(3);
  real_t* diag_data = new_diag.HostWrite();
  diag_data[0] = 10.0;
  diag_data[1] = 20.0;
  diag_data[2] = 30.0;

  mat.SetDiagonal(new_diag);

  // Verify updated values
  const auto& values = mat.GetValues();
  const real_t* val_data = values.HostRead();
  EXPECT_REAL_EQ(val_data[0], 10.0);
  EXPECT_REAL_EQ(val_data[1], 20.0);
  EXPECT_REAL_EQ(val_data[2], 30.0);
}

TYPED_TEST(SparseMArrayTest, OperatorPlusEquals) {
  // Create two CSR matrices
  SpDMatrix<real_t, SparseLayoutRight> mat1(DShape<2>(3, 3));
  SpDMatrix<real_t, SparseLayoutRight> mat2(DShape<2>(3, 3));

  std::vector<SparseTuple<real_t>> tuples1;
  tuples1.emplace_back(UArray<int>{0, 0}, 1.0);
  tuples1.emplace_back(UArray<int>{1, 1}, 2.0);
  mat1.SetFromTuples(tuples1.begin(), tuples1.end());

  std::vector<SparseTuple<real_t>> tuples2;
  tuples2.emplace_back(UArray<int>{0, 0}, 3.0);  // Overlapping
  tuples2.emplace_back(UArray<int>{2, 2}, 4.0);  // New position
  mat2.SetFromTuples(tuples2.begin(), tuples2.end());

  mat1 += mat2;

  EXPECT_EQ(mat1.GetNNZ(), 3);  // (0,0) merged, (1,1) and (2,2)

  // Verify the result by converting to COO for inspection
  auto result_coo = mat1.template ToLayout<SparseLayoutStride>();
  const auto& values = result_coo.GetValues();
  const real_t* val_data = values.HostRead();

  // The values should include: 4.0 at (0,0), 2.0 at (1,1), 4.0 at (2,2)
  bool found_00 = false, found_11 = false, found_22 = false;
  for (int k = 0; k < result_coo.GetNNZ(); ++k) {
    const int i = result_coo.GetMap().GetIndices(0)[k];
    const int j = result_coo.GetMap().GetIndices(1)[k];
    if (i == 0 && j == 0) {
      EXPECT_REAL_EQ(val_data[k], 4.0);
      found_00 = true;
    } else if (i == 1 && j == 1) {
      EXPECT_REAL_EQ(val_data[k], 2.0);
      found_11 = true;
    } else if (i == 2 && j == 2) {
      EXPECT_REAL_EQ(val_data[k], 4.0);
      found_22 = true;
    }
  }
  EXPECT_TRUE(found_00 && found_11 && found_22);
}

TYPED_TEST(SparseMArrayTest, OperatorMinusEquals) {
  // Create two COO matrices
  SpDMatrix<real_t, SparseLayoutStride> mat1(DShape<2>(3, 3));
  SpDMatrix<real_t, SparseLayoutStride> mat2(DShape<2>(3, 3));

  mat1.Insert(0, 0, 5.0);
  mat1.Insert(1, 1, 3.0);
  mat1.Finalize();

  mat2.Insert(0, 0, 2.0);
  mat2.Insert(2, 2, 1.0);
  mat2.Finalize();

  mat1 -= mat2;

  EXPECT_EQ(mat1.GetNNZ(), 3);  // (0,0): 3.0, (1,1): 3.0, (2,2): -1.0

  const auto& values = mat1.GetValues();
  const real_t* val_data = values.HostRead();

  bool found_00 = false, found_11 = false, found_22 = false;
  for (int k = 0; k < mat1.GetNNZ(); ++k) {
    const int i = mat1.GetMap().GetIndices(0)[k];
    const int j = mat1.GetMap().GetIndices(1)[k];
    if (i == 0 && j == 0) {
      EXPECT_REAL_EQ(val_data[k], 3.0);
      found_00 = true;
    } else if (i == 1 && j == 1) {
      EXPECT_REAL_EQ(val_data[k], 3.0);
      found_11 = true;
    } else if (i == 2 && j == 2) {
      EXPECT_REAL_EQ(val_data[k], -1.0);
      found_22 = true;
    }
  }
  EXPECT_TRUE(found_00 && found_11 && found_22);
}

TYPED_TEST(SparseMArrayTest, OperatorTimesEquals) {
  // Create two CSR matrices
  SpDMatrix<real_t, SparseLayoutRight> mat1(DShape<2>(3, 3));
  SpDMatrix<real_t, SparseLayoutRight> mat2(DShape<2>(3, 3));

  std::vector<SparseTuple<real_t>> tuples1;
  tuples1.emplace_back(UArray<int>{0, 0}, 2.0);
  tuples1.emplace_back(UArray<int>{1, 1}, 3.0);
  tuples1.emplace_back(UArray<int>{2, 2}, 4.0);
  mat1.SetFromTuples(tuples1.begin(), tuples1.end());

  std::vector<SparseTuple<real_t>> tuples2;
  tuples2.emplace_back(UArray<int>{0, 0}, 5.0);  // Overlapping
  tuples2.emplace_back(UArray<int>{1, 2}, 6.0);  // Non-overlapping
  mat2.SetFromTuples(tuples2.begin(), tuples2.end());

  mat1 *= mat2;

  // Only (0,0) overlaps, so result should have 1 non-zero
  EXPECT_EQ(mat1.GetNNZ(), 1);

  auto result_coo = mat1.template ToLayout<SparseLayoutStride>();
  const auto& values = result_coo.GetValues();
  const real_t* val_data = values.HostRead();

  EXPECT_EQ(result_coo.GetMap().GetIndices(0)[0], 0);
  EXPECT_EQ(result_coo.GetMap().GetIndices(1)[0], 0);
  EXPECT_REAL_EQ(val_data[0], 10.0);  // 2.0 * 5.0
}

TYPED_TEST(SparseMArrayTest, ScalarOperators) {
  // Test scalar compound assignment operators
  SpDMatrix<real_t, SparseLayoutStride> mat(DShape<2>(2, 2));
  mat.Insert(0, 0, 2.0);
  mat.Insert(1, 1, 4.0);
  mat.Finalize();

  // Test +=
  mat += 1.0;
  const auto& values_add = mat.GetValues();
  EXPECT_REAL_EQ(values_add.HostRead()[0], 3.0);
  EXPECT_REAL_EQ(values_add.HostRead()[1], 5.0);

  // Test *=
  mat *= 2.0;
  const auto& values_mul = mat.GetValues();
  EXPECT_REAL_EQ(values_mul.HostRead()[0], 6.0);
  EXPECT_REAL_EQ(values_mul.HostRead()[1], 10.0);

  // Test -=
  mat -= 1.0;
  const auto& values_sub = mat.GetValues();
  EXPECT_REAL_EQ(values_sub.HostRead()[0], 5.0);
  EXPECT_REAL_EQ(values_sub.HostRead()[1], 9.0);

  // Test /=
  mat /= 3.0;
  const auto& values_div = mat.GetValues();
  EXPECT_REAL_EQ(values_div.HostRead()[0], 5.0 / 3.0);
  EXPECT_REAL_EQ(values_div.HostRead()[1], 3.0);
}

TYPED_TEST(SparseMArrayTest, UnaryNegation) {
  SpDMatrix<real_t, SparseLayoutStride> mat(DShape<2>(2, 2));
  mat.Insert(0, 0, 2.0);
  mat.Insert(1, 1, -4.0);
  mat.Finalize();

  auto neg_mat = -mat;

  EXPECT_EQ(neg_mat.GetNNZ(), 2);
  const auto& values = neg_mat.GetValues();
  EXPECT_REAL_EQ(values.HostRead()[0], -2.0);
  EXPECT_REAL_EQ(values.HostRead()[1], 4.0);
}

// ============================================================================
// Type Alias Tests
// ============================================================================

TYPED_TEST(SparseMArrayTest, SpDMatrix_TypeAlias) {
  SpDMatrix<real_t> mat(DShape<2>(3, 4));

  EXPECT_EQ(mat.GetRank(), 2);
  EXPECT_EQ(mat.GetExtent(0), 3);
  EXPECT_EQ(mat.GetExtent(1), 4);
}

TYPED_TEST(SparseMArrayTest, SpDTensor_TypeAlias) {
  SpDTensor<real_t, 3> tensor(DShape<3>(2, 3, 4));

  EXPECT_EQ(tensor.GetRank(), 3);
  EXPECT_EQ(tensor.GetExtent(0), 2);
  EXPECT_EQ(tensor.GetExtent(1), 3);
  EXPECT_EQ(tensor.GetExtent(2), 4);
}

TYPED_TEST(SparseMArrayTest, SpSMatrix_TypeAlias) {
  SpSMatrix<real_t, 3, 4> mat;

  EXPECT_EQ(mat.GetRank(), 2);
  EXPECT_EQ(mat.GetExtent(0), 3);
  EXPECT_EQ(mat.GetExtent(1), 4);
}

TYPED_TEST(SparseMArrayTest, SliceOuter_CSR) {
  // Create CSR matrix (5×4)
  SparseMArray<double, DShape<2>, SparseLayoutRight> mat(DShape<2>(5, 4));
  std::vector<SparseTuple<double>> tuples;
  tuples.emplace_back(UArray<int>{0, 1}, 1.0);
  tuples.emplace_back(UArray<int>{1, 0}, 2.0);
  tuples.emplace_back(UArray<int>{1, 3}, 3.0);
  tuples.emplace_back(UArray<int>{2, 2}, 4.0);
  tuples.emplace_back(UArray<int>{3, 1}, 5.0);
  tuples.emplace_back(UArray<int>{4, 0}, 6.0);
  mat.SetFromTuples(tuples.begin(), tuples.end());

  // Extract rows 1-3 (rows 1 and 2)
  auto submat = mat.SliceOuter(1, 3);

  EXPECT_EQ(submat.GetExtent(0), 2);  // 2 rows
  EXPECT_EQ(submat.GetExtent(1), 4);  // Same number of columns
  EXPECT_EQ(submat.GetNNZ(), 3);      // Elements from rows 1 and 2

  // Verify elements are adjusted to new coordinate system
  const auto& row_indices = submat.GetMap().GetIndices(0);
  const auto& col_indices = submat.GetMap().GetIndices(1);
  const auto& values = submat.GetValues();
  const double* val_data = values.HostRead();

  // Check that indices are adjusted (row 1 becomes row 0, row 2 becomes row 1)
  bool found_00 = false, found_03 = false, found_12 = false;
  for (int k = 0; k < submat.GetNNZ(); ++k) {
    if (row_indices[k] == 0 && col_indices[k] == 0) {
      EXPECT_REAL_EQ(val_data[k], 2.0);  // Was (1,0) → (0,0)
      found_00 = true;
    } else if (row_indices[k] == 0 && col_indices[k] == 3) {
      EXPECT_REAL_EQ(val_data[k], 3.0);  // Was (1,3) → (0,3)
      found_03 = true;
    } else if (row_indices[k] == 1 && col_indices[k] == 2) {
      EXPECT_REAL_EQ(val_data[k], 4.0);  // Was (2,2) → (1,2)
      found_12 = true;
    }
  }
  EXPECT_TRUE(found_00 && found_03 && found_12);
}

TYPED_TEST(SparseMArrayTest, SliceOuter_CSC) {
  // Create CSC matrix (4×5)
  SparseMArray<double, DShape<2>, SparseLayoutLeft> mat(DShape<2>(4, 5));
  std::vector<SparseTuple<double>> tuples;
  tuples.emplace_back(UArray<int>{0, 0}, 1.0);
  tuples.emplace_back(UArray<int>{1, 1}, 2.0);
  tuples.emplace_back(UArray<int>{2, 2}, 3.0);
  tuples.emplace_back(UArray<int>{0, 3}, 4.0);
  tuples.emplace_back(UArray<int>{3, 4}, 5.0);
  mat.SetFromTuples(tuples.begin(), tuples.end());

  // Extract columns 1-4 (columns 1, 2, 3)
  auto submat = mat.SliceOuter(1, 4);

  EXPECT_EQ(submat.GetExtent(0), 4);  // Same number of rows
  EXPECT_EQ(submat.GetExtent(1), 3);  // 3 columns
  EXPECT_EQ(submat.GetNNZ(), 3);      // Elements from columns 1, 2, 3

  // Verify column indices are adjusted
  const auto& row_indices = submat.GetMap().GetIndices(0);
  const auto& col_indices = submat.GetMap().GetIndices(1);
  const auto& values = submat.GetValues();
  const double* val_data = values.HostRead();

  bool found_10 = false, found_21 = false, found_02 = false;
  for (int k = 0; k < submat.GetNNZ(); ++k) {
    if (row_indices[k] == 1 && col_indices[k] == 0) {
      EXPECT_REAL_EQ(val_data[k], 2.0);  // Was (1,1) → (1,0)
      found_10 = true;
    } else if (row_indices[k] == 2 && col_indices[k] == 1) {
      EXPECT_REAL_EQ(val_data[k], 3.0);  // Was (2,2) → (2,1)
      found_21 = true;
    } else if (row_indices[k] == 0 && col_indices[k] == 2) {
      EXPECT_REAL_EQ(val_data[k], 4.0);  // Was (0,3) → (0,2)
      found_02 = true;
    }
  }
  EXPECT_TRUE(found_10 && found_21 && found_02);
}

TYPED_TEST(SparseMArrayTest, Slice_2D) {
  // Create COO matrix (5×5)
  SparseMArray<double, DShape<2>, SparseLayoutStride> mat(DShape<2>(5, 5));
  mat.Insert(0, 0, 1.0);
  mat.Insert(1, 1, 2.0);
  mat.Insert(2, 2, 3.0);
  mat.Insert(3, 3, 4.0);
  mat.Insert(4, 4, 5.0);
  mat.Insert(1, 3, 6.0);
  mat.Insert(3, 1, 7.0);
  mat.Finalize();

  // Slice rows [1,4) and columns [1,4) - extract central 3×3 block
  UArray<int> ranges(4);
  int* range_data = ranges.HostWrite();
  range_data[0] = 1;  // row start
  range_data[1] = 4;  // row end
  range_data[2] = 1;  // col start
  range_data[3] = 4;  // col end

  auto submat = mat.Slice(ranges);

  EXPECT_EQ(submat.GetExtent(0), 3);  // 3 rows
  EXPECT_EQ(submat.GetExtent(1), 3);  // 3 columns
  EXPECT_EQ(submat.GetNNZ(), 5);      // (1,1), (2,2), (3,3), (1,3), (3,1)

  // Verify adjusted indices
  const auto& row_indices = submat.GetMap().GetIndices(0);
  const auto& col_indices = submat.GetMap().GetIndices(1);
  const auto& values = submat.GetValues();
  const double* val_data = values.HostRead();

  bool found_00 = false, found_11 = false, found_22 = false, found_02 = false,
       found_20 = false;
  for (int k = 0; k < submat.GetNNZ(); ++k) {
    if (row_indices[k] == 0 && col_indices[k] == 0) {
      EXPECT_REAL_EQ(val_data[k], 2.0);  // Was (1,1) → (0,0)
      found_00 = true;
    } else if (row_indices[k] == 1 && col_indices[k] == 1) {
      EXPECT_REAL_EQ(val_data[k], 3.0);  // Was (2,2) → (1,1)
      found_11 = true;
    } else if (row_indices[k] == 2 && col_indices[k] == 2) {
      EXPECT_REAL_EQ(val_data[k], 4.0);  // Was (3,3) → (2,2)
      found_22 = true;
    } else if (row_indices[k] == 0 && col_indices[k] == 2) {
      EXPECT_REAL_EQ(val_data[k], 6.0);  // Was (1,3) → (0,2)
      found_02 = true;
    } else if (row_indices[k] == 2 && col_indices[k] == 0) {
      EXPECT_REAL_EQ(val_data[k], 7.0);  // Was (3,1) → (2,0)
      found_20 = true;
    }
  }
  EXPECT_TRUE(found_00 && found_11 && found_22 && found_02 && found_20);
}

TYPED_TEST(SparseMArrayTest, Slice_3D) {
  // Create 3D COO tensor (4×4×4)
  SparseMArray<double, DShape<3>, SparseLayoutStride> tensor(
      DShape<3>(4, 4, 4));
  tensor.Insert(UArray<int>{0, 0, 0}, 1.0);
  tensor.Insert(UArray<int>{1, 1, 1}, 2.0);
  tensor.Insert(UArray<int>{2, 2, 2}, 3.0);
  tensor.Insert(UArray<int>{3, 3, 3}, 4.0);
  tensor.Insert(UArray<int>{1, 2, 3}, 5.0);
  tensor.Finalize();

  // Slice [1,3) × [1,4) × [0,3)
  UArray<int> ranges(6);
  int* range_data = ranges.HostWrite();
  range_data[0] = 1;  // dim 0 start
  range_data[1] = 3;  // dim 0 end
  range_data[2] = 1;  // dim 1 start
  range_data[3] = 4;  // dim 1 end
  range_data[4] = 0;  // dim 2 start
  range_data[5] = 3;  // dim 2 end

  auto subtensor = tensor.Slice(ranges);

  EXPECT_EQ(subtensor.GetExtent(0), 2);  // 2 in dim 0
  EXPECT_EQ(subtensor.GetExtent(1), 3);  // 3 in dim 1
  EXPECT_EQ(subtensor.GetExtent(2), 3);  // 3 in dim 2
  EXPECT_EQ(subtensor.GetNNZ(), 2);      // (1,1,1) and (2,2,2)

  // Verify adjusted indices
  const auto& idx0 = subtensor.GetMap().GetIndices(0);
  const auto& idx1 = subtensor.GetMap().GetIndices(1);
  const auto& idx2 = subtensor.GetMap().GetIndices(2);
  const auto& values = subtensor.GetValues();
  const double* val_data = values.HostRead();

  bool found_001 = false, found_112 = false;
  for (int k = 0; k < subtensor.GetNNZ(); ++k) {
    if (idx0[k] == 0 && idx1[k] == 0 && idx2[k] == 1) {
      EXPECT_REAL_EQ(val_data[k], 2.0);  // Was (1,1,1) → (0,0,1)
      found_001 = true;
    } else if (idx0[k] == 1 && idx1[k] == 1 && idx2[k] == 2) {
      EXPECT_REAL_EQ(val_data[k], 3.0);  // Was (2,2,2) → (1,1,2)
      found_112 = true;
    }
  }
  EXPECT_TRUE(found_001 && found_112);
}

// ============================================================================
// Element Access Tests (At, Contains)
// ============================================================================

TYPED_TEST(SparseMArrayTest, COO_At) {
  SpDMatrix<real_t, SparseLayoutStride> mat(DShape<2>(5, 5));

  mat.Insert(0, 0, 1.0);
  mat.Insert(1, 2, 3.5);
  mat.Insert(3, 4, -2.0);
  mat.Finalize();

  // Test existing elements
  EXPECT_REAL_EQ(mat.At(0, 0), 1.0);
  EXPECT_REAL_EQ(mat.At(1, 2), 3.5);
  EXPECT_REAL_EQ(mat.At(3, 4), -2.0);

  // Test non-existing elements (should return 0)
  EXPECT_REAL_EQ(mat.At(0, 1), 0.0);
  EXPECT_REAL_EQ(mat.At(2, 2), 0.0);
  EXPECT_REAL_EQ(mat.At(4, 4), 0.0);
}

TYPED_TEST(SparseMArrayTest, CSR_At) {
  SpDMatrix<real_t, SparseLayoutStride> coo(DShape<2>(4, 4));
  coo.Insert(0, 1, 5.0);
  coo.Insert(1, 1, 7.0);
  coo.Insert(2, 3, 9.0);
  coo.Finalize();

  auto csr = coo.ToLayout<SparseLayoutRight>();

  EXPECT_REAL_EQ(csr.At(0, 1), 5.0);
  EXPECT_REAL_EQ(csr.At(1, 1), 7.0);
  EXPECT_REAL_EQ(csr.At(2, 3), 9.0);
  EXPECT_REAL_EQ(csr.At(0, 0), 0.0);
  EXPECT_REAL_EQ(csr.At(3, 3), 0.0);
}

TYPED_TEST(SparseMArrayTest, CSC_At) {
  SpDMatrix<real_t, SparseLayoutStride> coo(DShape<2>(4, 4));
  coo.Insert(0, 1, 5.0);
  coo.Insert(1, 1, 7.0);
  coo.Insert(2, 3, 9.0);
  coo.Finalize();

  auto csc = coo.ToLayout<SparseLayoutLeft>();

  EXPECT_REAL_EQ(csc.At(0, 1), 5.0);
  EXPECT_REAL_EQ(csc.At(1, 1), 7.0);
  EXPECT_REAL_EQ(csc.At(2, 3), 9.0);
  EXPECT_REAL_EQ(csc.At(0, 0), 0.0);
  EXPECT_REAL_EQ(csc.At(3, 3), 0.0);
}

TYPED_TEST(SparseMArrayTest, COO_Contains) {
  SpDMatrix<real_t, SparseLayoutStride> mat(DShape<2>(5, 5));

  mat.Insert(0, 0, 1.0);
  mat.Insert(1, 2, 3.5);
  mat.Insert(3, 4, -2.0);
  mat.Finalize();

  // Test existing elements
  EXPECT_TRUE(mat.Contains(0, 0));
  EXPECT_TRUE(mat.Contains(1, 2));
  EXPECT_TRUE(mat.Contains(3, 4));

  // Test non-existing elements
  EXPECT_FALSE(mat.Contains(0, 1));
  EXPECT_FALSE(mat.Contains(2, 2));
  EXPECT_FALSE(mat.Contains(4, 4));
}

TYPED_TEST(SparseMArrayTest, CSR_Contains) {
  SpDMatrix<real_t, SparseLayoutStride> coo(DShape<2>(4, 4));
  coo.Insert(0, 1, 5.0);
  coo.Insert(1, 1, 7.0);
  coo.Insert(2, 3, 9.0);
  coo.Finalize();

  auto csr = coo.ToLayout<SparseLayoutRight>();

  EXPECT_TRUE(csr.Contains(0, 1));
  EXPECT_TRUE(csr.Contains(1, 1));
  EXPECT_TRUE(csr.Contains(2, 3));
  EXPECT_FALSE(csr.Contains(0, 0));
  EXPECT_FALSE(csr.Contains(3, 3));
}

// ============================================================================
// Reduction Operations Tests (Sum, Max, Min, Norm)
// ============================================================================

TYPED_TEST(SparseMArrayTest, COO_Sum) {
  SpDMatrix<real_t, SparseLayoutStride> mat(DShape<2>(5, 5));

  mat.Insert(0, 0, 1.0);
  mat.Insert(1, 2, 2.0);
  mat.Insert(2, 1, 3.0);
  mat.Insert(3, 4, 4.0);
  mat.Finalize();

  EXPECT_REAL_EQ(mat.Sum(), 10.0);
}

TYPED_TEST(SparseMArrayTest, CSR_Sum) {
  SpDMatrix<real_t, SparseLayoutStride> coo(DShape<2>(4, 4));
  coo.Insert(0, 0, 1.5);
  coo.Insert(1, 1, 2.5);
  coo.Insert(2, 2, 3.5);
  coo.Finalize();

  auto csr = coo.ToLayout<SparseLayoutRight>();
  EXPECT_REAL_EQ(csr.Sum(), 7.5);
}

TYPED_TEST(SparseMArrayTest, COO_Max) {
  SpDMatrix<real_t, SparseLayoutStride> mat(DShape<2>(5, 5));

  mat.Insert(0, 0, 1.0);
  mat.Insert(1, 2, -5.0);  // Absolute value: 5.0
  mat.Insert(2, 1, 3.0);
  mat.Insert(3, 4, 2.0);
  mat.Finalize();

  EXPECT_REAL_EQ(mat.Max(), 5.0);  // Max absolute value
}

TYPED_TEST(SparseMArrayTest, COO_Min) {
  SpDMatrix<real_t, SparseLayoutStride> mat(DShape<2>(5, 5));

  mat.Insert(0, 0, 10.0);
  mat.Insert(1, 2, -5.0);
  mat.Insert(2, 1, 0.5);  // Min absolute value
  mat.Insert(3, 4, -2.0);
  mat.Finalize();

  EXPECT_REAL_EQ(mat.Min(), 0.5);
}

TYPED_TEST(SparseMArrayTest, COO_NormL1) {
  SpDMatrix<real_t, SparseLayoutStride> mat(DShape<2>(5, 5));

  mat.Insert(0, 0, 1.0);
  mat.Insert(1, 2, -2.0);
  mat.Insert(2, 1, 3.0);
  mat.Finalize();

  EXPECT_REAL_EQ(mat.Norm(1), 6.0);  // |1| + |-2| + |3|
}

TYPED_TEST(SparseMArrayTest, COO_NormL2) {
  SpDMatrix<real_t, SparseLayoutStride> mat(DShape<2>(5, 5));

  mat.Insert(0, 0, 3.0);
  mat.Insert(1, 2, 4.0);
  mat.Finalize();

  EXPECT_REAL_EQ(mat.Norm(2), 5.0);  // sqrt(3^2 + 4^2) = sqrt(25) = 5
}

TYPED_TEST(SparseMArrayTest, CSR_Norm) {
  SpDMatrix<real_t, SparseLayoutStride> coo(DShape<2>(4, 4));
  coo.Insert(0, 0, 1.0);
  coo.Insert(1, 1, 2.0);
  coo.Insert(2, 2, 2.0);
  coo.Finalize();

  auto csr = coo.ToLayout<SparseLayoutRight>();
  EXPECT_REAL_EQ(csr.Norm(2), 3.0);  // sqrt(1 + 4 + 4) = sqrt(9) = 3
}

// ============================================================================
// Utility Functions Tests (Fill, Compact, ShrinkToFit)
// ============================================================================

TYPED_TEST(SparseMArrayTest, COO_Fill) {
  SpDMatrix<real_t, SparseLayoutStride> mat(DShape<2>(5, 5));

  mat.Insert(0, 0, 1.0);
  mat.Insert(1, 2, 2.0);
  mat.Insert(2, 1, 3.0);
  mat.Finalize();

  mat.Fill(7.0);

  const auto& values = mat.GetValues();
  const real_t* val_data = values.Read(values.UseDevice());

  for (int k = 0; k < mat.GetNNZ(); ++k) {
    EXPECT_REAL_EQ(val_data[k], 7.0);
  }
}

TYPED_TEST(SparseMArrayTest, CSR_Fill) {
  SpDMatrix<real_t, SparseLayoutStride> coo(DShape<2>(4, 4));
  coo.Insert(0, 0, 1.0);
  coo.Insert(1, 1, 2.0);
  coo.Insert(2, 2, 3.0);
  coo.Finalize();

  auto csr = coo.ToLayout<SparseLayoutRight>();
  csr.Fill(9.5);

  const auto& values = csr.GetValues();
  const real_t* val_data = values.Read(values.UseDevice());

  for (int k = 0; k < csr.GetNNZ(); ++k) {
    EXPECT_REAL_EQ(val_data[k], 9.5);
  }
}

TYPED_TEST(SparseMArrayTest, COO_Compact) {
  SpDMatrix<real_t, SparseLayoutStride> mat(DShape<2>(5, 5));

  mat.Insert(0, 0, 1.0);
  mat.Insert(1, 2, 0.0);  // Explicit zero
  mat.Insert(2, 1, 3.0);
  mat.Insert(3, 3, 0.0);  // Explicit zero
  mat.Insert(4, 4, 5.0);
  mat.Finalize();

  // Note: Finalize() already removes zeros, so we have 3 non-zeros
  EXPECT_EQ(mat.GetNNZ(), 3);

  // Add explicit zeros after finalization to test Compact
  auto& values = const_cast<UArray<real_t>&>(mat.GetValues());
  real_t* val_data = values.HostWrite();  // Always use host for test simplicity
  val_data[1] = 0.0;                      // Set second element to zero

  mat.Compact();

  EXPECT_EQ(mat.GetNNZ(), 2);  // Only non-zeros remain

  // Verify remaining values
  const auto& final_values = mat.GetValues();
  const real_t* final_val_data = final_values.HostRead();

  for (int k = 0; k < mat.GetNNZ(); ++k) {
    EXPECT_NE(final_val_data[k], 0.0);
  }
}

TYPED_TEST(SparseMArrayTest, CSR_Compact) {
  // Create a matrix with non-zeros
  SpDMatrix<real_t, SparseLayoutStride> coo(DShape<2>(4, 4));
  coo.Insert(0, 0, 1.0);
  coo.Insert(1, 1, 2.0);
  coo.Insert(2, 2, 3.0);
  coo.Finalize();

  auto csr = coo.ToLayout<SparseLayoutRight>();
  const int initial_nnz = csr.GetNNZ();
  EXPECT_EQ(initial_nnz, 3);

  // Compact should have no effect (no zeros)
  csr.Compact();

  EXPECT_EQ(csr.GetNNZ(), 3);

  // Verify values are preserved
  const auto& values = csr.GetValues();
  const real_t* val_data = values.HostRead();
  EXPECT_REAL_EQ(val_data[0], 1.0);
  EXPECT_REAL_EQ(val_data[1], 2.0);
  EXPECT_REAL_EQ(val_data[2], 3.0);
}

TYPED_TEST(SparseMArrayTest, CSC_Compact) {
  // Create a matrix with non-zeros
  SpDMatrix<real_t, SparseLayoutStride> coo(DShape<2>(4, 4));
  coo.Insert(0, 0, 1.0);
  coo.Insert(1, 1, 2.0);
  coo.Insert(2, 2, 3.0);
  coo.Finalize();

  auto csc = coo.ToLayout<SparseLayoutLeft>();
  const int initial_nnz = csc.GetNNZ();
  EXPECT_EQ(initial_nnz, 3);

  // Compact should have no effect (no zeros)
  csc.Compact();

  EXPECT_EQ(csc.GetNNZ(), 3);

  // Verify values are preserved
  const auto& values = csc.GetValues();
  const real_t* val_data = values.HostRead();
  EXPECT_REAL_EQ(val_data[0], 1.0);
  EXPECT_REAL_EQ(val_data[1], 2.0);
  EXPECT_REAL_EQ(val_data[2], 3.0);
}

TYPED_TEST(SparseMArrayTest, ShrinkToFit) {
  SpDMatrix<real_t, SparseLayoutStride> mat(DShape<2>(5, 5), 100);

  mat.Insert(0, 0, 1.0);
  mat.Insert(1, 2, 2.0);
  mat.Insert(2, 1, 3.0);
  mat.Finalize();

  EXPECT_EQ(mat.GetNNZ(), 3);

  mat.ShrinkToFit();

  // Verify capacity matches actual size
  const auto& values = mat.GetValues();
  EXPECT_EQ(values.GetSize(), 3);
}

// ==========================================================================
// SparseMIndex Tests
// ==========================================================================

TYPED_TEST(SparseMArrayTest, SparseMIndex_COO) {
  SpDMatrix<real_t, SparseLayoutStride> mat(DShape<2>(5, 5));

  mat.Insert(0, 0, 1.0);
  mat.Insert(1, 2, 3.5);
  mat.Insert(3, 4, -2.0);
  mat.Finalize();

  const auto& map = mat.GetMap();
  SparseMIndex<DShape<2>, SparseLayoutStride> idx(map);

  EXPECT_EQ(idx.GetNNZ(), 3);
  EXPECT_EQ(idx.GetNNZOffset(), 0);

  // Test SetNNZOffset and coordinate retrieval
  idx.SetNNZOffset(0);
  EXPECT_EQ(idx[0], 0);
  EXPECT_EQ(idx[1], 0);

  idx.SetNNZOffset(1);
  EXPECT_EQ(idx[0], 1);
  EXPECT_EQ(idx[1], 2);

  idx.SetNNZOffset(2);
  EXPECT_EQ(idx[0], 3);
  EXPECT_EQ(idx[1], 4);

  // Test SetMultiIndex (search for element)
  bool found = idx.SetMultiIndex(1, 2);
  EXPECT_TRUE(found);
  EXPECT_EQ(idx.GetNNZOffset(), 1);

  found = idx.SetMultiIndex(2, 2);
  EXPECT_FALSE(found);
}

TYPED_TEST(SparseMArrayTest, SparseMIndex_CSR) {
  SpDMatrix<real_t, SparseLayoutStride> coo(DShape<2>(4, 4));
  coo.Insert(0, 1, 5.0);
  coo.Insert(1, 1, 7.0);
  coo.Insert(2, 3, 9.0);
  coo.Finalize();

  auto csr = coo.ToLayout<SparseLayoutRight>();
  const auto& map = csr.GetMap();
  SparseMIndex<DShape<2>, SparseLayoutRight> idx(map);

  EXPECT_EQ(idx.GetNNZ(), 3);

  // Test coordinate retrieval
  idx.SetNNZOffset(0);
  EXPECT_EQ(idx[0], 0);
  EXPECT_EQ(idx[1], 1);

  idx.SetNNZOffset(1);
  EXPECT_EQ(idx[0], 1);
  EXPECT_EQ(idx[1], 1);

  idx.SetNNZOffset(2);
  EXPECT_EQ(idx[0], 2);
  EXPECT_EQ(idx[1], 3);

  // Test SetMultiIndex
  bool found = idx.SetMultiIndex(1, 1);
  EXPECT_TRUE(found);
  EXPECT_EQ(idx.GetNNZOffset(), 1);

  found = idx.SetMultiIndex(3, 3);
  EXPECT_FALSE(found);
}

TYPED_TEST(SparseMArrayTest, SparseMIndex_CSC) {
  SpDMatrix<real_t, SparseLayoutStride> coo(DShape<2>(4, 4));
  coo.Insert(0, 1, 5.0);
  coo.Insert(1, 1, 7.0);
  coo.Insert(2, 3, 9.0);
  coo.Finalize();

  auto csc = coo.ToLayout<SparseLayoutLeft>();
  const auto& map = csc.GetMap();
  SparseMIndex<DShape<2>, SparseLayoutLeft> idx(map);

  EXPECT_EQ(idx.GetNNZ(), 3);

  // Test coordinate retrieval
  idx.SetNNZOffset(0);
  EXPECT_EQ(idx[0], 0);
  EXPECT_EQ(idx[1], 1);

  idx.SetNNZOffset(1);
  EXPECT_EQ(idx[0], 1);
  EXPECT_EQ(idx[1], 1);

  idx.SetNNZOffset(2);
  EXPECT_EQ(idx[0], 2);
  EXPECT_EQ(idx[1], 3);

  // Test SetMultiIndex
  bool found = idx.SetMultiIndex(2, 3);
  EXPECT_TRUE(found);
  EXPECT_EQ(idx.GetNNZOffset(), 2);
}

// ==========================================================================
// SparseMIterator Tests
// ==========================================================================

TYPED_TEST(SparseMArrayTest, SparseMIterator_COO) {
  SpDMatrix<real_t, SparseLayoutStride> mat(DShape<2>(5, 5));

  mat.Insert(0, 0, 1.0);
  mat.Insert(1, 2, 3.5);
  mat.Insert(3, 4, -2.0);
  mat.Finalize();

  const auto& map = mat.GetMap();
  SparseMIterator<DShape<2>, SparseLayoutStride> iter(map);

  EXPECT_FALSE(iter.IsExhausted());
  EXPECT_EQ(iter.GetLimit(), 3);

  // Iterate and verify coordinates
  int count = 0;
  const int expected_coords[3][2] = {{0, 0}, {1, 2}, {3, 4}};

  while (!iter.IsExhausted()) {
    const auto& idx = iter.GetIndex();
    EXPECT_EQ(idx[0], expected_coords[count][0]);
    EXPECT_EQ(idx[1], expected_coords[count][1]);
    EXPECT_EQ(iter.GetNNZOffset(), count);
    ++iter;
    count++;
  }

  EXPECT_EQ(count, 3);
  EXPECT_TRUE(iter.IsExhausted());

  // Test Reset
  iter.Reset();
  EXPECT_FALSE(iter.IsExhausted());
  EXPECT_EQ(iter.GetNNZOffset(), 0);
}

TYPED_TEST(SparseMArrayTest, SparseMIterator_CSR) {
  SpDMatrix<real_t, SparseLayoutStride> coo(DShape<2>(4, 4));
  coo.Insert(0, 1, 5.0);
  coo.Insert(1, 1, 7.0);
  coo.Insert(2, 3, 9.0);
  coo.Finalize();

  auto csr = coo.ToLayout<SparseLayoutRight>();
  const auto& map = csr.GetMap();
  SparseMIterator<DShape<2>, SparseLayoutRight> iter(map);

  EXPECT_EQ(iter.GetLimit(), 3);

  // Iterate and verify coordinates
  int count = 0;
  const int expected_coords[3][2] = {{0, 1}, {1, 1}, {2, 3}};

  while (!iter.IsExhausted()) {
    const auto& idx = iter.GetIndex();
    EXPECT_EQ(idx[0], expected_coords[count][0]);
    EXPECT_EQ(idx[1], expected_coords[count][1]);
    ++iter;
    count++;
  }

  EXPECT_EQ(count, 3);
}

TYPED_TEST(SparseMArrayTest, SparseMIterator_CSC) {
  SpDMatrix<real_t, SparseLayoutStride> coo(DShape<2>(4, 4));
  coo.Insert(0, 1, 5.0);
  coo.Insert(1, 1, 7.0);
  coo.Insert(2, 3, 9.0);
  coo.Finalize();

  auto csc = coo.ToLayout<SparseLayoutLeft>();
  const auto& map = csc.GetMap();
  SparseMIterator<DShape<2>, SparseLayoutLeft> iter(map);

  EXPECT_EQ(iter.GetLimit(), 3);

  // CSC stores elements column-by-column
  // Within column 1: rows may be in any order
  // So we just verify that we can iterate over all elements
  // and that the coordinates are valid
  int count = 0;
  std::vector<std::pair<int, int>> coords;

  while (!iter.IsExhausted()) {
    const auto& idx = iter.GetIndex();
    coords.push_back({idx[0], idx[1]});
    ++iter;
    count++;
  }

  EXPECT_EQ(count, 3);

  // Verify all expected elements are present
  auto has_coord = [&coords](int i, int j) {
    for (const auto& c : coords) {
      if (c.first == i && c.second == j) return true;
    }
    return false;
  };

  EXPECT_TRUE(has_coord(0, 1));
  EXPECT_TRUE(has_coord(1, 1));
  EXPECT_TRUE(has_coord(2, 3));
}

TYPED_TEST(SparseMArrayTest, SparseMIterator_RangeFor) {
  SpDMatrix<real_t, SparseLayoutStride> mat(DShape<2>(5, 5));

  mat.Insert(0, 0, 1.0);
  mat.Insert(1, 2, 3.5);
  mat.Insert(3, 4, -2.0);
  mat.Finalize();

  const auto& map = mat.GetMap();
  SparseMIterator<DShape<2>, SparseLayoutStride> iter(map);

  // Test range-based for loop
  int count = 0;
  const int expected_coords[3][2] = {{0, 0}, {1, 2}, {3, 4}};

  for (const auto& idx : iter) {
    EXPECT_EQ(idx[0], expected_coords[count][0]);
    EXPECT_EQ(idx[1], expected_coords[count][1]);
    count++;
  }

  EXPECT_EQ(count, 3);
}

}  // namespace asc
