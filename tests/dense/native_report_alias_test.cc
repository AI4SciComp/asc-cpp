#include <array>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>

#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/cholesky.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/qr.h"
#include "asc/dense/lapack/report.h"
#include "native_alias_support.h"
#include "test_support.h"

namespace {
using namespace asc_native_test;  // NOLINT(google-build-using-namespace)

// A byte array can provide storage for a nested object (C++20 [intro.object]).
// Construct one aligned, live scalar inside the report's actual byte storage.
// The report and its other members remain alive. Never assign the report while
// the scalar is live: only the overlap-rejection path receives this report.
// Positive numerical controls use a separate report and the very same scalar.
template <typename T>
class ReportScalar {
 public:
  explicit ReportScalar(T initial) {
    void* storage = report.provider.source_sha256.data();
    std::size_t available = report.provider.source_sha256.size();
    if (std::align(alignof(T), sizeof(T), storage, available) == nullptr) {
      std::abort();
    }
    value_ = std::construct_at(static_cast<T*>(storage), initial);
  }
  ReportScalar(const ReportScalar&) = delete;
  ReportScalar& operator=(const ReportScalar&) = delete;
  ReportScalar(ReportScalar&&) = delete;
  ReportScalar& operator=(ReportScalar&&) = delete;
  ~ReportScalar() { std::destroy_at(value_); }

  T& value() { return *value_; }
  auto MatrixView(Layout layout) {
    return Take(asc::DenseBlasMatrixView<T>::Create(
        value_, 1, 1, layout, 1, {value_, sizeof(T), kHost}));
  }
  auto ConstMatrixView(Layout layout) {
    return Take(asc::DenseBlasMatrixView<const T>::Create(
        value_, 1, 1, layout, 1, {value_, sizeof(T), kHost}));
  }
  auto VectorView() {
    return Take(asc::DenseBlasVectorView<T>::Create(
        value_, 1, 1, {value_, sizeof(T), kHost}));
  }

  asc::LapackReport report;

 private:
  T* value_ = nullptr;
};

const char* LayoutName(Layout layout) {
  return layout == Layout::kRowMajor ? "row_major" : "column_major";
}

template <typename T>
const char* ScalarPrefix() {
  if constexpr (asc::DenseBlasComplex<T>) {
    return sizeof(asc::DenseBlasRealType<T>) == 4 ? "c" : "z";
  } else {
    return sizeof(T) == 4 ? "s" : "d";
  }
}

template <typename T>
void SolveControl(TestContext& test, asc::LapackCholeskyFactorView<T> factor,
                  Matrix<T>& rhs) {
  rhs.At(0, 0) = Scalar<T>(8, 4);
  rhs.At(0, 1) = Scalar<T>(12, -4);
  const auto before = rhs.values;
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, Observe(test, [&] {
                               return asc::Potrs(
                                   asc::ExecutionContext::Serial(), factor,
                                   rhs.View(), report);
                             }).ok());
  Report(test, report);
  // Independent A*X-B residual: original A is [4], not the stored factor [2].
  for (asc::index_t j = 0; j < 2; ++j) {
    ASC_DENSE_TEST_EQ(test, Widen(rhs.At(0, j)) * 4.0L,
                      Widen(before[rhs.Offset(0, j)]));
  }
  rhs.CheckPadding(test, before);
}

