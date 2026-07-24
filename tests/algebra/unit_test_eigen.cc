// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: tests/algebra/unit_test_eigen.cc
// Author: Yi Cai
// ============================================================================

#include <gtest/gtest.h>
#include <cmath>
#include "common/common.h"

#ifdef ASC_USE_EIGEN
#include "asc/linalg/eigen.h"

namespace asc {

// ============================================================================
// C Array ↔ Eigen Conversion Tests
// ============================================================================

TEST(EigenTest, ToEigenCArrayToEDVector) {
  real_t arr[5] = {1.0, 2.0, 3.0, 4.0, 5.0};
  EDVector<real_t> vec;

  CVectorToEigen(arr, vec);

  EXPECT_EQ(vec.size(), 5);
  for (int i = 0; i < 5; ++i) {
    EXPECT_REAL_EQ(vec(i), arr[i]);
  }
}

TEST(EigenTest, ToEigenCArrayToESVector) {
  real_t arr[3] = {1.0, 2.0, 3.0};
  ESVector<real_t, 3> vec;

  CVectorToEigen(arr, vec);

  for (int i = 0; i < 3; ++i) {
    EXPECT_REAL_EQ(vec(i), arr[i]);
  }
}

TEST(EigenTest, ToEigenCArrayToEDMatrix) {
  real_t arr[2][3] = {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}};
  EDMatrix<real_t> mat;

  CMatrixToEigen(arr, mat);

  EXPECT_EQ(mat.rows(), 2);
  EXPECT_EQ(mat.cols(), 3);
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 3; ++j) {
      EXPECT_REAL_EQ(mat(i, j), arr[i][j]);
    }
  }
}

TEST(EigenTest, ToEigenCArrayToESMatrix) {
  real_t arr[2][3] = {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}};
  ESMatrix<real_t, 2, 3> mat;

  CMatrixToEigen(arr, mat);

  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 3; ++j) {
      EXPECT_REAL_EQ(mat(i, j), arr[i][j]);
    }
  }
}

TEST(EigenTest, FromEigenEDVectorToCArray) {
  EDVector<real_t> vec(5);
  vec << 1.0, 2.0, 3.0, 4.0, 5.0;
  real_t arr[5];

  CVectorFromEigen(vec, arr);

  for (int i = 0; i < 5; ++i) {
    EXPECT_REAL_EQ(arr[i], vec(i));
  }
}

TEST(EigenTest, FromEigenESVectorToCArray) {
  ESVector<real_t, 3> vec;
  vec << 1.0, 2.0, 3.0;
  real_t arr[3];

  CVectorFromEigen(vec, arr);

  for (int i = 0; i < 3; ++i) {
    EXPECT_REAL_EQ(arr[i], vec(i));
  }
}

TEST(EigenTest, FromEigenEDMatrixToCArray) {
  EDMatrix<real_t> mat(2, 3);
  mat << 1.0, 2.0, 3.0, 4.0, 5.0, 6.0;
  real_t arr[2][3];

  CMatrixFromEigen(mat, arr);

  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 3; ++j) {
      EXPECT_REAL_EQ(arr[i][j], mat(i, j));
    }
  }
}

TEST(EigenTest, FromEigenESMatrixToCArray) {
  ESMatrix<real_t, 2, 3> mat;
  mat << 1.0, 2.0, 3.0, 4.0, 5.0, 6.0;
  real_t arr[2][3];

  CMatrixFromEigen(mat, arr);

  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 3; ++j) {
      EXPECT_REAL_EQ(arr[i][j], mat(i, j));
    }
  }
}

// ============================================================================
// ASC <--> Eigen Conversion Tests
// ============================================================================

TEST(EigenTest, ToEigenDVectorToEDVector) {
  DVector<real_t> asc_vec(5);
  for (int i = 0; i < 5; ++i) {
    asc_vec[i] = static_cast<real_t>(i + 1);
  }

  EDVector<real_t> eigen_vec;
  DVectorToEigen(asc_vec, eigen_vec);

  EXPECT_EQ(eigen_vec.size(), 5);
  for (int i = 0; i < 5; ++i) {
    EXPECT_REAL_EQ(eigen_vec(i), asc_vec[i]);
  }
}

