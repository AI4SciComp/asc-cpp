// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: tests/unit_test_carray.cc
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#include <gtest/gtest.h>
#include <sstream>
#include "common/common.h"
#include "asc/array/carray.h"

namespace asc {

// ============================================================================
// CPointer type tests
// ============================================================================

// Test CPointer type generation
TEST(CArrayTest, CPointerType) {
  // Test that CPointer<int, 1> is int*
  static_assert(std::is_same<CPointer<int, 1>, int*>::value,
                "CPointer<int, 1> should be int*");

  // Test that CPointer<int, 2> is int**
  static_assert(std::is_same<CPointer<int, 2>, int**>::value,
                "CPointer<int, 2> should be int**");

  // Test that CPointer<real_t, 3> is real_t***
  static_assert(std::is_same<CPointer<real_t, 3>, real_t***>::value,
                "CPointer<real_t, 3> should be real_t***");
}

// ============================================================================
// CPointer with zero-based indexing tests
// ============================================================================

// Test 1D CPointer allocation and deallocation
TEST(CArrayTest, CPointer1D) {
  int* ptr1d = nullptr;

  // Allocate 1D array
  New<int>(ptr1d, 0, 4);  // Range [0, 4]
  ASSERT_NE(ptr1d, nullptr);

  // Test initialization (should be zero-initialized)
  for (int i = 0; i <= 4; ++i) {
    EXPECT_EQ(ptr1d[i], 0);
  }

  // Modify values
  for (int i = 0; i <= 4; ++i) {
    ptr1d[i] = i * 10;
  }

  // Verify modifications
  for (int i = 0; i <= 4; ++i) {
    EXPECT_EQ(ptr1d[i], i * 10);
  }

  // Clean up
  Delete<int>(ptr1d, 0, 4);
}

// Test 2D CPointer allocation and deallocation
TEST(CArrayTest, CPointer2D) {
  real_t** ptr2d = nullptr;

  // Allocate 2D array (3x4)
  New<real_t>(ptr2d, 0, 2, 0, 3);
  ASSERT_NE(ptr2d, nullptr);

  // Test initialization
  for (int i = 0; i <= 2; ++i) {
    for (int j = 0; j <= 3; ++j) {
      EXPECT_REAL_EQ(ptr2d[i][j], 0.0);
    }
  }

  // Modify values
  for (int i = 0; i <= 2; ++i) {
    for (int j = 0; j <= 3; ++j) {
      ptr2d[i][j] = i * 10.0 + j;
    }
  }

  // Verify modifications
  for (int i = 0; i <= 2; ++i) {
    for (int j = 0; j <= 3; ++j) {
      EXPECT_REAL_EQ(ptr2d[i][j], i * 10.0 + j);
    }
  }

  // Clean up
  Delete<real_t>(ptr2d, 0, 2, 0, 3);
}

// Test 3D CPointer allocation and deallocation
TEST(CArrayTest, CPointer3D) {
  int*** ptr3d = nullptr;

  // Allocate 3D array (2x3x4)
  New<int>(ptr3d, 0, 1, 0, 2, 0, 3);
  ASSERT_NE(ptr3d, nullptr);

  // Test initialization
  for (int i = 0; i <= 1; ++i) {
    for (int j = 0; j <= 2; ++j) {
      for (int k = 0; k <= 3; ++k) {
        EXPECT_EQ(ptr3d[i][j][k], 0);
      }
    }
  }

  // Modify values
  int value = 0;
  for (int i = 0; i <= 1; ++i) {
    for (int j = 0; j <= 2; ++j) {
      for (int k = 0; k <= 3; ++k) {
        ptr3d[i][j][k] = value++;
      }
    }
  }

  // Verify modifications
  value = 0;
  for (int i = 0; i <= 1; ++i) {
    for (int j = 0; j <= 2; ++j) {
      for (int k = 0; k <= 3; ++k) {
        EXPECT_EQ(ptr3d[i][j][k], value++);
      }
    }
  }

  // Clean up
  Delete<int>(ptr3d, 0, 1, 0, 2, 0, 3);
}

// ============================================================================
// CPointer with arbitrary indexing tests (negative indices)
// ============================================================================

// Test 1D array with negative indices
TEST(CArrayTest, ArbitraryIndex1D) {
  real_t* vec1d = nullptr;
  New<real_t>(vec1d, -1, 3);

  // Fill with values
  for (int i = -1; i <= 3; ++i) {
    vec1d[i] = i * 10.0;
  }

  // Verify
  EXPECT_REAL_EQ(vec1d[-1], -10.0);
  EXPECT_REAL_EQ(vec1d[0], 0.0);
  EXPECT_REAL_EQ(vec1d[3], 30.0);

  Delete<real_t>(vec1d, -1, 3);
}

// Test 2D matrix with arbitrary indices
TEST(CArrayTest, ArbitraryIndex2D) {
  real_t** mat2d = nullptr;
  New<real_t>(mat2d, 0, 2, -1, 1);

  // Fill with values
  for (int i = 0; i <= 2; ++i) {
    for (int j = -1; j <= 1; ++j) {
      mat2d[i][j] = i * 10.0 + j;
    }
  }

  // Verify
  EXPECT_REAL_EQ(mat2d[0][-1], -1.0);
  EXPECT_REAL_EQ(mat2d[1][0], 10.0);
  EXPECT_REAL_EQ(mat2d[2][1], 21.0);

  Delete<real_t>(mat2d, 0, 2, -1, 1);
}

// Test 3D tensor with arbitrary indices
TEST(CArrayTest, ArbitraryIndex3D) {
  real_t*** ten3d = nullptr;
  New<real_t>(ten3d, -1, 1, 0, 1, -2, 0);

  // Fill with values
  for (int i = -1; i <= 1; ++i) {
    for (int j = 0; j <= 1; ++j) {
      for (int k = -2; k <= 0; ++k) {
        ten3d[i][j][k] = i * 100.0 + j * 10.0 + k;
      }
    }
  }

  // Verify
  EXPECT_REAL_EQ(ten3d[-1][0][-2], -102.0);
  EXPECT_REAL_EQ(ten3d[0][1][-1], 9.0);
  EXPECT_REAL_EQ(ten3d[1][1][0], 110.0);

  Delete<real_t>(ten3d, -1, 1, 0, 1, -2, 0);
}

// Test integer array with negative indices
TEST(CArrayTest, ArbitraryIndexInt) {
  int* ivec = nullptr;
  New<int>(ivec, -5, -2);

  for (int i = -5; i <= -2; ++i) {
    ivec[i] = i * i;
  }

  EXPECT_EQ(ivec[-5], 25);
  EXPECT_EQ(ivec[-4], 16);
  EXPECT_EQ(ivec[-3], 9);
  EXPECT_EQ(ivec[-2], 4);

  Delete<int>(ivec, -5, -2);
}

// ============================================================================
// CPointer Print tests
// ============================================================================

// Test CPointer Print for 1D
TEST(CArrayTest, CPointerPrint1D) {
  int* ptr = nullptr;
  New<int>(ptr, 0, 4);

  for (int i = 0; i <= 4; ++i) {
    ptr[i] = i + 1;
  }

  std::ostringstream oss;
  Print<int>(oss, ptr, 0, 4);

  // The output should be something like "[1, 2, 3, 4, 5]\n"
  std::string output = oss.str();
  EXPECT_NE(output.find('['), std::string::npos);
  EXPECT_NE(output.find(']'), std::string::npos);
  EXPECT_NE(output.find('1'), std::string::npos);
  EXPECT_NE(output.find('5'), std::string::npos);

  Delete<int>(ptr, 0, 4);
}

// Test CPointer Print for 2D
TEST(CArrayTest, CPointerPrint2D) {
  int** ptr = nullptr;
  New<int>(ptr, 0, 1, 0, 2);

  for (int i = 0; i <= 1; ++i) {
    for (int j = 0; j <= 2; ++j) {
      ptr[i][j] = i * 3 + j;
    }
  }

  std::ostringstream oss;
  Print<int>(oss, ptr, 0, 1, 0, 2);

  std::string output = oss.str();
  EXPECT_NE(output.find('['), std::string::npos);
  EXPECT_NE(output.find(']'), std::string::npos);

  Delete<int>(ptr, 0, 1, 0, 2);
}

// ============================================================================
// CArray type tests
// ============================================================================

// Test CArray type generation
TEST(CArrayTest, CArrayType) {
  // Test that CArray<int, 5> is int[5]
  static_assert(std::is_same<CArray<int, 5>, int[5]>::value,
                "CArray<int, 5> should be int[5]");

  // Test that CArray<real_t, 3, 4> is real_t[3][4]
  static_assert(std::is_same<CArray<real_t, 3, 4>, real_t[3][4]>::value,
                "CArray<real_t, 3, 4> should be real_t[3][4]");
}

// Test 1D CArray usage
TEST(CArrayTest, CArray1D) {
  CArray<int, 5> arr;

  // Initialize values
  for (int i = 0; i < 5; ++i) {
    arr[i] = i * 2;
  }

  // Verify values
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(arr[i], i * 2);
  }
}

