#include <array>
#include <cmath>
#include <complex>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <span>
#include <type_traits>
#include <utility>

#include "allocation_observation.h"
#include "allocation_probe.h"
#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/array.h"
#include "asc/dense/evaluate.h"
#include "asc/dense/layout.h"
#include "asc/dense/view.h"
#include "asc/expression/expression.h"
#include "asc/expression/writable.h"
#include "counting_memory_resource.h"
#include "test_support.h"

namespace {

using asc_dense_test::TestContext;
using MatrixExtents = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;
using VectorExtents = asc::Extents<asc::kDynamicExtent>;

struct UnapprovedScalar {
  double value;
};

static_assert(asc::DenseElement<std::complex<float>>);
static_assert(asc::DenseElement<std::complex<double>>);
static_assert(!asc::DenseElement<std::complex<long double>>);
static_assert(!asc::DenseElement<const std::complex<double>>);
static_assert(!asc::DenseElement<UnapprovedScalar>);
static_assert(!asc::DenseElement<bool>);
static_assert(std::is_trivially_copyable_v<std::complex<float>>);
static_assert(std::is_trivially_copyable_v<std::complex<double>>);
static_assert(std::is_trivially_destructible_v<std::complex<float>>);
static_assert(std::is_trivially_destructible_v<std::complex<double>>);

template <typename Element>
concept HasMinimum = requires(asc::DenseView<Element, 1> view) {
  asc::ReduceMin(asc::ExecutionContext::Serial(), view);
};

template <typename Element>
concept HasMaximum = requires(asc::DenseView<Element, 1> view) {
  asc::ReduceMax(asc::ExecutionContext::Serial(), view);
};

static_assert(HasMinimum<double> && HasMaximum<int>);
static_assert(!HasMinimum<std::complex<float>>);
static_assert(!HasMaximum<const std::complex<double>>);
static_assert(
    std::is_convertible_v<asc::DenseView<std::complex<double>, 2>,
                          asc::DenseView<const std::complex<double>, 2>>);
static_assert(
    !std::is_convertible_v<asc::DenseView<const std::complex<double>, 2>,
                           asc::DenseView<std::complex<double>, 2>>);

template <typename Value>
Value Take(asc::Result<Value> result) {
  if (!result.ok()) {
    std::abort();
  }
  return std::move(*result);
}

template <typename Real, typename Extents, typename Layout>
void CheckShape(TestContext& test, Extents extents, Layout layout) {
  using Value = std::complex<Real>;
  using Owner = asc::DenseArray<Value, Extents>;
  asc_dense_test::CountingMemoryResource resource;
  {
    auto owner = Take(Owner::Create(resource, extents, layout));
    auto view = Take(owner.view());
    static_assert(asc::ReadableExpression<decltype(view)>);
    static_assert(asc::WritableExpression<decltype(view)>);
    ASC_DENSE_TEST_EQ(test, owner.logical_size(), extents.logical_size());
    ASC_DENSE_TEST_EQ(test, resource.allocation_requests(), std::size_t{1});
    ASC_DENSE_TEST_EQ(
        test, resource.allocated_bytes(),
        static_cast<std::size_t>(owner.logical_size()) * sizeof(Value));
    if (owner.logical_size() == 0) {
      ASC_DENSE_TEST_CHECK(test, view.data() == nullptr);
    } else {
      ASC_DENSE_TEST_EQ(
          test, reinterpret_cast<std::uintptr_t>(view.data()) % alignof(Value),
          std::uintptr_t{0});
    }
    Value expected{};
    for (asc::extent_t index = 0; index < owner.logical_size(); ++index) {
      ASC_DENSE_TEST_EQ(test, view.data()[index], Value{});
      const auto value = static_cast<Real>(index + 1);
      view.data()[index] = Value(value, -value);
      expected += Value(value, -value);
    }
    const auto& const_owner = owner;
    auto const_view = Take(const_owner.view());
    static_assert(
        std::same_as<typename decltype(const_view)::element_type, const Value>);
    asc_dense_test::AllocationProbe probe;
    auto sum =
        Take(asc::ReduceSum(asc::ExecutionContext::Serial(), const_view));
    ASC_DENSE_TEST_EQ(test, sum, expected);
    ASC_DENSE_TEST_CHECK(
        test, asc_test::ProcessAllocationCountMatches(probe.count(), 0));
  }
  ASC_DENSE_TEST_EQ(test, resource.live_allocations(), std::size_t{0});
  ASC_DENSE_TEST_EQ(test, resource.allocated_bytes(),
                    resource.deallocated_bytes());
}

template <typename Real>
void TestRanksAndLayouts(TestContext& test) {
  CheckShape<Real>(test, Take(asc::Extents<>::Create()), asc::LayoutLeft{});
  CheckShape<Real>(test, Take(VectorExtents::Create(4)), asc::LayoutRight{});
  CheckShape<Real>(test, Take(asc::Extents<2, 3>::Create()), asc::LayoutLeft{});
  CheckShape<Real>(test, Take(asc::Extents<2, 3>::Create()),
                   asc::LayoutRight{});
  CheckShape<Real>(test,
                   Take(asc::Extents<2, asc::kDynamicExtent, 2>::Create(3)),
                   asc::LayoutLeft{});
  CheckShape<Real>(test,
                   Take(asc::Extents<2, asc::kDynamicExtent, 2>::Create(3)),
                   asc::LayoutRight{});
  CheckShape<Real>(test, Take(VectorExtents::Create(0)), asc::LayoutLeft{});
  CheckShape<Real>(test, Take(asc::Extents<0, 3>::Create()),
                   asc::LayoutRight{});
  using RankThree = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent,
                                 asc::kDynamicExtent>;
  CheckShape<Real>(test, Take(RankThree::Create(2, 0, 5)), asc::LayoutRight{});
}

template <typename Real>
void TestLifecycleAndRollback(TestContext& test) {
  using Value = std::complex<Real>;
  using Owner = asc::DenseArray<Value, MatrixExtents>;
  static_assert(std::is_nothrow_move_constructible_v<Owner>);
  static_assert(std::is_nothrow_move_assignable_v<Owner>);
  static_assert(!std::is_copy_constructible_v<Owner>);
  asc_dense_test::CountingMemoryResource resource;
  {
    const auto extents = Take(MatrixExtents::Create(2, 2));
    auto owner = Take(Owner::Create(resource, extents, asc::LayoutRight{}));
    auto original = Take(owner.view());
    original.data()[0] = Value(Real{2}, Real{-3});
    original.data()[1] =
        Value(Real{-0.0}, std::numeric_limits<Real>::infinity());
    original.data()[2] = Value(std::numeric_limits<Real>::quiet_NaN(), Real{4});
    auto clone = Take(owner.Clone(resource, asc::ExecutionContext::Serial()));
    auto cloned = Take(clone.view());
    ASC_DENSE_TEST_CHECK(test, cloned.data() != original.data());
    ASC_DENSE_TEST_EQ(test, cloned.data()[0], Value(Real{2}, Real{-3}));
    ASC_DENSE_TEST_CHECK(test, std::signbit(cloned.data()[1].real()));
    ASC_DENSE_TEST_CHECK(test, std::isinf(cloned.data()[1].imag()));
    ASC_DENSE_TEST_CHECK(test, std::isnan(cloned.data()[2].real()));
    cloned.data()[0] = Value(Real{9}, Real{1});
    ASC_DENSE_TEST_EQ(test, original.data()[0], Value(Real{2}, Real{-3}));
    resource.FailRequest(resource.allocation_requests());
    auto failed_clone = owner.Clone(resource, asc::ExecutionContext::Serial());
    ASC_DENSE_TEST_CHECK(test, !failed_clone.ok());
    ASC_DENSE_TEST_EQ(test, failed_clone.status().code(),
                      asc::ErrorCode::kAllocation);
    ASC_DENSE_TEST_EQ(test, Take(owner.view()).data(), original.data());
    resource.FailRequest(resource.allocation_requests());
    auto resized_extents = Take(MatrixExtents::Create(3, 1));
    auto resize = owner.DiscardResize(resized_extents);
    ASC_DENSE_TEST_EQ(test, resize.code(), asc::ErrorCode::kAllocation);
    ASC_DENSE_TEST_EQ(test, owner.logical_size(), asc::extent_t{4});
    ASC_DENSE_TEST_EQ(test, Take(owner.view()).data(), original.data());
    ASC_DENSE_TEST_EQ(test, original.data()[0], Value(Real{2}, Real{-3}));
    resource.DisableFailure();
    auto moved = std::move(clone);
    // These checks exercise explicitly documented moved-from postconditions.
    // NOLINTBEGIN(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
    ASC_DENSE_TEST_CHECK(test, !clone.valid());
    ASC_DENSE_TEST_CHECK(test, !clone.view().ok());
    ASC_DENSE_TEST_EQ(test, clone.DiscardResize(extents).code(),
                      asc::ErrorCode::kInvalidState);
    owner = std::move(moved);
    ASC_DENSE_TEST_CHECK(test, !moved.valid());
    // NOLINTEND(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
    ASC_DENSE_TEST_EQ(test, Take(owner.view()).data()[0],
                      Value(Real{9}, Real{1}));
    ASC_DENSE_TEST_CHECK(test, owner.DiscardResize(resized_extents).ok());
    ASC_DENSE_TEST_EQ(test, owner.mapping().kind(),
                      asc::DenseLayoutKind::kRight);
    auto resized = Take(owner.view());
    for (asc::extent_t index = 0; index < owner.logical_size(); ++index) {
      ASC_DENSE_TEST_EQ(test, resized.data()[index], Value{});
    }
    ASC_DENSE_TEST_CHECK(test,
                         owner.DiscardResize(extents, asc::LayoutLeft{}).ok());
    ASC_DENSE_TEST_EQ(test, owner.mapping().kind(),
                      asc::DenseLayoutKind::kLeft);
  }
  ASC_DENSE_TEST_EQ(test, resource.live_allocations(), std::size_t{0});
  ASC_DENSE_TEST_EQ(test, resource.allocated_bytes(),
                    resource.deallocated_bytes());
}

template <typename Real>
void TestConstructionPlacementAndFailure(TestContext& test) {
  using Value = std::complex<Real>;
  using Owner = asc::DenseArray<Value, VectorExtents>;
  const auto extents = Take(VectorExtents::Create(3));
  for (auto space : {asc::MemorySpace::kHost, asc::MemorySpace::kPinnedHost}) {
    asc_dense_test::CountingMemoryResource resource(space);
    auto owner = Take(Owner::CreateUninitialized(extents, resource));
    auto view = Take(owner.view());
    ASC_DENSE_TEST_EQ(test, view.memory_space(), space);
    for (asc::index_t index = 0; index < 3; ++index) {
      std::array<asc::index_t, 1> coordinates{index};
      ASC_DENSE_TEST_EQ(test, *Take(view.At(coordinates)), Value{});
    }
    if (space == asc::MemorySpace::kPinnedHost) {
      auto clone = owner.Clone(resource, asc::ExecutionContext::Serial());
      ASC_DENSE_TEST_EQ(test, clone.status().code(),
                        asc::ErrorCode::kMemoryAccess);
      ASC_DENSE_TEST_EQ(test, resource.allocation_requests(), std::size_t{1});
    }
  }
  for (auto space : {asc::MemorySpace::kDevice, asc::MemorySpace::kManaged}) {
    asc_dense_test::CountingMemoryResource resource(space);
    auto owner = Owner::CreateUninitialized(extents, resource);
    ASC_DENSE_TEST_EQ(test, owner.status().code(),
                      asc::ErrorCode::kUnsupported);
    auto empty =
        Owner::CreateUninitialized(Take(VectorExtents::Create(0)), resource);
    ASC_DENSE_TEST_EQ(test, empty.status().code(),
                      asc::ErrorCode::kUnsupported);
    ASC_DENSE_TEST_EQ(test, resource.allocation_requests(), std::size_t{0});
  }
  asc_dense_test::CountingMemoryResource failing;
  failing.FailRequest(0);
  auto owner = Owner::Create(failing, extents);
  ASC_DENSE_TEST_EQ(test, owner.status().code(), asc::ErrorCode::kAllocation);
  ASC_DENSE_TEST_EQ(test, failing.live_allocations(), std::size_t{0});
  asc_dense_test::CountingMemoryResource overflow;
  auto huge =
      Take(VectorExtents::Create(std::numeric_limits<asc::extent_t>::max()));
  auto too_large = Owner::CreateUninitialized(huge, overflow);
  ASC_DENSE_TEST_EQ(test, too_large.status().code(), asc::ErrorCode::kOverflow);
  ASC_DENSE_TEST_EQ(test, overflow.allocation_requests(), std::size_t{0});
}

template <typename Real>
void TestStridedAlgebra(TestContext& test) {
  using Value = std::complex<Real>;
  const Value poison(Real{81}, Real{-27});
  std::array<Value, 8> left{Value(1, 2),  Value(3, -1), poison, poison,
                            Value(-2, 1), Value(4, 2),  poison, poison};
  std::array<Value, 8> right{Value(2, -1), Value(1, 3),  poison, poison,
                             Value(3, 2),  Value(-1, 1), poison, poison};
  std::array<Value, 8> output;
  output.fill(poison);
  const std::array<asc::extent_t, 2> shape{2, 2};
  auto layout = Take(asc::DenseLayout<2>::Create(
      shape, asc::LayoutStride<2>{.strides = {1, 4}}));
  auto lhs = Take(asc::DenseView<const Value, 2>::Create(
      left.data(), layout, asc::MemorySpace::kHost));
  auto rhs = Take(asc::DenseView<const Value, 2>::Create(
      right.data(), layout, asc::MemorySpace::kHost));
  auto destination = Take(asc::DenseView<Value, 2>::Create(
      output.data(), layout, asc::MemorySpace::kHost));
  // Hand-computed complex products distinguish algebraic multiplication from
  // conjugating multiplication; padding is deliberately not mathematical data.
  const std::array<Value, 4> expected_products{Value(4, 3), Value(6, 8),
                                               Value(-8, -1), Value(-6, 2)};
  auto product = Take(asc::MakeMultiply(lhs, rhs));
  auto sum = Take(asc::MakeAdd(lhs, rhs));
  auto difference = Take(asc::MakeSubtract(lhs, rhs));
  auto negative = asc::MakeNegate(lhs);
  asc_dense_test::AllocationProbe probe;
  ASC_DENSE_TEST_CHECK(
      test, asc::Evaluate(asc::ExecutionContext::Serial(), product, destination)
                .ok());
  for (std::size_t index = 0; index < 4; ++index) {
    const std::size_t offset = (index / 2) * 4 + index % 2;
    ASC_DENSE_TEST_EQ(test, output[offset], expected_products[index]);
  }
  ASC_DENSE_TEST_EQ(
      test, Take(asc::ReduceSum(asc::ExecutionContext::Serial(), destination)),
      Value(-4, 12));
  ASC_DENSE_TEST_CHECK(
      test,
      asc::Evaluate(asc::ExecutionContext::Serial(), sum, destination).ok());
  ASC_DENSE_TEST_EQ(test, output[0], Value(3, 1));
  ASC_DENSE_TEST_CHECK(test, asc::Evaluate(asc::ExecutionContext::Serial(),
                                           difference, destination)
                                 .ok());
  ASC_DENSE_TEST_EQ(test, output[0], Value(-1, 3));
  ASC_DENSE_TEST_CHECK(test, asc::Evaluate(asc::ExecutionContext::Serial(),
                                           negative, destination)
                                 .ok());
  ASC_DENSE_TEST_EQ(test, output[0], Value(-1, -2));
  for (std::size_t offset : {2U, 3U, 6U, 7U}) {
    ASC_DENSE_TEST_EQ(test, output[offset], poison);
    ASC_DENSE_TEST_EQ(test, left[offset], poison);
    ASC_DENSE_TEST_EQ(test, right[offset], poison);
  }
  ASC_DENSE_TEST_CHECK(
      test, asc_test::ProcessAllocationCountMatches(probe.count(), 0));
}

template <typename Real>
void RunScalarTests(TestContext& test) {
  TestRanksAndLayouts<Real>(test);
  TestLifecycleAndRollback<Real>(test);
  TestConstructionPlacementAndFailure<Real>(test);
  TestStridedAlgebra<Real>(test);
}

}  // namespace

int main() {
  TestContext test;
  RunScalarTests<float>(test);
  RunScalarTests<double>(test);
  return test.Finish();
}