TEST(EigenTest, ToEigenSVectorToESVector) {
  SVector<real_t, 3> asc_vec;
  asc_vec[0] = 1.0;
  asc_vec[1] = 2.0;
  asc_vec[2] = 3.0;

  ESVector<real_t, 3> eigen_vec;
  SVectorToEigen(asc_vec, eigen_vec);

  for (int i = 0; i < 3; ++i) {
    EXPECT_REAL_EQ(eigen_vec(i), asc_vec[i]);
  }
}

TEST(EigenTest, ToEigenDMatrixToEDMatrix) {
  DMatrix<real_t> asc_mat(2, 3);
  int count = 1;
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 3; ++j) {
      asc_mat(i, j) = static_cast<real_t>(count++);
    }
  }

  EDMatrix<real_t> eigen_mat;
  DMatrixToEigen(asc_mat, eigen_mat);

  EXPECT_EQ(eigen_mat.rows(), 2);
  EXPECT_EQ(eigen_mat.cols(), 3);
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 3; ++j) {
      EXPECT_REAL_EQ(eigen_mat(i, j), asc_mat(i, j));
    }
  }
}

TEST(EigenTest, ToEigenSMatrixToESMatrix) {
  SMatrix<real_t, 2, 3> asc_mat;
  int count = 1;
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 3; ++j) {
      asc_mat(i, j) = static_cast<real_t>(count++);
    }
  }

  ESMatrix<real_t, 2, 3> eigen_mat;
  SMatrixToEigen(asc_mat, eigen_mat);

  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 3; ++j) {
      EXPECT_REAL_EQ(eigen_mat(i, j), asc_mat(i, j));
    }
  }
}

TEST(EigenTest, FromEigenEDVectorToDVector) {
  EDVector<real_t> eigen_vec(5);
  eigen_vec << 1.0, 2.0, 3.0, 4.0, 5.0;

  DVector<real_t> asc_vec;
  DVectorFromEigen(eigen_vec, asc_vec);

  EXPECT_EQ(asc_vec.GetSize(), 5);
  for (int i = 0; i < 5; ++i) {
    EXPECT_REAL_EQ(asc_vec[i], eigen_vec(i));
  }
}

TEST(EigenTest, FromEigenESVectorToSVector) {
  ESVector<real_t, 3> eigen_vec;
  eigen_vec << 1.0, 2.0, 3.0;

  SVector<real_t, 3> asc_vec;
  SVectorFromEigen(eigen_vec, asc_vec);

  for (int i = 0; i < 3; ++i) {
    EXPECT_REAL_EQ(asc_vec[i], eigen_vec(i));
  }
}

TEST(EigenTest, FromEigenEDMatrixToDMatrix) {
  EDMatrix<real_t> eigen_mat(2, 3);
  eigen_mat << 1.0, 2.0, 3.0, 4.0, 5.0, 6.0;

  DMatrix<real_t> asc_mat;
  DMatrixFromEigen(eigen_mat, asc_mat);

  auto shape = asc_mat.GetShape();
  EXPECT_EQ(shape[0], 2);
  EXPECT_EQ(shape[1], 3);
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 3; ++j) {
      EXPECT_REAL_EQ(asc_mat(i, j), eigen_mat(i, j));
    }
  }
}

TEST(EigenTest, FromEigenESMatrixToSMatrix) {
  ESMatrix<real_t, 2, 3> eigen_mat;
  eigen_mat << 1.0, 2.0, 3.0, 4.0, 5.0, 6.0;

  SMatrix<real_t, 2, 3> asc_mat;
  SMatrixFromEigen(eigen_mat, asc_mat);

  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 3; ++j) {
      EXPECT_REAL_EQ(asc_mat(i, j), eigen_mat(i, j));
    }
  }
}

// ============================================================================
// Round-trip Conversion Tests
// ============================================================================

TEST(EigenTest, RoundTripDVector) {
  DVector<real_t> original(5);
  for (int i = 0; i < 5; ++i) {
    original[i] = static_cast<real_t>(i + 1) * 1.5;
  }

  EDVector<real_t> eigen_vec;
  DVectorToEigen(original, eigen_vec);

  DVector<real_t> result;
  DVectorFromEigen(eigen_vec, result);

  EXPECT_EQ(result.GetSize(), original.GetSize());
  for (int i = 0; i < 5; ++i) {
    EXPECT_REAL_EQ(result[i], original[i]);
  }
}

