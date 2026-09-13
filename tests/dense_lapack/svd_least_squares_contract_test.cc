#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <iostream>
#include <limits>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "installed_lu/normal_return_guard.h"
#include "least_squares_test_support.h"
#include "svd_least_squares_faults.h"
#include "svd_least_squares_test_support.h"

namespace {
using asc_svd_least_squares_test::Execute;
using asc_svd_least_squares_test::ForeignCalls;
using asc_svd_least_squares_test::Guarded;
using asc_svd_least_squares_test::Index;
using asc_svd_least_squares_test::Integer;
using asc_svd_least_squares_test::Layout;
using asc_svd_least_squares_test::Matrix;
using asc_svd_least_squares_test::Narrow;
using asc_svd_least_squares_test::NotANumber;
using asc_svd_least_squares_test::Query;
using asc_svd_least_squares_test::Routine;
using asc_svd_least_squares_test::Scratch;
using asc_svd_least_squares_test::Take;
using asc_svd_least_squares_test::TestContext;
using asc_svd_least_squares_test::Wide;
using asc_svd_least_squares_test::Widen;
using asc_svd_least_squares_test::WithoutAllocation;

template <typename T>
void Cutoff(TestContext& test, const asc::ReferenceLapackProvider& provider,
            Routine routine, asc::extent_t size,
            asc::DenseBlasRealType<T> cutoff, Layout layout) {
  using Real = asc::DenseBlasRealType<T>;
  Matrix<T> a(size, size, layout);
  Matrix<T> b(size, 2, layout);
  Guarded<Real> s(static_cast<std::size_t>(size));
  const Wide phase = asc::DenseBlasComplex<T> ? Wide{0, 1} : Wide{-1, 0};
  for (asc::extent_t i = 0; i < size; ++i) {
    for (asc::extent_t j = 0; j < size; ++j) {
      a(i, j) = Narrow<T>(i == j ? phase * (i == 0 ? 4.L : 1.L) : Wide{});
    }
    for (asc::extent_t j = 0; j < 2; ++j) {
      b(i, j) = Narrow<T>(phase * (i == 0 ? 8.L : 3.L) * (j + 1.L));
    }
  }
  asc::LapackReport report;
  const auto plan = Take(WithoutAllocation(test, [&] {
    return Query(provider, routine, a.view(), b.view(), s.vector(), cutoff,
                 report);
  }));
  Scratch<T> scratch(plan, false);
  const auto workspace = scratch.view();
  asc::index_t rank = -91;
  const auto status = WithoutAllocation(test, [&] {
    return Execute(provider, routine, a.view(), b.view(), s.vector(), cutoff,
                   rank, plan, workspace, report);
  });
  ASC_DENSE_TEST_CHECK(test, status.ok());
  // These source-specific expected cutoffs are determined from a diagonal
  // matrix with exact singular values, not from another provider call.
  Real effective = cutoff;
  if (effective < Real{0} || (routine == Routine::kGelsd &&
                              (effective <= Real{0} || effective >= Real{1}))) {
    effective = std::numeric_limits<Real>::epsilon();
  }
  asc::index_t expected = effective < 1 ? 1 : 0;
  if (size == 2 && effective < Real{0.25}) {
    ++expected;
  }
  if (routine == Routine::kGelsd && size == 1) {
    expected = 1;
  }
  ASC_DENSE_TEST_EQ(test, rank, expected);
  const long double tolerance = 128.L * std::numeric_limits<Real>::epsilon();
  for (asc::extent_t i = 0; i < size; ++i) {
    ASC_DENSE_TEST_NEAR(test, static_cast<long double>(s.data()[i]),
                        i == 0 ? 4.L : 1.L, tolerance, tolerance);
    for (asc::extent_t j = 0; j < 2; ++j) {
      const long double wanted =
          i < expected ? (i == 0 ? 2.L : 3.L) * (j + 1.L) : 0;
      ASC_DENSE_TEST_CHECK(test,
                           std::abs(Widen(b(i, j)) - wanted) <= tolerance * 8);
    }
  }
  scratch.CheckGuards(test);
  s.CheckGuards(test);
}

template <typename T>
void Empty(TestContext& test, const asc::ReferenceLapackProvider& provider,
           Routine routine, asc::extent_t m, asc::extent_t n,
           asc::extent_t nrhs, bool zero, Layout layout) {
  using Real = asc::DenseBlasRealType<T>;
  Matrix<T> a(m, n, layout);
  Matrix<T> b(std::max(m, n), nrhs, layout);
  const auto k = std::min(m, n);
  Guarded<Real> s(static_cast<std::size_t>(k));
  for (asc::extent_t i = 0; i < m; ++i) {
    for (asc::extent_t j = 0; j < n; ++j) {
      a(i, j) = zero || i != j ? T{0} : T{1};
    }
  }
  const auto a_before = a.bytes();
  const auto b_before = b.bytes();
  const auto s_before = s.bytes();
  asc::LapackReport report;
  const auto plan = Take(WithoutAllocation(test, [&] {
    return Query(provider, routine, a.view(), b.view(), s.vector(), Real{-1},
                 report);
  }));
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  a.CheckSame(test, a_before);
  b.CheckSame(test, b_before);
  s.CheckSame(test, s_before);
  Scratch<T> scratch(plan, false);
  auto workspace = scratch.view();
  asc::index_t rank = -71;
  const auto status = WithoutAllocation(test, [&] {
    return Execute(provider, routine, a.view(), b.view(), s.vector(), Real{-1},
                   rank, plan, workspace, report);
  });
  const bool unsupported =
      k > 0 && nrhs == 0 && !zero && routine == Routine::kGelsd;
  if (unsupported) {
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kUnsupported);
    ASC_DENSE_TEST_CHECK(test, !report.called_provider);
    ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
    ASC_DENSE_TEST_EQ(test, rank, -71);
    a.CheckSame(test, a_before);
    b.CheckSame(test, b_before);
    s.CheckSame(test, s_before);
  } else {
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_EQ(test, rank, zero ? 0 : k);
    ASC_DENSE_TEST_EQ(test, report.called_provider, k > 0);
    if (k == 0) {
      a.CheckSame(test, a_before);
      b.CheckSame(test, b_before);
      s.CheckSame(test, s_before);
      ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
    } else {
      for (asc::extent_t i = 0; i < k; ++i) {
        ASC_DENSE_TEST_EQ(test, s.data()[i], zero ? Real{0} : Real{1});
      }
    }
  }
  a.CheckPadding(test, a_before);
  b.CheckPadding(test, b_before);
  s.CheckGuards(test);
  scratch.CheckGuards(test);
}

