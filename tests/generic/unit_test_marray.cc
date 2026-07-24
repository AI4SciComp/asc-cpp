// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: tests/generic/unit_test_marray.cc
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#include <gtest/gtest.h>
#include <algorithm>
#include <sstream>
#include "common/common.h"
#include "asc/array/marray.h"

namespace asc {

// ============================================================================
// DenseMArray Basic Tests - Constructors and Memory
// ============================================================================

template <typename MemType>
using MArrayTest = BaseTest<MemType>;

TYPED_TEST_SUITE(MArrayTest, AllMemoryTypes);

TYPED_TEST(MArrayTest, DefaultConstructor) {
  DVector<int> arr;
  EXPECT_EQ(arr.GetSize(), 0);
  EXPECT_TRUE(arr.IsEmpty());
}

TYPED_TEST(MArrayTest, VectorConstructor) {
  DVector<int> vec(this->kMemType, 10);

  EXPECT_EQ(vec.GetRank(), 1);
  EXPECT_EQ(vec.GetSize(), 10);
  EXPECT_EQ(vec.GetExtent(0), 10);
  EXPECT_FALSE(vec.IsEmpty());
}

TYPED_TEST(MArrayTest, MatrixConstructor) {
  DMatrix<int> mat(this->kMemType, 3, 4);
  EXPECT_EQ(mat.GetRank(), 2);
  EXPECT_EQ(mat.GetSize(), 12);
  EXPECT_EQ(mat.GetExtent(0), 3);
  EXPECT_EQ(mat.GetExtent(1), 4);
}

TYPED_TEST(MArrayTest, StaticVectorConstructor) {
  SVector<int, 5> vec(this->kMemType);
  EXPECT_EQ(vec.GetRank(), 1);
  EXPECT_EQ(vec.GetSize(), 5);
  EXPECT_EQ(vec.GetExtent(0), 5);
}

TYPED_TEST(MArrayTest, StaticMatrixConstructor) {
  SMatrix<int, 3, 4> mat(this->kMemType);
  EXPECT_EQ(mat.GetRank(), 2);
  EXPECT_EQ(mat.GetSize(), 12);
  EXPECT_EQ(mat.GetExtent(0), 3);
  EXPECT_EQ(mat.GetExtent(1), 4);
}

TYPED_TEST(MArrayTest, CopyConstructor) {
  DVector<int> vec1(this->kMemType, 5);
  vec1 << 0, 10, 20, 30, 40;

  DVector<int> vec2 = vec1;
  EXPECT_EQ(vec2.GetSize(), vec1.GetSize());

  const int* vec1_ptr = vec1.Read(vec1.UseDevice());
  const int* vec2_ptr = vec2.Read(vec2.UseDevice());

  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(vec2_ptr[i], vec1_ptr[i]);
  }

  // Modify vec2 and verify vec1 is not affected (deep copy)
  int* vec2_write = vec2.Write(vec2.UseDevice());
  vec2_write[0] = 999;

  vec1_ptr = vec1.Read(vec1.UseDevice());
  vec2_ptr = vec2.Read(vec2.UseDevice());
  EXPECT_NE(vec1_ptr[0], vec2_ptr[0]);
}

TYPED_TEST(MArrayTest, MoveConstructor) {
  DVector<int> vec1(this->kMemType, 5);
  vec1 << 0, 10, 20, 30, 40;

  DVector<int> vec2 = std::move(vec1);
  EXPECT_EQ(vec2.GetSize(), 5);

  const int* vec2_ptr = vec2.Read(vec2.UseDevice());
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(vec2_ptr[i], i * 10);
  }
}

TYPED_TEST(MArrayTest, CopyAssignment) {
  DVector<int> vec1(this->kMemType, 5);
  vec1 << 0, 1, 2, 3, 4;

  DVector<int> vec2(this->kMemType, 5);
  vec2 = vec1;

  EXPECT_EQ(vec2.GetSize(), vec1.GetSize());

  const int* vec1_ptr = vec1.Read(vec1.UseDevice());
  const int* vec2_ptr = vec2.Read(vec2.UseDevice());
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(vec2_ptr[i], vec1_ptr[i]);
  }
}

TYPED_TEST(MArrayTest, MoveAssignment) {
  DVector<int> vec1(this->kMemType, 5);
  vec1 << 0, 1, 2, 3, 4;
  DVector<int> vec2(this->kMemType, 5);
  vec2 = std::move(vec1);

  EXPECT_EQ(vec2.GetSize(), 5);

  const int* vec2_ptr = vec2.Read(vec2.UseDevice());
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(vec2_ptr[i], i);
  }
}

TYPED_TEST(MArrayTest, ScalarAssignment) {
  DVector<int> vec(this->kMemType, 10);
  vec = 42;

  const int* vec_ptr = vec.Read(vec.UseDevice());
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(vec_ptr[i], 42);
  }
}

TYPED_TEST(MArrayTest, ConvertedScalarAssignment) {
  DVector<real_t> vec(this->kMemType, 4);
  vec = 2;

  const real_t* vec_ptr = vec.Read(vec.UseDevice());
  for (int i = 0; i < 4; ++i) {
    EXPECT_REAL_EQ(vec_ptr[i], 2.0);
  }
}

// ============================================================================
// Element Access Tests
// ============================================================================

TYPED_TEST(MArrayTest, ElementAccess1D_LinearIndex) {
  DVector<int> vec(this->kMemType, 5);
  vec << 0, 2, 4, 6, 8;
  const int* vec_ptr = vec.Read(vec.UseDevice());
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(vec_ptr[i], i * 2);
    EXPECT_EQ(vec[i], i * 2);  // Operator[] access
  }
}

TYPED_TEST(MArrayTest, ElementAccess1D_FunctionCall) {
  DVector<int> vec(this->kMemType, 5);
  int* vec_ptr = vec.Write(vec.UseDevice());
  for (int i = 0; i < 5; ++i) {
    vec_ptr[i] = i * 3;
  }

  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(vec(i), i * 3);  // Operator() access
  }
}

TYPED_TEST(MArrayTest, ElementAccess2D) {
  DMatrix<int> mat(this->kMemType, 3, 4);

  // Initialize with column-major data.
  mat << 0, 10, 20, 1, 11, 21, 2, 12, 22, 3, 13, 23;

  const int* mat_ptr = mat.Read(mat.UseDevice());
  for (int j = 0; j < 4; ++j) {
    for (int i = 0; i < 3; ++i) {
      EXPECT_EQ(mat_ptr[j * 3 + i], i * 10 + j);
      EXPECT_EQ(mat(i, j), i * 10 + j);  // Operator() access
    }
  }
}

TYPED_TEST(MArrayTest, ElementAccessMIndex) {
  DMatrix<int> mat(this->kMemType, 3, 4);
  int* mat_ptr = mat.Write(mat.UseDevice());

  // Initialize using MIndex
  MIndex<DShape<2>> idx(3, 4);
  for (int j = 0; j < 4; ++j) {
    for (int i = 0; i < 3; ++i) {
      idx.SetMultiIndex(i, j);
      mat_ptr[idx.GetOffset()] = i + j * 10;
    }
  }

  // Access using MIndex
  for (int j = 0; j < 4; ++j) {
    for (int i = 0; i < 3; ++i) {
      idx.SetMultiIndex(i, j);
      EXPECT_EQ(mat(idx), i + j * 10);
    }
  }
}

// ============================================================================
// Shape and Resize Tests
// ============================================================================

