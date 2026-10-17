#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <span>
#include <utility>

#include "allocation_observation.h"
#include "allocation_probe.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/cholesky.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/lu.h"
#include "asc/dense/lapack/qr.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "test_support.h"

namespace {
using asc_dense_test::TestContext;
using Layout = asc::DenseBlasLayout;
using Op = asc::DenseBlasTranspose;
using Triangle = asc::DenseBlasTriangle;
using Wide = std::complex<long double>;
constexpr std::array kLayouts{Layout::kRowMajor, Layout::kColumnMajor};
constexpr std::array kOperations{Op::kNone, Op::kTranspose,
                                 Op::kConjugateTranspose};
constexpr std::array kTriangles{Triangle::kLower, Triangle::kUpper};
constexpr auto kHost = asc::MemorySpace::kHost;
std::size_t g_lu_modes = 0;
std::size_t g_cholesky_modes = 0;
std::size_t g_qr_modes = 0;

template <typename T>
T Take(asc::Result<T> result) {
  if (!result.ok()) {
    std::abort();
  }
  return std::move(*result);
}

template <typename T>
T Scalar(long double real, long double imaginary = 0) {
  using Real = asc::DenseBlasRealType<T>;
  if constexpr (asc::DenseBlasComplex<T>) {
    return {static_cast<Real>(real), static_cast<Real>(imaginary)};
  } else {
    return static_cast<T>(real);
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
struct Matrix {
  std::array<T, 40> values;
  asc::extent_t rows;
  asc::extent_t columns;
  Layout layout;
  asc::stride_t leading;

  Matrix(asc::extent_t m, asc::extent_t n, Layout order)
      : rows(m),
        columns(n),
        layout(order),
        leading((order == Layout::kRowMajor ? n : m) + 2) {
    values.fill(Scalar<T>(-79, 31));
  }
  [[nodiscard]] std::size_t Offset(asc::index_t i, asc::index_t j) const {
    return 3 + static_cast<std::size_t>(layout == Layout::kRowMajor
                                            ? i * leading + j
                                            : j * leading + i);
  }
  T& At(asc::index_t i, asc::index_t j) { return values[Offset(i, j)]; }
  auto View(asc::MemorySpace space = kHost) {
    return Take(asc::DenseBlasMatrixView<T>::Create(
        values.data() + 3, rows, columns, layout, leading,
        {values.data(), sizeof(values), space}));
  }
  [[nodiscard]] auto ConstView() const {
    return Take(asc::DenseBlasMatrixView<const T>::Create(
        values.data() + 3, rows, columns, layout, leading,
        {values.data(), sizeof(values), kHost}));
  }
  void CheckPadding(TestContext& test, const std::array<T, 40>& before) const {
    std::array<bool, 40> logical{};
    for (asc::index_t i = 0; i < rows; ++i) {
      for (asc::index_t j = 0; j < columns; ++j) {
        logical[Offset(i, j)] = true;
      }
    }
    for (std::size_t i = 0; i < values.size(); ++i) {
      if (!logical[i]) {
        ASC_DENSE_TEST_EQ(test, values[i], before[i]);
      }
    }
  }
};

template <typename T, std::size_t N>
auto Vector(std::array<T, N>& values, asc::extent_t size) {
  return Take(asc::DenseBlasVectorView<T>::Create(
      values.data() + 1, size, 1, {values.data(), sizeof(values), kHost}));
}

template <typename Function>
asc::Status Observe(TestContext& test, Function function) {
  asc::Status status;
  std::size_t count = 0;
  {
    const asc_dense_test::AllocationProbe probe;
    status = function();
    count = probe.count();
  }
  ASC_DENSE_TEST_CHECK(test, asc_test::ProcessAllocationCountMatches(count, 0));
  return status;
}

void Report(
    TestContext& test, const asc::LapackReport& report,
    asc::LapackOutcome outcome = asc::LapackOutcome::kSuccess,
    asc::LapackOutputValidity validity = asc::LapackOutputValidity::kComplete) {
  ASC_DENSE_TEST_EQ(test, report.provider, asc::LapackProviderIdentity{});
  ASC_DENSE_TEST_CHECK(test, !report.called_provider);
  ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
  ASC_DENSE_TEST_CHECK(test, !report.native_argument.has_value());
  ASC_DENSE_TEST_EQ(test, report.outcome, outcome);
  ASC_DENSE_TEST_EQ(test, report.output_validity, validity);
}

template <typename T>
void KnownRhs(Matrix<T>& rhs, T coefficient) {
  for (asc::index_t j = 0; j < rhs.columns; ++j) {
    const auto x = Scalar<T>(j == 0 ? 1 : -2, j == 0 ? -1 : 1);
    const auto b = Widen(coefficient) * Widen(x);
    rhs.At(0, j) = Scalar<T>(b.real(), b.imag());
  }
}

template <typename T>
void CheckSolution(TestContext& test, Matrix<T>& rhs) {
  const long double tolerance =
      64 * std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon();
  for (asc::index_t j = 0; j < rhs.columns; ++j) {
    const auto expected = Widen(Scalar<T>(j == 0 ? 1 : -2, j == 0 ? -1 : 1));
    const auto error = std::abs(Widen(rhs.At(0, j)) - expected);
    ASC_DENSE_TEST_CHECK(test, std::isfinite(error));
    ASC_DENSE_TEST_CHECK(test, error <= tolerance * std::abs(expected));
  }
}

template <typename T, typename Function>
void StructuralRejections(TestContext& test, Matrix<T>& rhs,
                          asc::LapackReport& report, Function execute) {
  const auto before = rhs.values;
  for (const auto space :
       {asc::MemorySpace::kDevice, asc::MemorySpace::kManaged,
        asc::MemorySpace::kPinnedHost}) {
    report.called_provider = true;
    report.native_info = 73;
    const auto status = Observe(test, [&] { return execute(rhs.View(space)); });
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kMemoryAccess);
    Report(test, report, asc::LapackOutcome::kNotRun,
           asc::LapackOutputValidity::kUnchanged);
    ASC_DENSE_TEST_EQ(test, rhs.values, before);
  }
  Matrix<T> wrong(2, 1, rhs.layout);
  const auto wrong_before = wrong.values;
  const auto status = Observe(test, [&] { return execute(wrong.View()); });
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kShape);
  ASC_DENSE_TEST_EQ(test, wrong.values, wrong_before);
  Report(test, report, asc::LapackOutcome::kNotRun,
         asc::LapackOutputValidity::kUnchanged);
}

template <typename T, typename Function>
void EmptySolve(TestContext& test, Layout layout, asc::LapackReport& report,
                Function execute) {
  Matrix<T> empty(1, 0, layout);
  const auto before = empty.values;
  ASC_DENSE_TEST_CHECK(
      test, Observe(test, [&] { return execute(empty.View()); }).ok());
  Report(test, report);
  ASC_DENSE_TEST_EQ(test, empty.values, before);
}

template <typename T>
void EmptyLuMatrix(TestContext& test, Layout layout, Layout rhs_layout,
                   Op operation) {
  Matrix<T> matrix(0, 0, layout);
  Matrix<T> rhs(0, 3, rhs_layout);
  std::array<asc::index_t, 3> pivots{-31, -37, -41};
  const auto matrix_before = matrix.values;
  const auto rhs_before = rhs.values;
  const auto pivots_before = pivots;
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, Observe(test, [&] {
                               return asc::Getrf(
                                   asc::ExecutionContext::Serial(),
                                   matrix.View(), Vector(pivots, 0), report);
                             }).ok());
  Report(test, report);
  const auto raw = Take(asc::RawLapackPivotView::Create(
      pivots.data() + 1, 0, asc::LapackFactorFamily::kLuPartialPivot,
      {pivots.data(), sizeof(pivots), kHost}));
  const auto factor =
      Take(asc::LapackLuFactorView<T>::Create(matrix.ConstView(), raw, report));
  ASC_DENSE_TEST_CHECK(test, Observe(test, [&] {
                               return asc::Getrs(
                                   asc::ExecutionContext::Serial(), operation,
                                   factor, rhs.View(), report);
                             }).ok());
  Report(test, report);
  ASC_DENSE_TEST_EQ(test, matrix.values, matrix_before);
  ASC_DENSE_TEST_EQ(test, rhs.values, rhs_before);
  ASC_DENSE_TEST_EQ(test, pivots, pivots_before);
}

