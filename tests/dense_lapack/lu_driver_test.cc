#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
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
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu_driver.h"
#include "lu_driver_faults.h"
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
void CheckPlan(TestContext& test, const Sample<T>& sample,
               const asc::LapackWorkspacePlan& plan) {
  ASC_DENSE_TEST_EQ(test, plan.regions[kScalar].minimum_entries,
                    asc::DenseBlasComplex<T>
                        ? 2 * sample.n
                        : std::max<asc::extent_t>(1, 4 * sample.n));
  ASC_DENSE_TEST_EQ(test, plan.regions[kScalar].entry_bytes, sizeof(T));
  ASC_DENSE_TEST_EQ(
      test, plan.regions[kReal].minimum_entries,
      asc::DenseBlasComplex<T> ? std::max<asc::extent_t>(1, 2 * sample.n) : 0);
  ASC_DENSE_TEST_EQ(test, plan.regions[kInteger].minimum_entries,
                    (asc::DenseBlasComplex<T> ? 1 : 2) * sample.n);
  ASC_DENSE_TEST_EQ(
      test, plan.regions[kInteger].entry_bytes,
      plan.identity.provider().integer_abi == asc::LapackIntegerAbi::kLp64
          ? 4U
          : 8U);
  ASC_DENSE_TEST_EQ(test, plan.regions[kPivots].minimum_entries, 0);
  asc::extent_t packing = 0;
  for (unsigned int operand = 0; operand < 4; ++operand) {
    if (sample.Layout(operand) == kRow) {
      packing += sample.n * (operand < 2 ? sample.n : sample.nrhs);
    }
  }
  ASC_DENSE_TEST_EQ(test, plan.regions[kLayout].minimum_entries, packing);
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
  ASC_DENSE_TEST_EQ(test, sample.rows, before.rows);
  ASC_DENSE_TEST_EQ(test, sample.columns, before.columns);
  CheckSolution(test, original, sample);
  CheckResidual(test, original, sample, trans);
  CheckScaling(test, original, sample, trans);
  sample.Guards(test, plan);
}

template <typename T>
void NumericalCases(TestContext& test,
                    const asc::ReferenceLapackProvider& provider) {
  for (auto trans : {kNone, kTranspose, kConjugate}) {
    for (unsigned int layouts = 0; layouts < 16; ++layouts) {
      for (asc::extent_t n : {1, 3, 8}) {
        for (asc::extent_t nrhs : {1, 3}) {
          for (int exponent : {-60, 0, 60}) {
            for (char mode : {'N', 'E'}) {
              Sample<T> sample(n, nrhs, layouts);
              Prepare(trans, exponent, sample);
              const auto original = sample;
              const auto plan = Take(sample.Query(provider, trans, mode));
              CheckPlan(test, sample, plan);
              asc::LapackReport report;
              ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                                           return sample.Execute(
                                               provider, trans, mode, plan,
                                               sample.Workspace(plan), report);
                                         }).ok());
              ASC_DENSE_TEST_EQ(test, report.native_info, 0);
              ASC_DENSE_TEST_CHECK(test, report.called_provider);
              ASC_DENSE_TEST_CHECK(
                  test, asc::LapackLuFactorView<T>::Create(
                            sample.Matrix(std::as_const(sample.af), 1),
                            sample.Pivots(), report)
                            .ok());
              CheckSolution(test, original, sample);
              CheckResidual(test, original, sample, trans);
              CheckReconstruction(test, sample);
              CheckScaling(test, original, sample, trans);
              CheckAllPadding(test, original, sample);
              CheckGrowth(test, sample, sample.n);
              sample.Guards(test, plan);
              Reuse(test, provider, original, sample, trans);
            }
          }
        }
      }
    }
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
void SuppliedScaling(TestContext& test,
                     const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  using Equed = asc::LapackEquilibration;
  for (auto trans : {kNone, kTranspose, kConjugate}) {
    for (auto equed :
         {Equed::kNone, Equed::kRows, Equed::kColumns, Equed::kBoth}) {
      for (unsigned int layouts = 0; layouts < 16; ++layouts) {
        Sample<T> sample(3, 3, layouts);
        Prepare(trans, 0, sample);
        const auto original = sample;
        sample.equed = equed;
        for (asc::extent_t i = 0; i < sample.n; ++i) {
          sample.rows[i + 1] =
              RowScaled(equed) ? std::ldexp(Real{1}, static_cast<int>(i - 1))
                               : std::numeric_limits<Real>::quiet_NaN();
          sample.columns[i + 1] =
              ColumnScaled(equed) ? std::ldexp(Real{1}, static_cast<int>(1 - i))
                                  : std::numeric_limits<Real>::quiet_NaN();
        }
        FactorSupplied(test, provider, sample);
        const auto before = sample;
        const auto plan = Take(sample.Query(provider, trans, 'F'));
        asc::LapackReport report;
        ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                                     return sample.Execute(
                                         provider, trans, 'F', plan,
                                         sample.Workspace(plan), report);
                                   }).ok());
        ASC_DENSE_TEST_EQ(test, report.native_info, 0);
        ASC_DENSE_TEST_EQ(test, sample.a, before.a);
        ASC_DENSE_TEST_EQ(test, sample.af, before.af);
        ASC_DENSE_TEST_EQ(test, sample.pivots, before.pivots);
        ASC_DENSE_TEST_CHECK(test,
                             SameRepresentation(sample.rows, before.rows));
        ASC_DENSE_TEST_CHECK(
            test, SameRepresentation(sample.columns, before.columns));
        CheckSolution(test, original, sample);
        CheckResidual(test, original, sample, trans);
        CheckScaling(test, original, sample, trans);
        CheckReconstruction(test, sample);
        CheckGrowth(test, sample, sample.n);
        CheckAllPadding(test, before, sample);
        sample.Guards(test, plan);
      }
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