TEST(EigenTest, RoundTripSVector) {
  SVector<real_t, 3> original;
  original[0] = 1.5;
  original[1] = 2.5;
  original[2] = 3.5;

  ESVector<real_t, 3> eigen_vec;
  SVectorToEigen(original, eigen_vec);

  SVector<real_t, 3> result;
  SVectorFromEigen(eigen_vec, result);

  for (int i = 0; i < 3; ++i) {
    EXPECT_REAL_EQ(result[i], original[i]);
  }
}

TEST(EigenTest, RoundTripDMatrix) {
  DMatrix<real_t> original(2, 3);
  real_t val = 1.0;
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 3; ++j) {
      original(i, j) = val;
      val += 0.5;
    }
  }

  EDMatrix<real_t> eigen_mat;
  DMatrixToEigen(original, eigen_mat);

  DMatrix<real_t> result;
  DMatrixFromEigen(eigen_mat, result);

  auto shape = result.GetShape();
  EXPECT_EQ(shape[0], 2);
  EXPECT_EQ(shape[1], 3);
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 3; ++j) {
      EXPECT_REAL_EQ(result(i, j), original(i, j));
    }
  }
}

TEST(EigenTest, RoundTripSMatrix) {
  SMatrix<real_t, 2, 3> original;
  real_t val = 1.0;
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 3; ++j) {
      original(i, j) = val;
      val += 0.5;
    }
  }

  ESMatrix<real_t, 2, 3> eigen_mat;
  SMatrixToEigen(original, eigen_mat);

  SMatrix<real_t, 2, 3> result;
  SMatrixFromEigen(eigen_mat, result);

  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 3; ++j) {
      EXPECT_REAL_EQ(result(i, j), original(i, j));
    }
  }
}

// ============================================================================
// Data Layout Tests (Column-Major)
// ============================================================================

TEST(EigenTest, DataLayoutConsistencyMatrix) {
  // Both ASC and Eigen use column-major layout by default
  DMatrix<real_t> asc_mat(2, 3);

  // Fill in column-major order: [col0_row0, col0_row1, col1_row0, col1_row1,
  // ...]
  real_t* data = asc_mat.ReadWrite();
  for (int i = 0; i < 6; ++i) {
    data[i] = static_cast<real_t>(i + 1);
  }

  // Convert to Eigen
  EDMatrix<real_t> eigen_mat;
  DMatrixToEigen(asc_mat, eigen_mat);

  // Check that Eigen reads the same data in column-major order
  EXPECT_REAL_EQ(eigen_mat(0, 0), 1.0);  // col0, row0
  EXPECT_REAL_EQ(eigen_mat(1, 0), 2.0);  // col0, row1
  EXPECT_REAL_EQ(eigen_mat(0, 1), 3.0);  // col1, row0
  EXPECT_REAL_EQ(eigen_mat(1, 1), 4.0);  // col1, row1
  EXPECT_REAL_EQ(eigen_mat(0, 2), 5.0);  // col2, row0
  EXPECT_REAL_EQ(eigen_mat(1, 2), 6.0);  // col2, row1
}

// ============================================================================
// Type Alias Tests
// ============================================================================

TEST(EigenTest, TypeAliases) {
  // Test that type aliases compile correctly
  EDVector<real_t> ed_vec(5);
  ESVector<real_t, 3> es_vec;
  EDMatrix<real_t> ed_mat(2, 3);
  ESMatrix<real_t, 2, 3> es_mat;
  ESpMatrix<real_t> sp_mat(10, 10);

  // Just verify they compile and have expected properties
  EXPECT_EQ(ed_vec.size(), 5);
  EXPECT_EQ(es_vec.size(), 3);
  EXPECT_EQ(ed_mat.rows(), 2);
  EXPECT_EQ(ed_mat.cols(), 3);
  EXPECT_EQ(es_mat.rows(), 2);
  EXPECT_EQ(es_mat.cols(), 3);
  EXPECT_EQ(sp_mat.rows(), 10);
  EXPECT_EQ(sp_mat.cols(), 10);
}

// ============================================================================
// Sparse Matrix Conversion Tests (SpDMatrix/SpSMatrix <-> ESpMatrix)
// ============================================================================