TYPED_TEST(MArrayTest, SetShape_SameSize) {
  DMatrix<int> mat(this->kMemType, 3, 2);
  mat << 1, 2, 3, 4, 5, 6;

  // Reshape from (6) to (2, 3) - same total size
  mat.SetShape(DShape<2>(2, 3));

  EXPECT_EQ(mat.GetRank(), 2);
  EXPECT_EQ(mat.GetExtent(0), 2);
  EXPECT_EQ(mat.GetExtent(1), 3);
  EXPECT_EQ(mat.GetSize(), 6);

  // Data should remain unchanged
  const int* vec_ptr = mat.Read(mat.UseDevice());
  for (int i = 0; i < 6; ++i) {
    EXPECT_EQ(vec_ptr[i], i + 1);
  }
}

TYPED_TEST(MArrayTest, SetShape_DifferentSize) {
  DVector<int> vec(this->kMemType, 4);
  vec << 1, 2, 3, 4;

  // Reshape to larger size
  vec.SetShape(DShape<1>(10));

  EXPECT_EQ(vec.GetSize(), 10);
}

// ============================================================================
// Initialization Methods Tests
// ============================================================================

TYPED_TEST(MArrayTest, SetZeros) {
  DVector<real_t> vec(this->kMemType, 10);
  vec.SetZeros();

  const real_t* vec_ptr = vec.Read(vec.UseDevice());
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(vec_ptr[i], 0.0_r);
  }
}

TYPED_TEST(MArrayTest, SetOnes) {
  DVector<real_t> vec(this->kMemType, 10);
  vec.SetOnes();

  const real_t* vec_ptr = vec.Read(vec.UseDevice());
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(vec_ptr[i], 1.0_r);
  }
}

TYPED_TEST(MArrayTest, SetConstants) {
  DVector<real_t> vec(this->kMemType, 10);
  vec.SetConstants(3.14_r);

  const real_t* vec_ptr = vec.Read(vec.UseDevice());
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(vec_ptr[i], 3.14_r);
  }
}

TYPED_TEST(MArrayTest, SetIdentity2D) {
  DMatrix<real_t> mat(this->kMemType, 3, 3);
  mat.SetIdentity();

  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      if (i == j) {
        EXPECT_EQ(mat(i, j), 1.0_r);
      } else {
        EXPECT_EQ(mat(i, j), 0.0_r);
      }
    }
  }
}

TYPED_TEST(MArrayTest, SetIdentity3D) {
  DTensor<real_t, 3> tensor(this->kMemType, 3, 3, 3);
  tensor.SetIdentity();

  // Check diagonal elements (i == j == k)
  EXPECT_EQ(tensor(0, 0, 0), 1.0_r);
  EXPECT_EQ(tensor(1, 1, 1), 1.0_r);
  EXPECT_EQ(tensor(2, 2, 2), 1.0_r);

  // Check non-diagonal elements
  EXPECT_EQ(tensor(0, 1, 0), 0.0_r);
  EXPECT_EQ(tensor(1, 0, 1), 0.0_r);
  EXPECT_EQ(tensor(0, 0, 1), 0.0_r);
}

TYPED_TEST(MArrayTest, CommaInitialization) {
  SMatrix<int, 2, 3> mat(this->kMemType);
  mat << 1, 2, 3, 4, 5, 6;

  // Column-major order: (0,0), (1,0), (0,1), (1,1), (0,2), (1,2)
  EXPECT_EQ(mat(0, 0), 1);
  EXPECT_EQ(mat(1, 0), 2);
  EXPECT_EQ(mat(0, 1), 3);
  EXPECT_EQ(mat(1, 1), 4);
  EXPECT_EQ(mat(0, 2), 5);
  EXPECT_EQ(mat(1, 2), 6);
}

// ============================================================================
// Copy Methods Tests
// ============================================================================

TYPED_TEST(MArrayTest, CopyToPointer) {
  DVector<int> vec(this->kMemType, 5);
  vec << 1, 2, 3, 4, 5;

  int dest[5] = {0};
  vec.CopyTo(dest);

  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(dest[i], i + 1);
  }
}

TYPED_TEST(MArrayTest, CopyFromPointer) {
  DVector<int> vec(this->kMemType, 5);
  int data[5] = {10, 20, 30, 40, 50};

  vec.CopyFrom(data);

  const int* vec_ptr = vec.Read(vec.UseDevice());
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(vec_ptr[i], data[i]);
  }
}

// ============================================================================
// Swap and MakeRef Tests
// ============================================================================

TYPED_TEST(MArrayTest, Swap) {
  DVector<int> vec1(this->kMemType, 5);
  DVector<int> vec2(this->kMemType, 5);

  vec1 << 1, 2, 3, 4, 5;
  vec2 << 10, 20, 30, 40, 50;

  Swap(vec1, vec2);

  const int* vec1_ptr = vec1.Read(vec1.UseDevice());
  const int* vec2_ptr = vec2.Read(vec2.UseDevice());

  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(vec1_ptr[i], 10 * (i + 1));
    EXPECT_EQ(vec2_ptr[i], i + 1);
  }
}

TYPED_TEST(MArrayTest, MakeRef) {
  DVector<int> vec1(this->kMemType, 5);
  vec1 << 1, 2, 3, 4, 5;

  DVector<int> vec2(this->kMemType, 5);
  vec1.MakeRef(vec2);

  EXPECT_EQ(vec2.GetSize(), vec1.GetSize());

  // Modify vec2 and verify vec1 is affected (reference)
  int* vec2_ptr = vec2.Write(vec2.UseDevice());
  vec2_ptr[0] = 999;

  const int* vec1_ptr = vec1.Read(vec1.UseDevice());
  EXPECT_EQ(vec1_ptr[0], 999);
}

// ============================================================================
// Save and Load Tests
// ============================================================================

TYPED_TEST(MArrayTest, SaveAndLoad) {
  DVector<int> vec(this->kMemType, 5);
  int* vec_ptr = vec.Write(vec.UseDevice());
  for (int i = 0; i < 5; ++i) {
    vec_ptr[i] = i * 10;
  }

  // Save to stringstream
  std::stringstream ss;
  vec.Save(ss);

  // Load into new array
  DVector<int> vec2;
  vec2.Load(ss);

  EXPECT_EQ(vec2.GetSize(), 5);
  const int* vec2_ptr = vec2.Read(vec2.UseDevice());
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(vec2_ptr[i], i * 10);
  }
}

TYPED_TEST(MArrayTest, SaveAndLoadMatrix) {
  DMatrix<int> mat(this->kMemType, 2, 3);
  int* mat_ptr = mat.Write(mat.UseDevice());
  for (int i = 0; i < 6; ++i) {
    mat_ptr[i] = i;
  }

  std::stringstream ss;
  mat.Save(ss);

  DMatrix<int> mat2;
  mat2.Load(ss);

  EXPECT_EQ(mat2.GetExtent(0), 2);
  EXPECT_EQ(mat2.GetExtent(1), 3);

  const int* mat2_ptr = mat2.Read(mat2.UseDevice());
  for (int i = 0; i < 6; ++i) {
    EXPECT_EQ(mat2_ptr[i], i);
  }
}

// ============================================================================
// Memory Type Tests
// ============================================================================

TEST(MemoryTest, EmptyDeviceMemoryKeepsHostMetadataValid) {
  Memory<int> memory(MemoryType::kDevice);

  EXPECT_TRUE(IsHostMemory(memory.GetHostMemoryType()));
  EXPECT_EQ(memory.GetMemoryType(), MemoryType::kDevice);

  memory.Delete();
}

TYPED_TEST(MArrayTest, GetMemoryType) {
  DVector<int> vec(this->kMemType, 10);

  // Verify memory type matches what was requested
  EXPECT_EQ(vec.GetMemory().GetMemoryType(), this->kMemType);
}

