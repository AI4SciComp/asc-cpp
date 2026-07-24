// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: tests/generic/unit_test_new_mindex.cc
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
//
// Unit tests for the new unified mindex.h architecture
// ============================================================================

#include <gtest/gtest.h>
#include <cstdint>
#include <limits>
#include "common/common.h"
#include "asc/array/mindex.h"
#include "asc/array/miterator.h"

namespace asc {

namespace {

#ifdef ASC_USE_EXCEPTION
#define EXPECT_ASC_FAILURE(statement) EXPECT_THROW(statement, ErrorException)
#else
#define EXPECT_ASC_FAILURE(statement) EXPECT_DEATH(statement, "ASC")
#endif

}  // namespace

// ============================================================================
// Test 1: MShape
// ============================================================================

TEST(MShape, StaticShape) {
  MShape<3, 4> shape;
  EXPECT_EQ(shape.GetRank(), 2);
  EXPECT_EQ(shape.GetDynamicRank(), 0);
  EXPECT_TRUE(shape.IsStatic());
  EXPECT_FALSE(shape.IsDynamic());
  EXPECT_EQ(shape.GetExtent(0), 3);
  EXPECT_EQ(shape.GetExtent(1), 4);
  EXPECT_EQ(shape.GetSize(), 12);
}

TEST(MShape, DynamicShape) {
  MShape<kDynamicExtent, kDynamicExtent> shape(3, 4);
  EXPECT_EQ(shape.GetRank(), 2);
  EXPECT_EQ(shape.GetDynamicRank(), 2);
  EXPECT_FALSE(shape.IsStatic());
  EXPECT_TRUE(shape.IsDynamic());
  EXPECT_EQ(shape.GetExtent(0), 3);
  EXPECT_EQ(shape.GetExtent(1), 4);
  EXPECT_EQ(shape.GetSize(), 12);
}

TEST(MShape, MixedExtentShapeIsUnsupported) {
  using MixedShape = MShape<3, kDynamicExtent>;
  EXPECT_EQ(MixedShape::GetRank(), 2);
  EXPECT_EQ(MixedShape::GetDynamicRank(), 1);
  EXPECT_FALSE(MixedShape::IsStatic());
  EXPECT_FALSE(MixedShape::IsDynamic());
  EXPECT_FALSE(MixedShape::IsSupported());
}

TEST(MShape, DynamicShapeMultipleExtents) {
  DShape<4> shape(2, 4, 3, 5);
  EXPECT_EQ(shape.GetRank(), 4);
  EXPECT_EQ(shape.GetDynamicRank(), 4);
  EXPECT_FALSE(shape.IsStatic());
  EXPECT_TRUE(shape.IsDynamic());
  EXPECT_TRUE(shape.IsSupported());
  EXPECT_EQ(shape.GetExtent(0), 2);
  EXPECT_EQ(shape.GetExtent(1), 4);
  EXPECT_EQ(shape.GetExtent(2), 3);
  EXPECT_EQ(shape.GetExtent(3), 5);
  EXPECT_EQ(shape.GetSize(), 2 * 4 * 3 * 5);
}

TEST(MShape, DynamicShapeFromInitializerList) {
  DShape<3> shape{2, 3, 4};
  EXPECT_EQ(shape.GetRank(), 3);
  EXPECT_EQ(shape.GetDynamicRank(), 3);
  EXPECT_TRUE(shape.IsDynamic());
  EXPECT_EQ(shape.GetExtent(0), 2);
  EXPECT_EQ(shape.GetExtent(1), 3);
  EXPECT_EQ(shape.GetExtent(2), 4);
  EXPECT_EQ(shape.GetSize(), 24);
}

TEST(MShape, DynamicShapeRejectsBadRuntimeExtents) {
  EXPECT_ASC_FAILURE(([] {
    DShape<3> shape{2, 3};
    (void)shape;
  }()));
  EXPECT_ASC_FAILURE(([] {
    DShape<2> shape(2, -1);
    (void)shape;
  }()));
  EXPECT_ASC_FAILURE(([] {
    const auto too_large =
        static_cast<std::int64_t>(std::numeric_limits<int>::max()) + 1;
    DShape<1> shape(too_large);
    (void)shape;
  }()));
}

TEST(MShape, StaticShapeSupported) {
  using StaticShape = MShape<2, 3, 5>;
  EXPECT_TRUE(StaticShape::IsSupported());
  StaticShape shape;
  EXPECT_EQ(shape.GetSize(), 30);
}

TEST(MShape, ArrayAccess) {
  MShape<3, 4> shape;
  EXPECT_EQ(shape[0], 3);
  EXPECT_EQ(shape[1], 4);
}

// ============================================================================
// Test 2: MapBase and LayoutLeft
// ============================================================================

TEST(LayoutLeft, Construction) {
  MShape<3, 4> shape;
  LayoutLeft::Map<MShape<3, 4>> map(shape);

  EXPECT_EQ(map.GetShape().GetSize(), 12);
  EXPECT_EQ(map.GetExtent(0), 3);
  EXPECT_EQ(map.GetExtent(1), 4);
  EXPECT_EQ(map.GetSize(), 12);
}

TEST(LayoutLeft, Strides) {
  MShape<3, 4> shape;
  LayoutLeft::Map<MShape<3, 4>> map(shape);

  // Column-major: stride[0] = 1, stride[1] = 3
  EXPECT_EQ(map.GetStride(0), 1);
  EXPECT_EQ(map.GetStride(1), 3);
}

TEST(LayoutLeft, Fold) {
  MShape<3, 4> shape;
  LayoutLeft::Map<MShape<3, 4>> map(shape);

  // Column-major: offset = i + j*3
  EXPECT_EQ(map(0, 0), 0);
  EXPECT_EQ(map(1, 0), 1);
  EXPECT_EQ(map(0, 1), 3);
  EXPECT_EQ(map(1, 2), 7);   // 1 + 2*3 = 7
  EXPECT_EQ(map(2, 3), 11);  // 2 + 3*3 = 11
}

TEST(LayoutLeft, Unfold) {
  MShape<3, 4> shape;
  LayoutLeft::Map<MShape<3, 4>> map(shape);

  int multi_idx[2];

  // Offset 0 → (0, 0)
  map.Unfold(0, multi_idx);
  EXPECT_EQ(multi_idx[0], 0);
  EXPECT_EQ(multi_idx[1], 0);

  // Offset 1 → (1, 0)
  map.Unfold(1, multi_idx);
  EXPECT_EQ(multi_idx[0], 1);
  EXPECT_EQ(multi_idx[1], 0);

  // Offset 3 → (0, 1)
  map.Unfold(3, multi_idx);
  EXPECT_EQ(multi_idx[0], 0);
  EXPECT_EQ(multi_idx[1], 1);

  // Offset 7 → (1, 2)
  map.Unfold(7, multi_idx);
  EXPECT_EQ(multi_idx[0], 1);
  EXPECT_EQ(multi_idx[1], 2);

  // Offset 11 → (2, 3)
  map.Unfold(11, multi_idx);
  EXPECT_EQ(multi_idx[0], 2);
  EXPECT_EQ(multi_idx[1], 3);
}

TEST(LayoutLeft, BidirectionalConsistency) {
  MShape<3, 4> shape;
  LayoutLeft::Map<MShape<3, 4>> map(shape);

  // For all multi-indices, check multi → offset → multi consistency
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 4; ++j) {
      int offset = map(i, j);
      int multi_idx[2];
      map.Unfold(offset, multi_idx);
      EXPECT_EQ(multi_idx[0], i);
      EXPECT_EQ(multi_idx[1], j);
    }
  }
}