template <typename T, typename CheckFailure, typename CheckAlias>
void WorkspaceRejections(const asc::LapackWorkspacePlan& plan,
                         Scratch<T>& scratch, Matrix<T>& a,
                         CheckFailure check_failure, CheckAlias check_alias) {
  using Real = asc::DenseBlasRealType<T>;
  for (const auto kind :
       {asc::LapackWorkspaceKind::kScalar, asc::LapackWorkspaceKind::kReal,
        asc::LapackWorkspaceKind::kInteger, asc::LapackWorkspaceKind::kScratch,
        asc::LapackWorkspaceKind::kLayoutConversion}) {
    const auto i = Index(kind);
    auto workspace = scratch.view();
    const auto region = workspace.regions[i];
    if (region.size() == 0) {
      continue;
    }
    workspace.regions[i] = {region.data(), region.size() - 1,
                            asc::MemorySpace::kHost};
    check_failure(plan, workspace, Real{-1}, asc::ErrorCode::kInvalidArgument);
    for (const auto space :
         {asc::MemorySpace::kPinnedHost, asc::MemorySpace::kDevice,
          asc::MemorySpace::kManaged}) {
      workspace.regions[i] = {region.data(), region.size(), space};
      check_failure(plan, workspace, Real{-1}, asc::ErrorCode::kMemoryAccess);
    }
    // A genuinely live operand span is used, never a fabricated backing.
    workspace.regions[i] = {a.view().data(),
                            a.view().reachable_storage().size(),
                            asc::MemorySpace::kHost};
    check_alias(workspace);
  }
}