TYPED_TEST(MArrayTest, UseDevice) {
  DVector<int> vec(this->kMemType, 10);

  EXPECT_FALSE(vec.UseDevice());

  vec.UseDevice(true);
  EXPECT_TRUE(vec.UseDevice());

  vec.UseDevice(false);
  EXPECT_FALSE(vec.UseDevice());
}

// ============================================================================
// Iterator Tests
// ============================================================================

TYPED_TEST(MArrayTest, Iterator1D) {
  SVector<int, 5> vec(this->kMemType);
  int* data = vec.Write(vec.UseDevice());
  for (int i = 0; i < 5; ++i) {
    data[i] = i * 2;
  }
  vec << 0, 2, 4, 6, 8;
  MIterator<MShape<5>> it;
  int count = 0;
  for (; !it.IsExhausted(); ++it) {
    EXPECT_EQ(vec[it.GetOffset()], count * 2);
    count++;
  }
  EXPECT_EQ(count, 5);
}

TYPED_TEST(MArrayTest, Iterator2D) {
  SMatrix<int, 3, 4> mat(this->kMemType);
  int* data = mat.Write(mat.UseDevice());

  MIterator<MShape<3, 4>> it;
  int idx = 0;
  for (; !it.IsExhausted(); ++it) {
    data[it.GetOffset()] = idx++;
  }

  // Verify all elements were set
  for (int j = 0; j < 4; ++j) {
    for (int i = 0; i < 3; ++i) {
      EXPECT_EQ(mat(i, j), j * 3 + i);
    }
  }
}

// ============================================================================
// Print Tests
// ============================================================================

TYPED_TEST(MArrayTest, Print1D) {
  SVector<int, 5> vec(this->kMemType);
  vec << 1, 2, 3, 4, 5;

  std::stringstream ss;
  vec.Print(ss);

  // Just verify it doesn't crash and produces some output
  EXPECT_GT(ss.str().length(), 0);
}

TYPED_TEST(MArrayTest, Print2D) {
  SMatrix<int, 2, 3> mat(this->kMemType);
  mat << 1, 2, 3, 4, 5, 6;

  std::stringstream ss;
  mat.Print(ss);

  EXPECT_GT(ss.str().length(), 0);
}

// ============================================================================
// View Tests - Cross-Rank Views
// ============================================================================

TYPED_TEST(MArrayTest, Vector1DTo2D) {
  DVector<int> vec(this->kMemType, 12);
  vec << 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11;

  // Create 2D view (3x4) of 1D vector - zero copy
  DMatrix<int> mat = vec.View<DShape<2>>(3, 4);

  EXPECT_EQ(mat.GetRank(), 2);
  EXPECT_EQ(mat.GetExtent(0), 3);
  EXPECT_EQ(mat.GetExtent(1), 4);
  EXPECT_EQ(mat.GetSize(), 12);

  // Verify data is shared (zero-copy)
  mat(1, 2) = 999;
  EXPECT_EQ(vec[1 + 2 * 3], 999);  // Column-major: mat(1,2) = vec[7]

  // Verify all elements
  for (int j = 0; j < 4; ++j) {
    for (int i = 0; i < 3; ++i) {
      int offset = i + j * 3;
      if (offset != 7) {
        EXPECT_EQ(mat(i, j), offset);
      }
    }
  }
}

TYPED_TEST(MArrayTest, Vector1DTo3D) {
  DVector<int> vec(this->kMemType, 24);
  for (int i = 0; i < 24; ++i) {
    vec[i] = i;
  }

  // Create 3D view (2x3x4) of 1D vector
  auto tensor = vec.View<DShape<3>>(2, 3, 4);

  EXPECT_EQ(tensor.GetRank(), 3);
  EXPECT_EQ(tensor.GetExtent(0), 2);
  EXPECT_EQ(tensor.GetExtent(1), 3);
  EXPECT_EQ(tensor.GetExtent(2), 4);
  EXPECT_EQ(tensor.GetSize(), 24);

  // Verify data is shared
  tensor(1, 1, 1) = 888;
  int expected_idx = 1 + 1 * 2 + 1 * 2 * 3;  // Column-major
  EXPECT_EQ(vec[expected_idx], 888);
}

TYPED_TEST(MArrayTest, Matrix2DTo1D) {
  DMatrix<int> mat(this->kMemType, 3, 4);
  int* data = mat.Write(mat.UseDevice());
  for (int i = 0; i < 12; ++i) {
    data[i] = i * 10;
  }

  // Create 1D view of 2D matrix
  auto vec = mat.View<DShape<1>>(12);

  EXPECT_EQ(vec.GetRank(), 1);
  EXPECT_EQ(vec.GetSize(), 12);

  // Verify data is shared
  vec[5] = 777;
  EXPECT_EQ(mat.Read(mat.UseDevice())[5], 777);

  // Verify all elements
  for (int i = 0; i < 12; ++i) {
    if (i != 5) {
      EXPECT_EQ(vec[i], i * 10);
    }
  }
}

TYPED_TEST(MArrayTest, Matrix2DTo3D) {
  DMatrix<real_t> mat(this->kMemType, 6, 2);
  for (int i = 0; i < 12; ++i) {
    mat.Write(mat.UseDevice())[i] = static_cast<real_t>(i);
  }

  // Reshape 2D (6x2) to 3D (2x2x3)
  auto tensor = mat.View<DShape<3>>(2, 2, 3);

  EXPECT_EQ(tensor.GetRank(), 3);
  EXPECT_EQ(tensor.GetSize(), 12);

  // Verify data sharing
  tensor(0, 1, 2) = 12.34_r;
  int idx = 0 + 1 * 2 + 2 * 2 * 2;  // Column-major
  EXPECT_EQ(mat.Read(mat.UseDevice())[idx], 12.34_r);
}

TYPED_TEST(MArrayTest, ViewAsWithShapeObject) {
  DVector<int> vec(this->kMemType, 20);
  for (int i = 0; i < 20; ++i) {
    vec[i] = i;
  }

  // Use shape object instead of variadic extents
  DShape<2> mat_shape(4, 5);
  auto mat = vec.View(mat_shape);

  EXPECT_EQ(mat.GetExtent(0), 4);
  EXPECT_EQ(mat.GetExtent(1), 5);

  // Verify shared data
  mat(2, 3) = 999;
  EXPECT_EQ(vec[2 + 3 * 4], 999);
}

TYPED_TEST(MArrayTest, StaticToStatic) {
  SVector<int, 12> vec(this->kMemType);
  vec << 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11;

  // View static 1D as static 2D
  auto mat = vec.View<MShape<3, 4>>(3, 4);

  EXPECT_EQ(mat.GetRank(), 2);
  EXPECT_EQ(mat.GetSize(), 12);

  // Verify shared data
  mat(2, 1) = 555;
  EXPECT_EQ(vec[2 + 1 * 3], 555);
}

TYPED_TEST(MArrayTest, MultipleViews) {
  DVector<int> vec(this->kMemType, 12);
  for (int i = 0; i < 12; ++i) {
    vec[i] = i;
  }

  // Create multiple views - all share same data
  auto mat = vec.View<DShape<2>>(3, 4);
  auto tensor = vec.View<DShape<3>>(2, 2, 3);

  // Modify through matrix view
  mat(1, 1) = 111;

  // Should be visible in vector and tensor
  int idx1 = 1 + 1 * 3;  // mat(1,1) in column-major
  EXPECT_EQ(vec[idx1], 111);

  // Find same element in tensor view
  // We need to find which tensor indices map to linear index 4
  // For shape (2,2,3): linear = i + j*2 + k*2*2
  // So idx1=4 means: 4 = i + j*2 + k*4
  // One solution: i=0, j=0, k=1
  EXPECT_EQ(tensor(0, 0, 1), 111);
}