template <typename T>
void PotrsReportAliases(TestContext& test, Layout layout, Triangle triangle,
                        Layout rhs_layout) {
  ReportScalar<T> storage(T{4});
  asc::LapackReport factor_report;
  ASC_DENSE_TEST_CHECK(test, Observe(test, [&] {
                               return asc::Potrf(
                                   asc::ExecutionContext::Serial(), triangle,
                                   storage.MatrixView(layout), factor_report);
                             }).ok());
  Report(test, factor_report);
  ASC_DENSE_TEST_EQ(test, storage.value(), T{2});
  const auto factor = Take(asc::LapackCholeskyFactorView<T>::Create(
      storage.ConstMatrixView(layout), triangle, factor_report));
  Matrix<T> rhs(1, 2, rhs_layout);
  SolveControl(test, factor, rhs);
  const auto report_before = Bytes(storage.report);
  const auto factor_report_before = Bytes(factor_report);
  const auto rhs_before = rhs.values;
  ASC_DENSE_TEST_EQ(test,
                    Observe(test,
                            [&] {
                              return asc::Potrs(asc::ExecutionContext::Serial(),
                                                factor, rhs.View(),
                                                storage.report);
                            })
                        .code(),
                    asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, Bytes(storage.report), report_before);
  ASC_DENSE_TEST_EQ(test, Bytes(factor_report), factor_report_before);
  ASC_DENSE_TEST_EQ(test, rhs.values, rhs_before);
  ASC_DENSE_TEST_EQ(test, storage.value(), T{2});

  ReportScalar<T> rhs_storage(Scalar<T>(8, 4));
  const auto rhs_report_before = Bytes(rhs_storage.report);
  ASC_DENSE_TEST_EQ(test,
                    Observe(test,
                            [&] {
                              return asc::Potrs(
                                  asc::ExecutionContext::Serial(), factor,
                                  rhs_storage.MatrixView(rhs_layout),
                                  rhs_storage.report);
                            })
                        .code(),
                    asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, Bytes(rhs_storage.report), rhs_report_before);
  ASC_DENSE_TEST_EQ(test, Bytes(storage.report), report_before);

  // A factor/RHS collision resets a disjoint report, but preserves the factor.
  asc::LapackReport report;
  ASC_DENSE_TEST_EQ(test,
                    Observe(test,
                            [&] {
                              return asc::Potrs(
                                  asc::ExecutionContext::Serial(), factor,
                                  storage.MatrixView(rhs_layout), report);
                            })
                        .code(),
                    asc::ErrorCode::kInvalidArgument);
  RejectedReport(test, report);
  ASC_DENSE_TEST_EQ(test, Bytes(storage.report), report_before);
  SolveControl(test, factor, rhs);
  ASC_DENSE_TEST_EQ(test, Bytes(storage.report), report_before);
  std::printf("native report alias: lapack.%spotrs %s.%s.%s\n",
              ScalarPrefix<T>(), LayoutName(layout),
              triangle == Triangle::kLower ? "lower" : "upper",
              LayoutName(rhs_layout));
}

template <typename T>
void CheckQr(TestContext& test, Matrix<T>& matrix, T tau,
             const std::array<T, 2>& original) {
  const std::array<Wide, 2> v{Wide{1}, Widen(matrix.At(1, 0))};
  std::array<std::array<Wide, 2>, 2> q{};
  const long double tolerance =
      64 * std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon();
  for (std::size_t i = 0; i < 2; ++i) {
    for (std::size_t j = 0; j < 2; ++j) {
      q[i][j] =
          Wide{i == j ? 1.0L : 0.0L} - Widen(tau) * v[i] * std::conj(v[j]);
    }
    const auto reconstructed = q[i][0] * Widen(matrix.At(0, 0));
    ASC_DENSE_TEST_CHECK(test, std::abs(reconstructed - Widen(original[i])) <=
                                   tolerance * std::abs(Widen(original[i])));
  }
  for (std::size_t i = 0; i < 2; ++i) {
    for (std::size_t j = 0; j < 2; ++j) {
      const Wide inner =
          std::conj(q[0][i]) * q[0][j] + std::conj(q[1][i]) * q[1][j];
      ASC_DENSE_TEST_CHECK(
          test, std::abs(inner - Wide{i == j ? 1.0L : 0.0L}) <= tolerance);
    }
  }
}