TEST(LayoutLeft, RequiredSize) {
  MShape<3, 4> shape;
  LayoutLeft::Map<MShape<3, 4>> map(shape);
  EXPECT_EQ(map.GetRequiredSize(), 12);
}

TEST(LayoutLeft, IsContiguous) {
  MShape<3, 4> shape;
  LayoutLeft::Map<MShape<3, 4>> map(shape);
  EXPECT_TRUE(map.IsContiguous());
}

// ============================================================================
// Test 3: LayoutRight
// ============================================================================

TEST(LayoutRight, Strides) {
  MShape<3, 4> shape;
  LayoutRight::Map<MShape<3, 4>> map(shape);

  // Row-major: stride[0] = 4, stride[1] = 1
  EXPECT_EQ(map.GetStride(0), 4);
  EXPECT_EQ(map.GetStride(1), 1);
}

TEST(LayoutRight, Fold) {
  MShape<3, 4> shape;
  LayoutRight::Map<MShape<3, 4>> map(shape);

  // Row-major: offset = i*4 + j
  EXPECT_EQ(map(0, 0), 0);
  EXPECT_EQ(map(0, 1), 1);
  EXPECT_EQ(map(1, 0), 4);
  EXPECT_EQ(map(1, 2), 6);   // 1*4 + 2 = 6
  EXPECT_EQ(map(2, 3), 11);  // 2*4 + 3 = 11
}

