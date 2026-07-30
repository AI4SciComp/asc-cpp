#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>

#include "allocation_probe.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/layout.h"
#include "asc/dense/view.h"
#include "asc/expression/expression.h"
#include "test_support.h"

namespace {

using asc_dense_test::TestContext;

static_assert(std::is_trivially_copyable_v<asc::DenseView<float, 1>>);
static_assert(std::is_constructible_v<asc::DenseView<const float, 1>,
                                      asc::DenseView<float, 1>>);
static_assert(!std::is_constructible_v<asc::DenseView<float, 1>,
                                       asc::DenseView<const float, 1>>);
static_assert(static_cast<int>(asc::ExpressionOperation::kExternal) == 0);
static_assert(static_cast<int>(asc::ExpressionOperation::kScalar) == 1);
static_assert(static_cast<int>(asc::ExpressionOperation::kNegate) == 2);
static_assert(static_cast<int>(asc::ExpressionOperation::kAdd) == 3);
static_assert(static_cast<int>(asc::ExpressionOperation::kSubtract) == 4);
static_assert(static_cast<int>(asc::ExpressionOperation::kMultiply) == 5);
static_assert(static_cast<int>(asc::ExpressionOperation::kTerminal) == 6);

template <typename Result>
void CheckError(TestContext& test, const Result& result,
                asc::ErrorCode expected) {
  ASC_DENSE_TEST_CHECK(test, !result.ok());
  if (!result.ok()) {
    ASC_DENSE_TEST_EQ(test, result.status().code(), expected);
  }
}

void TestRankZeroAndContiguousLayouts(TestContext& test) {
  const std::array<asc::extent_t, 0> scalar_shape{};
  auto scalar = asc::DenseLayout<0>::Create(
      std::span<const asc::extent_t, 0>(scalar_shape));
  ASC_DENSE_TEST_CHECK(test, scalar.ok());
  if (scalar.ok()) {
    ASC_DENSE_TEST_EQ(test, scalar->logical_size(), asc::extent_t{1});
    ASC_DENSE_TEST_EQ(test, scalar->required_span_size(), std::size_t{1});
    ASC_DENSE_TEST_CHECK(test, scalar->is_unique());
    ASC_DENSE_TEST_CHECK(test, scalar->is_exhaustive());
    const std::array<asc::index_t, 0> coordinate{};
    auto offset = scalar->Offset(std::span<const asc::index_t, 0>(coordinate));
    ASC_DENSE_TEST_CHECK(test, offset.ok());
    if (offset.ok()) {
      ASC_DENSE_TEST_EQ(test, *offset, std::size_t{0});
    }
  }

  const std::array<asc::extent_t, 3> shape{2, 3, 4};
  auto left =
      asc::DenseLayout<3>::Create(std::span<const asc::extent_t, 3>(shape));
  auto right = asc::DenseLayout<3>::Create(
      std::span<const asc::extent_t, 3>(shape), asc::LayoutRight{});
  ASC_DENSE_TEST_CHECK(test, left.ok());
  ASC_DENSE_TEST_CHECK(test, right.ok());
  if (!left.ok() || !right.ok()) {
    return;
  }

  const std::array<asc::stride_t, 3> expected_left_strides{1, 2, 6};
  const std::array<asc::stride_t, 3> expected_right_strides{12, 4, 1};
  for (std::size_t dimension = 0; dimension < shape.size(); ++dimension) {
    ASC_DENSE_TEST_EQ(test, left->strides()[dimension],
                      expected_left_strides[dimension]);
    ASC_DENSE_TEST_EQ(test, right->strides()[dimension],
                      expected_right_strides[dimension]);
  }
  ASC_DENSE_TEST_EQ(test, left->kind(), asc::DenseLayoutKind::kLeft);
  ASC_DENSE_TEST_EQ(test, right->kind(), asc::DenseLayoutKind::kRight);
  ASC_DENSE_TEST_EQ(test, left->logical_size(), asc::extent_t{24});
  ASC_DENSE_TEST_EQ(test, left->required_span_size(), std::size_t{24});
  ASC_DENSE_TEST_CHECK(test, left->is_unique());
  ASC_DENSE_TEST_CHECK(test, left->is_exhaustive());
  ASC_DENSE_TEST_CHECK(test, right->is_unique());
  ASC_DENSE_TEST_CHECK(test, right->is_exhaustive());

  for (asc::index_t i2 = 0; i2 < shape[2]; ++i2) {
    for (asc::index_t i1 = 0; i1 < shape[1]; ++i1) {
      for (asc::index_t i0 = 0; i0 < shape[0]; ++i0) {
        const std::array<asc::index_t, 3> coordinate{i0, i1, i2};
        auto left_offset =
            left->Offset(std::span<const asc::index_t, 3>(coordinate));
        auto right_offset =
            right->Offset(std::span<const asc::index_t, 3>(coordinate));
        ASC_DENSE_TEST_CHECK(test, left_offset.ok());
        ASC_DENSE_TEST_CHECK(test, right_offset.ok());
        if (left_offset.ok()) {
          ASC_DENSE_TEST_EQ(
              test, *left_offset,
              static_cast<std::size_t>(i0 + shape[0] * (i1 + shape[1] * i2)));
        }
        if (right_offset.ok()) {
          ASC_DENSE_TEST_EQ(
              test, *right_offset,
              static_cast<std::size_t>((i0 * shape[1] + i1) * shape[2] + i2));
        }
      }
    }
  }
}

template <std::size_t Rank>
void CheckSmallContiguousShape(TestContext& test,
                               const std::array<asc::extent_t, Rank>& shape) {
  auto left = asc::DenseLayout<Rank>::Create(
      std::span<const asc::extent_t, Rank>(shape));
  auto right = asc::DenseLayout<Rank>::Create(
      std::span<const asc::extent_t, Rank>(shape), asc::LayoutRight{});
  ASC_DENSE_TEST_CHECK(test, left.ok());
  ASC_DENSE_TEST_CHECK(test, right.ok());
  if (!left.ok() || !right.ok()) {
    return;
  }
  ASC_DENSE_TEST_CHECK(test, left->is_unique());
  ASC_DENSE_TEST_CHECK(test, left->is_exhaustive());
  ASC_DENSE_TEST_CHECK(test, right->is_unique());
  ASC_DENSE_TEST_CHECK(test, right->is_exhaustive());

  asc::extent_t logical_size = 1;
  for (asc::extent_t extent : shape) {
    if (extent == 0) {
      logical_size = 0;
      break;
    }
    logical_size *= extent;
  }
  ASC_DENSE_TEST_EQ(test, left->logical_size(), logical_size);
  ASC_DENSE_TEST_EQ(test, right->logical_size(), logical_size);
  ASC_DENSE_TEST_EQ(test, left->required_span_size(),
                    static_cast<std::size_t>(logical_size));
  ASC_DENSE_TEST_EQ(test, right->required_span_size(),
                    static_cast<std::size_t>(logical_size));
  if (logical_size == 0) {
    return;
  }

  std::array<bool, 125> left_seen{};
  std::array<bool, 125> right_seen{};
  std::array<asc::index_t, Rank> coordinate{};
  while (true) {
    asc::stride_t expected_left = 0;
    asc::stride_t left_stride = 1;
    for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
      expected_left += coordinate[dimension] * left_stride;
      left_stride *= shape[dimension];
    }
    asc::stride_t expected_right = 0;
    asc::stride_t right_stride = 1;
    for (std::size_t reverse = Rank; reverse > 0; --reverse) {
      const std::size_t dimension = reverse - 1;
      expected_right += coordinate[dimension] * right_stride;
      right_stride *= shape[dimension];
    }

    auto left_offset =
        left->Offset(std::span<const asc::index_t, Rank>(coordinate));
    auto right_offset =
        right->Offset(std::span<const asc::index_t, Rank>(coordinate));
    ASC_DENSE_TEST_CHECK(test, left_offset.ok());
    ASC_DENSE_TEST_CHECK(test, right_offset.ok());
    if (left_offset.ok()) {
      ASC_DENSE_TEST_EQ(test, *left_offset,
                        static_cast<std::size_t>(expected_left));
      ASC_DENSE_TEST_CHECK(test, *left_offset < left->required_span_size());
      ASC_DENSE_TEST_CHECK(test, !left_seen[*left_offset]);
      left_seen[*left_offset] = true;
    }
    if (right_offset.ok()) {
      ASC_DENSE_TEST_EQ(test, *right_offset,
                        static_cast<std::size_t>(expected_right));
      ASC_DENSE_TEST_CHECK(test, *right_offset < right->required_span_size());
      ASC_DENSE_TEST_CHECK(test, !right_seen[*right_offset]);
      right_seen[*right_offset] = true;
    }

    std::size_t dimension = 0;
    for (; dimension < Rank; ++dimension) {
      ++coordinate[dimension];
      if (coordinate[dimension] < shape[dimension]) {
        break;
      }
      coordinate[dimension] = 0;
    }
    if (dimension == Rank) {
      break;
    }
  }
  for (asc::extent_t offset = 0; offset < logical_size; ++offset) {
    ASC_DENSE_TEST_CHECK(test, left_seen[static_cast<std::size_t>(offset)]);
    ASC_DENSE_TEST_CHECK(test, right_seen[static_cast<std::size_t>(offset)]);
  }
}

