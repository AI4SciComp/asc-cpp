// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: tests/generic/unit_test_indexing.cc
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#include <gtest/gtest.h>

#include "common/common.h"
#include "asc/array/mindex.h"
#include "asc/array/mlayout.h"

namespace asc {

namespace {

#ifdef ASC_USE_EXCEPTION
#define EXPECT_ASC_FAILURE(statement) EXPECT_THROW(statement, ErrorException)
#else
#define EXPECT_ASC_FAILURE(statement) EXPECT_DEATH(statement, "ASC")
#endif

void ExpectArrayEquals(const int* actual, std::initializer_list<int> expected) {
  int i = 0;
  for (int value : expected) EXPECT_EQ(actual[i++], value);
}

template <typename Map>
int FoldOne(const Map& map, int value) {
  int index[1] = {value};
  return map.Fold(index);
}

template <typename Array>
void ExpectMIndexArrayEquals(const Array& actual,
                             std::initializer_list<int> expected) {
  constexpr int n_dims = Array::IndexType::GetNDims();
  ASSERT_EQ(static_cast<int>(expected.size()), n_dims * actual.GetSize());
  auto values = expected.begin();
  for (int i = 0; i < actual.GetSize(); ++i) {
    for (int dim = 0; dim < n_dims; ++dim) {
      EXPECT_EQ(actual.Get(i)[dim], *values++);
    }
  }
}

}  // namespace

TEST(LayoutPaddingTest, KeepsDefaultLayoutMapping) {
  LayoutLeft::Map<DShape<2>> column(DShape<2>(2, 3));
  int column_index[2] = {1, 2};
  EXPECT_EQ(column.Fold(column_index), 5);
  column_index[0] = 0;
  column_index[1] = 1;
  EXPECT_EQ(column.Fold(column_index), 2);
  int out[2] = {};
  column.Unfold(4, out);
  ExpectArrayEquals(out, {0, 2});

  LayoutRight::Map<DShape<2>> row(DShape<2>(2, 3));
  int row_index[2] = {1, 0};
  EXPECT_EQ(row.Fold(row_index), 3);
  row_index[1] = 2;
  EXPECT_EQ(row.Fold(row_index), 5);
  row.Unfold(4, out);
  ExpectArrayEquals(out, {1, 1});
}

TEST(LayoutPaddingTest, AppliesBoundaryPolicies) {
  LayoutLeft::Map<DShape<1>, PaddingType::kEmpty> empty(DShape<1>(3));
  EXPECT_EQ(FoldOne(empty, -1), -1);
  EXPECT_EQ(FoldOne(empty, 3), -1);

  LayoutLeft::Map<DShape<1>, PaddingType::kWrap> wrap(DShape<1>(3));
  EXPECT_EQ(FoldOne(wrap, -1), 2);
  EXPECT_EQ(FoldOne(wrap, 3), 0);
  EXPECT_EQ(FoldOne(wrap, 4), 1);

  LayoutLeft::Map<DShape<1>, PaddingType::kEdge> edge(DShape<1>(3));
  EXPECT_EQ(FoldOne(edge, -1), 0);
  EXPECT_EQ(FoldOne(edge, 4), 2);

  LayoutLeft::Map<DShape<1>, PaddingType::kReflect> reflect(DShape<1>(3));
  EXPECT_EQ(FoldOne(reflect, -1), 1);
  EXPECT_EQ(FoldOne(reflect, 3), 1);
  EXPECT_EQ(FoldOne(reflect, 4), 0);

  LayoutLeft::Map<DShape<1>, PaddingType::kSymmetric> symmetric(DShape<1>(3));
  EXPECT_EQ(FoldOne(symmetric, -1), 0);
  EXPECT_EQ(FoldOne(symmetric, 3), 2);
  EXPECT_EQ(FoldOne(symmetric, 4), 1);
}

TEST(MIndexArrayTest, BuildsBoxIndices) {
  auto column = MIndexArray<DShape<2>, LayoutLeft>::Box(2);
  ASSERT_EQ(column.GetSize(), 9);
  ExpectMIndexArrayEquals(column,
                          {0, 0, 1, 0, 2, 0, 0, 1, 1, 1,
                           2, 1, 0, 2, 1, 2, 2, 2});

  auto row = MIndexArray<DShape<2>, LayoutRight>::Box(1);
  ASSERT_EQ(row.GetSize(), 4);
  ExpectMIndexArrayEquals(row, {0, 0, 0, 1, 1, 0, 1, 1});
}

TEST(MIndexArrayTest, BuildsTotalDegreeIndices) {
  auto column = MIndexArray<DShape<2>, LayoutLeft>::TotalDegree(2);
  ASSERT_EQ(column.GetSize(), 6);
  ExpectMIndexArrayEquals(column, {0, 0, 1, 0, 0, 1,
                                   2, 0, 1, 1, 0, 2});

  auto row = MIndexArray<DShape<2>, LayoutRight>::TotalDegree(2);
  ASSERT_EQ(row.GetSize(), 6);
  ExpectMIndexArrayEquals(row, {0, 0, 0, 1, 1, 0,
                                0, 2, 1, 1, 2, 0});
}

TEST(MIndexArrayTest, UsesProvidedShape) {
  auto indices =
      MIndexArray<MShape<3, 3>, LayoutLeft>::TotalDegree(MShape<3, 3>{}, 2);
  ASSERT_EQ(indices.GetSize(), 6);
  EXPECT_EQ(indices.Get(5).GetOffset(), 6);
  ExpectArrayEquals(indices.Get(5).GetMultiIndex(), {0, 2});
}

TEST(IndexingTest, ValidationFailures) {
  LayoutLeft::Map<DShape<1>> unset;
  int index[1] = {0};
  EXPECT_ASC_FAILURE((void)unset.Fold(index));

  auto bad_box = [] { (void)MIndexArray<DShape<2>, LayoutLeft>::Box(-1); };
  EXPECT_ASC_FAILURE(bad_box());
  auto bad_degree = [] {
    (void)MIndexArray<DShape<2>, LayoutLeft>::TotalDegree(1, 2);
  };
  EXPECT_ASC_FAILURE(bad_degree());
  auto bad_static_shape = [] {
    (void)MIndexArray<MShape<2, 2>, LayoutLeft>::Box(2);
  };
  EXPECT_ASC_FAILURE(bad_static_shape());
}

}  // namespace asc
