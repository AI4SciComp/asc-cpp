#include <array>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/cholesky.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/lu.h"
#include "asc/dense/lapack/qr.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "native_alias_support.h"
#include "test_support.h"

namespace {
using namespace asc_native_test;  // NOLINT(google-build-using-namespace)
constexpr std::array kOperations{Op::kNone, Op::kTranspose,
                                 Op::kConjugateTranspose};
std::size_t g_mode_groups = 0;

template <typename T>
void Emit(const char* routine, Layout layout, Layout rhs_layout = kLayouts[0],
          Op operation = kOperations[0], Triangle triangle = kTriangles[0]) {
  std::printf(
      "native alias mode: %s scalar_bytes=%zu complex=%d layout=%d "
      "rhs_layout=%d transpose=%d triangle=%d\n",
      routine, sizeof(T), static_cast<int>(asc::DenseBlasComplex<T>),
      static_cast<int>(layout), static_cast<int>(rhs_layout),
      static_cast<int>(operation), static_cast<int>(triangle));
  ++g_mode_groups;
}

template <typename T>
void GetrfValidation(TestContext& test, Layout layout) {
  Matrix<T> matrix(2, 2, layout);
  matrix.At(0, 0) = T{1};
  matrix.At(0, 1) = T{0};
  matrix.At(1, 0) = T{0};
  matrix.At(1, 1) = T{1};
  std::array<asc::index_t, 8> pivots{};
  const auto before = matrix.values;
  const auto pivot_before = pivots;
  asc::LapackReport report;
  const auto vector = [&](asc::extent_t count, asc::stride_t increment,
                          asc::MemorySpace placement = kHost) {
    return Take(asc::DenseBlasVectorView<asc::index_t>::Create(
        pivots.data() + 2, count, increment,
        {pivots.data(), sizeof(pivots), placement}));
  };
  const auto reject = [&](auto input, auto output, asc::ErrorCode code) {
    report.called_provider = true;
    report.native_info = 73;
    report.diagnostic_index = 19;
    ASC_DENSE_TEST_EQ(test,
                      Observe(test,
                              [&] {
                                return asc::Getrf(
                                    asc::ExecutionContext::Serial(), input,
                                    output, report);
                              })
                          .code(),
                      code);
    RejectedReport(test, report);
    ASC_DENSE_TEST_EQ(test, matrix.values, before);
    ASC_DENSE_TEST_EQ(test, pivots, pivot_before);
  };
  for (asc::extent_t count : {0, 1, 3}) {
    reject(matrix.View(), vector(count, 1), asc::ErrorCode::kShape);
  }
  for (asc::stride_t increment : {-1, 2}) {
    reject(matrix.View(), vector(2, increment),
           asc::ErrorCode::kInvalidArgument);
  }
  for (auto placement : {asc::MemorySpace::kDevice, asc::MemorySpace::kManaged,
                         asc::MemorySpace::kPinnedHost}) {
    reject(matrix.View(placement), vector(2, 1), asc::ErrorCode::kMemoryAccess);
    reject(matrix.View(), vector(2, 1, placement),
           asc::ErrorCode::kMemoryAccess);
  }
}

template <typename T>
void GetrfAlias(TestContext& test, Layout layout) {
  GappedMatrix<T, std::array<asc::index_t, 2>> storage;
  const auto before = Bytes(storage);
  auto pivots = Take(asc::DenseBlasVectorView<asc::index_t>::Create(
      storage.middle.data(), 2, 1,
      {storage.middle.data(), sizeof(storage.middle), kHost}));
  asc::LapackReport report;
  ASC_DENSE_TEST_EQ(test,
                    Observe(test,
                            [&] {
                              return asc::Getrf(asc::ExecutionContext::Serial(),
                                                storage.View(layout), pivots,
                                                report);
                            })
                        .code(),
                    asc::ErrorCode::kInvalidArgument);
  RejectedReport(test, report);
  ASC_DENSE_TEST_EQ(test, Bytes(storage), before);
}

template <typename T>
void ComplexPivots(TestContext& test, Layout layout) {
  if constexpr (asc::DenseBlasComplex<T>) {
    Matrix<T> matrix(2, 1, layout);
    std::array<asc::index_t, 3> pivots{-11, -13, -17};
    asc::LapackReport report;
    for (const bool tie : {false, true}) {
      matrix.At(0, 0) = tie ? T{4, 1} : T{5, 0};
      matrix.At(1, 0) = tie ? T{3, 2} : T{3, 3};
      const auto before = matrix.values;
      ASC_DENSE_TEST_CHECK(
          test, Observe(test, [&] {
                  return asc::Getrf(asc::ExecutionContext::Serial(),
                                    matrix.View(), Vector(pivots, 1), report);
                }).ok());
      Report(test, report);
      ASC_DENSE_TEST_EQ(test, pivots[1], tie ? 1 : 2);
      ASC_DENSE_TEST_EQ(test, pivots.front(), -11);
      ASC_DENSE_TEST_EQ(test, pivots.back(), -17);
      matrix.CheckPadding(test, before);
    }
  }
}

template <typename T>
void GetrsDefensive(TestContext& test, Matrix<T>& matrix,
                    asc::LapackLuFactorView<T> factor,
                    asc::DenseBlasVectorView<asc::index_t> pivots,
                    Layout rhs_layout, Op operation) {
  Matrix<T> rhs(2, 2, rhs_layout);
  const auto before = rhs.values;
  const auto matrix_before = matrix.values;
  asc::LapackReport report;
  // Explicit borrowed-buffer invalidation tests defensive revalidation.
  for (asc::index_t bad : {-1, 0, 3}) {
    pivots.data()[0] = bad;
    ASC_DENSE_TEST_EQ(test,
                      Observe(test,
                              [&] {
                                return asc::Getrs(
                                    asc::ExecutionContext::Serial(), operation,
                                    factor, rhs.View(), report);
                              })
                          .code(),
                      asc::ErrorCode::kIndex);
    RejectedReport(test, report);
    ASC_DENSE_TEST_EQ(test, rhs.values, before);
    ASC_DENSE_TEST_EQ(test, pivots.data()[0], bad);
    ASC_DENSE_TEST_EQ(test, pivots.data()[1], 2);
  }
  pivots.data()[0] = 1;
  pivots.data()[1] = 1;
  ASC_DENSE_TEST_EQ(test,
                    Observe(test,
                            [&] {
                              return asc::Getrs(asc::ExecutionContext::Serial(),
                                                operation, factor, rhs.View(),
                                                report);
                            })
                        .code(),
                    asc::ErrorCode::kIndex);
  RejectedReport(test, report);
  ASC_DENSE_TEST_EQ(test, rhs.values, before);
  ASC_DENSE_TEST_EQ(test, pivots.data()[1], 1);
  pivots.data()[1] = 2;
  // Fixed uint8_t storage permits deliberate malformed-option input.
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  const auto invalid = static_cast<Op>(255);
  ASC_DENSE_TEST_EQ(test,
                    Observe(test,
                            [&] {
                              return asc::Getrs(asc::ExecutionContext::Serial(),
                                                invalid, factor, rhs.View(),
                                                report);
                            })
                        .code(),
                    asc::ErrorCode::kInvalidArgument);
  RejectedReport(test, report);
  ASC_DENSE_TEST_EQ(test, rhs.values, before);
  ASC_DENSE_TEST_EQ(test, matrix.values, matrix_before);
}

template <typename T>
void GetrsAliases(TestContext& test, Layout layout, Layout rhs_layout,
                  Op operation) {
  Matrix<T> matrix(2, 2, layout);
  matrix.At(0, 0) = T{1};
  matrix.At(0, 1) = T{0};
  matrix.At(1, 0) = T{0};
  matrix.At(1, 1) = T{1};
  GappedMatrix<T, std::array<asc::index_t, 2>> rhs;
  auto pivots = Take(asc::DenseBlasVectorView<asc::index_t>::Create(
      rhs.middle.data(), 2, 1, {rhs.middle.data(), sizeof(rhs.middle), kHost}));
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, Observe(test, [&] {
                               return asc::Getrf(
                                   asc::ExecutionContext::Serial(),
                                   matrix.View(), pivots, report);
                             }).ok());
  const auto raw = Take(asc::RawLapackPivotView::Create(
      rhs.middle.data(), 2, asc::LapackFactorFamily::kLuPartialPivot,
      {rhs.middle.data(), sizeof(rhs.middle), kHost}));
  const auto factor =
      Take(asc::LapackLuFactorView<T>::Create(matrix.ConstView(), raw, report));
  const auto before = matrix.values;
  const auto rhs_before = Bytes(rhs);
  const auto factor_alias = Take(asc::DenseBlasMatrixView<T>::Create(
      matrix.values.data() + 3, 2, 2, rhs_layout, matrix.leading,
      {matrix.values.data(), sizeof(matrix.values), kHost}));
  for (const auto& output : {factor_alias, rhs.View(rhs_layout)}) {
    ASC_DENSE_TEST_EQ(test,
                      Observe(test,
                              [&] {
                                return asc::Getrs(
                                    asc::ExecutionContext::Serial(), operation,
                                    factor, output, report);
                              })
                          .code(),
                      asc::ErrorCode::kInvalidArgument);
    RejectedReport(test, report);
    ASC_DENSE_TEST_EQ(test, matrix.values, before);
    ASC_DENSE_TEST_EQ(test, Bytes(rhs), rhs_before);
  }
  GetrsDefensive(test, matrix, factor, pivots, rhs_layout, operation);
  Emit<T>("getrs", layout, rhs_layout, operation);
}

