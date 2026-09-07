#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string_view>
#include <vector>

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
#include "asc/dense/providers/lapack_qr.h"
#include "qr_faults.h"
#include "qr_test_support.h"

namespace {
using asc_qr_test::ConstVector;
using asc_qr_test::ExplicitQ;
using asc_qr_test::Fault;
using asc_qr_test::Fill;
using asc_qr_test::ForeignCalls;
using asc_qr_test::kPacking;
using asc_qr_test::kScalar;
using asc_qr_test::Layout;
using asc_qr_test::Matrix;
using asc_qr_test::Narrow;
using asc_qr_test::Near;
using asc_qr_test::NotANumber;
using asc_qr_test::QueryCalls;
using asc_qr_test::Routine;
using asc_qr_test::Scratch;
using asc_qr_test::SetFault;
using asc_qr_test::Take;
using asc_qr_test::TestContext;
using asc_qr_test::Vector;
using asc_qr_test::Wide;
using asc_qr_test::Widen;
using asc_qr_test::WithoutAllocation;
constexpr std::array kLayouts{Layout::kColumnMajor, Layout::kRowMajor};

template <typename T>
auto FactorQuery(const asc::ReferenceLapackProvider& provider, bool unblocked,
                 asc::DenseBlasMatrixView<T> matrix,
                 asc::DenseBlasVectorView<T> tau, asc::LapackReport& report) {
  return unblocked ? asc::QueryGeqr2Workspace(provider, matrix, tau, report)
                   : asc::QueryGeqrfWorkspace(provider, matrix, tau, report);
}

template <typename T>
auto FactorExecute(const asc::ReferenceLapackProvider& provider, bool unblocked,
                   asc::DenseBlasMatrixView<T> matrix,
                   asc::DenseBlasVectorView<T> tau,
                   const asc::LapackWorkspacePlan& plan,
                   const asc::LapackWorkspace& workspace,
                   asc::LapackReport& report) {
  return unblocked ? asc::Geqr2(provider, matrix, tau, plan, workspace, report)
                   : asc::Geqrf(provider, matrix, tau, plan, workspace, report);
}

template <typename T>
auto GenerateQuery(const asc::ReferenceLapackProvider& provider,
                   asc::DenseBlasMatrixView<T> matrix,
                   asc::DenseBlasVectorView<const T> tau,
                   asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    return asc::QueryUngqrWorkspace(provider, matrix, tau, report);
  } else {
    return asc::QueryOrgqrWorkspace(provider, matrix, tau, report);
  }
}

template <typename T>
auto Generate(const asc::ReferenceLapackProvider& provider,
              asc::DenseBlasMatrixView<T> matrix,
              asc::DenseBlasVectorView<const T> tau,
              const asc::LapackWorkspacePlan& plan,
              const asc::LapackWorkspace& workspace,
              asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    return asc::Ungqr(provider, matrix, tau, plan, workspace, report);
  } else {
    return asc::Orgqr(provider, matrix, tau, plan, workspace, report);
  }
}

template <typename T>
auto ApplyQuery(const asc::ReferenceLapackProvider& provider,
                asc::DenseBlasSide side, asc::DenseBlasTranspose transpose,
                asc::DenseBlasMatrixView<const T> reflectors,
                asc::DenseBlasVectorView<const T> tau,
                asc::DenseBlasMatrixView<T> matrix, asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    return asc::QueryUnmqrWorkspace(provider, side, transpose, reflectors, tau,
                                    matrix, report);
  } else {
    return asc::QueryOrmqrWorkspace(provider, side, transpose, reflectors, tau,
                                    matrix, report);
  }
}

template <typename T>
auto Apply(const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasSide side, asc::DenseBlasTranspose transpose,
           asc::DenseBlasMatrixView<const T> reflectors,
           asc::DenseBlasVectorView<const T> tau,
           asc::DenseBlasMatrixView<T> matrix,
           const asc::LapackWorkspacePlan& plan,
           const asc::LapackWorkspace& workspace, asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    return asc::Unmqr(provider, side, transpose, reflectors, tau, matrix, plan,
                      workspace, report);
  } else {
    return asc::Ormqr(provider, side, transpose, reflectors, tau, matrix, plan,
                      workspace, report);
  }
}

