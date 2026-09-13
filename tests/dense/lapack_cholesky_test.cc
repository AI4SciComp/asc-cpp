#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <type_traits>
#include <utility>

#include "allocation_observation.h"
#include "allocation_probe.h"
#include "asc/core/contracts.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/cholesky.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "test_support.h"

namespace {

using asc_dense_test::TestContext;
using Wide = std::complex<long double>;
using Triangle = asc::DenseBlasTriangle;
using Layout = asc::DenseBlasLayout;
constexpr std::array kTriangles{Triangle::kLower, Triangle::kUpper};
constexpr std::array kLayouts{Layout::kColumnMajor, Layout::kRowMajor};
std::size_t g_reconstructions = 0;
std::size_t g_solves = 0;

template <typename T>
T Take(asc::Result<T> result) {
  ASC_CHECK(result.ok());
  return std::move(*result);
}

template <typename T>
T Narrow(Wide value) {
  using Real = asc::DenseBlasRealType<T>;
  if constexpr (asc::DenseBlasComplex<T>) {
    return {static_cast<Real>(value.real()), static_cast<Real>(value.imag())};
  } else {
    return static_cast<T>(value.real());
  }
}

template <typename T>
Wide Widen(T value) {
  if constexpr (asc::DenseBlasComplex<T>) {
    return {value.real(), value.imag()};
  } else {
    return {value, 0};
  }
}

template <typename T>
T NotANumber() {
  const auto nan = std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN();
  return Narrow<T>({nan, nan});
}

template <typename T>
bool SameBits(T left, T right) {
  if constexpr (asc::DenseBlasComplex<T>) {
    return SameBits(left.real(), right.real()) &&
           SameBits(left.imag(), right.imag());
  } else {
    using Bits = std::conditional_t<sizeof(T) == sizeof(std::uint32_t),
                                    std::uint32_t, std::uint64_t>;
    return std::bit_cast<Bits>(left) == std::bit_cast<Bits>(right);
  }
}

template <typename T>
class Matrix {
 public:
  Matrix(asc::extent_t rows, asc::extent_t columns, Layout layout)
      : rows_(rows),
        columns_(columns),
        layout_(layout),
        leading_((layout == Layout::kRowMajor ? columns : rows) + 2) {
    storage_.fill(Narrow<T>({-731, 19}));
  }
  [[nodiscard]] std::size_t Offset(std::size_t row, std::size_t column) const {
    const auto leading = static_cast<std::size_t>(leading_);
    return 3 + (layout_ == Layout::kRowMajor ? row * leading + column
                                             : column * leading + row);
  }
  T& operator()(std::size_t row, std::size_t column) {
    return storage_[Offset(row, column)];
  }
  const T& operator()(std::size_t row, std::size_t column) const {
    return storage_[Offset(row, column)];
  }
  asc::DenseBlasMatrixView<T> view(
      asc::MemorySpace placement = asc::MemorySpace::kHost) {
    return Take(asc::DenseBlasMatrixView<T>::Create(
        storage_.data() + 3, rows_, columns_, layout_, leading_,
        {storage_.data(), sizeof(storage_), placement}));
  }
  [[nodiscard]] asc::DenseBlasMatrixView<const T> const_view() const {
    return Take(asc::DenseBlasMatrixView<const T>::Create(
        storage_.data() + 3, rows_, columns_, layout_, leading_,
        {storage_.data(), sizeof(storage_), asc::MemorySpace::kHost}));
  }
  [[nodiscard]] const std::array<T, 96>& bytes() const { return storage_; }
  void CheckUntouched(TestContext& test, const std::array<T, 96>& before,
                      Triangle triangle, bool all_logical = false) const {
    std::array<bool, 96> writable{};
    for (std::size_t row = 0; row < static_cast<std::size_t>(rows_); ++row) {
      for (std::size_t col = 0; col < static_cast<std::size_t>(columns_);
           ++col) {
        if (all_logical ||
            (triangle == Triangle::kLower ? row >= col : row <= col)) {
          writable[Offset(row, col)] = true;
        }
      }
    }
    for (std::size_t i = 0; i < before.size(); ++i) {
      if (!writable[i]) {
        ASC_DENSE_TEST_CHECK(test, SameBits(storage_[i], before[i]));
      }
    }
  }

