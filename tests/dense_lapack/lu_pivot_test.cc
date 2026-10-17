#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu.h"
#include "installed_lu/normal_return_guard.h"
#include "lapack_build_config.h"
#include "lu_pivot_faults.h"
#include "lu_pivot_test_support.h"

namespace asc_lu_pivot_test {
// Test-only routing; no production options or dispatch tables are introduced.
enum class Routine : std::uint8_t { kGetrf2, kGetf2, kGetrf, kGesv };
struct Mode {
  Routine routine;
  int m;
  int n;
  asc::DenseBlasLayout a;
  asc::DenseBlasLayout b = kColumn;
  int nrhs = 0;
  bool singular = false;
  Fault fault = Fault::kNone;
};
std::size_t g_cases = 0;

std::array<char, 16> Name(char scalar, Routine routine) {
  constexpr std::array<std::string_view, 4> kNames{"getrf2", "getf2", "getrf",
                                                   "gesv"};
  std::array<char, 16> result{};
  result[0] = scalar;
  const auto name = kNames[static_cast<std::size_t>(routine)];
  std::copy(name.begin(), name.end(), result.begin() + 1);
  return result;
}

bool Called(const Mode& mode) { return mode.m != 0 && mode.n != 0; }
int ActualInfo(const Mode& mode) {
  return Called(mode) && mode.singular ? std::min(mode.m, mode.n) : 0;
}
void LogMode(const Mode& mode, char scalar) {
  const auto name = Name(scalar, mode.routine);
  std::printf(
      "LU_PIVOT_MODE routine=%s m=%d n=%d a=%d b=%d nrhs=%d singular=%d "
      "fault=%d\n",
      name.data(), mode.m, mode.n, static_cast<int>(mode.a),
      static_cast<int>(mode.b), mode.nrhs, static_cast<int>(mode.singular),
      static_cast<int>(mode.fault));
}

void CheckReport(TestContext& test,
                 const asc::ReferenceLapackProvider& provider,
                 const asc::Status& status, const asc::LapackReport& report,
                 const Mode& mode, char scalar) {
  ++g_cases;
  const auto observed = asc_lapack_test::ObserveLuPivot();
  const bool called = Called(mode);
  const bool defect = called && mode.fault != Fault::kNone;
  const int actual = ActualInfo(mode);
  ASC_DENSE_TEST_EQ(test, observed.calls, called ? 1U : 0U);
  ASC_DENSE_TEST_EQ(test, observed.actual_info, actual);
  ASC_DENSE_TEST_EQ(test, observed.entries,
                    static_cast<std::size_t>(std::min(mode.m, mode.n)));
  ASC_DENSE_TEST_EQ(test, observed.valid_actual, observed.entries);
  ASC_DENSE_TEST_EQ(test, report.called_provider, called);
  ASC_DENSE_TEST_EQ(test, report.provider, provider.identity());
  ASC_DENSE_TEST_EQ(test, std::string_view(report.routine.data()),
                    Name(scalar, mode.routine).data());
  ASC_DENSE_TEST_CHECK(test, !report.native_argument.has_value());
  ASC_DENSE_TEST_EQ(test, report.factor_family, kLu);
  if (called) {
    ASC_DENSE_TEST_EQ(test, report.native_info, actual);
  } else {
    ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
  }
  if (actual != 0) {
    ASC_DENSE_TEST_EQ(test, report.diagnostic_index, actual - 1);
  } else {
    ASC_DENSE_TEST_CHECK(test, !report.diagnostic_index.has_value());
  }
  if (defect) {
    ASC_DENSE_TEST_CHECK(test, observed.valid_output < observed.entries);
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
    ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kPartialResult);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kUnusable);
  } else {
    ASC_DENSE_TEST_EQ(test, observed.valid_output, observed.entries);
    ASC_DENSE_TEST_EQ(
        test, status.code(),
        actual == 0 ? asc::ErrorCode::kOk : asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      actual == 0 ? asc::LapackOutcome::kSuccess
                                  : asc::LapackOutcome::kSingular);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      actual == 0
                          ? asc::LapackOutputValidity::kComplete
                          : asc::LapackOutputValidity::kDocumentedPartial);
  }
  std::printf(
      "LU_PIVOT_RESULT routine=%s calls=%zu entries=%zu valid_actual=%zu "
      "valid_output=%zu info=%lld incoming_last=%lld actual_last=%lld "
      "outgoing_last=%lld status=%d\n",
      Name(scalar, mode.routine).data(), observed.calls, observed.entries,
      observed.valid_actual, observed.valid_output,
      static_cast<long long>(observed.actual_info),
      static_cast<long long>(observed.incoming_last),
      static_cast<long long>(observed.actual_last),
      static_cast<long long>(observed.outgoing_last),
      static_cast<int>(status.code()));
}