// Helper function to create a test sparse matrix
SpDMatrix<real_t> CreateTestSpDMatrix(int nrows, int ncols) {
  SpDMatrix<real_t> sp_mat(DShape<2>(nrows, ncols));

  // Build using COO format first, then convert
  SparseMArray<real_t, DShape<2>, SparseLayoutStride> coo(DShape<2>(nrows, ncols));

  // Add some non-zero elements (diagonal + some off-diagonal)
  for (int i = 0; i < std::min(nrows, ncols); ++i) {
    coo.Insert(i, i, static_cast<real_t>(i + 1) * 1.5);  // Diagonal
  }
  // Add some off-diagonal elements
  if (nrows > 1 && ncols > 1) {
    coo.Insert(0, 1, 2.5);
    coo.Insert(1, 0, 3.5);
  }
  if (nrows > 2 && ncols > 2) {
    coo.Insert(0, 2, 4.5);
    coo.Insert(2, 0, 5.5);
  }

  coo.Finalize();

  // Convert to default sparse layout
  sp_mat = coo.ToLayout<DefaultSparseLayout>();
  return sp_mat;
}

// ----------------------------------------------------------------------------
// SpDMatrixMap Tests (Zero-copy)
// ----------------------------------------------------------------------------

TEST(EigenTest, SpDMatrixMapBasic) {
  SpDMatrix<real_t> asc_sp = CreateTestSpDMatrix(4, 4);

  // Create zero-copy map
  auto eigen_map = SpDMatrixMap(asc_sp);

  // Check dimensions
  EXPECT_EQ(eigen_map.rows(), 4);
  EXPECT_EQ(eigen_map.cols(), 4);
  EXPECT_EQ(eigen_map.nonZeros(), asc_sp.GetNNZ());

  // Check values match
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      EXPECT_REAL_EQ(eigen_map.coeff(i, j), asc_sp.At(i, j));
    }
  }
}

TEST(EigenTest, SpDMatrixMapRectangular) {
  SpDMatrix<real_t> asc_sp = CreateTestSpDMatrix(3, 5);

  auto eigen_map = SpDMatrixMap(asc_sp);

  EXPECT_EQ(eigen_map.rows(), 3);
  EXPECT_EQ(eigen_map.cols(), 5);

  // Verify element access
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 5; ++j) {
      EXPECT_REAL_EQ(eigen_map.coeff(i, j), asc_sp.At(i, j));
    }
  }
}

TEST(EigenTest, SpDMatrixMapEmpty) {
  SpDMatrix<real_t> asc_sp(DShape<2>(3, 3));  // Empty sparse matrix

  auto eigen_map = SpDMatrixMap(asc_sp);

  EXPECT_EQ(eigen_map.rows(), 3);
  EXPECT_EQ(eigen_map.cols(), 3);
  EXPECT_EQ(eigen_map.nonZeros(), 0);
}

// ----------------------------------------------------------------------------
// SpSMatrixMap Tests (Zero-copy, Static size)
// ----------------------------------------------------------------------------

TEST(EigenTest, SpSMatrixMapBasic) {
  // Create static sparse matrix using COO
  SparseMArray<real_t, MShape<4, 4>, SparseLayoutStride> coo;
  coo.Insert(0, 0, 1.0);
  coo.Insert(1, 1, 2.0);
  coo.Insert(2, 2, 3.0);
  coo.Insert(0, 1, 0.5);
  coo.Finalize();

  SpSMatrix<real_t, 4, 4> asc_sp = coo.ToLayout<DefaultSparseLayout>();

  auto eigen_map = SpSMatrixMap(asc_sp);

  EXPECT_EQ(eigen_map.rows(), 4);
  EXPECT_EQ(eigen_map.cols(), 4);
  EXPECT_EQ(eigen_map.nonZeros(), asc_sp.GetNNZ());

  // Verify values
  EXPECT_REAL_EQ(eigen_map.coeff(0, 0), 1.0);
  EXPECT_REAL_EQ(eigen_map.coeff(1, 1), 2.0);
  EXPECT_REAL_EQ(eigen_map.coeff(2, 2), 3.0);
  EXPECT_REAL_EQ(eigen_map.coeff(0, 1), 0.5);
  EXPECT_REAL_EQ(eigen_map.coeff(3, 3), 0.0);  // Not stored
}

