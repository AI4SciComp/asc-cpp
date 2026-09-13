#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <string_view>
#include <utility>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu_driver.h"
#include "installed_lu/normal_return_guard.h"
#include "lapack_build_config.h"
#include "lu_driver_pivot_faults.h"
#include "lu_driver_test_support.h"

namespace asc_driver_test {
template <typename T>
bool SameRepresentation(const T& first, const T& second) {
  using Bytes = std::array<std::byte, sizeof(T)>;
  // NaN payloads and signs are deliberately compared, not numerical equality.
  return std::bit_cast<Bytes>(first) == std::bit_cast<Bytes>(second);
}

bool RowScaled(asc::LapackEquilibration equed) {
  return equed == asc::LapackEquilibration::kRows ||
         equed == asc::LapackEquilibration::kBoth;
}
bool ColumnScaled(asc::LapackEquilibration equed) {
  return equed == asc::LapackEquilibration::kColumns ||
         equed == asc::LapackEquilibration::kBoth;
}

template <typename T>
void CheckGrowth(TestContext& test, const Sample<T>& sample,
                 asc::extent_t columns) {
  long double a_max = 0;
  long double u_max = 0;
  for (asc::extent_t j = 0; j < columns; ++j) {
    for (asc::extent_t i = 0; i < sample.n; ++i) {
      a_max =
          std::max(a_max, std::abs(ToWide(sample.a[sample.Offset(0, i, j)])));
    }
    for (asc::extent_t i = 0; i <= j; ++i) {
      u_max =
          std::max(u_max, std::abs(ToWide(sample.af[sample.Offset(1, i, j)])));
    }
  }
  const auto expected = u_max == 0 ? 1 : a_max / u_max;
  const auto epsilon =
      std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon();
  ASC_DENSE_TEST_CHECK(
      test, std::abs(sample.statistics.reciprocal_pivot_growth - expected) <=
                16 * epsilon * expected);
}

template <typename T, std::size_t Size>
void CheckPadding(TestContext& test, const Sample<T>& sample,
                  const std::array<T, Size>& before,
                  const std::array<T, Size>& after, unsigned int operand) {
  for (std::size_t offset = 0; offset < Size; ++offset) {
    bool value = false;
    for (asc::extent_t i = 0; i < sample.n; ++i) {
      for (asc::extent_t j = 0; j < (operand < 2 ? sample.n : sample.nrhs);
           ++j) {
        value |= offset == sample.Offset(operand, i, j);
      }
    }
    if (!value) {
      ASC_DENSE_TEST_EQ(test, after[offset], before[offset]);
    }
  }
}

template <typename T>
void CheckAllPadding(TestContext& test, const Sample<T>& before,
                     const Sample<T>& after) {
  CheckPadding(test, after, before.a, after.a, 0);
  CheckPadding(test, after, before.af, after.af, 1);
  CheckPadding(test, after, before.b, after.b, 2);
  CheckPadding(test, after, before.x, after.x, 3);
  for (const auto* scales : {&after.rows, &after.columns}) {
    const auto& original = scales == &after.rows ? before.rows : before.columns;
    ASC_DENSE_TEST_EQ(test, scales->front(), original.front());
    for (std::size_t i = 1 + after.n; i < scales->size(); ++i) {
      ASC_DENSE_TEST_EQ(test, (*scales)[i], original[i]);
    }
  }
  ASC_DENSE_TEST_EQ(test, after.pivots.front(), before.pivots.front());
  for (std::size_t i = 1 + after.n; i < after.pivots.size(); ++i) {
    ASC_DENSE_TEST_EQ(test, after.pivots[i], before.pivots[i]);
  }
}

template <typename T>
void CheckReconstruction(TestContext& test, const Sample<T>& sample) {
  std::array<Wide, 64> permuted{};
  long double norm = 0;
  for (asc::extent_t i = 0; i < sample.n; ++i) {
    for (asc::extent_t j = 0; j < sample.n; ++j) {
      permuted[i * sample.n + j] = ToWide(sample.a[sample.Offset(0, i, j)]);
      norm = std::max(norm, std::abs(permuted[i * sample.n + j]));
    }
  }
  for (asc::extent_t i = 0; i < sample.n; ++i) {
    const auto pivot = sample.pivots[i + 1] - 1;
    ASC_DENSE_TEST_CHECK(test, pivot >= i && pivot < sample.n);
    for (asc::extent_t j = 0; j < sample.n; ++j) {
      std::swap(permuted[i * sample.n + j], permuted[pivot * sample.n + j]);
    }
  }
  long double error = 0;
  for (asc::extent_t i = 0; i < sample.n; ++i) {
    for (asc::extent_t j = 0; j < sample.n; ++j) {
      Wide actual = 0;
      for (asc::extent_t k = 0; k <= std::min(i, j); ++k) {
        const Wide lower =
            i == k ? Wide{1} : ToWide(sample.af[sample.Offset(1, i, k)]);
        actual += lower * ToWide(sample.af[sample.Offset(1, k, j)]);
      }
      error = std::max(error, std::abs(actual - permuted[i * sample.n + j]));
    }
  }
  const auto epsilon =
      std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon();
  ASC_DENSE_TEST_CHECK(test, error <= 64 * epsilon * norm);
}

template <typename T>
void CheckSolution(TestContext& test, const Sample<T>& original,
                   const Sample<T>& solved) {
  using Real = asc::DenseBlasRealType<T>;
  const auto epsilon = std::numeric_limits<Real>::epsilon();
  ASC_DENSE_TEST_CHECK(test, solved.statistics.reciprocal_condition > 0);
  ASC_DENSE_TEST_CHECK(
      test, solved.statistics.reciprocal_condition <= 1 + 8 * epsilon);
  ASC_DENSE_TEST_CHECK(test, solved.statistics.reciprocal_pivot_growth > 0);
  for (asc::extent_t j = 0; j < solved.nrhs; ++j) {
    long double error = 0;
    long double norm = 0;
    for (asc::extent_t i = 0; i < solved.n; ++i) {
      const auto actual = ToWide(solved.x[solved.Offset(3, i, j)]);
      const auto expected = ToWide(TrueSolution<T>(i, j));
      error = std::max(error, Magnitude(actual - expected));
      norm = std::max(norm, Magnitude(actual));
    }
    ASC_DENSE_TEST_CHECK(test, error < 64 * epsilon * norm);
    ASC_DENSE_TEST_CHECK(test, std::isfinite(solved.ferr[j + 1]));
    ASC_DENSE_TEST_CHECK(test, solved.ferr[j + 1] >= error / norm / 8);
    ASC_DENSE_TEST_CHECK(
        test, solved.berr[j + 1] >= 0 && solved.berr[j + 1] < 16 * epsilon);
  }
  ASC_DENSE_TEST_EQ(test, original.layouts, solved.layouts);
}

template <typename T>
void CheckScaling(TestContext& test, const Sample<T>& original,
                  const Sample<T>& solved, asc::DenseBlasTranspose trans) {
  using Real = asc::DenseBlasRealType<T>;
  const auto epsilon = std::numeric_limits<Real>::epsilon();
  for (asc::extent_t i = 0; i < solved.n; ++i) {
    for (asc::extent_t j = 0; j < solved.n; ++j) {
      auto expected = ToWide(original.a[original.Offset(0, i, j)]);
      if (RowScaled(solved.equed)) {
        expected *= solved.rows[i + 1];
      }
      if (ColumnScaled(solved.equed)) {
        expected *= solved.columns[j + 1];
      }
      ASC_DENSE_TEST_CHECK(
          test, std::abs(ToWide(solved.a[solved.Offset(0, i, j)]) - expected) <=
                    4 * epsilon * std::abs(expected));
    }
    for (asc::extent_t j = 0; j < solved.nrhs; ++j) {
      auto expected = ToWide(original.b[original.Offset(2, i, j)]);
      if (trans == kNone && RowScaled(solved.equed)) {
        expected *= solved.rows[i + 1];
      }
      if (trans != kNone && ColumnScaled(solved.equed)) {
        expected *= solved.columns[i + 1];
      }
      ASC_DENSE_TEST_CHECK(
          test, std::abs(ToWide(solved.b[solved.Offset(2, i, j)]) - expected) <=
                    4 * epsilon * std::abs(expected));
    }
  }
}

template <typename T>
void CheckResidual(TestContext& test, const Sample<T>& original,
                   const Sample<T>& solved, asc::DenseBlasTranspose trans) {
  auto restored = original;
  restored.x = solved.x;
  const auto epsilon =
      std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon();
  for (asc::extent_t j = 0; j < restored.nrhs; ++j) {
    ASC_DENSE_TEST_CHECK(test,
                         BackwardError(restored, trans, j) < 32 * epsilon);
  }
}

template <typename T>
void FactorSupplied(TestContext& test,
                    const asc::ReferenceLapackProvider& provider,
                    Sample<T>& sample) {
  std::array<T, 80> packed{};
  for (asc::extent_t i = 0; i < sample.n; ++i) {
    for (asc::extent_t j = 0; j < sample.n; ++j) {
      auto& a = sample.a[sample.Offset(0, i, j)];
      if (RowScaled(sample.equed)) {
        a *= sample.rows[i + 1];
      }
      if (ColumnScaled(sample.equed)) {
        a *= sample.columns[j + 1];
      }
      packed[j * 10 + i] = a;
    }
  }
  const auto factors = Take(asc::DenseBlasMatrixView<T>::Create(
      packed.data(), sample.n, sample.n, kColumn, 10,
      {packed.data(), sizeof(packed), kHost}));
  auto pivots = Vector(sample.pivots, sample.n);
  const auto plan = Take(asc::QueryGetrfWorkspace(provider, factors, pivots));
  alignas(16) std::array<std::byte, 64> integer{};
  asc::LapackWorkspace workspace;
  workspace.regions[kInteger] = {integer.data(), sizeof(integer), kHost};
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(
      test,
      asc::Getrf(provider, factors, pivots, plan, workspace, report).ok());
  for (asc::extent_t i = 0; i < sample.n; ++i) {
    for (asc::extent_t j = 0; j < sample.n; ++j) {
      sample.af[sample.Offset(1, i, j)] = packed[j * 10 + i];
    }
  }
}

template <typename T>
void SetDiagonal(Sample<T>& sample, int kind) {
  using Real = asc::DenseBlasRealType<T>;
  for (asc::extent_t i = 0; i < sample.n; ++i) {
    const Real middle =
        kind == 0 ? Real{0} : std::numeric_limits<Real>::epsilon() / 16;
    const Real diagonal = i == 1 ? middle : Real{1};
    sample.pivots[i + 1] = i + 1;
    for (asc::extent_t j = 0; j < sample.n; ++j) {
      sample.a[sample.Offset(0, i, j)] = i == j ? T{diagonal} : T{};
      sample.af[sample.Offset(1, i, j)] = i == j ? T{diagonal} : T{};
    }
    for (asc::extent_t j = 0; j < sample.nrhs; ++j) {
      sample.b[sample.Offset(2, i, j)] = diagonal * TrueSolution<T>(i, j);
    }
  }
}

using Fault = asc_lapack_test::LuDriverPivotFault;
struct PivotMode {
  int n;
  int nrhs;
  unsigned int layouts;
  asc::DenseBlasTranspose trans;
  char fact;
  asc::LapackEquilibration equed;
  // -1: ordinary nonsingular; 0: exact singular; 1: condition warning.
  int kind = -1;
};
template <typename T>
Sample<T> PrepareCase(TestContext& test,
                      const asc::ReferenceLapackProvider& provider,
                      const PivotMode& mode, Sample<T>& original) {
  using Real = asc::DenseBlasRealType<T>;
  Prepare(mode.trans, mode.fact == 'E' ? -100 : 0, original);
  if (mode.kind >= 0) {
    SetDiagonal(original, mode.kind);
  }
  auto sample = original;
  if (mode.fact == 'F') {
    sample.equed = mode.equed;
    for (asc::extent_t i = 0; i < sample.n; ++i) {
      sample.rows[i + 1] = RowScaled(mode.equed)
                               ? std::ldexp(Real{1}, static_cast<int>(i - 1))
                               : std::numeric_limits<Real>::quiet_NaN();
      sample.columns[i + 1] = ColumnScaled(mode.equed)
                                  ? std::ldexp(Real{1}, static_cast<int>(1 - i))
                                  : std::numeric_limits<Real>::quiet_NaN();
    }
    if (mode.kind != 0) {
      FactorSupplied(test, provider, sample);
    }
  } else if (mode.fact == 'E') {
    // An output-only enum is not read by preflight or passed as FACT=E input.
    sample.equed = asc::LapackEquilibration::kColumns;
  }
  return sample;
}
template <typename T>
void CheckPublication(TestContext& test, const PivotMode& mode, Fault fault,
                      const Sample<T>& before, const Sample<T>& positive,
                      const Sample<T>& after) {
  const bool defect = mode.n != 0 && mode.fact != 'F' && fault != Fault::kNone;
  ASC_DENSE_TEST_EQ(test, after.a,
                    defect && after.Layout(0) == kRow ? before.a : positive.a);
  ASC_DENSE_TEST_EQ(
      test, after.af,
      defect && after.Layout(1) == kRow ? before.af : positive.af);
  ASC_DENSE_TEST_EQ(test, after.b,
                    defect && after.Layout(2) == kRow ? before.b : positive.b);
  ASC_DENSE_TEST_EQ(test, after.x,
                    defect && after.Layout(3) == kRow ? before.x : positive.x);
  ASC_DENSE_TEST_EQ(test, after.pivots,
                    defect ? before.pivots : positive.pivots);
  ASC_DENSE_TEST_EQ(test, after.equed, defect ? before.equed : positive.equed);
  ASC_DENSE_TEST_CHECK(test, SameRepresentation(after.rows, positive.rows));
  ASC_DENSE_TEST_CHECK(test,
                       SameRepresentation(after.columns, positive.columns));
  ASC_DENSE_TEST_EQ(test, after.ferr, positive.ferr);
  ASC_DENSE_TEST_EQ(test, after.berr, positive.berr);
  ASC_DENSE_TEST_EQ(test, after.statistics.reciprocal_condition,
                    positive.statistics.reciprocal_condition);
  ASC_DENSE_TEST_EQ(test, after.statistics.reciprocal_pivot_growth,
                    defect ? before.statistics.reciprocal_pivot_growth
                           : positive.statistics.reciprocal_pivot_growth);
  if (mode.fact == 'N' || mode.fact == 'F') {
    ASC_DENSE_TEST_EQ(test, after.a, before.a);
  }
  if (mode.fact == 'N') {
    ASC_DENSE_TEST_EQ(test, after.b, before.b);
  }
  if (mode.fact == 'F') {
    ASC_DENSE_TEST_EQ(test, after.af, before.af);
    ASC_DENSE_TEST_EQ(test, after.pivots, before.pivots);
    ASC_DENSE_TEST_CHECK(test, SameRepresentation(after.rows, before.rows));
    ASC_DENSE_TEST_CHECK(test,
                         SameRepresentation(after.columns, before.columns));
  }
  CheckAllPadding(test, before, after);
  for (std::size_t i = static_cast<std::size_t>(mode.nrhs) + 1;
       i < after.ferr.size(); ++i) {
    ASC_DENSE_TEST_EQ(test, after.ferr[i], before.ferr[i]);
    ASC_DENSE_TEST_EQ(test, after.berr[i], before.berr[i]);
  }
}
template <typename T>
void CheckNumerics(TestContext& test, const PivotMode& mode,
                   const Sample<T>& original, const Sample<T>& before,
                   const Sample<T>& sample) {
  using Real = asc::DenseBlasRealType<T>;
  if (mode.n == 0) {
    ASC_DENSE_TEST_EQ(test, sample.statistics.reciprocal_condition, Real{1});
    ASC_DENSE_TEST_EQ(test, sample.statistics.reciprocal_pivot_growth, Real{1});
    ASC_DENSE_TEST_EQ(test, sample.x, before.x);
    for (int j = 0; j < mode.nrhs; ++j) {
      ASC_DENSE_TEST_EQ(test, sample.ferr[j + 1], Real{0});
      ASC_DENSE_TEST_EQ(test, sample.berr[j + 1], Real{0});
    }
    return;
  }
  CheckReconstruction(test, sample);
  CheckGrowth(test, sample, mode.kind == 0 ? 2 : sample.n);
  CheckScaling(test, original, sample, mode.trans);
  if (mode.kind == 0) {
    ASC_DENSE_TEST_EQ(test, sample.statistics.reciprocal_condition, Real{0});
    ASC_DENSE_TEST_EQ(test, sample.x, before.x);
    ASC_DENSE_TEST_EQ(test, sample.ferr, before.ferr);
    ASC_DENSE_TEST_EQ(test, sample.berr, before.berr);
  } else {
    CheckSolution(test, original, sample);
    CheckResidual(test, original, sample, mode.trans);
    if (mode.kind == 1) {
      ASC_DENSE_TEST_EQ(test, sample.statistics.reciprocal_condition,
                        mode.fact == 'E'
                            ? Real{1}
                            : std::numeric_limits<Real>::epsilon() / 16);
    }
  }
}
template <typename T>
void Reuse(TestContext& test, const asc::ReferenceLapackProvider& provider,
           const Sample<T>& original, Sample<T>& sample,
           asc::DenseBlasTranspose trans) {
  sample.b = original.b;
  sample.x = original.x;
  const auto before = sample;
  const auto plan = Take(sample.Query(provider, trans, 'F'));
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                               return sample.Execute(provider, trans, 'F', plan,
                                                     sample.Workspace(plan),
                                                     report);
                             }).ok());
  ASC_DENSE_TEST_EQ(test, sample.a, before.a);
  ASC_DENSE_TEST_EQ(test, sample.af, before.af);
  ASC_DENSE_TEST_EQ(test, sample.pivots, before.pivots);
  ASC_DENSE_TEST_CHECK(test, SameRepresentation(sample.rows, before.rows));
  ASC_DENSE_TEST_CHECK(test,
                       SameRepresentation(sample.columns, before.columns));
  CheckSolution(test, original, sample);
  CheckResidual(test, original, sample, trans);
  CheckScaling(test, original, sample, trans);
  sample.Guards(test, plan);
}

