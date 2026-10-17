#include <algorithm>
#include <array>
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
#include "asc/dense/providers/lapack_lu_refinement.h"
#include "installed_lu/normal_return_guard.h"
#include "lu_refinement_faults.h"
#include "lu_refinement_test_support.h"

namespace asc_refinement_test {
template <typename T>
void CheckPlan(TestContext& test, const Sample<T>& sample,
               const asc::LapackWorkspacePlan& plan) {
  const auto integer_width =
      plan.identity.provider().integer_abi == asc::LapackIntegerAbi::kLp64 ? 4U
                                                                           : 8U;
  ASC_DENSE_TEST_EQ(test, plan.regions[kInteger].entry_bytes, integer_width);
  ASC_DENSE_TEST_EQ(test, plan.regions[kInteger].minimum_entries,
                    (asc::DenseBlasComplex<T> ? 1 : 2) * sample.n);
  ASC_DENSE_TEST_EQ(test, plan.regions[kScalar].minimum_entries,
                    (asc::DenseBlasComplex<T> ? 2 : 3) * sample.n);
  ASC_DENSE_TEST_EQ(test, plan.regions[kReal].minimum_entries,
                    asc::DenseBlasComplex<T> ? sample.n : 0);
  ASC_DENSE_TEST_EQ(test, plan.regions[kPivots].minimum_entries, 0);
  asc::extent_t packing = 0;
  for (unsigned int i = 0; i < 4; ++i) {
    if (sample.Layout(i) == kRow) {
      packing += sample.n * (i < 2 ? sample.n : sample.nrhs);
    }
  }
  ASC_DENSE_TEST_EQ(test, plan.regions[kLayout].minimum_entries, packing);
}

template <typename T>
void CheckNumericalOutputs(TestContext& test, const Sample<T>& before,
                           const Sample<T>& after,
                           asc::DenseBlasTranspose trans) {
  using Real = asc::DenseBlasRealType<T>;
  const auto epsilon = std::numeric_limits<Real>::epsilon();
  ASC_DENSE_TEST_EQ(test, before.a, after.a);
  ASC_DENSE_TEST_EQ(test, before.af, after.af);
  ASC_DENSE_TEST_EQ(test, before.b, after.b);
  ASC_DENSE_TEST_EQ(test, before.pivots, after.pivots);
  for (asc::extent_t column = 0; column < after.nrhs; ++column) {
    const auto initial_error = BackwardError(before, trans, column);
    const auto final_error = BackwardError(after, trans, column);
    ASC_DENSE_TEST_CHECK(test, final_error < initial_error / 8);
    ASC_DENSE_TEST_CHECK(test, final_error < 16 * epsilon);
    const Real ferr = after.ferr[1 + column];
    const Real berr = after.berr[1 + column];
    ASC_DENSE_TEST_CHECK(test, std::isfinite(ferr) && ferr > epsilon / 32);
    ASC_DENSE_TEST_CHECK(test, ferr < 128 * epsilon);
    ASC_DENSE_TEST_CHECK(test, std::isfinite(berr) && berr >= 0);
    ASC_DENSE_TEST_CHECK(test, std::abs(berr - final_error) < 8 * epsilon);
    long double solution_norm = 0;
    long double forward_error = 0;
    for (asc::extent_t i = 0; i < after.n; ++i) {
      const auto actual = ToWide(after.x[after.Offset(3, i, column)]);
      const auto expected = ToWide(TrueSolution<T>(i, column));
      solution_norm = std::max(solution_norm, Magnitude(actual));
      forward_error = std::max(forward_error, Magnitude(actual - expected));
    }
    ASC_DENSE_TEST_CHECK(test, forward_error / solution_norm < 8 * ferr);
  }
  for (std::size_t i = 0; i < after.x.size(); ++i) {
    bool value = false;
    for (asc::extent_t row = 0; row < after.n; ++row) {
      for (asc::extent_t column = 0; column < after.nrhs; ++column) {
        value |= i == after.Offset(3, row, column);
      }
    }
    if (!value) {
      ASC_DENSE_TEST_EQ(test, after.x[i], before.x[i]);
    }
  }
}

template <typename T>
void NumericalCases(TestContext& test,
                    const asc::ReferenceLapackProvider& provider) {
  for (unsigned int layouts = 0; layouts < 16; ++layouts) {
    for (auto trans : {kNone, kTranspose, kConjugate}) {
      for (asc::extent_t n : {1, 3, 8}) {
        for (asc::extent_t nrhs : {1, 3}) {
          for (int exponent : {-60, 0, 60}) {
            Sample<T> sample(n, nrhs, layouts);
            Prepare(test, provider, trans, exponent, sample);
            const auto before = sample;
            const auto plan = Take(WithoutAllocation(
                test, [&] { return sample.Query(provider, trans); }));
            CheckPlan(test, sample, plan);
            ASC_DENSE_TEST_EQ(test, before.x, sample.x);
            ASC_DENSE_TEST_EQ(test, before.ferr, sample.ferr);
            ASC_DENSE_TEST_EQ(test, before.berr, sample.berr);
            for (int repetition = 0; repetition < 2; ++repetition) {
              asc::LapackReport report;
              ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                                           return sample.Execute(
                                               provider, trans, plan,
                                               sample.Workspace(plan), report);
                                         }).ok());
              ASC_DENSE_TEST_CHECK(test,
                                   report.called_provider &&
                                       report.provider == provider.identity());
              ASC_DENSE_TEST_EQ(test, report.native_info, 0);
              ASC_DENSE_TEST_EQ(test, report.output_validity,
                                asc::LapackOutputValidity::kComplete);
              CheckNumericalOutputs(test, before, sample, trans);
              sample.Guards(test, plan);
            }
          }
        }
      }
    }
  }
}