// ----------------------------------------------------------------------------
// SpDMatrixToEigen Tests (Single copy)
// ----------------------------------------------------------------------------

TEST(EigenTest, SpDMatrixToEigenBasic) {
  SpDMatrix<real_t> asc_sp = CreateTestSpDMatrix(4, 4);

  const int expected_outer[] = {0, 3, 5, 7, 8};
  const int expected_inner[] = {0, 1, 2, 0, 1, 0, 2, 3};
  const auto& asc_map = asc_sp.GetMap();
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(asc_map.GetOuterPtr()[i], expected_outer[i]);
  }
  for (int i = 0; i < 8; ++i) {
    EXPECT_EQ(asc_map.GetInnerIndices(0)[i], expected_inner[i]);
  }

  ESpMatrix<real_t> eigen_sp = SpDMatrixToEigen(asc_sp);

  EXPECT_EQ(eigen_sp.rows(), 4);
  EXPECT_EQ(eigen_sp.cols(), 4);
  EXPECT_EQ(eigen_sp.nonZeros(), asc_sp.GetNNZ());

  // Verify all elements
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      EXPECT_REAL_EQ(eigen_sp.coeff(i, j), asc_sp.At(i, j));
    }
  }
}

TEST(EigenTest, SpDMatrixToEigenRectangular) {
  SpDMatrix<real_t> asc_sp = CreateTestSpDMatrix(5, 3);

  ESpMatrix<real_t> eigen_sp = SpDMatrixToEigen(asc_sp);

  EXPECT_EQ(eigen_sp.rows(), 5);
  EXPECT_EQ(eigen_sp.cols(), 3);

  for (int i = 0; i < 5; ++i) {
    for (int j = 0; j < 3; ++j) {
      EXPECT_REAL_EQ(eigen_sp.coeff(i, j), asc_sp.At(i, j));
    }
  }
}

TEST(EigenTest, SpDMatrixToEigenEmpty) {
  SpDMatrix<real_t> asc_sp(DShape<2>(3, 4));  // Empty

  ESpMatrix<real_t> eigen_sp = SpDMatrixToEigen(asc_sp);

  EXPECT_EQ(eigen_sp.rows(), 3);
  EXPECT_EQ(eigen_sp.cols(), 4);
  EXPECT_EQ(eigen_sp.nonZeros(), 0);
}

// ----------------------------------------------------------------------------
// SpSMatrixToEigen Tests (Single copy, Static size)
// ----------------------------------------------------------------------------

TEST(EigenTest, SpSMatrixToEigenBasic) {
  SparseMArray<real_t, MShape<3, 3>, SparseLayoutStride> coo;
  coo.Insert(0, 0, 1.0);
  coo.Insert(1, 1, 2.0);
  coo.Insert(2, 2, 3.0);
  coo.Insert(1, 2, 4.0);
  coo.Finalize();

  SpSMatrix<real_t, 3, 3> asc_sp = coo.ToLayout<DefaultSparseLayout>();

  ESpMatrix<real_t> eigen_sp = SpSMatrixToEigen(asc_sp);

  EXPECT_EQ(eigen_sp.rows(), 3);
  EXPECT_EQ(eigen_sp.cols(), 3);
  EXPECT_EQ(eigen_sp.nonZeros(), 4);

  EXPECT_REAL_EQ(eigen_sp.coeff(0, 0), 1.0);
  EXPECT_REAL_EQ(eigen_sp.coeff(1, 1), 2.0);
  EXPECT_REAL_EQ(eigen_sp.coeff(2, 2), 3.0);
  EXPECT_REAL_EQ(eigen_sp.coeff(1, 2), 4.0);
}

// ----------------------------------------------------------------------------
// SpDMatrixFromEigen Tests (Single copy)
// ----------------------------------------------------------------------------