 private:
  asc::extent_t rows_;
  asc::extent_t columns_;
  Layout layout_;
  asc::stride_t leading_;
  std::array<T, 96> storage_{};
};

// Independent row-oriented fixtures. The complex lower factor is
// [[2,0,0],[1+i,3,0],[-1+2i,2-i,2]], giving the explicit Hermitian table.
template <typename T>
Wide Coefficient(std::size_t row, std::size_t column) {
  if constexpr (asc::DenseBlasComplex<T>) {
    constexpr std::array<Wide, 9> kA{Wide{4, 0},  Wide{2, -2}, Wide{-2, -4},
                                     Wide{2, 2},  Wide{11, 0}, Wide{7, 0},
                                     Wide{-2, 4}, Wide{7, 0},  Wide{14, 0}};
    return kA[row * 3 + column];
  } else {
    constexpr std::array<int, 9> kA{4, 2, -2, 2, 10, 5, -2, 5, 9};
    return {static_cast<long double>(kA[row * 3 + column]), 0};
  }
}

template <typename T>
T ScaledCoefficient(std::size_t row, std::size_t column,
                    asc::DenseBlasRealType<T> scale) {
  return Narrow<T>(Coefficient<T>(row, column) *
                   static_cast<long double>(scale));
}

template <typename T>
Wide LowerOracle(std::size_t row, std::size_t column) {
  constexpr std::array<Wide, 9> kL{Wide{2, 0},  Wide{0, 0},  Wide{0, 0},
                                   Wide{1, 1},  Wide{3, 0},  Wide{0, 0},
                                   Wide{-1, 2}, Wide{2, -1}, Wide{2, 0}};
  const auto value = kL[row * 3 + column];
  return asc::DenseBlasComplex<T> ? value : Wide{value.real(), 0};
}

template <typename T>
Wide Solution(std::size_t row, std::size_t column) {
  constexpr std::array<Wide, 9> kX{Wide{1, 1},  Wide{2, -1},  Wide{-1, 0},
                                   Wide{-1, 2}, Wide{0, 1},   Wide{2, -1},
                                   Wide{2, 0},  Wide{-1, -1}, Wide{1, 2}};
  const auto value = kX[row * 3 + column];
  return asc::DenseBlasComplex<T> ? value : Wide{value.real(), 0};
}

void CheckReport(TestContext& test, const asc::LapackReport& report,
                 asc::LapackOutcome outcome,
                 asc::LapackOutputValidity validity) {
  ASC_DENSE_TEST_EQ(test, report.provider, asc::LapackProviderIdentity{});
  ASC_DENSE_TEST_CHECK(test, !report.called_provider);
  ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
  ASC_DENSE_TEST_CHECK(test, !report.native_argument.has_value());
  ASC_DENSE_TEST_EQ(test, report.outcome, outcome);
  ASC_DENSE_TEST_EQ(test, report.output_validity, validity);
  ASC_DENSE_TEST_CHECK(test, report.routine[0] != '\0');
}

void CheckRatio(TestContext& test, long double numerator,
                long double denominator, long double tolerance) {
  long double ratio = 0;
  if (denominator != 0) {
    ratio = numerator / denominator;
  } else if (numerator != 0) {
    ratio = std::numeric_limits<long double>::infinity();
  }
  ASC_DENSE_TEST_CHECK(test, std::isfinite(ratio) && ratio <= tolerance);
}

template <typename T>
Wide LowerFactor(const Matrix<T>& factor, Triangle triangle, std::size_t row,
                 std::size_t column) {
  if (row < column) {
    return {};
  }
  if (triangle == Triangle::kLower) {
    return Widen(factor(row, column));
  }
  const auto stored_row = column;
  const auto stored_column = row;
  return std::conj(Widen(factor(stored_row, stored_column)));
}

template <typename T>
void CheckFactor(TestContext& test, const Matrix<T>& factor, Triangle triangle,
                 asc::DenseBlasRealType<T> scale) {
  using Real = asc::DenseBlasRealType<T>;
  const long double tolerance = 256 * std::numeric_limits<Real>::epsilon();
  long double error = 0;
  long double norm = 0;
  const long double root = std::sqrt(static_cast<long double>(scale));
  for (std::size_t row = 0; row < 3; ++row) {
    long double row_error = 0;
    long double row_norm = 0;
    for (std::size_t column = 0; column < 3; ++column) {
      Wide reconstructed{};
      for (std::size_t k = 0; k < 3; ++k) {
        reconstructed += LowerFactor(factor, triangle, row, k) *
                         std::conj(LowerFactor(factor, triangle, column, k));
      }
      const Wide expected = Widen(ScaledCoefficient<T>(row, column, scale));
      row_error += std::abs(reconstructed - expected);
      row_norm += std::abs(expected);
      CheckRatio(test,
                 std::abs(LowerFactor(factor, triangle, row, column) -
                          LowerOracle<T>(row, column) * root),
                 root, tolerance);
    }
    error = std::max(error, row_error);
    norm = std::max(norm, row_norm);
    const auto diagonal = Widen(factor(row, row));
    ASC_DENSE_TEST_CHECK(test, diagonal.real() > 0 && diagonal.imag() == 0);
  }
  CheckRatio(test, error, norm, tolerance);
  ++g_reconstructions;
}

template <typename T>
void CheckSolution(TestContext& test, const Matrix<T>& rhs,
                   const std::array<Wide, 9>& original_rhs, std::size_t nrhs,
                   asc::DenseBlasRealType<T> scale) {
  long double residual_norm = 0;
  long double a_norm = 0;
  long double x_norm = 0;
  long double b_norm = 0;
  const long double tolerance =
      512 * std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon();
  for (std::size_t row = 0; row < 3; ++row) {
    long double ar = 0;
    long double xr = 0;
    long double br = 0;
    long double rr = 0;
    for (std::size_t k = 0; k < 3; ++k) {
      ar += std::abs(Widen(ScaledCoefficient<T>(row, k, scale)));
    }
    for (std::size_t column = 0; column < nrhs; ++column) {
      Wide product{};
      for (std::size_t k = 0; k < 3; ++k) {
        product +=
            Widen(ScaledCoefficient<T>(row, k, scale)) * Widen(rhs(k, column));
      }
      rr += std::abs(product - original_rhs[row * 3 + column]);
      xr += std::abs(Widen(rhs(row, column)));
      br += std::abs(original_rhs[row * 3 + column]);
      ASC_DENSE_TEST_CHECK(
          test, std::abs(Widen(rhs(row, column)) - Solution<T>(row, column)) <=
                    tolerance * 4);
    }
    residual_norm = std::max(residual_norm, rr);
    a_norm = std::max(a_norm, ar);
    x_norm = std::max(x_norm, xr);
    b_norm = std::max(b_norm, br);
  }
  CheckRatio(test, residual_norm, a_norm * x_norm + b_norm, tolerance);
}

template <typename T>
void ExerciseSolve(TestContext& test, const Matrix<T>& factors,
                   asc::LapackCholeskyFactorView<T> factor, Layout layout,
                   std::size_t nrhs, asc::DenseBlasRealType<T> scale) {
  Matrix<T> rhs(3, static_cast<asc::extent_t>(nrhs), layout);
  std::array<Wide, 9> original_rhs{};
  for (std::size_t row = 0; row < 3; ++row) {
    for (std::size_t column = 0; column < nrhs; ++column) {
      Wide value{};
      for (std::size_t k = 0; k < 3; ++k) {
        value +=
            Widen(ScaledCoefficient<T>(row, k, scale)) * Solution<T>(k, column);
      }
      rhs(row, column) = Narrow<T>(value);
      original_rhs[row * 3 + column] = Widen(rhs(row, column));
    }
  }
  const auto before = rhs.bytes();
  const auto original_factor = factors.bytes();
  asc::LapackReport report;
  asc::Status status;
  std::size_t allocations = 0;
  {
    asc_dense_test::AllocationProbe probe;
    status =
        asc::Potrs(asc::ExecutionContext::Serial(), factor, rhs.view(), report);
    allocations = probe.count();
  }
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_CHECK(test,
                       asc_test::ProcessAllocationCountMatches(allocations, 0));
  CheckReport(test, report, asc::LapackOutcome::kSuccess,
              asc::LapackOutputValidity::kComplete);
  CheckSolution(test, rhs, original_rhs, nrhs, scale);
  rhs.CheckUntouched(test, before, Triangle::kLower, true);
  for (std::size_t i = 0; i < original_factor.size(); ++i) {
    ASC_DENSE_TEST_CHECK(test,
                         SameBits(original_factor[i], factors.bytes()[i]));
  }
  ++g_solves;
}

template <typename T>
void ExerciseFactor(TestContext& test, Layout layout, Triangle triangle,
                    asc::DenseBlasRealType<T> scale) {
  Matrix<T> matrix(3, 3, layout);
  for (std::size_t row = 0; row < 3; ++row) {
    for (std::size_t column = 0; column < 3; ++column) {
      const bool selected =
          triangle == Triangle::kLower ? row >= column : row <= column;
      matrix(row, column) =
          selected ? ScaledCoefficient<T>(row, column, scale) : NotANumber<T>();
    }
    if constexpr (asc::DenseBlasComplex<T>) {
      matrix(row, row).imag(
          std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN());
    }
  }
  const auto before = matrix.bytes();
  asc::LapackReport report;
  asc::Status status;
  std::size_t allocations = 0;
  {
    asc_dense_test::AllocationProbe probe;
    status = asc::Potrf(asc::ExecutionContext::Serial(), triangle,
                        matrix.view(), report);
    allocations = probe.count();
  }
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_CHECK(test,
                       asc_test::ProcessAllocationCountMatches(allocations, 0));
  CheckReport(test, report, asc::LapackOutcome::kSuccess,
              asc::LapackOutputValidity::kComplete);
  ASC_DENSE_TEST_CHECK(
      test, report.factor_family == asc::LapackFactorFamily::kCholesky);
  ASC_DENSE_TEST_CHECK(test, !report.diagnostic_index.has_value());
  matrix.CheckUntouched(test, before, triangle);
  CheckFactor(test, matrix, triangle, scale);
  const auto factor = Take(asc::LapackCholeskyFactorView<T>::Create(
      matrix.const_view(), triangle, report));
  for (const auto rhs_layout : kLayouts) {
    for (const std::size_t nrhs : {std::size_t{1}, std::size_t{3}}) {
      ExerciseSolve(test, matrix, factor, rhs_layout, nrhs, scale);
    }
  }
}

template <typename T>
void TestPositiveFactors(TestContext& test) {
  using Real = asc::DenseBlasRealType<T>;
  constexpr int kExponent = sizeof(Real) == sizeof(float) ? 100 : 900;
  const std::array<Real, 3> scales{std::ldexp(Real{1}, -kExponent), Real{1},
                                   std::ldexp(Real{1}, kExponent)};
  for (const auto layout : kLayouts) {
    for (const auto triangle : kTriangles) {
      for (const auto scale : scales) {
        ExerciseFactor<T>(test, layout, triangle, scale);
      }
    }
  }
}

template <typename T>
void TestFailure(TestContext& test) {
  for (const auto layout : kLayouts) {
    for (const auto triangle : kTriangles) {
      for (const auto failure :
           {0.0L, -1.0L, std::numeric_limits<long double>::quiet_NaN()}) {
        Matrix<T> matrix(3, 3, layout);
        for (std::size_t row = 0; row < 3; ++row) {
          for (std::size_t column = 0; column < 3; ++column) {
            const bool selected =
                triangle == Triangle::kLower ? row >= column : row <= column;
            matrix(row, column) = selected ? T{0} : NotANumber<T>();
          }
          matrix(row, row) = Narrow<T>({row == 2 ? failure : 4.0L, 0});
        }
        const auto before = matrix.bytes();
        asc::LapackReport report;
        const auto status = asc::Potrf(asc::ExecutionContext::Serial(),
                                       triangle, matrix.view(), report);
        ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
        CheckReport(test, report, asc::LapackOutcome::kNotPositiveDefinite,
                    asc::LapackOutputValidity::kDocumentedPartial);
        ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-1),
                          asc::index_t{2});
        ASC_DENSE_TEST_EQ(test, Widen(matrix(0, 0)), Wide(2, 0));
        ASC_DENSE_TEST_EQ(test, Widen(matrix(1, 1)), Wide(2, 0));
        ASC_DENSE_TEST_EQ(test, Widen(matrix(2, 2)).imag(), 0.0L);
        ASC_DENSE_TEST_CHECK(test, std::isnan(failure)
                                       ? std::isnan(Widen(matrix(2, 2)).real())
                                       : Widen(matrix(2, 2)).real() == failure);
        ASC_DENSE_TEST_CHECK(test, !asc::LapackCholeskyFactorView<T>::Create(
                                        matrix.const_view(), triangle, report)
                                        .ok());
        matrix.CheckUntouched(test, before, triangle);
      }
    }
  }
}