template <typename T>
void TinySafeguard(TestContext& test,
                   const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  const Real safe1 = 2 * std::numeric_limits<Real>::min();
  const Real epsilon = std::numeric_limits<Real>::epsilon();
  for (auto trans : {kNone, kTranspose, kConjugate}) {
    for (unsigned int layouts : {0U, 15U}) {
      for (bool extreme : {false, true}) {
        const Real tiny = std::numeric_limits<Real>::min() *
                          (extreme ? Real{1} / 1024 : Real{2});
        Sample<T> sample(1, 1, layouts);
        sample.a[1] = tiny;
        sample.af[1] = tiny;
        sample.pivots[1] = 1;
        sample.b[1] = tiny;
        sample.x[1] = Value<T>(0.75L);
        const auto plan = Take(sample.Query(provider, trans));
        asc::LapackReport report;
        const auto status = WithoutAllocation(test, [&] {
          return sample.Execute(provider, trans, plan, sample.Workspace(plan),
                                report);
        });
        ASC_DENSE_TEST_EQ(
            test, status.code(),
            extreme ? asc::ErrorCode::kNumerical : asc::ErrorCode::kOk);
        ASC_DENSE_TEST_EQ(test, report.outcome,
                          extreme ? asc::LapackOutcome::kAccuracyWarning
                                  : asc::LapackOutcome::kSuccess);
        ASC_DENSE_TEST_EQ(test, report.output_validity,
                          extreme
                              ? asc::LapackOutputValidity::kDocumentedPartial
                              : asc::LapackOutputValidity::kComplete);
        ASC_DENSE_TEST_CHECK(test, report.called_provider);
        if (extreme) {
          ASC_DENSE_TEST_EQ(test, report.diagnostic_index, 0);
        }
        const Real expected_berr = safe1 / (2 * tiny + safe1);
        const Real expected_ferr = safe1 / tiny;
        ASC_DENSE_TEST_CHECK(test, std::abs(sample.x[1] - T{1}) < 16 * epsilon);
        ASC_DENSE_TEST_CHECK(
            test, std::abs(sample.berr[1] - expected_berr) < 16 * epsilon);
        if (!extreme) {
          ASC_DENSE_TEST_CHECK(test, std::abs(sample.ferr[1] - expected_ferr) <
                                         64 * epsilon * expected_ferr);
        } else {
          // Fidelity to an independently reproduced upstream limitation.
          // Finite FERR (~2048) mathematical success remains an unmet gate.
          if constexpr (asc::DenseBlasComplex<T>) {
            ASC_DENSE_TEST_CHECK(test, trans == kNone
                                           ? std::isnan(sample.ferr[1])
                                           : std::isinf(sample.ferr[1]));
          } else {
            ASC_DENSE_TEST_CHECK(test, std::isinf(sample.ferr[1]));
          }
        }
        ASC_DENSE_TEST_EQ(test, report.native_info, 0);
        sample.Guards(test, plan);
      }
    }
  }
}