template <typename T>
void PotrfValidation(TestContext& test, Layout layout, Triangle triangle) {
  Matrix<T> matrix(2, 2, layout);
  const auto before = matrix.values;
  asc::LapackReport report;
  for (auto placement : {asc::MemorySpace::kDevice, asc::MemorySpace::kManaged,
                         asc::MemorySpace::kPinnedHost}) {
    ASC_DENSE_TEST_EQ(test,
                      Observe(test,
                              [&] {
                                return asc::Potrf(
                                    asc::ExecutionContext::Serial(), triangle,
                                    matrix.View(placement), report);
                              })
                          .code(),
                      asc::ErrorCode::kMemoryAccess);
    RejectedReport(test, report);
    ASC_DENSE_TEST_EQ(test, matrix.values, before);
  }
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  const auto invalid = static_cast<Triangle>(255);
  ASC_DENSE_TEST_EQ(test,
                    Observe(test,
                            [&] {
                              return asc::Potrf(asc::ExecutionContext::Serial(),
                                                invalid, matrix.View(), report);
                            })
                        .code(),
                    asc::ErrorCode::kInvalidArgument);
  RejectedReport(test, report);
  ASC_DENSE_TEST_EQ(test, matrix.values, before);
  Matrix<T> wrong(2, 3, layout);
  const auto wrong_before = wrong.values;
  ASC_DENSE_TEST_EQ(test,
                    Observe(test,
                            [&] {
                              return asc::Potrf(asc::ExecutionContext::Serial(),
                                                triangle, wrong.View(), report);
                            })
                        .code(),
                    asc::ErrorCode::kShape);
  RejectedReport(test, report);
  ASC_DENSE_TEST_EQ(test, wrong.values, wrong_before);
}

