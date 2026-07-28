#include "asc/sparse/evaluate.h"

#include <array>
#include <cstddef>
#include <span>
#include <type_traits>

#include "allocation_probe.h"
#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/expression/expression.h"
#include "asc/sparse/compressed.h"
#include "asc/sparse/coordinate.h"
#include "test_expression.h"
#include "test_support.h"

namespace {

using asc_sparse_test::TestContext;
using MatrixExtents = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;

constexpr std::array<asc::extent_t, 2> kShape{3, 4};
constexpr std::array<asc::index_t, 10> kCoordinates{0, 1, 0, 3, 1,
                                                    0, 1, 2, 2, 1};
constexpr std::array<asc::nnz_t, 4> kCsrOffsets{0, 2, 4, 5};
constexpr std::array<asc::index_t, 5> kCsrIndices{1, 3, 0, 2, 1};
constexpr std::array<double, 5> kSourceValues{2.0, -1.0, 4.0, 5.0, 3.0};
constexpr std::array<double, 5> kInitialValues{11.0, 12.0, 13.0, 14.0, 15.0};

template <typename Result>
void CheckError(TestContext& test, const Result& result,
                asc::ErrorCode expected) {
  ASC_SPARSE_TEST_CHECK(test, !result.ok());
  if (!result.ok()) {
    ASC_SPARSE_TEST_EQ(test, result.status().code(), expected);
  }
}

void CheckError(TestContext& test, const asc::Status& status,
                asc::ErrorCode expected) {
  ASC_SPARSE_TEST_CHECK(test, !status.ok());
  if (!status.ok()) {
    ASC_SPARSE_TEST_EQ(test, status.code(), expected);
  }
}

asc::Result<asc::CoordinateArray<double, MatrixExtents>> MakeCoordinate(
    asc::MemoryResource& resource, std::span<const double> values) {
  auto extents = MatrixExtents::Create(3, 4);
  if (!extents.ok()) {
    return extents.status();
  }
  return asc::CoordinateArray<double, MatrixExtents>::Create(
      resource, *extents, kCoordinates, values);
}

template <typename View>
void CheckValues(TestContext& test, View view,
                 std::span<const double> expected) {
  ASC_SPARSE_TEST_EQ(test, view.nnz(),
                     static_cast<asc::nnz_t>(expected.size()));
  for (asc::nnz_t position = 0; position < view.nnz(); ++position) {
    auto value = view.AtStored(position);
    ASC_SPARSE_TEST_CHECK(test, value.ok());
    if (value.ok()) {
      ASC_SPARSE_TEST_EQ(test, **value,
                         expected[static_cast<std::size_t>(position)]);
    }
  }
}

void TestCoordinateAndCompressedEvaluation(TestContext& test) {
  asc::HostMemoryResource source_resource;
  asc::HostMemoryResource coordinate_resource;
  asc::HostMemoryResource compressed_resource;
  auto source = MakeCoordinate(source_resource, kSourceValues);
  auto coordinate_destination =
      MakeCoordinate(coordinate_resource, kInitialValues);
  auto compressed_destination = asc::CsrArray<double>::Create(
      compressed_resource, kShape, kCsrOffsets, kCsrIndices, kInitialValues);
  ASC_SPARSE_TEST_CHECK(test, source.ok());
  ASC_SPARSE_TEST_CHECK(test, coordinate_destination.ok());
  ASC_SPARSE_TEST_CHECK(test, compressed_destination.ok());
  if (!source.ok() || !coordinate_destination.ok() ||
      !compressed_destination.ok()) {
    return;
  }
  auto source_view = source->view();
  auto coordinate_view = coordinate_destination->view();
  auto compressed_view = compressed_destination->view();
  ASC_SPARSE_TEST_CHECK(test, source_view.ok());
  ASC_SPARSE_TEST_CHECK(test, coordinate_view.ok());
  ASC_SPARSE_TEST_CHECK(test, compressed_view.ok());
  if (!source_view.ok() || !coordinate_view.ok() || !compressed_view.ok()) {
    return;
  }

  auto negated = asc::MakeNegate(*source_view);
  static_assert(asc::ReadableExpression<decltype(negated)>);
  const asc::ExecutionContext context = asc::ExecutionContext::Serial();
  ASC_SPARSE_TEST_CHECK(test,
                        asc::Evaluate(context, negated, *coordinate_view).ok());
  constexpr std::array<double, 5> kNegated{-2.0, 1.0, -4.0, -5.0, -3.0};
  CheckValues(test, *coordinate_view, kNegated);

  ASC_SPARSE_TEST_CHECK(test,
                        asc::Evaluate(context, negated, *compressed_view).ok());
  CheckValues(test, *compressed_view, kNegated);
  for (std::size_t position = 0; position < kCsrOffsets.size(); ++position) {
    auto offset =
        compressed_view->OuterOffset(static_cast<asc::extent_t>(position));
    ASC_SPARSE_TEST_CHECK(test, offset.ok());
    if (offset.ok()) {
      ASC_SPARSE_TEST_EQ(test, *offset, kCsrOffsets[position]);
    }
  }
  for (std::size_t position = 0; position < kCsrIndices.size(); ++position) {
    auto index = compressed_view->InnerIndex(static_cast<asc::nnz_t>(position));
    ASC_SPARSE_TEST_CHECK(test, index.ok());
    if (index.ok()) {
      ASC_SPARSE_TEST_EQ(test, *index, kCsrIndices[position]);
    }
  }

  // A truthful structure-preserving external expression is sampled only at
  // the destination's canonical stored coordinates.
  constexpr std::array<double, 12> kLogicalValues{
      0.0, 21.0, 0.0, 23.0, 30.0, 0.0, 32.0, 0.0, 0.0, 41.0, 0.0, 0.0};
  const M4TestExpression<double, 2, asc::SparsityEffect::kStructurePreserving>
      external{
          .values = kLogicalValues.data(),
          .shape = kShape,
          .memory_space = asc::MemorySpace::kHost,
          .alias_bytes = kLogicalValues.size() * sizeof(double),
      };
  ASC_SPARSE_TEST_CHECK(
      test, asc::Evaluate(context, external, *compressed_view).ok());
  constexpr std::array<double, 5> kExternalExpected{21.0, 23.0, 30.0, 32.0,
                                                    41.0};
  CheckValues(test, *compressed_view, kExternalExpected);

  // Direct descriptor-identical assignment is the sole accepted overlap.
  const auto before_self = kExternalExpected;
  ASC_SPARSE_TEST_CHECK(
      test, asc::Evaluate(context, *compressed_view, *compressed_view).ok());
  CheckValues(test, *compressed_view, before_self);
  asc::CsrView<const double> const_self(*compressed_view);
  ASC_SPARSE_TEST_CHECK(
      test, asc::Evaluate(context, const_self, *compressed_view).ok());
  CheckValues(test, *compressed_view, before_self);

  std::size_t allocations = 0;
  {
    asc_sparse_test::AllocationProbe probe;
    const asc::Status status =
        asc::Evaluate(context, external, *compressed_view);
    allocations = probe.count();
    ASC_SPARSE_TEST_CHECK(test, status.ok());
  }
  ASC_SPARSE_TEST_EQ(test, allocations, std::size_t{0});
}

template <asc::SparsityEffect Effect>
void CheckEffectRejected(TestContext& test,
                         asc::CoordinateView<double, 2> destination) {
  constexpr std::array<double, 12> kLogicalValues{};
  const M4TestExpression<double, 2, Effect> expression{
      .values = kLogicalValues.data(),
      .shape = kShape,
      .memory_space = asc::MemorySpace::kHost,
      .alias_bytes = kLogicalValues.size() * sizeof(double),
  };
  const std::array<double, 5> before{
      destination.values()[0], destination.values()[1], destination.values()[2],
      destination.values()[3], destination.values()[4]};
  CheckError(
      test,
      asc::Evaluate(asc::ExecutionContext::Serial(), expression, destination),
      asc::ErrorCode::kUnsupported);
  CheckValues(test, destination, before);
}

void TestTransactionalRejections(TestContext& test) {
  asc::HostMemoryResource resource;
  auto destination_owner = MakeCoordinate(resource, kInitialValues);
  ASC_SPARSE_TEST_CHECK(test, destination_owner.ok());
  if (!destination_owner.ok()) {
    return;
  }
  auto destination = destination_owner->view();
  ASC_SPARSE_TEST_CHECK(test, destination.ok());
  if (!destination.ok()) {
    return;
  }

  CheckEffectRejected<asc::SparsityEffect::kStructureFiltering>(test,
                                                                *destination);
  CheckEffectRejected<asc::SparsityEffect::kStructureUnion>(test, *destination);
  CheckEffectRejected<asc::SparsityEffect::kStructureIntersection>(
      test, *destination);
  CheckEffectRejected<asc::SparsityEffect::kValueDependent>(test, *destination);
  CheckEffectRejected<asc::SparsityEffect::kDensifying>(test, *destination);
  CheckEffectRejected<asc::SparsityEffect::kDestinationRequired>(test,
                                                                 *destination);

  constexpr std::array<double, 12> kLogicalValues{};
  const M4TestExpression<double, 2, asc::SparsityEffect::kStructurePreserving>
      wrong_shape{
          .values = kLogicalValues.data(),
          .shape = {3, 5},
          .memory_space = asc::MemorySpace::kHost,
          .alias_bytes = kLogicalValues.size() * sizeof(double),
      };
  const std::array<double, 5> before = kInitialValues;
  CheckError(
      test,
      asc::Evaluate(asc::ExecutionContext::Serial(), wrong_shape, *destination),
      asc::ErrorCode::kShape);
  CheckValues(test, *destination, before);

  const std::array<double, 3> rank_one_values{};
  const M4TestExpression<double, 1, asc::SparsityEffect::kStructurePreserving>
      wrong_rank{
          .values = rank_one_values.data(),
          .shape = {3},
          .memory_space = asc::MemorySpace::kHost,
          .alias_bytes = rank_one_values.size() * sizeof(double),
      };
  CheckError(
      test,
      asc::Evaluate(asc::ExecutionContext::Serial(), wrong_rank, *destination),
      asc::ErrorCode::kShape);
  CheckValues(test, *destination, before);

  const M4TestExpression<double, 0, asc::SparsityEffect::kStructurePreserving>
      rank_zero{
          .values = rank_one_values.data(),
          .shape = {},
          .memory_space = asc::MemorySpace::kHost,
          .alias_bytes = sizeof(double),
      };
  CheckError(
      test,
      asc::Evaluate(asc::ExecutionContext::Serial(), rank_zero, *destination),
      asc::ErrorCode::kShape);
  CheckValues(test, *destination, before);

  const M4TestExpression<double, 2, asc::SparsityEffect::kStructurePreserving>
      inaccessible{
          .values = kLogicalValues.data(),
          .shape = kShape,
          .memory_space = asc::MemorySpace::kHost,
          .alias_bytes = kLogicalValues.size() * sizeof(double),
          .access_valid = false,
      };
  CheckError(test,
             asc::Evaluate(asc::ExecutionContext::Serial(), inaccessible,
                           *destination),
             asc::ErrorCode::kMemoryAccess);
  CheckValues(test, *destination, before);

  const M4TestExpression<double, 2, asc::SparsityEffect::kStructurePreserving>
      device_source{
          .values = kLogicalValues.data(),
          .shape = kShape,
          .memory_space = asc::MemorySpace::kDevice,
          .alias_bytes = kLogicalValues.size() * sizeof(double),
      };
  CheckError(test,
             asc::Evaluate(asc::ExecutionContext::Serial(), device_source,
                           *destination),
             asc::ErrorCode::kMemoryAccess);
  CheckValues(test, *destination, before);

  // Source starts inside the destination values: this is not exact
  // self-assignment and must be rejected before the first write.
  const M4TestExpression<double, 2, asc::SparsityEffect::kStructurePreserving>
      overlapping{
          .values = destination->values() + 1,
          .shape = kShape,
          .memory_space = asc::MemorySpace::kHost,
          .alias_bytes = 4 * sizeof(double),
      };
  CheckError(
      test,
      asc::Evaluate(asc::ExecutionContext::Serial(), overlapping, *destination),
      asc::ErrorCode::kInvalidArgument);
  CheckValues(test, *destination, before);

  std::array<double, 5> device_values = kInitialValues;
  auto device_destination = asc::CoordinateView<double, 2>::Create(
      kCoordinates.data(), device_values.data(), kShape, 5,
      asc::MemorySpace::kDevice);
  ASC_SPARSE_TEST_CHECK(test, device_destination.ok());
  if (device_destination.ok()) {
    CheckError(test,
               asc::Evaluate(asc::ExecutionContext::Serial(), wrong_shape,
                             *device_destination),
               asc::ErrorCode::kMemoryAccess);
    ASC_SPARSE_TEST_EQ(test, device_values, kInitialValues);
  }
}

void TestEmptyDestination(TestContext& test) {
  asc::HostMemoryResource resource;
  auto extents = MatrixExtents::Create(0, 4);
  ASC_SPARSE_TEST_CHECK(test, extents.ok());
  if (!extents.ok()) {
    return;
  }
  auto owner = asc::CoordinateArray<double, MatrixExtents>::Create(
      resource, *extents, std::span<const asc::index_t>(),
      std::span<const double>());
  ASC_SPARSE_TEST_CHECK(test, owner.ok());
  if (!owner.ok()) {
    return;
  }
  auto destination = owner->view();
  ASC_SPARSE_TEST_CHECK(test, destination.ok());
  if (!destination.ok()) {
    return;
  }
  const M4TestExpression<double, 2, asc::SparsityEffect::kStructurePreserving>
      expression{
          .values = nullptr,
          .shape = {0, 4},
          .memory_space = asc::MemorySpace::kHost,
          .alias_bytes = 0,
      };
  ASC_SPARSE_TEST_CHECK(test, asc::Evaluate(asc::ExecutionContext::Serial(),
                                            expression, *destination)
                                  .ok());
}

void TestCoordinateStructuralOverlapRejected(TestContext& test) {
  constexpr std::array<asc::extent_t, 1> kVectorShape{4};
  std::array<asc::index_t, 3> source_coordinates{0, 1, 3};
  constexpr std::array<asc::index_t, 3> kSourceValues{101, 202, 303};
  constexpr std::array<asc::index_t, 2> kDestinationCoordinates{0, 3};
  auto source = asc::CoordinateView<const asc::index_t, 1>::Create(
      source_coordinates.data(), kSourceValues.data(), kVectorShape, 3,
      asc::MemorySpace::kHost);
  auto destination = asc::CoordinateView<asc::index_t, 1>::Create(
      kDestinationCoordinates.data(), source_coordinates.data() + 1,
      kVectorShape, 2, asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(test, source.ok());
  ASC_SPARSE_TEST_CHECK(test, destination.ok());
  if (!source.ok() || !destination.ok()) {
    return;
  }

  const auto before = source_coordinates;
  CheckError(
      test,
      asc::Evaluate(asc::ExecutionContext::Serial(), *source, *destination),
      asc::ErrorCode::kInvalidArgument);
  ASC_SPARSE_TEST_EQ(test, source_coordinates, before);

  for (const asc::index_t* coordinate :
       {source_coordinates.data(), source_coordinates.data() + 1,
        source_coordinates.data() + 2}) {
    ASC_SPARSE_TEST_CHECK(test,
                          asc::MayAlias(*source, asc::AliasToken(coordinate)));
  }
}

void TestCompressedStructuralOverlapRejected(TestContext& test) {
  constexpr std::array<asc::extent_t, 2> kMatrixShape{2, 2};
  std::array<asc::nnz_t, 3> source_offsets{0, 1, 2};
  std::array<asc::index_t, 2> source_indices{0, 1};
  constexpr std::array<asc::index_t, 2> kSourceValues{17, 29};
  constexpr std::array<asc::nnz_t, 3> kDestinationOffsets{0, 1, 2};
  constexpr std::array<asc::index_t, 2> kDestinationIndices{0, 1};
  auto source = asc::CsrView<const asc::index_t>::Create(
      source_offsets.data(), source_indices.data(), kSourceValues.data(),
      kMatrixShape, 2, asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(test, source.ok());
  if (!source.ok()) {
    return;
  }

  auto offsets_destination = asc::CsrView<asc::index_t>::Create(
      kDestinationOffsets.data(), kDestinationIndices.data(),
      source_offsets.data() + 1, kMatrixShape, 2, asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(test, offsets_destination.ok());
  if (offsets_destination.ok()) {
    const auto before = source_offsets;
    CheckError(test,
               asc::Evaluate(asc::ExecutionContext::Serial(), *source,
                             *offsets_destination),
               asc::ErrorCode::kInvalidArgument);
    ASC_SPARSE_TEST_EQ(test, source_offsets, before);
  }

  auto indices_destination = asc::CsrView<asc::index_t>::Create(
      kDestinationOffsets.data(), kDestinationIndices.data(),
      source_indices.data(), kMatrixShape, 2, asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(test, indices_destination.ok());
  if (indices_destination.ok()) {
    const auto before = source_indices;
    CheckError(test,
               asc::Evaluate(asc::ExecutionContext::Serial(), *source,
                             *indices_destination),
               asc::ErrorCode::kInvalidArgument);
    ASC_SPARSE_TEST_EQ(test, source_indices, before);
  }

  for (const asc::nnz_t* offset :
       {source_offsets.data(), source_offsets.data() + 1,
        source_offsets.data() + 2}) {
    ASC_SPARSE_TEST_CHECK(test,
                          asc::MayAlias(*source, asc::AliasToken(offset)));
  }
  for (const asc::index_t* index :
       {source_indices.data(), source_indices.data() + 1}) {
    ASC_SPARSE_TEST_CHECK(test, asc::MayAlias(*source, asc::AliasToken(index)));
  }
}

}  // namespace

int main() {
  TestContext test;
  TestCoordinateAndCompressedEvaluation(test);
  TestTransactionalRejections(test);
  TestEmptyDestination(test);
  TestCoordinateStructuralOverlapRejected(test);
  TestCompressedStructuralOverlapRejected(test);
  return test.Finish();
}
