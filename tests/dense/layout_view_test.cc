#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>

#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/layout.h"
#include "asc/dense/view.h"
#include "test_support.h"

namespace {

static_assert(asc::DenseElement<double>);
static_assert(asc::DenseElement<const double>);
static_assert(std::is_trivially_copyable_v<asc::DenseView<double, 2>>);
static_assert(std::is_constructible_v<asc::DenseView<const double, 2>,
                                      asc::DenseView<double, 2>>);
static_assert(!std::is_constructible_v<asc::DenseView<double, 2>,
                                       asc::DenseView<const double, 2>>);

template <std::size_t Rank>
auto MakeLeft(const std::array<asc::extent_t, Rank>& shape) {
  return asc::DenseLayoutMapping<Rank>::Create(asc::LayoutLeft{}, shape);
}

template <std::size_t Rank>
auto MakeRight(const std::array<asc::extent_t, Rank>& shape) {
  return asc::DenseLayoutMapping<Rank>::Create(asc::LayoutRight{}, shape);
}

template <std::size_t Rank>
auto MakeStride(const std::array<asc::extent_t, Rank>& shape,
                const std::array<asc::stride_t, Rank>& strides) {
  return asc::DenseLayoutMapping<Rank>::Create(asc::LayoutStride{}, shape,
                                               strides);
}

void CheckRankZeroAndZeroExtents(asc_dense_test::TestContext& context) {
  const std::array<asc::extent_t, 0> scalar_shape = {};
  const auto scalar_mapping = MakeLeft(scalar_shape);
  ASC_DENSE_TEST_CHECK(context, scalar_mapping.ok());
  ASC_DENSE_TEST_EQ(context, scalar_mapping->logical_size(), 1);
  ASC_DENSE_TEST_EQ(context, scalar_mapping->required_span_size(), 1);
  ASC_DENSE_TEST_CHECK(context, scalar_mapping->is_unique());
  ASC_DENSE_TEST_CHECK(context, scalar_mapping->is_exhaustive());
  const std::array<asc::index_t, 0> scalar_index = {};
  const auto scalar_offset = scalar_mapping->Offset(scalar_index);
  ASC_DENSE_TEST_CHECK(context, scalar_offset.ok());
  ASC_DENSE_TEST_EQ(context, *scalar_offset, 0);

  double scalar = 7.0;
  const auto scalar_view = asc::DenseView<double, 0>::Create(
      &scalar, *scalar_mapping, asc::MemorySpace::kHost);
  ASC_DENSE_TEST_CHECK(context, scalar_view.ok());
  const auto scalar_element = scalar_view->At(scalar_index);
  ASC_DENSE_TEST_CHECK(context, scalar_element.ok());
  ASC_DENSE_TEST_EQ(context, **scalar_element, 7.0);

  const std::array<asc::extent_t, 3> zero_left_shape = {
      std::numeric_limits<asc::extent_t>::max(),
      std::numeric_limits<asc::extent_t>::max(), 0};
  const auto zero_left = MakeLeft(zero_left_shape);
  ASC_DENSE_TEST_CHECK(context, zero_left.ok());
  if (zero_left.ok()) {
    ASC_DENSE_TEST_EQ(context, zero_left->logical_size(), 0);
    ASC_DENSE_TEST_EQ(context, zero_left->required_span_size(), 0);
    ASC_DENSE_TEST_CHECK(context, zero_left->is_unique());
    ASC_DENSE_TEST_CHECK(context, zero_left->is_exhaustive());
  }

  const std::array<asc::extent_t, 3> zero_right_shape = {
      0, std::numeric_limits<asc::extent_t>::max(),
      std::numeric_limits<asc::extent_t>::max()};
  const auto zero_right = MakeRight(zero_right_shape);
  ASC_DENSE_TEST_CHECK(context, zero_right.ok());
  if (zero_right.ok()) {
    ASC_DENSE_TEST_EQ(context, zero_right->logical_size(), 0);
    ASC_DENSE_TEST_EQ(context, zero_right->required_span_size(), 0);
    ASC_DENSE_TEST_CHECK(context, zero_right->is_unique());
    ASC_DENSE_TEST_CHECK(context, zero_right->is_exhaustive());
  }

  const std::array<asc::extent_t, 2> zero_stride_shape = {
      0, std::numeric_limits<asc::extent_t>::max()};
  const std::array<asc::stride_t, 2> zero_strides = {
      0, std::numeric_limits<asc::stride_t>::max()};
  const auto zero_stride = MakeStride(zero_stride_shape, zero_strides);
  ASC_DENSE_TEST_CHECK(context, zero_stride.ok());
  if (zero_stride.ok()) {
    ASC_DENSE_TEST_EQ(context, zero_stride->logical_size(), 0);
    ASC_DENSE_TEST_EQ(context, zero_stride->required_span_size(), 0);
  }
}

void CheckNamedAndPaddedMappings(asc_dense_test::TestContext& context) {
  const std::array<asc::extent_t, 3> shape = {2, 3, 4};
  const auto left = MakeLeft(shape);
  const auto right = MakeRight(shape);
  ASC_DENSE_TEST_CHECK(context, left.ok());
  ASC_DENSE_TEST_CHECK(context, right.ok());
  ASC_DENSE_TEST_EQ(context, left->strides(),
                    (std::array<asc::stride_t, 3>{1, 2, 6}));
  ASC_DENSE_TEST_EQ(context, right->strides(),
                    (std::array<asc::stride_t, 3>{12, 4, 1}));
  ASC_DENSE_TEST_EQ(context, left->logical_size(), 24);
  ASC_DENSE_TEST_EQ(context, left->required_span_size(), 24);
  ASC_DENSE_TEST_CHECK(context, left->is_unique());
  ASC_DENSE_TEST_CHECK(context, left->is_exhaustive());
  ASC_DENSE_TEST_CHECK(context, right->is_unique());
  ASC_DENSE_TEST_CHECK(context, right->is_exhaustive());

  const std::array<asc::index_t, 3> coordinate = {1, 1, 2};
  const auto left_offset = left->Offset(coordinate);
  const auto right_offset = right->Offset(coordinate);
  ASC_DENSE_TEST_CHECK(context, left_offset.ok());
  ASC_DENSE_TEST_CHECK(context, right_offset.ok());
  ASC_DENSE_TEST_EQ(context, *left_offset, 15);
  ASC_DENSE_TEST_EQ(context, *right_offset, 18);

  const std::array<asc::extent_t, 2> padded_shape = {2, 3};
  const std::array<asc::stride_t, 2> padded_strides = {1, 4};
  const auto padded = MakeStride(padded_shape, padded_strides);
  ASC_DENSE_TEST_CHECK(context, padded.ok());
  ASC_DENSE_TEST_CHECK(context, padded->is_unique());
  ASC_DENSE_TEST_CHECK(context, !padded->is_exhaustive());
  ASC_DENSE_TEST_EQ(context, padded->logical_size(), 6);
  ASC_DENSE_TEST_EQ(context, padded->required_span_size(), 10);

  std::array<bool, 10> observed{};
  for (asc::index_t column = 0; column < 3; ++column) {
    for (asc::index_t row = 0; row < 2; ++row) {
      const std::array<asc::index_t, 2> index = {row, column};
      const auto offset = padded->Offset(index);
      ASC_DENSE_TEST_CHECK(context, offset.ok());
      if (offset.ok()) {
        const auto converted = asc::CheckedCast<std::size_t>(*offset);
        ASC_DENSE_TEST_CHECK(context, converted.ok());
        if (converted.ok()) {
          ASC_DENSE_TEST_CHECK(context, !observed[*converted]);
          observed[*converted] = true;
        }
      }
    }
  }
}

void CheckInvalidMappingsAndBounds(asc_dense_test::TestContext& context) {
  const std::array<asc::extent_t, 2> negative_shape = {-1, 2};
  const auto negative = MakeLeft(negative_shape);
  ASC_DENSE_TEST_CHECK(context, !negative.ok());

  const std::array<asc::extent_t, 2> shape = {2, 2};
  const std::array<asc::stride_t, 2> negative_strides = {1, -2};
  const auto negative_stride = MakeStride(shape, negative_strides);
  ASC_DENSE_TEST_CHECK(context, !negative_stride.ok());
  ASC_DENSE_TEST_EQ(context, negative_stride.status().code(),
                    asc::ErrorCode::kInvalidArgument);

  const std::array<asc::stride_t, 2> repeated_strides = {1, 1};
  const auto repeated = MakeStride(shape, repeated_strides);
  ASC_DENSE_TEST_CHECK(context, repeated.ok());
  ASC_DENSE_TEST_CHECK(context, !repeated->is_unique());
  ASC_DENSE_TEST_CHECK(context, !repeated->is_exhaustive());

  std::array<double, 4> values{};
  const auto mutable_repeated = asc::DenseView<double, 2>::Create(
      values.data(), *repeated, asc::MemorySpace::kHost);
  const auto const_repeated = asc::DenseView<const double, 2>::Create(
      values.data(), *repeated, asc::MemorySpace::kHost);
  ASC_DENSE_TEST_CHECK(context, !mutable_repeated.ok());
  ASC_DENSE_TEST_CHECK(context, const_repeated.ok());

  const std::array<asc::extent_t, 2> overflow_shape = {
      std::numeric_limits<asc::extent_t>::max(), 2};
  const auto overflow = MakeLeft(overflow_shape);
  ASC_DENSE_TEST_CHECK(context, !overflow.ok());
  ASC_DENSE_TEST_EQ(context, overflow.status().code(),
                    asc::ErrorCode::kOverflow);

  const auto mapping = MakeLeft(shape);
  ASC_DENSE_TEST_CHECK(context, mapping.ok());
  const std::array<asc::index_t, 2> negative_index = {-1, 0};
  const std::array<asc::index_t, 2> end_index = {0, 2};
  ASC_DENSE_TEST_CHECK(context, !mapping->Offset(negative_index).ok());
  ASC_DENSE_TEST_CHECK(context, !mapping->Offset(end_index).ok());

  const auto nonempty_null = asc::DenseView<double, 2>::Create(
      nullptr, *mapping, asc::MemorySpace::kHost);
  ASC_DENSE_TEST_CHECK(context, !nonempty_null.ok());

  const std::array<asc::extent_t, 2> empty_shape = {2, 0};
  const auto empty_mapping = MakeRight(empty_shape);
  ASC_DENSE_TEST_CHECK(context, empty_mapping.ok());
  const auto empty_null = asc::DenseView<double, 2>::Create(
      nullptr, *empty_mapping, asc::MemorySpace::kHost);
  ASC_DENSE_TEST_CHECK(context, empty_null.ok());
}

void CheckViewsAndSubviews(asc_dense_test::TestContext& context) {
  const std::array<asc::extent_t, 2> shape = {4, 5};
  const auto mapping = MakeLeft(shape);
  ASC_DENSE_TEST_CHECK(context, mapping.ok());

  std::array<double, 20> values{};
  for (std::size_t index = 0; index < values.size(); ++index) {
    values[index] = static_cast<double>(index);
  }
  const auto view = asc::DenseView<double, 2>::Create(values.data(), *mapping,
                                                      asc::MemorySpace::kHost);
  ASC_DENSE_TEST_CHECK(context, view.ok());

  const std::array<asc::index_t, 2> index = {2, 3};
  const auto element = view->At(index);
  ASC_DENSE_TEST_CHECK(context, element.ok());
  ASC_DENSE_TEST_EQ(context, **element, 14.0);
  **element = 99.0;
  ASC_DENSE_TEST_EQ(context, values[14], 99.0);

  const asc::DenseView<const double, 2> const_view(*view);
  const auto const_element = const_view.At(index);
  ASC_DENSE_TEST_CHECK(context, const_element.ok());
  ASC_DENSE_TEST_EQ(context, **const_element, 99.0);

  const std::array<asc::index_t, 2> offsets = {1, 2};
  const std::array<asc::extent_t, 2> extents = {2, 2};
  const auto subview = view->Subview(offsets, extents);
  ASC_DENSE_TEST_CHECK(context, subview.ok());
  ASC_DENSE_TEST_EQ(context, subview->shape(), extents);
  ASC_DENSE_TEST_EQ(context, subview->mapping().strides(), mapping->strides());
  ASC_DENSE_TEST_EQ(context, subview->space(), asc::MemorySpace::kHost);
  ASC_DENSE_TEST_EQ(context, subview->data(), values.data() + 9);
  ASC_DENSE_TEST_EQ(context, subview->alias_token(), view->alias_token());
  const std::array<asc::index_t, 2> subindex = {1, 1};
  const auto subelement = subview->At(subindex);
  ASC_DENSE_TEST_CHECK(context, subelement.ok());
  ASC_DENSE_TEST_EQ(context, **subelement, values[14]);

  const std::array<asc::index_t, 2> invalid_offsets = {3, 4};
  const auto invalid_subview = view->Subview(invalid_offsets, extents);
  ASC_DENSE_TEST_CHECK(context, !invalid_subview.ok());

  const std::array<asc::index_t, 2> end_offsets = {4, 5};
  const std::array<asc::extent_t, 2> empty_extents = {0, 0};
  const auto empty_subview = view->Subview(end_offsets, empty_extents);
  ASC_DENSE_TEST_CHECK(context, empty_subview.ok());
  ASC_DENSE_TEST_EQ(context, empty_subview->data(), view->data());

  const auto device_view = asc::DenseView<const double, 2>::Create(
      values.data(), *mapping, asc::MemorySpace::kDevice);
  ASC_DENSE_TEST_CHECK(context, device_view.ok());
  const auto inaccessible = device_view->At(index);
  ASC_DENSE_TEST_CHECK(context, !inaccessible.ok());
  ASC_DENSE_TEST_EQ(context, inaccessible.status().code(),
                    asc::ErrorCode::kMemoryAccess);
}

}  // namespace

int main() {
  asc_dense_test::TestContext context;
  CheckRankZeroAndZeroExtents(context);
  CheckNamedAndPaddedMappings(context);
  CheckInvalidMappingsAndBounds(context);
  CheckViewsAndSubviews(context);
  return context.Finish();
}
