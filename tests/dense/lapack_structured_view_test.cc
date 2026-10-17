#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <span>
#include <type_traits>

#include "../allocation_observation.h"
#include "allocation_probe.h"
#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/types.h"
#include "test_support.h"

namespace {

using asc_dense_test::TestContext;

template <typename Element>
asc::DenseBlasVectorView<Element> Vector(std::span<Element> values) {
  auto result = asc::DenseBlasVectorView<Element>::Create(
      values.data(), static_cast<asc::extent_t>(values.size()), 1,
      {values.data(), values.size_bytes(), asc::MemorySpace::kHost});
  if (!result.ok()) {
    std::abort();
  }
  return *result;
}

template <typename Element>
asc::DenseBlasMatrixView<const Element> Matrix(std::span<Element> values,
                                               asc::extent_t rows,
                                               asc::extent_t columns) {
  auto result = asc::DenseBlasMatrixView<const Element>::Create(
      values.data(), rows, columns, asc::DenseBlasLayout::kColumnMajor,
      std::max<asc::extent_t>(1, rows),
      {values.data(), values.size_bytes(), asc::MemorySpace::kHost});
  if (!result.ok()) {
    std::abort();
  }
  return *result;
}

void TestFactorBandCapacity(TestContext& test) {
  // For KL=2, KU=1, four input rows fit GBMV but GBTRF requires six rows.
  std::array<double, 30> storage{};
  const asc::ConstMemoryView backing(storage.data(), sizeof(storage),
                                     asc::MemorySpace::kHost);
  auto ordinary = asc::DenseBlasBandMatrixView<double>::Create(
      storage.data(), 5, 5, 2, 1, asc::DenseBlasLayout::kColumnMajor, 4,
      backing);
  ASC_DENSE_TEST_CHECK(test, ordinary.ok());
  auto inadequate = asc::LapackLuBandView<double>::Create(storage.data(), 5, 5,
                                                          2, 1, 4, backing);
  ASC_DENSE_TEST_CHECK(test, !inadequate.ok());
  auto factors = asc::LapackLuBandView<double>::Create(storage.data(), 5, 5, 2,
                                                       1, 6, backing);
  ASC_DENSE_TEST_CHECK(test, factors.ok());
  ASC_DENSE_TEST_EQ(test, factors->diagonal_row(), 3);
  ASC_DENSE_TEST_EQ(test, factors->storage().rows(), 6);
  ASC_DENSE_TEST_EQ(test, factors->storage().reachable_storage().size(),
                    sizeof(storage));
  ASC_DENSE_TEST_CHECK(test,
                       !asc::LapackLuBandView<double>::Create(
                            storage.data(), 5, 5, 2, 1, 6,
                            {storage.data(), sizeof(storage) - sizeof(double),
                             asc::MemorySpace::kHost})
                            .ok());
  ASC_DENSE_TEST_CHECK(
      test, !asc::LapackLuBandView<double>::Create(
                 storage.data(), 5, 5,
                 std::numeric_limits<asc::extent_t>::max(), 1, 6, backing)
                 .ok());
  ASC_DENSE_TEST_CHECK(
      test, !asc::LapackLuBandView<double>::Create(
                 storage.data(), 5, 5, 2, 1, 6,
                 {storage.data(), sizeof(storage), asc::MemorySpace::kDevice})
                 .ok());
  ASC_DENSE_TEST_CHECK(
      test, asc::LapackLuBandView<double>::Create(
                nullptr, 0, 0, 0, 0, 1, {nullptr, 0, asc::MemorySpace::kHost})
                .ok());
}

void TestPositiveDefiniteBand(TestContext& test) {
  std::array<std::complex<double>, 15> storage{};
  const asc::ConstMemoryView backing(storage.data(), sizeof(storage),
                                     asc::MemorySpace::kHost);
  auto upper =
      asc::LapackPositiveDefiniteBandView<std::complex<double>>::Create(
          storage.data(), 5, 2, asc::DenseBlasTriangle::kUpper, 3, backing);
  auto lower =
      asc::LapackPositiveDefiniteBandView<std::complex<double>>::Create(
          storage.data(), 5, 2, asc::DenseBlasTriangle::kLower, 3, backing);
  ASC_DENSE_TEST_CHECK(test, upper.ok() && lower.ok());
  ASC_DENSE_TEST_EQ(test, upper->diagonal_row(), 2);
  ASC_DENSE_TEST_EQ(test, lower->diagonal_row(), 0);
  ASC_DENSE_TEST_CHECK(
      test,
      !asc::LapackPositiveDefiniteBandView<std::complex<double>>::Create(
           storage.data(), 5, 2, asc::DenseBlasTriangle::kLower, 2, backing)
           .ok());
  // A fixed-underlying enum can contain 9; the descriptor must reject that tag.
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  const auto invalid_triangle = static_cast<asc::DenseBlasTriangle>(9);
  ASC_DENSE_TEST_CHECK(
      test, !asc::LapackPositiveDefiniteBandView<std::complex<double>>::Create(
                 storage.data(), 5, 2, invalid_triangle, 3, backing)
                 .ok());
}

template <typename T>
void TestPositiveBandLayouts(TestContext& test) {
  std::array<T, 40> values{};
  const asc::ConstMemoryView backing(values.data(), sizeof(values),
                                     asc::MemorySpace::kHost);
  for (auto layout :
       {asc::DenseBlasLayout::kColumnMajor, asc::DenseBlasLayout::kRowMajor}) {
    const bool column = layout == asc::DenseBlasLayout::kColumnMajor;
    for (auto triangle :
         {asc::DenseBlasTriangle::kUpper, asc::DenseBlasTriangle::kLower}) {
      for (asc::extent_t bandwidth : {0, 2, 5, 6}) {
        auto band = asc::LapackPositiveDefiniteBandView<T>::Create(
            values.data(), 5, bandwidth, triangle, layout, 8, backing);
        ASC_DENSE_TEST_CHECK(test, band.ok());
        if (!band.ok()) {
          continue;
        }
        ASC_DENSE_TEST_EQ(test, band->order(), 5);
        ASC_DENSE_TEST_EQ(test, band->bandwidth(), bandwidth);
        ASC_DENSE_TEST_EQ(test, band->layout(), layout);
        ASC_DENSE_TEST_EQ(test, band->triangle(), triangle);
        ASC_DENSE_TEST_EQ(test, band->storage().rows(), column ? 8 : 5);
        ASC_DENSE_TEST_EQ(test, band->storage().columns(), column ? 5 : 8);
        ASC_DENSE_TEST_EQ(test, band->storage().reachable_storage().size(),
                          sizeof(values));
        const bool upper = triangle == asc::DenseBlasTriangle::kUpper;
        ASC_DENSE_TEST_EQ(test, band->diagonal_row(),
                          upper == column ? bandwidth : 0);
        ASC_DENSE_TEST_CHECK(
            test, !asc::LapackPositiveDefiniteBandView<T>::Create(
                       values.data(), 5, bandwidth, triangle, layout, 8,
                       {values.data(), sizeof(values) - sizeof(T),
                        asc::MemorySpace::kHost})
                       .ok());
      }
      auto empty = asc::LapackPositiveDefiniteBandView<T>::Create(
          nullptr, 0, 5, triangle, layout, 6,
          {nullptr, 0, asc::MemorySpace::kHost});
      ASC_DENSE_TEST_CHECK(test, empty.ok());
      ASC_DENSE_TEST_EQ(test, empty->order(), 0);
      ASC_DENSE_TEST_EQ(test, empty->storage().reachable_storage().size(), 0U);
      ASC_DENSE_TEST_CHECK(
          test, asc::LapackPositiveDefiniteBandView<T>::Create(
                    values.data(), 1, 6, triangle, layout, 8, backing)
                    .ok());
    }
  }
}

void TestPositiveBandInvalid(TestContext& test) {
  std::array<double, 4> values{};
  const asc::ConstMemoryView backing(values.data(), sizeof(values),
                                     asc::MemorySpace::kHost);
  const auto upper = asc::DenseBlasTriangle::kUpper;
  const auto row = asc::DenseBlasLayout::kRowMajor;
  const auto maximum = std::numeric_limits<asc::extent_t>::max();
  for (auto layout : {asc::DenseBlasLayout::kColumnMajor, row}) {
    ASC_DENSE_TEST_CHECK(
        test, !asc::LapackPositiveDefiniteBandView<double>::Create(
                   values.data(), 1, maximum, upper, layout, maximum, backing)
                   .ok());
    ASC_DENSE_TEST_CHECK(
        test, !asc::LapackPositiveDefiniteBandView<double>::Create(
                   values.data(), maximum, 0, upper, layout, 2, backing)
                   .ok());
    ASC_DENSE_TEST_CHECK(test,
                         !asc::LapackPositiveDefiniteBandView<double>::Create(
                              values.data(), 1, 2, upper, layout, 2, backing)
                              .ok());
    ASC_DENSE_TEST_CHECK(test,
                         !asc::LapackPositiveDefiniteBandView<double>::Create(
                              nullptr, 1, 0, upper, layout, 1,
                              {nullptr, 0, asc::MemorySpace::kHost})
                              .ok());
  }
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  const auto invalid = static_cast<asc::DenseBlasLayout>(9);
  ASC_DENSE_TEST_CHECK(test,
                       !asc::LapackPositiveDefiniteBandView<double>::Create(
                            values.data(), 1, 0, upper, invalid, 1, backing)
                            .ok());
}

void TestTridiagonalStorage(TestContext& test) {
  std::array<double, 4> diagonal{1, 2, 3, 4};
  std::array<double, 3> lower{5, 6, 7};
  std::array<double, 3> upper{8, 9, 10};
  std::array<double, 2> fill{11, 12};
  auto primary = asc::LapackTridiagonalView<double>::Create(
      Vector<double>(lower), Vector<double>(diagonal), Vector<double>(upper));
  ASC_DENSE_TEST_CHECK(test, primary.ok());
  auto factors = asc::LapackTridiagonalLuStorage<double>::Create(
      *primary, Vector<double>(fill));
  ASC_DENSE_TEST_CHECK(test, factors.ok());
  ASC_DENSE_TEST_EQ(test, factors->second_upper().size(), 2);
  ASC_DENSE_TEST_CHECK(test,
                       !asc::LapackTridiagonalView<double>::Create(
                            Vector<double>(lower), Vector<double>(diagonal),
                            Vector<double>(lower))
                            .ok());
  ASC_DENSE_TEST_CHECK(
      test, !asc::LapackTridiagonalLuStorage<double>::Create(
                 *primary, Vector<double>(std::span<double>(upper).first(2)))
                 .ok());
  ASC_DENSE_TEST_CHECK(
      test, !asc::LapackTridiagonalLuStorage<double>::Create(
                 *primary, Vector<double>(std::span<double>(fill).first(1)))
                 .ok());
  auto reverse = asc::DenseBlasVectorView<double>::Create(
      lower.data() + 2, 3, -1,
      {lower.data(), sizeof(lower), asc::MemorySpace::kHost});
  ASC_DENSE_TEST_CHECK(test, reverse.ok());
  ASC_DENSE_TEST_CHECK(
      test, !asc::LapackTridiagonalView<double>::Create(
                 *reverse, Vector<double>(diagonal), Vector<double>(upper))
                 .ok());
  auto empty = asc::LapackTridiagonalView<double>::Create(
      Vector<double>({}), Vector<double>({}), Vector<double>({}));
  ASC_DENSE_TEST_CHECK(test, empty.ok());
  ASC_DENSE_TEST_CHECK(test, asc::LapackTridiagonalLuStorage<double>::Create(
                                 *empty, Vector<double>({}))
                                 .ok());
}

void TestMixedDiagonalAndBidiagonal(TestContext& test) {
  static_assert(
      std::is_same_v<asc::LapackRealElement<std::complex<double>>, double>);
  static_assert(
      std::is_same_v<asc::LapackRealElement<const std::complex<float>>,
                     const float>);
  std::array<double, 3> diagonal{1, 2, 3};
  std::array<std::complex<double>, 2> off_diagonal{};
  auto hermitian =
      asc::LapackPositiveDefiniteTridiagonalView<std::complex<double>>::Create(
          Vector<double>(diagonal), Vector<std::complex<double>>(off_diagonal));
  ASC_DENSE_TEST_CHECK(test, hermitian.ok());
  ASC_DENSE_TEST_EQ(test, hermitian->order(), 3);
  ASC_DENSE_TEST_CHECK(
      test,
      !asc::LapackPositiveDefiniteTridiagonalView<std::complex<double>>::Create(
           Vector<double>(diagonal),
           Vector<std::complex<double>>(
               std::span<std::complex<double>>(off_diagonal).first(1)))
           .ok());
  std::array<double, 2> real_off{};
  auto bidiagonal = asc::LapackBidiagonalView<double>::Create(
      Vector<double>(diagonal), Vector<double>(real_off),
      asc::DenseBlasTriangle::kUpper);
  ASC_DENSE_TEST_CHECK(test, bidiagonal.ok());
  ASC_DENSE_TEST_EQ(test, bidiagonal->triangle(),
                    asc::DenseBlasTriangle::kUpper);
  ASC_DENSE_TEST_CHECK(
      test, !asc::LapackBidiagonalView<double>::Create(
                 Vector<double>(diagonal),
                 Vector<double>(std::span<double>(diagonal).first(2)),
                 asc::DenseBlasTriangle::kLower)
                 .ok());
}

void TestRfpShapesAndModes(TestContext& test) {
  // Independently computed triangular numbers: 4*5/2=10 and 5*6/2=15.
  std::array<double, 10> even{};
  std::array<double, 15> odd{};
  auto even_normal = asc::LapackRfpView<double>::Create(
      Vector<double>(even), 4, asc::DenseBlasTriangle::kUpper,
      asc::LapackRfpOrientation::kNormal);
  auto odd_transpose = asc::LapackRfpView<double>::Create(
      Vector<double>(odd), 5, asc::DenseBlasTriangle::kLower,
      asc::LapackRfpOrientation::kTranspose);
  ASC_DENSE_TEST_CHECK(test, even_normal.ok() && odd_transpose.ok());
  ASC_DENSE_TEST_EQ(test, even_normal->physical_rows(), 5);
  ASC_DENSE_TEST_EQ(test, even_normal->physical_columns(), 2);
  ASC_DENSE_TEST_EQ(test, odd_transpose->physical_rows(), 3);
  ASC_DENSE_TEST_EQ(test, odd_transpose->physical_columns(), 5);
  ASC_DENSE_TEST_CHECK(
      test, !asc::LapackRfpView<double>::Create(
                 Vector<double>(even), 5, asc::DenseBlasTriangle::kUpper,
                 asc::LapackRfpOrientation::kNormal)
                 .ok());
  ASC_DENSE_TEST_CHECK(
      test, !asc::LapackRfpView<double>::Create(
                 Vector<double>(even), 4, asc::DenseBlasTriangle::kUpper,
                 asc::LapackRfpOrientation::kConjugateTranspose)
                 .ok());
  std::array<std::complex<double>, 10> complex_even{};
  auto complex_transpose = asc::LapackRfpView<std::complex<double>>::Create(
      Vector<std::complex<double>>(complex_even), 4,
      asc::DenseBlasTriangle::kLower,
      asc::LapackRfpOrientation::kConjugateTranspose);
  ASC_DENSE_TEST_CHECK(test, complex_transpose.ok());
  ASC_DENSE_TEST_EQ(test, complex_transpose->physical_rows(), 2);
  ASC_DENSE_TEST_EQ(test, complex_transpose->physical_columns(), 5);
  ASC_DENSE_TEST_CHECK(test, !asc::LapackRfpView<std::complex<double>>::Create(
                                  Vector<std::complex<double>>(complex_even), 4,
                                  asc::DenseBlasTriangle::kLower,
                                  asc::LapackRfpOrientation::kTranspose)
                                  .ok());
  ASC_DENSE_TEST_CHECK(
      test,
      !asc::LapackRfpView<double>::Create(
           Vector<double>({}), std::numeric_limits<asc::extent_t>::max(),
           asc::DenseBlasTriangle::kLower, asc::LapackRfpOrientation::kNormal)
           .ok());
  ASC_DENSE_TEST_CHECK(
      test, asc::LapackRfpView<double>::Create(
                Vector<double>({}), 0, asc::DenseBlasTriangle::kLower,
                asc::LapackRfpOrientation::kNormal)
                .ok());
}

void TestReusableFactorTags(TestContext& test) {
  std::array<double, 9> square{};
  std::array<double, 12> rectangular{};
  std::array<double, 3> tau{};
  asc::LapackReport report;
  report.outcome = asc::LapackOutcome::kSuccess;
  report.output_validity = asc::LapackOutputValidity::kComplete;
  report.factor_family = asc::LapackFactorFamily::kCholesky;
  const auto square_matrix = Matrix<double>(square, 3, 3);
  const auto tall_matrix = Matrix<double>(rectangular, 4, 3);
  ASC_DENSE_TEST_CHECK(
      test, asc::LapackCholeskyFactorView<double>::Create(
                square_matrix, asc::DenseBlasTriangle::kLower, report)
                .ok());
  ASC_DENSE_TEST_CHECK(test,
                       !asc::LapackCholeskyFactorView<double>::Create(
                            tall_matrix, asc::DenseBlasTriangle::kLower, report)
                            .ok());
  ASC_DENSE_TEST_CHECK(test,
                       !asc::LapackHouseholderQrFactorView<double>::Create(
                            tall_matrix, Vector<const double>(tau), report)
                            .ok());
  report.factor_family = asc::LapackFactorFamily::kHouseholderQr;
  auto qr = asc::LapackHouseholderQrFactorView<double>::Create(
      tall_matrix, Vector<const double>(tau), report);
  ASC_DENSE_TEST_CHECK(test, qr.ok());
  ASC_DENSE_TEST_CHECK(
      test, !asc::LapackHouseholderQrFactorView<double>::Create(
                 tall_matrix,
                 Vector<const double>(std::span<double>(rectangular).first(3)),
                 report)
                 .ok());
  report.output_validity = asc::LapackOutputValidity::kDocumentedPartial;
  ASC_DENSE_TEST_CHECK(test,
                       !asc::LapackHouseholderQrFactorView<double>::Create(
                            tall_matrix, Vector<const double>(tau), report)
                            .ok());
}

void TestNoAllocation(TestContext& test) {
  std::array<double, 3> diagonal{};
  std::array<double, 2> lower{};
  std::array<double, 2> upper{};
  const auto d = Vector<double>(diagonal);
  const auto dl = Vector<double>(lower);
  const auto du = Vector<double>(upper);
  std::size_t count = 0;
  {
    asc_dense_test::AllocationProbe probe;
    ASC_DENSE_TEST_CHECK(
        test, asc::LapackTridiagonalView<double>::Create(dl, d, du).ok());
    ASC_DENSE_TEST_CHECK(
        test, asc::LapackPositiveDefiniteBandView<double>::Create(
                  diagonal.data(), 3, 0, asc::DenseBlasTriangle::kUpper,
                  asc::DenseBlasLayout::kRowMajor, 1,
                  {diagonal.data(), sizeof(diagonal), asc::MemorySpace::kHost})
                  .ok());
    count = probe.count();
  }
  ASC_DENSE_TEST_CHECK(test, asc_test::ProcessAllocationCountMatches(count, 0));
}

}  // namespace

int main() {
  TestContext test;
  TestFactorBandCapacity(test);
  TestPositiveDefiniteBand(test);
  TestPositiveBandLayouts<float>(test);
  TestPositiveBandLayouts<double>(test);
  TestPositiveBandLayouts<std::complex<float>>(test);
  TestPositiveBandLayouts<std::complex<double>>(test);
  TestPositiveBandInvalid(test);
  TestTridiagonalStorage(test);
  TestMixedDiagonalAndBidiagonal(test);
  TestRfpShapesAndModes(test);
  TestReusableFactorTags(test);
  TestNoAllocation(test);
  return test.Finish();
}