void CheckReport(TestContext& test, const asc::LapackReport& report,
                 bool called, bool query, bool factor) {
  ASC_DENSE_TEST_EQ(test, report.called_provider, called);
  ASC_DENSE_TEST_EQ(test, report.native_info.has_value(), called);
  if (report.native_info.has_value()) {
    ASC_DENSE_TEST_EQ(test, *report.native_info, 0);
  }
  ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSuccess);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    query ? asc::LapackOutputValidity::kUnchanged
                          : asc::LapackOutputValidity::kComplete);
  ASC_DENSE_TEST_EQ(test, report.factor_family.has_value(), factor);
  if (report.factor_family.has_value()) {
    ASC_DENSE_TEST_EQ(test, *report.factor_family,
                      asc::LapackFactorFamily::kHouseholderQr);
  }
}

template <typename T>
void CheckOrthogonal(TestContext& test, const std::vector<Wide>& q,
                     asc::extent_t m) {
  for (asc::extent_t row = 0; row < m; ++row) {
    for (asc::extent_t col = 0; col < m; ++col) {
      Wide sum{};
      for (asc::extent_t i = 0; i < m; ++i) {
        sum += std::conj(q[static_cast<std::size_t>(i * m + row)]) *
               q[static_cast<std::size_t>(i * m + col)];
      }
      Near<T>(test, sum, row == col ? Wide{1} : Wide{}, m);
    }
  }
}

template <typename T>
void CheckFactor(TestContext& test, const Matrix<T>& factor,
                 const Matrix<T>& original, const std::vector<T>& tau) {
  const auto m = factor.const_view().rows();
  const auto n = factor.const_view().columns();
  const auto k = std::min(m, n);
  const auto q = ExplicitQ(factor, tau, k);
  CheckOrthogonal<T>(test, q, m);
  for (asc::extent_t row = 0; row < m; ++row) {
    for (asc::extent_t col = 0; col < n; ++col) {
      Wide sum{};
      for (asc::extent_t i = 0; i < k && i <= col; ++i) {
        sum += q[static_cast<std::size_t>(row * m + i)] * Widen(factor(i, col));
      }
      Near<T>(test, sum, Widen(original(row, col)), m);
    }
  }
  if constexpr (asc::DenseBlasComplex<T>) {
    for (asc::extent_t i = 0; i < k; ++i) {
      ASC_DENSE_TEST_EQ(test, factor(i, i).imag(), 0);
    }
  }
}

template <typename T>
void FactorCase(TestContext& test, const asc::ReferenceLapackProvider& provider,
                asc::extent_t m, asc::extent_t n, Layout layout, bool unblocked,
                bool preferred, int fixture, std::string_view scalar) {
  Matrix<T> a(m, n, layout);
  Fill(a, fixture);
  const Matrix<T> original = a;
  std::vector<T> tau(static_cast<std::size_t>(std::min(m, n)), T{-91});
  const auto old_tau = tau;
  asc::LapackReport report;
  auto plan = WithoutAllocation(test, [&] {
    return FactorQuery(provider, unblocked, a.view(),
                       Vector(tau, std::min(m, n)), report);
  });
  ASC_DENSE_TEST_CHECK(test, plan.ok());
  CheckReport(test, report, !unblocked, true, false);
  a.CheckSame(test, original.bytes());
  ASC_DENSE_TEST_EQ(test, tau, old_tau);
  if (!plan.ok()) {
    return;
  }
  Scratch<T> scratch(*plan, preferred);
  const auto workspace = scratch.view();
  const auto status = WithoutAllocation(test, [&] {
    return FactorExecute(provider, unblocked, a.view(),
                         Vector(tau, std::min(m, n)), *plan, workspace, report);
  });
  ASC_DENSE_TEST_CHECK(test, status.ok());
  CheckReport(test, report, m != 0 && n != 0, false, true);
  a.CheckPadding(test, original.bytes());
  CheckFactor(test, a, original, tau);
  std::cout << R"({"profile":"reference-qr-factor","scalar":")" << scalar
            << R"(","unblocked":)" << unblocked << ",\"m\":" << m
            << ",\"n\":" << n << ",\"layout\":" << static_cast<int>(layout)
            << ",\"preferred\":" << preferred << ",\"fixture\":" << fixture
            << "}\n";
}