template <typename T>
void FailureOutputs(TestContext& test,
                    const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  for (unsigned int layout : {0U, 15U}) {
    for (char mode : {'N', 'E', 'F'}) {
      for (int kind : {0, 1}) {
        Sample<T> sample(3, 3, layout);
        SetDiagonal(sample, kind);
        const auto before = sample;
        const auto plan = Take(sample.Query(provider, kNone, mode));
        asc::LapackReport report;
        const auto status = WithoutAllocation(test, [&] {
          return sample.Execute(provider, kNone, mode, plan,
                                sample.Workspace(plan), report);
        });
        const bool preflight = mode == 'F' && kind == 0;
        ASC_DENSE_TEST_EQ(test, status.ok(), mode == 'E' && kind == 1);
        ASC_DENSE_TEST_EQ(test, report.called_provider, !preflight);
        if (preflight) {
          ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
          ASC_DENSE_TEST_EQ(test, sample.statistics.reciprocal_condition,
                            Real{-113});
          ASC_DENSE_TEST_EQ(test, sample.statistics.reciprocal_pivot_growth,
                            Real{-127});
        } else {
          ASC_DENSE_TEST_EQ(test, report.native_info,
                            kind == 0 ? 2 : (mode == 'E' ? 0 : 4));
          ASC_DENSE_TEST_EQ(test, sample.statistics.reciprocal_pivot_growth,
                            Real{1});
        }
        if (kind == 0) {
          ASC_DENSE_TEST_EQ(test, report.outcome,
                            asc::LapackOutcome::kSingular);
          ASC_DENSE_TEST_EQ(test, report.diagnostic_index, 1);
          ASC_DENSE_TEST_EQ(test, sample.x, before.x);
          ASC_DENSE_TEST_EQ(test, sample.ferr, before.ferr);
          ASC_DENSE_TEST_EQ(test, sample.berr, before.berr);
          if (!preflight) {
            ASC_DENSE_TEST_EQ(test, sample.statistics.reciprocal_condition,
                              Real{0});
            CheckReconstruction(test, sample);
            CheckGrowth(test, sample, 2);
          }
        } else {
          ASC_DENSE_TEST_EQ(test, sample.statistics.reciprocal_condition,
                            mode == 'E'
                                ? Real{1}
                                : std::numeric_limits<Real>::epsilon() / 16);
          if (mode != 'E') {
            ASC_DENSE_TEST_EQ(test, report.outcome,
                              asc::LapackOutcome::kAccuracyWarning);
            ASC_DENSE_TEST_EQ(test, report.output_validity,
                              asc::LapackOutputValidity::kDocumentedPartial);
            ASC_DENSE_TEST_CHECK(
                test, !asc::LapackLuFactorView<T>::Create(
                           sample.Matrix(std::as_const(sample.af), 1),
                           sample.Pivots(), report)
                           .ok());
          }
          CheckSolution(test, before, sample);
          CheckResidual(test, before, sample, kNone);
        }
        sample.Guards(test, plan);
      }
    }
  }
}