template <typename T>
void LuProvenance(TestContext& test, Matrix<T>& matrix,
                  asc::RawLapackPivotView pivots, Matrix<T>& rhs, Op operation,
                  asc::LapackReport factor_report) {
  factor_report.provider.kind = asc::LapackProviderKind::kReference;
  const auto foreign = Take(asc::LapackLuFactorView<T>::Create(
      matrix.ConstView(), pivots, factor_report));
  asc::LapackReport report;
  const auto before = rhs.values;
  const auto status = Observe(test, [&] {
    return asc::Getrs(asc::ExecutionContext::Serial(), operation, foreign,
                      rhs.View(), report);
  });
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidState);
  Report(test, report, asc::LapackOutcome::kNotRun,
         asc::LapackOutputValidity::kUnchanged);
  ASC_DENSE_TEST_EQ(test, rhs.values, before);
}

template <typename T>
void LuMode(TestContext& test, Layout layout, Layout rhs_layout, Op operation) {
  Matrix<T> matrix(1, 1, layout);
  matrix.At(0, 0) = Scalar<T>(2, 1);
  const auto original = matrix.values;
  std::array<asc::index_t, 3> pivots{-31, -37, -41};
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, Observe(test, [&] {
                               return asc::Getrf(
                                   asc::ExecutionContext::Serial(),
                                   matrix.View(), Vector(pivots, 1), report);
                             }).ok());
  Report(test, report);
  ASC_DENSE_TEST_EQ(test, pivots[1], 1);
  const auto raw = Take(asc::RawLapackPivotView::Create(
      pivots.data() + 1, 1, asc::LapackFactorFamily::kLuPartialPivot,
      {pivots.data(), sizeof(pivots), kHost}));
  const auto factor_report = report;
  const auto factor =
      Take(asc::LapackLuFactorView<T>::Create(matrix.ConstView(), raw, report));
  Matrix<T> rhs(1, 2, rhs_layout);
  const auto coefficient =
      operation == Op::kConjugateTranspose ? Scalar<T>(2, -1) : Scalar<T>(2, 1);
  KnownRhs(rhs, coefficient);
  const auto before = rhs.values;
  auto execute = [&](auto output) {
    return asc::Getrs(asc::ExecutionContext::Serial(), operation, factor,
                      output, report);
  };
  ASC_DENSE_TEST_CHECK(test,
                       Observe(test, [&] { return execute(rhs.View()); }).ok());
  Report(test, report);
  CheckSolution(test, rhs);
  rhs.CheckPadding(test, before);
  StructuralRejections(test, rhs, report, execute);
  EmptySolve<T>(test, rhs_layout, report, execute);
  LuProvenance(test, matrix, raw, rhs, operation, factor_report);
  const auto solution = rhs.values;
  pivots[1] = 0;
  ASC_DENSE_TEST_EQ(test,
                    Observe(test, [&] { return execute(rhs.View()); }).code(),
                    asc::ErrorCode::kIndex);
  ASC_DENSE_TEST_EQ(test, rhs.values, solution);
  Report(test, report, asc::LapackOutcome::kNotRun,
         asc::LapackOutputValidity::kUnchanged);
  pivots[1] = 1;
  matrix.At(0, 0) = T{0};
  ASC_DENSE_TEST_EQ(test,
                    Observe(test, [&] { return execute(rhs.View()); }).code(),
                    asc::ErrorCode::kNumerical);
  Report(test, report, asc::LapackOutcome::kSingular,
         asc::LapackOutputValidity::kUnchanged);
  ASC_DENSE_TEST_EQ(test, rhs.values, solution);
  ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-1), 0);
  matrix.CheckPadding(test, original);
  ASC_DENSE_TEST_EQ(test, pivots.front(), -31);
  ASC_DENSE_TEST_EQ(test, pivots.back(), -41);
  ++g_lu_modes;
}