template <typename T>
void GenerateCase(TestContext& test,
                  const asc::ReferenceLapackProvider& provider,
                  const Matrix<T>& factor, const std::vector<T>& tau,
                  asc::extent_t n, asc::extent_t k, Layout layout,
                  bool preferred) {
  const auto m = factor.const_view().rows();
  const auto expected = ExplicitQ(factor, tau, k);
  Matrix<T> q(m, n, layout);
  for (asc::extent_t row = 0; row < m; ++row) {
    for (asc::extent_t col = 0; col < n; ++col) {
      q(row, col) = col < k && row > col ? factor(row, col) : NotANumber<T>();
    }
  }
  const auto before = q.bytes();
  const auto old_tau = tau;
  asc::LapackReport report;
  const auto plan = WithoutAllocation(test, [&] {
    return GenerateQuery(provider, q.view(), ConstVector(tau, k), report);
  });
  ASC_DENSE_TEST_CHECK(test, plan.ok());
  CheckReport(test, report, true, true, false);
  q.CheckSame(test, before);
  if (!plan.ok()) {
    return;
  }
  Scratch<T> scratch(*plan, preferred);
  const auto workspace = scratch.view();
  const auto status = WithoutAllocation(test, [&] {
    return Generate(provider, q.view(), ConstVector(tau, k), *plan, workspace,
                    report);
  });
  ASC_DENSE_TEST_CHECK(test, status.ok());
  CheckReport(test, report, n != 0, false, false);
  ASC_DENSE_TEST_EQ(test, tau, old_tau);
  q.CheckPadding(test, before);
  for (asc::extent_t row = 0; row < m; ++row) {
    for (asc::extent_t col = 0; col < n; ++col) {
      Near<T>(test, Widen(q(row, col)),
              expected[static_cast<std::size_t>(row * m + col)], m);
    }
  }
}

template <typename T>
void CheckApplied(TestContext& test, const Matrix<T>& output,
                  const Matrix<T>& input, const std::vector<Wide>& q,
                  asc::extent_t order, bool left, bool adjoint) {
  const auto m = output.const_view().rows();
  const auto n = output.const_view().columns();
  for (asc::extent_t row = 0; row < m; ++row) {
    for (asc::extent_t col = 0; col < n; ++col) {
      Wide expected{};
      for (asc::extent_t inner = 0; inner < order; ++inner) {
        const auto qr = left ? row : inner;
        const auto qc = left ? inner : col;
        const auto value =
            adjoint ? std::conj(q[static_cast<std::size_t>(qc * order + qr)])
                    : q[static_cast<std::size_t>(qr * order + qc)];
        expected += value * Widen(left ? input(inner, col) : input(row, inner));
      }
      Near<T>(test, Widen(output(row, col)), expected, order);
    }
  }
}

template <typename T>
void ApplyCase(TestContext& test, const asc::ReferenceLapackProvider& provider,
               const Matrix<T>& factor, const std::vector<T>& tau,
               asc::extent_t k, Layout reflector_layout, Layout output_layout,
               bool left, bool adjoint, bool preferred) {
  const auto order = factor.const_view().rows();
  Matrix<T> reflectors(order, k, reflector_layout);
  for (asc::extent_t row = 0; row < order; ++row) {
    for (asc::extent_t col = 0; col < k; ++col) {
      reflectors(row, col) = row > col ? factor(row, col) : NotANumber<T>();
    }
  }
  const auto expected = ExplicitQ(factor, tau, k);
  const auto old_reflectors = reflectors.bytes();
  const auto old_tau = tau;
  Matrix<T> output(left ? order : 3, left ? 3 : order, output_layout);
  Fill(output, 0);
  const Matrix<T> original = output;
  const auto side =
      left ? asc::DenseBlasSide::kLeft : asc::DenseBlasSide::kRight;
  const auto adjoint_mode = asc::DenseBlasComplex<T>
                                ? asc::DenseBlasTranspose::kConjugateTranspose
                                : asc::DenseBlasTranspose::kTranspose;
  const auto transpose =
      adjoint ? adjoint_mode : asc::DenseBlasTranspose::kNone;
  asc::LapackReport report;
  const auto plan = WithoutAllocation(test, [&] {
    return ApplyQuery(provider, side, transpose, reflectors.const_view(),
                      ConstVector(tau, k), output.view(), report);
  });
  ASC_DENSE_TEST_CHECK(test, plan.ok());
  CheckReport(test, report, true, true, false);
  output.CheckSame(test, original.bytes());
  reflectors.CheckSame(test, old_reflectors);
  if (!plan.ok()) {
    return;
  }
  Scratch<T> scratch(*plan, preferred);
  const auto workspace = scratch.view();
  const auto status = WithoutAllocation(test, [&] {
    return Apply(provider, side, transpose, reflectors.const_view(),
                 ConstVector(tau, k), output.view(), *plan, workspace, report);
  });
  ASC_DENSE_TEST_CHECK(test, status.ok());
  CheckReport(test, report, order != 0 && k != 0, false, false);
  reflectors.CheckSame(test, old_reflectors);
  ASC_DENSE_TEST_EQ(test, tau, old_tau);
  output.CheckPadding(test, original.bytes());
  CheckApplied(test, output, original, expected, order, left, adjoint);
}

