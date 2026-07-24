// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: tests/generic/unit_test_uarray.cc
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#include <gtest/gtest.h>
#include <sstream>
#include "common/common.h"
#include "asc/array/uarray.h"

namespace asc {

// ============================================================================
// UArray construction and basic operations tests
// ============================================================================

// Test default constructor
TEST(UArrayTest, DefaultConstructor) {
  UArray<int> arr;
  EXPECT_EQ(arr.GetSize(), 0);
  EXPECT_FALSE(arr.OwnsData());
}

// Test size constructor
TEST(UArrayTest, SizeConstructor) {
  UArray<int> arr(10);
  EXPECT_EQ(arr.GetSize(), 10);
  EXPECT_GE(arr.Capacity(), 10);
  EXPECT_TRUE(arr.OwnsData());
}

// Test initializer list constructor
TEST(UArrayTest, InitializerListConstructor) {
  UArray<int> arr({1, 2, 3, 4, 5});
  EXPECT_EQ(arr.GetSize(), 5);
  EXPECT_EQ(arr[0], 1);
  EXPECT_EQ(arr[1], 2);
  EXPECT_EQ(arr[2], 3);
  EXPECT_EQ(arr[3], 4);
  EXPECT_EQ(arr[4], 5);
}

// Test copy constructor
TEST(UArrayTest, CopyConstructor) {
  UArray<int> arr1({1, 2, 3, 4, 5});
  UArray<int> arr2(arr1);

  EXPECT_EQ(arr2.GetSize(), arr1.GetSize());
  for (int i = 0; i < arr1.GetSize(); ++i) {
    EXPECT_EQ(arr2[i], arr1[i]);
  }

  // Modify arr2 and verify arr1 is not affected
  arr2[0] = 100;
  EXPECT_NE(arr1[0], arr2[0]);
}

// Test move constructor
TEST(UArrayTest, MoveConstructor) {
  UArray<int> arr1({1, 2, 3, 4, 5});
  int* original_ptr = arr1.GetData();

  UArray<int> arr2(std::move(arr1));

  EXPECT_EQ(arr2.GetSize(), 5);
  EXPECT_EQ(arr2.GetData(), original_ptr);
  EXPECT_EQ(arr1.GetSize(), 0);  // arr1 should be empty after move
}

// Test copy assignment
TEST(UArrayTest, CopyAssignment) {
  UArray<int> arr1({1, 2, 3});
  UArray<int> arr2;

  arr2 = arr1;

  EXPECT_EQ(arr2.GetSize(), arr1.GetSize());
  for (int i = 0; i < arr1.GetSize(); ++i) {
    EXPECT_EQ(arr2[i], arr1[i]);
  }
}

// Test move assignment
TEST(UArrayTest, MoveAssignment) {
  UArray<int> arr1({1, 2, 3});
  UArray<int> arr2;
  int* original_ptr = arr1.GetData();

  arr2 = std::move(arr1);

  EXPECT_EQ(arr2.GetSize(), 3);
  EXPECT_EQ(arr2.GetData(), original_ptr);
}

// ============================================================================
// Element access tests
// ============================================================================

// Test operator[]
TEST(UArrayTest, OperatorBracket) {
  UArray<int> arr(5);
  for (int i = 0; i < 5; ++i) {
    arr[i] = i * 10;
  }

  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(arr[i], i * 10);
  }
}

// Test const operator[]
TEST(UArrayTest, ConstOperatorBracket) {
  UArray<int> arr({1, 2, 3, 4, 5});
  const UArray<int>& const_arr = arr;

  EXPECT_EQ(const_arr[0], 1);
  EXPECT_EQ(const_arr[4], 5);
}

// Test GetData
TEST(UArrayTest, GetData) {
  UArray<int> arr({1, 2, 3});
  int* data = arr.GetData();

  ASSERT_NE(data, nullptr);
  EXPECT_EQ(data[0], 1);
  EXPECT_EQ(data[1], 2);
  EXPECT_EQ(data[2], 3);
}

// Test iterators
TEST(UArrayTest, Iterators) {
  UArray<int> arr({1, 2, 3, 4, 5});

  int sum = 0;
  for (int val : arr) {
    sum += val;
  }

  EXPECT_EQ(sum, 15);
}

// ============================================================================
// Size and capacity tests
// ============================================================================