template <typename T>
void QrReportAlias(TestContext& test, Layout layout, bool aliases_tau) {
  ReportScalar<T> storage(Scalar<T>(-17, 3));
  std::array<T, 3> other{T{-19}, T{-23}, T{-29}};
  const auto tau = aliases_tau ? storage.VectorView() : Vector(other, 1);
  const auto work = aliases_tau ? Vector(other, 1) : storage.VectorView();
  Matrix<T> matrix(2, 1, layout);
  const std::array original{Scalar<T>(3, 1), Scalar<T>(4, -2)};
  matrix.At(0, 0) = original[0];
  matrix.At(1, 0) = original[1];
  const auto before = matrix.values;
  const auto other_before = other;
  const auto report_before = Bytes(storage.report);
  ASC_DENSE_TEST_EQ(test,
                    Observe(test,
                            [&] {
                              return asc::Geqrf(asc::ExecutionContext::Serial(),
                                                matrix.View(), tau, work,
                                                storage.report);
                            })
                        .code(),
                    asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, Bytes(storage.report), report_before);
  ASC_DENSE_TEST_EQ(test, matrix.values, before);
  ASC_DENSE_TEST_EQ(test, other, other_before);

  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, Observe(test, [&] {
                               return asc::Geqrf(
                                   asc::ExecutionContext::Serial(),
                                   matrix.View(), tau, work, report);
                             }).ok());
  Report(test, report);
  CheckQr(test, matrix, *tau.data(), original);
  matrix.CheckPadding(test, before);
  ASC_DENSE_TEST_EQ(test, other.front(), other_before.front());
  ASC_DENSE_TEST_EQ(test, other.back(), other_before.back());
}

template <typename T>
void QrValidationOrder(TestContext& test, Layout layout) {
  ReportScalar<T> storage(T{-17});
  Matrix<T> matrix(2, 2, layout);
  std::array<T, 4> tau{T{-1}, T{-2}, T{-3}, T{-4}};
  const auto before = matrix.values;
  const auto tau_before = tau;
  const auto report_before = Bytes(storage.report);
  // The public native API has a fixed workspace length, not a query API.
  // One live scratch scalar is insufficient here; overlap takes precedence.
  ASC_DENSE_TEST_EQ(test,
                    Observe(test,
                            [&] {
                              return asc::Geqrf(asc::ExecutionContext::Serial(),
                                                matrix.View(), Vector(tau, 2),
                                                storage.VectorView(),
                                                storage.report);
                            })
                        .code(),
                    asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, Bytes(storage.report), report_before);
  asc::LapackReport report;
  ASC_DENSE_TEST_EQ(test,
                    Observe(test,
                            [&] {
                              return asc::Geqrf(asc::ExecutionContext::Serial(),
                                                matrix.View(), Vector(tau, 2),
                                                storage.VectorView(), report);
                            })
                        .code(),
                    asc::ErrorCode::kShape);
  RejectedReport(test, report);
  ASC_DENSE_TEST_EQ(test, Bytes(storage.report), report_before);
  ASC_DENSE_TEST_EQ(test, matrix.values, before);
  ASC_DENSE_TEST_EQ(test, tau, tau_before);
}

template <typename T>
void Run(TestContext& test) {
  for (const auto layout : kLayouts) {
    for (const auto triangle : kTriangles) {
      for (const auto rhs_layout : kLayouts) {
        PotrsReportAliases<T>(test, layout, triangle, rhs_layout);
      }
    }
    QrReportAlias<T>(test, layout, true);
    QrReportAlias<T>(test, layout, false);
    QrValidationOrder<T>(test, layout);
    std::printf("native report alias: lapack.%sgeqrf %s\n", ScalarPrefix<T>(),
                LayoutName(layout));
  }
}
}  // namespace

int main() {
  TestContext test;
  Run<float>(test);
  Run<double>(test);
  Run<std::complex<float>>(test);
  Run<std::complex<double>>(test);
  return test.Finish();
}
