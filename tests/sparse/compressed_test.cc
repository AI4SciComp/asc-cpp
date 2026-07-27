#include "asc/sparse/compressed.h"

#include <array>
#include <concepts>
#include <cstddef>
#include <span>
#include <type_traits>
#include <utility>

#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/expression/expression.h"
#include "asc/expression/writable.h"
#include "test_support.h"

namespace {

using Csr = asc::CsrArray<double>;
using Csc = asc::CscArray<double>;

static_assert(!std::copy_constructible<Csr>);
static_assert(!std::is_copy_assignable_v<Csr>);
static_assert(std::move_constructible<Csr>);
static_assert(!std::copy_constructible<Csc>);
static_assert(std::is_trivially_copyable_v<asc::CsrView<double>>);
static_assert(std::is_trivially_copyable_v<asc::CscView<const double>>);
static_assert(
    std::constructible_from<asc::CsrView<const double>, asc::CsrView<double>>);
static_assert(
    !std::constructible_from<asc::CsrView<double>, asc::CsrView<const double>>);
static_assert(asc::ReadableExpression<asc::CsrView<const double>>);
static_assert(asc::PlacedReadableExpression<asc::CscView<const double>>);
static_assert(asc::WritableExpression<asc::CsrView<double>>);
static_assert(!asc::WritableExpression<asc::CsrView<const double>>);

constexpr std::array<asc::extent_t, 2> kShape = {4, 5};
constexpr std::array<asc::nnz_t, 5> kCsrOffsets = {0, 2, 4, 7, 9};
constexpr std::array<asc::index_t, 9> kCsrIndices = {0, 3, 1, 4, 0, 2, 4, 1, 4};
constexpr std::array<double, 9> kCsrValues = {2, -1, 4, 0, 7, 3, 5, -2, 6};
constexpr std::array<asc::nnz_t, 6> kCscOffsets = {0, 2, 4, 5, 6, 9};
constexpr std::array<asc::index_t, 9> kCscIndices = {0, 2, 1, 3, 2, 0, 1, 2, 3};
constexpr std::array<double, 9> kCscValues = {2, 7, 4, -2, 3, -1, 0, 5, 6};

template <typename Element, asc::SparseCompressedFormat Format>
std::span<const asc::nnz_t> OuterOffsets(
    const asc::CompressedSparseView<Element, Format>& view) {
  const asc::extent_t outer_extent = Format == asc::SparseCompressedFormat::kCsr
                                         ? view.shape()[0]
                                         : view.shape()[1];
  return std::span<const asc::nnz_t>(
      view.outer_offset_data(),
      static_cast<std::size_t>(outer_extent + static_cast<asc::extent_t>(1)));
}

template <typename Element, asc::SparseCompressedFormat Format>
std::span<const asc::index_t> InnerIndices(
    const asc::CompressedSparseView<Element, Format>& view) {
  return std::span<const asc::index_t>(view.inner_index_data(),
                                       static_cast<std::size_t>(view.nnz()));
}

template <typename Element, asc::SparseCompressedFormat Format>
std::span<Element> Values(
    const asc::CompressedSparseView<Element, Format>& view) {
  return std::span<Element>(view.value_data(),
                            static_cast<std::size_t>(view.nnz()));
}

void CheckCsrAndCscViews(asc_sparse_test::TestContext& context) {
  auto csr_values = kCsrValues;
  auto csr = asc::CsrView<double>::Create(kShape, kCsrOffsets, kCsrIndices,
                                          csr_values, asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(context, csr.ok());
  ASC_SPARSE_TEST_EQ(context, csr->shape(), kShape);
  ASC_SPARSE_TEST_EQ(context, csr->nnz(), 9);
  ASC_SPARSE_TEST_RANGE_EQ(context, OuterOffsets(*csr), kCsrOffsets);
  ASC_SPARSE_TEST_RANGE_EQ(context, InnerIndices(*csr), kCsrIndices);
  ASC_SPARSE_TEST_RANGE_EQ(context, Values(*csr), kCsrValues);
  ASC_SPARSE_TEST_EQ(context, csr->space(), asc::MemorySpace::kHost);

  auto explicit_zero = csr->Find(1, 4);
  auto missing = csr->Find(1, 3);
  auto stored = csr->ValueAt(7);
  ASC_SPARSE_TEST_CHECK(context, explicit_zero.ok());
  ASC_SPARSE_TEST_CHECK(context, missing.ok());
  ASC_SPARSE_TEST_CHECK(context, stored.ok());
  ASC_SPARSE_TEST_EQ(context, **explicit_zero, 0.0);
  ASC_SPARSE_TEST_CHECK(context, *missing == nullptr);
  ASC_SPARSE_TEST_EQ(context, **stored, -2.0);
  ASC_SPARSE_TEST_EQ(
      context,
      asc::ReadExpression(*csr, std::span<const asc::index_t, 2>(
                                    std::array<asc::index_t, 2>{3, 4})),
      6.0);
  ASC_SPARSE_TEST_EQ(
      context,
      asc::ReadExpression(*csr, std::span<const asc::index_t, 2>(
                                    std::array<asc::index_t, 2>{3, 3})),
      0.0);

  auto csc_values = kCscValues;
  auto csc = asc::CscView<double>::Create(kShape, kCscOffsets, kCscIndices,
                                          csc_values, asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(context, csc.ok());
  ASC_SPARSE_TEST_RANGE_EQ(context, OuterOffsets(*csc), kCscOffsets);
  ASC_SPARSE_TEST_RANGE_EQ(context, InnerIndices(*csc), kCscIndices);
  ASC_SPARSE_TEST_RANGE_EQ(context, Values(*csc), kCscValues);
  auto csc_value = csc->Find(0, 3);
  ASC_SPARSE_TEST_CHECK(context, csc_value.ok());
  ASC_SPARSE_TEST_EQ(context, **csc_value, -1.0);

  asc::CsrView<const double> const_csr(*csr);
  ASC_SPARSE_TEST_CHECK(
      context, asc::MayAlias(const_csr,
                             asc::AliasToken::FromIdentity(csr_values.data())));
  int independent = 0;
  ASC_SPARSE_TEST_CHECK(
      context,
      !asc::MayAlias(const_csr, asc::AliasToken::FromIdentity(&independent)));
}

void CheckMalformedMetadata(asc_sparse_test::TestContext& context) {
  std::array<double, 2> values = {1.0, 2.0};
  constexpr std::array<asc::extent_t, 2> kSmallShape = {2, 3};
  constexpr std::array<asc::nnz_t, 3> kOffsets = {0, 1, 2};
  constexpr std::array<asc::index_t, 2> kIndices = {0, 2};

  auto valid = asc::CsrView<double>::Create(kSmallShape, kOffsets, kIndices,
                                            values, asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(context, valid.ok());

  auto negative_shape = asc::CsrView<double>::Create(
      std::array<asc::extent_t, 2>{-1, 3}, std::array<asc::nnz_t, 1>{0},
      std::span<const asc::index_t>(), std::span<double>(),
      asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(context, !negative_shape.ok());
  ASC_SPARSE_TEST_EQ(context, negative_shape.status().code(),
                     asc::ErrorCode::kShape);

  auto wrong_lengths = asc::CsrView<double>::Create(
      kSmallShape, kOffsets, std::array<asc::index_t, 1>{0}, values,
      asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(context, !wrong_lengths.ok());
  ASC_SPARSE_TEST_EQ(context, wrong_lengths.status().code(),
                     asc::ErrorCode::kInvalidArgument);

  auto wrong_outer_length =
      asc::CsrView<double>::Create(kSmallShape, std::array<asc::nnz_t, 2>{0, 2},
                                   kIndices, values, asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(context, !wrong_outer_length.ok());

  auto nonzero_first = asc::CsrView<double>::Create(
      kSmallShape, std::array<asc::nnz_t, 3>{1, 1, 2}, kIndices, values,
      asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(context, !nonzero_first.ok());

  auto final_mismatch = asc::CsrView<double>::Create(
      kSmallShape, std::array<asc::nnz_t, 3>{0, 1, 1}, kIndices, values,
      asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(context, !final_mismatch.ok());

  constexpr std::array<asc::extent_t, 2> kThreeRows = {3, 3};
  auto decreasing = asc::CsrView<double>::Create(
      kThreeRows, std::array<asc::nnz_t, 4>{0, 2, 1, 2}, kIndices, values,
      asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(context, !decreasing.ok());
  ASC_SPARSE_TEST_EQ(context, decreasing.status().code(),
                     asc::ErrorCode::kInvalidArgument);

  auto negative_offset = asc::CsrView<double>::Create(
      kSmallShape, std::array<asc::nnz_t, 3>{0, -1, 2}, kIndices, values,
      asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(context, !negative_offset.ok());

  auto bad_index = asc::CsrView<double>::Create(
      kSmallShape, kOffsets, std::array<asc::index_t, 2>{0, 3}, values,
      asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(context, !bad_index.ok());
  ASC_SPARSE_TEST_EQ(context, bad_index.status().code(),
                     asc::ErrorCode::kIndex);

  auto unsorted = asc::CsrView<double>::Create(
      std::array<asc::extent_t, 2>{1, 3}, std::array<asc::nnz_t, 2>{0, 2},
      std::array<asc::index_t, 2>{2, 0}, values, asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(context, !unsorted.ok());

  auto duplicate = asc::CsrView<double>::Create(
      std::array<asc::extent_t, 2>{1, 3}, std::array<asc::nnz_t, 2>{0, 2},
      std::array<asc::index_t, 2>{1, 1}, values, asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(context, !duplicate.ok());

  auto bad_space =
      asc::CsrView<double>::Create(kSmallShape, kOffsets, kIndices, values,
                                   static_cast<asc::MemorySpace>(255));
  ASC_SPARSE_TEST_CHECK(context, !bad_space.ok());

  auto device = asc::CsrView<double>::Create(kSmallShape, kOffsets, kIndices,
                                             values, asc::MemorySpace::kDevice);
  ASC_SPARSE_TEST_CHECK(context, device.ok());
  ASC_SPARSE_TEST_CHECK(context, !device->Find(0, 0).ok());
  ASC_SPARSE_TEST_CHECK(context, !device->ValueAt(0).ok());
}

void CheckEmptyInvariants(asc_sparse_test::TestContext& context) {
  std::array<double, 0> values{};
  std::array<asc::index_t, 0> indices{};
  constexpr std::array<asc::nnz_t, 1> kOneZero = {0};
  constexpr std::array<asc::nnz_t, 6> kSixZeros = {0, 0, 0, 0, 0, 0};
  constexpr std::array<asc::nnz_t, 5> kFiveZeros = {0, 0, 0, 0, 0};

  auto zero_rows =
      asc::CsrView<double>::Create(std::array<asc::extent_t, 2>{0, 5}, kOneZero,
                                   indices, values, asc::MemorySpace::kHost);
  auto zero_columns =
      asc::CscView<double>::Create(std::array<asc::extent_t, 2>{4, 0}, kOneZero,
                                   indices, values, asc::MemorySpace::kHost);
  auto empty_csc = asc::CscView<double>::Create(
      std::array<asc::extent_t, 2>{4, 5}, kSixZeros, indices, values,
      asc::MemorySpace::kHost);
  auto empty_csr = asc::CsrView<double>::Create(
      std::array<asc::extent_t, 2>{4, 5}, kFiveZeros, indices, values,
      asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(context, zero_rows.ok());
  ASC_SPARSE_TEST_CHECK(context, zero_columns.ok());
  ASC_SPARSE_TEST_CHECK(context, empty_csc.ok());
  ASC_SPARSE_TEST_CHECK(context, empty_csr.ok());
  ASC_SPARSE_TEST_RANGE_EQ(context, OuterOffsets(*zero_rows), kOneZero);
  ASC_SPARSE_TEST_RANGE_EQ(context, OuterOffsets(*zero_columns), kOneZero);
  ASC_SPARSE_TEST_RANGE_EQ(context, OuterOffsets(*empty_csc), kSixZeros);
  ASC_SPARSE_TEST_RANGE_EQ(context, OuterOffsets(*empty_csr), kFiveZeros);
}

void CheckOverlapRejection(asc_sparse_test::TestContext& context) {
  constexpr std::array<asc::extent_t, 2> kSmallShape = {2, 2};
  std::array<double, 2> separate_values = {4.0, 5.0};

  alignas(std::max_align_t) std::array<std::byte, 96> structural_storage{};
  const auto structural_snapshot = structural_storage;
  auto* structural = reinterpret_cast<std::int64_t*>(structural_storage.data());
  structural[0] = 0;
  structural[1] = 1;
  structural[2] = 2;
  structural[3] = 0;
  structural[4] = 1;
  const auto initialized_structural_snapshot = structural_storage;
  auto structural_overlap = asc::CsrView<double>::Create(
      kSmallShape, std::span<const asc::nnz_t>(structural, 3),
      std::span<const asc::index_t>(structural + 2, 2), separate_values,
      asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(context, !structural_overlap.ok());
  ASC_SPARSE_TEST_EQ(context, structural_overlap.status().code(),
                     asc::ErrorCode::kInvalidArgument);
  ASC_SPARSE_TEST_EQ(context, structural_storage,
                     initialized_structural_snapshot);
  static_cast<void>(structural_snapshot);

  alignas(std::max_align_t) std::array<std::byte, 96> value_storage{};
  auto* words = reinterpret_cast<std::int64_t*>(value_storage.data());
  words[0] = 0;
  words[1] = 1;
  words[2] = 2;
  words[3] = 0;
  words[4] = 1;
  const auto value_snapshot = value_storage;
  auto outer_value_overlap = asc::CsrView<double>::Create(
      kSmallShape, std::span<const asc::nnz_t>(words, 3),
      std::span<const asc::index_t>(words + 3, 2),
      std::span<double>(reinterpret_cast<double*>(words), 2),
      asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(context, !outer_value_overlap.ok());
  ASC_SPARSE_TEST_EQ(context, outer_value_overlap.status().code(),
                     asc::ErrorCode::kInvalidArgument);
  ASC_SPARSE_TEST_EQ(context, value_storage, value_snapshot);

  auto inner_value_overlap = asc::CsrView<double>::Create(
      kSmallShape, std::span<const asc::nnz_t>(words, 3),
      std::span<const asc::index_t>(words + 3, 2),
      std::span<double>(reinterpret_cast<double*>(words + 3), 2),
      asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(context, !inner_value_overlap.ok());
  ASC_SPARSE_TEST_EQ(context, inner_value_overlap.status().code(),
                     asc::ErrorCode::kInvalidArgument);
  ASC_SPARSE_TEST_EQ(context, value_storage, value_snapshot);
}

void CheckOwnershipAndRollback(asc_sparse_test::TestContext& context) {
  for (std::size_t failed_attempt = 1; failed_attempt <= 3; ++failed_attempt) {
    asc_sparse_test::CountingMemoryResource resource;
    resource.FailAllocationAttempt(failed_attempt);
    auto result =
        Csr::Create(kShape, kCsrOffsets, kCsrIndices, kCsrValues, resource);
    ASC_SPARSE_TEST_CHECK(context, !result.ok());
    ASC_SPARSE_TEST_EQ(context, result.status().code(),
                       asc::ErrorCode::kAllocation);
    ASC_SPARSE_TEST_EQ(context, resource.successful_allocations(),
                       failed_attempt - 1);
    ASC_SPARSE_TEST_EQ(context, resource.deallocations(), failed_attempt - 1);
    ASC_SPARSE_TEST_EQ(context, resource.live_allocations(), 0U);
  }

  asc_sparse_test::CountingMemoryResource resource;
  auto owner =
      Csr::Create(kShape, kCsrOffsets, kCsrIndices, kCsrValues, resource);
  ASC_SPARSE_TEST_CHECK(context, owner.ok());
  ASC_SPARSE_TEST_EQ(context, resource.successful_allocations(), 3U);
  auto view = owner->view();
  ASC_SPARSE_TEST_CHECK(context, view.ok());
  ASC_SPARSE_TEST_RANGE_EQ(context, Values(*view), kCsrValues);

  Csr moved(std::move(*owner));
  ASC_SPARSE_TEST_CHECK(context, !owner->view().ok());
  ASC_SPARSE_TEST_CHECK(context, moved.view().ok());

  asc_sparse_test::CountingMemoryResource device_resource;
  device_resource.set_space(asc::MemorySpace::kDevice);
  auto unsupported = Csr::Create(kShape, kCsrOffsets, kCsrIndices, kCsrValues,
                                 device_resource);
  ASC_SPARSE_TEST_CHECK(context, !unsupported.ok());
  ASC_SPARSE_TEST_EQ(context, unsupported.status().code(),
                     asc::ErrorCode::kUnsupported);
}

}  // namespace

int main() {
  asc_sparse_test::TestContext context;
  CheckCsrAndCscViews(context);
  CheckMalformedMetadata(context);
  CheckEmptyInvariants(context);
  CheckOverlapRejection(context);
  CheckOwnershipAndRollback(context);
  return context.Finish();
}