TEST(EigenTest, SpDMatrixFromEigenBasic) {
  // Create Eigen sparse matrix
  ESpMatrix<real_t> eigen_sp(4, 4);
  std::vector<Eigen::Triplet<real_t>> triplets;
  triplets.emplace_back(0, 0, 1.0);
  triplets.emplace_back(1, 1, 2.0);
  triplets.emplace_back(2, 2, 3.0);
  triplets.emplace_back(3, 3, 4.0);
  triplets.emplace_back(0, 1, 0.5);
  triplets.emplace_back(1, 0, 0.75);
  eigen_sp.setFromTriplets(triplets.begin(), triplets.end());

  SpDMatrix<real_t> asc_sp;
  SpDMatrixFromEigen(eigen_sp, asc_sp);

  EXPECT_EQ(asc_sp.GetExtent(0), 4);
  EXPECT_EQ(asc_sp.GetExtent(1), 4);
  EXPECT_EQ(asc_sp.GetNNZ(), 6);

  // Verify all elements
  EXPECT_REAL_EQ(asc_sp.At(0, 0), 1.0);
  EXPECT_REAL_EQ(asc_sp.At(1, 1), 2.0);
  EXPECT_REAL_EQ(asc_sp.At(2, 2), 3.0);
  EXPECT_REAL_EQ(asc_sp.At(3, 3), 4.0);
  EXPECT_REAL_EQ(asc_sp.At(0, 1), 0.5);
  EXPECT_REAL_EQ(asc_sp.At(1, 0), 0.75);
  EXPECT_REAL_EQ(asc_sp.At(0, 2), 0.0);  // Not stored
}

TEST(EigenTest, SpDMatrixFromEigenRectangular) {
  ESpMatrix<real_t> eigen_sp(3, 5);
  std::vector<Eigen::Triplet<real_t>> triplets;
  triplets.emplace_back(0, 0, 1.0);
  triplets.emplace_back(1, 1, 2.0);
  triplets.emplace_back(2, 2, 3.0);
  triplets.emplace_back(0, 4, 4.0);
  eigen_sp.setFromTriplets(triplets.begin(), triplets.end());

  SpDMatrix<real_t> asc_sp;
  SpDMatrixFromEigen(eigen_sp, asc_sp);

  EXPECT_EQ(asc_sp.GetExtent(0), 3);
  EXPECT_EQ(asc_sp.GetExtent(1), 5);

  EXPECT_REAL_EQ(asc_sp.At(0, 0), 1.0);
  EXPECT_REAL_EQ(asc_sp.At(1, 1), 2.0);
  EXPECT_REAL_EQ(asc_sp.At(2, 2), 3.0);
  EXPECT_REAL_EQ(asc_sp.At(0, 4), 4.0);
}

TEST(EigenTest, SpDMatrixFromEigenEmpty) {
  ESpMatrix<real_t> eigen_sp(3, 4);  // Empty

  SpDMatrix<real_t> asc_sp;
  SpDMatrixFromEigen(eigen_sp, asc_sp);

  EXPECT_EQ(asc_sp.GetExtent(0), 3);
  EXPECT_EQ(asc_sp.GetExtent(1), 4);
  EXPECT_EQ(asc_sp.GetNNZ(), 0);
}

// ----------------------------------------------------------------------------
// SpSMatrixFromEigen Tests (Single copy, Static size)
// ----------------------------------------------------------------------------

TEST(EigenTest, SpSMatrixFromEigenBasic) {
  ESpMatrix<real_t> eigen_sp(3, 3);
  std::vector<Eigen::Triplet<real_t>> triplets;
  triplets.emplace_back(0, 0, 1.0);
  triplets.emplace_back(1, 1, 2.0);
  triplets.emplace_back(2, 2, 3.0);
  triplets.emplace_back(0, 2, 4.0);
  eigen_sp.setFromTriplets(triplets.begin(), triplets.end());

  SpSMatrix<real_t, 3, 3> asc_sp;
  SpSMatrixFromEigen(eigen_sp, asc_sp);

  EXPECT_EQ(asc_sp.GetExtent(0), 3);
  EXPECT_EQ(asc_sp.GetExtent(1), 3);
  EXPECT_EQ(asc_sp.GetNNZ(), 4);

  EXPECT_REAL_EQ(asc_sp.At(0, 0), 1.0);
  EXPECT_REAL_EQ(asc_sp.At(1, 1), 2.0);
  EXPECT_REAL_EQ(asc_sp.At(2, 2), 3.0);
  EXPECT_REAL_EQ(asc_sp.At(0, 2), 4.0);
}

// ----------------------------------------------------------------------------
// Round-trip Conversion Tests (Sparse)
// ----------------------------------------------------------------------------