template <typename T>
void QCases(TestContext& test, const asc::ReferenceLapackProvider& provider,
            asc::extent_t m, asc::extent_t original_n,
            std::string_view scalar) {
  Matrix<T> factor(m, original_n, Layout::kColumnMajor);
  Fill(factor, 0);
  const auto original_k = std::min(m, original_n);
  std::vector<T> tau(static_cast<std::size_t>(original_k));
  asc::LapackReport report;
  const auto plan = Take(asc::QueryGeqrfWorkspace(
      provider, factor.view(), Vector(tau, original_k), report));
  Scratch<T> scratch(plan, true);
  const auto workspace = scratch.view();
  ASC_DENSE_TEST_CHECK(
      test, asc::Geqrf(provider, factor.view(), Vector(tau, original_k), plan,
                       workspace, report)
                .ok());
  for (const auto k : {asc::extent_t{0}, original_k / 2, original_k}) {
    for (const auto layout : kLayouts) {
      for (const bool preferred : {false, true}) {
        for (const auto n : {k, std::min(m, k + 1), m}) {
          GenerateCase(test, provider, factor, tau, n, k, layout, preferred);
        }
        for (const auto output_layout : kLayouts) {
          for (const bool left : {false, true}) {
            for (const bool adjoint : {false, true}) {
              ApplyCase(test, provider, factor, tau, k, layout, output_layout,
                        left, adjoint, preferred);
            }
          }
        }
        std::cout << R"({"profile":"reference-qr-q-modes","scalar":")" << scalar
                  << R"(","m":)" << m << ",\"k\":" << k
                  << ",\"layout\":" << static_cast<int>(layout)
                  << ",\"preferred\":" << preferred << "}\n";
      }
    }
  }
}

template <typename T>
auto RouteQuery(const asc::ReferenceLapackProvider& provider, Routine routine,
                Matrix<T>& matrix, const Matrix<T>& reflectors,
                std::vector<T>& tau, asc::LapackReport& report) {
  const auto k = static_cast<asc::extent_t>(tau.size());
  if (routine == Routine::kGeqrf || routine == Routine::kGeqr2) {
    return FactorQuery(provider, routine == Routine::kGeqr2, matrix.view(),
                       Vector(tau, k), report);
  }
  if (routine == Routine::kGenerate) {
    return GenerateQuery(provider, matrix.view(), ConstVector(tau, k), report);
  }
  return ApplyQuery(provider, asc::DenseBlasSide::kLeft,
                    asc::DenseBlasTranspose::kNone, reflectors.const_view(),
                    ConstVector(tau, k), matrix.view(), report);
}

template <typename T>
auto RouteExecute(const asc::ReferenceLapackProvider& provider, Routine routine,
                  Matrix<T>& matrix, const Matrix<T>& reflectors,
                  std::vector<T>& tau, const asc::LapackWorkspacePlan& plan,
                  const asc::LapackWorkspace& workspace,
                  asc::LapackReport& report) {
  const auto k = static_cast<asc::extent_t>(tau.size());
  if (routine == Routine::kGeqrf || routine == Routine::kGeqr2) {
    return FactorExecute(provider, routine == Routine::kGeqr2, matrix.view(),
                         Vector(tau, k), plan, workspace, report);
  }
  if (routine == Routine::kGenerate) {
    return Generate(provider, matrix.view(), ConstVector(tau, k), plan,
                    workspace, report);
  }
  return Apply(provider, asc::DenseBlasSide::kLeft,
               asc::DenseBlasTranspose::kNone, reflectors.const_view(),
               ConstVector(tau, k), matrix.view(), plan, workspace, report);
}