template <typename T>
void EmptyAndSingular(TestContext& test,
                      const asc::ReferenceLapackProvider& provider) {
  for (unsigned int layouts : {0U, 15U}) {
    for (const auto shape :
         {std::array<asc::extent_t, 2>{0, 0}, {0, 3}, {3, 0}, {3, 3}}) {
      Sample<T> sample(shape[0], shape[1], layouts);
      for (asc::extent_t i = 0; i < sample.n; ++i) {
        sample.pivots[1 + i] = i + 1;
        sample.af[sample.Offset(1, i, i)] = i == 1 ? T{0} : T{1};
      }
      const auto before = sample;
      const auto plan = Take(sample.Query(provider, kNone));
      asc::LapackReport report;
      const auto status = WithoutAllocation(test, [&] {
        return sample.Execute(provider, kNone, plan, sample.Workspace(plan),
                              report);
      });
      ASC_DENSE_TEST_CHECK(
          test, !report.called_provider && !report.native_info.has_value());
      ASC_DENSE_TEST_EQ(test, sample.x, before.x);
      ASC_DENSE_TEST_EQ(test, sample.scalar, before.scalar);
      ASC_DENSE_TEST_EQ(test, sample.packed, before.packed);
      if (sample.n != 0 && sample.nrhs != 0) {
        ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
        ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSingular);
        ASC_DENSE_TEST_EQ(test, report.diagnostic_index, 1);
        ASC_DENSE_TEST_EQ(test, report.output_validity,
                          asc::LapackOutputValidity::kUnchanged);
        ASC_DENSE_TEST_EQ(test, sample.ferr, before.ferr);
        ASC_DENSE_TEST_EQ(test, sample.berr, before.berr);
      } else {
        ASC_DENSE_TEST_CHECK(test, status.ok());
        for (asc::extent_t i = 0; i < sample.nrhs; ++i) {
          ASC_DENSE_TEST_EQ(test, sample.ferr[1 + i], 0);
          ASC_DENSE_TEST_EQ(test, sample.berr[1 + i], 0);
        }
      }
      sample.Guards(test, plan);
    }
  }
}

template <typename T>
void PreflightCases(TestContext& test,
                    const asc::ReferenceLapackProvider& provider) {
  for (int mode = 0; mode < 11; ++mode) {
    Sample<T> sample(3, 3, 15);
    Prepare(test, provider, kNone, 0, sample);
    auto plan = Take(sample.Query(provider, kNone));
    auto workspace = sample.Workspace(plan);
    auto trans = kNone;
    if (mode == 0) {
      workspace.regions[kScalar] = {nullptr, 0, kHost};
    }
    if (mode == 1) {
      workspace.regions[kInteger] = {sample.integer.data() + 8, 1, kHost};
    }
    if (mode == 2) {
      workspace.regions[kInteger] = {sample.integer.data() + 1, 48, kHost};
    }
    if (mode == 3) {
      workspace.regions[kLayout] = {sample.a.data(), 36 * sizeof(T), kHost};
    }
    if (mode == 4) {
      workspace.regions[kScalar] = {sample.scalar.data() + 1, 9 * sizeof(T),
                                    asc::MemorySpace::kDevice};
    }
    if (mode == 5) {
      plan.regions[kInteger].entry_bytes *= 2;
    }
    if (mode == 6) {
      trans = static_cast<asc::DenseBlasTranspose>(255);
    }
    if (mode == 7) {
      sample.pivots[2] = 1;
    }
    if (mode == 8) {
      plan.identity = Take(sample.Query(provider, kTranspose)).identity;
    }
    if (mode == 9) {
      workspace.regions[kScalar] = {
          sample.integer.data() + 8,
          static_cast<std::size_t>(plan.regions[kScalar].minimum_entries) *
              sizeof(T),
          kHost};
    }
    if (mode == 10) {
      plan.total_byte_limit = 0;
    }
    const auto before = sample;
    asc::LapackReport report;
    report.called_provider = true;
    report.native_info = 0;
    ASC_DENSE_TEST_CHECK(test, !WithoutAllocation(test, [&] {
                                  return sample.Execute(provider, trans, plan,
                                                        workspace, report);
                                }).ok());
    ASC_DENSE_TEST_CHECK(
        test, !report.called_provider && !report.native_info.has_value());
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kUnchanged);
    ASC_DENSE_TEST_EQ(test, sample.a, before.a);
    ASC_DENSE_TEST_EQ(test, sample.af, before.af);
    ASC_DENSE_TEST_EQ(test, sample.pivots, before.pivots);
    ASC_DENSE_TEST_EQ(test, sample.b, before.b);
    ASC_DENSE_TEST_EQ(test, sample.x, before.x);
    ASC_DENSE_TEST_EQ(test, sample.ferr, before.ferr);
    ASC_DENSE_TEST_EQ(test, sample.berr, before.berr);
    ASC_DENSE_TEST_EQ(test, sample.scalar, before.scalar);
    ASC_DENSE_TEST_EQ(test, sample.real, before.real);
    ASC_DENSE_TEST_EQ(test, sample.integer, before.integer);
    ASC_DENSE_TEST_EQ(test, sample.converted, before.converted);
    ASC_DENSE_TEST_EQ(test, sample.packed, before.packed);
  }
}

