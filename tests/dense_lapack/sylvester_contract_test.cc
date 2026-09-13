#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <source_location>
#include <string_view>
#include <vector>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_sylvester.h"
#include "installed_lu/normal_return_guard.h"
#include "sylvester_faults.h"
#include "sylvester_test_support.h"

namespace {
using asc::extent_t;
using asc_sylvester_test::CharacterLengthsValid;
using asc_sylvester_test::Fault;
using asc_sylvester_test::ForeignCalls;
using asc_sylvester_test::kPacking;
using asc_sylvester_test::Layout;
using asc_sylvester_test::Matrix;
using asc_sylvester_test::NotANumber;
using asc_sylvester_test::Operation;
using asc_sylvester_test::ProviderIntegerMinimum;
using asc_sylvester_test::Scratch;
using asc_sylvester_test::SetFault;
using asc_sylvester_test::Take;
using asc_sylvester_test::TestContext;
using asc_sylvester_test::WithoutAllocation;
using Sign = asc::LapackSylvesterSign;

template <typename T>
struct Fixture {
  Matrix<T> a;
  Matrix<T> b;
  Matrix<T> c;
  asc::DenseBlasRealType<T> scale = -73;
  asc::LapackReport report;
  explicit Fixture(Layout layout)
      : a(3, 3, layout), b(2, 2, layout), c(3, 2, layout) {
    for (extent_t i = 0; i < 3; ++i) {
      for (extent_t j = 0; j < 3; ++j) {
        a(i, j) = i == j ? T{3} : T{};
      }
      for (extent_t j = 0; j < 2; ++j) {
        c(i, j) = T{2};
      }
    }
    for (extent_t i = 0; i < 2; ++i) {
      for (extent_t j = 0; j < 2; ++j) {
        b(i, j) = i == j ? T{7} : T{};
      }
    }
  }
  auto Query(const asc::ReferenceLapackProvider& provider) {
    return asc::QueryTrsylWorkspace(
        provider, Operation::kNone, Operation::kNone, Sign::kPlus,
        a.const_view(), b.const_view(), c.view(), report);
  }
  auto Execute(const asc::ReferenceLapackProvider& provider,
               const asc::LapackWorkspacePlan& plan,
               const asc::LapackWorkspace& workspace) {
    return asc::Trsyl(provider, Operation::kNone, Operation::kNone, Sign::kPlus,
                      a.const_view(), b.const_view(), c.view(), scale, plan,
                      workspace, report);
  }
};

template <typename T, typename Function>
void Reject(TestContext& test, Fixture<T>& f, Scratch<T>& scratch,
            Function function, asc::ErrorCode expected,
            std::source_location caller = std::source_location::current()) {
  const auto a = f.a.bytes();
  const auto b = f.b.bytes();
  const auto c = f.c.bytes();
  // Keep an independent pre-call snapshot even though mutation is indirect.
  const std::vector<T> work(scratch.bytes().begin(), scratch.bytes().end());
  const auto scale = f.scale;
  const auto calls = ForeignCalls();
  const auto status = WithoutAllocation(test, function);
  test.CheckEqual(status.code(), expected, "status.code()", "expected",
                  caller.file_name(), static_cast<int>(caller.line()));
  ASC_DENSE_TEST_EQ(test, ForeignCalls(), calls);
  ASC_DENSE_TEST_CHECK(test,
                       !f.report.called_provider && !f.report.native_info);
  ASC_DENSE_TEST_EQ(test, f.report.outcome, asc::LapackOutcome::kNotRun);
  ASC_DENSE_TEST_EQ(test, f.scale, scale);
  f.a.CheckSame(test, a);
  f.b.CheckSame(test, b);
  f.c.CheckSame(test, c);
  scratch.CheckSame(test, work);
}

template <typename T>
void CheckWorkspace(TestContext& test,
                    const asc::ReferenceLapackProvider& provider, Fixture<T>& f,
                    const asc::LapackWorkspacePlan& plan, Scratch<T>& scratch) {
  auto short_workspace = scratch.workspace;
  const auto region = short_workspace.regions[kPacking];
  short_workspace.regions[kPacking] = asc::MutableMemoryView(
      region.data(), region.size() - sizeof(T), asc::MemorySpace::kHost);
  Reject(
      test, f, scratch,
      [&] { return f.Execute(provider, plan, short_workspace); },
      asc::ErrorCode::kInvalidArgument);
  auto stale = plan;
  ++stale.regions[kPacking].preferred_entries;
  Reject(
      test, f, scratch,
      [&] { return f.Execute(provider, stale, scratch.workspace); },
      asc::ErrorCode::kInvalidState);
  // Every nonempty supplied role, including unused roles, is provider-bound.
  std::array<std::byte, 64> unused{};
  for (std::size_t role = 0; role < scratch.workspace.regions.size(); ++role) {
    auto inaccessible = scratch.workspace;
    const auto selected =
        role == kPacking ? region
                         : asc::MutableMemoryView(unused.data(), unused.size(),
                                                  asc::MemorySpace::kHost);
    inaccessible.regions[role] = asc::MutableMemoryView(
        selected.data(), selected.size(), asc::MemorySpace::kPinnedHost);
    Reject(
        test, f, scratch,
        [&] { return f.Execute(provider, plan, inaccessible); },
        asc::ErrorCode::kMemoryAccess);
  }
  auto overlapping = scratch.workspace;
  overlapping.regions[0] = overlapping.regions[kPacking];
  Reject(
      test, f, scratch, [&] { return f.Execute(provider, plan, overlapping); },
      asc::ErrorCode::kInvalidArgument);
  // A byte-misaligned region is rejected without dereferencing a scalar.
  auto misaligned = scratch.workspace;
  misaligned.regions[kPacking] =
      asc::MutableMemoryView(static_cast<std::byte*>(region.data()) + 1,
                             region.size() - 1, asc::MemorySpace::kHost);
  Reject(
      test, f, scratch, [&] { return f.Execute(provider, plan, misaligned); },
      asc::ErrorCode::kInvalidArgument);
}

template <typename T>
void CheckOptions(TestContext& test,
                  const asc::ReferenceLapackProvider& provider, Fixture<T>& f,
                  const asc::LapackWorkspacePlan& plan, Scratch<T>& scratch) {
  Reject(
      test, f, scratch,
      [&] {
        // Intentional invalid enumerator exercises checked argument rejection.
        // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
        const auto invalid_sign = static_cast<Sign>(0);
        return asc::Trsyl(provider, Operation::kNone, Operation::kNone,
                          invalid_sign, f.a.const_view(), f.b.const_view(),
                          f.c.view(), f.scale, plan, scratch.workspace,
                          f.report);
      },
      asc::ErrorCode::kInvalidArgument);
  Reject(
      test, f, scratch,
      [&] {
        // Intentional invalid enumerator exercises checked argument rejection.
        // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
        const auto invalid_operation = static_cast<Operation>(-1);
        return asc::Trsyl(provider, invalid_operation, Operation::kNone,
                          Sign::kPlus, f.a.const_view(), f.b.const_view(),
                          f.c.view(), f.scale, plan, scratch.workspace,
                          f.report);
      },
      asc::ErrorCode::kInvalidArgument);
  if constexpr (asc::DenseBlasComplex<T>) {
    Reject(
        test, f, scratch,
        [&] {
          return asc::Trsyl(provider, Operation::kTranspose, Operation::kNone,
                            Sign::kPlus, f.a.const_view(), f.b.const_view(),
                            f.c.view(), f.scale, plan, scratch.workspace,
                            f.report);
        },
        asc::ErrorCode::kInvalidArgument);
  }
}

template <typename T>
void CheckShapes(TestContext& test,
                 const asc::ReferenceLapackProvider& provider, Fixture<T>& f,
                 const asc::LapackWorkspacePlan& plan, Scratch<T>& scratch) {
  Reject(
      test, f, scratch,
      [&] {
        return asc::Trsyl(provider, Operation::kNone, Operation::kNone,
                          Sign::kPlus, f.b.const_view(), f.b.const_view(),
                          f.c.view(), f.scale, plan, scratch.workspace,
                          f.report);
      },
      asc::ErrorCode::kInvalidArgument);
  const auto original_c = f.c.view();
  const auto wrong_c = Take(asc::DenseBlasMatrixView<T>::Create(
      original_c.data(), 2, 2, original_c.layout(),
      original_c.leading_dimension(), original_c.reachable_storage()));
  Reject(
      test, f, scratch,
      [&] {
        return asc::Trsyl(provider, Operation::kNone, Operation::kNone,
                          Sign::kPlus, f.a.const_view(), f.b.const_view(),
                          wrong_c, f.scale, plan, scratch.workspace, f.report);
      },
      asc::ErrorCode::kShape);
  Reject(
      test, f, scratch,
      [&] {
        return asc::Trsyl(provider, Operation::kNone, Operation::kNone,
                          Sign::kPlus, f.a.const_view(), f.b.const_view(),
                          f.c.view(asc::MemorySpace::kPinnedHost), f.scale,
                          plan, scratch.workspace, f.report);
      },
      asc::ErrorCode::kMemoryAccess);
}

template <typename T>
void CheckSchurStructure(TestContext& test,
                         const asc::ReferenceLapackProvider& provider,
                         Fixture<T>& f, const asc::LapackWorkspacePlan& plan,
                         Scratch<T>& scratch) {
  if constexpr (!asc::DenseBlasComplex<T>) {
    // Query is numerical-input-free even for an invalid Schur representation.
    f.a(1, 0) = -1;
    f.a(0, 1) = 1;
    f.a(1, 1) = 4;
    ASC_DENSE_TEST_CHECK(test, f.Query(provider).ok());
    Reject(
        test, f, scratch,
        [&] { return f.Execute(provider, plan, scratch.workspace); },
        asc::ErrorCode::kInvalidArgument);
    f.a(1, 1) = 3;
    f.a(0, 1) = -1;
    Reject(
        test, f, scratch,
        [&] { return f.Execute(provider, plan, scratch.workspace); },
        asc::ErrorCode::kInvalidArgument);
    f.a(0, 1) = 1;
    f.a(2, 1) = -1;
    f.a(1, 2) = 1;
    Reject(
        test, f, scratch,
        [&] { return f.Execute(provider, plan, scratch.workspace); },
        asc::ErrorCode::kInvalidArgument);
    f.a(2, 1) = 0;
    f.a(0, 1) = NotANumber<T>();
    Reject(
        test, f, scratch,
        [&] { return f.Execute(provider, plan, scratch.workspace); },
        asc::ErrorCode::kInvalidArgument);
  }
}

template <typename T>
void Preflight(TestContext& test, const asc::ReferenceLapackProvider& provider,
               Layout layout) {
  Fixture<T> f(layout);
  const auto calls = ForeignCalls();
  const auto plan =
      Take(WithoutAllocation(test, [&] { return f.Query(provider); }));
  ASC_DENSE_TEST_EQ(test, ForeignCalls(), calls);
  Scratch<T> scratch(plan);
  CheckWorkspace(test, provider, f, plan, scratch);
  CheckOptions(test, provider, f, plan, scratch);
  CheckShapes(test, provider, f, plan, scratch);
  CheckSchurStructure(test, provider, f, plan, scratch);
}

template <typename T>
void LargeDiagonalCase(TestContext& test,
                       const asc::ReferenceLapackProvider& provider,
                       Layout layout, Operation operation_a,
                       Operation operation_b, Sign sign, bool negative,
                       bool imaginary) {
  using Real = asc::DenseBlasRealType<T>;
  const Real large = std::numeric_limits<Real>::max() * Real{0.75} *
                     (negative ? Real{-1} : Real{1});
  Fixture<T> f(layout);
  Real first = large;
  Real second = sign == Sign::kPlus ? large : -large;
  if (imaginary) {
    first *= operation_a == Operation::kConjugateTranspose ? -1 : 1;
    second *= operation_b == Operation::kConjugateTranspose ? -1 : 1;
  }
  f.a(2, 2) = asc_sylvester_test::Narrow<T>(
      imaginary ? asc_sylvester_test::Wide{0, first}
                : asc_sylvester_test::Wide{first, 0});
  f.b(1, 1) = asc_sylvester_test::Narrow<T>(
      imaginary ? asc_sylvester_test::Wide{0, second}
                : asc_sylvester_test::Wide{second, 0});
  const auto calls = ForeignCalls();
  const auto plan = Take(WithoutAllocation(test, [&] {
    return asc::QueryTrsylWorkspace(provider, operation_a, operation_b, sign,
                                    f.a.const_view(), f.b.const_view(),
                                    f.c.view(), f.report);
  }));
  ASC_DENSE_TEST_EQ(test, ForeignCalls(), calls);
  Scratch<T> scratch(plan);
  Reject(
      test, f, scratch,
      [&] {
        return asc::Trsyl(provider, operation_a, operation_b, sign,
                          f.a.const_view(), f.b.const_view(), f.c.view(),
                          f.scale, plan, scratch.workspace, f.report);
      },
      asc::ErrorCode::kNumerical);
  ASC_DENSE_TEST_EQ(test, f.report.output_validity,
                    asc::LapackOutputValidity::kUnchanged);
}

template <typename T>
void LargeDiagonals(TestContext& test,
                    const asc::ReferenceLapackProvider& provider,
                    Layout layout) {
  for (const auto a : {Operation::kNone, Operation::kTranspose,
                       Operation::kConjugateTranspose}) {
    for (const auto b : {Operation::kNone, Operation::kTranspose,
                         Operation::kConjugateTranspose}) {
      if (asc::DenseBlasComplex<T> &&
          (a == Operation::kTranspose || b == Operation::kTranspose)) {
        continue;
      }
      for (const auto sign : {Sign::kPlus, Sign::kMinus}) {
        for (const bool negative : {false, true}) {
          LargeDiagonalCase<T>(test, provider, layout, a, b, sign, negative,
                               false);
          if constexpr (asc::DenseBlasComplex<T>) {
            LargeDiagonalCase<T>(test, provider, layout, a, b, sign, negative,
                                 true);
          }
        }
      }
    }
  }
}

template <typename T>
void LargeFiniteControls(TestContext& test,
                         const asc::ReferenceLapackProvider& provider,
                         Layout layout) {
  using Real = asc::DenseBlasRealType<T>;
  const Real limit = std::numeric_limits<Real>::max();
  for (const bool cancellation : {false, true}) {
    Matrix<T> a(1, 1, layout);
    Matrix<T> b(1, 1, layout);
    Matrix<T> c(1, 1, layout);
    a(0, 0) = T{limit * (cancellation ? Real{0.75} : Real{0.5})};
    b(0, 0) = T{limit * (cancellation ? Real{-0.5} : Real{0.5})};
    c(0, 0) = T{limit * (cancellation ? Real{0.25} : Real{0.5})};
    const auto old_a = a.bytes();
    const auto old_b = b.bytes();
    const auto old_c = c.bytes();
    asc::LapackReport report;
    Real scale = -73;
    const auto plan = Take(asc::QueryTrsylWorkspace(
        provider, Operation::kNone, Operation::kNone, Sign::kPlus,
        a.const_view(), b.const_view(), c.view(), report));
    Scratch<T> scratch(plan);
    const auto calls = ForeignCalls();
    const auto status = WithoutAllocation(test, [&] {
      return asc::Trsyl(provider, Operation::kNone, Operation::kNone,
                        Sign::kPlus, a.const_view(), b.const_view(), c.view(),
                        scale, plan, scratch.workspace, report);
    });
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_EQ(test, ForeignCalls(), calls + 1);
    ASC_DENSE_TEST_EQ(test, report.native_info, 0);
    ASC_DENSE_TEST_EQ(test, scale, 1);
    const auto expected = cancellation ? Real{1} : Real{0.5};
    ASC_DENSE_TEST_CHECK(test, std::abs(c(0, 0) - T{expected}) <=
                                   8 * std::numeric_limits<Real>::epsilon());
    a.CheckSame(test, old_a);
    b.CheckSame(test, old_b);
    c.CheckPadding(test, old_c);
    scratch.CheckGuards(test);
  }
}

template <typename T>
void Empty(TestContext& test, const asc::ReferenceLapackProvider& provider,
           Layout layout) {
  for (const auto shape : {std::array<extent_t, 2>{0, 0}, {0, 3}, {3, 0}}) {
    Matrix<T> a(shape[0], shape[0], layout);
    Matrix<T> b(shape[1], shape[1], layout);
    Matrix<T> c(shape[0], shape[1], layout);
    // Meaningful but noncanonical data would fail nonempty real execution.
    for (extent_t i = 0; i < shape[0]; ++i) {
      for (extent_t j = 0; j < shape[0]; ++j) {
        a(i, j) = NotANumber<T>();
      }
    }
    for (extent_t i = 0; i < shape[1]; ++i) {
      for (extent_t j = 0; j < shape[1]; ++j) {
        b(i, j) = NotANumber<T>();
      }
    }
    const auto old_a = a.bytes();
    const auto old_b = b.bytes();
    const auto old_c = c.bytes();
    asc::LapackReport report;
    const auto before = ForeignCalls();
    const auto plan = Take(WithoutAllocation(test, [&] {
      return asc::QueryTrsylWorkspace(
          provider, Operation::kNone, Operation::kNone, Sign::kPlus,
          a.const_view(), b.const_view(), c.view(), report);
    }));
    ASC_DENSE_TEST_EQ(test, plan.regions[kPacking].minimum_entries, 0);
    const asc::LapackWorkspace workspace;
    asc::DenseBlasRealType<T> scale = -73;
    const auto status = WithoutAllocation(test, [&] {
      return asc::Trsyl(provider, Operation::kNone, Operation::kNone,
                        Sign::kPlus, a.const_view(), b.const_view(), c.view(),
                        scale, plan, workspace, report);
    });
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_EQ(test, ForeignCalls(), before);
    ASC_DENSE_TEST_EQ(test, scale, 1);
    ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kComplete);
    a.CheckSame(test, old_a);
    b.CheckSame(test, old_b);
    c.CheckSame(test, old_c);
  }
}