template <typename T>
void EmptyCholeskyMatrix(TestContext& test, Layout layout, Layout rhs_layout,
                         Triangle triangle) {
  Matrix<T> matrix(0, 0, layout);
  Matrix<T> rhs(0, 3, rhs_layout);
  const auto matrix_before = matrix.values;
  const auto rhs_before = rhs.values;
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, Observe(test, [&] {
                               return asc::Potrf(
                                   asc::ExecutionContext::Serial(), triangle,
                                   matrix.View(), report);
                             }).ok());
  Report(test, report);
  const auto factor = Take(asc::LapackCholeskyFactorView<T>::Create(
      matrix.ConstView(), triangle, report));
  ASC_DENSE_TEST_CHECK(test, Observe(test, [&] {
                               return asc::Potrs(
                                   asc::ExecutionContext::Serial(), factor,
                                   rhs.View(), report);
                             }).ok());
  Report(test, report);
  ASC_DENSE_TEST_EQ(test, matrix.values, matrix_before);
  ASC_DENSE_TEST_EQ(test, rhs.values, rhs_before);
}

template <typename T>
void CholeskyProvenance(TestContext& test, Matrix<T>& matrix, Matrix<T>& rhs,
                        Triangle triangle, asc::LapackReport factor_report) {
  factor_report.provider.kind = asc::LapackProviderKind::kReference;
  const auto foreign = Take(asc::LapackCholeskyFactorView<T>::Create(
      matrix.ConstView(), triangle, factor_report));
  const auto before = rhs.values;
  asc::LapackReport report;
  const auto status = Observe(test, [&] {
    return asc::Potrs(asc::ExecutionContext::Serial(), foreign, rhs.View(),
                      report);
  });
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidArgument);
  Report(test, report, asc::LapackOutcome::kNotRun,
         asc::LapackOutputValidity::kUnchanged);
  ASC_DENSE_TEST_EQ(test, rhs.values, before);
}