// Test 2D CArray usage
TEST(CArrayTest, CArray2D) {
  CArray<real_t, 3, 4> arr;

  // Initialize values
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 4; ++j) {
      arr[i][j] = i * 1.5 + j * 0.5;
    }
  }

  // Verify values
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 4; ++j) {
      EXPECT_REAL_EQ(arr[i][j], i * 1.5 + j * 0.5);
    }
  }
}

// Test 3D CArray usage
TEST(CArrayTest, CArray3D) {
  CArray<int, 2, 3, 4> arr;

  // Initialize values
  int value = 0;
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 3; ++j) {
      for (int k = 0; k < 4; ++k) {
        arr[i][j][k] = value++;
      }
    }
  }

  // Verify values
  value = 0;
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 3; ++j) {
      for (int k = 0; k < 4; ++k) {
        EXPECT_EQ(arr[i][j][k], value++);
      }
    }
  }
}

// Test CArray Print for 1D
TEST(CArrayTest, CArrayPrint1D) {
  CArray<int, 4> arr = {10, 20, 30, 40};

  std::ostringstream oss;
  Print<int, 4>(oss, arr);

  std::string output = oss.str();
  EXPECT_NE(output.find('['), std::string::npos);
  EXPECT_NE(output.find(']'), std::string::npos);
  EXPECT_NE(output.find("10"), std::string::npos);
  EXPECT_NE(output.find("40"), std::string::npos);
}