template <typename T>
void EmptyCases(TestContext& test,
                const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  for (unsigned int layout : {0U, 15U}) {
    for (char mode : {'N', 'E', 'F'}) {
      for (const auto shape :
           {std::array<asc::extent_t, 2>{0, 0}, {0, 3}, {3, 0}}) {
        Sample<T> sample(shape[0], shape[1], layout);
        SetDiagonal(sample, 1);
        const auto before = sample;
        const auto plan = Take(sample.Query(provider, kNone, mode));
        asc::LapackReport report;
        const auto status = WithoutAllocation(test, [&] {
          return sample.Execute(provider, kNone, mode, plan,
                                sample.Workspace(plan), report);
        });
        ASC_DENSE_TEST_EQ(test, status.ok(), sample.n == 0 || mode == 'E');
        ASC_DENSE_TEST_EQ(test, report.called_provider, sample.n != 0);
        ASC_DENSE_TEST_EQ(test, sample.x, before.x);
        if (sample.n == 0) {
          ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
          ASC_DENSE_TEST_EQ(test, sample.statistics.reciprocal_condition,
                            Real{1});
          ASC_DENSE_TEST_EQ(test, sample.statistics.reciprocal_pivot_growth,
                            Real{1});
          if constexpr (asc::DenseBlasComplex<T>) {
            ASC_DENSE_TEST_EQ(test, sample.real[1], Real{1});
          } else {
            ASC_DENSE_TEST_EQ(test, sample.scalar[1], T{1});
          }
          for (asc::extent_t j = 1; j <= sample.nrhs; ++j) {
            ASC_DENSE_TEST_EQ(test, sample.ferr[j], Real{0});
            ASC_DENSE_TEST_EQ(test, sample.berr[j], Real{0});
          }
        } else {
          ASC_DENSE_TEST_EQ(test, report.native_info, mode == 'E' ? 0 : 4);
          CheckReconstruction(test, sample);
        }
        sample.Guards(test, plan);
      }
    }
  }
}

template <typename T>
void PreflightCases(TestContext& test,
                    const asc::ReferenceLapackProvider& provider) {
  for (char mode : {'N', 'E', 'F'}) {
    Sample<T> sample(3, 3, 15);
    SetDiagonal(sample, 1);
    const auto plan = Take(sample.Query(provider, kNone, mode));
    const auto before = sample;
    for (int fault = 0; fault < 9; ++fault) {
      auto supplied = plan;
      auto workspace = sample.Workspace(plan);
      if (fault == 0) {
        workspace.regions[kScalar] = {nullptr, 0, kHost};
      }
      if (fault == 1) {
        workspace.regions[kInteger] = {sample.integer.data() + 1,
                                       sizeof(sample.integer) - 1, kHost};
      }
      if (fault == 2) {
        workspace.regions[kLayout] = {sample.a.data(), sizeof(sample.a), kHost};
      }
      if (fault == 3) {
        ++supplied.regions[kInteger].entry_bytes;
      }
      if (fault == 4) {
        workspace.regions[kScalar] = {sample.x.data(), sizeof(sample.x), kHost};
      }
      if (fault == 5) {
        workspace.regions[kInteger] = {sample.integer.data() + 8,
                                       sizeof(sample.integer) - 8,
                                       asc::MemorySpace::kDevice};
      }
      if (fault == 6) {
        supplied.total_byte_limit = 0;
      }
      if (fault == 7) {
        supplied = Take(sample.Query(provider, kTranspose, mode));
      }
      if (fault == 8) {
        ++supplied.regions[kScalar].minimum_entries;
      }
      asc::LapackReport report;
      const auto status = WithoutAllocation(test, [&] {
        return sample.Execute(provider, kNone, mode, supplied, workspace,
                              report);
      });
      ASC_DENSE_TEST_CHECK(test, !status.ok());
      ASC_DENSE_TEST_CHECK(
          test, !report.called_provider && !report.native_info.has_value());
      ASC_DENSE_TEST_EQ(test, sample.a, before.a);
      ASC_DENSE_TEST_EQ(test, sample.af, before.af);
      ASC_DENSE_TEST_EQ(test, sample.b, before.b);
      ASC_DENSE_TEST_EQ(test, sample.x, before.x);
      ASC_DENSE_TEST_EQ(test, sample.pivots, before.pivots);
      ASC_DENSE_TEST_EQ(test, sample.rows, before.rows);
      ASC_DENSE_TEST_EQ(test, sample.columns, before.columns);
      ASC_DENSE_TEST_EQ(test, sample.ferr, before.ferr);
      ASC_DENSE_TEST_EQ(test, sample.berr, before.berr);
      ASC_DENSE_TEST_EQ(test, sample.equed, before.equed);
      ASC_DENSE_TEST_EQ(test, sample.statistics.reciprocal_condition,
                        before.statistics.reciprocal_condition);
      ASC_DENSE_TEST_EQ(test, sample.statistics.reciprocal_pivot_growth,
                        before.statistics.reciprocal_pivot_growth);
      ASC_DENSE_TEST_EQ(test, sample.scalar, before.scalar);
      ASC_DENSE_TEST_EQ(test, sample.real, before.real);
      ASC_DENSE_TEST_EQ(test, sample.integer, before.integer);
      ASC_DENSE_TEST_EQ(test, sample.packed, before.packed);
    }
  }
}