template <typename T>
void TestZeroAndScalar(TestContext& test) {
  using Real = asc::DenseBlasRealType<T>;
  for (const auto layout : kLayouts) {
    for (const auto triangle : kTriangles) {
      Matrix<T> empty(0, 0, layout);
      asc::LapackReport report;
      ASC_DENSE_TEST_CHECK(test, asc::Potrf(asc::ExecutionContext::Serial(),
                                            triangle, empty.view(), report)
                                     .ok());
      const auto factor = Take(asc::LapackCholeskyFactorView<T>::Create(
          empty.const_view(), triangle, report));
      Matrix<T> empty_rhs(0, 3, layout);
      ASC_DENSE_TEST_CHECK(test, asc::Potrs(asc::ExecutionContext::Serial(),
                                            factor, empty_rhs.view(), report)
                                     .ok());
      for (const Real value : {std::numeric_limits<Real>::denorm_min(), Real{4},
                               std::numeric_limits<Real>::max() / Real{4}}) {
        Matrix<T> scalar(1, 1, layout);
        scalar(0, 0) = T{value};
        ASC_DENSE_TEST_CHECK(test, asc::Potrf(asc::ExecutionContext::Serial(),
                                              triangle, scalar.view(), report)
                                       .ok());
        const Wide root = Widen(scalar(0, 0));
        CheckRatio(test, std::abs(root * root - Wide(value, 0)), value,
                   16 * std::numeric_limits<Real>::epsilon());
      }
    }
  }
}