TYPED_TEST(MArrayTest, ConstView) {
  DVector<int> vec(this->kMemType, 6);
  vec << 0, 1, 2, 3, 4, 5;

  const auto& const_vec = vec;

  // Create const view
  auto mat = const_vec.View<DShape<2>>(2, 3);

  EXPECT_EQ(mat(0, 1), 2);  // Column-major: (0,1) -> index 2
  EXPECT_EQ(mat(1, 2), 5);  // Column-major: (1,2) -> index 5
}

// ============================================================================
// Transpose and Permute Tests
// ============================================================================

TYPED_TEST(MArrayTest, TransposeBasic) {
  DMatrix<int> mat(this->kMemType, 3, 4);
  // Fill in column-major order
  mat << 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11;

  // Transpose: 3x4 -> 4x3
  auto mat_T = mat.Transpose();

  EXPECT_EQ(mat_T.GetRank(), 2);
  EXPECT_EQ(mat_T.GetExtent(0), 4);
  EXPECT_EQ(mat_T.GetExtent(1), 3);
  EXPECT_EQ(mat_T.GetSize(), 12);

  // Verify transposition: mat_T(i, j) == mat(j, i)
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 3; ++j) {
      EXPECT_EQ(mat_T(i, j), mat(j, i))
          << "Mismatch at mat_T(" << i << ", " << j << ")";
    }
  }
}

TYPED_TEST(MArrayTest, TransposeZeroCopy) {
  DMatrix<int> mat(this->kMemType, 2, 3);
  mat << 1, 2, 3, 4, 5, 6;

  auto mat_T = mat.Transpose();

  // Verify zero-copy: modifying transpose affects original
  mat_T(1, 0) = 999;  // mat_T(1, 0) corresponds to mat(0, 1)

  EXPECT_EQ(mat(0, 1), 999);
  EXPECT_EQ(mat_T(1, 0), 999);
}

TYPED_TEST(MArrayTest, TransposeSquareMatrix) {
  DMatrix<int> mat(this->kMemType, 3, 3);
  mat << 1, 2, 3, 4, 5, 6, 7, 8, 9;

  auto mat_T = mat.Transpose();

  // Verify diagonal unchanged
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(mat_T(i, i), mat(i, i));
  }

  // Verify off-diagonal swapped
  EXPECT_EQ(mat_T(0, 1), mat(1, 0));
  EXPECT_EQ(mat_T(0, 2), mat(2, 0));
  EXPECT_EQ(mat_T(1, 2), mat(2, 1));
}

TYPED_TEST(MArrayTest, TransposeIdentity) {
  DMatrix<real_t> mat(this->kMemType, 3, 3);
  mat.SetIdentity();

  auto mat_T = mat.Transpose();

  // Identity matrix should be unchanged after transpose
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      EXPECT_EQ(mat_T(i, j), mat(i, j));
      if (i == j) {
        EXPECT_EQ(mat_T(i, j), 1.0_r);
      } else {
        EXPECT_EQ(mat_T(i, j), 0.0_r);
      }
    }
  }
}

TYPED_TEST(MArrayTest, TransposeStatic) {
  SMatrix<int, 2, 3> mat(this->kMemType);
  mat << 10, 20, 30, 40, 50, 60;

  SMatrixView<int, 3, 2> mat_T = mat.Transpose();

  EXPECT_EQ(mat_T.GetExtent(0), 3);
  EXPECT_EQ(mat_T.GetExtent(1), 2);

  // Verify transposition: mat_T(i,j) == mat(j,i)
  // Original mat (2x3): [[10,30,50], [20,40,60]]
  // Transposed (3x2):   [[10,20], [30,40], [50,60]]
  EXPECT_EQ(mat_T(0, 0), 10);  // mat_T(0,0) = mat(0,0)
  EXPECT_EQ(mat_T(0, 1), 20);  // mat_T(0,1) = mat(1,0)
  EXPECT_EQ(mat_T(1, 0), 30);  // mat_T(1,0) = mat(0,1)
  EXPECT_EQ(mat_T(1, 1), 40);  // mat_T(1,1) = mat(1,1)
  EXPECT_EQ(mat_T(2, 0), 50);  // mat_T(2,0) = mat(0,2)
  EXPECT_EQ(mat_T(2, 1), 60);  // mat_T(2,1) = mat(1,2)
}

TYPED_TEST(MArrayTest, Permute2D) {
  DMatrix<int> mat(this->kMemType, 3, 4);
  mat << 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11;

  // Permutation (0, 1) -> (1, 0) is equivalent to transpose
  DMatrixView<int> mat_perm = mat.Permute<1, 0>();

  EXPECT_EQ(mat_perm.GetExtent(0), 4);
  EXPECT_EQ(mat_perm.GetExtent(1), 3);

  // Verify same as transpose
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 3; ++j) {
      EXPECT_EQ(mat_perm(i, j), mat(j, i));
    }
  }
}

TYPED_TEST(MArrayTest, Permute3D_Swap01) {
  DTensor<int, 3> tensor(this->kMemType, 2, 3, 4);

  // Initialize with recognizable pattern
  int* data = tensor.Write(tensor.UseDevice());
  for (int k = 0; k < 4; ++k) {
    for (int j = 0; j < 3; ++j) {
      for (int i = 0; i < 2; ++i) {
        int idx = i + j * 2 + k * 2 * 3;
        data[idx] = i * 100 + j * 10 + k;
      }
    }
  }

  // Permutation: (0, 1, 2) -> (1, 0, 2) - swap first two dimensions
  DTensorView<int, 3> tensor_perm = tensor.Permute<1, 0, 2>();

  EXPECT_EQ(tensor_perm.GetExtent(0), 3);
  EXPECT_EQ(tensor_perm.GetExtent(1), 2);
  EXPECT_EQ(tensor_perm.GetExtent(2), 4);

  // Verify permutation: tensor_perm(j, i, k) == tensor(i, j, k)
  for (int k = 0; k < 4; ++k) {
    for (int j = 0; j < 3; ++j) {
      for (int i = 0; i < 2; ++i) {
        EXPECT_EQ(tensor_perm(j, i, k), tensor(i, j, k));
      }
    }
  }
}

TYPED_TEST(MArrayTest, Permute3D_CyclicRotation) {
  DTensor<int, 3> tensor(this->kMemType, 2, 3, 4);

  int* data = tensor.Write(tensor.UseDevice());
  for (int k = 0; k < 4; ++k) {
    for (int j = 0; j < 3; ++j) {
      for (int i = 0; i < 2; ++i) {
        int idx = i + j * 2 + k * 2 * 3;
        data[idx] = idx;
      }
    }
  }

  // Cyclic rotation: (0, 1, 2) -> (2, 0, 1)
  DTensorView<int, 3> tensor_perm = tensor.Permute<2, 0, 1>();

  EXPECT_EQ(tensor_perm.GetExtent(0), 4);  // Was dim 2
  EXPECT_EQ(tensor_perm.GetExtent(1), 2);  // Was dim 0
  EXPECT_EQ(tensor_perm.GetExtent(2), 3);  // Was dim 1

  // Verify: tensor_perm(k, i, j) == tensor(i, j, k)
  for (int k = 0; k < 4; ++k) {
    for (int j = 0; j < 3; ++j) {
      for (int i = 0; i < 2; ++i) {
        EXPECT_EQ(tensor_perm(k, i, j), tensor(i, j, k));
      }
    }
  }
}