void ProviderDefects(TestContext& test,
                     const asc::ReferenceLapackProvider& provider) {
  using asc_lapack_test::RefinementFault;
  for (auto fault : {RefinementFault::kNegative, RefinementFault::kPositive}) {
    Sample<double> sample(3, 3, 15);
    Prepare(test, provider, kNone, 0, sample);
    const auto plan = Take(sample.Query(provider, kNone));
    asc::LapackReport report;
    asc_lapack_test::SetRefinementFault(fault);
    const auto status = WithoutAllocation(test, [&] {
      return sample.Execute(provider, kNone, plan, sample.Workspace(plan),
                            report);
    });
    asc_lapack_test::SetRefinementFault(RefinementFault::kNone);
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
    ASC_DENSE_TEST_CHECK(test, report.called_provider);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kUnusable);
    ASC_DENSE_TEST_EQ(test, report.native_info,
                      fault == RefinementFault::kNegative ? -7 : 1);
    if (fault == RefinementFault::kNegative) {
      ASC_DENSE_TEST_EQ(test, report.native_argument, 7);
    }
  }
}

void EstimateDefects(TestContext& test,
                     const asc::ReferenceLapackProvider& provider) {
  using asc_lapack_test::RefinementFault;
  for (auto fault :
       {RefinementFault::kNanForward, RefinementFault::kInfiniteBackward,
        RefinementFault::kNegativeForward,
        RefinementFault::kNegativeBackward}) {
    Sample<double> sample(3, 3, 15);
    Prepare(test, provider, kNone, 0, sample);
    auto expected = sample;
    const auto plan = Take(sample.Query(provider, kNone));
    asc::LapackReport report;
    ASC_DENSE_TEST_CHECK(test, expected
                                   .Execute(provider, kNone, plan,
                                            expected.Workspace(plan), report)
                                   .ok());
    asc_lapack_test::SetRefinementFault(fault);
    const auto status = WithoutAllocation(test, [&] {
      return sample.Execute(provider, kNone, plan, sample.Workspace(plan),
                            report);
    });
    asc_lapack_test::SetRefinementFault(RefinementFault::kNone);
    const bool negative = fault == RefinementFault::kNegativeForward ||
                          fault == RefinementFault::kNegativeBackward;
    ASC_DENSE_TEST_EQ(
        test, status.code(),
        negative ? asc::ErrorCode::kProvider : asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      negative ? asc::LapackOutcome::kPartialResult
                               : asc::LapackOutcome::kAccuracyWarning);
    ASC_DENSE_TEST_EQ(test, report.native_info, 0);
    ASC_DENSE_TEST_CHECK(test, report.called_provider);
    ASC_DENSE_TEST_EQ(test, report.diagnostic_index, 1);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kDocumentedPartial);
    ASC_DENSE_TEST_EQ(test, sample.x, expected.x);
    for (std::size_t i : {0U, 1U, 3U, 4U}) {
      ASC_DENSE_TEST_EQ(test, sample.ferr[i], expected.ferr[i]);
      ASC_DENSE_TEST_EQ(test, sample.berr[i], expected.berr[i]);
    }
    if (fault == RefinementFault::kNanForward) {
      ASC_DENSE_TEST_CHECK(test, std::isnan(sample.ferr[2]));
      ASC_DENSE_TEST_EQ(test, sample.berr[2], expected.berr[2]);
    } else if (fault == RefinementFault::kInfiniteBackward) {
      ASC_DENSE_TEST_CHECK(test, std::isinf(sample.berr[2]));
      ASC_DENSE_TEST_EQ(test, sample.ferr[2], expected.ferr[2]);
    } else if (fault == RefinementFault::kNegativeForward) {
      ASC_DENSE_TEST_EQ(test, sample.ferr[2], -1);
      ASC_DENSE_TEST_EQ(test, sample.berr[2], expected.berr[2]);
    } else {
      ASC_DENSE_TEST_EQ(test, sample.berr[2], -1);
      ASC_DENSE_TEST_EQ(test, sample.ferr[2], expected.ferr[2]);
    }
    sample.Guards(test, plan);
  }
}