template <typename T>
void QueryFailures(TestContext& test,
                   const asc::ReferenceLapackProvider& provider) {
  Matrix<T> matrix(3, 3, Layout::kRowMajor);
  Matrix<T> reflectors(3, 3, Layout::kColumnMajor);
  std::vector<T> tau(3);
  const auto before = matrix.bytes();
  asc::LapackReport report;
  for (const auto routine :
       {Routine::kGeqrf, Routine::kGenerate, Routine::kApply}) {
    for (const auto fault :
         {Fault::kNegative, Fault::kMinimumInteger, Fault::kPositive,
          Fault::kQueryZero, Fault::kQueryNan}) {
      SetFault(routine, fault);
      const auto calls = QueryCalls();
      const auto result = WithoutAllocation(test, [&] {
        return RouteQuery(provider, routine, matrix, reflectors, tau, report);
      });
      ASC_DENSE_TEST_CHECK(test, !result.ok());
      ASC_DENSE_TEST_EQ(test, QueryCalls(), calls + 1);
      ASC_DENSE_TEST_CHECK(test, report.called_provider);
      ASC_DENSE_TEST_CHECK(test, report.native_info.has_value());
      ASC_DENSE_TEST_EQ(test, report.output_validity,
                        asc::LapackOutputValidity::kUnchanged);
      if (fault == Fault::kNegative) {
        ASC_DENSE_TEST_EQ(test, report.native_info.value_or(99), -4);
        ASC_DENSE_TEST_EQ(test, report.native_argument.value_or(99), 4);
      } else if (fault == Fault::kMinimumInteger) {
        const auto minimum =
            provider.identity().integer_abi == asc::LapackIntegerAbi::kLp64
                ? std::numeric_limits<std::int32_t>::min()
                : std::numeric_limits<std::int64_t>::min();
        ASC_DENSE_TEST_EQ(test, report.native_info.value_or(99), minimum);
        ASC_DENSE_TEST_CHECK(test, !report.native_argument.has_value());
      }
      matrix.CheckSame(test, before);
    }
  }
  if constexpr (asc::DenseBlasComplex<T>) {
    SetFault(Routine::kGenerate, Fault::kQueryImaginary);
    const auto result = WithoutAllocation(test, [&] {
      return GenerateQuery(provider, matrix.view(), ConstVector(tau, 3),
                           report);
    });
    ASC_DENSE_TEST_CHECK(test, !result.ok());
    matrix.CheckSame(test, before);
  }
  SetFault(Routine::kGeqr2, Fault::kNegative);
  const auto calls = ForeignCalls();
  ASC_DENSE_TEST_CHECK(test, RouteQuery(provider, Routine::kGeqr2, matrix,
                                        reflectors, tau, report)
                                 .ok());
  ASC_DENSE_TEST_EQ(test, ForeignCalls(), calls);
  CheckReport(test, report, false, true, false);
  SetFault(Routine::kGeqrf, Fault::kNone);
}

template <typename T>
void ExecutionFailures(TestContext& test,
                       const asc::ReferenceLapackProvider& provider) {
  for (const auto routine : {Routine::kGeqrf, Routine::kGeqr2,
                             Routine::kGenerate, Routine::kApply}) {
    for (const auto layout : kLayouts) {
      Matrix<T> matrix(3, 3, layout);
      Matrix<T> reflectors(3, 3, Layout::kColumnMajor);
      std::vector<T> tau(3);
      const auto before = matrix.bytes();
      asc::LapackReport report;
      const auto plan =
          Take(RouteQuery(provider, routine, matrix, reflectors, tau, report));
      Scratch<T> scratch(plan, true);
      const auto workspace = scratch.view();
      for (const auto fault :
           {Fault::kNegative, Fault::kMinimumInteger, Fault::kPositive}) {
        SetFault(routine, fault);
        const auto queries = QueryCalls();
        const auto status = WithoutAllocation(test, [&] {
          return RouteExecute(provider, routine, matrix, reflectors, tau, plan,
                              workspace, report);
        });
        ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
        ASC_DENSE_TEST_EQ(test, QueryCalls(), queries);
        ASC_DENSE_TEST_CHECK(test, report.called_provider);
        ASC_DENSE_TEST_EQ(test, report.output_validity,
                          asc::LapackOutputValidity::kUnusable);
        ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
        if (layout == Layout::kRowMajor) {
          matrix.CheckSame(test, before);
        } else {
          ASC_DENSE_TEST_EQ(test, matrix(0, 0), T{-881});
          matrix.CheckPadding(test, before);
        }
      }
      SetFault(routine, Fault::kNone);
    }
  }
}