void TestSmallContiguousProperties(TestContext& test) {
  for (asc::extent_t extent0 = 0; extent0 <= 5; ++extent0) {
    CheckSmallContiguousShape(test, std::array<asc::extent_t, 1>{extent0});
    for (asc::extent_t extent1 = 0; extent1 <= 5; ++extent1) {
      CheckSmallContiguousShape(test,
                                std::array<asc::extent_t, 2>{extent0, extent1});
      for (asc::extent_t extent2 = 0; extent2 <= 5; ++extent2) {
        CheckSmallContiguousShape(
            test, std::array<asc::extent_t, 3>{extent0, extent1, extent2});
      }
    }
  }
}

void TestStrideLayouts(TestContext& test) {
  const std::array<asc::extent_t, 2> shape{2, 3};
  const std::array<std::array<asc::stride_t, 2>, 4> stride_cases{
      std::array<asc::stride_t, 2>{1, 2},
      std::array<asc::stride_t, 2>{3, 1},
      std::array<asc::stride_t, 2>{1, 4},
      std::array<asc::stride_t, 2>{2, 5},
  };
  const std::array<std::array<std::size_t, 6>, 4> expected_offsets{
      std::array<std::size_t, 6>{0, 1, 2, 3, 4, 5},
      std::array<std::size_t, 6>{0, 3, 1, 4, 2, 5},
      std::array<std::size_t, 6>{0, 1, 4, 5, 8, 9},
      std::array<std::size_t, 6>{0, 2, 5, 7, 10, 12},
  };
  const std::array<std::size_t, 4> expected_spans{6, 6, 10, 13};

  for (std::size_t case_index = 0; case_index < stride_cases.size();
       ++case_index) {
    auto layout = asc::DenseLayout<2>::Create(
        std::span<const asc::extent_t, 2>(shape),
        asc::LayoutStride<2>{.strides = stride_cases[case_index]});
    ASC_DENSE_TEST_CHECK(test, layout.ok());
    if (!layout.ok()) {
      continue;
    }
    ASC_DENSE_TEST_EQ(test, layout->required_span_size(),
                      expected_spans[case_index]);
    ASC_DENSE_TEST_CHECK(test, layout->is_unique());
    ASC_DENSE_TEST_EQ(test, layout->is_exhaustive(), case_index < 2);
    ASC_DENSE_TEST_EQ(test, layout->kind(), asc::DenseLayoutKind::kStride);

    std::size_t logical_index = 0;
    for (asc::index_t i1 = 0; i1 < shape[1]; ++i1) {
      for (asc::index_t i0 = 0; i0 < shape[0]; ++i0) {
        const std::array<asc::index_t, 2> coordinate{i0, i1};
        auto offset =
            layout->Offset(std::span<const asc::index_t, 2>(coordinate));
        ASC_DENSE_TEST_CHECK(test, offset.ok());
        if (offset.ok()) {
          ASC_DENSE_TEST_EQ(test, *offset,
                            expected_offsets[case_index][logical_index]);
        }
        ++logical_index;
      }
    }
  }

  for (const auto strides : {std::array<asc::stride_t, 2>{1, 1},
                             std::array<asc::stride_t, 2>{0, 1}}) {
    auto colliding =
        asc::DenseLayout<2>::Create(std::span<const asc::extent_t, 2>(shape),
                                    asc::LayoutStride<2>{.strides = strides});
    ASC_DENSE_TEST_CHECK(test, colliding.ok());
    if (colliding.ok()) {
      ASC_DENSE_TEST_CHECK(test, !colliding->is_unique());
      ASC_DENSE_TEST_CHECK(test, !colliding->is_exhaustive());
    }
  }
}