template <typename T>
void CholeskyMode(TestContext& test, Layout layout, Layout rhs_layout,
                  Triangle triangle) {
  Matrix<T> matrix(1, 1, layout);
  matrix.At(0, 0) = Scalar<T>(4, 53);
  const auto original = matrix.values;
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, Observe(test, [&] {
                               return asc::Potrf(
                                   asc::ExecutionContext::Serial(), triangle,
                                   matrix.View(), report);
                             }).ok());
  Report(test, report);
  ASC_DENSE_TEST_EQ(test, matrix.At(0, 0), T{2});
  if constexpr (asc::DenseBlasComplex<T>) {
    ASC_DENSE_TEST_CHECK(test, !std::signbit(matrix.At(0, 0).imag()));
  }
  const auto factor_report = report;
  const auto factor = Take(asc::LapackCholeskyFactorView<T>::Create(
      matrix.ConstView(), triangle, report));
  Matrix<T> rhs(1, 2, rhs_layout);
  KnownRhs(rhs, T{4});
  const auto before = rhs.values;
  auto execute = [&](auto output) {
    return asc::Potrs(asc::ExecutionContext::Serial(), factor, output, report);
  };
  ASC_DENSE_TEST_CHECK(test,
                       Observe(test, [&] { return execute(rhs.View()); }).ok());
  Report(test, report);
  CheckSolution(test, rhs);
  rhs.CheckPadding(test, before);
  StructuralRejections(test, rhs, report, execute);
  EmptySolve<T>(test, rhs_layout, report, execute);
  CholeskyProvenance(test, matrix, rhs, triangle, factor_report);
  const auto solution = rhs.values;
  matrix.At(0, 0) = T{0};
  ASC_DENSE_TEST_EQ(test,
                    Observe(test, [&] { return execute(rhs.View()); }).code(),
                    asc::ErrorCode::kNumerical);
  Report(test, report, asc::LapackOutcome::kNotPositiveDefinite,
         asc::LapackOutputValidity::kUnchanged);
  ASC_DENSE_TEST_EQ(test, rhs.values, solution);
  ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-1), 0);
  matrix.CheckPadding(test, original);
  ++g_cholesky_modes;
}

template <typename T>
void QrNumericalFailures(TestContext& test, Layout layout) {
  using Real = asc::DenseBlasRealType<T>;
  Matrix<T> matrix(2, 1, layout);
  matrix.At(0, 0) = T{std::numeric_limits<Real>::quiet_NaN()};
  matrix.At(1, 0) = T{4};
  const auto before = matrix.values;
  std::array<T, 3> tau{T{-3}, T{-5}, T{-7}};
  std::array<T, 3> work{T{-11}, T{-13}, T{-17}};
  const auto tau_before = tau;
  const auto work_before = work;
  asc::LapackReport report;
  const auto execute = [&] {
    return asc::Geqrf(asc::ExecutionContext::Serial(), matrix.View(),
                      Vector(tau, 1), Vector(work, 1), report);
  };
  ASC_DENSE_TEST_EQ(test, Observe(test, execute).code(),
                    asc::ErrorCode::kNumerical);
  Report(test, report, asc::LapackOutcome::kNotRun,
         asc::LapackOutputValidity::kUnchanged);
  ASC_DENSE_TEST_CHECK(
      test, std::ranges::equal(std::as_bytes(std::span(matrix.values)),
                               std::as_bytes(std::span(before))));
  ASC_DENSE_TEST_EQ(test, tau, tau_before);
  ASC_DENSE_TEST_EQ(test, work, work_before);
  matrix.At(0, 0) = T{std::numeric_limits<Real>::max()};
  matrix.At(1, 0) = matrix.At(0, 0);
  ASC_DENSE_TEST_EQ(test, Observe(test, execute).code(),
                    asc::ErrorCode::kNumerical);
  Report(test, report, asc::LapackOutcome::kPartialResult,
         asc::LapackOutputValidity::kUnusable);
  ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-1), 0);
  matrix.CheckPadding(test, before);
  ASC_DENSE_TEST_EQ(test, tau.front(), tau_before.front());
  ASC_DENSE_TEST_EQ(test, tau.back(), tau_before.back());
  ASC_DENSE_TEST_EQ(test, work.front(), work_before.front());
  ASC_DENSE_TEST_EQ(test, work.back(), work_before.back());
}