template <typename T>
void TestInvalidAndReuse(TestContext& test) {
  Matrix<T> matrix(2, 2, Layout::kRowMajor);
  matrix(0, 0) = T{4};
  matrix(0, 1) = T{2};
  matrix(1, 0) = T{2};
  matrix(1, 1) = T{10};
  const auto before = matrix.bytes();
  asc::LapackReport report;
  for (const auto space :
       {asc::MemorySpace::kDevice, asc::MemorySpace::kManaged,
        asc::MemorySpace::kPinnedHost}) {
    auto status = asc::Potrf(asc::ExecutionContext::Serial(), Triangle::kLower,
                             matrix.view(space), report);
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kMemoryAccess);
    CheckReport(test, report, asc::LapackOutcome::kNotRun,
                asc::LapackOutputValidity::kUnchanged);
  }
  // Nonnamed fixed-underlying value is intentional malformed-option input.
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  const auto invalid_triangle = static_cast<Triangle>(92);
  ASC_DENSE_TEST_EQ(test,
                    asc::Potrf(asc::ExecutionContext::Serial(),
                               invalid_triangle, matrix.view(), report)
                        .code(),
                    asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, matrix.bytes(), before);
  Matrix<T> rectangular(2, 3, Layout::kColumnMajor);
  ASC_DENSE_TEST_EQ(test,
                    asc::Potrf(asc::ExecutionContext::Serial(),
                               Triangle::kUpper, rectangular.view(), report)
                        .code(),
                    asc::ErrorCode::kShape);
  ASC_DENSE_TEST_CHECK(test, asc::Potrf(asc::ExecutionContext::Serial(),
                                        Triangle::kLower, matrix.view(), report)
                                 .ok());
  const auto factor = Take(asc::LapackCholeskyFactorView<T>::Create(
      matrix.const_view(), Triangle::kLower, report));
  ASC_DENSE_TEST_EQ(
      test,
      asc::Potrs(asc::ExecutionContext::Serial(), factor, matrix.view(), report)
          .code(),
      asc::ErrorCode::kInvalidArgument);
  Matrix<T> rhs(2, 1, Layout::kColumnMajor);
  rhs(0, 0) = T{1};
  rhs(1, 0) = T{2};
  const auto rhs_before = rhs.bytes();
  matrix(1, 1) = T{0};  // Deliberately invalidate borrowed factor contents.
  ASC_DENSE_TEST_EQ(
      test,
      asc::Potrs(asc::ExecutionContext::Serial(), factor, rhs.view(), report)
          .code(),
      asc::ErrorCode::kNumerical);
  CheckReport(test, report, asc::LapackOutcome::kNotPositiveDefinite,
              asc::LapackOutputValidity::kUnchanged);
  ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-1),
                    asc::index_t{1});
  ASC_DENSE_TEST_EQ(test, rhs.bytes(), rhs_before);
  Matrix<T> no_rhs(2, 0, Layout::kColumnMajor);
  ASC_DENSE_TEST_CHECK(test, asc::Potrs(asc::ExecutionContext::Serial(), factor,
                                        no_rhs.view(), report)
                                 .ok());
}