TYPED_TEST(MArrayTest, Permute3D_Reverse) {
  DTensor<int, 3> tensor(this->kMemType, 2, 3, 4);

  int* data = tensor.Write(tensor.UseDevice());
  for (int k = 0; k < 4; ++k) {
    for (int j = 0; j < 3; ++j) {
      for (int i = 0; i < 2; ++i) {
        int idx = i + j * 2 + k * 2 * 3;
        data[idx] = i * 100 + j * 10 + k;
      }
    }
  }

  // Reverse dimensions: (0, 1, 2) -> (2, 1, 0)
  DTensorView<int, 3> tensor_perm = tensor.Permute<2, 1, 0>();

  EXPECT_EQ(tensor_perm.GetExtent(0), 4);
  EXPECT_EQ(tensor_perm.GetExtent(1), 3);
  EXPECT_EQ(tensor_perm.GetExtent(2), 2);

  // Verify: tensor_perm(k, j, i) == tensor(i, j, k)
  for (int k = 0; k < 4; ++k) {
    for (int j = 0; j < 3; ++j) {
      for (int i = 0; i < 2; ++i) {
        EXPECT_EQ(tensor_perm(k, j, i), tensor(i, j, k));
      }
    }
  }
}

TYPED_TEST(MArrayTest, PermuteZeroCopy) {
  DTensor<int, 3> tensor(this->kMemType, 2, 3, 4);
  tensor = 0;
  tensor(1, 2, 3) = 42;

  DTensorView<int, 3> tensor_perm = tensor.Permute<2, 0, 1>();

  // Verify zero-copy: tensor(1, 2, 3) = tensor_perm(3, 1, 2)
  EXPECT_EQ(tensor_perm(3, 1, 2), 42);

  // Modify through permuted view
  tensor_perm(0, 0, 0) = 999;

  // Should affect original: tensor(0, 0, 0) = tensor_perm(0, 0, 0)
  EXPECT_EQ(tensor(0, 0, 0), 999);
}

TYPED_TEST(MArrayTest, PermuteIdentityPermutation) {
  DMatrix<int> mat(this->kMemType, 3, 4);
  mat << 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11;

  // Identity permutation: (0, 1) -> (0, 1)
  DMatrixView<int> mat_perm = mat.Permute<0, 1>();
  EXPECT_EQ(mat_perm.GetExtent(0), 3);
  EXPECT_EQ(mat_perm.GetExtent(1), 4);

  // Should be identical to original
  for (int j = 0; j < 4; ++j) {
    for (int i = 0; i < 3; ++i) {
      EXPECT_EQ(mat_perm(i, j), mat(i, j));
    }
  }
}

// ============================================================================
// DenseMArray Slice Tests
// ============================================================================

TYPED_TEST(MArrayTest, SliceMatrixColumn) {
  DMatrix<int> mat(this->kMemType, 3, 4);
  mat << 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11;

  // Extract column 2 (third column in column-major layout)
  auto col = mat.Slice(1, 2);

  EXPECT_EQ(col.GetRank(), 1);
  EXPECT_EQ(col.GetSize(), 3);

  EXPECT_EQ(col(0), 6);
  EXPECT_EQ(col(1), 7);
  EXPECT_EQ(col(2), 8);
}

TYPED_TEST(MArrayTest, SliceMatrixRow) {
  DMatrix<int> mat(this->kMemType, 3, 4);
  mat << 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11;

  // Extract row 1 (second row)
  auto row = mat.Slice(0, 1);

  EXPECT_EQ(row.GetRank(), 1);
  EXPECT_EQ(row.GetSize(), 4);

  // For row slices (strided in column-major), use indexed access
  // instead of raw pointer access to account for strides
  EXPECT_EQ(row(0), 1);
  EXPECT_EQ(row(1), 4);
  EXPECT_EQ(row(2), 7);
  EXPECT_EQ(row(3), 10);
}

TYPED_TEST(MArrayTest, SliceZeroCopy) {
  DMatrix<int> mat(this->kMemType, 3, 4);
  mat = 0;
  mat(2, 3) = 42;

  // Extract column 3
  auto col = mat.Slice(1, 3);

  // Verify zero-copy: col[2] should be mat(2, 3)
  const int* col_data = col.Read(col.UseDevice());
  EXPECT_EQ(col_data[2], 42);

  // Modify through slice
  int* col_write = col.Write(col.UseDevice());
  col_write[0] = 999;

  // Should affect original matrix
  EXPECT_EQ(mat(0, 3), 999);
}

TYPED_TEST(MArrayTest, Slice3DTensor) {
  DTensor<int, 3> tensor(this->kMemType, 2, 3, 4);
  // Initialize tensor with unique values
  int* data = tensor.Write(tensor.UseDevice());
  for (int i = 0; i < 24; ++i) {
    data[i] = i;
  }

  // Slice along dimension 2 (third dimension), index 1
  auto slice = tensor.Slice(2, 1);

  EXPECT_EQ(slice.GetRank(), 2);
  EXPECT_EQ(slice.GetExtent(0), 2);
  EXPECT_EQ(slice.GetExtent(1), 3);

  // Verify values: slice(i, j) should be tensor(i, j, 1)
  for (int j = 0; j < 3; ++j) {
    for (int i = 0; i < 2; ++i) {
      EXPECT_EQ(slice(i, j), tensor(i, j, 1));
    }
  }
}

TYPED_TEST(MArrayTest, SliceZeroCopyBidirectional) {
  // Test that modifications propagate both ways between slice and original
  DMatrix<int> mat(this->kMemType, 4, 5);
  mat = 0;

  // Extract column 2
  auto col = mat.Slice(1, 2);

  // Modify original, check slice reflects it
  mat(1, 2) = 100;
  EXPECT_EQ(col(1), 100);

  // Modify slice, check original reflects it
  col(3) = 200;
  EXPECT_EQ(mat(3, 2), 200);

  // Multiple modifications
  for (int i = 0; i < 4; ++i) {
    col(i) = i * 10;
  }
  for (int i = 0; i < 4; ++i) {
    EXPECT_EQ(mat(i, 2), i * 10);
  }
}

TYPED_TEST(MArrayTest, TransposeZeroCopyModification) {
  // Test that transpose creates a zero-copy view
  SMatrix<int, 3, 4> mat(this->kMemType);
  mat = 0;
  mat(1, 2) = 42;

  // Create transpose view
  auto mat_T = mat.Transpose();

  // Verify zero-copy: mat_T(2, 1) should be mat(1, 2)
  EXPECT_EQ(mat_T(2, 1), 42);

  // Modify through transpose
  mat_T(3, 0) = 99;
  EXPECT_EQ(mat(0, 3), 99);

  // Modify original, check transpose reflects it
  mat(2, 1) = 77;
  EXPECT_EQ(mat_T(1, 2), 77);
}

TYPED_TEST(MArrayTest, PermuteZeroCopyModification) {
  // Test that permute creates a zero-copy view
  DTensor<int, 3> tensor(this->kMemType, 2, 3, 4);
  tensor = 0;
  tensor(1, 2, 3) = 123;

  // Create permuted view: (0, 1, 2) -> (2, 0, 1)
  auto tensor_perm = tensor.template Permute<2, 0, 1>();

  // Verify shape is permuted
  EXPECT_EQ(tensor_perm.GetExtent(0), 4);
  EXPECT_EQ(tensor_perm.GetExtent(1), 2);
  EXPECT_EQ(tensor_perm.GetExtent(2), 3);

  // Verify zero-copy: tensor_perm(3, 1, 2) should be tensor(1, 2, 3)
  EXPECT_EQ(tensor_perm(3, 1, 2), 123);

  // Modify through permuted view
  tensor_perm(2, 0, 1) = 456;
  EXPECT_EQ(tensor(0, 1, 2), 456);

  // Modify original, check permuted view reflects it
  tensor(1, 0, 3) = 789;
  EXPECT_EQ(tensor_perm(3, 1, 0), 789);
}