template <typename T>
void QrMode(TestContext& test, Layout layout) {
  Matrix<T> matrix(2, 1, layout);
  matrix.At(0, 0) = T{3};
  matrix.At(1, 0) = T{4};
  const auto original = matrix.values;
  std::array<T, 3> tau{T{-3}, T{-5}, T{-7}};
  std::array<T, 3> work{T{-11}, T{-13}, T{-17}};
  const auto tau_before = tau;
  const auto work_before = work;
  asc::LapackReport report;
  auto execute = [&](asc::extent_t count) {
    return asc::Geqrf(asc::ExecutionContext::Serial(), matrix.View(),
                      Vector(tau, 1), Vector(work, count), report);
  };
  ASC_DENSE_TEST_EQ(test, Observe(test, [&] { return execute(0); }).code(),
                    asc::ErrorCode::kShape);
  Report(test, report, asc::LapackOutcome::kNotRun,
         asc::LapackOutputValidity::kUnchanged);
  ASC_DENSE_TEST_EQ(test, matrix.values, original);
  ASC_DENSE_TEST_EQ(test, tau, tau_before);
  ASC_DENSE_TEST_EQ(test, work, work_before);
  ASC_DENSE_TEST_CHECK(test, Observe(test, [&] { return execute(1); }).ok());
  Report(test, report);
  ASC_DENSE_TEST_EQ(test, matrix.At(0, 0), T{-5});
  ASC_DENSE_TEST_EQ(test, matrix.At(1, 0), T{0.5});
  const auto tolerance =
      8 * std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon();
  ASC_DENSE_TEST_CHECK(test, std::abs(Widen(tau[1]) - Wide{1.6L}) <= tolerance);
  matrix.CheckPadding(test, original);
  ASC_DENSE_TEST_EQ(test, tau.front(), tau_before.front());
  ASC_DENSE_TEST_EQ(test, tau.back(), tau_before.back());
  ASC_DENSE_TEST_EQ(test, work.front(), work_before.front());
  ASC_DENSE_TEST_EQ(test, work.back(), work_before.back());
  ++g_qr_modes;
}

template <typename T>
void Run(TestContext& test) {
  for (const auto layout : kLayouts) {
    QrMode<T>(test, layout);
    QrNumericalFailures<T>(test, layout);
    for (const auto rhs_layout : kLayouts) {
      for (const auto operation : kOperations) {
        LuMode<T>(test, layout, rhs_layout, operation);
        EmptyLuMatrix<T>(test, layout, rhs_layout, operation);
      }
      for (const auto triangle : kTriangles) {
        CholeskyMode<T>(test, layout, rhs_layout, triangle);
        EmptyCholeskyMatrix<T>(test, layout, rhs_layout, triangle);
      }
    }
  }
}
}  // namespace

int main() {
  TestContext test;
  Run<float>(test);
  Run<double>(test);
  Run<std::complex<float>>(test);
  Run<std::complex<double>>(test);
  ASC_DENSE_TEST_EQ(test, g_lu_modes, 48U);
  ASC_DENSE_TEST_EQ(test, g_cholesky_modes, 32U);
  ASC_DENSE_TEST_EQ(test, g_qr_modes, 8U);
  std::printf("native scalar LU modes=%zu Cholesky modes=%zu QR modes=%zu\n",
              g_lu_modes, g_cholesky_modes, g_qr_modes);
  return test.Finish();
}