asc::LapackReport DirtyReport() {
  asc::LapackReport report;
  report.native_info = 701;
  report.native_argument = 702;
  report.diagnostic_index = 703;
  report.factor_family = kLu;
  return report;
}

template <typename T>
auto FactorPlan(const asc::ReferenceLapackProvider& provider, Matrix<T>& matrix,
                std::array<asc::index_t, 69>& pivots, const Mode& mode) {
  const auto view = Pivots(pivots, std::min(mode.m, mode.n));
  if (mode.routine == Routine::kGetrf) {
    return asc::QueryGetrfWorkspace(provider, matrix.View(), view);
  }
  return mode.routine == Routine::kGetrf2
             ? asc::QueryGetrf2Workspace(provider, matrix.View(), view)
             : asc::QueryGetf2Workspace(provider, matrix.View(), view);
}
template <typename T>
asc::Status Factor(const asc::ReferenceLapackProvider& provider,
                   Matrix<T>& matrix, std::array<asc::index_t, 69>& pivots,
                   const asc::LapackWorkspacePlan& plan,
                   const asc::LapackWorkspace& workspace,
                   asc::LapackReport& report, const Mode& mode) {
  const auto view = Pivots(pivots, std::min(mode.m, mode.n));
  if (mode.routine == Routine::kGetrf) {
    return asc::Getrf(provider, matrix.View(), view, plan, workspace, report);
  }
  return mode.routine == Routine::kGetrf2
             ? asc::Getrf2(provider, matrix.View(), view, plan, workspace,
                           report)
             : asc::Getf2(provider, matrix.View(), view, plan, workspace,
                          report);
}

template <typename T>
void CheckPivots(TestContext& test, const std::array<asc::index_t, 69>& pivots,
                 const std::array<asc::index_t, 69>& before, int count) {
  ASC_DENSE_TEST_EQ(test, pivots.front(), before.front());
  for (std::size_t i = static_cast<std::size_t>(count) + 1; i < pivots.size();
       ++i) {
    ASC_DENSE_TEST_EQ(test, pivots[i], before[i]);
  }
}

template <typename T>
void FactorCases(TestContext& test,
                 const asc::ReferenceLapackProvider& provider, Mode mode,
                 char scalar) {
  Matrix<T> original(mode.m, mode.n, mode.a);
  Fill(original, mode.singular);
  auto positive = original;
  for (auto fault : {Fault::kNone, Fault::kOmitAll, Fault::kOmitLast,
                     Fault::kShortAll, Fault::kShortLast}) {
    if ((fault == Fault::kShortAll || fault == Fault::kShortLast) &&
        ASC_LAPACK_INTEGER_BITS != 64) {
      continue;
    }
    mode.fault = fault;
    LogMode(mode, scalar);
    auto factor = original;
    std::array<asc::index_t, 69> pivots;
    pivots.fill(-503);
    const auto before = pivots;
    asc_lapack_test::SetLuPivotFault(fault);
    const auto plan = Take(Observe(
        test, [&] { return FactorPlan(provider, factor, pivots, mode); }));
    ASC_DENSE_TEST_EQ(test, asc_lapack_test::ObserveLuPivot().calls, 0U);
    Scratch<T> scratch;
    const auto workspace = scratch.Workspace(plan);
    auto report = DirtyReport();
    const auto status = Observe(test, [&] {
      return Factor(provider, factor, pivots, plan, workspace, report, mode);
    });
    CheckReport(test, provider, status, report, mode, scalar);
    scratch.CheckGuards(test);
    factor.CheckPadding(test);
    if (fault == Fault::kNone || !Called(mode)) {
      Reconstruction(test, original, factor, pivots);
      CheckPivots<T>(test, pivots, before, std::min(mode.m, mode.n));
      positive = factor;
    } else {
      ASC_DENSE_TEST_EQ(test, pivots, before);
      ASC_DENSE_TEST_EQ(test, factor.data,
                        mode.a == kRow ? original.data : positive.data);
    }
  }
}