template <typename T>
void WideUnusedStride(TestContext& test,
                      const asc::ReferenceLapackProvider& provider) {
  T a{2};
  T b{3};
  T c{5};
  const extent_t stride = std::numeric_limits<extent_t>::max();
  const auto av = Take(asc::DenseBlasMatrixView<const T>::Create(
      &a, 1, 1, Layout::kColumnMajor, stride,
      {&a, sizeof(a), asc::MemorySpace::kHost}));
  const auto bv = Take(asc::DenseBlasMatrixView<const T>::Create(
      &b, 1, 1, Layout::kRowMajor, stride,
      {&b, sizeof(b), asc::MemorySpace::kHost}));
  const auto cv = Take(asc::DenseBlasMatrixView<T>::Create(
      &c, 1, 1, Layout::kColumnMajor, stride,
      {&c, sizeof(c), asc::MemorySpace::kHost}));
  asc::LapackReport report;
  const auto plan =
      asc::QueryTrsylWorkspace(provider, Operation::kNone, Operation::kNone,
                               Sign::kPlus, av, bv, cv, report);
  ASC_DENSE_TEST_CHECK(test, plan.ok());
  if (!plan.ok()) {
    return;
  }
  Scratch<T> scratch(*plan);
  asc::DenseBlasRealType<T> scale = -73;
  const auto before = ForeignCalls();
  const auto status = WithoutAllocation(test, [&] {
    return asc::Trsyl(provider, Operation::kNone, Operation::kNone, Sign::kPlus,
                      av, bv, cv, scale, *plan, scratch.workspace, report);
  });
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_EQ(test, ForeignCalls(), before + 1);
  ASC_DENSE_TEST_EQ(test, c, T{1});
  ASC_DENSE_TEST_EQ(test, scale, 1);
}