template <typename T>
void TestSolveRejections(TestContext& test) {
  Matrix<T> matrix(2, 2, Layout::kColumnMajor);
  matrix(0, 0) = T{1};
  matrix(1, 0) = T{0};
  matrix(0, 1) = NotANumber<T>();
  matrix(1, 1) = T{1};
  asc::LapackReport factor_report;
  ASC_CHECK(asc::Potrf(asc::ExecutionContext::Serial(), Triangle::kLower,
                       matrix.view(), factor_report)
                .ok());
  const auto factor = Take(asc::LapackCholeskyFactorView<T>::Create(
      matrix.const_view(), Triangle::kLower, factor_report));
  Matrix<T> rhs(2, 1, Layout::kRowMajor);
  rhs(0, 0) = T{2};
  rhs(1, 0) = T{3};
  const auto saved = rhs.bytes();
  asc::LapackReport report = factor_report;
  for (const auto placement :
       {asc::MemorySpace::kDevice, asc::MemorySpace::kManaged,
        asc::MemorySpace::kPinnedHost}) {
    asc_dense_test::AllocationProbe probe;
    const auto status = asc::Potrs(asc::ExecutionContext::Serial(), factor,
                                   rhs.view(placement), report);
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kMemoryAccess);
    ASC_DENSE_TEST_CHECK(
        test, asc_test::ProcessAllocationCountMatches(probe.count(), 0));
    ASC_DENSE_TEST_EQ(test, rhs.bytes(), saved);
  }
  Matrix<T> wrong_rows(3, 1, Layout::kRowMajor);
  const auto wrong_saved = wrong_rows.bytes();
  ASC_DENSE_TEST_EQ(test,
                    asc::Potrs(asc::ExecutionContext::Serial(), factor,
                               wrong_rows.view(), report)
                        .code(),
                    asc::ErrorCode::kShape);
  ASC_DENSE_TEST_EQ(test, wrong_rows.bytes(), wrong_saved);
  factor_report.provider.kind = asc::LapackProviderKind::kReference;
  const auto foreign = Take(asc::LapackCholeskyFactorView<T>::Create(
      matrix.const_view(), Triangle::kLower, factor_report));
  ASC_DENSE_TEST_EQ(
      test,
      asc::Potrs(asc::ExecutionContext::Serial(), foreign, rhs.view(), report)
          .code(),
      asc::ErrorCode::kInvalidArgument);
  CheckReport(test, report, asc::LapackOutcome::kNotRun,
              asc::LapackOutputValidity::kUnchanged);
  ASC_DENSE_TEST_EQ(test, rhs.bytes(), saved);
  factor_report.factor_family = asc::LapackFactorFamily::kLuPartialPivot;
  ASC_DENSE_TEST_EQ(test,
                    asc::LapackCholeskyFactorView<T>::Create(
                        matrix.const_view(), Triangle::kLower, factor_report)
                        .status()
                        .code(),
                    asc::ErrorCode::kInvalidState);
}