TEST(LayoutRight, Unfold) {
  MShape<3, 4> shape;
  LayoutRight::Map<MShape<3, 4>> map(shape);

  int multi_idx[2];

  // Row-major memory layout (last dimension varies fastest in memory)
  // Offset 0 → (0, 0)
  map.Unfold(0, multi_idx);
  EXPECT_EQ(multi_idx[0], 0);
  EXPECT_EQ(multi_idx[1], 0);

  // Offset 1 → (0, 1) in row-major memory order
  map.Unfold(1, multi_idx);
  EXPECT_EQ(multi_idx[0], 0);
  EXPECT_EQ(multi_idx[1], 1);

  // Offset 3 → (0, 3)
  map.Unfold(3, multi_idx);
  EXPECT_EQ(multi_idx[0], 0);
  EXPECT_EQ(multi_idx[1], 3);
}

TEST(LayoutRight, BidirectionalConsistency) {
  MShape<3, 4> shape;
  LayoutRight::Map<MShape<3, 4>> map(shape);

  // For all memory offsets, check offset to multi-index to offset consistency.
  for (int offset = 0; offset < 12; ++offset) {
    int multi_idx[2];
    map.Unfold(offset, multi_idx);
    int reconstructed = map(multi_idx[0], multi_idx[1]);
    EXPECT_EQ(reconstructed, offset);
    EXPECT_GE(multi_idx[0], 0);
    EXPECT_LT(multi_idx[0], 3);
    EXPECT_GE(multi_idx[1], 0);
    EXPECT_LT(multi_idx[1], 4);
  }
}

TEST(LayoutRight, IsContiguous) {
  MShape<3, 4> shape;
  LayoutRight::Map<MShape<3, 4>> map(shape);
  EXPECT_TRUE(map.IsContiguous());
}

// ============================================================================
// Test 4: LayoutStride
// ============================================================================

TEST(LayoutStride, CustomStrides) {
  MShape<3, 4> shape;
  int strides[] = {4, 1};  // Row-major strides
  LayoutStride::Map<MShape<3, 4>> map(shape, strides);

  EXPECT_EQ(map.GetStride(0), 4);
  EXPECT_EQ(map.GetStride(1), 1);
}

TEST(LayoutStride, Fold) {
  MShape<3, 4> shape;
  int strides[] = {4, 1};
  LayoutStride::Map<MShape<3, 4>> map(shape, strides);

  // Same as row-major
  EXPECT_EQ(map(0, 0), 0);
  EXPECT_EQ(map(1, 2), 6);
  EXPECT_EQ(map(2, 3), 11);
}

TEST(LayoutStride, TransposedStrides) {
  MShape<3, 4> shape;
  int trans_strides[] = {1, 3};  // Transposed from column-major
  LayoutStride::Map<MShape<3, 4>> map(shape, trans_strides);

  EXPECT_EQ(map(0, 0), 0);
  EXPECT_EQ(map(1, 0), 1);
  EXPECT_EQ(map(0, 1), 3);
}

TEST(LayoutStride, RequiredSize) {
  MShape<3, 4> shape;
  int strides[] = {4, 1};
  LayoutStride::Map<MShape<3, 4>> map(shape, strides);

  // max_offset = (3-1)*4 + (4-1)*1 = 8 + 3 = 11
  // required_size = 11 + 1 = 12
  EXPECT_EQ(map.GetRequiredSize(), 12);
}

TEST(LayoutStride, NonContiguousRequiredSize) {
  MShape<2, 2> shape;
  int strides[] = {3, 1};  // Non-contiguous (padding)
  LayoutStride::Map<MShape<2, 2>> map(shape, strides);

  // max_offset = (2-1)*3 + (2-1)*1 = 3 + 1 = 4
  // required_size = 4 + 1 = 5
  EXPECT_EQ(map.GetRequiredSize(), 5);
}

TEST(LayoutStride, IsContiguousColumnMajor) {
  MShape<3, 4> shape;
  int col_major_strides[] = {1, 3};
  LayoutStride::Map<MShape<3, 4>> map(shape, col_major_strides);
  EXPECT_TRUE(map.IsContiguous());
}