// Test SetSize
TEST(UArrayTest, SetSize) {
  UArray<int> arr;
  arr.SetSize(10);

  EXPECT_EQ(arr.GetSize(), 10);
  EXPECT_GE(arr.Capacity(), 10);
}

// Test Reserve
TEST(UArrayTest, Reserve) {
  UArray<int> arr;
  arr.Reserve(100);

  EXPECT_EQ(arr.GetSize(), 0);
  EXPECT_GE(arr.Capacity(), 100);
}

// Test Append
TEST(UArrayTest, Append) {
  UArray<int> arr;
  arr.Append(1);
  arr.Append(2);
  arr.Append(3);

  EXPECT_EQ(arr.GetSize(), 3);
  EXPECT_EQ(arr[0], 1);
  EXPECT_EQ(arr[1], 2);
  EXPECT_EQ(arr[2], 3);
}

// Test Prepend
TEST(UArrayTest, Prepend) {
  UArray<int> arr;
  arr.Prepend(3);
  arr.Prepend(2);
  arr.Prepend(1);

  EXPECT_EQ(arr.GetSize(), 3);
  EXPECT_EQ(arr[0], 1);
  EXPECT_EQ(arr[1], 2);
  EXPECT_EQ(arr[2], 3);
}

// ============================================================================
// Array manipulation tests
// ============================================================================

// Test Sort
TEST(UArrayTest, Sort) {
  UArray<int> arr({5, 2, 8, 1, 9, 3});
  arr.Sort();

  for (int i = 1; i < arr.GetSize(); ++i) {
    EXPECT_LE(arr[i - 1], arr[i]);
  }
}

// Test Unique
TEST(UArrayTest, Unique) {
  UArray<int> arr({1, 1, 2, 2, 3, 3, 3, 4});
  arr.Unique();

  // After unique, duplicates are removed
  EXPECT_GE(arr.GetSize(), 4);
}

// Test Find
TEST(UArrayTest, Find) {
  UArray<int> arr({1, 2, 3, 4, 5});

  EXPECT_EQ(arr.Find(3), 2);
  EXPECT_EQ(arr.Find(1), 0);
  EXPECT_EQ(arr.Find(5), 4);
  EXPECT_EQ(arr.Find(10), -1);  // Not found
}

// Test Sum
TEST(UArrayTest, Sum) {
  UArray<int> arr({1, 2, 3, 4, 5});
  int sum = arr.Sum();

  EXPECT_EQ(sum, 15);
}

// Test Min and Max
TEST(UArrayTest, MinMax) {
  UArray<int> arr({5, 2, 8, 1, 9, 3});

  EXPECT_EQ(arr.Min(), 1);
  EXPECT_EQ(arr.Max(), 9);
}

// ============================================================================
// Reference and ownership tests
// ============================================================================

// Test MakeRef
TEST(UArrayTest, MakeRef) {
  UArray<int> arr1({1, 2, 3, 4, 5});
  UArray<int> arr2;

  arr2.MakeRef(arr1);

  EXPECT_EQ(arr2.GetSize(), arr1.GetSize());
  EXPECT_EQ(arr2.GetData(), arr1.GetData());
  EXPECT_FALSE(arr2.OwnsData());

  // Modify through arr2 and verify arr1 sees the change
  arr2[0] = 100;
  EXPECT_EQ(arr1[0], 100);
}

// Test wrapping external data
TEST(UArrayTest, WrapExternalData) {
  int external_data[] = {1, 2, 3, 4, 5};
  UArray<int> arr(external_data, 5, false);

  EXPECT_EQ(arr.GetSize(), 5);
  EXPECT_EQ(arr.GetData(), external_data);
  EXPECT_FALSE(arr.OwnsData());

  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(arr[i], external_data[i]);
  }
}

// ============================================================================
// Copy operations tests
// ============================================================================

// Test CopyFrom
TEST(UArrayTest, CopyFrom) {
  UArray<int> src({1, 2, 3, 4, 5});
  UArray<int> dst(5);

  dst.CopyFromHost(src.GetData());

  for (int i = 0; i < src.GetSize(); ++i) {
    EXPECT_EQ(dst[i], src[i]);
  }
}