std::size_t g_cases = 0;
template <typename T>
std::array<std::byte, 144> WarmWorkspace(
    TestContext& test, const asc::ReferenceLapackProvider& provider, int n,
    char scalar) {
  Sample<T> warm(n, 0, 0);
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n; ++j) {
      warm.a[warm.Offset(0, i, j)] = i == j ? Value<T>(2 + i) : T{};
    }
  }
  asc_lapack_test::SetLuDriverPivotFault(Fault::kNone);
  const auto plan = Take(WithoutAllocation(
      test, [&] { return warm.Query(provider, kNone, 'N'); }));
  ASC_DENSE_TEST_EQ(test, asc_lapack_test::ObserveLuDriverPivot().calls, 0U);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                               return warm.Execute(provider, kNone, 'N', plan,
                                                   warm.Workspace(plan),
                                                   report);
                             }).ok());
  const auto observed = asc_lapack_test::ObserveLuDriverPivot();
  ASC_DENSE_TEST_EQ(test, observed.calls, n == 0 ? 0U : 1U);
  ASC_DENSE_TEST_EQ(test, observed.actual_info, 0);
  ASC_DENSE_TEST_EQ(test, report.called_provider, n != 0);
  if (n == 0) {
    ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
  } else {
    ASC_DENSE_TEST_EQ(test, report.native_info, 0);
  }
  for (int i = 0; i < n; ++i) {
    ASC_DENSE_TEST_EQ(test, warm.pivots[i + 1], i + 1);
    ASC_DENSE_TEST_EQ(test, observed.actual[i], i + 1);
  }
  warm.Guards(test, plan);
  std::printf("GESVX_PIVOT_WARM scalar=%c n=%d calls=%zu info=%lld\n", scalar,
              n, observed.calls, static_cast<long long>(observed.actual_info));
  // This is actual caller workspace left by a successful prior native call.
  return warm.integer;
}
int ExpectedInfo(const PivotMode& mode) {
  if (mode.kind == 0) {
    return 2;
  }
  return mode.kind == 1 && mode.fact != 'E' ? 4 : 0;
}
void CheckReport(TestContext& test,
                 const asc::ReferenceLapackProvider& provider,
                 const PivotMode& mode, char scalar, Fault fault,
                 const asc::Status& status, const asc::LapackReport& report) {
  const auto observed = asc_lapack_test::ObserveLuDriverPivot();
  const bool called = mode.n != 0;
  const bool defect = called && mode.fact != 'F' && fault != Fault::kNone;
  const int actual = ExpectedInfo(mode);
  ASC_DENSE_TEST_EQ(test, observed.calls, called ? 1U : 0U);
  ASC_DENSE_TEST_EQ(test, observed.entries, static_cast<std::size_t>(mode.n));
  ASC_DENSE_TEST_EQ(test, observed.valid_actual,
                    static_cast<std::size_t>(mode.n));
  ASC_DENSE_TEST_EQ(test, observed.actual_info, actual);
  ASC_DENSE_TEST_EQ(test, observed.redirected_output,
                    called && mode.fact != 'F');
  ASC_DENSE_TEST_CHECK(test, observed.input_preserved);
  ASC_DENSE_TEST_EQ(test, report.called_provider, called);
  ASC_DENSE_TEST_EQ(test, report.provider, provider.identity());
  const std::array routine{scalar, 'g', 'e', 's', 'v', 'x', '\0'};
  ASC_DENSE_TEST_EQ(test, std::string_view(report.routine.data()),
                    std::string_view(routine.data()));
  ASC_DENSE_TEST_CHECK(test, !report.native_argument.has_value());
  if (called) {
    ASC_DENSE_TEST_EQ(test, report.native_info, actual);
  } else {
    ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
  }
  ASC_DENSE_TEST_EQ(test, report.factor_family.has_value(), mode.fact != 'F');
  if (mode.fact != 'F') {
    ASC_DENSE_TEST_EQ(test, report.factor_family,
                      asc::LapackFactorFamily::kLuPartialPivot);
  }
  if (actual == 2) {
    ASC_DENSE_TEST_EQ(test, report.diagnostic_index, 1);
  } else {
    ASC_DENSE_TEST_CHECK(test, !report.diagnostic_index.has_value());
  }
  if (defect) {
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
    ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kPartialResult);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kUnusable);
  } else {
    ASC_DENSE_TEST_EQ(
        test, status.code(),
        actual == 0 ? asc::ErrorCode::kOk : asc::ErrorCode::kNumerical);
    auto expected = asc::LapackOutcome::kSuccess;
    if (actual != 0) {
      expected = actual == 2 ? asc::LapackOutcome::kSingular
                             : asc::LapackOutcome::kAccuracyWarning;
    }
    ASC_DENSE_TEST_EQ(test, report.outcome, expected);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      actual == 0
                          ? asc::LapackOutputValidity::kComplete
                          : asc::LapackOutputValidity::kDocumentedPartial);
  }
  std::printf(
      "GESVX_PIVOT_RESULT calls=%zu n=%zu valid_actual=%zu info=%lld "
      "redirected=%d input_preserved=%d status=%d\n",
      observed.calls, observed.entries, observed.valid_actual,
      static_cast<long long>(observed.actual_info),
      static_cast<int>(observed.redirected_output),
      static_cast<int>(observed.input_preserved),
      static_cast<int>(status.code()));
}
template <typename T>
void PivotCase(TestContext& test, const asc::ReferenceLapackProvider& provider,
               const PivotMode& mode, char scalar) {
  Sample<T> original(mode.n, mode.nrhs, mode.layouts);
  auto before = PrepareCase(test, provider, mode, original);
  before.integer = WarmWorkspace<T>(test, provider, mode.n, scalar);
  auto positive = before;
  for (auto fault : {Fault::kNone, Fault::kOmitAll, Fault::kOmitLast,
                     Fault::kShortAll, Fault::kShortLast}) {
    if (fault >= Fault::kShortAll && ASC_LAPACK_INTEGER_BITS != 64) {
      continue;
    }
    auto sample = before;
    asc_lapack_test::SetLuDriverPivotFault(fault);
    const auto plan = Take(WithoutAllocation(
        test, [&] { return sample.Query(provider, mode.trans, mode.fact); }));
    ASC_DENSE_TEST_EQ(test, asc_lapack_test::ObserveLuDriverPivot().calls, 0U);
    const auto work = sample.Workspace(plan);
    asc::LapackReport report;
    report.native_info = 701;
    report.native_argument = 702;
    report.diagnostic_index = 703;
    std::printf(
        "GESVX_PIVOT_MODE scalar=%c n=%d nrhs=%d layouts=%u trans=%d fact=%c "
        "equed=%d kind=%d fault=%d\n",
        scalar, mode.n, mode.nrhs, mode.layouts, static_cast<int>(mode.trans),
        mode.fact, static_cast<int>(mode.equed), mode.kind,
        static_cast<int>(fault));
    const auto status = WithoutAllocation(test, [&] {
      return sample.Execute(provider, mode.trans, mode.fact, plan, work,
                            report);
    });
    ++g_cases;
    CheckReport(test, provider, mode, scalar, fault, status, report);
    if (fault == Fault::kNone) {
      CheckNumerics(test, mode, original, before, sample);
      positive = sample;
    }
    CheckPublication(test, mode, fault, before, positive, sample);
    sample.Guards(test, plan);
  }
  if (mode.n != 0 && mode.kind < 0) {
    asc_lapack_test::SetLuDriverPivotFault(Fault::kNone);
    Reuse(test, provider, original, positive, mode.trans);
    const auto observed = asc_lapack_test::ObserveLuDriverPivot();
    ASC_DENSE_TEST_EQ(test, observed.calls, 1U);
    ASC_DENSE_TEST_CHECK(
        test, observed.input_preserved && !observed.redirected_output);
    std::printf(
        "GESVX_PIVOT_REUSE scalar=%c n=%d nrhs=%d layouts=%u trans=%d fact=%c "
        "equed=%d info=%lld\n",
        scalar, mode.n, mode.nrhs, mode.layouts, static_cast<int>(mode.trans),
        mode.fact, static_cast<int>(mode.equed),
        static_cast<long long>(observed.actual_info));
  }
}
template <typename T>
void RunPivots(TestContext& test, const asc::ReferenceLapackProvider& provider,
               char scalar) {
  for (int n : {0, 1, 3}) {
    for (int nrhs : {0, 1, 2}) {
      for (unsigned int layouts = 0; layouts < 16; ++layouts) {
        for (auto trans : {kNone, kTranspose, kConjugate}) {
          for (char fact : {'N', 'E', 'F'}) {
            for (auto equed : {asc::LapackEquilibration::kNone,
                               asc::LapackEquilibration::kRows,
                               asc::LapackEquilibration::kColumns,
                               asc::LapackEquilibration::kBoth}) {
              if (fact != 'F' && equed != asc::LapackEquilibration::kNone) {
                continue;
              }
              PivotCase<T>(test, provider,
                           {n, nrhs, layouts, trans, fact, equed}, scalar);
            }
          }
        }
      }
    }
  }
  for (unsigned int layouts = 0; layouts < 16; ++layouts) {
    for (auto trans : {kNone, kTranspose, kConjugate}) {
      for (char fact : {'N', 'E', 'F'}) {
        for (int kind : {0, 1}) {
          if (kind == 0 && fact == 'F') {
            continue;  // Existing driver test retains the singular preflight.
          }
          PivotCase<T>(test, provider,
                       {3, 2, layouts, trans, fact,
                        asc::LapackEquilibration::kNone, kind},
                       scalar);
        }
      }
    }
  }
}
}  // namespace asc_driver_test
namespace driver = asc_driver_test;
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 2) {
    return 2;
  }
  const auto provider = driver::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  driver::TestContext test;
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    driver::RunPivots<float>(test, provider, 's');
  } else if (scalar == "d") {
    driver::RunPivots<double>(test, provider, 'd');
  } else if (scalar == "c") {
    driver::RunPivots<std::complex<float>>(test, provider, 'c');
  } else if (scalar == "z") {
    driver::RunPivots<std::complex<double>>(test, provider, 'z');
  } else {
    return 2;
  }
  std::printf("GESVX_PIVOT_COMPLETED scalar=%s abi=%d cases=%zu\n", argv[1],
              ASC_LAPACK_INTEGER_BITS, driver::g_cases);
  return test.Finish();
}