template <typename T>
void Structural(TestContext& test, const asc::ReferenceLapackProvider& provider,
                Routine routine) {
  using Real = asc::DenseBlasRealType<T>;
  Matrix<T> a(3, 2, Layout::kRowMajor);
  Matrix<T> b(3, 2, Layout::kColumnMajor);
  Guarded<Real> s(2);
  asc::LapackReport report;
  const auto plan = Take(Query(provider, routine, a.view(), b.view(),
                               s.vector(), Real{-1}, report));
  const auto a_before = a.bytes();
  const auto b_before = b.bytes();
  const auto s_before = s.bytes();
  Scratch<T> scratch(plan, false);
  asc::index_t rank = -37;
  auto check_failure = [&](const auto& candidate_plan, const auto& workspace,
                           Real cutoff, asc::ErrorCode expected) {
    const auto status = WithoutAllocation(test, [&] {
      return Execute(provider, routine, a.view(), b.view(), s.vector(), cutoff,
                     rank, candidate_plan, workspace, report);
    });
    ASC_DENSE_TEST_EQ(test, status.code(), expected);
    ASC_DENSE_TEST_CHECK(test, !report.called_provider);
    ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
    ASC_DENSE_TEST_EQ(test, rank, -37);
    a.CheckSame(test, a_before);
    b.CheckSame(test, b_before);
    s.CheckSame(test, s_before);
    scratch.CheckGuards(test);
  };
  check_failure(plan, scratch.view(), Real{0}, asc::ErrorCode::kInvalidState);
  check_failure(plan, scratch.view(), std::numeric_limits<Real>::infinity(),
                asc::ErrorCode::kInvalidArgument);
  check_failure(plan, scratch.view(), std::numeric_limits<Real>::quiet_NaN(),
                asc::ErrorCode::kInvalidArgument);
  auto altered_plan = plan;
  ++altered_plan.regions[Index(asc::LapackWorkspaceKind::kScalar)]
        .preferred_entries;
  check_failure(altered_plan, scratch.view(), Real{-1},
                asc::ErrorCode::kInvalidState);
  const auto check_alias = [&](const asc::LapackWorkspace& workspace) {
    const auto status = WithoutAllocation(test, [&] {
      return Execute(provider, routine, a.view(), b.view(), s.vector(),
                     Real{-1}, rank, plan, workspace, report);
    });
    ASC_DENSE_TEST_CHECK(test, !status.ok());
    ASC_DENSE_TEST_CHECK(test, !report.called_provider);
    a.CheckSame(test, a_before);
    b.CheckSame(test, b_before);
    s.CheckSame(test, s_before);
  };
  WorkspaceRejections(plan, scratch, a, check_failure, check_alias);
  for (const auto space :
       {asc::MemorySpace::kPinnedHost, asc::MemorySpace::kDevice,
        asc::MemorySpace::kManaged}) {
    const auto result = WithoutAllocation(test, [&] {
      return Query(provider, routine, a.view(space), b.view(), s.vector(),
                   Real{-1}, report);
    });
    ASC_DENSE_TEST_CHECK(test, !result.ok());
    ASC_DENSE_TEST_EQ(test, result.status().code(),
                      asc::ErrorCode::kMemoryAccess);
    ASC_DENSE_TEST_CHECK(test, !report.called_provider);
  }
  Guarded<T> unused(1);
  for (const auto kind : {asc::LapackWorkspaceKind::kLogical,
                          asc::LapackWorkspaceKind::kPivotConversion,
                          asc::LapackWorkspaceKind::kProviderComplex}) {
    auto workspace = scratch.view();
    workspace.regions[Index(kind)] = {unused.data(), sizeof(T),
                                      asc::MemorySpace::kPinnedHost};
    check_failure(plan, workspace, Real{-1}, asc::ErrorCode::kMemoryAccess);
  }
  // Signed zero is a legitimate, bit-distinct source cutoff/plan key.
  const auto negative_zero = Take(Query(provider, routine, a.view(), b.view(),
                                        s.vector(), -Real{0}, report));
  Scratch<T> zero_scratch(negative_zero, false);
  check_failure(negative_zero, zero_scratch.view(), Real{0},
                asc::ErrorCode::kInvalidState);
}