TEST(LayoutStride, IsContiguousRowMajor) {
  MShape<3, 4> shape;
  int row_major_strides[] = {4, 1};
  LayoutStride::Map<MShape<3, 4>> map(shape, row_major_strides);
  EXPECT_TRUE(map.IsContiguous());
}

TEST(LayoutStride, IsNotContiguous) {
  MShape<2, 2> shape;
  int non_contiguous_strides[] = {3, 1};
  LayoutStride::Map<MShape<2, 2>> map(shape, non_contiguous_strides);
  EXPECT_FALSE(map.IsContiguous());
}

// ============================================================================
// Test 5: MIndex
// ============================================================================

TEST(MIndex, ConstructionFromMap) {
  MShape<3, 4> shape;
  LayoutLeft::Map<MShape<3, 4>> map(shape);
  MIndex<MShape<3, 4>> idx(map);

  EXPECT_EQ(idx.GetOffset(), 0);
  EXPECT_EQ(idx[0], 0);
  EXPECT_EQ(idx[1], 0);
}

TEST(MIndex, SetOffset) {
  MShape<3, 4> shape;
  LayoutLeft::Map<MShape<3, 4>> map(shape);
  MIndex<MShape<3, 4>> idx(map);

  idx.SetOffset(7);
  EXPECT_EQ(idx.GetOffset(), 7);
  EXPECT_EQ(idx[0], 1);  // 7 % 3 = 1
  EXPECT_EQ(idx[1], 2);  // 7 / 3 = 2
}

TEST(MIndex, SetMulti) {
  MShape<3, 4> shape;
  LayoutLeft::Map<MShape<3, 4>> map(shape);
  MIndex<MShape<3, 4>> idx(map);

  idx.SetMultiIndex(1, 2);
  EXPECT_EQ(idx[0], 1);
  EXPECT_EQ(idx[1], 2);
  EXPECT_EQ(idx.GetOffset(), 7);  // 1 + 2*3 = 7
}

TEST(MIndex, SetMultiArray) {
  MShape<3, 4> shape;
  LayoutLeft::Map<MShape<3, 4>> map(shape);
  MIndex<MShape<3, 4>> idx(map);

  idx.SetMultiIndex(2, 3);  // Pass indices directly, not as array
  EXPECT_EQ(idx[0], 2);
  EXPECT_EQ(idx[1], 3);
  EXPECT_EQ(idx.GetOffset(), 11);  // 2 + 3*3 = 11
}

TEST(MIndex, GetAccessors) {
  MShape<3, 4> shape;
  LayoutLeft::Map<MShape<3, 4>> map(shape);
  MIndex<MShape<3, 4>> idx(map);

  idx.SetMultiIndex(1, 2);
  EXPECT_EQ(idx.GetExtent(0), 3);
  EXPECT_EQ(idx.GetExtent(1), 4);
  EXPECT_EQ(idx.GetStride(0), 1);
  EXPECT_EQ(idx.GetStride(1), 3);
  EXPECT_EQ(idx.GetSize(), 12);
}

TEST(MIndex, DynamicShape) {
  MShape<kDynamicExtent, kDynamicExtent> shape(3, 4);
  LayoutLeft::Map<MShape<kDynamicExtent, kDynamicExtent>> map(shape);
  MIndex<MShape<kDynamicExtent, kDynamicExtent>> idx(map);

  idx.SetMultiIndex(1, 2);
  EXPECT_EQ(idx.GetOffset(), 7);
  EXPECT_EQ(idx[0], 1);
  EXPECT_EQ(idx[1], 2);
}

// ============================================================================
// Test 6: MIterator
// ============================================================================

TEST(MIterator, Construction) {
  MShape<2, 3> shape;
  LayoutLeft::Map<MShape<2, 3>> map(shape);
  MIterator<MShape<2, 3>> iter(map);

  EXPECT_FALSE(iter.IsExhausted());
  EXPECT_EQ(iter.GetOffset(), 0);
  EXPECT_EQ(iter[0], 0);
  EXPECT_EQ(iter[1], 0);
}