template <typename T>
void PotrfReportAlias(TestContext& test, Layout layout, Triangle triangle) {
  GappedMatrix<T, asc::LapackReport> storage;
  storage.middle.native_info = 73;
  storage.middle.called_provider = true;
  const auto before = Bytes(storage);
  ASC_DENSE_TEST_EQ(test,
                    Observe(test,
                            [&] {
                              return asc::Potrf(asc::ExecutionContext::Serial(),
                                                triangle, storage.View(layout),
                                                storage.middle);
                            })
                        .code(),
                    asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, Bytes(storage), before);
}

template <typename T>
void PotrsDefensive(TestContext& test, Matrix<T>& matrix,
                    asc::LapackCholeskyFactorView<T> factor,
                    Layout rhs_layout) {
  using Real = asc::DenseBlasRealType<T>;
  Matrix<T> rhs(2, 2, rhs_layout);
  const auto before = rhs.values;
  asc::LapackReport report;
  const auto reject = [&](T diagonal) {
    // Deliberate invalidation exercises the required pre-solve scan.
    matrix.At(1, 1) = diagonal;
    const auto matrix_before = Bytes(matrix.values);
    ASC_DENSE_TEST_EQ(test,
                      Observe(test,
                              [&] {
                                return asc::Potrs(
                                    asc::ExecutionContext::Serial(), factor,
                                    rhs.View(), report);
                              })
                          .code(),
                      asc::ErrorCode::kNumerical);
    Report(test, report, asc::LapackOutcome::kNotPositiveDefinite,
           asc::LapackOutputValidity::kUnchanged);
    ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-1), 1);
    ASC_DENSE_TEST_EQ(test, rhs.values, before);
    ASC_DENSE_TEST_EQ(test, Bytes(matrix.values), matrix_before);
  };
  for (Real diagonal :
       {Real{0}, Real{-1}, std::numeric_limits<Real>::infinity(),
        std::numeric_limits<Real>::quiet_NaN()}) {
    reject(T{diagonal});
  }
  if constexpr (asc::DenseBlasComplex<T>) {
    reject(T{1, 1});
  }
  matrix.At(1, 1) = T{1};
}