template <typename T>
void OperandRejections(TestContext& test,
                       const asc::ReferenceLapackProvider& provider,
                       Routine routine) {
  using Real = asc::DenseBlasRealType<T>;
  Matrix<T> a(3, 2, Layout::kRowMajor);
  Matrix<T> b(3, 2, Layout::kColumnMajor);
  Matrix<T> wrong_b(2, 2, Layout::kColumnMajor);
  Guarded<Real> s(4);
  const auto sv =
      Take(asc::DenseBlasVectorView<Real>::Create(s.data(), 2, 1, s.view()));
  const auto a_before = a.bytes();
  const auto b_before = b.bytes();
  const auto s_before = s.bytes();
  asc::LapackReport report;
  auto reject = [&](auto av, auto bv, auto singular, asc::ErrorCode expected) {
    const auto result = WithoutAllocation(test, [&] {
      return Query(provider, routine, av, bv, singular, Real{-1}, report);
    });
    ASC_DENSE_TEST_CHECK(test, !result.ok());
    ASC_DENSE_TEST_EQ(test, result.status().code(), expected);
    ASC_DENSE_TEST_CHECK(test, !report.called_provider);
    ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
    a.CheckSame(test, a_before);
    b.CheckSame(test, b_before);
    s.CheckSame(test, s_before);
  };
  reject(a.view(), wrong_b.view(), sv, asc::ErrorCode::kShape);
  reject(a.view(), b.view(), s.vector(), asc::ErrorCode::kShape);
  const auto strided =
      Take(asc::DenseBlasVectorView<Real>::Create(s.data(), 2, 2, s.view()));
  reject(a.view(), b.view(), strided, asc::ErrorCode::kInvalidArgument);
  reject(a.view(), a.view(), sv, asc::ErrorCode::kInvalidArgument);
  // The standard complex array representation permits examining its real
  // components; rejection must precede reading or overwriting either view.
  const auto alias = Take(asc::DenseBlasVectorView<Real>::Create(
      reinterpret_cast<Real*>(a.view().data()), 2, 1,
      {a.view().data(), a.view().reachable_storage().size(),
       asc::MemorySpace::kHost}));
  reject(a.view(), b.view(), alias, asc::ErrorCode::kInvalidArgument);
  for (const auto space :
       {asc::MemorySpace::kPinnedHost, asc::MemorySpace::kDevice,
        asc::MemorySpace::kManaged}) {
    reject(a.view(), b.view(space), sv, asc::ErrorCode::kMemoryAccess);
    const auto inaccessible = Take(asc::DenseBlasVectorView<Real>::Create(
        s.data(), 2, 1, {s.data(), s.view().size(), space}));
    reject(a.view(), b.view(), inaccessible, asc::ErrorCode::kMemoryAccess);
  }
}

template <typename T>
void MetadataRejections(TestContext& test,
                        const asc::ReferenceLapackProvider& provider,
                        Routine routine) {
  using Real = asc::DenseBlasRealType<T>;
  Matrix<T> a(3, 2, Layout::kRowMajor);
  Matrix<T> b(3, 2, Layout::kColumnMajor);
  Guarded<Real> s(2);
  asc::LapackReport report;
  auto plan = Take(Query(provider, routine, a.view(), b.view(), s.vector(),
                         Real{-1}, report));
  Scratch<T> scratch(plan, false);
  const auto scratch_before = scratch;
  auto workspace = scratch.view();
  asc::index_t rank = -37;
  const auto a_before = a.bytes();
  const auto b_before = b.bytes();
  const auto s_before = s.bytes();
  auto reject = [&](asc::index_t& candidate_rank) {
    report.called_provider = true;
    report.native_info = 73;
    const auto status = WithoutAllocation(test, [&] {
      return Execute(provider, routine, a.view(), b.view(), s.vector(),
                     Real{-1}, candidate_rank, plan, workspace, report);
    });
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidArgument);
    // Metadata overlap is rejected before report reset; the prior report
    // remains intact rather than clobbering aliased caller-owned state.
    ASC_DENSE_TEST_CHECK(test, report.called_provider);
    ASC_DENSE_TEST_EQ(test, *report.native_info, 73);
    ASC_DENSE_TEST_EQ(test, rank, -37);
    a.CheckSame(test, a_before);
    b.CheckSame(test, b_before);
    s.CheckSame(test, s_before);
    scratch.CheckSame(test, scratch_before);
  };
  report.diagnostic_index = -81;
  reject(*report.diagnostic_index);
  ASC_DENSE_TEST_EQ(test, *report.diagnostic_index, -81);
  const auto slot = Index(asc::LapackWorkspaceKind::kLayoutConversion);
  for (const auto storage : std::array<asc::MutableMemoryView, 4>{
           {{&report, sizeof(report), asc::MemorySpace::kHost},
            {&rank, sizeof(rank), asc::MemorySpace::kHost},
            {&plan, sizeof(plan), asc::MemorySpace::kHost},
            {&workspace, sizeof(workspace), asc::MemorySpace::kHost}}}) {
    workspace.regions[slot] = storage;
    reject(rank);
  }
}