template <typename T>
void WorkspaceFailures(TestContext& test,
                       const asc::ReferenceLapackProvider& provider) {
  Matrix<T> matrix(3, 3, Layout::kRowMajor);
  Matrix<T> reflectors(3, 3, Layout::kColumnMajor);
  std::vector<T> tau(3);
  const auto before = matrix.bytes();
  asc::LapackReport report;
  for (const auto routine : {Routine::kGeqrf, Routine::kGeqr2,
                             Routine::kGenerate, Routine::kApply}) {
    const auto plan =
        Take(RouteQuery(provider, routine, matrix, reflectors, tau, report));
    Scratch<T> scratch(plan, false);
    const auto original = scratch.view();
    for (const auto role : {kScalar, kPacking}) {
      auto workspace = original;
      const auto region = workspace.regions[role];
      workspace.regions[role] = {region.data(), region.size() - 1,
                                 region.space()};
      const auto calls = ForeignCalls();
      const auto status = WithoutAllocation(test, [&] {
        return RouteExecute(provider, routine, matrix, reflectors, tau, plan,
                            workspace, report);
      });
      ASC_DENSE_TEST_CHECK(test, !status.ok());
      ASC_DENSE_TEST_EQ(test, ForeignCalls(), calls);
      ASC_DENSE_TEST_CHECK(test, !report.called_provider);
      ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
      matrix.CheckSame(test, before);
    }
    auto stale = plan;
    ++stale.regions[kScalar].preferred_entries;
    const auto queries = QueryCalls();
    ASC_DENSE_TEST_CHECK(test, !WithoutAllocation(test, [&] {
                                  return RouteExecute(provider, routine, matrix,
                                                      reflectors, tau, stale,
                                                      original, report);
                                }).ok());
    ASC_DENSE_TEST_EQ(test, QueryCalls(), queries);
    auto aliased = original;
    aliased.regions[kScalar] = {matrix.view().data(), sizeof(T) * 3,
                                asc::MemorySpace::kHost};
    ASC_DENSE_TEST_CHECK(test, !WithoutAllocation(test, [&] {
                                  return RouteExecute(provider, routine, matrix,
                                                      reflectors, tau, plan,
                                                      aliased, report);
                                }).ok());
    aliased = original;
    aliased.regions[kScalar] = {&report, sizeof(report),
                                asc::MemorySpace::kHost};
    report.native_info = 991;
    ASC_DENSE_TEST_CHECK(test, !WithoutAllocation(test, [&] {
                                  return RouteExecute(provider, routine, matrix,
                                                      reflectors, tau, plan,
                                                      aliased, report);
                                }).ok());
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(99), 991);
    matrix.CheckSame(test, before);
  }
}

