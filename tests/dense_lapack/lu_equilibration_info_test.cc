#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu_equilibration.h"
#include "installed_lu/normal_return_guard.h"
#include "lapack_build_config.h"
#include "lu_aux_info_faults.h"
#include "lu_aux_info_test_support.h"

namespace asc_lu_aux_info_test {
template <typename T>
void FillEquilibration(Matrix<T>& matrix, int kind) {
  using Real = asc::DenseBlasRealType<T>;
  for (int i = 0; i < matrix.rows; ++i) {
    for (int j = 0; j < matrix.columns; ++j) {
      const Real v = std::ldexp(Real{1}, i + j);
      if ((kind == 1 && i + 1 == matrix.rows) ||
          (kind == 2 && j + 1 == matrix.columns)) {
        matrix.At(i, j) = T{};
      } else if constexpr (asc::DenseBlasComplex<T>) {
        matrix.At(i, j) = T{v / 2, v / 2};
      } else {
        matrix.At(i, j) = v;
      }
    }
  }
}
template <typename Real>
void CheckScales(TestContext& test, int m, int n,
                 const std::array<Real, 5>& rows,
                 const std::array<Real, 5>& columns,
                 const asc::LapackEquilibrationStatistics<Real>& stats) {
  for (int i = 0; i < m; ++i) {
    ASC_DENSE_TEST_EQ(test, rows[i + 1], std::ldexp(Real{1}, -i - n + 1));
  }
  for (int j = 0; j < n; ++j) {
    ASC_DENSE_TEST_EQ(test, columns[j + 1], std::ldexp(Real{1}, n - 1 - j));
  }
  ASC_DENSE_TEST_EQ(test, stats.row_condition, std::ldexp(Real{1}, 1 - m));
  ASC_DENSE_TEST_EQ(test, stats.column_condition, std::ldexp(Real{1}, 1 - n));
  ASC_DENSE_TEST_EQ(test, stats.absolute_maximum,
                    std::ldexp(Real{1}, m + n - 2));
}
template <typename Real>
void CheckEquilibrationOutputs(
    TestContext& test, int m, int n, int actual, Fault fault,
    const std::array<Real, 5>& rows, const std::array<Real, 5>& columns,
    const asc::LapackEquilibrationStatistics<Real>& stats,
    std::array<Real, 5>& positive_rows, std::array<Real, 5>& positive_columns,
    asc::LapackEquilibrationStatistics<Real>& positive_stats) {
  if (m != 0 && n != 0 && actual == 0) {
    CheckScales(test, m, n, rows, columns, stats);
  }
  if (m == 0 || n == 0) {
    ASC_DENSE_TEST_EQ(test, stats.row_condition, Real{1});
    ASC_DENSE_TEST_EQ(test, stats.column_condition, Real{1});
    ASC_DENSE_TEST_EQ(test, stats.absolute_maximum, Real{0});
    for (auto v : rows) {
      ASC_DENSE_TEST_EQ(test, v, Real{-53});
    }
    for (auto v : columns) {
      ASC_DENSE_TEST_EQ(test, v, Real{-59});
    }
  }
  if (fault == Fault::kNone) {
    positive_rows = rows;
    positive_columns = columns;
    positive_stats = stats;
  }
  ASC_DENSE_TEST_EQ(test, rows, positive_rows);
  ASC_DENSE_TEST_EQ(test, columns, positive_columns);
  ASC_DENSE_TEST_EQ(test, stats.row_condition, positive_stats.row_condition);
  ASC_DENSE_TEST_EQ(test, stats.column_condition,
                    positive_stats.column_condition);
  ASC_DENSE_TEST_EQ(test, stats.absolute_maximum,
                    positive_stats.absolute_maximum);
  ASC_DENSE_TEST_EQ(test, rows.front(), Real{-53});
  ASC_DENSE_TEST_EQ(test, columns.front(), Real{-59});
  for (std::size_t i = static_cast<std::size_t>(m) + 1; i < rows.size(); ++i) {
    ASC_DENSE_TEST_EQ(test, rows[i], Real{-53});
  }
  for (std::size_t i = static_cast<std::size_t>(n) + 1; i < columns.size();
       ++i) {
    ASC_DENSE_TEST_EQ(test, columns[i], Real{-59});
  }
}
template <typename T>
void EquilibrationCase(TestContext& test,
                       const asc::ReferenceLapackProvider& provider, int m,
                       int n, asc::DenseBlasLayout layout, bool radix, int kind,
                       char scalar) {
  using Real = asc::DenseBlasRealType<T>;
  Matrix<T> matrix(m, n, layout);
  FillEquilibration(matrix, kind);
  const auto before = matrix.data;
  std::array<Real, 5> positive_rows{};
  std::array<Real, 5> positive_columns{};
  asc::LapackEquilibrationStatistics<Real> positive_stats;
  const bool called = m != 0 && n != 0;
  int actual = 0;
  if (kind != 0) {
    actual = kind == 1 ? m : m + n;
  }
  for (auto fault : {Fault::kNone, Fault::kWithhold, Fault::kShortZero}) {
    if (fault == Fault::kShortZero && ASC_LAPACK_INTEGER_BITS != 64) {
      continue;
    }
    std::array<Real, 5> rows;
    std::array<Real, 5> columns;
    rows.fill(Real{-53});
    columns.fill(Real{-59});
    asc::LapackEquilibrationStatistics<Real> stats{-31, -37, -41};
    asc_lapack_test::SetLuAuxInfoFault(fault);
    const auto plan = Take(Observe(test, [&] {
      return radix ? asc::QueryGeequbWorkspace(provider, matrix.ConstView(),
                                               Vector(rows, m),
                                               Vector(columns, n), stats)
                   : asc::QueryGeequWorkspace(provider, matrix.ConstView(),
                                              Vector(rows, m),
                                              Vector(columns, n), stats);
    }));
    ASC_DENSE_TEST_EQ(test, asc_lapack_test::ObserveLuAuxInfo().calls, 0U);
    Scratch<T> scratch;
    const auto work = scratch.Workspace(plan);
    auto report = DirtyReport();
    std::printf(
        "LU_AUX_MODE family=equilibration scalar=%c m=%d n=%d layout=%d "
        "radix=%d kind=%d fault=%d\n",
        scalar, m, n, static_cast<int>(layout), static_cast<int>(radix), kind,
        static_cast<int>(fault));
    const auto status = Observe(test, [&] {
      return radix ? asc::Geequb(provider, matrix.ConstView(), Vector(rows, m),
                                 Vector(columns, n), stats, plan, work, report)
                   : asc::Geequ(provider, matrix.ConstView(), Vector(rows, m),
                                Vector(columns, n), stats, plan, work, report);
    });
    auto outcome = asc::LapackOutcome::kSuccess;
    if (actual != 0) {
      outcome = radix ? asc::LapackOutcome::kPartialResult
                      : asc::LapackOutcome::kSingular;
    }
    CheckInfo(test, provider, status, report, called, fault,
              Name(scalar, radix ? "geequb" : "geequ").data(), actual, outcome,
              actual == 0 ? asc::LapackOutputValidity::kComplete
                          : asc::LapackOutputValidity::kDocumentedPartial);
    ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
    if (actual != 0 && fault == Fault::kNone) {
      ASC_DENSE_TEST_EQ(test, report.diagnostic_index,
                        kind == 1 ? m - 1 : n - 1);
    } else {
      ASC_DENSE_TEST_CHECK(test, !report.diagnostic_index.has_value());
    }
    CheckEquilibrationOutputs(test, m, n, actual, fault, rows, columns, stats,
                              positive_rows, positive_columns, positive_stats);
    ASC_DENSE_TEST_EQ(test, matrix.data, before);
    scratch.Guards(test);
  }
}
template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider,
         char scalar) {
  for (auto shape : {std::array{0, 0}, std::array{0, 3}, std::array{3, 0},
                     std::array{1, 1}, std::array{3, 2}, std::array{2, 3}}) {
    for (auto layout : {kRow, kColumn}) {
      for (bool radix : {false, true}) {
        for (int kind : {0, 1, 2}) {
          if (kind != 0 && (shape[0] < 2 || shape[1] < 2)) {
            continue;
          }
          EquilibrationCase<T>(test, provider, shape[0], shape[1], layout,
                               radix, kind, scalar);
        }
      }
    }
  }
}
}  // namespace asc_lu_aux_info_test
namespace aux_test = asc_lu_aux_info_test;
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 2) {
    return 2;
  }
  const auto provider = aux_test::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  aux_test::TestContext test;
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    aux_test::Run<float>(test, provider, 's');
  } else if (scalar == "d") {
    aux_test::Run<double>(test, provider, 'd');
  } else if (scalar == "c") {
    aux_test::Run<std::complex<float>>(test, provider, 'c');
  } else if (scalar == "z") {
    aux_test::Run<std::complex<double>>(test, provider, 'z');
  } else {
    return 2;
  }
  std::printf(
      "LU_AUX_COMPLETED family=equilibration scalar=%s abi=%d cases=%zu\n",
      argv[1], ASC_LAPACK_INTEGER_BITS, aux_test::Cases());
  return test.Finish();
}