TYPED_TEST(MArrayTest, ViewZeroCopyModification) {
  // Test that View creates a zero-copy view
  DVector<int> vec(this->kMemType, 12);
  for (int i = 0; i < 12; ++i) {
    vec[i] = i;
  }

  // Reshape to 3x4 matrix using View
  auto mat = vec.template View<DShape<2>>(3, 4);

  // Verify zero-copy: mat(1, 2) should be vec[7] (column-major)
  EXPECT_EQ(mat(1, 2), 7);

  // Modify through view
  mat(2, 1) = 999;
  EXPECT_EQ(vec[5], 999);  // Column-major: (2, 1) -> 2 + 1*3 = 5

  // Modify original, check view reflects it
  vec[0] = 888;
  EXPECT_EQ(mat(0, 0), 888);
}

TYPED_TEST(MArrayTest, MultipleViewsShareMemory) {
  // Test that multiple views of the same array all share memory
  DMatrix<int> mat(this->kMemType, 3, 4);
  mat = 0;

  // Create multiple views
  auto col1 = mat.Slice(1, 1);
  auto col2 = mat.Slice(1, 2);
  auto transpose = mat.Transpose();

  // Modify through first view
  col1(1) = 111;
  EXPECT_EQ(mat(1, 1), 111);
  EXPECT_EQ(transpose(1, 1), 111);

  // Modify through second view
  col2(2) = 222;
  EXPECT_EQ(mat(2, 2), 222);
  EXPECT_EQ(transpose(2, 2), 222);

  // Modify through transpose
  transpose(3, 0) = 333;
  EXPECT_EQ(mat(0, 3), 333);

  // All views should reflect all changes
  mat(1, 2) = 444;
  EXPECT_EQ(col2(1), 444);
  EXPECT_EQ(transpose(2, 1), 444);
}

TYPED_TEST(MArrayTest, NestedViewsZeroCopy) {
  // Test that views of views still maintain zero-copy semantics
  DTensor<int, 3> tensor(this->kMemType, 3, 4, 5);
  tensor = 0;

  // Create a slice to get a 2D matrix
  auto mat = tensor.Slice(2, 2);  // Fix dimension 2 at index 2
  EXPECT_EQ(mat.GetRank(), 2);
  EXPECT_EQ(mat.GetExtent(0), 3);
  EXPECT_EQ(mat.GetExtent(1), 4);

  // Create a slice of the slice to get a 1D vector
  auto col = mat.Slice(1, 1);  // Fix dimension 1 at index 1
  EXPECT_EQ(col.GetRank(), 1);
  EXPECT_EQ(col.GetExtent(0), 3);

  // Modify through nested view
  col(1) = 777;

  // Should propagate all the way back to original
  EXPECT_EQ(mat(1, 1), 777);
  EXPECT_EQ(tensor(1, 1, 2), 777);

  // Modify original, check nested views reflect it
  tensor(2, 1, 2) = 888;
  EXPECT_EQ(mat(2, 1), 888);
  EXPECT_EQ(col(2), 888);
}

// ============================================================================
// DenseMArray Broadcasting Tests
// ============================================================================

TYPED_TEST(MArrayTest, BroadcastAddSameSize) {
  DVector<int> a(this->kMemType, 5);
  DVector<int> b(this->kMemType, 5);
  a << 1, 2, 3, 4, 5;
  b << 10, 20, 30, 40, 50;

  a += b;

  const int* result = a.Read(a.UseDevice());
  EXPECT_EQ(result[0], 11);
  EXPECT_EQ(result[1], 22);
  EXPECT_EQ(result[2], 33);
  EXPECT_EQ(result[3], 44);
  EXPECT_EQ(result[4], 55);
}

TYPED_TEST(MArrayTest, BroadcastSameShapeAcrossLayoutsUsesLogicalIndices) {
  DMatrix<int, LayoutLeft> col_major(this->kMemType, 2, 3);
  DMatrix<int, LayoutRight> row_major(this->kMemType, 2, 3);

  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 3; ++j) {
      col_major(i, j) = 10 * i + j;
      row_major(i, j) = 100 + 10 * i + j;
    }
  }

  col_major += row_major;

  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 3; ++j) {
      EXPECT_EQ(col_major(i, j), 100 + 20 * i + 2 * j);
    }
  }
}

TYPED_TEST(MArrayTest, AssignmentAcrossLayoutsResizesDynamicDestination) {
  DMatrix<int, LayoutLeft> col_major(this->kMemType, 2, 3);
  DMatrix<int, LayoutRight> row_major(this->kMemType);

  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 3; ++j) {
      col_major(i, j) = 10 * i + j;
    }
  }

  row_major = col_major;

  EXPECT_EQ(row_major.GetExtent(0), 2);
  EXPECT_EQ(row_major.GetExtent(1), 3);
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 3; ++j) {
      EXPECT_EQ(row_major(i, j), 10 * i + j);
    }
  }
}

TYPED_TEST(MArrayTest, ViewBroadcastOperations) {
  // Test that views can participate in broadcast operations
  DMatrix<int> mat(this->kMemType, 3, 4);
  mat << 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12;

  // Extract a column
  auto col = mat.Slice(1, 1);  // Column 1: [4, 5, 6]
  EXPECT_EQ(col.GetSize(), 3);

  // Create another vector for operation
  DVector<int> vec(this->kMemType, 3);
  vec << 100, 200, 300;

  // Perform operation on the view
  col += vec;

  // Verify the column was modified
  EXPECT_EQ(col(0), 104);  // 4 + 100
  EXPECT_EQ(col(1), 205);  // 5 + 200
  EXPECT_EQ(col(2), 306);  // 6 + 300

  // Verify the original matrix was also modified (zero-copy)
  EXPECT_EQ(mat(0, 1), 104);
  EXPECT_EQ(mat(1, 1), 205);
  EXPECT_EQ(mat(2, 1), 306);

  // Verify other columns are unchanged
  EXPECT_EQ(mat(0, 0), 1);
  EXPECT_EQ(mat(1, 0), 2);
  EXPECT_EQ(mat(2, 0), 3);
}

TYPED_TEST(MArrayTest, MultipleViewsBroadcast) {
  // Test broadcasting between two views of the same matrix
  DMatrix<int> mat(this->kMemType, 3, 4);
  mat = 1;  // Initialize all to 1

  // Extract two different columns
  auto col0 = mat.Slice(1, 0);
  auto col1 = mat.Slice(1, 1);

  // Set values in first column
  col0 << 10, 20, 30;

  // Add first column to second column
  col1 += col0;

  // Verify second column was updated
  EXPECT_EQ(col1(0), 11);  // 1 + 10
  EXPECT_EQ(col1(1), 21);  // 1 + 20
  EXPECT_EQ(col1(2), 31);  // 1 + 30

  // Verify both columns in original matrix
  EXPECT_EQ(mat(0, 0), 10);
  EXPECT_EQ(mat(1, 0), 20);
  EXPECT_EQ(mat(2, 0), 30);
  EXPECT_EQ(mat(0, 1), 11);
  EXPECT_EQ(mat(1, 1), 21);
  EXPECT_EQ(mat(2, 1), 31);
}

TYPED_TEST(MArrayTest, ViewScalarBroadcast) {
  // Test scalar broadcasting on views
  DMatrix<int> mat(this->kMemType, 4, 5);
  mat = 0;

  // Extract a column
  auto col = mat.Slice(1, 2);
  col << 1, 2, 3, 4;

  // Scalar multiplication
  col *= 10;

  // Verify the view
  EXPECT_EQ(col(0), 10);
  EXPECT_EQ(col(1), 20);
  EXPECT_EQ(col(2), 30);
  EXPECT_EQ(col(3), 40);

  // Verify original matrix reflects changes
  EXPECT_EQ(mat(0, 2), 10);
  EXPECT_EQ(mat(1, 2), 20);
  EXPECT_EQ(mat(2, 2), 30);
  EXPECT_EQ(mat(3, 2), 40);

  // Verify other columns are still zero
  for (int j = 0; j < 5; ++j) {
    if (j != 2) {
      for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(mat(i, j), 0);
      }
    }
  }
}