TEST(MIterator, Increment) {
  MShape<2, 3> shape;
  LayoutLeft::Map<MShape<2, 3>> map(shape);
  MIterator<MShape<2, 3>> iter(map);

  // Iteration order: (0,0), (1,0), (0,1), (1,1), (0,2), (1,2)
  EXPECT_EQ(iter[0], 0);
  EXPECT_EQ(iter[1], 0);

  ++iter;
  EXPECT_EQ(iter[0], 1);
  EXPECT_EQ(iter[1], 0);

  ++iter;
  EXPECT_EQ(iter[0], 0);
  EXPECT_EQ(iter[1], 1);

  ++iter;
  EXPECT_EQ(iter[0], 1);
  EXPECT_EQ(iter[1], 1);

  ++iter;
  EXPECT_EQ(iter[0], 0);
  EXPECT_EQ(iter[1], 2);

  ++iter;
  EXPECT_EQ(iter[0], 1);
  EXPECT_EQ(iter[1], 2);

  ++iter;
  EXPECT_TRUE(iter.IsExhausted());
}

TEST(MIterator, FullIteration) {
  MShape<2, 3> shape;
  LayoutLeft::Map<MShape<2, 3>> map(shape);
  MIterator<MShape<2, 3>> iter(map);

  int count = 0;
  while (!iter.IsExhausted()) {
    ++count;
    ++iter;
  }
  EXPECT_EQ(count, 6);
}

TEST(MIterator, Reset) {
  MShape<2, 3> shape;
  LayoutLeft::Map<MShape<2, 3>> map(shape);
  MIterator<MShape<2, 3>> iter(map);

  ++iter;
  ++iter;
  EXPECT_EQ(iter.GetOffset(), 2);

  iter.Reset();
  EXPECT_EQ(iter.GetOffset(), 0);
  EXPECT_EQ(iter[0], 0);
  EXPECT_EQ(iter[1], 0);
}

TEST(MIterator, GetIndex) {
  MShape<2, 3> shape;
  LayoutLeft::Map<MShape<2, 3>> map(shape);
  MIterator<MShape<2, 3>> iter(map);

  ++iter;
  const auto& idx = iter.GetIndex();
  EXPECT_EQ(idx.GetOffset(), 1);
  EXPECT_EQ(idx[0], 1);
  EXPECT_EQ(idx[1], 0);
}

TEST(MIterator, RangeBasedFor) {
  MShape<2, 3> shape;
  LayoutLeft::Map<MShape<2, 3>> map(shape);
  MIterator<MShape<2, 3>> iter(map);

  int count = 0;
  for (const auto& idx : iter) {
    EXPECT_GE(idx[0], 0);
    EXPECT_LT(idx[0], 2);
    EXPECT_GE(idx[1], 0);
    EXPECT_LT(idx[1], 3);
    ++count;
  }
  EXPECT_EQ(count, 6);
}

TEST(MIterator, LayoutRight) {
  MShape<2, 3> shape;
  LayoutRight::Map<MShape<2, 3>> map(shape);
  MIterator<MShape<2, 3>, LayoutRight> iter(map);

  // Row-major iteration: last dimension varies fastest.
  EXPECT_EQ(iter[0], 0);
  EXPECT_EQ(iter[1], 0);
  EXPECT_EQ(iter.GetOffset(), 0);

  ++iter;
  EXPECT_EQ(iter[0], 0);
  EXPECT_EQ(iter[1], 1);
  EXPECT_EQ(iter.GetOffset(), 1);

  ++iter;
  EXPECT_EQ(iter[0], 0);
  EXPECT_EQ(iter[1], 2);
  EXPECT_EQ(iter.GetOffset(), 2);

  ++iter;
  EXPECT_EQ(iter[0], 1);
  EXPECT_EQ(iter[1], 0);
  EXPECT_EQ(iter.GetOffset(), 3);
}

// ============================================================================
// Test 7: Type Aliases
// ============================================================================

TEST(TypeAliases, DShape) {
  DShape<2> shape(3, 4);
  EXPECT_EQ(shape.GetRank(), 2);
  EXPECT_EQ(shape.GetExtent(0), 3);
  EXPECT_EQ(shape.GetExtent(1), 4);
}

TEST(TypeAliases, DIndex) {
  DIndex<2> idx(3, 4);
  idx.SetMultiIndex(1, 2);
  EXPECT_EQ(idx.GetOffset(), 7);
}

TEST(TypeAliases, DIterator) {
  DIterator<2> iter(2, 3);
  EXPECT_EQ(iter.GetLimit(), 6);
}