void ProviderDefects(TestContext& test,
                     const asc::ReferenceLapackProvider& provider) {
  using asc_lapack_test::DriverFault;
  for (auto fault : {DriverFault::kNegative, DriverFault::kExcessiveInfo,
                     DriverFault::kBadPivot, DriverFault::kInvalidEqued,
                     DriverFault::kChangedEqued, DriverFault::kNanRcond,
                     DriverFault::kNanFerr, DriverFault::kNegativeBerr,
                     DriverFault::kInfiniteGrowth}) {
    Sample<double> sample(3, 3, 15);
    Prepare(kNone, 0, sample);
    auto expected = sample;
    const auto plan = Take(sample.Query(provider, kNone, 'N'));
    asc::LapackReport report;
    ASC_DENSE_TEST_CHECK(test, expected
                                   .Execute(provider, kNone, 'N', plan,
                                            expected.Workspace(plan), report)
                                   .ok());
    asc_lapack_test::SetDriverFault(fault);
    const auto status = WithoutAllocation(test, [&] {
      return sample.Execute(provider, kNone, 'N', plan, sample.Workspace(plan),
                            report);
    });
    asc_lapack_test::SetDriverFault(DriverFault::kNone);
    const bool warning = fault == DriverFault::kNanRcond ||
                         fault == DriverFault::kNanFerr ||
                         fault == DriverFault::kInfiniteGrowth;
    const bool estimate_defect = warning || fault == DriverFault::kNegativeBerr;
    ASC_DENSE_TEST_EQ(
        test, status.code(),
        warning ? asc::ErrorCode::kNumerical : asc::ErrorCode::kProvider);
    ASC_DENSE_TEST_CHECK(test, report.called_provider);
    ASC_DENSE_TEST_EQ(test, report.native_info,
                      fault == DriverFault::kNegative
                          ? -8
                          : (fault == DriverFault::kExcessiveInfo ? 5 : 0));
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      estimate_defect
                          ? asc::LapackOutputValidity::kDocumentedPartial
                          : asc::LapackOutputValidity::kUnusable);
    if (estimate_defect) {
      ASC_DENSE_TEST_EQ(test, sample.x, expected.x);
    }
    if (fault == DriverFault::kNegative) {
      ASC_DENSE_TEST_EQ(test, report.native_argument, 8);
    }
    if (fault == DriverFault::kNanRcond) {
      ASC_DENSE_TEST_CHECK(test,
                           std::isnan(sample.statistics.reciprocal_condition));
    }
    if (fault == DriverFault::kNanFerr) {
      ASC_DENSE_TEST_CHECK(test, std::isnan(sample.ferr[1]));
    }
    if (fault == DriverFault::kNegativeBerr) {
      ASC_DENSE_TEST_EQ(test, sample.berr[1], -1);
    }
    if (fault == DriverFault::kInfiniteGrowth) {
      ASC_DENSE_TEST_CHECK(
          test, std::isinf(sample.statistics.reciprocal_pivot_growth));
    }
    sample.Guards(test, plan);
  }
}

