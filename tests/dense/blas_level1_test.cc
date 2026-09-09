#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdlib>
#include <limits>

#include "allocation_observation.h"
#include "allocation_probe.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "test_support.h"

namespace {

using asc_dense_test::TestContext;

template <typename Element>
using Vector = asc::DenseBlasVectorView<Element>;

template <typename Element, std::size_t Size>
Vector<Element> MakeVector(std::array<Element, Size>& storage,
                           Element* logical_first, asc::extent_t size,
                           asc::stride_t increment,
                           asc::MemorySpace space = asc::MemorySpace::kHost) {
  auto vector = Vector<Element>::Create(
      logical_first, size, increment,
      asc::ConstMemoryView(storage.data(), sizeof(storage), space));
  if (!vector.ok()) {
    std::abort();
  }
  return *vector;
}

template <typename Element, std::size_t Size>
Vector<Element> MakeVector(std::array<Element, Size>& storage,
                           asc::extent_t size, asc::stride_t increment = 1,
                           asc::MemorySpace space = asc::MemorySpace::kHost) {
  return MakeVector(storage, storage.data(), size, increment, space);
}

template <typename Element>
Element& At(Vector<Element> vector, asc::index_t index) {
  return vector.data()[index * vector.increment()];
}

void CheckError(TestContext& test, const asc::Status& status,
                asc::ErrorCode expected) {
  ASC_DENSE_TEST_CHECK(test, !status.ok());
  if (!status.ok()) {
    ASC_DENSE_TEST_EQ(test, status.code(), expected);
  }
}

template <typename Result>
void CheckResultError(TestContext& test, const Result& result,
                      asc::ErrorCode expected) {
  ASC_DENSE_TEST_CHECK(test, !result.ok());
  if (!result.ok()) {
    ASC_DENSE_TEST_EQ(test, result.status().code(), expected);
  }
}

template <typename Real>
void CheckNear(TestContext& test, Real actual, Real expected) {
  ASC_DENSE_TEST_NEAR(test, actual, expected,
                      asc_dense_test::DefaultTolerance<Real>(),
                      asc_dense_test::DefaultTolerance<Real>());
}

template <typename Real>
void CheckNear(TestContext& test, std::complex<Real> actual,
               std::complex<Real> expected) {
  CheckNear(test, actual.real(), expected.real());
  CheckNear(test, actual.imag(), expected.imag());
}

void TestDescriptorValidation(TestContext& test) {
  std::array<double, 5> storage{};
  CheckResultError(test,
                   Vector<double>::Create(
                       storage.data(), -1, 1,
                       asc::ConstMemoryView(storage.data(), sizeof(storage),
                                            asc::MemorySpace::kHost)),
                   asc::ErrorCode::kInvalidArgument);
  CheckResultError(test,
                   Vector<double>::Create(
                       storage.data(), 1, 0,
                       asc::ConstMemoryView(storage.data(), sizeof(storage),
                                            asc::MemorySpace::kHost)),
                   asc::ErrorCode::kInvalidArgument);
  CheckResultError(test,
                   Vector<double>::Create(
                       storage.data() + 4, 3, 1,
                       asc::ConstMemoryView(storage.data(), sizeof(storage),
                                            asc::MemorySpace::kHost)),
                   asc::ErrorCode::kMemoryAccess);
  CheckResultError(test,
                   Vector<double>::Create(
                       storage.data(), 2, -1,
                       asc::ConstMemoryView(storage.data(), sizeof(storage),
                                            asc::MemorySpace::kHost)),
                   asc::ErrorCode::kMemoryAccess);

  auto reverse = Vector<double>::Create(
      storage.data() + 4, 3, -2,
      asc::ConstMemoryView(storage.data(), sizeof(storage),
                           asc::MemorySpace::kHost));
  ASC_DENSE_TEST_CHECK(test, reverse.ok());
  if (reverse.ok()) {
    ASC_DENSE_TEST_EQ(test, reverse->data(), storage.data() + 4);
    ASC_DENSE_TEST_EQ(test, reverse->size(), asc::extent_t{3});
    ASC_DENSE_TEST_EQ(test, reverse->increment(), asc::stride_t{-2});
    ASC_DENSE_TEST_EQ(test, reverse->reachable_storage().size(),
                      std::size_t{5 * sizeof(double)});
    Vector<const double> read_only = *reverse;
    ASC_DENSE_TEST_EQ(test, read_only.data(), storage.data() + 4);
  }

  auto empty = Vector<float>::Create(
      nullptr, 0, -3,
      asc::ConstMemoryView(nullptr, 0, asc::MemorySpace::kHost));
  ASC_DENSE_TEST_CHECK(test, empty.ok());
}

template <typename Element>
void TestVectorUpdates(TestContext& test) {
  const asc::ExecutionContext context = asc::ExecutionContext::Serial();
  std::array<Element, 6> x_storage{
      Element{1}, Element{0}, Element{2}, Element{0}, Element{3}, Element{0},
  };
  std::array<Element, 6> y_storage{
      Element{6}, Element{0}, Element{5}, Element{0}, Element{4}, Element{0},
  };
  auto x = MakeVector(x_storage, x_storage.data() + 4, 3, -2);
  auto y = MakeVector(y_storage, 3, 2);

  ASC_DENSE_TEST_CHECK(test, asc::Swap(context, x, y).ok());
  ASC_DENSE_TEST_EQ(test, At(x, 0), Element{6});
  ASC_DENSE_TEST_EQ(test, At(x, 1), Element{5});
  ASC_DENSE_TEST_EQ(test, At(x, 2), Element{4});
  ASC_DENSE_TEST_EQ(test, At(y, 0), Element{3});
  ASC_DENSE_TEST_EQ(test, At(y, 1), Element{2});
  ASC_DENSE_TEST_EQ(test, At(y, 2), Element{1});

  ASC_DENSE_TEST_CHECK(test,
                       asc::Copy(context, Vector<const Element>(x), y).ok());
  ASC_DENSE_TEST_CHECK(test, asc::Scal(context, Element{2}, y).ok());
  ASC_DENSE_TEST_CHECK(
      test, asc::Axpy(context, Element{-1}, Vector<const Element>(x), y).ok());
  ASC_DENSE_TEST_EQ(test, At(y, 0), Element{6});
  ASC_DENSE_TEST_EQ(test, At(y, 1), Element{5});
  ASC_DENSE_TEST_EQ(test, At(y, 2), Element{4});

  std::size_t allocations = 0;
  {
    asc_dense_test::AllocationProbe probe;
    const asc::Status status = asc::Scal(context, Element{1}, y);
    allocations = probe.count();
    ASC_DENSE_TEST_CHECK(test, status.ok());
  }
  ASC_DENSE_TEST_CHECK(test,
                       asc_test::ProcessAllocationCountMatches(allocations, 0));
}

template <typename Real>
void TestRealRotations(TestContext& test) {
  const asc::ExecutionContext context = asc::ExecutionContext::Serial();
  std::array<Real, 1> a_storage{Real{3}};
  std::array<Real, 1> b_storage{Real{4}};
  std::array<Real, 1> c_storage{};
  std::array<Real, 1> s_storage{};
  auto a = MakeVector(a_storage, 1);
  auto b = MakeVector(b_storage, 1);
  auto c = MakeVector(c_storage, 1);
  auto s = MakeVector(s_storage, 1);
  ASC_DENSE_TEST_CHECK(test, asc::Rotg(context, a, b, c, s).ok());
  CheckNear(test, At(a, 0), Real{5});
  CheckNear(test, At(c, 0), Real{0.6});
  CheckNear(test, At(s, 0), Real{0.8});

  std::array<Real, 3> x_storage{Real{1}, Real{2}, Real{3}};
  std::array<Real, 3> y_storage{Real{4}, Real{5}, Real{6}};
  auto x = MakeVector(x_storage, 3);
  auto y = MakeVector(y_storage, 3);
  ASC_DENSE_TEST_CHECK(test, asc::Rot(context, x, y, Real{0}, Real{1}).ok());
  ASC_DENSE_TEST_EQ(test, x_storage,
                    (std::array<Real, 3>{Real{4}, Real{5}, Real{6}}));
  ASC_DENSE_TEST_EQ(test, y_storage,
                    (std::array<Real, 3>{Real{-1}, Real{-2}, Real{-3}}));

  std::array<Real, 1> d1_storage{Real{1}};
  std::array<Real, 1> d2_storage{Real{1}};
  std::array<Real, 1> x1_storage{Real{1}};
  std::array<Real, 1> y1_storage{Real{1}};
  std::array<Real, 5> parameter_storage{};
  auto d1 = MakeVector(d1_storage, 1);
  auto d2 = MakeVector(d2_storage, 1);
  auto x1 = MakeVector(x1_storage, 1);
  auto y1 = MakeVector(y1_storage, 1);
  auto parameters = MakeVector(parameter_storage, 5);
  ASC_DENSE_TEST_CHECK(
      test,
      asc::Rotmg(context, d1, d2, x1, Vector<const Real>(y1), parameters).ok());
  CheckNear(test, At(d1, 0), Real{0.5});
  CheckNear(test, At(d2, 0), Real{0.5});
  CheckNear(test, At(x1, 0), Real{2});
  ASC_DENSE_TEST_EQ(test, At(parameters, 0), Real{1});

  std::array<Real, 2> modified_x_storage{Real{1}, Real{2}};
  std::array<Real, 2> modified_y_storage{Real{1}, Real{4}};
  auto modified_x = MakeVector(modified_x_storage, 2);
  auto modified_y = MakeVector(modified_y_storage, 2);
  ASC_DENSE_TEST_CHECK(test, asc::Rotm(context, modified_x, modified_y,
                                       Vector<const Real>(parameters))
                                 .ok());
  ASC_DENSE_TEST_EQ(test, modified_x_storage,
                    (std::array<Real, 2>{Real{2}, Real{6}}));
  ASC_DENSE_TEST_EQ(test, modified_y_storage,
                    (std::array<Real, 2>{Real{0}, Real{2}}));

  d1_storage[0] = Real{2};
  d2_storage[0] = Real{1};
  x1_storage[0] = Real{3};
  y1_storage[0] = Real{1};
  parameter_storage.fill(Real{9});
  ASC_DENSE_TEST_CHECK(
      test,
      asc::Rotmg(context, d1, d2, x1, Vector<const Real>(y1), parameters).ok());
  CheckNear(test, d1_storage[0], Real{36} / Real{19});
  CheckNear(test, d2_storage[0], Real{18} / Real{19});
  CheckNear(test, x1_storage[0], Real{19} / Real{6});
  ASC_DENSE_TEST_EQ(test, parameter_storage[0], Real{0});
  CheckNear(test, parameter_storage[2], Real{-1} / Real{3});
  CheckNear(test, parameter_storage[3], Real{1} / Real{6});

  d1_storage[0] = Real{2};
  d2_storage[0] = Real{0};
  x1_storage[0] = Real{3};
  y1_storage[0] = Real{1};
  parameter_storage.fill(Real{9});
  ASC_DENSE_TEST_CHECK(
      test,
      asc::Rotmg(context, d1, d2, x1, Vector<const Real>(y1), parameters).ok());
  ASC_DENSE_TEST_EQ(test, d1_storage[0], Real{2});
  ASC_DENSE_TEST_EQ(test, d2_storage[0], Real{0});
  ASC_DENSE_TEST_EQ(test, x1_storage[0], Real{3});
  ASC_DENSE_TEST_EQ(test, parameter_storage[0], Real{-2});
  ASC_DENSE_TEST_EQ(test, parameter_storage[1], Real{9});
}

template <typename Real>
void TestComplexRotationsAndScal(TestContext& test) {
  using Complex = std::complex<Real>;
  const asc::ExecutionContext context = asc::ExecutionContext::Serial();
  std::array<Complex, 1> a_storage{Complex{3, 4}};
  const Complex original_a = a_storage[0];
  const std::array<Complex, 1> original_b{Complex{2, -1}};
  std::array<Complex, 1> b_storage = original_b;
  std::array<Real, 1> c_storage{};
  std::array<Complex, 1> s_storage{};
  auto a = MakeVector(a_storage, 1);
  auto b = MakeVector(b_storage, 1);
  auto c = MakeVector(c_storage, 1);
  auto s = MakeVector(s_storage, 1);
  ASC_DENSE_TEST_CHECK(
      test, asc::Rotg(context, a, Vector<const Complex>(b), c, s).ok());
  ASC_DENSE_TEST_EQ(test, b_storage, original_b);
  const Complex annihilated =
      -std::conj(At(s, 0)) * original_a + At(c, 0) * original_b[0];
  CheckNear(test, annihilated, Complex{0, 0});

  std::array<Complex, 2> x_storage{Complex{1, 1}, Complex{2, -1}};
  std::array<Complex, 2> y_storage{Complex{3, -2}, Complex{-1, 4}};
  const auto old_x = x_storage;
  const auto old_y = y_storage;
  auto x = MakeVector(x_storage, 2);
  auto y = MakeVector(y_storage, 2);
  ASC_DENSE_TEST_CHECK(test, asc::Rot(context, x, y, Real{0}, Real{1}).ok());
  ASC_DENSE_TEST_EQ(test, x_storage, old_y);
  ASC_DENSE_TEST_EQ(test, y_storage,
                    (std::array<Complex, 2>{-old_x[0], -old_x[1]}));

  ASC_DENSE_TEST_CHECK(test, asc::Scal(context, Real{2}, x).ok());
  ASC_DENSE_TEST_CHECK(test, asc::Scal(context, Complex{0, 1}, x).ok());
}

template <typename Real>
void TestRealReductions(TestContext& test) {
  const asc::ExecutionContext context = asc::ExecutionContext::Serial();
  std::array<Real, 3> left_storage{Real{1}, Real{-2}, Real{3}};
  std::array<Real, 3> right_storage{Real{4}, Real{5}, Real{6}};
  std::array<Real, 1> result_storage{};
  auto left = MakeVector(left_storage, 3);
  auto right = MakeVector(right_storage, 3);
  auto result = MakeVector(result_storage, 1);
  ASC_DENSE_TEST_CHECK(test, asc::Dot(context, Vector<const Real>(left),
                                      Vector<const Real>(right), result)
                                 .ok());
  ASC_DENSE_TEST_EQ(test, result_storage[0], Real{12});
  ASC_DENSE_TEST_CHECK(
      test, asc::Nrm2(context, Vector<const Real>(left), result).ok());
  CheckNear(test, result_storage[0], std::sqrt(Real{14}));
  ASC_DENSE_TEST_CHECK(
      test, asc::Asum(context, Vector<const Real>(left), result).ok());
  ASC_DENSE_TEST_EQ(test, result_storage[0], Real{6});

  std::array<asc::index_t, 1> index_storage{};
  auto index = MakeVector(index_storage, 1);
  ASC_DENSE_TEST_CHECK(
      test, asc::Iamax(context, Vector<const Real>(left), index).ok());
  ASC_DENSE_TEST_EQ(test, index_storage[0], asc::index_t{2});

  std::array<Real, 5> tied_storage{Real{5}, Real{0}, Real{1}, Real{0}, Real{5}};
  auto tied = MakeVector(tied_storage, tied_storage.data() + 4, 3, -2);
  ASC_DENSE_TEST_CHECK(
      test, asc::Iamax(context, Vector<const Real>(tied), index).ok());
  ASC_DENSE_TEST_EQ(test, index_storage[0], asc::index_t{0});
}

void TestMixedDotProducts(TestContext& test) {
  const asc::ExecutionContext context = asc::ExecutionContext::Serial();
  std::array<float, 3> left_storage{1.0F, 2.0F, 3.0F};
  std::array<float, 3> right_storage{4.0F, 5.0F, 6.0F};
  std::array<float, 1> float_result_storage{};
  std::array<double, 1> double_result_storage{};
  auto left = MakeVector(left_storage, 3);
  auto right = MakeVector(right_storage, 3);
  auto float_result = MakeVector(float_result_storage, 1);
  auto double_result = MakeVector(double_result_storage, 1);
  ASC_DENSE_TEST_CHECK(test, asc::Dot(context, 2.0F, Vector<const float>(left),
                                      Vector<const float>(right), float_result)
                                 .ok());
  ASC_DENSE_TEST_EQ(test, float_result_storage[0], 34.0F);

  left_storage = {1.0e8F, 1.0F, -1.0e8F};
  right_storage = {1.0F, 1.0F, 1.0F};
  ASC_DENSE_TEST_CHECK(test, asc::Dot(context, 0.0F, Vector<const float>(left),
                                      Vector<const float>(right), float_result)
                                 .ok());
  ASC_DENSE_TEST_EQ(test, float_result_storage[0], 1.0F);

  left_storage = {1.0F, 2.0F, 3.0F};
  right_storage = {4.0F, 5.0F, 6.0F};
  ASC_DENSE_TEST_CHECK(
      test, asc::Dot(context, asc::DenseBlasDotAccumulation::kDouble,
                     Vector<const float>(left), Vector<const float>(right),
                     double_result)
                .ok());
  ASC_DENSE_TEST_EQ(test, double_result_storage[0], 32.0);
}

template <typename Real>
void TestExtremeValues(TestContext& test) {
  const asc::ExecutionContext context = asc::ExecutionContext::Serial();
  const Real large = std::numeric_limits<Real>::max() / Real{4};
  std::array<Real, 2> large_storage{large, large};
  std::array<Real, 1> result_storage{};
  auto values = MakeVector(large_storage, 2);
  auto result = MakeVector(result_storage, 1);
  ASC_DENSE_TEST_CHECK(
      test, asc::Nrm2(context, Vector<const Real>(values), result).ok());
  ASC_DENSE_TEST_CHECK(test, std::isfinite(result_storage[0]));
  CheckNear(test, result_storage[0], std::hypot(large, large));

  std::array<Real, 1> infinity_storage{std::numeric_limits<Real>::infinity()};
  auto infinity = MakeVector(infinity_storage, 1);
  ASC_DENSE_TEST_CHECK(
      test, asc::Nrm2(context, Vector<const Real>(infinity), result).ok());
  ASC_DENSE_TEST_CHECK(test, std::isinf(result_storage[0]));

  std::array<Real, 1> nan_storage{std::numeric_limits<Real>::quiet_NaN()};
  auto nan = MakeVector(nan_storage, 1);
  ASC_DENSE_TEST_CHECK(
      test, asc::Nrm2(context, Vector<const Real>(nan), result).ok());
  ASC_DENSE_TEST_CHECK(test, std::isnan(result_storage[0]));

  std::array<Real, 1> destination_storage{Real{7}};
  auto destination = MakeVector(destination_storage, 1);
  ASC_DENSE_TEST_CHECK(
      test,
      asc::Axpy(context, Real{0}, Vector<const Real>(nan), destination).ok());
  ASC_DENSE_TEST_EQ(test, destination_storage[0], Real{7});
}

template <typename Real>
void TestComplexReductions(TestContext& test) {
  using Complex = std::complex<Real>;
  const asc::ExecutionContext context = asc::ExecutionContext::Serial();
  std::array<Complex, 2> left_storage{Complex{1, 2}, Complex{3, -1}};
  std::array<Complex, 2> right_storage{Complex{2, -1}, Complex{-2, 4}};
  std::array<Complex, 1> complex_result_storage{};
  std::array<Real, 1> real_result_storage{};
  auto left = MakeVector(left_storage, 2);
  auto right = MakeVector(right_storage, 2);
  auto complex_result = MakeVector(complex_result_storage, 1);
  auto real_result = MakeVector(real_result_storage, 1);
  ASC_DENSE_TEST_CHECK(test,
                       asc::Dotu(context, Vector<const Complex>(left),
                                 Vector<const Complex>(right), complex_result)
                           .ok());
  CheckNear(
      test, complex_result_storage[0],
      left_storage[0] * right_storage[0] + left_storage[1] * right_storage[1]);
  ASC_DENSE_TEST_CHECK(test,
                       asc::Dotc(context, Vector<const Complex>(left),
                                 Vector<const Complex>(right), complex_result)
                           .ok());
  CheckNear(test, complex_result_storage[0],
            std::conj(left_storage[0]) * right_storage[0] +
                std::conj(left_storage[1]) * right_storage[1]);
  ASC_DENSE_TEST_CHECK(
      test, asc::Nrm2(context, Vector<const Complex>(left), real_result).ok());
  CheckNear(test, real_result_storage[0], std::sqrt(Real{15}));
  ASC_DENSE_TEST_CHECK(
      test, asc::Asum(context, Vector<const Complex>(left), real_result).ok());
  ASC_DENSE_TEST_EQ(test, real_result_storage[0], Real{7});

  std::array<asc::index_t, 1> index_storage{};
  auto index = MakeVector(index_storage, 1);
  ASC_DENSE_TEST_CHECK(
      test, asc::Iamax(context, Vector<const Complex>(left), index).ok());
  ASC_DENSE_TEST_EQ(test, index_storage[0], asc::index_t{1});
}

void TestFailuresAndEmpty(TestContext& test) {
  const asc::ExecutionContext context = asc::ExecutionContext::Serial();
  std::array<float, 6> storage{1, 2, 3, 4, 5, 6};
  auto source = MakeVector(storage, storage.data(), 3, 1);
  auto partial = MakeVector(storage, storage.data() + 1, 3, 1);
  CheckError(test, asc::Copy(context, Vector<const float>(source), partial),
             asc::ErrorCode::kInvalidArgument);
  CheckError(test, asc::Swap(context, source, partial),
             asc::ErrorCode::kInvalidArgument);
  CheckError(test, asc::Rot(context, source, source, 1.0F, 0.0F),
             asc::ErrorCode::kInvalidArgument);
  auto overlapping_result = MakeVector(storage, storage.data(), 1, 1);
  CheckError(test,
             asc::Dot(context, Vector<const float>(source),
                      Vector<const float>(source), overlapping_result),
             asc::ErrorCode::kInvalidArgument);

  std::array<float, 2> short_storage{};
  auto short_vector = MakeVector(short_storage, 2);
  CheckError(
      test, asc::Axpy(context, 1.0F, Vector<const float>(source), short_vector),
      asc::ErrorCode::kShape);

  auto device = MakeVector(storage, 3, 1, asc::MemorySpace::kDevice);
  CheckError(test, asc::Scal(context, 2.0F, device),
             asc::ErrorCode::kMemoryAccess);

  std::array<float, 9> parameter_storage{};
  auto noncontiguous_parameters =
      MakeVector(parameter_storage, parameter_storage.data(), 5, 2);
  std::array<float, 3> rotation_x_storage{};
  std::array<float, 3> rotation_y_storage{};
  auto rotation_x = MakeVector(rotation_x_storage, 3);
  auto rotation_y = MakeVector(rotation_y_storage, 3);
  CheckError(test,
             asc::Rotm(context, rotation_x, rotation_y,
                       Vector<const float>(noncontiguous_parameters)),
             asc::ErrorCode::kInvalidArgument);

  auto empty = Vector<float>::Create(
      nullptr, 0, 1, asc::ConstMemoryView(nullptr, 0, asc::MemorySpace::kHost));
  ASC_DENSE_TEST_CHECK(test, empty.ok());
  std::array<asc::index_t, 1> index_storage{};
  auto index = MakeVector(index_storage, 1);
  std::array<float, 1> scalar_storage{99.0F};
  std::array<double, 1> double_scalar_storage{99.0};
  auto scalar = MakeVector(scalar_storage, 1);
  auto double_scalar = MakeVector(double_scalar_storage, 1);
  if (empty.ok()) {
    const Vector<const float> const_empty(*empty);
    ASC_DENSE_TEST_CHECK(
        test, asc::Dot(context, const_empty, const_empty, scalar).ok());
    ASC_DENSE_TEST_EQ(test, scalar_storage[0], 0.0F);
    ASC_DENSE_TEST_CHECK(test, asc::Nrm2(context, const_empty, scalar).ok());
    ASC_DENSE_TEST_EQ(test, scalar_storage[0], 0.0F);
    scalar_storage[0] = 99.0F;
    ASC_DENSE_TEST_CHECK(test, asc::Asum(context, const_empty, scalar).ok());
    ASC_DENSE_TEST_EQ(test, scalar_storage[0], 0.0F);
    ASC_DENSE_TEST_CHECK(
        test, asc::Dot(context, 2.0F, const_empty, const_empty, scalar).ok());
    ASC_DENSE_TEST_EQ(test, scalar_storage[0], 2.0F);
    ASC_DENSE_TEST_CHECK(
        test, asc::Dot(context, asc::DenseBlasDotAccumulation::kDouble,
                       const_empty, const_empty, double_scalar)
                  .ok());
    ASC_DENSE_TEST_EQ(test, double_scalar_storage[0], 0.0);
    ASC_DENSE_TEST_CHECK(test, asc::Iamax(context, const_empty, index).ok());
    ASC_DENSE_TEST_EQ(test, index_storage[0], asc::index_t{-1});
  }
  CheckError(test,
             asc::Dot(context, static_cast<asc::DenseBlasDotAccumulation>(99),
                      Vector<const float>(source), Vector<const float>(source),
                      double_scalar),
             asc::ErrorCode::kInvalidArgument);
}

}  // namespace

int main() {
  TestContext test;
  TestDescriptorValidation(test);
  TestVectorUpdates<float>(test);
  TestVectorUpdates<double>(test);
  TestVectorUpdates<std::complex<float>>(test);
  TestVectorUpdates<std::complex<double>>(test);
  TestRealRotations<float>(test);
  TestRealRotations<double>(test);
  TestComplexRotationsAndScal<float>(test);
  TestComplexRotationsAndScal<double>(test);
  TestRealReductions<float>(test);
  TestRealReductions<double>(test);
  TestMixedDotProducts(test);
  TestExtremeValues<float>(test);
  TestExtremeValues<double>(test);
  TestComplexReductions<float>(test);
  TestComplexReductions<double>(test);
  TestFailuresAndEmpty(test);
  return test.Finish();
}