TEST(TypeAliases, SIndex) {
  SIndex<3, 4> idx;
  idx.SetMultiIndex(1, 2);
  EXPECT_EQ(idx.GetOffset(), 7);
}

TEST(TypeAliases, SIterator) {
  SIterator<2, 3> iter;
  EXPECT_EQ(iter.GetLimit(), 6);
}

// ============================================================================
// Test 8: Compile-time Shape Manipulation
// ============================================================================

TEST(ShapeManipulation, TransposedShape) {
  using OriginalShape = MShape<2, 3, 4>;
  using ReversedShape = TransposedShape<OriginalShape>;

  ReversedShape shape;
  EXPECT_EQ(shape.GetExtent(0), 4);
  EXPECT_EQ(shape.GetExtent(1), 3);
  EXPECT_EQ(shape.GetExtent(2), 2);
}

TEST(ShapeManipulation, PermutedShape) {
  using OriginalShape = MShape<2, 3, 4>;
  using PermutedShape = PermutedShape<OriginalShape, 2, 0, 1>;

  PermutedShape shape;
  EXPECT_EQ(shape.GetExtent(0), 4);
  EXPECT_EQ(shape.GetExtent(1), 2);
  EXPECT_EQ(shape.GetExtent(2), 3);
}

TEST(ShapeManipulation, CreateReverseShape) {
  MShape<kDynamicExtent, kDynamicExtent, kDynamicExtent> shape(2, 3, 4);
  auto rev_shape = CreateReverseShape(shape);

  EXPECT_EQ(rev_shape.GetExtent(0), 4);
  EXPECT_EQ(rev_shape.GetExtent(1), 3);
  EXPECT_EQ(rev_shape.GetExtent(2), 2);
}

TEST(ShapeManipulation, CreatePermuteShape) {
  MShape<kDynamicExtent, kDynamicExtent, kDynamicExtent> shape(2, 3, 4);
  auto perm_shape = CreatePermuteShape<decltype(shape), 2, 0, 1>(shape);

  EXPECT_EQ(perm_shape.GetExtent(0), 4);
  EXPECT_EQ(perm_shape.GetExtent(1), 2);
  EXPECT_EQ(perm_shape.GetExtent(2), 3);
}

// ============================================================================
// Test 9: Fully Dynamic Shape Integration Tests
// ============================================================================

TEST(DynamicShapeIntegration, LayoutLeft) {
  DShape<3> shape(2, 4, 3);
  LayoutLeft::Map<DShape<3>> map(shape);

  EXPECT_EQ(map.GetSize(), 24);
  EXPECT_EQ(map.GetExtent(0), 2);
  EXPECT_EQ(map.GetExtent(1), 4);
  EXPECT_EQ(map.GetExtent(2), 3);

  // Verify strides (column-major)
  EXPECT_EQ(map.GetStride(0), 1);
  EXPECT_EQ(map.GetStride(1), 2);      // 2 * 1
  EXPECT_EQ(map.GetStride(2), 8);      // 2 * 4

  // Verify mapping
  EXPECT_EQ(map(0, 0, 0), 0);
  EXPECT_EQ(map(1, 0, 0), 1);
  EXPECT_EQ(map(0, 1, 0), 2);
  EXPECT_EQ(map(0, 0, 1), 8);
  EXPECT_EQ(map(1, 3, 2), 1 + 3*2 + 2*8);  // 1 + 6 + 16 = 23
}

TEST(DynamicShapeIntegration, LayoutRight) {
  DShape<3> shape(2, 3, 5);
  LayoutRight::Map<DShape<3>> map(shape);

  EXPECT_EQ(map.GetSize(), 30);
  EXPECT_EQ(map.GetExtent(0), 2);
  EXPECT_EQ(map.GetExtent(1), 3);
  EXPECT_EQ(map.GetExtent(2), 5);

  // Verify strides (row-major)
  EXPECT_EQ(map.GetStride(0), 15);     // 3 * 5
  EXPECT_EQ(map.GetStride(1), 5);      // 5
  EXPECT_EQ(map.GetStride(2), 1);      // 1

  // Verify mapping
  EXPECT_EQ(map(0, 0, 0), 0);
  EXPECT_EQ(map(0, 0, 1), 1);
  EXPECT_EQ(map(0, 1, 0), 5);
  EXPECT_EQ(map(1, 0, 0), 15);
  EXPECT_EQ(map(1, 2, 4), 1*15 + 2*5 + 4*1);  // 15 + 10 + 4 = 29
}