template <typename T>
void EmptyLargeStrides(TestContext& test,
                       const asc::ReferenceLapackProvider& provider) {
  Sample<T> sample(0, 3, 15);
  const asc::extent_t stride =
      static_cast<asc::extent_t>(std::numeric_limits<std::int32_t>::max()) + 1;
  const auto a = Take(asc::DenseBlasMatrixView<const T>::Create(
      nullptr, 0, 0, kRow, stride, {nullptr, 0, kHost}));
  const auto af = Take(asc::DenseBlasMatrixView<T>::Create(
      nullptr, 0, 0, kRow, stride + 1, {nullptr, 0, kHost}));
  const auto b = Take(asc::DenseBlasMatrixView<const T>::Create(
      nullptr, 0, 3, kRow, stride + 2, {nullptr, 0, kHost}));
  const auto x = Take(asc::DenseBlasMatrixView<T>::Create(
      nullptr, 0, 3, kRow, stride + 3, {nullptr, 0, kHost}));
  const auto plan = Take(asc::QueryGesvxWorkspace(
      provider, kNone, a, af, Vector(sample.pivots, 0), b, x,
      Vector(sample.ferr, 3), Vector(sample.berr, 3), sample.statistics));
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(
      test, WithoutAllocation(test, [&] {
              return asc::Gesvx(
                  provider, kNone, a, af, Vector(sample.pivots, 0), b, x,
                  Vector(sample.ferr, 3), Vector(sample.berr, 3),
                  sample.statistics, plan, sample.Workspace(plan), report);
            }).ok());
  ASC_DENSE_TEST_CHECK(
      test, !report.called_provider && !report.native_info.has_value());
  ASC_DENSE_TEST_EQ(test, sample.statistics.reciprocal_condition, 1);
  const auto changed = Take(asc::DenseBlasMatrixView<const T>::Create(
      nullptr, 0, 0, kRow, stride + 4, {nullptr, 0, kHost}));
  sample.statistics.reciprocal_condition = -1;
  ASC_DENSE_TEST_EQ(test,
                    WithoutAllocation(test,
                                      [&] {
                                        return asc::Gesvx(
                                            provider, kNone, changed, af,
                                            Vector(sample.pivots, 0), b, x,
                                            Vector(sample.ferr, 3),
                                            Vector(sample.berr, 3),
                                            sample.statistics, plan,
                                            sample.Workspace(plan), report);
                                      })
                        .code(),
                    asc::ErrorCode::kInvalidState);
  ASC_DENSE_TEST_EQ(test, sample.statistics.reciprocal_condition, -1);
  sample.Guards(test, plan);
}

