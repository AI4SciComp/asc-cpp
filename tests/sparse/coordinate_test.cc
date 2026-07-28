#include "asc/sparse/coordinate.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
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
#include "test_resources.h"
#include "test_support.h"

namespace {

using asc_sparse_test::TestContext;
using Shape0 = asc::Extents<>;
using Shape2 = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;
using Shape3 =
    asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent, asc::kDynamicExtent>;

static_assert(asc::SparseElement<int>);
static_assert(asc::SparseElement<float>);
static_assert(asc::SparseElement<double>);
static_assert(!asc::SparseElement<bool>);
static_assert(
    !std::is_copy_constructible_v<asc::CoordinateBuilder<double, Shape2>>);
static_assert(
    !std::is_copy_assignable_v<asc::CoordinateBuilder<double, Shape2>>);
static_assert(
    !std::is_copy_constructible_v<asc::CoordinateArray<double, Shape2>>);
static_assert(!std::is_copy_assignable_v<asc::CoordinateArray<double, Shape2>>);
static_assert(
    std::is_move_constructible_v<asc::CoordinateBuilder<double, Shape2>>);
static_assert(
    std::is_move_constructible_v<asc::CoordinateArray<double, Shape2>>);
static_assert(std::is_trivially_copyable_v<asc::CoordinateView<double, 2>>);
static_assert(std::is_constructible_v<asc::CoordinateView<const double, 2>,
                                      asc::CoordinateView<double, 2>>);
static_assert(!std::is_constructible_v<asc::CoordinateView<double, 2>,
                                       asc::CoordinateView<const double, 2>>);

template <typename Result>
void CheckError(TestContext& test, const Result& result,
                asc::ErrorCode expected) {
  ASC_SPARSE_TEST_CHECK(test, !result.ok());
  if (!result.ok()) {
    ASC_SPARSE_TEST_EQ(test, result.status().code(), expected);
  }
}

void CheckStatusError(TestContext& test, const asc::Status& status,
                      asc::ErrorCode expected) {
  ASC_SPARSE_TEST_CHECK(test, !status.ok());
  if (!status.ok()) {
    ASC_SPARSE_TEST_EQ(test, status.code(), expected);
  }
}

template <std::size_t Rank>
std::span<const asc::index_t, Rank> Indices(
    const std::array<asc::index_t, Rank>& values) {
  return std::span<const asc::index_t, Rank>(values);
}

void TestCanonicalFinalizationAndView(TestContext& test) {
  asc_sparse_test::TrackingMemoryResource resource;
  auto extents = Shape2::Create(3, 4);
  ASC_SPARSE_TEST_CHECK(test, extents.ok());
  if (!extents.ok()) {
    return;
  }
  auto builder =
      asc::CoordinateBuilder<double, Shape2>::Create(resource, *extents, 5);
  ASC_SPARSE_TEST_CHECK(test, builder.ok());
  ASC_SPARSE_TEST_EQ(test, resource.successful_allocations(), std::size_t{2});
  if (!builder.ok()) {
    return;
  }
  constexpr std::array<asc::index_t, 2> kNegativeAdd{-1, 0};
  constexpr std::array<asc::index_t, 2> kOutsideAdd{3, 0};
  CheckStatusError(test, builder->Add(Indices(kNegativeAdd), 1.0),
                   asc::ErrorCode::kIndex);
  CheckStatusError(test, builder->Add(Indices(kOutsideAdd), 1.0),
                   asc::ErrorCode::kIndex);
  ASC_SPARSE_TEST_EQ(test, builder->size(), asc::nnz_t{0});

  constexpr std::array<std::array<asc::index_t, 2>, 5> kCoordinates{{
      {2, 1},
      {0, 3},
      {2, 1},
      {1, 0},
      {0, 1},
  }};
  constexpr std::array<double, 5> kValues{5.0, 2.0, -1.0, 0.0, 7.0};
  for (std::size_t position = 0; position < kCoordinates.size(); ++position) {
    ASC_SPARSE_TEST_CHECK(
        test,
        builder->Add(Indices(kCoordinates[position]), kValues[position]).ok());
  }
  ASC_SPARSE_TEST_EQ(test, builder->size(), asc::nnz_t{5});

  constexpr std::array<asc::index_t, 2> kExtra{0, 0};
  CheckStatusError(test, builder->Add(Indices(kExtra), 99.0),
                   asc::ErrorCode::kInvalidState);
  ASC_SPARSE_TEST_EQ(test, builder->size(), asc::nnz_t{5});

  auto finalized = std::move(*builder).Finalize(asc::ExecutionContext::Serial(),
                                                asc::DuplicatePolicy::kSum,
                                                asc::ExplicitZeroPolicy::kKeep);
  ASC_SPARSE_TEST_CHECK(test, finalized.ok());
  if (!finalized.ok()) {
    return;
  }
  ASC_SPARSE_TEST_CHECK(test, finalized->valid());
  ASC_SPARSE_TEST_CHECK(test, !builder->valid());
  ASC_SPARSE_TEST_EQ(test, finalized->nnz(), asc::nnz_t{4});

  auto view = finalized->view();
  ASC_SPARSE_TEST_CHECK(test, view.ok());
  if (!view.ok()) {
    return;
  }
  ASC_SPARSE_TEST_EQ(test, view->rank(), asc::rank_t{2});
  ASC_SPARSE_TEST_EQ(test, view->shape(), (std::array<asc::extent_t, 2>{3, 4}));
  ASC_SPARSE_TEST_EQ(test, view->memory_space(), asc::MemorySpace::kHost);

  constexpr std::array<std::array<asc::index_t, 2>, 4> kExpectedCoordinates{{
      {0, 1},
      {0, 3},
      {1, 0},
      {2, 1},
  }};
  constexpr std::array<double, 4> kExpectedValues{7.0, 2.0, 0.0, 4.0};
  for (asc::nnz_t position = 0; position < view->nnz(); ++position) {
    auto coordinate = view->Coordinate(position);
    auto value = view->AtStored(position);
    ASC_SPARSE_TEST_CHECK(test, coordinate.ok());
    ASC_SPARSE_TEST_CHECK(test, value.ok());
    if (coordinate.ok() && value.ok()) {
      ASC_SPARSE_TEST_EQ(
          test, (*coordinate)[0],
          kExpectedCoordinates[static_cast<std::size_t>(position)][0]);
      ASC_SPARSE_TEST_EQ(
          test, (*coordinate)[1],
          kExpectedCoordinates[static_cast<std::size_t>(position)][1]);
      ASC_SPARSE_TEST_EQ(test, **value,
                         kExpectedValues[static_cast<std::size_t>(position)]);
    }
  }

  constexpr std::array<asc::index_t, 2> kPresent{2, 1};
  constexpr std::array<asc::index_t, 2> kMissing{2, 2};
  auto present = view->Lookup(Indices(kPresent));
  auto missing = view->Lookup(Indices(kMissing));
  ASC_SPARSE_TEST_CHECK(test, present.ok());
  ASC_SPARSE_TEST_CHECK(test, missing.ok());
  if (present.ok()) {
    ASC_SPARSE_TEST_EQ(test, *present, 4.0);
  }
  if (missing.ok()) {
    ASC_SPARSE_TEST_EQ(test, *missing, 0.0);
  }

  auto mutable_value = view->AtStored(1);
  ASC_SPARSE_TEST_CHECK(test, mutable_value.ok());
  if (mutable_value.ok()) {
    **mutable_value = -8.0;
  }
  const auto& const_owner = *finalized;
  auto const_view = const_owner.view();
  ASC_SPARSE_TEST_CHECK(test, const_view.ok());
  if (const_view.ok()) {
    auto observed = const_view->AtStored(1);
    ASC_SPARSE_TEST_CHECK(test, observed.ok());
    if (observed.ok()) {
      ASC_SPARSE_TEST_EQ(test, **observed, -8.0);
    }
  }

  static_assert(asc::ReadableExpression<decltype(*view)>);
  static_assert(asc::PlacedReadableExpression<decltype(*view)>);
  static_assert(asc::WritableExpression<decltype(*view)>);
  ASC_SPARSE_TEST_EQ(test, asc::ExpressionShape(*view), view->shape());
  ASC_SPARSE_TEST_EQ(test, asc::ExpressionSpace(*view),
                     asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_EQ(test, asc::ExpressionSparsityEffect(*view),
                     asc::SparsityEffect::kStructurePreserving);
  ASC_SPARSE_TEST_EQ(test, asc::ExpressionOperationCategory(*view),
                     asc::ExpressionOperation::kTerminal);
  ASC_SPARSE_TEST_CHECK(test, asc::WritableExpressionIsUnique(*view));
  const auto alias = asc::ExpressionAlias(*view);
  ASC_SPARSE_TEST_EQ(test, alias.identity(), view->values());
  ASC_SPARSE_TEST_CHECK(test, alias.has_byte_span());
  ASC_SPARSE_TEST_EQ(test, alias.data(), view->values());
  ASC_SPARSE_TEST_EQ(test, alias.size(), std::size_t{4} * sizeof(double));
  ASC_SPARSE_TEST_CHECK(
      test, asc::MayAlias(*view, asc::AliasToken(view->coordinates())));
  ASC_SPARSE_TEST_CHECK(
      test, asc::MayAlias(*view, asc::AliasToken(view->coordinates() + 7)));
  ASC_SPARSE_TEST_CHECK(test,
                        asc::MayAlias(*view, asc::AliasToken(view->values())));
  ASC_SPARSE_TEST_CHECK(
      test, asc::MayAlias(*view, asc::AliasToken(view->values() + 3)));

  CheckError(test, view->Coordinate(-1), asc::ErrorCode::kIndex);
  CheckError(test, view->Coordinate(view->nnz()), asc::ErrorCode::kIndex);
  CheckError(test, view->AtStored(-1), asc::ErrorCode::kIndex);
  constexpr std::array<asc::index_t, 2> kNegative{-1, 0};
  constexpr std::array<asc::index_t, 2> kOutside{3, 0};
  CheckError(test, view->Lookup(Indices(kNegative)), asc::ErrorCode::kIndex);
  CheckError(test, view->Lookup(Indices(kOutside)), asc::ErrorCode::kIndex);
}

void TestFinalizationRollbackAndPolicies(TestContext& test) {
  auto extents = Shape2::Create(3, 4);
  ASC_SPARSE_TEST_CHECK(test, extents.ok());
  if (!extents.ok()) {
    return;
  }
  asc::HostMemoryResource resource;
  auto builder =
      asc::CoordinateBuilder<double, Shape2>::Create(resource, *extents, 5);
  ASC_SPARSE_TEST_CHECK(test, builder.ok());
  if (!builder.ok()) {
    return;
  }
  constexpr std::array<std::array<asc::index_t, 2>, 5> kCoordinates{{
      {2, 1},
      {0, 3},
      {2, 1},
      {1, 0},
      {0, 1},
  }};
  constexpr std::array<double, 5> kValues{5.0, 2.0, -1.0, 0.0, 7.0};
  for (std::size_t position = 0; position < kCoordinates.size(); ++position) {
    ASC_SPARSE_TEST_CHECK(
        test,
        builder->Add(Indices(kCoordinates[position]), kValues[position]).ok());
  }

  auto rejected = std::move(*builder).Finalize(asc::ExecutionContext::Serial(),
                                               asc::DuplicatePolicy::kReject,
                                               asc::ExplicitZeroPolicy::kKeep);
  CheckError(test, rejected, asc::ErrorCode::kInvalidArgument);
  ASC_SPARSE_TEST_CHECK(test, builder->valid());
  ASC_SPARSE_TEST_EQ(test, builder->size(), asc::nnz_t{5});

  auto dropped = std::move(*builder).Finalize(asc::ExecutionContext::Serial(),
                                              asc::DuplicatePolicy::kSum,
                                              asc::ExplicitZeroPolicy::kDrop);
  ASC_SPARSE_TEST_CHECK(test, dropped.ok());
  if (dropped.ok()) {
    ASC_SPARSE_TEST_EQ(test, dropped->nnz(), asc::nnz_t{3});
    auto view = dropped->view();
    ASC_SPARSE_TEST_CHECK(test, view.ok());
    if (view.ok()) {
      constexpr std::array<asc::index_t, 2> kZeroCoordinate{1, 0};
      auto zero = view->Lookup(Indices(kZeroCoordinate));
      ASC_SPARSE_TEST_CHECK(test, zero.ok());
      if (zero.ok()) {
        ASC_SPARSE_TEST_EQ(test, *zero, 0.0);
      }
    }
  }

  auto integral_builder = asc::CoordinateBuilder<std::int64_t, Shape2>::Create(
      resource, *extents, 2);
  ASC_SPARSE_TEST_CHECK(test, integral_builder.ok());
  if (integral_builder.ok()) {
    constexpr std::array<asc::index_t, 2> kCoordinate{0, 0};
    ASC_SPARSE_TEST_CHECK(test,
                          integral_builder
                              ->Add(Indices(kCoordinate),
                                    std::numeric_limits<std::int64_t>::max())
                              .ok());
    ASC_SPARSE_TEST_CHECK(test,
                          integral_builder->Add(Indices(kCoordinate), 1).ok());
    auto overflow = std::move(*integral_builder)
                        .Finalize(asc::ExecutionContext::Serial(),
                                  asc::DuplicatePolicy::kSum,
                                  asc::ExplicitZeroPolicy::kKeep);
    CheckError(test, overflow, asc::ErrorCode::kOverflow);
    ASC_SPARSE_TEST_CHECK(test, integral_builder->valid());
    ASC_SPARSE_TEST_EQ(test, integral_builder->size(), asc::nnz_t{2});
  }

  auto invalid_duplicate =
      asc::CoordinateBuilder<double, Shape2>::Create(resource, *extents, 1);
  ASC_SPARSE_TEST_CHECK(test, invalid_duplicate.ok());
  if (invalid_duplicate.ok()) {
    constexpr std::array<asc::index_t, 2> kCoordinate{0, 0};
    ASC_SPARSE_TEST_CHECK(
        test, invalid_duplicate->Add(Indices(kCoordinate), 1.0).ok());
    auto invalid = std::move(*invalid_duplicate)
                       .Finalize(asc::ExecutionContext::Serial(),
                                 static_cast<asc::DuplicatePolicy>(255),
                                 asc::ExplicitZeroPolicy::kKeep);
    CheckError(test, invalid, asc::ErrorCode::kInvalidArgument);
    ASC_SPARSE_TEST_CHECK(test, invalid_duplicate->valid());
    ASC_SPARSE_TEST_EQ(test, invalid_duplicate->size(), asc::nnz_t{1});
  }

  auto invalid_zero =
      asc::CoordinateBuilder<double, Shape2>::Create(resource, *extents, 1);
  ASC_SPARSE_TEST_CHECK(test, invalid_zero.ok());
  if (invalid_zero.ok()) {
    constexpr std::array<asc::index_t, 2> kCoordinate{0, 0};
    ASC_SPARSE_TEST_CHECK(test,
                          invalid_zero->Add(Indices(kCoordinate), 1.0).ok());
    auto invalid = std::move(*invalid_zero)
                       .Finalize(asc::ExecutionContext::Serial(),
                                 asc::DuplicatePolicy::kSum,
                                 static_cast<asc::ExplicitZeroPolicy>(255));
    CheckError(test, invalid, asc::ErrorCode::kInvalidArgument);
    ASC_SPARSE_TEST_CHECK(test, invalid_zero->valid());
    ASC_SPARSE_TEST_EQ(test, invalid_zero->size(), asc::nnz_t{1});
  }

  auto special =
      asc::CoordinateBuilder<double, Shape2>::Create(resource, *extents, 2);
  ASC_SPARSE_TEST_CHECK(test, special.ok());
  if (special.ok()) {
    constexpr std::array<asc::index_t, 2> kZero{0, 0};
    constexpr std::array<asc::index_t, 2> kNan{0, 2};
    ASC_SPARSE_TEST_CHECK(test, special->Add(Indices(kZero), 0.0).ok());
    ASC_SPARSE_TEST_CHECK(
        test,
        special->Add(Indices(kNan), std::numeric_limits<double>::quiet_NaN())
            .ok());
    auto finalized = std::move(*special).Finalize(
        asc::ExecutionContext::Serial(), asc::DuplicatePolicy::kReject,
        asc::ExplicitZeroPolicy::kDrop);
    ASC_SPARSE_TEST_CHECK(test, finalized.ok());
    if (finalized.ok()) {
      ASC_SPARSE_TEST_EQ(test, finalized->nnz(), asc::nnz_t{1});
      auto view = finalized->view();
      ASC_SPARSE_TEST_CHECK(test, view.ok());
      if (view.ok()) {
        auto value = view->AtStored(0);
        ASC_SPARSE_TEST_CHECK(test, value.ok());
        if (value.ok()) {
          ASC_SPARSE_TEST_CHECK(test, std::isnan(**value));
        }
      }
    }
  }
}

void TestRanksAndBoundaryShapes(TestContext& test) {
  asc::HostMemoryResource resource;

  auto scalar_extents = Shape0::Create();
  ASC_SPARSE_TEST_CHECK(test, scalar_extents.ok());
  if (scalar_extents.ok()) {
    auto scalar = asc::CoordinateBuilder<double, Shape0>::Create(
        resource, *scalar_extents, 2);
    ASC_SPARSE_TEST_CHECK(test, scalar.ok());
    if (scalar.ok()) {
      const std::array<asc::index_t, 0> coordinate{};
      ASC_SPARSE_TEST_CHECK(test, scalar->Add(Indices(coordinate), 2.5).ok());
      ASC_SPARSE_TEST_CHECK(test, scalar->Add(Indices(coordinate), 1.5).ok());
      auto finalized = std::move(*scalar).Finalize(
          asc::ExecutionContext::Serial(), asc::DuplicatePolicy::kSum,
          asc::ExplicitZeroPolicy::kKeep);
      ASC_SPARSE_TEST_CHECK(test, finalized.ok());
      if (finalized.ok()) {
        ASC_SPARSE_TEST_EQ(test, finalized->nnz(), asc::nnz_t{1});
        auto view = finalized->view();
        ASC_SPARSE_TEST_CHECK(test, view.ok());
        if (view.ok()) {
          auto stored_coordinate = view->Coordinate(0);
          auto lookup = view->Lookup(Indices(coordinate));
          ASC_SPARSE_TEST_CHECK(test, stored_coordinate.ok());
          ASC_SPARSE_TEST_CHECK(test, lookup.ok());
          if (stored_coordinate.ok()) {
            ASC_SPARSE_TEST_EQ(test, stored_coordinate->size(), std::size_t{0});
          }
          if (lookup.ok()) {
            ASC_SPARSE_TEST_EQ(test, *lookup, 4.0);
          }
        }
      }
    }
  }

  auto empty_extents = Shape2::Create(0, 4);
  ASC_SPARSE_TEST_CHECK(test, empty_extents.ok());
  if (empty_extents.ok()) {
    CheckError(test,
               asc::CoordinateBuilder<double, Shape2>::Create(
                   resource, *empty_extents, 1),
               asc::ErrorCode::kShape);
    auto empty = asc::CoordinateBuilder<double, Shape2>::Create(
        resource, *empty_extents, 0);
    ASC_SPARSE_TEST_CHECK(test, empty.ok());
    if (empty.ok()) {
      constexpr std::array<asc::index_t, 2> kCoordinate{0, 0};
      CheckStatusError(test, empty->Add(Indices(kCoordinate), 1.0),
                       asc::ErrorCode::kIndex);
      auto finalized = std::move(*empty).Finalize(
          asc::ExecutionContext::Serial(), asc::DuplicatePolicy::kReject,
          asc::ExplicitZeroPolicy::kKeep);
      ASC_SPARSE_TEST_CHECK(test, finalized.ok());
      if (finalized.ok()) {
        ASC_SPARSE_TEST_EQ(test, finalized->nnz(), asc::nnz_t{0});
      }
    }
  }

  auto rank3_extents = Shape3::Create(2, 3, 2);
  ASC_SPARSE_TEST_CHECK(test, rank3_extents.ok());
  if (rank3_extents.ok()) {
    auto rank3 = asc::CoordinateBuilder<int, Shape3>::Create(resource,
                                                             *rank3_extents, 4);
    ASC_SPARSE_TEST_CHECK(test, rank3.ok());
    if (rank3.ok()) {
      constexpr std::array<std::array<asc::index_t, 3>, 4> kInserted{{
          {1, 2, 0},
          {0, 2, 1},
          {1, 0, 1},
          {0, 0, 0},
      }};
      for (std::size_t position = 0; position < kInserted.size(); ++position) {
        ASC_SPARSE_TEST_CHECK(test, rank3
                                        ->Add(Indices(kInserted[position]),
                                              static_cast<int>(position + 1))
                                        .ok());
      }
      auto finalized = std::move(*rank3).Finalize(
          asc::ExecutionContext::Serial(), asc::DuplicatePolicy::kReject,
          asc::ExplicitZeroPolicy::kKeep);
      ASC_SPARSE_TEST_CHECK(test, finalized.ok());
      if (finalized.ok()) {
        auto view = finalized->view();
        ASC_SPARSE_TEST_CHECK(test, view.ok());
        constexpr std::array<std::array<asc::index_t, 3>, 4> kExpected{{
            {0, 0, 0},
            {0, 2, 1},
            {1, 0, 1},
            {1, 2, 0},
        }};
        if (view.ok()) {
          for (asc::nnz_t position = 0; position < 4; ++position) {
            auto coordinate = view->Coordinate(position);
            ASC_SPARSE_TEST_CHECK(test, coordinate.ok());
            if (coordinate.ok()) {
              for (std::size_t dimension = 0; dimension < 3; ++dimension) {
                ASC_SPARSE_TEST_EQ(
                    test, (*coordinate)[dimension],
                    kExpected[static_cast<std::size_t>(position)][dimension]);
              }
            }
          }
        }
      }
    }
  }

  auto ordinary_extents = Shape2::Create(1, 1);
  ASC_SPARSE_TEST_CHECK(test, ordinary_extents.ok());
  if (ordinary_extents.ok()) {
    CheckError(test,
               asc::CoordinateBuilder<double, Shape2>::Create(
                   resource, *ordinary_extents, -1),
               asc::ErrorCode::kInvalidArgument);
    CheckError(test,
               asc::CoordinateBuilder<double, Shape2>::Create(
                   resource, *ordinary_extents,
                   std::numeric_limits<asc::nnz_t>::max()),
               asc::ErrorCode::kOverflow);
  }
}

void TestExternalViewValidation(TestContext& test) {
  constexpr std::array<asc::extent_t, 2> kShape{3, 4};
  std::array<asc::index_t, 4> canonical_coordinates{0, 1, 2, 3};
  std::array<double, 2> values{2.0, 5.0};
  auto canonical = asc::CoordinateView<double, 2>::Create(
      canonical_coordinates.data(), values.data(), kShape, 2,
      asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(test, canonical.ok());

  auto device = asc::CoordinateView<double, 2>::Create(
      canonical_coordinates.data(), values.data(), kShape, 2,
      asc::MemorySpace::kDevice);
  ASC_SPARSE_TEST_CHECK(test, device.ok());
  if (device.ok()) {
    CheckError(test, device->AtStored(0), asc::ErrorCode::kMemoryAccess);
    constexpr std::array<asc::index_t, 2> kCoordinate{0, 1};
    CheckError(test, device->Lookup(Indices(kCoordinate)),
               asc::ErrorCode::kMemoryAccess);
    CheckStatusError(
        test,
        asc::ValidateExpressionAccess(asc::ExecutionContext::Serial(), *device),
        asc::ErrorCode::kMemoryAccess);
  }

  std::array<asc::index_t, 4> unsorted{2, 3, 0, 1};
  CheckError(
      test,
      asc::CoordinateView<double, 2>::Create(
          unsorted.data(), values.data(), kShape, 2, asc::MemorySpace::kHost),
      asc::ErrorCode::kInvalidArgument);
  std::array<asc::index_t, 4> duplicate{0, 1, 0, 1};
  CheckError(
      test,
      asc::CoordinateView<double, 2>::Create(
          duplicate.data(), values.data(), kShape, 2, asc::MemorySpace::kHost),
      asc::ErrorCode::kInvalidArgument);
  std::array<asc::index_t, 4> out_of_range{0, 1, 3, 0};
  CheckError(test,
             asc::CoordinateView<double, 2>::Create(out_of_range.data(),
                                                    values.data(), kShape, 2,
                                                    asc::MemorySpace::kHost),
             asc::ErrorCode::kIndex);
  CheckError(test,
             asc::CoordinateView<double, 2>::Create(
                 nullptr, values.data(), kShape, 2, asc::MemorySpace::kHost),
             asc::ErrorCode::kInvalidArgument);
  CheckError(test,
             asc::CoordinateView<double, 2>::Create(
                 canonical_coordinates.data(), nullptr, kShape, 2,
                 asc::MemorySpace::kHost),
             asc::ErrorCode::kInvalidArgument);
  CheckError(test,
             asc::CoordinateView<double, 2>::Create(
                 canonical_coordinates.data(), values.data(), kShape, -1,
                 asc::MemorySpace::kHost),
             asc::ErrorCode::kShape);
  constexpr std::array<asc::extent_t, 2> kZeroShape{0, 4};
  CheckError(test,
             asc::CoordinateView<double, 2>::Create(
                 canonical_coordinates.data(), values.data(), kZeroShape, 1,
                 asc::MemorySpace::kHost),
             asc::ErrorCode::kShape);
  constexpr std::array<asc::extent_t, 2> kLargeShape{
      std::numeric_limits<asc::extent_t>::max(), 1};
  CheckError(
      test,
      asc::CoordinateView<double, 2>::Create(
          canonical_coordinates.data(), values.data(), kLargeShape,
          std::numeric_limits<asc::nnz_t>::max(), asc::MemorySpace::kDevice),
      asc::ErrorCode::kOverflow);
}

void TestOwnerConstructionRollbackAndMoves(TestContext& test) {
  auto extents = Shape2::Create(2, 2);
  ASC_SPARSE_TEST_CHECK(test, extents.ok());
  if (!extents.ok()) {
    return;
  }
  constexpr std::array<asc::index_t, 4> kCoordinates{0, 0, 1, 1};
  constexpr std::array<double, 2> kValues{3.0, 4.0};

  {
    asc_sparse_test::TrackingMemoryResource resource;
    resource.FailOnAllocation(2);
    auto failed = asc::CoordinateArray<double, Shape2>::Create(
        resource, *extents, kCoordinates, kValues);
    CheckError(test, failed, asc::ErrorCode::kAllocation);
    ASC_SPARSE_TEST_EQ(test, resource.successful_allocations(), std::size_t{1});
    ASC_SPARSE_TEST_EQ(test, resource.deallocations(), std::size_t{1});
    ASC_SPARSE_TEST_EQ(test, resource.live_allocations(), std::size_t{0});
  }

  {
    asc_sparse_test::TrackingMemoryResource resource;
    auto owner = asc::CoordinateArray<double, Shape2>::Create(
        resource, *extents, kCoordinates, kValues);
    ASC_SPARSE_TEST_CHECK(test, owner.ok());
    if (owner.ok()) {
      auto moved = std::move(*owner);
      ASC_SPARSE_TEST_CHECK(test, moved.valid());
      ASC_SPARSE_TEST_CHECK(test, !owner->valid());
      CheckError(test, owner->view(), asc::ErrorCode::kInvalidState);
      auto view = moved.view();
      ASC_SPARSE_TEST_CHECK(test, view.ok());
    }
    ASC_SPARSE_TEST_EQ(test, resource.live_allocations(), std::size_t{0});
    ASC_SPARSE_TEST_EQ(test, resource.successful_allocations(),
                       resource.deallocations());
  }

  asc_sparse_test::TrackingMemoryResource device_resource(
      asc::MemorySpace::kDevice);
  CheckError(test,
             asc::CoordinateArray<double, Shape2>::Create(
                 device_resource, *extents, kCoordinates, kValues),
             asc::ErrorCode::kUnsupported);
  ASC_SPARSE_TEST_EQ(test, device_resource.allocation_attempts(),
                     std::size_t{0});

  constexpr std::array<asc::index_t, 4> kUnsorted{1, 1, 0, 0};
  asc_sparse_test::TrackingMemoryResource invalid_resource;
  CheckError(test,
             asc::CoordinateArray<double, Shape2>::Create(
                 invalid_resource, *extents, kUnsorted, kValues),
             asc::ErrorCode::kInvalidArgument);
  ASC_SPARSE_TEST_EQ(test, invalid_resource.live_allocations(), std::size_t{0});
  ASC_SPARSE_TEST_EQ(test, invalid_resource.successful_allocations(),
                     invalid_resource.deallocations());
}

}  // namespace

int main() {
  TestContext test;
  TestCanonicalFinalizationAndView(test);
  TestFinalizationRollbackAndPolicies(test);
  TestRanksAndBoundaryShapes(test);
  TestExternalViewValidation(test);
  TestOwnerConstructionRollbackAndMoves(test);
  return test.Finish();
}