template <typename T>
void TestInvalidFactorDiagonal(TestContext& test) {
  using Real = asc::DenseBlasRealType<T>;
  Matrix<T> matrix(1, 1, Layout::kColumnMajor);
  matrix(0, 0) = T{1};
  asc::LapackReport report;
  ASC_CHECK(asc::Potrf(asc::ExecutionContext::Serial(), Triangle::kUpper,
                       matrix.view(), report)
                .ok());
  const auto factor = Take(asc::LapackCholeskyFactorView<T>::Create(
      matrix.const_view(), Triangle::kUpper, report));
  Matrix<T> rhs(1, 1, Layout::kRowMajor);
  rhs(0, 0) = T{3};
  const auto saved = rhs.bytes();
  for (const Real invalid :
       {Real{0}, Real{-1}, std::numeric_limits<Real>::infinity(),
        std::numeric_limits<Real>::quiet_NaN()}) {
    matrix(0, 0) = T{invalid};  // Deliberate invalidation, tested defensively.
    asc_dense_test::AllocationProbe probe;
    ASC_DENSE_TEST_EQ(
        test,
        asc::Potrs(asc::ExecutionContext::Serial(), factor, rhs.view(), report)
            .code(),
        asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_CHECK(
        test, asc_test::ProcessAllocationCountMatches(probe.count(), 0));
    CheckReport(test, report, asc::LapackOutcome::kNotPositiveDefinite,
                asc::LapackOutputValidity::kUnchanged);
    ASC_DENSE_TEST_EQ(test, rhs.bytes(), saved);
  }
  if constexpr (asc::DenseBlasComplex<T>) {
    matrix(0, 0) = T{1, 1};
    ASC_DENSE_TEST_EQ(
        test,
        asc::Potrs(asc::ExecutionContext::Serial(), factor, rhs.view(), report)
            .code(),
        asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_EQ(test, rhs.bytes(), saved);
  }
}

template <typename T>
void TestScalarFailures(TestContext& test) {
  for (const auto triangle : kTriangles) {
    for (const auto layout : kLayouts) {
      for (const T value : {T{0}, T{-1}, NotANumber<T>()}) {
        Matrix<T> scalar(1, 1, layout);
        scalar(0, 0) = value;
        asc::LapackReport report;
        asc_dense_test::AllocationProbe probe;
        const auto status = asc::Potrf(asc::ExecutionContext::Serial(),
                                       triangle, scalar.view(), report);
        ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
        ASC_DENSE_TEST_CHECK(
            test, asc_test::ProcessAllocationCountMatches(probe.count(), 0));
        CheckReport(test, report, asc::LapackOutcome::kNotPositiveDefinite,
                    asc::LapackOutputValidity::kDocumentedPartial);
        ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-1),
                          asc::index_t{0});
      }
    }
  }
}

template <typename T>
void TestType(TestContext& test) {
  TestPositiveFactors<T>(test);
  TestFailure<T>(test);
  TestZeroAndScalar<T>(test);
  TestInvalidAndReuse<T>(test);
  TestSolveRejections<T>(test);
  TestInvalidFactorDiagonal<T>(test);
  TestScalarFailures<T>(test);
}

}  // namespace

int main() {
  TestContext test;
  TestType<float>(test);
  TestType<double>(test);
  TestType<std::complex<float>>(test);
  TestType<std::complex<double>>(test);
  std::cout << "native Cholesky reconstructions=" << g_reconstructions
            << " solve residuals=" << g_solves << '\n';
  return test.Finish();
}
