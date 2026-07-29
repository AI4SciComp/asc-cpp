#include <array>
#include <cstddef>
#include <limits>
#include <span>
#include <type_traits>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/expression/expression.h"
#include "asc/expression/writable.h"
#include "asc/sparse/compressed.h"
#include "asc/sparse/coordinate.h"
#include "test_resources.h"
#include "test_support.h"

namespace {

using asc_sparse_test::TestContext;
using DynamicMatrixExtents =
    asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;

constexpr std::array<asc::extent_t, 2> kShape{3, 4};
constexpr std::array<asc::nnz_t, 4> kCsrOffsets{0, 2, 4, 5};
constexpr std::array<asc::index_t, 5> kCsrIndices{1, 3, 0, 2, 1};
constexpr std::array<double, 5> kCsrValues{2.0, -1.0, 4.0, 5.0, 3.0};
constexpr std::array<asc::nnz_t, 5> kCscOffsets{0, 1, 3, 4, 5};
constexpr std::array<asc::index_t, 5> kCscIndices{1, 0, 2, 1, 0};
constexpr std::array<double, 5> kCscValues{4.0, 2.0, 3.0, 5.0, -1.0};
constexpr std::array<asc::index_t, 10> kCoordinateIndices{0, 1, 0, 3, 1,
                                                          0, 1, 2, 2, 1};

static_assert(!std::is_copy_constructible_v<asc::CsrArray<double>>);
static_assert(!std::is_copy_assignable_v<asc::CscArray<double>>);
static_assert(std::is_move_constructible_v<asc::CsrArray<double>>);
static_assert(std::is_trivially_copyable_v<asc::CsrView<double>>);
static_assert(std::is_trivially_copyable_v<asc::CscView<const double>>);
static_assert(
    std::is_constructible_v<asc::CsrView<const double>, asc::CsrView<double>>);
static_assert(
    !std::is_constructible_v<asc::CsrView<double>, asc::CsrView<const double>>);

template <typename Result>
void CheckError(TestContext& test, const Result& result,
                asc::ErrorCode expected) {
  ASC_SPARSE_TEST_CHECK(test, !result.ok());
  if (!result.ok()) {
    ASC_SPARSE_TEST_EQ(test, result.status().code(), expected);
  }
}

template <typename Element, asc::SparseCompressedFormat Format,
          std::size_t OffsetCount, std::size_t Nnz>
void CheckCompressed(
    TestContext& test, asc::CompressedSparseView<Element, Format> view,
    const std::array<asc::nnz_t, OffsetCount>& expected_offsets,
    const std::array<asc::index_t, Nnz>& expected_indices,
    const std::array<std::remove_const_t<Element>, Nnz>& expected_values) {
  ASC_SPARSE_TEST_EQ(test, view.shape(), kShape);
  ASC_SPARSE_TEST_EQ(test, view.nnz(), static_cast<asc::nnz_t>(Nnz));
  for (std::size_t position = 0; position < OffsetCount; ++position) {
    auto observed = view.OuterOffset(static_cast<asc::extent_t>(position));
    ASC_SPARSE_TEST_CHECK(test, observed.ok());
    if (observed.ok()) {
      ASC_SPARSE_TEST_EQ(test, *observed, expected_offsets[position]);
    }
  }
  for (std::size_t position = 0; position < Nnz; ++position) {
    auto index = view.InnerIndex(static_cast<asc::nnz_t>(position));
    auto value = view.AtStored(static_cast<asc::nnz_t>(position));
    ASC_SPARSE_TEST_CHECK(test, index.ok());
    ASC_SPARSE_TEST_CHECK(test, value.ok());
    if (index.ok()) {
      ASC_SPARSE_TEST_EQ(test, *index, expected_indices[position]);
    }
    if (value.ok()) {
      ASC_SPARSE_TEST_EQ(test, **value, expected_values[position]);
    }
  }
}

void TestViewsAndMetadata(TestContext& test) {
  auto csr = asc::CsrView<const double>::Create(
      kCsrOffsets, kCsrIndices, kCsrValues, kShape, asc::MemorySpace::kHost);
  auto csc = asc::CscView<const double>::Create(
      kCscOffsets, kCscIndices, kCscValues, kShape, asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(test, csr.ok());
  ASC_SPARSE_TEST_CHECK(test, csc.ok());
  if (!csr.ok() || !csc.ok()) {
    return;
  }
  CheckCompressed(test, *csr, kCsrOffsets, kCsrIndices, kCsrValues);
  CheckCompressed(test, *csc, kCscOffsets, kCscIndices, kCscValues);
  ASC_SPARSE_TEST_EQ(test, csr->format(), asc::SparseCompressedFormat::kCsr);
  ASC_SPARSE_TEST_EQ(test, csc->format(), asc::SparseCompressedFormat::kCsc);
  ASC_SPARSE_TEST_EQ(test, csr->rows(), asc::extent_t{3});
  ASC_SPARSE_TEST_EQ(test, csr->columns(), asc::extent_t{4});

  constexpr std::array<asc::index_t, 2> kPresent{1, 2};
  constexpr std::array<asc::index_t, 2> kMissing{2, 3};
  auto csr_present = csr->Lookup(kPresent);
  auto csc_present = csc->Lookup(kPresent);
  auto missing = csr->Lookup(kMissing);
  ASC_SPARSE_TEST_CHECK(test, csr_present.ok());
  ASC_SPARSE_TEST_CHECK(test, csc_present.ok());
  ASC_SPARSE_TEST_CHECK(test, missing.ok());
  if (csr_present.ok()) {
    ASC_SPARSE_TEST_EQ(test, *csr_present, 5.0);
  }
  if (csc_present.ok()) {
    ASC_SPARSE_TEST_EQ(test, *csc_present, 5.0);
  }
  if (missing.ok()) {
    ASC_SPARSE_TEST_EQ(test, *missing, 0.0);
  }

  static_assert(asc::ReadableExpression<decltype(*csr)>);
  static_assert(asc::PlacedReadableExpression<decltype(*csr)>);
  ASC_SPARSE_TEST_EQ(test, asc::ExpressionShape(*csr), kShape);
  ASC_SPARSE_TEST_EQ(test, asc::ExpressionSparsityEffect(*csr),
                     asc::SparsityEffect::kStructurePreserving);
  ASC_SPARSE_TEST_EQ(test, asc::ExpressionOperationCategory(*csr),
                     asc::ExpressionOperation::kTerminal);
  ASC_SPARSE_TEST_EQ(test, asc::ExpressionSpace(*csr), asc::MemorySpace::kHost);
  const auto alias = asc::ExpressionAlias(*csr);
  ASC_SPARSE_TEST_EQ(test, alias.identity(), csr->values());
  ASC_SPARSE_TEST_CHECK(test, alias.has_byte_span());
  ASC_SPARSE_TEST_EQ(test, alias.data(), csr->values());
  ASC_SPARSE_TEST_EQ(test, alias.size(), kCsrValues.size() * sizeof(double));
  ASC_SPARSE_TEST_CHECK(
      test, asc::MayAlias(*csr, asc::AliasToken(csr->outer_offsets())));
  ASC_SPARSE_TEST_CHECK(
      test, asc::MayAlias(*csr, asc::AliasToken(csr->outer_offsets() + 3)));
  ASC_SPARSE_TEST_CHECK(
      test, asc::MayAlias(*csr, asc::AliasToken(csr->inner_indices())));
  ASC_SPARSE_TEST_CHECK(
      test, asc::MayAlias(*csr, asc::AliasToken(csr->inner_indices() + 4)));
  ASC_SPARSE_TEST_CHECK(test,
                        asc::MayAlias(*csr, asc::AliasToken(csr->values())));
  ASC_SPARSE_TEST_CHECK(
      test, asc::MayAlias(*csr, asc::AliasToken(csr->values() + 4)));

  std::array<double, 5> mutable_values = kCsrValues;
  auto mutable_csr =
      asc::CsrView<double>::Create(kCsrOffsets, kCsrIndices, mutable_values,
                                   kShape, asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(test, mutable_csr.ok());
  if (mutable_csr.ok()) {
    static_assert(asc::WritableExpression<decltype(*mutable_csr)>);
    ASC_SPARSE_TEST_CHECK(test, asc::WritableExpressionIsUnique(*mutable_csr));
    auto value = mutable_csr->AtStored(2);
    ASC_SPARSE_TEST_CHECK(test, value.ok());
    if (value.ok()) {
      **value = 9.0;
      ASC_SPARSE_TEST_EQ(test, mutable_values[2], 9.0);
    }
    asc::CsrView<const double> const_view(*mutable_csr);
    auto observed = const_view.AtStored(2);
    ASC_SPARSE_TEST_CHECK(test, observed.ok());
    if (observed.ok()) {
      ASC_SPARSE_TEST_EQ(test, **observed, 9.0);
    }
  }

  CheckError(test, csr->OuterOffset(-1), asc::ErrorCode::kIndex);
  CheckError(test, csr->OuterOffset(4), asc::ErrorCode::kIndex);
  CheckError(test, csr->InnerIndex(-1), asc::ErrorCode::kIndex);
  CheckError(test, csr->AtStored(5), asc::ErrorCode::kIndex);
  constexpr std::array<asc::index_t, 2> kOutside{3, 0};
  CheckError(test, csr->Lookup(kOutside), asc::ErrorCode::kIndex);
}

void TestMalformedCompressedMetadata(TestContext& test) {
  std::array<double, 5> values = kCsrValues;
  constexpr std::array<asc::nnz_t, 3> kShortOffsets{0, 2, 5};
  CheckError(test,
             asc::CsrView<double>::Create(kShortOffsets, kCsrIndices, values,
                                          kShape, asc::MemorySpace::kHost),
             asc::ErrorCode::kShape);

  constexpr std::array<asc::nnz_t, 4> kNonzeroFirst{1, 2, 4, 5};
  CheckError(test,
             asc::CsrView<double>::Create(kNonzeroFirst, kCsrIndices, values,
                                          kShape, asc::MemorySpace::kHost),
             asc::ErrorCode::kInvalidArgument);
  constexpr std::array<asc::nnz_t, 4> kDecreasing{0, 3, 2, 5};
  CheckError(test,
             asc::CsrView<double>::Create(kDecreasing, kCsrIndices, values,
                                          kShape, asc::MemorySpace::kHost),
             asc::ErrorCode::kInvalidArgument);
  constexpr std::array<asc::nnz_t, 4> kNegativeOffset{0, -1, 4, 5};
  CheckError(test,
             asc::CsrView<double>::Create(kNegativeOffset, kCsrIndices, values,
                                          kShape, asc::MemorySpace::kHost),
             asc::ErrorCode::kInvalidArgument);
  constexpr std::array<asc::nnz_t, 4> kWrongFinal{0, 2, 4, 4};
  CheckError(test,
             asc::CsrView<double>::Create(kWrongFinal, kCsrIndices, values,
                                          kShape, asc::MemorySpace::kHost),
             asc::ErrorCode::kInvalidArgument);

  constexpr std::array<asc::index_t, 5> kNegativeInner{-1, 3, 0, 2, 1};
  CheckError(test,
             asc::CsrView<double>::Create(kCsrOffsets, kNegativeInner, values,
                                          kShape, asc::MemorySpace::kHost),
             asc::ErrorCode::kIndex);
  constexpr std::array<asc::index_t, 5> kOutsideInner{1, 4, 0, 2, 1};
  CheckError(test,
             asc::CsrView<double>::Create(kCsrOffsets, kOutsideInner, values,
                                          kShape, asc::MemorySpace::kHost),
             asc::ErrorCode::kIndex);
  constexpr std::array<asc::index_t, 5> kUnsortedInner{3, 1, 0, 2, 1};
  CheckError(test,
             asc::CsrView<double>::Create(kCsrOffsets, kUnsortedInner, values,
                                          kShape, asc::MemorySpace::kHost),
             asc::ErrorCode::kInvalidArgument);
  constexpr std::array<asc::index_t, 5> kDuplicateInner{1, 1, 0, 2, 1};
  CheckError(test,
             asc::CsrView<double>::Create(kCsrOffsets, kDuplicateInner, values,
                                          kShape, asc::MemorySpace::kHost),
             asc::ErrorCode::kInvalidArgument);

  constexpr std::array<asc::nnz_t, 4> kShortCscOffsets{0, 1, 3, 5};
  CheckError(test,
             asc::CscView<double>::Create(kShortCscOffsets, kCscIndices, values,
                                          kShape, asc::MemorySpace::kHost),
             asc::ErrorCode::kShape);
  constexpr std::array<asc::index_t, 5> kCscOutsideInner{3, 0, 2, 1, 0};
  CheckError(test,
             asc::CscView<double>::Create(kCscOffsets, kCscOutsideInner, values,
                                          kShape, asc::MemorySpace::kHost),
             asc::ErrorCode::kIndex);
  constexpr std::array<asc::index_t, 5> kCscUnsortedInner{1, 2, 0, 1, 0};
  CheckError(
      test,
      asc::CscView<double>::Create(kCscOffsets, kCscUnsortedInner, values,
                                   kShape, asc::MemorySpace::kHost),
      asc::ErrorCode::kInvalidArgument);
  constexpr std::array<asc::index_t, 5> kCscDuplicateInner{1, 0, 0, 1, 0};
  CheckError(
      test,
      asc::CscView<double>::Create(kCscOffsets, kCscDuplicateInner, values,
                                   kShape, asc::MemorySpace::kHost),
      asc::ErrorCode::kInvalidArgument);

  CheckError(
      test,
      asc::CsrView<double>::Create(nullptr, kCsrIndices.data(), values.data(),
                                   kShape, 5, asc::MemorySpace::kHost),
      asc::ErrorCode::kInvalidArgument);
  CheckError(
      test,
      asc::CsrView<double>::Create(kCsrOffsets.data(), nullptr, values.data(),
                                   kShape, 5, asc::MemorySpace::kHost),
      asc::ErrorCode::kInvalidArgument);
  CheckError(
      test,
      asc::CsrView<double>::Create(kCsrOffsets.data(), kCsrIndices.data(),
                                   nullptr, kShape, 5, asc::MemorySpace::kHost),
      asc::ErrorCode::kInvalidArgument);

  constexpr std::array<asc::extent_t, 2> kNegativeShape{-1, 4};
  CheckError(test,
             asc::CsrView<double>::Create(
                 kCsrOffsets.data(), kCsrIndices.data(), values.data(),
                 kNegativeShape, 5, asc::MemorySpace::kHost),
             asc::ErrorCode::kShape);
  constexpr std::array<asc::extent_t, 2> kOverflowShape{
      std::numeric_limits<asc::extent_t>::max(), 0};
  CheckError(test,
             asc::CsrView<double>::Create(kCsrOffsets.data(), nullptr, nullptr,
                                          kOverflowShape, 0,
                                          asc::MemorySpace::kDevice),
             asc::ErrorCode::kOverflow);
  CheckError(test,
             asc::CsrView<double>::Create(kCsrOffsets.data(),
                                          kCsrIndices.data(), values.data(),
                                          kShape, -1, asc::MemorySpace::kHost),
             asc::ErrorCode::kShape);

  auto device = asc::CsrView<double>::Create(
      kCsrOffsets.data(), kCsrIndices.data(), values.data(), kShape, 5,
      asc::MemorySpace::kDevice);
  ASC_SPARSE_TEST_CHECK(test, device.ok());
  if (device.ok()) {
    CheckError(test, device->OuterOffset(0), asc::ErrorCode::kMemoryAccess);
    CheckError(test, device->InnerIndex(0), asc::ErrorCode::kMemoryAccess);
    CheckError(test, device->AtStored(0), asc::ErrorCode::kMemoryAccess);
  }
}

asc::Result<asc::CoordinateArray<double, DynamicMatrixExtents>>
MakeCoordinateOracle(asc::MemoryResource& resource) {
  auto extents = DynamicMatrixExtents::Create(3, 4);
  if (!extents.ok()) {
    return extents.status();
  }
  return asc::CoordinateArray<double, DynamicMatrixExtents>::Create(
      resource, *extents, kCoordinateIndices, kCsrValues);
}

void TestAllConversions(TestContext& test) {
  asc::HostMemoryResource source_resource;
  asc::HostMemoryResource destination_resource;
  const asc::ExecutionContext context = asc::ExecutionContext::Serial();
  auto coordinate = MakeCoordinateOracle(source_resource);
  ASC_SPARSE_TEST_CHECK(test, coordinate.ok());
  if (!coordinate.ok()) {
    return;
  }
  auto coordinate_view = coordinate->view();
  ASC_SPARSE_TEST_CHECK(test, coordinate_view.ok());
  if (!coordinate_view.ok()) {
    return;
  }

  auto csr = asc::ConvertToCsr<double>(context, *coordinate_view,
                                       destination_resource);
  auto csc = asc::ConvertToCsc<double>(context, *coordinate_view,
                                       destination_resource);
  ASC_SPARSE_TEST_CHECK(test, csr.ok());
  ASC_SPARSE_TEST_CHECK(test, csc.ok());
  if (!csr.ok() || !csc.ok()) {
    return;
  }
  auto csr_view = csr->view();
  auto csc_view = csc->view();
  ASC_SPARSE_TEST_CHECK(test, csr_view.ok());
  ASC_SPARSE_TEST_CHECK(test, csc_view.ok());
  if (!csr_view.ok() || !csc_view.ok()) {
    return;
  }
  CheckCompressed(test, *csr_view, kCsrOffsets, kCsrIndices, kCsrValues);
  CheckCompressed(test, *csc_view, kCscOffsets, kCscIndices, kCscValues);

  auto csc_from_csr =
      asc::ConvertToCsc<double>(context, *csr_view, destination_resource);
  auto csr_from_csc =
      asc::ConvertToCsr<double>(context, *csc_view, destination_resource);
  ASC_SPARSE_TEST_CHECK(test, csc_from_csr.ok());
  ASC_SPARSE_TEST_CHECK(test, csr_from_csc.ok());
  if (csc_from_csr.ok()) {
    auto view = csc_from_csr->view();
    ASC_SPARSE_TEST_CHECK(test, view.ok());
    if (view.ok()) {
      CheckCompressed(test, *view, kCscOffsets, kCscIndices, kCscValues);
    }
  }
  if (csr_from_csc.ok()) {
    auto view = csr_from_csc->view();
    ASC_SPARSE_TEST_CHECK(test, view.ok());
    if (view.ok()) {
      CheckCompressed(test, *view, kCsrOffsets, kCsrIndices, kCsrValues);
    }
  }

  auto coordinate_from_csr =
      asc::ConvertToCoordinate(context, *csr_view, destination_resource);
  auto coordinate_from_csc =
      asc::ConvertToCoordinate(context, *csc_view, destination_resource);
  ASC_SPARSE_TEST_CHECK(test, coordinate_from_csr.ok());
  ASC_SPARSE_TEST_CHECK(test, coordinate_from_csc.ok());
  for (auto* converted : {&coordinate_from_csr, &coordinate_from_csc}) {
    if (!converted->ok()) {
      continue;
    }
    auto view = (*converted)->view();
    ASC_SPARSE_TEST_CHECK(test, view.ok());
    if (!view.ok()) {
      continue;
    }
    ASC_SPARSE_TEST_EQ(test, view->shape(), kShape);
    ASC_SPARSE_TEST_EQ(test, view->nnz(), asc::nnz_t{5});
    for (asc::nnz_t position = 0; position < 5; ++position) {
      auto coordinate_at = view->Coordinate(position);
      auto value_at = view->AtStored(position);
      ASC_SPARSE_TEST_CHECK(test, coordinate_at.ok());
      ASC_SPARSE_TEST_CHECK(test, value_at.ok());
      if (coordinate_at.ok()) {
        ASC_SPARSE_TEST_EQ(
            test, (*coordinate_at)[0],
            kCoordinateIndices[static_cast<std::size_t>(2 * position)]);
        ASC_SPARSE_TEST_EQ(
            test, (*coordinate_at)[1],
            kCoordinateIndices[static_cast<std::size_t>(2 * position + 1)]);
      }
      if (value_at.ok()) {
        ASC_SPARSE_TEST_EQ(test, **value_at,
                           kCsrValues[static_cast<std::size_t>(position)]);
      }
    }
  }

  // Conversion never mutates the source.
  CheckCompressed(test, *csr_view, kCsrOffsets, kCsrIndices, kCsrValues);
  CheckCompressed(test, *csc_view, kCscOffsets, kCscIndices, kCscValues);
}

template <asc::SparseCompressedFormat Format>
void CheckEmptyOwner(TestContext& test, asc::extent_t rows,
                     asc::extent_t columns, std::size_t expected_offsets) {
  asc::HostMemoryResource resource;
  const std::array<asc::extent_t, 2> shape{rows, columns};
  std::array<asc::nnz_t, 5> zero_offsets{};
  auto owner = asc::CompressedSparseArray<double, Format>::Create(
      resource, shape,
      std::span<const asc::nnz_t>(zero_offsets.data(), expected_offsets),
      std::span<const asc::index_t>(), std::span<const double>());
  ASC_SPARSE_TEST_CHECK(test, owner.ok());
  if (owner.ok()) {
    auto view = owner->view();
    ASC_SPARSE_TEST_CHECK(test, view.ok());
    if (view.ok()) {
      ASC_SPARSE_TEST_EQ(test, view->nnz(), asc::nnz_t{0});
      for (std::size_t position = 0; position < expected_offsets; ++position) {
        auto offset = view->OuterOffset(static_cast<asc::extent_t>(position));
        ASC_SPARSE_TEST_CHECK(test, offset.ok());
        if (offset.ok()) {
          ASC_SPARSE_TEST_EQ(test, *offset, asc::nnz_t{0});
        }
      }
    }
  }
}

void TestEmptyAndExplicitZeroRoundTrips(TestContext& test) {
  CheckEmptyOwner<asc::SparseCompressedFormat::kCsr>(test, 0, 4, 1);
  CheckEmptyOwner<asc::SparseCompressedFormat::kCsc>(test, 0, 4, 5);
  CheckEmptyOwner<asc::SparseCompressedFormat::kCsr>(test, 3, 0, 4);
  CheckEmptyOwner<asc::SparseCompressedFormat::kCsc>(test, 3, 0, 1);
  CheckEmptyOwner<asc::SparseCompressedFormat::kCsr>(test, 0, 0, 1);
  CheckEmptyOwner<asc::SparseCompressedFormat::kCsc>(test, 0, 0, 1);

  asc::HostMemoryResource resource;
  const asc::ExecutionContext context = asc::ExecutionContext::Serial();
  for (const std::array<asc::extent_t, 2> shape :
       {std::array<asc::extent_t, 2>{0, 4}, std::array<asc::extent_t, 2>{3, 0},
        std::array<asc::extent_t, 2>{0, 0}}) {
    auto extents = DynamicMatrixExtents::Create(shape[0], shape[1]);
    ASC_SPARSE_TEST_CHECK(test, extents.ok());
    if (!extents.ok()) {
      continue;
    }
    auto coordinate =
        asc::CoordinateArray<double, DynamicMatrixExtents>::Create(
            resource, *extents, std::span<const asc::index_t>(),
            std::span<const double>());
    ASC_SPARSE_TEST_CHECK(test, coordinate.ok());
    if (!coordinate.ok()) {
      continue;
    }
    auto coordinate_view = coordinate->view();
    ASC_SPARSE_TEST_CHECK(test, coordinate_view.ok());
    if (!coordinate_view.ok()) {
      continue;
    }
    auto csr = asc::ConvertToCsr<double>(context, *coordinate_view, resource);
    auto csc = asc::ConvertToCsc<double>(context, *coordinate_view, resource);
    ASC_SPARSE_TEST_CHECK(test, csr.ok());
    ASC_SPARSE_TEST_CHECK(test, csc.ok());
    if (!csr.ok() || !csc.ok()) {
      continue;
    }
    auto csr_view = csr->view();
    auto csc_view = csc->view();
    ASC_SPARSE_TEST_CHECK(test, csr_view.ok());
    ASC_SPARSE_TEST_CHECK(test, csc_view.ok());
    if (!csr_view.ok() || !csc_view.ok()) {
      continue;
    }
    auto round_trip_csr =
        asc::ConvertToCoordinate(context, *csr_view, resource);
    auto round_trip_csc =
        asc::ConvertToCoordinate(context, *csc_view, resource);
    auto csc_from_csr = asc::ConvertToCsc<double>(context, *csr_view, resource);
    auto csr_from_csc = asc::ConvertToCsr<double>(context, *csc_view, resource);
    ASC_SPARSE_TEST_CHECK(test, round_trip_csr.ok());
    ASC_SPARSE_TEST_CHECK(test, round_trip_csc.ok());
    ASC_SPARSE_TEST_CHECK(test, csc_from_csr.ok());
    ASC_SPARSE_TEST_CHECK(test, csr_from_csc.ok());
    if (round_trip_csr.ok()) {
      ASC_SPARSE_TEST_EQ(test, round_trip_csr->nnz(), asc::nnz_t{0});
    }
    if (round_trip_csc.ok()) {
      ASC_SPARSE_TEST_EQ(test, round_trip_csc->nnz(), asc::nnz_t{0});
    }
  }

  constexpr std::array<asc::extent_t, 2> kSmallShape{2, 2};
  constexpr std::array<asc::nnz_t, 3> kOffsets{0, 1, 2};
  constexpr std::array<asc::index_t, 2> kIndices{0, 1};
  constexpr std::array<double, 2> kValues{0.0, 3.0};
  auto csr = asc::CsrArray<double>::Create(resource, kSmallShape, kOffsets,
                                           kIndices, kValues);
  ASC_SPARSE_TEST_CHECK(test, csr.ok());
  if (!csr.ok()) {
    return;
  }
  auto csr_view = csr->view();
  ASC_SPARSE_TEST_CHECK(test, csr_view.ok());
  if (!csr_view.ok()) {
    return;
  }
  auto coordinate = asc::ConvertToCoordinate(context, *csr_view, resource);
  ASC_SPARSE_TEST_CHECK(test, coordinate.ok());
  if (coordinate.ok()) {
    ASC_SPARSE_TEST_EQ(test, coordinate->nnz(), asc::nnz_t{2});
    auto view = coordinate->view();
    ASC_SPARSE_TEST_CHECK(test, view.ok());
    if (view.ok()) {
      auto zero = view->AtStored(0);
      ASC_SPARSE_TEST_CHECK(test, zero.ok());
      if (zero.ok()) {
        ASC_SPARSE_TEST_EQ(test, **zero, 0.0);
      }
    }
  }
}

void TestOwnerAndConversionFailureRollback(TestContext& test) {
  for (std::size_t failed_attempt = 1; failed_attempt <= 3; ++failed_attempt) {
    asc_sparse_test::TrackingMemoryResource resource;
    resource.FailOnAllocation(failed_attempt);
    auto owner = asc::CsrArray<double>::Create(resource, kShape, kCsrOffsets,
                                               kCsrIndices, kCsrValues);
    CheckError(test, owner, asc::ErrorCode::kAllocation);
    ASC_SPARSE_TEST_EQ(test, resource.live_allocations(), std::size_t{0});
    ASC_SPARSE_TEST_EQ(test, resource.successful_allocations(),
                       resource.deallocations());
  }

  asc::HostMemoryResource source_resource;
  auto source_owner = MakeCoordinateOracle(source_resource);
  ASC_SPARSE_TEST_CHECK(test, source_owner.ok());
  if (source_owner.ok()) {
    auto source = source_owner->view();
    ASC_SPARSE_TEST_CHECK(test, source.ok());
    if (source.ok()) {
      for (std::size_t failed_attempt = 1; failed_attempt <= 3;
           ++failed_attempt) {
        asc_sparse_test::TrackingMemoryResource destination;
        destination.FailOnAllocation(failed_attempt);
        auto converted = asc::ConvertToCsr<double>(
            asc::ExecutionContext::Serial(), *source, destination);
        CheckError(test, converted, asc::ErrorCode::kAllocation);
        ASC_SPARSE_TEST_EQ(test, destination.live_allocations(),
                           std::size_t{0});
        ASC_SPARSE_TEST_EQ(test, destination.successful_allocations(),
                           destination.deallocations());
      }

      asc_sparse_test::TrackingMemoryResource destination;
      {
        auto converted = asc::ConvertToCsr<double>(
            asc::ExecutionContext::Serial(), *source, destination);
        ASC_SPARSE_TEST_CHECK(test, converted.ok());
        ASC_SPARSE_TEST_EQ(test, destination.successful_allocations(),
                           std::size_t{3});
        ASC_SPARSE_TEST_EQ(test, destination.live_allocations(),
                           std::size_t{3});
      }
      ASC_SPARSE_TEST_EQ(test, destination.live_allocations(), std::size_t{0});
      ASC_SPARSE_TEST_EQ(test, destination.successful_allocations(),
                         destination.deallocations());
    }
  }

  asc_sparse_test::TrackingMemoryResource malformed_resource;
  constexpr std::array<asc::nnz_t, 4> kBadOffsets{0, 2, 4, 4};
  CheckError(
      test,
      asc::CsrArray<double>::Create(malformed_resource, kShape, kBadOffsets,
                                    kCsrIndices, kCsrValues),
      asc::ErrorCode::kInvalidArgument);
  ASC_SPARSE_TEST_EQ(test, malformed_resource.allocation_attempts(),
                     std::size_t{0});

  asc_sparse_test::TrackingMemoryResource device_resource(
      asc::MemorySpace::kDevice);
  CheckError(test,
             asc::CsrArray<double>::Create(device_resource, kShape, kCsrOffsets,
                                           kCsrIndices, kCsrValues),
             asc::ErrorCode::kUnsupported);
  ASC_SPARSE_TEST_EQ(test, device_resource.allocation_attempts(),
                     std::size_t{0});

  std::array<double, 5> values = kCsrValues;
  auto device_source = asc::CsrView<double>::Create(
      kCsrOffsets.data(), kCsrIndices.data(), values.data(), kShape, 5,
      asc::MemorySpace::kDevice);
  ASC_SPARSE_TEST_CHECK(test, device_source.ok());
  if (device_source.ok()) {
    asc_sparse_test::TrackingMemoryResource destination;
    auto converted = asc::ConvertToCsc<double>(asc::ExecutionContext::Serial(),
                                               *device_source, destination);
    CheckError(test, converted, asc::ErrorCode::kMemoryAccess);
    ASC_SPARSE_TEST_EQ(test, destination.allocation_attempts(), std::size_t{0});
  }
}

}  // namespace

int main() {
  TestContext test;
  TestViewsAndMetadata(test);
  TestMalformedCompressedMetadata(test);
  TestAllConversions(test);
  TestEmptyAndExplicitZeroRoundTrips(test);
  TestOwnerAndConversionFailureRollback(test);
  return test.Finish();
}