template <typename T>
void DescriptorFailures(TestContext& test,
                        const asc::ReferenceLapackProvider& provider) {
  Matrix<T> matrix(3, 3, Layout::kRowMajor);
  Matrix<T> reflectors(3, 3, Layout::kColumnMajor);
  std::vector<T> tau(6);
  asc::LapackReport report;
  const auto calls = ForeignCalls();
  for (const auto placement :
       {asc::MemorySpace::kPinnedHost, asc::MemorySpace::kDevice,
        asc::MemorySpace::kManaged}) {
    const auto result = WithoutAllocation(test, [&] {
      return asc::QueryGeqrfWorkspace(provider, matrix.view(placement),
                                      Vector(tau, 3), report);
    });
    ASC_DENSE_TEST_EQ(test, result.status().code(),
                      asc::ErrorCode::kMemoryAccess);
  }
  const auto strided = Take(asc::DenseBlasVectorView<T>::Create(
      tau.data(), 3, 2,
      {tau.data(), tau.size() * sizeof(T), asc::MemorySpace::kHost}));
  ASC_DENSE_TEST_CHECK(test, !WithoutAllocation(test, [&] {
                                return asc::QueryGeqrfWorkspace(
                                    provider, matrix.view(), strided, report);
                              }).ok());
  ASC_DENSE_TEST_CHECK(test, !WithoutAllocation(test, [&] {
                                return asc::QueryGeqrfWorkspace(
                                    provider, matrix.view(), Vector(tau, 2),
                                    report);
                              }).ok());
  ASC_DENSE_TEST_CHECK(test, !WithoutAllocation(test, [&] {
                                return GenerateQuery(provider, matrix.view(),
                                                     ConstVector(tau, 4),
                                                     report);
                              }).ok());
  const auto invalid = asc::DenseBlasComplex<T>
                           ? asc::DenseBlasTranspose::kTranspose
                           : asc::DenseBlasTranspose::kConjugateTranspose;
  ASC_DENSE_TEST_CHECK(test, !WithoutAllocation(test, [&] {
                                return ApplyQuery(
                                    provider, asc::DenseBlasSide::kLeft,
                                    invalid, reflectors.const_view(),
                                    ConstVector(tau, 3), matrix.view(), report);
                              }).ok());
  const auto alias_tau = Take(asc::DenseBlasVectorView<T>::Create(
      matrix.view().data(), 3, 1, matrix.view().reachable_storage()));
  ASC_DENSE_TEST_CHECK(test, !WithoutAllocation(test, [&] {
                                return asc::QueryGeqrfWorkspace(
                                    provider, matrix.view(), alias_tau, report);
                              }).ok());
  ASC_DENSE_TEST_EQ(test, ForeignCalls(), calls);
}

template <typename T>
void HugeStride(TestContext& test,
                const asc::ReferenceLapackProvider& provider) {
  T value = Narrow<T>({3, 4});
  const T input = value;
  T reflector = NotANumber<T>();
  std::vector<T> tau(1);
  const auto huge = std::numeric_limits<asc::stride_t>::max();
  const auto matrix = Take(asc::DenseBlasMatrixView<T>::Create(
      &value, 1, 1, Layout::kRowMajor, huge,
      {&value, sizeof(value), asc::MemorySpace::kHost}));
  const auto raw = Take(asc::DenseBlasMatrixView<const T>::Create(
      &reflector, 1, 1, Layout::kRowMajor, huge,
      {&reflector, sizeof(reflector), asc::MemorySpace::kHost}));
  asc::LapackReport report;
  for (const bool unblocked : {false, true}) {
    value = input;
    const auto plan = WithoutAllocation(test, [&] {
      return FactorQuery(provider, unblocked, matrix, Vector(tau, 1), report);
    });
    ASC_DENSE_TEST_CHECK(test, plan.ok());
    if (!plan.ok()) {
      continue;
    }
    Scratch<T> scratch(*plan, true);
    const auto workspace = scratch.view();
    ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                                 return FactorExecute(provider, unblocked,
                                                      matrix, Vector(tau, 1),
                                                      *plan, workspace, report);
                               }).ok());
    const auto expected_q = Widen(input) / Widen(value);
    value = NotANumber<T>();
    const auto q_plan = Take(WithoutAllocation(test, [&] {
      return GenerateQuery(provider, matrix, ConstVector(tau, 1), report);
    }));
    Scratch<T> q_scratch(q_plan, true);
    const auto q_workspace = q_scratch.view();
    ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                                 return Generate(provider, matrix,
                                                 ConstVector(tau, 1), q_plan,
                                                 q_workspace, report);
                               }).ok());
    Near<T>(test, Widen(value), expected_q, 1);
    value = T{2};
    const auto apply_plan = Take(WithoutAllocation(test, [&] {
      return ApplyQuery(provider, asc::DenseBlasSide::kLeft,
                        asc::DenseBlasTranspose::kNone, raw,
                        ConstVector(tau, 1), matrix, report);
    }));
    Scratch<T> apply_scratch(apply_plan, true);
    const auto apply_workspace = apply_scratch.view();
    ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                                 return Apply(
                                     provider, asc::DenseBlasSide::kLeft,
                                     asc::DenseBlasTranspose::kNone, raw,
                                     ConstVector(tau, 1), matrix, apply_plan,
                                     apply_workspace, report);
                               }).ok());
    Near<T>(test, Widen(value), 2.0L * expected_q, 1);
  }
}