TEST(EigenTest, RoundTripSpDMatrix) {
  SpDMatrix<real_t> original = CreateTestSpDMatrix(5, 5);

  // ASC -> Eigen
  ESpMatrix<real_t> eigen_sp = SpDMatrixToEigen(original);

  // Eigen -> ASC
  SpDMatrix<real_t> result;
  SpDMatrixFromEigen(eigen_sp, result);

  EXPECT_EQ(result.GetExtent(0), original.GetExtent(0));
  EXPECT_EQ(result.GetExtent(1), original.GetExtent(1));
  EXPECT_EQ(result.GetNNZ(), original.GetNNZ());

  // Verify all elements
  for (int i = 0; i < 5; ++i) {
    for (int j = 0; j < 5; ++j) {
      EXPECT_REAL_EQ(result.At(i, j), original.At(i, j));
    }
  }
}

TEST(EigenTest, RoundTripSpSMatrix) {
  SparseMArray<real_t, MShape<4, 4>, SparseLayoutStride> coo;
  coo.Insert(0, 0, 1.5);
  coo.Insert(1, 1, 2.5);
  coo.Insert(2, 2, 3.5);
  coo.Insert(3, 3, 4.5);
  coo.Insert(0, 3, 5.5);
  coo.Insert(3, 0, 6.5);
  coo.Finalize();

  SpSMatrix<real_t, 4, 4> original = coo.ToLayout<DefaultSparseLayout>();

  // ASC -> Eigen (zero-copy Map)
  auto eigen_map = SpSMatrixToEigen(original);

  // To convert to owning SparseMatrix for SpSMatrixFromEigen, assign from map
  ESpMatrix<real_t> eigen_sp = eigen_map;

  // Eigen -> ASC
  SpSMatrix<real_t, 4, 4> result;
  SpSMatrixFromEigen(eigen_sp, result);

  EXPECT_EQ(result.GetNNZ(), original.GetNNZ());

  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      EXPECT_REAL_EQ(result.At(i, j), original.At(i, j));
    }
  }
}

TEST(EigenTest, RoundTripEigenToASCToEigen) {
  // Start from Eigen
  ESpMatrix<real_t> original(4, 4);
  std::vector<Eigen::Triplet<real_t>> triplets;
  triplets.emplace_back(0, 0, 1.0);
  triplets.emplace_back(1, 1, 2.0);
  triplets.emplace_back(2, 2, 3.0);
  triplets.emplace_back(3, 3, 4.0);
  triplets.emplace_back(0, 2, 5.0);
  triplets.emplace_back(2, 0, 6.0);
  original.setFromTriplets(triplets.begin(), triplets.end());

  // Eigen -> ASC
  SpDMatrix<real_t> asc_sp;
  SpDMatrixFromEigen(original, asc_sp);

  // ASC -> Eigen
  ESpMatrix<real_t> result = SpDMatrixToEigen(asc_sp);

  EXPECT_EQ(result.rows(), original.rows());
  EXPECT_EQ(result.cols(), original.cols());
  EXPECT_EQ(result.nonZeros(), original.nonZeros());

  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      EXPECT_REAL_EQ(result.coeff(i, j), original.coeff(i, j));
    }
  }
}

// ----------------------------------------------------------------------------
// Map vs Copy Consistency Tests
// ----------------------------------------------------------------------------

TEST(EigenTest, MapAndCopyConsistency) {
  SpDMatrix<real_t> asc_sp = CreateTestSpDMatrix(4, 4);

  // Get zero-copy map
  auto eigen_map = SpDMatrixMap(asc_sp);

  // Get copy
  ESpMatrix<real_t> eigen_copy = SpDMatrixToEigen(asc_sp);

  // Both should have same values
  EXPECT_EQ(eigen_map.rows(), eigen_copy.rows());
  EXPECT_EQ(eigen_map.cols(), eigen_copy.cols());
  EXPECT_EQ(eigen_map.nonZeros(), eigen_copy.nonZeros());

  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      EXPECT_REAL_EQ(eigen_map.coeff(i, j), eigen_copy.coeff(i, j));
    }
  }
}

// ----------------------------------------------------------------------------
// Eigen Operations with Mapped ASC Data
// ----------------------------------------------------------------------------