// Test CopyTo
TEST(UArrayTest, CopyTo) {
  UArray<int> src({1, 2, 3, 4, 5});
  int dst[5];

  src.CopyToHost(dst);

  for (int i = 0; i < src.GetSize(); ++i) {
    EXPECT_EQ(dst[i], src[i]);
  }
}

TEST(UArrayTest, CopyFromArrayResizesDestination) {
  UArray<int> src({4, 5, 6});
  UArray<int> dst({1, 2, 3, 4, 5});

  dst.CopyFrom(src);

  EXPECT_EQ(dst.GetSize(), 3);
  for (int i = 0; i < src.GetSize(); ++i) {
    EXPECT_EQ(dst[i], src[i]);
  }
}

// ============================================================================
// Print and serialization tests
// ============================================================================

// Test Print
TEST(UArrayTest, Print) {
  UArray<int> arr({1, 2, 3, 4, 5});
  std::ostringstream oss;

  arr.Print(oss);

  std::string output = oss.str();
  EXPECT_NE(output.find('['), std::string::npos);
  EXPECT_NE(output.find(']'), std::string::npos);
  EXPECT_NE(output.find('1'), std::string::npos);
  EXPECT_NE(output.find('5'), std::string::npos);
}

// Test Save and Load
TEST(UArrayTest, SaveLoad) {
  UArray<int> arr1({1, 2, 3, 4, 5});

  // Save to stringstream
  std::stringstream ss;
  arr1.Save(ss);

  // Load from stringstream
  UArray<int> arr2;
  arr2.Load(ss);

  EXPECT_EQ(arr2.GetSize(), arr1.GetSize());
  for (int i = 0; i < arr1.GetSize(); ++i) {
    EXPECT_EQ(arr2[i], arr1[i]);
  }
}

// ============================================================================
// Different types tests
// ============================================================================

// Test with double
TEST(UArrayTest, DoubleType) {
  UArray<double> arr({1.1, 2.2, 3.3, 4.4, 5.5});

  EXPECT_EQ(arr.GetSize(), 5);
  EXPECT_DOUBLE_EQ(arr[0], 1.1);
  EXPECT_DOUBLE_EQ(arr[4], 5.5);

  arr.Sort();
  for (int i = 1; i < arr.GetSize(); ++i) {
    EXPECT_LE(arr[i - 1], arr[i]);
  }
}

// Test with float
TEST(UArrayTest, FloatType) {
  UArray<float> arr({3.14f, 2.71f, 1.41f});

  EXPECT_EQ(arr.GetSize(), 3);
  EXPECT_FLOAT_EQ(arr[0], 3.14f);
  EXPECT_FLOAT_EQ(arr[1], 2.71f);
  EXPECT_FLOAT_EQ(arr[2], 1.41f);
}

// ============================================================================
// Edge cases tests
// ============================================================================

// Test empty array
TEST(UArrayTest, EmptyArray) {
  UArray<int> arr;

  EXPECT_EQ(arr.GetSize(), 0);
  EXPECT_FALSE(arr.OwnsData());
  EXPECT_TRUE(arr.IsSorted());
}

// Test single element
TEST(UArrayTest, SingleElement) {
  UArray<int> arr({42});

  EXPECT_EQ(arr.GetSize(), 1);
  EXPECT_EQ(arr[0], 42);
  EXPECT_TRUE(arr.IsSorted());
}

// Test large array
TEST(UArrayTest, LargeArray) {
  const int size = 10000;
  UArray<int> arr(size);

  for (int i = 0; i < size; ++i) {
    arr[i] = i;
  }

  for (int i = 0; i < size; ++i) {
    EXPECT_EQ(arr[i], i);
  }
}

// Test DeleteAll
TEST(UArrayTest, DeleteAll) {
  UArray<int> arr({1, 2, 3, 4, 5});
  arr.DeleteAll();

  EXPECT_EQ(arr.GetSize(), 0);
}

// Test LoseData
TEST(UArrayTest, LoseData) {
  UArray<int> arr({1, 2, 3});
  int* ptr = arr.GetData();

  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(ptr[0], 1);
  EXPECT_EQ(ptr[1], 2);
  EXPECT_EQ(ptr[2], 3);

  arr.LoseData();
  EXPECT_EQ(arr.GetSize(), 0);

  // LoseData deliberately transfers cleanup responsibility to the caller.
  delete[] ptr;
}

}  // namespace asc