void TestZeroAndInvalidLayouts(TestContext& test) {
  const std::array<asc::extent_t, 3> zero_shape{3, 0, 5};
  auto zero = asc::DenseLayout<3>::Create(
      std::span<const asc::extent_t, 3>(zero_shape));
  ASC_DENSE_TEST_CHECK(test, zero.ok());
  if (zero.ok()) {
    ASC_DENSE_TEST_EQ(test, zero->logical_size(), asc::extent_t{0});
    ASC_DENSE_TEST_EQ(test, zero->required_span_size(), std::size_t{0});
    ASC_DENSE_TEST_CHECK(test, zero->is_unique());
    ASC_DENSE_TEST_CHECK(test, zero->is_exhaustive());
  }

  const std::array<asc::extent_t, 2> negative_shape{2, -1};
  CheckError(test,
             asc::DenseLayout<2>::Create(
                 std::span<const asc::extent_t, 2>(negative_shape)),
             asc::ErrorCode::kShape);

  const std::array<asc::extent_t, 2> shape{2, 3};
  CheckError(
      test,
      asc::DenseLayout<2>::Create(std::span<const asc::extent_t, 2>(shape),
                                  asc::LayoutStride<2>{.strides = {1, -2}}),
      asc::ErrorCode::kInvalidArgument);

  const std::array<asc::extent_t, 2> overflowing_shape{
      std::numeric_limits<asc::extent_t>::max(), 2};
  CheckError(test,
             asc::DenseLayout<2>::Create(
                 std::span<const asc::extent_t, 2>(overflowing_shape)),
             asc::ErrorCode::kOverflow);

  const std::array<asc::extent_t, 2> small_shape{2, 2};
  CheckError(test,
             asc::DenseLayout<2>::Create(
                 std::span<const asc::extent_t, 2>(small_shape),
                 asc::LayoutStride<2>{
                     .strides = {std::numeric_limits<asc::stride_t>::max(),
                                 std::numeric_limits<asc::stride_t>::max()}}),
             asc::ErrorCode::kOverflow);

  auto layout =
      asc::DenseLayout<2>::Create(std::span<const asc::extent_t, 2>(shape));
  ASC_DENSE_TEST_CHECK(test, layout.ok());
  if (layout.ok()) {
    const std::array<asc::index_t, 2> negative{-1, 0};
    const std::array<asc::index_t, 2> outside{0, 3};
    CheckError(test, layout->Offset(std::span<const asc::index_t, 2>(negative)),
               asc::ErrorCode::kIndex);
    CheckError(test, layout->Offset(std::span<const asc::index_t, 2>(outside)),
               asc::ErrorCode::kIndex);
  }
}