TEST(DynamicShapeIntegration, MIndex) {
  DShape<3> shape(3, 2, 4);
  LayoutLeft::Map<DShape<3>> map(shape);
  MIndex<DShape<3>> idx(map);

  idx.SetMultiIndex(1, 1, 2);
  EXPECT_EQ(idx[0], 1);
  EXPECT_EQ(idx[1], 1);
  EXPECT_EQ(idx[2], 2);
  // Offset: 1 + 1*3 + 2*6 = 1 + 3 + 12 = 16
  EXPECT_EQ(idx.GetOffset(), 16);

  // Test SetOffset with fully dynamic shape
  idx.SetOffset(10);
  // 10 = i + j*3 + k*6
  // i = 10 % 3 = 1, temp = 10/3 = 3
  // j = 3 % 2 = 1, k = 3/2 = 1
  // So: (1, 1, 1)
  EXPECT_EQ(idx[0], 1);
  EXPECT_EQ(idx[1], 1);
  EXPECT_EQ(idx[2], 1);
}

TEST(DynamicShapeIntegration, MIterator) {
  DShape<2> shape(2, 3);
  LayoutLeft::Map<DShape<2>> map(shape);
  MIterator<DShape<2>> iter(map);

  EXPECT_EQ(iter.GetLimit(), 6);
  EXPECT_FALSE(iter.IsExhausted());

  // Iterate through all elements
  int count = 0;
  while (!iter.IsExhausted()) {
    EXPECT_GE(iter[0], 0);
    EXPECT_LT(iter[0], 2);
    EXPECT_GE(iter[1], 0);
    EXPECT_LT(iter[1], 3);
    ++count;
    ++iter;
  }
  EXPECT_EQ(count, 6);
}

TEST(DynamicShapeIntegration, LargeRankDynamicShape) {
  DShape<5> shape(4, 2, 5, 3, 6);

  EXPECT_EQ(shape.GetRank(), 5);
  EXPECT_EQ(shape.GetDynamicRank(), 5);
  EXPECT_EQ(shape.GetExtent(0), 4);
  EXPECT_EQ(shape.GetExtent(1), 2);
  EXPECT_EQ(shape.GetExtent(2), 5);
  EXPECT_EQ(shape.GetExtent(3), 3);
  EXPECT_EQ(shape.GetExtent(4), 6);
  EXPECT_EQ(shape.GetSize(), 4 * 2 * 5 * 3 * 6);

  // Test with LayoutLeft
  LayoutLeft::Map<DShape<5>> map(shape);
  EXPECT_EQ(map.GetStride(0), 1);
  EXPECT_EQ(map.GetStride(1), 4);      // 4
  EXPECT_EQ(map.GetStride(2), 8);      // 4 * 2
  EXPECT_EQ(map.GetStride(3), 40);     // 4 * 2 * 5
  EXPECT_EQ(map.GetStride(4), 120);    // 4 * 2 * 5 * 3
}

// ============================================================================
// Test 10: Edge Cases
// ============================================================================

TEST(EdgeCases, Rank0) {
  // Rank-0 (scalar) not directly supported, but rank-1 with size 1 works
  MShape<1> shape;
  LayoutLeft::Map<MShape<1>> map(shape);
  EXPECT_EQ(map.GetSize(), 1);
}

TEST(EdgeCases, LargeRank) {
  MShape<2, 2, 2, 2, 2> shape;  // 5D
  LayoutLeft::Map<MShape<2, 2, 2, 2, 2>> map(shape);
  EXPECT_EQ(map.GetSize(), 32);
}

TEST(EdgeCases, SingleElement) {
  MShape<1, 1> shape;
  LayoutLeft::Map<MShape<1, 1>> map(shape);

  EXPECT_EQ(map(0, 0), 0);
  EXPECT_EQ(map.GetSize(), 1);
}

TEST(EdgeCases, PastTheEndIterator) {
  MShape<2, 3> shape;
  LayoutLeft::Map<MShape<2, 3>> map(shape);
  MIndex<MShape<2, 3>> idx(map);

  // Set to past-the-end
  idx.SetOffset(6);
  EXPECT_EQ(idx.GetOffset(), 6);
  // Multi-indices not updated for past-the-end
}

}  // namespace asc