template <typename T>
Matrix<T> RightHandSide(const Matrix<T>& matrix, const Mode& mode) {
  Matrix<T> rhs(mode.n, mode.nrhs, mode.b);
  for (int i = 0; i < mode.n; ++i) {
    for (int j = 0; j < mode.nrhs; ++j) {
      T sum{};
      for (int k = 0; k < mode.n; ++k) {
        sum += matrix.At(i, k) * Value<T>(k + j + 1, k - j);
      }
      rhs.At(i, j) = sum;
    }
  }
  return rhs;
}

template <typename T>
void CheckSolution(TestContext& test, const Matrix<T>& rhs) {
  for (int i = 0; i < rhs.rows; ++i) {
    for (int j = 0; j < rhs.columns; ++j) {
      ASC_DENSE_TEST_CHECK(
          test,
          std::abs(Wide(rhs.At(i, j)) - Wide(Value<T>(i + j + 1, i - j))) <=
              1024.L *
                  std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon());
    }
  }
}

template <typename T>
Matrix<T> Rhs(const Matrix<T>& original, asc::DenseBlasLayout rhs_layout,
              asc::DenseBlasTranspose trans, int nrhs) {
  Matrix<T> rhs_original(original.rows, nrhs, rhs_layout);
  for (int i = 0; i < original.rows; ++i) {
    for (int j = 0; j < nrhs; ++j) {
      T sum{};
      for (int k = 0; k < original.rows; ++k) {
        T a = trans == asc::DenseBlasTranspose::kNone ? original.At(i, k)
                                                      : original.At(k, i);
        if constexpr (asc::DenseBlasComplex<T>) {
          if (trans == asc::DenseBlasTranspose::kConjugateTranspose) {
            a = std::conj(a);
          }
        }
        sum += a * Value<T>(k + j + 1, k - j);
      }
      rhs_original.At(i, j) = sum;
    }
  }
  return rhs_original;
}

template <typename T>
void ReuseDriver(TestContext& test,
                 const asc::ReferenceLapackProvider& provider,
                 const Matrix<T>& original, const Matrix<T>& factor,
                 const std::array<asc::index_t, 69>& pivots,
                 const asc::LapackReport& factor_report, const Mode& mode,
                 char scalar) {
  const auto factors_before = factor.data;
  const auto pivots_before = pivots;
  const auto factor_view = Take(asc::LapackLuFactorView<T>::Create(
      factor.ConstView(), Raw(pivots, mode.n), factor_report));
  for (auto trans :
       {asc::DenseBlasTranspose::kNone, asc::DenseBlasTranspose::kTranspose,
        asc::DenseBlasTranspose::kConjugateTranspose}) {
    auto rhs = Rhs(original, mode.b, trans, mode.nrhs);
    const auto plan = Take(Observe(test, [&] {
      return asc::QueryGetrsWorkspace(provider, trans, factor_view, rhs.View());
    }));
    Scratch<T> scratch;
    const auto workspace = scratch.Workspace(plan);
    auto report = DirtyReport();
    const auto status = Observe(test, [&] {
      return asc::Getrs(provider, trans, factor_view, rhs.View(), plan,
                        workspace, report);
    });
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_EQ(test, report.called_provider,
                      mode.n != 0 && mode.nrhs != 0);
    CheckSolution(test, rhs);
    rhs.CheckPadding(test);
    scratch.CheckGuards(test);
    ASC_DENSE_TEST_EQ(test, factor.data, factors_before);
    ASC_DENSE_TEST_EQ(test, pivots, pivots_before);
    std::printf(
        "LU_PIVOT_REUSE scalar=%c n=%d a=%d b=%d nrhs=%d trans=%d status=%d\n",
        scalar, mode.n, static_cast<int>(mode.a), static_cast<int>(mode.b),
        mode.nrhs, static_cast<int>(trans), static_cast<int>(status.code()));
  }
}