template <typename T>
void InvalidSuppliedScales(TestContext& test,
                           const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  Sample<T> sample(3, 3, 15);
  Prepare(kNone, 0, sample);
  sample.equed = asc::LapackEquilibration::kRows;
  sample.rows.fill(1);
  FactorSupplied(test, provider, sample);
  const auto plan = Take(sample.Query(provider, kNone, 'F'));
  for (Real bad : {Real{-1}, Real{0}, std::numeric_limits<Real>::infinity(),
                   std::numeric_limits<Real>::quiet_NaN()}) {
    sample.rows[2] = bad;
    const auto before = sample;
    asc::LapackReport report;
    ASC_DENSE_TEST_EQ(test,
                      WithoutAllocation(test,
                                        [&] {
                                          return sample.Execute(
                                              provider, kNone, 'F', plan,
                                              sample.Workspace(plan), report);
                                        })
                          .code(),
                      asc::ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_CHECK(
        test, !report.called_provider && !report.native_info.has_value());
    ASC_DENSE_TEST_EQ(test, sample.a, before.a);
    ASC_DENSE_TEST_EQ(test, sample.af, before.af);
    ASC_DENSE_TEST_EQ(test, sample.b, before.b);
    ASC_DENSE_TEST_EQ(test, sample.x, before.x);
    ASC_DENSE_TEST_EQ(test, sample.ferr, before.ferr);
    ASC_DENSE_TEST_EQ(test, sample.berr, before.berr);
    ASC_DENSE_TEST_EQ(test, sample.statistics.reciprocal_condition,
                      before.statistics.reciprocal_condition);
    ASC_DENSE_TEST_EQ(test, sample.scalar, before.scalar);
    ASC_DENSE_TEST_EQ(test, sample.real, before.real);
    ASC_DENSE_TEST_EQ(test, sample.integer, before.integer);
    ASC_DENSE_TEST_EQ(test, sample.packed, before.packed);
  }
}

template <typename T>
struct DriverOperands {
  using Real = asc::DenseBlasRealType<T>;
  asc::DenseBlasMatrixView<const T> a;
  asc::DenseBlasMatrixView<const T> af;
  asc::RawLapackPivotView pivots;
  asc::DenseBlasVectorView<const Real> r;
  asc::DenseBlasVectorView<const Real> c;
  asc::DenseBlasMatrixView<T> b;
  asc::DenseBlasMatrixView<T> x;
  asc::DenseBlasVectorView<Real> ferr;
  asc::DenseBlasVectorView<Real> berr;
  asc::DenseBlasTranspose trans;
};

template <typename T>
void DamageScales(Sample<T>& sample, int fault, DriverOperands<T>& values) {
  using Real = asc::DenseBlasRealType<T>;
  if (fault == 0) {
    values.r = Take(asc::DenseBlasVectorView<const Real>::Create(
        nullptr, 0, 1, {nullptr, 0, kHost}));
  }
  if (fault == 1) {
    values.r = Take(asc::DenseBlasVectorView<const Real>::Create(
        sample.rows.data() + 1, 3, 2,
        {sample.rows.data(), sizeof(sample.rows), kHost}));
  }
  if (fault == 2) {
    values.r = Take(asc::DenseBlasVectorView<const Real>::Create(
        sample.rows.data() + 1, 3, 1,
        {sample.rows.data(), sizeof(sample.rows), asc::MemorySpace::kDevice}));
  }
  if (fault == 3) {
    values.r = values.ferr;
  }
  if (fault == 4) {
    values.pivots = Take(asc::RawLapackPivotView::Create(
        sample.pivots.data() + 1, 3, asc::LapackFactorFamily::kRook,
        {sample.pivots.data(), sizeof(sample.pivots), kHost}));
  }
  if (fault == 5) {
    sample.pivots[1] = 0;
  }
}

template <typename T>
void DamageMatrices(Sample<T>& sample, int fault, DriverOperands<T>& values) {
  using Real = asc::DenseBlasRealType<T>;
  if (fault == 6) {
    values.a = values.af;
  }
  if (fault == 7) {
    values.b = values.x;
  }
  if (fault == 8) {
    values.af = Take(asc::DenseBlasMatrixView<const T>::Create(
        sample.af.data() + 1, 3, 2, kRow, 10,
        {sample.af.data(), sizeof(sample.af), kHost}));
  }
  if (fault == 9) {
    values.trans = static_cast<asc::DenseBlasTranspose>(255);
  }
  if (fault == 10) {
    values.ferr = Vector(sample.ferr, 2);
  }
  if (fault == 11) {
    values.ferr = Take(asc::DenseBlasVectorView<Real>::Create(
        sample.rows.data() + 1, 3, 2,
        {sample.rows.data(), sizeof(sample.rows), kHost}));
  }
  if (fault == 12) {
    values.b = Take(asc::DenseBlasMatrixView<T>::Create(
        sample.b.data() + 1, 3, 1, kRow, 5,
        {sample.b.data(), sizeof(sample.b), kHost}));
    values.x = Take(asc::DenseBlasMatrixView<T>::Create(
        sample.x.data() + 1, 3, 1, kRow, 5,
        {sample.x.data(), sizeof(sample.x), kHost}));
    values.ferr = Take(asc::DenseBlasVectorView<Real>::Create(
        &sample.statistics.reciprocal_condition, 1, 1,
        {&sample.statistics, sizeof(sample.statistics), kHost}));
    values.berr = Vector(sample.berr, 1);
  }
  if (fault == 13) {
    values.c = Vector(sample.columns, 2);
  }
  if (fault == 14) {
    sample.af[1] = T{};
  }
}

template <typename T>
DriverOperands<T> BadDescriptors(Sample<T>& sample, int fault) {
  using Real = asc::DenseBlasRealType<T>;
  DriverOperands<T> values{
      sample.Matrix(std::as_const(sample.a), 0),
      sample.Matrix(std::as_const(sample.af), 1),
      sample.Pivots(),
      static_cast<asc::DenseBlasVectorView<const Real>>(Vector(sample.rows, 3)),
      static_cast<asc::DenseBlasVectorView<const Real>>(
          Vector(sample.columns, 3)),
      sample.Matrix(sample.b, 2),
      sample.Matrix(sample.x, 3),
      Vector(sample.ferr, 3),
      Vector(sample.berr, 3),
      kNone};
  DamageScales(sample, fault, values);
  DamageMatrices(sample, fault, values);
  return values;
}

template <typename T>
void DescriptorCases(TestContext& test,
                     const asc::ReferenceLapackProvider& provider) {
  Sample<T> base(3, 3, 15);
  Prepare(kNone, 0, base);
  base.equed = asc::LapackEquilibration::kRows;
  base.rows.fill(1);
  FactorSupplied(test, provider, base);
  const auto plan = Take(base.Query(provider, kNone, 'F'));
  for (int fault = 0; fault < 15; ++fault) {
    auto sample = base;
    const auto values = BadDescriptors(sample, fault);
    const auto before = sample;
    asc::LapackReport report;
    ASC_DENSE_TEST_CHECK(
        test, !WithoutAllocation(test, [&] {
                 return asc::GesvxFactored(
                     provider, values.trans, values.a, values.af, values.pivots,
                     sample.equed, values.r, values.c, values.b, values.x,
                     values.ferr, values.berr, sample.statistics, plan,
                     sample.Workspace(plan), report);
               }).ok());
    ASC_DENSE_TEST_CHECK(
        test, !report.called_provider && !report.native_info.has_value());
    ASC_DENSE_TEST_EQ(test, sample.a, before.a);
    ASC_DENSE_TEST_EQ(test, sample.af, before.af);
    ASC_DENSE_TEST_EQ(test, sample.b, before.b);
    ASC_DENSE_TEST_EQ(test, sample.x, before.x);
    ASC_DENSE_TEST_EQ(test, sample.ferr, before.ferr);
    ASC_DENSE_TEST_EQ(test, sample.berr, before.berr);
    ASC_DENSE_TEST_EQ(test, sample.statistics.reciprocal_condition,
                      before.statistics.reciprocal_condition);
    ASC_DENSE_TEST_EQ(test, sample.statistics.reciprocal_pivot_growth,
                      before.statistics.reciprocal_pivot_growth);
    ASC_DENSE_TEST_EQ(test, sample.scalar, before.scalar);
    ASC_DENSE_TEST_EQ(test, sample.real, before.real);
    ASC_DENSE_TEST_EQ(test, sample.integer, before.integer);
    ASC_DENSE_TEST_EQ(test, sample.packed, before.packed);
  }
}

template <typename T>
void TinyInputFidelity(TestContext& test,
                       const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  const auto epsilon = std::numeric_limits<Real>::epsilon();
  const Real safe1 = 2 * std::numeric_limits<Real>::min();
  for (char mode : {'N', 'E', 'F'}) {
    for (auto trans : {kNone, kTranspose, kConjugate}) {
      for (unsigned int layouts : {0U, 15U}) {
        for (bool extreme : {false, true}) {
          Sample<T> sample(1, 1, layouts);
          const Real tiny = std::numeric_limits<Real>::min() *
                            (extreme ? Real{1} / 1024 : Real{2});
          sample.a[1] = tiny;
          sample.af[1] = tiny;
          sample.b[1] = tiny;
          sample.pivots[1] = 1;
          const auto plan = Take(sample.Query(provider, trans, mode));
          asc::LapackReport report;
          const auto status = WithoutAllocation(test, [&] {
            return sample.Execute(provider, trans, mode, plan,
                                  sample.Workspace(plan), report);
          });
          // Direct pinned source reproduces this failure of finite RCOND/FERR
          // expectations. Fidelity/warning checks do not close that math gate.
          const bool limited = extreme && mode != 'E';
          ASC_DENSE_TEST_EQ(
              test, status.code(),
              limited ? asc::ErrorCode::kNumerical : asc::ErrorCode::kOk);
          ASC_DENSE_TEST_EQ(test, report.native_info, limited ? 2 : 0);
          ASC_DENSE_TEST_EQ(test, report.outcome,
                            limited ? asc::LapackOutcome::kAccuracyWarning
                                    : asc::LapackOutcome::kSuccess);
          ASC_DENSE_TEST_EQ(test, report.output_validity,
                            limited
                                ? asc::LapackOutputValidity::kDocumentedPartial
                                : asc::LapackOutputValidity::kComplete);
          ASC_DENSE_TEST_CHECK(test, report.called_provider);
          ASC_DENSE_TEST_EQ(test, sample.x[1], T{1});
          ASC_DENSE_TEST_EQ(test, sample.statistics.reciprocal_condition,
                            limited ? Real{0} : Real{1});
          ASC_DENSE_TEST_EQ(test, sample.statistics.reciprocal_pivot_growth,
                            Real{1});
          const Real expected_berr = mode == 'E' && trans == kNone
                                         ? Real{0}
                                         : safe1 / (2 * tiny + safe1);
          ASC_DENSE_TEST_CHECK(
              test, std::abs(sample.berr[1] - expected_berr) < 16 * epsilon);
          if (limited) {
            if constexpr (asc::DenseBlasComplex<T>) {
              ASC_DENSE_TEST_CHECK(test, trans == kNone
                                             ? std::isnan(sample.ferr[1])
                                             : std::isinf(sample.ferr[1]));
            } else {
              ASC_DENSE_TEST_CHECK(test, std::isinf(sample.ferr[1]));
            }
          } else {
            ASC_DENSE_TEST_CHECK(
                test, std::isfinite(sample.ferr[1]) && sample.ferr[1] >= 0);
          }
          if (mode == 'E') {
            ASC_DENSE_TEST_EQ(test, sample.equed,
                              asc::LapackEquilibration::kRows);
            ASC_DENSE_TEST_EQ(test, sample.a[1],
                              T{extreme ? Real{1} / 1024 : Real{1}});
            ASC_DENSE_TEST_EQ(test, sample.b[1],
                              trans == kNone ? sample.a[1] : T{tiny});
          } else {
            ASC_DENSE_TEST_EQ(test, sample.a[1], T{tiny});
            ASC_DENSE_TEST_EQ(test, sample.af[1], T{tiny});
            ASC_DENSE_TEST_EQ(test, sample.b[1], T{tiny});
          }
          sample.Guards(test, plan);
        }
      }
    }
  }
}

template <typename T>
void EmptyUnusedScales(TestContext& test,
                       const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  using Equed = asc::LapackEquilibration;
  const auto empty = Take(asc::DenseBlasVectorView<const Real>::Create(
      nullptr, 0, 1, {nullptr, 0, kHost}));
  for (auto equed :
       {Equed::kNone, Equed::kRows, Equed::kColumns, Equed::kBoth}) {
    Sample<T> sample(3, 3, 15);
    Prepare(kNone, 0, sample);
    const auto original = sample;
    sample.equed = equed;
    sample.rows.fill(1);
    sample.columns.fill(1);
    FactorSupplied(test, provider, sample);
    const auto r = RowScaled(equed)
                       ? static_cast<asc::DenseBlasVectorView<const Real>>(
                             Vector(sample.rows, 3))
                       : empty;
    const auto c = ColumnScaled(equed)
                       ? static_cast<asc::DenseBlasVectorView<const Real>>(
                             Vector(sample.columns, 3))
                       : empty;
    const auto a = sample.Matrix(std::as_const(sample.a), 0);
    const auto af = sample.Matrix(std::as_const(sample.af), 1);
    const auto b = sample.Matrix(sample.b, 2);
    const auto x = sample.Matrix(sample.x, 3);
    const auto plan = Take(asc::QueryGesvxFactoredWorkspace(
        provider, kNone, a, af, sample.Pivots(), equed, r, c, b, x,
        Vector(sample.ferr, 3), Vector(sample.berr, 3), sample.statistics));
    asc::LapackReport report;
    ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                                 return asc::GesvxFactored(
                                     provider, kNone, a, af, sample.Pivots(),
                                     equed, r, c, b, x, Vector(sample.ferr, 3),
                                     Vector(sample.berr, 3), sample.statistics,
                                     plan, sample.Workspace(plan), report);
                               }).ok());
    ASC_DENSE_TEST_EQ(test, report.native_info, 0);
    ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
    CheckSolution(test, original, sample);
    CheckResidual(test, original, sample, kNone);
    sample.Guards(test, plan);
  }
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  NumericalCases<T>(test, provider);
  SuppliedScaling<T>(test, provider);
  FailureOutputs<T>(test, provider);
  EmptyCases<T>(test, provider);
  PreflightCases<T>(test, provider);
  EmptyLargeStrides<T>(test, provider);
  InvalidSuppliedScales<T>(test, provider);
  DescriptorCases<T>(test, provider);
  TinyInputFidelity<T>(test, provider);
  EmptyUnusedScales<T>(test, provider);
}
}  // namespace asc_driver_test

int main(int argc, char** argv) {
  if (argc != 2) {
    return 2;
  }
  asc_dense_test::TestContext test;
  const auto provider = asc_driver_test::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const std::string_view scalar = argv[1];
  if (scalar == "s") {
    asc_driver_test::Run<float>(test, provider);
  } else if (scalar == "d") {
    asc_driver_test::Run<double>(test, provider);
  } else if (scalar == "c") {
    asc_driver_test::Run<std::complex<float>>(test, provider);
  } else if (scalar == "z") {
    asc_driver_test::Run<std::complex<double>>(test, provider);
  } else {
    return 2;
  }
  asc_driver_test::ProviderDefects(test, provider);
  const int result = test.Finish();
  if (result == 0) {
    std::printf("GESVX %s: initial numerical/three-mode/layout checks passed\n",
                argv[1]);
  }
  return result;
}