template <typename T>
void ZeroIgnoringB(TestContext& test,
                   const asc::ReferenceLapackProvider& provider,
                   Routine routine, Layout layout) {
  using Real = asc::DenseBlasRealType<T>;
  // All-zero A makes B semantically ignored by both pinned drivers.
  Matrix<T> a(3, 5, layout);
  Matrix<T> b(5, 2, layout);
  Guarded<Real> s(3);
  for (asc::extent_t i = 0; i < 3; ++i) {
    for (asc::extent_t j = 0; j < 5; ++j) {
      a(i, j) = T{};
    }
  }
  for (asc::extent_t i = 0; i < 5; ++i) {
    for (asc::extent_t j = 0; j < 2; ++j) {
      b(i, j) = NotANumber<T>();
    }
  }
  asc::LapackReport report;
  const auto plan = Take(Query(provider, routine, a.view(), b.view(),
                               s.vector(), Real{-1}, report));
  Scratch<T> scratch(plan, false);
  const auto workspace = scratch.view();
  asc::index_t rank = -9;
  const auto status = WithoutAllocation(test, [&] {
    return Execute(provider, routine, a.view(), b.view(), s.vector(), Real{-1},
                   rank, plan, workspace, report);
  });
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_EQ(test, rank, 0);
  for (asc::extent_t i = 0; i < 5; ++i) {
    for (asc::extent_t j = 0; j < 2; ++j) {
      ASC_DENSE_TEST_EQ(test, b(i, j), T{});
    }
  }
  for (asc::extent_t i = 0; i < 3; ++i) {
    ASC_DENSE_TEST_EQ(test, s.data()[i], Real{0});
  }
}

template <typename T>
void Nonfinite(TestContext& test, const asc::ReferenceLapackProvider& provider,
               Routine routine, Layout layout) {
  using Real = asc::DenseBlasRealType<T>;
  for (const bool rhs_bad : {false, true}) {
    for (const Real nonfinite : {std::numeric_limits<Real>::quiet_NaN(),
                                 std::numeric_limits<Real>::infinity(),
                                 -std::numeric_limits<Real>::infinity()}) {
      for (const bool imaginary : {false, true}) {
        if (imaginary && !asc::DenseBlasComplex<T>) {
          continue;
        }
        Matrix<T> a(3, 5, layout);
        Matrix<T> b(5, 2, layout);
        Guarded<Real> s(3);
        const T poison =
            Narrow<T>(imaginary ? Wide{0, nonfinite} : Wide{nonfinite, 0});
        if (rhs_bad) {
          b(2, 1) = poison;
        } else {
          a(2, 4) = poison;
        }
        s.data()[0] = std::numeric_limits<Real>::quiet_NaN();
        const auto a_before = a.bytes();
        const auto b_before = b.bytes();
        const auto s_before = s.bytes();
        asc::LapackReport report;
        const auto plan = Take(WithoutAllocation(test, [&] {
          return Query(provider, routine, a.view(), b.view(), s.vector(),
                       Real{-1}, report);
        }));
        a.CheckSame(test, a_before);
        b.CheckSame(test, b_before);
        s.CheckSame(test, s_before);
        Scratch<T> scratch(plan, false);
        const auto scratch_before = scratch;
        const auto workspace = scratch.view();
        asc::index_t rank = -9;
        const auto calls = ForeignCalls();
        const auto status = WithoutAllocation(test, [&] {
          return Execute(provider, routine, a.view(), b.view(), s.vector(),
                         Real{-1}, rank, plan, workspace, report);
        });
        ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
        ASC_DENSE_TEST_EQ(test, ForeignCalls(), calls);
        ASC_DENSE_TEST_CHECK(test, !report.called_provider);
        ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
        ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kNotRun);
        ASC_DENSE_TEST_EQ(test, report.output_validity,
                          asc::LapackOutputValidity::kUnchanged);
        ASC_DENSE_TEST_EQ(test, rank, -9);
        a.CheckSame(test, a_before);
        b.CheckSame(test, b_before);
        s.CheckSame(test, s_before);
        scratch.CheckSame(test, scratch_before);
      }
    }
  }
  ZeroIgnoringB<T>(test, provider, routine, layout);
}