template <typename T>
void DriverCases(TestContext& test,
                 const asc::ReferenceLapackProvider& provider, Mode mode,
                 char scalar) {
  Matrix<T> original(mode.n, mode.n, mode.a);
  Fill(original, mode.singular);
  const auto rhs_original = RightHandSide(original, mode);
  auto positive = original;
  auto positive_rhs = rhs_original;
  for (auto fault : {Fault::kNone, Fault::kOmitAll, Fault::kOmitLast,
                     Fault::kShortAll, Fault::kShortLast}) {
    if ((fault == Fault::kShortAll || fault == Fault::kShortLast) &&
        ASC_LAPACK_INTEGER_BITS != 64) {
      continue;
    }
    mode.fault = fault;
    LogMode(mode, scalar);
    auto factor = original;
    auto rhs = rhs_original;
    std::array<asc::index_t, 69> pivots;
    pivots.fill(-503);
    const auto before = pivots;
    asc_lapack_test::SetLuPivotFault(fault);
    const auto plan = Take(Observe(test, [&] {
      return asc::QueryGesvWorkspace(provider, factor.View(),
                                     Pivots(pivots, mode.n), rhs.View());
    }));
    ASC_DENSE_TEST_EQ(test, asc_lapack_test::ObserveLuPivot().calls, 0U);
    Scratch<T> scratch;
    const auto workspace = scratch.Workspace(plan);
    auto report = DirtyReport();
    const auto status = Observe(test, [&] {
      return asc::Gesv(provider, factor.View(), Pivots(pivots, mode.n),
                       rhs.View(), plan, workspace, report);
    });
    CheckReport(test, provider, status, report, mode, scalar);
    scratch.CheckGuards(test);
    factor.CheckPadding(test);
    rhs.CheckPadding(test);
    if (fault == Fault::kNone || !Called(mode)) {
      Reconstruction(test, original, factor, pivots);
      CheckPivots<T>(test, pivots, before, mode.n);
      if (mode.singular) {
        ASC_DENSE_TEST_EQ(test, rhs.data, rhs_original.data);
      } else {
        CheckSolution(test, rhs);
        if (fault == Fault::kNone) {
          ReuseDriver(test, provider, original, factor, pivots, report, mode,
                      scalar);
        }
      }
      positive = factor;
      positive_rhs = rhs;
    } else {
      ASC_DENSE_TEST_EQ(test, pivots, before);
      ASC_DENSE_TEST_EQ(test, factor.data,
                        mode.a == kRow ? original.data : positive.data);
      ASC_DENSE_TEST_EQ(test, rhs.data,
                        mode.b == kRow ? rhs_original.data : positive_rhs.data);
    }
  }
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider,
         char scalar) {
  for (auto routine : {Routine::kGetrf, Routine::kGetrf2, Routine::kGetf2}) {
    for (const auto shape :
         {std::array{1, 1}, std::array{4, 4}, std::array{4, 3},
          std::array{3, 4}, std::array{0, 3}, std::array{3, 0},
          std::array{0, 0}, std::array{67, 65}, std::array{65, 67}}) {
      for (auto layout : {kRow, kColumn}) {
        for (bool singular : {false, true}) {
          Mode mode{routine, shape[0], shape[1], layout};
          mode.singular = singular;
          FactorCases<T>(test, provider, mode, scalar);
        }
      }
    }
  }
  for (int n : {0, 1, 4, 67}) {
    for (auto layout : {kRow, kColumn}) {
      for (bool singular : {false, true}) {
        Mode mode{Routine::kGesv, n, n, layout};
        mode.singular = singular;
        for (auto rhs_layout : {kRow, kColumn}) {
          mode.b = rhs_layout;
          for (int nrhs : {0, 1, 2}) {
            mode.nrhs = nrhs;
            DriverCases<T>(test, provider, mode, scalar);
          }
        }
      }
    }
  }
}
}  // namespace asc_lu_pivot_test

namespace lu_test = asc_lu_pivot_test;

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 2) {
    return 2;
  }
  const auto provider = lu_test::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  lu_test::TestContext test;
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    lu_test::Run<float>(test, provider, 's');
  } else if (scalar == "d") {
    lu_test::Run<double>(test, provider, 'd');
  } else if (scalar == "c") {
    lu_test::Run<std::complex<float>>(test, provider, 'c');
  } else if (scalar == "z") {
    lu_test::Run<std::complex<double>>(test, provider, 'z');
  } else {
    return 2;
  }
  std::printf("LU_PIVOT_COMPLETED scalar=%s abi=%d cases=%zu\n", argv[1],
              ASC_LAPACK_INTEGER_BITS, lu_test::g_cases);
  return test.Finish();
}