std::int64_t ExpectedInfo(Fault fault) {
  switch (fault) {
    case Fault::kNegative:
      return -12;
    case Fault::kMinimumInteger:
      return ProviderIntegerMinimum();
    case Fault::kImpossiblePositive:
      return -(ProviderIntegerMinimum() + 1);
    case Fault::kPerturbed:
      return 1;
    default:
      return 0;
  }
}

template <typename T>
void Faults(TestContext& test, const asc::ReferenceLapackProvider& provider,
            Layout layout) {
  for (const auto fault :
       {Fault::kNegative, Fault::kMinimumInteger, Fault::kImpossiblePositive,
        Fault::kNegativeScale, Fault::kLargeScale, Fault::kNanScale,
        Fault::kInfiniteScale, Fault::kZeroScale, Fault::kNonfiniteResult,
        Fault::kPerturbed}) {
    Fixture<T> f(layout);
    const auto plan = Take(f.Query(provider));
    Scratch<T> scratch(plan);
    const auto old_a = f.a.bytes();
    const auto old_b = f.b.bytes();
    const auto old_c = f.c.bytes();
    const auto before = ForeignCalls();
    SetFault(fault);
    const auto status = WithoutAllocation(
        test, [&] { return f.Execute(provider, plan, scratch.workspace); });
    SetFault(Fault::kNone);
    const bool warning = fault == Fault::kZeroScale ||
                         fault == Fault::kNonfiniteResult ||
                         fault == Fault::kPerturbed;
    ASC_DENSE_TEST_EQ(
        test, status.code(),
        warning ? asc::ErrorCode::kNumerical : asc::ErrorCode::kProvider);
    ASC_DENSE_TEST_EQ(test, ForeignCalls(), before + 1);
    ASC_DENSE_TEST_CHECK(test, f.report.called_provider);
    ASC_DENSE_TEST_EQ(test, f.report.native_info, ExpectedInfo(fault));
    ASC_DENSE_TEST_EQ(test, f.report.output_validity,
                      warning ? asc::LapackOutputValidity::kDocumentedPartial
                              : asc::LapackOutputValidity::kUnusable);
    if (warning) {
      ASC_DENSE_TEST_EQ(test, f.report.outcome,
                        asc::LapackOutcome::kAccuracyWarning);
      ASC_DENSE_TEST_EQ(test, f.scale, fault == Fault::kZeroScale ? 0 : 0.5);
      if (fault != Fault::kNonfiniteResult) {
        ASC_DENSE_TEST_EQ(test, f.c(0, 0), T{719});
      }
    } else {
      ASC_DENSE_TEST_EQ(test, f.scale, -73);
      if (layout == Layout::kRowMajor) {
        f.c.CheckSame(test, old_c);
      } else {
        ASC_DENSE_TEST_EQ(test, f.c(0, 0), T{719});
      }
    }
    if (fault == Fault::kNegative) {
      ASC_DENSE_TEST_EQ(test, f.report.native_argument, 12);
    } else {
      ASC_DENSE_TEST_CHECK(test, !f.report.native_argument);
    }
    f.a.CheckSame(test, old_a);
    f.b.CheckSame(test, old_b);
    f.c.CheckPadding(test, old_c);
    scratch.CheckGuards(test);
  }
}