template <typename T>
void DescriptorCases(TestContext& test,
                     const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  Sample<T> sample(3, 3, 15);
  Prepare(test, provider, kNone, 0, sample);
  const auto plan = Take(sample.Query(provider, kNone));
  for (int mode = 0; mode < 9; ++mode) {
    auto a = sample.Matrix(std::as_const(sample.a), 0);
    auto af = sample.Matrix(std::as_const(sample.af), 1);
    auto b = sample.Matrix(std::as_const(sample.b), 2);
    auto x = sample.Matrix(sample.x, 3);
    auto pivots = sample.Pivots();
    auto ferr = Vector(sample.ferr, 3);
    auto berr = Vector(sample.berr, 3);
    if (mode == 0) {
      af = a;
    }
    if (mode == 1) {
      b = x;
    }
    if (mode == 2) {
      ferr = berr;
    }
    if (mode == 3) {
      ferr = Vector(sample.ferr, 2);
    }
    if (mode == 4) {
      af = Take(asc::DenseBlasMatrixView<const T>::Create(
          sample.af.data() + 1, 3, 2, kRow, 10,
          {sample.af.data(), sizeof(sample.af), kHost}));
    }
    if (mode == 5) {
      a = Take(asc::DenseBlasMatrixView<const T>::Create(
          sample.a.data() + 1, 3, 3, kRow, 10,
          {sample.a.data(), sizeof(sample.a), asc::MemorySpace::kDevice}));
    }
    if (mode == 6) {
      pivots = Take(asc::RawLapackPivotView::Create(
          sample.pivots.data() + 1, 3, asc::LapackFactorFamily::kRook,
          {sample.pivots.data(), sizeof(sample.pivots), kHost}));
    }
    if (mode == 7) {
      ferr = Take(asc::DenseBlasVectorView<Real>::Create(
          sample.ferr.data(), 3, 2,
          {sample.ferr.data(), sizeof(sample.ferr), kHost}));
    }
    if (mode == 8) {
      ferr = Take(asc::DenseBlasVectorView<Real>::Create(
          sample.ferr.data() + 3, 3, -1,
          {sample.ferr.data(), sizeof(sample.ferr), kHost}));
    }
    const auto before = sample;
    asc::LapackReport report;
    ASC_DENSE_TEST_CHECK(
        test, !WithoutAllocation(test, [&] {
                 return asc::Gerfs(provider, kNone, a, af, pivots, b, x, ferr,
                                   berr, plan, sample.Workspace(plan), report);
               }).ok());
    ASC_DENSE_TEST_CHECK(
        test, !report.called_provider && !report.native_info.has_value());
    ASC_DENSE_TEST_EQ(test, sample.x, before.x);
    ASC_DENSE_TEST_EQ(test, sample.ferr, before.ferr);
    ASC_DENSE_TEST_EQ(test, sample.berr, before.berr);
    ASC_DENSE_TEST_EQ(test, sample.scalar, before.scalar);
    ASC_DENSE_TEST_EQ(test, sample.real, before.real);
    ASC_DENSE_TEST_EQ(test, sample.integer, before.integer);
    ASC_DENSE_TEST_EQ(test, sample.packed, before.packed);
  }
}