void TestViewCreationAndAccess(TestContext& test) {
  const std::array<asc::extent_t, 2> shape{2, 3};
  auto mapping =
      asc::DenseLayout<2>::Create(std::span<const asc::extent_t, 2>(shape),
                                  asc::LayoutStride<2>{.strides = {1, 4}});
  ASC_DENSE_TEST_CHECK(test, mapping.ok());
  if (!mapping.ok()) {
    return;
  }
  std::array<double, 10> storage{};
  for (std::size_t index = 0; index < storage.size(); ++index) {
    storage[index] = static_cast<double>(index);
  }

  auto view = asc::DenseView<double, 2>::Create(storage.data(), *mapping,
                                                asc::MemorySpace::kHost);
  ASC_DENSE_TEST_CHECK(test, view.ok());
  if (!view.ok()) {
    return;
  }
  ASC_DENSE_TEST_EQ(test, view->data(), storage.data());
  ASC_DENSE_TEST_EQ(test, view->logical_size(), asc::extent_t{6});
  const std::array<asc::index_t, 2> coordinate{1, 2};
  auto element = view->At(std::span<const asc::index_t, 2>(coordinate));
  ASC_DENSE_TEST_CHECK(test, element.ok());
  if (element.ok()) {
    ASC_DENSE_TEST_EQ(test, *element, storage.data() + 9);
    ASC_DENSE_TEST_EQ(test, **element, 9.0);
  }

  asc::DenseView<const double, 2> const_view(*view);
  auto const_element =
      const_view.At(std::span<const asc::index_t, 2>(coordinate));
  ASC_DENSE_TEST_CHECK(test, const_element.ok());
  if (const_element.ok()) {
    ASC_DENSE_TEST_EQ(test, **const_element, 9.0);
  }

  auto device_view = asc::DenseView<double, 2>::Create(
      storage.data(), *mapping, asc::MemorySpace::kDevice);
  ASC_DENSE_TEST_CHECK(test, device_view.ok());
  if (device_view.ok()) {
    CheckError(test,
               device_view->At(std::span<const asc::index_t, 2>(coordinate)),
               asc::ErrorCode::kMemoryAccess);
  }

  CheckError(test,
             asc::DenseView<double, 2>::Create(nullptr, *mapping,
                                               asc::MemorySpace::kHost),
             asc::ErrorCode::kInvalidArgument);

  const std::array<asc::extent_t, 2> empty_shape{0, 3};
  auto empty_mapping = asc::DenseLayout<2>::Create(
      std::span<const asc::extent_t, 2>(empty_shape));
  ASC_DENSE_TEST_CHECK(test, empty_mapping.ok());
  if (empty_mapping.ok()) {
    auto empty = asc::DenseView<double, 2>::Create(nullptr, *empty_mapping,
                                                   asc::MemorySpace::kHost);
    ASC_DENSE_TEST_CHECK(test, empty.ok());
  }

  auto colliding =
      asc::DenseLayout<2>::Create(std::span<const asc::extent_t, 2>(shape),
                                  asc::LayoutStride<2>{.strides = {1, 1}});
  ASC_DENSE_TEST_CHECK(test, colliding.ok());
  if (colliding.ok()) {
    CheckError(test,
               asc::DenseView<double, 2>::Create(storage.data(), *colliding,
                                                 asc::MemorySpace::kHost),
               asc::ErrorCode::kInvalidArgument);
    CheckError(test,
               asc::DenseView<const double, 2>::Create(
                   storage.data(), *colliding, asc::MemorySpace::kHost),
               asc::ErrorCode::kInvalidArgument);
  }
}