// Test CArray Print for 2D
TEST(CArrayTest, CArrayPrint2D) {
  CArray<int, 2, 3> arr = {{1, 2, 3}, {4, 5, 6}};

  std::ostringstream oss;
  Print<int, 2, 3>(oss, arr);

  std::string output = oss.str();
  EXPECT_NE(output.find('['), std::string::npos);
  EXPECT_NE(output.find(']'), std::string::npos);
  EXPECT_NE(output.find('1'), std::string::npos);
  EXPECT_NE(output.find('6'), std::string::npos);
}

// Test CArray Print for 3D
TEST(CArrayTest, CArrayPrint3D) {
  CArray<int, 2, 2, 2> arr = {{{1, 2}, {3, 4}}, {{5, 6}, {7, 8}}};

  std::ostringstream oss;
  Print<int, 2, 2, 2>(oss, arr);

  std::string output = oss.str();
  EXPECT_NE(output.find('['), std::string::npos);
  EXPECT_NE(output.find(']'), std::string::npos);
}

// ============================================================================
// Different data types tests
// ============================================================================

// Test different data types
TEST(CArrayTest, DifferentTypes) {
  // Test with real_t
  CArray<real_t, 3> darr = {1.1, 2.2, 3.3};
  EXPECT_REAL_EQ(darr[0], 1.1);
  EXPECT_REAL_EQ(darr[1], 2.2);
  EXPECT_REAL_EQ(darr[2], 3.3);

  // Test with long
  int64_t* lptr = nullptr;
  New<int64_t>(lptr, 0, 1);
  lptr[0] = 100000L;
  lptr[1] = 200000L;
  EXPECT_EQ(lptr[0], 100000L);
  EXPECT_EQ(lptr[1], 200000L);
  Delete<int64_t>(lptr, 0, 1);
}

// ============================================================================
// Large arrays test
// ============================================================================

// Test large arrays
TEST(CArrayTest, LargeArrays) {
  const int size = 999;
  int* ptr = nullptr;
  New<int>(ptr, 0, size);

  // Initialize with pattern
  for (int i = 0; i <= size; ++i) {
    ptr[i] = i;
  }

  // Verify pattern
  for (int i = 0; i <= size; ++i) {
    EXPECT_EQ(ptr[i], i);
  }

  Delete<int>(ptr, 0, size);
}

}  // namespace asc