template <typename T>
void UnwrittenInfoCase(TestContext& test,
                       const asc::ReferenceLapackProvider& provider,
                       Layout layout, Operation operation_a,
                       Operation operation_b, Sign sign, bool withheld) {
  using Real = asc::DenseBlasRealType<T>;
  Fixture<T> f(layout);
  const auto plan = Take(asc::QueryTrsylWorkspace(
      provider, operation_a, operation_b, sign, f.a.const_view(),
      f.b.const_view(), f.c.view(), f.report));
  Scratch<T> scratch(plan);
  const auto a_before = f.a.bytes();
  const auto b_before = f.b.bytes();
  const auto c_before = f.c.bytes();
  const auto calls = ForeignCalls();
  SetFault(withheld ? Fault::kUnwrittenInfo : Fault::kNone);
  const auto status = WithoutAllocation(test, [&] {
    return asc::Trsyl(provider, operation_a, operation_b, sign,
                      f.a.const_view(), f.b.const_view(), f.c.view(), f.scale,
                      plan, scratch.workspace, f.report);
  });
  SetFault(Fault::kNone);
  ASC_DENSE_TEST_EQ(test, ForeignCalls(), calls + 1);
  ASC_DENSE_TEST_CHECK(test, f.report.called_provider);
  ASC_DENSE_TEST_EQ(test, status.code(),
                    withheld ? asc::ErrorCode::kProvider : asc::ErrorCode::kOk);
  ASC_DENSE_TEST_EQ(test, f.report.native_info,
                    withheld ? ProviderIntegerMinimum() : 0);
  ASC_DENSE_TEST_CHECK(test, !f.report.native_argument.has_value());
  ASC_DENSE_TEST_EQ(test, f.report.output_validity,
                    withheld ? asc::LapackOutputValidity::kUnusable
                             : asc::LapackOutputValidity::kComplete);
  ASC_DENSE_TEST_EQ(test, f.scale, withheld ? -73 : 1);
  if (withheld && layout == Layout::kRowMajor) {
    f.c.CheckSame(test, c_before);
  } else {
    const T expected{Real{2} / (sign == Sign::kPlus ? Real{10} : Real{-4})};
    for (extent_t i = 0; i < 3; ++i) {
      for (extent_t j = 0; j < 2; ++j) {
        ASC_DENSE_TEST_CHECK(test,
                             std::abs(f.c(i, j) - expected) <=
                                 4 * std::numeric_limits<Real>::epsilon());
      }
    }
  }
  f.a.CheckSame(test, a_before);
  f.b.CheckSame(test, b_before);
  f.c.CheckPadding(test, c_before);
  scratch.CheckGuards(test);
}