void TestSubviews(TestContext& test) {
  const std::array<asc::extent_t, 2> shape{3, 4};
  auto mapping =
      asc::DenseLayout<2>::Create(std::span<const asc::extent_t, 2>(shape),
                                  asc::LayoutStride<2>{.strides = {1, 5}});
  ASC_DENSE_TEST_CHECK(test, mapping.ok());
  if (!mapping.ok()) {
    return;
  }
  std::array<int, 18> storage{};
  auto view = asc::DenseView<int, 2>::Create(storage.data(), *mapping,
                                             asc::MemorySpace::kHost);
  ASC_DENSE_TEST_CHECK(test, view.ok());
  if (!view.ok()) {
    return;
  }

  const std::array<asc::index_t, 2> offsets{1, 1};
  const std::array<asc::extent_t, 2> extents{2, 2};
  auto subview = view->Subview(std::span<const asc::index_t, 2>(offsets),
                               std::span<const asc::extent_t, 2>(extents));
  ASC_DENSE_TEST_CHECK(test, subview.ok());
  if (subview.ok()) {
    ASC_DENSE_TEST_EQ(test, subview->data(), storage.data() + 6);
    ASC_DENSE_TEST_EQ(test, subview->extents()[0], asc::extent_t{2});
    ASC_DENSE_TEST_EQ(test, subview->extents()[1], asc::extent_t{2});
    ASC_DENSE_TEST_EQ(test, subview->strides()[0], asc::stride_t{1});
    ASC_DENSE_TEST_EQ(test, subview->strides()[1], asc::stride_t{5});
    ASC_DENSE_TEST_EQ(test, subview->memory_space(), asc::MemorySpace::kHost);
    const std::array<asc::index_t, 2> coordinate{1, 1};
    auto element = subview->At(std::span<const asc::index_t, 2>(coordinate));
    ASC_DENSE_TEST_CHECK(test, element.ok());
    if (element.ok()) {
      ASC_DENSE_TEST_EQ(test, *element, storage.data() + 12);
    }
  }

  const std::array<asc::index_t, 2> negative_offset{-1, 0};
  CheckError(test,
             view->Subview(std::span<const asc::index_t, 2>(negative_offset),
                           std::span<const asc::extent_t, 2>(extents)),
             asc::ErrorCode::kInvalidArgument);
  const std::array<asc::extent_t, 2> negative_extent{-1, 1};
  const std::array<asc::index_t, 2> zero_offset{0, 0};
  CheckError(test,
             view->Subview(std::span<const asc::index_t, 2>(zero_offset),
                           std::span<const asc::extent_t, 2>(negative_extent)),
             asc::ErrorCode::kInvalidArgument);
  const std::array<asc::index_t, 2> outside_offset{2, 3};
  CheckError(test,
             view->Subview(std::span<const asc::index_t, 2>(outside_offset),
                           std::span<const asc::extent_t, 2>(extents)),
             asc::ErrorCode::kIndex);
  const std::array<asc::index_t, 2> overflowing_offset{
      std::numeric_limits<asc::index_t>::max(), 0};
  CheckError(test,
             view->Subview(std::span<const asc::index_t, 2>(overflowing_offset),
                           std::span<const asc::extent_t, 2>(extents)),
             asc::ErrorCode::kOverflow);

  const std::array<asc::extent_t, 2> empty_extent{0, 2};
  auto empty = view->Subview(std::span<const asc::index_t, 2>(outside_offset),
                             std::span<const asc::extent_t, 2>(empty_extent));
  CheckError(test, empty, asc::ErrorCode::kIndex);

  const std::array<asc::index_t, 2> empty_offset{3, 1};
  auto valid_empty =
      view->Subview(std::span<const asc::index_t, 2>(empty_offset),
                    std::span<const asc::extent_t, 2>(empty_extent));
  ASC_DENSE_TEST_CHECK(test, valid_empty.ok());
  if (valid_empty.ok()) {
    ASC_DENSE_TEST_EQ(test, valid_empty->logical_size(), asc::extent_t{0});
    ASC_DENSE_TEST_EQ(test, valid_empty->mapping().required_span_size(),
                      std::size_t{0});
  }

  std::size_t allocation_count = 0;
  {
    asc_dense_test::AllocationProbe probe;
    auto no_allocation =
        view->Subview(std::span<const asc::index_t, 2>(offsets),
                      std::span<const asc::extent_t, 2>(extents));
    allocation_count = probe.count();
    ASC_DENSE_TEST_CHECK(test, no_allocation.ok());
  }
  ASC_DENSE_TEST_CHECK(
      test, asc_test::ProcessAllocationCountMatches(allocation_count, 0));
}