template <typename T>
void SingletonStride(TestContext& test,
                     const asc::ReferenceLapackProvider& provider,
                     Routine routine, Layout layout) {
  using Real = asc::DenseBlasRealType<T>;
  std::array<T, 1> a{Narrow<T>({-2, 0})};
  std::array<T, 1> b{Narrow<T>({-6, 0})};
  Guarded<Real> s(1);
  const auto leading = layout == Layout::kRowMajor
                           ? std::numeric_limits<asc::stride_t>::max()
                           : std::numeric_limits<Integer>::max() -
                                 (routine == Routine::kGelss ? 1 : 0);
  const auto av = Take(asc::DenseBlasMatrixView<T>::Create(
      a.data(), 1, 1, layout, leading,
      {a.data(), sizeof(a), asc::MemorySpace::kHost}));
  const auto bv = Take(asc::DenseBlasMatrixView<T>::Create(
      b.data(), 1, 1, Layout::kRowMajor,
      std::numeric_limits<asc::stride_t>::max(),
      {b.data(), sizeof(b), asc::MemorySpace::kHost}));
  asc::LapackReport report;
  const auto plan = Take(WithoutAllocation(test, [&] {
    return Query(provider, routine, av, bv, s.vector(), Real{-1}, report);
  }));
  Scratch<T> scratch(plan, false);
  const auto workspace = scratch.view();
  asc::index_t rank = -1;
  const auto status = WithoutAllocation(test, [&] {
    return Execute(provider, routine, av, bv, s.vector(), Real{-1}, rank, plan,
                   workspace, report);
  });
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_EQ(test, rank, 1);
  ASC_DENSE_TEST_EQ(test, s.data()[0], Real{2});
  ASC_DENSE_TEST_EQ(test, b[0], T{3});
  scratch.CheckGuards(test);
}

template <typename T>
int Run(const asc::ReferenceLapackProvider& provider, std::string_view scalar) {
  using Real = asc::DenseBlasRealType<T>;
  TestContext test;
  for (const Routine routine : {Routine::kGelss, Routine::kGelsd}) {
    for (const Layout layout : {Layout::kRowMajor, Layout::kColumnMajor}) {
      Nonfinite<T>(test, provider, routine, layout);
      SingletonStride<T>(test, provider, routine, layout);
      for (const auto size : {asc::extent_t{1}, asc::extent_t{2}}) {
        for (const Real cutoff :
             {-std::numeric_limits<Real>::max(), Real{-1}, -Real{0}, Real{0},
              Real{0.125}, Real{0.25}, Real{0.5}, Real{1},
              std::numeric_limits<Real>::max()}) {
          Cutoff<T>(test, provider, routine, size, cutoff, layout);
        }
      }
      for (const auto shape : {std::array<asc::extent_t, 2>{0, 0},
                               {0, 3},
                               {3, 0},
                               {3, 2},
                               {2, 3},
                               {1, 1}}) {
        for (const bool zero : {false, true}) {
          for (const asc::extent_t nrhs : {0, 2}) {
            if (nrhs == 2 && std::min(shape[0], shape[1]) > 0) {
              continue;
            }
            Empty<T>(test, provider, routine, shape[0], shape[1], nrhs, zero,
                     layout);
          }
        }
      }
    }
    Structural<T>(test, provider, routine);
    OperandRejections<T>(test, provider, routine);
    MetadataRejections<T>(test, provider, routine);
  }
  const int result = test.Finish();
  if (result == 0) {
    std::cout << "SVD_LS_CONTRACT scalar=" << scalar
              << " cutoff=finite_source_rules empty=separate_paths "
                 "workspace=all_roles_host_and_short rollback=checked\n";
  }
  return result;
}
}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 2) {
    return 2;
  }
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    return Run<float>(provider, scalar);
  }
  if (scalar == "d") {
    return Run<double>(provider, scalar);
  }
  if (scalar == "c") {
    return Run<std::complex<float>>(provider, scalar);
  }
  if (scalar == "z") {
    return Run<std::complex<double>>(provider, scalar);
  }
  return 2;
}