TEST(EigenTest, EigenOperationsWithMap) {
  SpDMatrix<real_t> asc_sp = CreateTestSpDMatrix(4, 4);

  // Create map and use it in Eigen operations
  auto eigen_map = SpDMatrixMap(asc_sp);

  // Create a dense vector
  EDVector<real_t> x(4);
  x << 1.0, 2.0, 3.0, 4.0;

  // Sparse matrix-vector multiplication using mapped data
  EDVector<real_t> y = eigen_map * x;

  EXPECT_EQ(y.size(), 4);

  // Verify result by computing expected values
  for (int i = 0; i < 4; ++i) {
    real_t expected = 0.0;
    for (int j = 0; j < 4; ++j) {
      expected += asc_sp.At(i, j) * x(j);
    }
    EXPECT_NEAR(y(i), expected, kRealTolerance);
  }
}

TEST(EigenTest, EigenSolveWithConvertedMatrix) {
  // Create a simple SPD matrix
  ESpMatrix<real_t> A(3, 3);
  std::vector<Eigen::Triplet<real_t>> triplets;
  triplets.emplace_back(0, 0, 4.0);
  triplets.emplace_back(0, 1, 1.0);
  triplets.emplace_back(1, 0, 1.0);
  triplets.emplace_back(1, 1, 3.0);
  triplets.emplace_back(1, 2, 1.0);
  triplets.emplace_back(2, 1, 1.0);
  triplets.emplace_back(2, 2, 2.0);
  A.setFromTriplets(triplets.begin(), triplets.end());

  // Convert to ASC and back
  SpDMatrix<real_t> asc_A;
  SpDMatrixFromEigen(A, asc_A);

  ESpMatrix<real_t> A_back = SpDMatrixToEigen(asc_A);

  // Solve using Eigen
  EDVector<real_t> b(3);
  b << 1.0, 2.0, 3.0;

  Eigen::SimplicialLDLT<ESpMatrix<real_t>> solver;
  solver.compute(A_back);
  EXPECT_EQ(solver.info(), Eigen::Success);

  EDVector<real_t> x = solver.solve(b);
  EXPECT_EQ(solver.info(), Eigen::Success);

  // Verify solution: A * x ≈ b
  EDVector<real_t> residual = A_back * x - b;
  EXPECT_LT(residual.norm(), kRealTolerance);
}

// ----------------------------------------------------------------------------
// Large Sparse Matrix Tests
// ----------------------------------------------------------------------------

TEST(EigenTest, LargeSpDMatrixConversion) {
  const int n = 100;

  // Create a sparse matrix with diagonal and some off-diagonal elements
  SparseMArray<real_t, DShape<2>, SparseLayoutStride> coo(DShape<2>(n, n));

  // Diagonal
  for (int i = 0; i < n; ++i) {
    coo.Insert(i, i, static_cast<real_t>(i + 1));
  }
  // Super-diagonal
  for (int i = 0; i < n - 1; ++i) {
    coo.Insert(i, i + 1, 0.5);
  }
  // Sub-diagonal
  for (int i = 1; i < n; ++i) {
    coo.Insert(i, i - 1, 0.5);
  }

  coo.Finalize();
  SpDMatrix<real_t> asc_sp = coo.ToLayout<DefaultSparseLayout>();

  // Convert to Eigen
  ESpMatrix<real_t> eigen_sp = SpDMatrixToEigen(asc_sp);

  EXPECT_EQ(eigen_sp.rows(), n);
  EXPECT_EQ(eigen_sp.cols(), n);
  EXPECT_EQ(eigen_sp.nonZeros(), asc_sp.GetNNZ());

  // Verify diagonal
  for (int i = 0; i < n; ++i) {
    EXPECT_REAL_EQ(eigen_sp.coeff(i, i), static_cast<real_t>(i + 1));
  }

  // Convert back
  SpDMatrix<real_t> result;
  SpDMatrixFromEigen(eigen_sp, result);

  EXPECT_EQ(result.GetNNZ(), asc_sp.GetNNZ());

  // Verify all diagonal elements after round-trip
  for (int i = 0; i < n; ++i) {
    EXPECT_REAL_EQ(result.At(i, i), asc_sp.At(i, i));
  }
}

}  // namespace asc

#endif  // ASC_USE_EIGEN