TYPED_TEST(MArrayTest, TransposedViewBroadcast) {
  // Test that transposed views work with broadcast operations
  SMatrix<int, 3, 4> mat(this->kMemType);
  mat << 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12;

  auto mat_T = mat.Transpose();
  // mat_T is now 4x3 (transposed from 3x4)
  EXPECT_EQ(mat_T.GetExtent(0), 4);
  EXPECT_EQ(mat_T.GetExtent(1), 3);

  // Extract first row from the transposed matrix (which is first column in original)
  auto row = mat_T.Slice(0, 0);  // Fix dimension 0 (rows) at index 0
  EXPECT_EQ(row.GetSize(), 3);

  // Multiply by scalar
  row *= 2;

  // Verify through transpose (first row of transpose: mat_T(0,:))
  EXPECT_EQ(mat_T(0, 0), 2);   // Was 1
  EXPECT_EQ(mat_T(0, 1), 4);   // Was 2
  EXPECT_EQ(mat_T(0, 2), 6);   // Was 3

  // Verify in original matrix (first column should be real_td)
  EXPECT_EQ(mat(0, 0), 2);
  EXPECT_EQ(mat(1, 0), 4);
  EXPECT_EQ(mat(2, 0), 6);

  // Verify other elements unchanged
  EXPECT_EQ(mat(0, 1), 4);
  EXPECT_EQ(mat(1, 1), 5);
  EXPECT_EQ(mat(2, 1), 6);
}

TYPED_TEST(MArrayTest, NestedViewBroadcast) {
  // Test broadcasting on nested views
  DTensor<int, 3> tensor(this->kMemType, 2, 3, 4);
  tensor = 1;

  // Create a 2D slice
  auto mat = tensor.Slice(2, 1);  // Fix 3rd dimension at index 1
  EXPECT_EQ(mat.GetRank(), 2);

  // Create a 1D slice from the 2D slice
  auto col = mat.Slice(1, 1);  // Fix 2nd dimension at index 1
  EXPECT_EQ(col.GetRank(), 1);
  EXPECT_EQ(col.GetSize(), 2);

  // Modify through scalar broadcast
  col += 99;

  // Verify through nested view
  EXPECT_EQ(col(0), 100);
  EXPECT_EQ(col(1), 100);

  // Verify through intermediate view
  EXPECT_EQ(mat(0, 1), 100);
  EXPECT_EQ(mat(1, 1), 100);

  // Verify in original tensor
  EXPECT_EQ(tensor(0, 1, 1), 100);
  EXPECT_EQ(tensor(1, 1, 1), 100);

  // Verify other elements unchanged
  EXPECT_EQ(tensor(0, 0, 0), 1);
  EXPECT_EQ(tensor(0, 0, 1), 1);
  EXPECT_EQ(tensor(1, 0, 1), 1);
}

// ============================================================================
// DenseMArray Reduction Operation Tests
// ============================================================================

TYPED_TEST(MArrayTest, NormL0) {
  DVector<real_t> x(this->kMemType, 5);
  x << 1.0, -2.0, 3.0, -4.0, 5.0;

  // L0 "norm": total number of elements
  real_t l0 = x.Norm(0);
  EXPECT_REAL_EQ(l0, 5.0);
}

TYPED_TEST(MArrayTest, NormL1) {
  DVector<real_t> x(this->kMemType, 5);
  x << 1.0, -2.0, 3.0, -4.0, 5.0;

  // Test member function
  real_t l1 = x.Norm(1);
  EXPECT_REAL_EQ(l1, 15.0);
}

TYPED_TEST(MArrayTest, NormL2) {
  DVector<real_t> x(this->kMemType, 3);
  x << 3.0, 4.0, 0.0;

  // Test member function with explicit p=2
  real_t l2 = x.Norm(2);
  EXPECT_REAL_EQ(l2, 5.0);

  // Test member function with default parameter
  real_t l2_default = x.Norm();
  EXPECT_REAL_EQ(l2_default, 5.0);
}

TYPED_TEST(MArrayTest, NormLinf) {
  DVector<real_t> x(this->kMemType, 5);
  x << 1.0, -7.0, 3.0, -4.0, 5.0;

  // L-infinity norm: maximum absolute value
  real_t linf = x.Norm(-1);
  EXPECT_REAL_EQ(linf, 7.0);
}

TYPED_TEST(MArrayTest, NormLp) {
  DVector<real_t> x(this->kMemType, 3);
  x << 1.0, 2.0, 2.0;

  // L3 norm: (|x1|^3 + |x2|^3 + |x3|^3)^(1/3)
  real_t l3 = x.Norm(3);
  EXPECT_NEAR(l3, std::pow(1.0 + 8.0 + 8.0, 1.0 / 3.0), kRealTolerance);
}

TYPED_TEST(MArrayTest, NormMatrix) {
  DMatrix<real_t> mat(this->kMemType, 2, 3);
  mat << 1.0, 2.0, 3.0, 4.0, 5.0, 6.0;

  // L2 norm of all elements (Frobenius norm)
  real_t l2 = mat.Norm(2);
  real_t expected = std::sqrt(1.0 + 4.0 + 9.0 + 16.0 + 25.0 + 36.0);
  EXPECT_NEAR(l2, expected, kRealTolerance);

  // L1 norm of all elements
  real_t l1 = mat.Norm(1);
  EXPECT_REAL_EQ(l1, 21.0);
}

TYPED_TEST(MArrayTest, NormLayoutAware) {
  // Test that Norm works correctly with sliced views
  DMatrix<real_t> mat(this->kMemType, 3, 2);
  // Column-major initialization: (0,0)=1, (1,0)=2, (2,0)=3, (0,1)=4, (1,1)=5, (2,1)=6
  mat << 1.0, 2.0, 3.0, 4.0, 5.0, 6.0;

  // Full matrix norm as reference
  real_t full_norm = mat.Norm(2);
  real_t expected_full = std::sqrt(1.0 + 4.0 + 9.0 + 16.0 + 25.0 + 36.0);
  EXPECT_NEAR(full_norm, expected_full, kRealTolerance);

  // Test column slice (contiguous in column-major layout)
  auto col0 = mat.Slice(1, 0);
  real_t col0_norm = col0.Norm(2);
  // Column 0 contains: mat(0, 0)=1, mat(1, 0)=2, mat(2, 0)=3
  real_t expected_col0 = std::sqrt(1.0 + 4.0 + 9.0);
  EXPECT_NEAR(col0_norm, expected_col0, kRealTolerance);
}

TYPED_TEST(MArrayTest, NormLayoutRowAware) {
  // Test that Norm works correctly with sliced views
  DMatrix<real_t> mat(this->kMemType, 3, 2);
  // Column-major initialization: (0,0)=1, (1,0)=2, (2,0)=3, (0,1)=4, (1,1)=5, (2,1)=6
  mat << 1.0, 2.0, 3.0, 4.0, 5.0, 6.0;

  // Test column slice (contiguous in column-major layout)
  auto col0 = mat.Slice(0, 0);
  real_t col0_norm = col0.Norm(2);
  // Column 0 contains: mat(0, 0)=1, mat(0, 1)=4
  real_t expected_col0 = std::sqrt(1.0 + 16.0);
  EXPECT_NEAR(col0_norm, expected_col0, kRealTolerance);
}