template <typename T>
void PotrsAliases(TestContext& test, Layout layout, Layout rhs_layout,
                  Triangle triangle) {
  Matrix<T> matrix(2, 2, layout);
  matrix.At(0, 0) = T{1};
  matrix.At(0, 1) = T{0};
  matrix.At(1, 0) = T{0};
  matrix.At(1, 1) = T{1};
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, Observe(test, [&] {
                               return asc::Potrf(
                                   asc::ExecutionContext::Serial(), triangle,
                                   matrix.View(), report);
                             }).ok());
  const auto factor = Take(asc::LapackCholeskyFactorView<T>::Create(
      matrix.ConstView(), triangle, report));
  const auto before = matrix.values;
  const auto factor_alias = Take(asc::DenseBlasMatrixView<T>::Create(
      matrix.values.data() + 3, 2, 2, rhs_layout, matrix.leading,
      {matrix.values.data(), sizeof(matrix.values), kHost}));
  ASC_DENSE_TEST_EQ(test,
                    Observe(test,
                            [&] {
                              return asc::Potrs(asc::ExecutionContext::Serial(),
                                                factor, factor_alias, report);
                            })
                        .code(),
                    asc::ErrorCode::kInvalidArgument);
  RejectedReport(test, report);
  ASC_DENSE_TEST_EQ(test, matrix.values, before);
  GappedMatrix<T, asc::LapackReport> rhs;
  rhs.middle.called_provider = true;
  rhs.middle.native_info = 73;
  const auto rhs_before = Bytes(rhs);
  ASC_DENSE_TEST_EQ(test,
                    Observe(test,
                            [&] {
                              return asc::Potrs(asc::ExecutionContext::Serial(),
                                                factor, rhs.View(rhs_layout),
                                                rhs.middle);
                            })
                        .code(),
                    asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, matrix.values, before);
  ASC_DENSE_TEST_EQ(test, Bytes(rhs), rhs_before);
  PotrsDefensive(test, matrix, factor, rhs_layout);
  Emit<T>("potrs", layout, rhs_layout, kOperations[0], triangle);
}

template <typename T>
struct QrOperands {
  Matrix<T> matrix;
  std::array<T, 8> tau{};
  std::array<T, 8> work{};
  asc::LapackReport report;
  explicit QrOperands(Layout layout) : matrix(3, 2, layout) {}
  auto VectorView(std::array<T, 8>& storage, asc::extent_t count = 2,
                  asc::stride_t increment = 1,
                  asc::MemorySpace placement = kHost) {
    return Take(asc::DenseBlasVectorView<T>::Create(
        storage.data() + 2, count, increment,
        {storage.data(), sizeof(storage), placement}));
  }
  void Reject(TestContext& test, asc::DenseBlasMatrixView<T> input,
              asc::DenseBlasVectorView<T> coefficients,
              asc::DenseBlasVectorView<T> scratch, asc::ErrorCode code) {
    const auto before = matrix.values;
    const auto tau_before = tau;
    const auto work_before = work;
    ASC_DENSE_TEST_EQ(test,
                      Observe(test,
                              [&] {
                                return asc::Geqrf(
                                    asc::ExecutionContext::Serial(), input,
                                    coefficients, scratch, report);
                              })
                          .code(),
                      code);
    RejectedReport(test, report);
    ASC_DENSE_TEST_EQ(test, matrix.values, before);
    ASC_DENSE_TEST_EQ(test, tau, tau_before);
    ASC_DENSE_TEST_EQ(test, work, work_before);
  }
};

