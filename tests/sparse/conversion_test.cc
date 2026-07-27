#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/sparse/compressed.h"
#include "test_support.h"

namespace {

using MatrixExtents = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;
using CoordinateOwner = asc::CoordinateArray<double, MatrixExtents>;

constexpr std::array<asc::extent_t, 2> kShape = {4, 5};
constexpr std::array<asc::index_t, 18> kCoordinates = {
    0, 0, 0, 3, 1, 1, 1, 4, 2, 0, 2, 2, 2, 4, 3, 1, 3, 4};
constexpr std::array<double, 9> kValues = {2, -1, 4, 0, 7, 3, 5, -2, 6};
constexpr std::array<asc::nnz_t, 5> kCsrOffsets = {0, 2, 4, 7, 9};
constexpr std::array<asc::index_t, 9> kCsrIndices = {0, 3, 1, 4, 0, 2, 4, 1, 4};
constexpr std::array<asc::nnz_t, 6> kCscOffsets = {0, 2, 4, 5, 6, 9};
constexpr std::array<asc::index_t, 9> kCscIndices = {0, 2, 1, 3, 2, 0, 1, 2, 3};
constexpr std::array<double, 9> kCscValues = {2, 7, 4, -2, 3, -1, 0, 5, 6};

template <typename Element, std::size_t Rank>
void CheckCoordinateView(asc_sparse_test::TestContext& context,
                         const asc::CoordinateView<Element, Rank>& view,
                         std::span<const asc::index_t> coordinates,
                         std::span<const double> values) {
  ASC_SPARSE_TEST_EQ(context, view.nnz(),
                     static_cast<asc::nnz_t>(values.size()));
  ASC_SPARSE_TEST_RANGE_EQ(
      context,
      std::span<const asc::index_t>(view.coordinate_data(),
                                    static_cast<std::size_t>(view.nnz()) *
                                        static_cast<std::size_t>(Rank)),
      coordinates);
  ASC_SPARSE_TEST_RANGE_EQ(
      context,
      std::span<const Element>(view.value_data(),
                               static_cast<std::size_t>(view.nnz())),
      values);
}

template <typename Element, asc::SparseCompressedFormat Format>
void CheckCompressedView(asc_sparse_test::TestContext& context,
                         const asc::CompressedSparseView<Element, Format>& view,
                         std::span<const asc::nnz_t> offsets,
                         std::span<const asc::index_t> indices,
                         std::span<const double> values) {
  const asc::extent_t outer_extent = Format == asc::SparseCompressedFormat::kCsr
                                         ? view.shape()[0]
                                         : view.shape()[1];
  ASC_SPARSE_TEST_EQ(context, view.shape(), kShape);
  ASC_SPARSE_TEST_EQ(context, view.nnz(),
                     static_cast<asc::nnz_t>(values.size()));
  ASC_SPARSE_TEST_RANGE_EQ(
      context,
      std::span<const asc::nnz_t>(view.outer_offset_data(),
                                  static_cast<std::size_t>(outer_extent + 1)),
      offsets);
  ASC_SPARSE_TEST_RANGE_EQ(
      context,
      std::span<const asc::index_t>(view.inner_index_data(),
                                    static_cast<std::size_t>(view.nnz())),
      indices);
  ASC_SPARSE_TEST_RANGE_EQ(
      context,
      std::span<const Element>(view.value_data(),
                               static_cast<std::size_t>(view.nnz())),
      values);
}

asc::Result<CoordinateOwner> MakeCoordinateOwner(
    asc::MemoryResource& resource) {
  auto extents = MatrixExtents::Create(4, 5);
  if (!extents.ok()) {
    return extents.status();
  }
  auto builder = asc::CoordinateBuilder<double, MatrixExtents>::Create(
      *extents, 9, resource);
  if (!builder.ok()) {
    return builder.status();
  }
  for (std::size_t position = 0; position < kValues.size(); ++position) {
    const std::array<asc::index_t, 2> coordinate = {
        kCoordinates[2 * position], kCoordinates[2 * position + 1]};
    const asc::Status added = builder->Add(coordinate, kValues[position]);
    if (!added.ok()) {
      return added;
    }
  }
  return builder->Finalize(asc::ExecutionContext::Serial(),
                           asc::DuplicatePolicy::kReject,
                           asc::ExplicitZeroPolicy::kKeep);
}

void CheckAllSixConversions(asc_sparse_test::TestContext& context) {
  asc::HostMemoryResource source_resource;
  auto coordinate = MakeCoordinateOwner(source_resource);
  ASC_SPARSE_TEST_CHECK(context, coordinate.ok());
  auto coordinate_view = coordinate->view();

  asc_sparse_test::CountingMemoryResource csr_resource;
  auto csr = asc::ToCsr(asc::ExecutionContext::Serial(), *coordinate_view,
                        csr_resource);
  ASC_SPARSE_TEST_CHECK(context, csr.ok());
  ASC_SPARSE_TEST_EQ(context, csr_resource.successful_allocations(), 3U);
  auto csr_view = csr->view();
  CheckCompressedView(context, *csr_view, kCsrOffsets, kCsrIndices, kValues);

  asc_sparse_test::CountingMemoryResource csc_resource;
  auto csc = asc::ToCsc(asc::ExecutionContext::Serial(), *coordinate_view,
                        csc_resource);
  ASC_SPARSE_TEST_CHECK(context, csc.ok());
  ASC_SPARSE_TEST_EQ(context, csc_resource.successful_allocations(), 3U);
  auto csc_view = csc->view();
  CheckCompressedView(context, *csc_view, kCscOffsets, kCscIndices, kCscValues);

  asc_sparse_test::CountingMemoryResource csr_to_csc_resource;
  auto csc_from_csr = asc::ToCsc(asc::ExecutionContext::Serial(), *csr_view,
                                 csr_to_csc_resource);
  ASC_SPARSE_TEST_CHECK(context, csc_from_csr.ok());
  ASC_SPARSE_TEST_EQ(context, csr_to_csc_resource.successful_allocations(), 3U);
  auto csc_from_csr_view = csc_from_csr->view();
  CheckCompressedView(context, *csc_from_csr_view, kCscOffsets, kCscIndices,
                      kCscValues);

  asc_sparse_test::CountingMemoryResource csc_to_csr_resource;
  auto csr_from_csc = asc::ToCsr(asc::ExecutionContext::Serial(), *csc_view,
                                 csc_to_csr_resource);
  ASC_SPARSE_TEST_CHECK(context, csr_from_csc.ok());
  ASC_SPARSE_TEST_EQ(context, csc_to_csr_resource.successful_allocations(), 3U);
  auto csr_from_csc_view = csr_from_csc->view();
  CheckCompressedView(context, *csr_from_csc_view, kCsrOffsets, kCsrIndices,
                      kValues);

  asc_sparse_test::CountingMemoryResource csr_to_coordinate_resource;
  auto coordinate_from_csr = asc::ToCoordinate(
      asc::ExecutionContext::Serial(), *csr_view, csr_to_coordinate_resource);
  ASC_SPARSE_TEST_CHECK(context, coordinate_from_csr.ok());
  ASC_SPARSE_TEST_EQ(context,
                     csr_to_coordinate_resource.successful_allocations(), 2U);
  auto coordinate_from_csr_view = coordinate_from_csr->view();
  CheckCoordinateView(context, *coordinate_from_csr_view, kCoordinates,
                      kValues);

  asc_sparse_test::CountingMemoryResource csc_to_coordinate_resource;
  auto coordinate_from_csc = asc::ToCoordinate(
      asc::ExecutionContext::Serial(), *csc_view, csc_to_coordinate_resource);
  ASC_SPARSE_TEST_CHECK(context, coordinate_from_csc.ok());
  ASC_SPARSE_TEST_EQ(context,
                     csc_to_coordinate_resource.successful_allocations(), 2U);
  auto coordinate_from_csc_view = coordinate_from_csc->view();
  CheckCoordinateView(context, *coordinate_from_csc_view, kCoordinates,
                      kValues);

  CheckCoordinateView(context, *coordinate_view, kCoordinates, kValues);
}

void CheckPayloadAndEmptyRoundTrips(asc_sparse_test::TestContext& context) {
  constexpr std::uint64_t kNanBits = 0x7ff8000000001234ULL;
  std::array<double, 1> nan_values = {std::bit_cast<double>(kNanBits)};
  constexpr std::array<asc::extent_t, 2> kOneShape = {1, 1};
  constexpr std::array<asc::nnz_t, 2> kOneOffsets = {0, 1};
  constexpr std::array<asc::index_t, 1> kOneIndex = {0};
  auto nan_csr = asc::CsrView<const double>::Create(
      kOneShape, kOneOffsets, kOneIndex, std::span<const double>(nan_values),
      asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(context, nan_csr.ok());
  asc::HostMemoryResource csc_resource;
  auto nan_csc =
      asc::ToCsc(asc::ExecutionContext::Serial(), *nan_csr, csc_resource);
  ASC_SPARSE_TEST_CHECK(context, nan_csc.ok());
  auto nan_csc_view = nan_csc->view();
  asc::HostMemoryResource csr_resource;
  auto nan_round_trip =
      asc::ToCsr(asc::ExecutionContext::Serial(), *nan_csc_view, csr_resource);
  ASC_SPARSE_TEST_CHECK(context, nan_round_trip.ok());
  auto nan_round_trip_view = nan_round_trip->view();
  ASC_SPARSE_TEST_EQ(
      context,
      std::bit_cast<std::uint64_t>(nan_round_trip_view->value_data()[0]),
      kNanBits);

  constexpr std::array<asc::extent_t, 2> kEmptyShape = {0, 5};
  constexpr std::array<asc::nnz_t, 1> kEmptyOffsets = {0};
  const std::span<const asc::index_t> empty_indices;
  const std::span<const double> empty_values;
  auto empty = asc::CsrView<const double>::Create(kEmptyShape, kEmptyOffsets,
                                                  empty_indices, empty_values,
                                                  asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(context, empty.ok());
  asc::HostMemoryResource empty_csc_resource;
  auto empty_csc =
      asc::ToCsc(asc::ExecutionContext::Serial(), *empty, empty_csc_resource);
  ASC_SPARSE_TEST_CHECK(context, empty_csc.ok());
  auto empty_csc_view = empty_csc->view();
  ASC_SPARSE_TEST_EQ(context, empty_csc_view->shape(), kEmptyShape);
  ASC_SPARSE_TEST_EQ(context, empty_csc_view->nnz(), 0);
  for (std::size_t index = 0; index < 6; ++index) {
    ASC_SPARSE_TEST_EQ(context, empty_csc_view->outer_offset_data()[index], 0);
  }
}

void CheckTransactionalAllocationFailure(
    asc_sparse_test::TestContext& context) {
  asc::HostMemoryResource source_resource;
  auto coordinate = MakeCoordinateOwner(source_resource);
  auto coordinate_view = coordinate->view();
  const std::array<asc::index_t, 18> coordinate_snapshot = kCoordinates;
  const std::array<double, 9> value_snapshot = kValues;

  for (std::size_t failed_attempt = 1; failed_attempt <= 3; ++failed_attempt) {
    asc_sparse_test::CountingMemoryResource resource;
    resource.FailAllocationAttempt(failed_attempt);
    auto result =
        asc::ToCsr(asc::ExecutionContext::Serial(), *coordinate_view, resource);
    ASC_SPARSE_TEST_CHECK(context, !result.ok());
    ASC_SPARSE_TEST_EQ(context, result.status().code(),
                       asc::ErrorCode::kAllocation);
    ASC_SPARSE_TEST_EQ(context, resource.live_allocations(), 0U);
    CheckCoordinateView(context, *coordinate_view, coordinate_snapshot,
                        value_snapshot);
  }

  asc::HostMemoryResource csr_resource;
  auto csr = asc::ToCsr(asc::ExecutionContext::Serial(), *coordinate_view,
                        csr_resource);
  auto csr_view = csr->view();
  for (std::size_t failed_attempt = 1; failed_attempt <= 2; ++failed_attempt) {
    asc_sparse_test::CountingMemoryResource resource;
    resource.FailAllocationAttempt(failed_attempt);
    auto result =
        asc::ToCoordinate(asc::ExecutionContext::Serial(), *csr_view, resource);
    ASC_SPARSE_TEST_CHECK(context, !result.ok());
    ASC_SPARSE_TEST_EQ(context, result.status().code(),
                       asc::ErrorCode::kAllocation);
    ASC_SPARSE_TEST_EQ(context, resource.live_allocations(), 0U);
    CheckCompressedView(context, *csr_view, kCsrOffsets, kCsrIndices, kValues);
  }
}

}  // namespace

int main() {
  asc_sparse_test::TestContext context;
  CheckAllSixConversions(context);
  CheckPayloadAndEmptyRoundTrips(context);
  CheckTransactionalAllocationFailure(context);
  return context.Finish();
}