template <typename T>
void EmptyLargeStrides(TestContext& test,
                       const asc::ReferenceLapackProvider& provider) {
  const asc::extent_t stride =
      static_cast<asc::extent_t>(std::numeric_limits<std::int32_t>::max()) + 1;
  Sample<T> sample(0, 3, 15);
  const auto a = Take(asc::DenseBlasMatrixView<const T>::Create(
      nullptr, 0, 0, kRow, stride, {nullptr, 0, kHost}));
  const auto b = Take(asc::DenseBlasMatrixView<const T>::Create(
      nullptr, 0, 3, kRow, stride + 1, {nullptr, 0, kHost}));
  const auto x = Take(asc::DenseBlasMatrixView<T>::Create(
      nullptr, 0, 3, kRow, stride + 2, {nullptr, 0, kHost}));
  const auto query =
      asc::QueryGerfsWorkspace(provider, kNone, a, a, sample.Pivots(), b, x,
                               Vector(sample.ferr, 3), Vector(sample.berr, 3));
  ASC_DENSE_TEST_CHECK(test, query.ok());
  if (!query.ok()) {
    return;
  }
  const auto before = sample;
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(
      test, WithoutAllocation(test, [&] {
              return asc::Gerfs(provider, kNone, a, a, sample.Pivots(), b, x,
                                Vector(sample.ferr, 3), Vector(sample.berr, 3),
                                *query, sample.Workspace(*query), report);
            }).ok());
  ASC_DENSE_TEST_CHECK(
      test, !report.called_provider && !report.native_info.has_value());
  for (std::size_t j = 1; j < 4; ++j) {
    ASC_DENSE_TEST_EQ(test, sample.ferr[j], 0);
    ASC_DENSE_TEST_EQ(test, sample.berr[j], 0);
  }
  ASC_DENSE_TEST_EQ(test, sample.x, before.x);
  sample.Guards(test, *query);
  const auto changed = Take(asc::DenseBlasMatrixView<const T>::Create(
      nullptr, 0, 0, kRow, stride + 3, {nullptr, 0, kHost}));
  ASC_DENSE_TEST_EQ(
      test,
      WithoutAllocation(test,
                        [&] {
                          return asc::Gerfs(
                              provider, kNone, changed, a, sample.Pivots(), b,
                              x, Vector(sample.ferr, 3), Vector(sample.berr, 3),
                              *query, sample.Workspace(*query), report);
                        })
          .code(),
      asc::ErrorCode::kInvalidState);
  const auto foreign = Take(asc::DenseBlasMatrixView<const T>::Create(
      nullptr, 0, 0, kColumn, stride, {nullptr, 0, kHost}));
  const auto foreign_query = asc::QueryGerfsWorkspace(
      provider, kNone, foreign, a, sample.Pivots(), b, x,
      Vector(sample.ferr, 3), Vector(sample.berr, 3));
  ASC_DENSE_TEST_EQ(
      test, foreign_query.ok(),
      provider.identity().integer_abi != asc::LapackIntegerAbi::kLp64);
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  NumericalCases<T>(test, provider);
  TinySafeguard<T>(test, provider);
  EmptyAndSingular<T>(test, provider);
  PreflightCases<T>(test, provider);
  DescriptorCases<T>(test, provider);
  EmptyLargeStrides<T>(test, provider);
}
}  // namespace asc_refinement_test

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 2) {
    return 2;
  }
  asc_dense_test::TestContext test;
  const auto provider = asc_refinement_test::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const std::string_view scalar = argv[1];
  if (scalar == "s") {
    asc_refinement_test::Run<float>(test, provider);
  } else if (scalar == "d") {
    asc_refinement_test::Run<double>(test, provider);
  } else if (scalar == "c") {
    asc_refinement_test::Run<std::complex<float>>(test, provider);
  } else if (scalar == "z") {
    asc_refinement_test::Run<std::complex<double>>(test, provider);
  } else {
    return 2;
  }
  asc_refinement_test::ProviderDefects(test, provider);
  asc_refinement_test::EstimateDefects(test, provider);
  const int result = test.Finish();
  if (result == 0) {
    std::printf(
        "GERFS %s: residual/error/workspace/INFO/allocation checks passed\n",
        argv[1]);
  }
  return result;
}