template <typename T>
void QrMalformed(TestContext& test, QrOperands<T>& values) {
  for (asc::extent_t count : {0, 1, 3}) {
    values.Reject(test, values.matrix.View(),
                  values.VectorView(values.tau, count),
                  values.VectorView(values.work), asc::ErrorCode::kShape);
  }
  for (asc::extent_t count : {0, 1}) {
    values.Reject(test, values.matrix.View(), values.VectorView(values.tau),
                  values.VectorView(values.work, count),
                  asc::ErrorCode::kShape);
  }
  for (asc::stride_t increment : {-1, 2}) {
    values.Reject(
        test, values.matrix.View(), values.VectorView(values.tau, 2, increment),
        values.VectorView(values.work), asc::ErrorCode::kInvalidArgument);
    values.Reject(test, values.matrix.View(), values.VectorView(values.tau),
                  values.VectorView(values.work, 2, increment),
                  asc::ErrorCode::kInvalidArgument);
  }
  for (auto placement : {asc::MemorySpace::kDevice, asc::MemorySpace::kManaged,
                         asc::MemorySpace::kPinnedHost}) {
    values.Reject(test, values.matrix.View(placement),
                  values.VectorView(values.tau), values.VectorView(values.work),
                  asc::ErrorCode::kMemoryAccess);
    values.Reject(test, values.matrix.View(),
                  values.VectorView(values.tau, 2, 1, placement),
                  values.VectorView(values.work),
                  asc::ErrorCode::kMemoryAccess);
    values.Reject(test, values.matrix.View(), values.VectorView(values.tau),
                  values.VectorView(values.work, 2, 1, placement),
                  asc::ErrorCode::kMemoryAccess);
  }
}

template <typename T>
void QrAliases(TestContext& test, QrOperands<T>& values) {
  auto& storage = values.matrix.values;
  const auto matrix_alias = Take(asc::DenseBlasVectorView<T>::Create(
      storage.data() + 3, 2, 1, {storage.data(), sizeof(storage), kHost}));
  const auto unused_work_alias = Take(asc::DenseBlasVectorView<T>::Create(
      storage.data(), 4, 1, {storage.data(), sizeof(storage), kHost}));
  values.Reject(test, values.matrix.View(), matrix_alias,
                values.VectorView(values.work),
                asc::ErrorCode::kInvalidArgument);
  values.Reject(test, values.matrix.View(), values.VectorView(values.tau),
                matrix_alias, asc::ErrorCode::kInvalidArgument);
  values.Reject(test, values.matrix.View(), values.VectorView(values.tau),
                values.VectorView(values.tau),
                asc::ErrorCode::kInvalidArgument);
  values.Reject(test, values.matrix.View(), values.VectorView(values.tau),
                unused_work_alias, asc::ErrorCode::kInvalidArgument);
}

template <typename T>
void QrReportAlias(TestContext& test, Layout layout) {
  GappedMatrix<T, asc::LapackReport> matrix;
  std::array<T, 3> tau{};
  std::array<T, 3> work{};
  matrix.middle.native_info = 73;
  matrix.middle.called_provider = true;
  const auto before = Bytes(matrix);
  const auto tau_before = tau;
  const auto work_before = work;
  ASC_DENSE_TEST_EQ(test,
                    Observe(test,
                            [&] {
                              return asc::Geqrf(asc::ExecutionContext::Serial(),
                                                matrix.View(layout),
                                                Vector(tau, 2), Vector(work, 2),
                                                matrix.middle);
                            })
                        .code(),
                    asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, Bytes(matrix), before);
  ASC_DENSE_TEST_EQ(test, tau, tau_before);
  ASC_DENSE_TEST_EQ(test, work, work_before);
}

template <typename T>
void Run(TestContext& test) {
  for (auto layout : kLayouts) {
    GetrfValidation<T>(test, layout);
    GetrfAlias<T>(test, layout);
    ComplexPivots<T>(test, layout);
    Emit<T>("getrf", layout);
    QrOperands<T> qr(layout);
    QrMalformed(test, qr);
    QrAliases(test, qr);
    QrReportAlias<T>(test, layout);
    Emit<T>("geqrf", layout);
    for (auto triangle : kTriangles) {
      PotrfValidation<T>(test, layout, triangle);
      PotrfReportAlias<T>(test, layout, triangle);
      Emit<T>("potrf", layout, kLayouts[0], kOperations[0], triangle);
    }
    for (auto rhs_layout : kLayouts) {
      for (auto operation : kOperations) {
        GetrsAliases<T>(test, layout, rhs_layout, operation);
      }
      for (auto triangle : kTriangles) {
        PotrsAliases<T>(test, layout, rhs_layout, triangle);
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
  ASC_DENSE_TEST_EQ(test, g_mode_groups, std::size_t{112});
  return test.Finish();
}