void TestExpressionAdapter(TestContext& test) {
  const std::array<asc::extent_t, 2> shape{2, 3};
  auto mapping =
      asc::DenseLayout<2>::Create(std::span<const asc::extent_t, 2>(shape),
                                  asc::LayoutStride<2>{.strides = {1, 4}});
  ASC_DENSE_TEST_CHECK(test, mapping.ok());
  if (!mapping.ok()) {
    return;
  }
  std::array<double, 11> storage{};
  storage[9] = 42.0;
  auto view = asc::DenseView<const double, 2>::Create(storage.data(), *mapping,
                                                      asc::MemorySpace::kHost);
  ASC_DENSE_TEST_CHECK(test, view.ok());
  if (!view.ok()) {
    return;
  }

  static_assert(asc::ReadableExpression<asc::DenseView<const double, 2>>);
  static_assert(asc::kExpressionRank<asc::DenseView<const double, 2>> == 2);
  ASC_DENSE_TEST_EQ(test, asc::ExpressionOperationCategory(*view),
                    asc::ExpressionOperation::kTerminal);
  ASC_DENSE_TEST_EQ(test, asc::ExpressionSparsityEffect(*view),
                    asc::SparsityEffect::kStructurePreserving);
  const auto expression_shape = asc::ExpressionShape(*view);
  ASC_DENSE_TEST_EQ(test, expression_shape[0], shape[0]);
  ASC_DENSE_TEST_EQ(test, expression_shape[1], shape[1]);
  const std::array<asc::index_t, 2> coordinate{1, 2};
  ASC_DENSE_TEST_EQ(
      test,
      asc::ExpressionRead(*view, std::span<const asc::index_t, 2>(coordinate)),
      42.0);

  ASC_DENSE_TEST_CHECK(test,
                       asc::MayAlias(*view, asc::AliasToken(storage.data())));
  ASC_DENSE_TEST_CHECK(
      test, asc::MayAlias(*view, asc::AliasToken(storage.data() + 9)));
  ASC_DENSE_TEST_CHECK(
      test, asc::MayAlias(*view, asc::AliasToken(storage.data() + 2)));
  ASC_DENSE_TEST_CHECK(
      test, !asc::MayAlias(*view, asc::AliasToken(storage.data() + 10)));
  ASC_DENSE_TEST_CHECK(test, !asc::MayAlias(*view, asc::AliasToken(nullptr)));
}

}  // namespace

int main() {
  TestContext test;
  TestRankZeroAndContiguousLayouts(test);
  TestSmallContiguousProperties(test);
  TestStrideLayouts(test);
  TestZeroAndInvalidLayouts(test);
  TestViewCreationAndAccess(test);
  TestSubviews(test);
  TestExpressionAdapter(test);
  return test.Finish();
}