template <typename T>
void EmptyBoundaries(TestContext& test,
                     const asc::ReferenceLapackProvider& provider) {
  const asc::extent_t limit =
      provider.identity().integer_abi == asc::LapackIntegerAbi::kLp64
          ? std::numeric_limits<std::int32_t>::max()
          : std::numeric_limits<std::int64_t>::max();
  const auto empty = asc::ConstMemoryView(nullptr, 0, asc::MemorySpace::kHost);
  const auto a = Take(asc::DenseBlasMatrixView<T>::Create(
      nullptr, 0, limit, Layout::kColumnMajor, 1, empty));
  const auto q = Take(asc::DenseBlasMatrixView<T>::Create(
      nullptr, limit, 0, Layout::kRowMajor, 1, empty));
  std::vector<T> tau;
  asc::LapackReport report;
  const auto plan = WithoutAllocation(test, [&] {
    return asc::QueryGeqrfWorkspace(provider, a, Vector(tau, 0), report);
  });
  ASC_DENSE_TEST_CHECK(test, plan.ok());
  if (plan.ok()) {
    Scratch<T> scratch(*plan, false);
    const auto workspace = scratch.view();
    ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                                 return asc::Geqrf(provider, a, Vector(tau, 0),
                                                   *plan, workspace, report);
                               }).ok());
    CheckReport(test, report, false, false, true);
  }
  const auto q_plan = WithoutAllocation(test, [&] {
    return GenerateQuery(provider, q, ConstVector(tau, 0), report);
  });
  ASC_DENSE_TEST_CHECK(test, q_plan.ok());
  if (q_plan.ok()) {
    Scratch<T> scratch(*q_plan, false);
    const auto workspace = scratch.view();
    ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                                 return Generate(provider, q,
                                                 ConstVector(tau, 0), *q_plan,
                                                 workspace, report);
                               }).ok());
    CheckReport(test, report, false, false, false);
  }
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider,
         std::string_view scalar) {
  EmptyBoundaries<T>(test, provider);
  HugeStride<T>(test, provider);
  QueryFailures<T>(test, provider);
  ExecutionFailures<T>(test, provider);
  WorkspaceFailures<T>(test, provider);
  DescriptorFailures<T>(test, provider);
  for (const auto shape : {std::array<asc::extent_t, 2>{0, 0},
                           {0, 3},
                           {3, 0},
                           {1, 1},
                           {5, 3},
                           {3, 3},
                           {3, 5},
                           {131, 129}}) {
    for (const auto layout : kLayouts) {
      for (const bool unblocked : {false, true}) {
        for (const bool preferred : {false, true}) {
          FactorCase<T>(test, provider, shape[0], shape[1], layout, unblocked,
                        preferred, 0, scalar);
        }
      }
    }
  }
  for (const int fixture : {1, 2}) {
    for (const auto layout : kLayouts) {
      for (const bool unblocked : {false, true}) {
        FactorCase<T>(test, provider, 5, 3, layout, unblocked, true, fixture,
                      scalar);
      }
    }
  }
  for (const auto shape : {std::array<asc::extent_t, 2>{0, 0},
                           {1, 1},
                           {5, 3},
                           {3, 5},
                           {131, 129}}) {
    QCases<T>(test, provider, shape[0], shape[1], scalar);
  }
}
}  // namespace

int main(int argc, char** argv) {
  TestContext test;
  if (argc != 2) {
    return 2;
  }
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    Run<float>(test, provider, scalar);
  } else if (scalar == "d") {
    Run<double>(test, provider, scalar);
  } else if (scalar == "c") {
    Run<std::complex<float>>(test, provider, scalar);
  } else if (scalar == "z") {
    Run<std::complex<double>>(test, provider, scalar);
  } else {
    return 2;
  }
  return test.Finish();
}