TYPED_TEST(MArrayTest, Sum) {
  DVector<real_t> x(this->kMemType, 5);
  x << 1.0, 2.0, 3.0, 4.0, 5.0;

  real_t sum = x.Sum();
  EXPECT_REAL_EQ(sum, 15.0);
}

TYPED_TEST(MArrayTest, Product) {
  DVector<real_t> x(this->kMemType, 5);
  x << 1.0, 2.0, 3.0, 4.0, 5.0;

  real_t prod = x.Product();
  EXPECT_REAL_EQ(prod, 120.0);
}

TYPED_TEST(MArrayTest, Mean) {
  DVector<real_t> x(this->kMemType, 5);
  x << 1.0, 2.0, 3.0, 4.0, 5.0;

  real_t mean = x.Mean();
  EXPECT_REAL_EQ(mean, 3.0);
}

TYPED_TEST(MArrayTest, MaximumReduction) {
  DVector<real_t> x(this->kMemType, 5);
  x << 3.0, 7.0, 2.0, 9.0, 4.0;

  real_t max_val = x.Maximum();
  EXPECT_REAL_EQ(max_val, 9.0);
}

TYPED_TEST(MArrayTest, MinimumReduction) {
  DVector<real_t> x(this->kMemType, 5);
  x << 3.0, 7.0, 2.0, 9.0, 4.0;

  real_t min_val = x.Minimum();
  EXPECT_REAL_EQ(min_val, 2.0);
}

TYPED_TEST(MArrayTest, DotAcrossLayoutsUsesLogicalIndices) {
  DMatrix<real_t, LayoutLeft> col_major(this->kMemType, 2, 3);
  DMatrix<real_t, LayoutRight> row_major(this->kMemType, 2, 3);

  real_t expected = 0.0_r;
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 3; ++j) {
      col_major(i, j) = static_cast<real_t>(10 * i + j);
      row_major(i, j) = static_cast<real_t>(100 + 10 * i + j);
      expected += col_major(i, j) * row_major(i, j);
    }
  }

  EXPECT_REAL_EQ(col_major.Dot(row_major), expected);
}

// ============================================================================
// Scalar Type Tests - Rank-0 Tensor (DenseMArray<T, DShape<0>>)
// ============================================================================

TYPED_TEST(MArrayTest, ScalarTypeConstruction) {
  // Scalar is an alias for DenseMArray<T, DShape<0>> - a rank-0 tensor
  Scalar<real_t> s(this->kMemType);

  EXPECT_EQ(s.GetRank(), 0);
  EXPECT_EQ(s.GetSize(), 1);
  EXPECT_FALSE(s.IsEmpty());
}

TYPED_TEST(MArrayTest, ScalarTypeValueAssignment) {
  Scalar<real_t> s(this->kMemType);

  // Assign value using scalar assignment operator
  s = 3.14;

  // Read value back
  EXPECT_NEAR(s.Read(s.UseDevice())[0], 3.14, kRealTolerance);
}

TYPED_TEST(MArrayTest, ScalarTypeElementAccess) {
  Scalar<int> s(this->kMemType);

  // Set value via linear indexing
  s[0] = 42;

  // Read value back
  EXPECT_EQ(s[0], 42);
  EXPECT_EQ(s.Read(s.UseDevice())[0], 42);
}

TYPED_TEST(MArrayTest, ScalarTypeInPlaceOps) {
  Scalar<real_t> s(this->kMemType);
  s = 8.0;

  // In-place addition with scalar value
  s += 2.0;
  EXPECT_NEAR(s.Read(s.UseDevice())[0], 10.0, kRealTolerance);

  // In-place subtraction
  s -= 3.0;
  EXPECT_NEAR(s.Read(s.UseDevice())[0], 7.0, kRealTolerance);

  // In-place multiplication
  s *= 2.0;
  EXPECT_NEAR(s.Read(s.UseDevice())[0], 14.0, kRealTolerance);

  // In-place division
  s /= 2.0;
  EXPECT_NEAR(s.Read(s.UseDevice())[0], 7.0, kRealTolerance);
}

TYPED_TEST(MArrayTest, ScalarTypeCopyAndMove) {
  Scalar<int> s1(this->kMemType);
  s1 = 99;

  // Copy construction
  Scalar<int> s2(s1);
  EXPECT_EQ(s2[0], 99);

  // Move construction
  Scalar<int> s3(std::move(s1));
  EXPECT_EQ(s3[0], 99);

  // Copy assignment
  Scalar<int> s4(this->kMemType);
  s4 = s2;
  EXPECT_EQ(s4[0], 99);

  // Move assignment
  Scalar<int> s5(this->kMemType);
  s5 = std::move(s3);
  EXPECT_EQ(s5[0], 99);
}

TYPED_TEST(MArrayTest, ScalarTypeNorm) {
  Scalar<real_t> s(this->kMemType);
  s = 3.0;

  // L2 norm of a scalar is its absolute value
  real_t norm = s.Norm(2);
  EXPECT_NEAR(norm, 3.0, kRealTolerance);

  s = -4.0;
  norm = s.Norm(2);
  EXPECT_NEAR(norm, 4.0, kRealTolerance);
}

TYPED_TEST(MArrayTest, ScalarTypeWithDifferentTypes) {
  // Test Scalar with different element types
  Scalar<int> si(this->kMemType);
  Scalar<float> sf(this->kMemType);
  Scalar<real_t> sd(this->kMemType);

  si = 10;
  sf = 3.14f;
  sd = 2.718;

  EXPECT_EQ(si[0], 10);
  EXPECT_NEAR(sf[0], 3.14f, 1e-6f);
  EXPECT_NEAR(sd[0], 2.718, kRealTolerance);
}

TYPED_TEST(MArrayTest, ScalarTypeSerialization) {
  Scalar<real_t> s1(this->kMemType);
  s1 = 123.456;

  // Save to stream
  std::ostringstream oss;
  s1.Save(oss);

  // Load from stream
  Scalar<real_t> s2(this->kMemType);
  std::istringstream iss(oss.str());
  s2.Load(iss);

  EXPECT_NEAR(s2[0], 123.456, kRealTolerance);
}

TYPED_TEST(MArrayTest, ScalarTypeMemoryOperations) {
  Scalar<real_t> s(this->kMemType);

  // Test Write access
  s.Write(s.UseDevice())[0] = 7.89;
  EXPECT_NEAR(s.Read(s.UseDevice())[0], 7.89, kRealTolerance);

  // Test ReadWrite access
  s.ReadWrite(s.UseDevice())[0] += 1.0;
  EXPECT_NEAR(s.Read(s.UseDevice())[0], 8.89, kRealTolerance);
}

// ============================================================================
// Comparison Operators
// ============================================================================

TYPED_TEST(MArrayTest, Equality) {
  DVector<real_t> a(this->kMemType, 4);
  DVector<real_t> b(this->kMemType, 4);
  DVector<real_t> c(this->kMemType, 4);
  a << 1.0, 2.0, 3.0, 4.0;
  b << 1.0, 2.0, 3.0, 4.0;
  c << 1.0, 2.0, 3.0, 5.0;

  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a == c);
}

TYPED_TEST(MArrayTest, Inequality) {
  DVector<real_t> a(this->kMemType, 4);
  DVector<real_t> b(this->kMemType, 4);
  DVector<real_t> c(this->kMemType, 4);
  a << 1.0, 2.0, 3.0, 4.0;
  b << 1.0, 2.0, 3.0, 4.0;
  c << 1.0, 2.0, 3.0, 5.0;

  EXPECT_FALSE(a != b);
  EXPECT_TRUE(a != c);
}

}  // namespace asc