template <typename T>
void UnwrittenInfo(TestContext& test,
                   const asc::ReferenceLapackProvider& provider,
                   Layout layout) {
  for (const auto a : {Operation::kNone, Operation::kTranspose,
                       Operation::kConjugateTranspose}) {
    for (const auto b : {Operation::kNone, Operation::kTranspose,
                         Operation::kConjugateTranspose}) {
      if (asc::DenseBlasComplex<T> &&
          (a == Operation::kTranspose || b == Operation::kTranspose)) {
        continue;
      }
      for (const auto sign : {Sign::kPlus, Sign::kMinus}) {
        for (const bool withheld : {false, true}) {
          UnwrittenInfoCase<T>(test, provider, layout, a, b, sign, withheld);
        }
      }
    }
  }
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  for (const auto layout : {Layout::kColumnMajor, Layout::kRowMajor}) {
    Preflight<T>(test, provider, layout);
    LargeDiagonals<T>(test, provider, layout);
    LargeFiniteControls<T>(test, provider, layout);
    Empty<T>(test, provider, layout);
    Faults<T>(test, provider, layout);
    UnwrittenInfo<T>(test, provider, layout);
  }
  WideUnusedStride<T>(test, provider);
  ASC_DENSE_TEST_CHECK(test, CharacterLengthsValid());
}
}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 2) {
    return 2;
  }
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    Run<float>(test, provider);
  } else if (scalar == "d") {
    Run<double>(test, provider);
  } else if (scalar == "c") {
    Run<std::complex<float>>(test, provider);
  } else if (scalar == "z") {
    Run<std::complex<double>>(test, provider);
  } else {
    return 2;
  }
  return test.Finish();
}
