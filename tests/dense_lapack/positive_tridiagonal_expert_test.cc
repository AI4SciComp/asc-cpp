#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_positive_tridiagonal.h"
#include "asc/dense/providers/lapack_positive_tridiagonal_expert.h"
#include "installed_lu/normal_return_guard.h"
#include "internal_positive_tridiagonal_expert_counts.h"
#include "internal_tridiagonal.h"
#include "positive_tridiagonal_refinement_test_support.h"
#include "tridiagonal_test_support.h"

namespace {
using asc_tridiagonal_test::Rhs;
using asc_tridiagonal_test::Take;
using asc_tridiagonal_test::TestContext;
using asc_tridiagonal_test::ToWide;
using asc_tridiagonal_test::Vector;
using asc_tridiagonal_test::Wide;
using asc_tridiagonal_test::WithoutAllocation;
constexpr auto kLower = asc::DenseBlasTriangle::kLower;
constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;
template <typename T>
using Real = asc::DenseBlasRealType<T>;

template <typename T, typename F>
void ExecuteCase(TestContext& test,
                 const asc::ReferenceLapackProvider& provider,
                 asc_ptrfs_test::Fixture<T>& fixture, F factor,
                 asc::DenseBlasLayout b_layout, asc::DenseBlasLayout x_layout,
                 asc::extent_t nrhs) {
  Rhs<T> b(fixture.n, nrhs, b_layout);
  Rhs<T> x(fixture.n, nrhs, x_layout);
  asc_ptrfs_test::Initialize(fixture, b, x);
  for (asc::extent_t j = 0; j < x.columns; ++j) {
    for (asc::extent_t i = 0; i < x.rows; ++i) {
      x.At(i, j) = T{std::numeric_limits<Real<T>>::quiet_NaN()};
    }
  }
  std::array<Real<T>, 4> ferr{-9, -9, -9, -9};
  std::array<Real<T>, 4> berr{-11, -11, -11, -11};
  Real<T> condition = -13;
  const auto fv = Vector(ferr, nrhs);
  const auto bv = Vector(berr, nrhs);
  const auto original_b = b.data;
  const auto original_d = fixture.d;
  const auto original_e = fixture.e;
  const auto plan = WithoutAllocation(test, [&] {
    return Take(asc::QueryPtsvxWorkspace(provider, fixture.Original(), factor,
                                         std::as_const(b).View(), x.View(),
                                         condition, fv, bv));
  });
  asc_ptrfs_test::Storage<T> storage;
  const auto workspace = storage.Workspace(plan);
  asc::LapackReport report;
  const auto status = WithoutAllocation(test, [&] {
    return asc::Ptsvx(provider, fixture.Original(), factor,
                      std::as_const(b).View(), x.View(), condition, fv, bv,
                      plan, workspace, report);
  });
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_EQ(test, report.called_provider, fixture.n != 0);
  ASC_DENSE_TEST_CHECK(
      test, fixture.n == 0 ? !report.native_info : report.native_info == 0);
  ASC_DENSE_TEST_CHECK(test, std::isfinite(condition) && condition > 0);
  for (asc::extent_t j = 0; j < nrhs; ++j) {
    ASC_DENSE_TEST_CHECK(test,
                         std::isfinite(fv.data()[j]) && fv.data()[j] >= 0);
    ASC_DENSE_TEST_CHECK(test,
                         std::isfinite(bv.data()[j]) && bv.data()[j] >= 0);
    for (asc::extent_t i = 0; i < fixture.n; ++i) {
      ASC_DENSE_TEST_CHECK(test,
                           std::abs(ToWide(x.At(i, j)) -
                                    ToWide(asc_ptrfs_test::Exact<T>(i, j))) <=
                               64 * std::numeric_limits<Real<T>>::epsilon());
    }
  }
  ASC_DENSE_TEST_EQ(test, b.data, original_b);
  ASC_DENSE_TEST_EQ(test, fixture.d, original_d);
  ASC_DENSE_TEST_EQ(test, fixture.e, original_e);
  b.Guards(test);
  x.Guards(test);
  storage.Guards(test, workspace);
  ASC_DENSE_TEST_EQ(test, ferr.front(), Real<T>{-9});
  ASC_DENSE_TEST_EQ(test, ferr.back(), Real<T>{-9});
  ASC_DENSE_TEST_EQ(test, berr.front(), Real<T>{-11});
  ASC_DENSE_TEST_EQ(test, berr.back(), Real<T>{-11});
}
template <typename T>
void Ordinary(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  for (const int exponent : {-16, 0, 16}) {
    for (const asc::extent_t nrhs : {0, 2}) {
      for (const asc::extent_t n : {0, 1, 3, 5, 9}) {
        for (const auto b : {asc::DenseBlasLayout::kColumnMajor,
                             asc::DenseBlasLayout::kRowMajor}) {
          for (const auto x : {asc::DenseBlasLayout::kColumnMajor,
                               asc::DenseBlasLayout::kRowMajor}) {
            for (const auto triangle : {kLower, kUpper}) {
              asc_ptrfs_test::Fixture<T> fixture(n, exponent, triangle);
              const auto d = fixture.df;
              const auto e = fixture.ef;
              ExecuteCase(test, provider, fixture, fixture.Factor(provider), b,
                          x, nrhs);
              ASC_DENSE_TEST_EQ(test, fixture.df, d);
              ASC_DENSE_TEST_EQ(test, fixture.ef, e);
            }
            asc_ptrfs_test::Fixture<T> fixture(n, exponent, kLower);
            std::array<Real<T>, 11> df{};
            std::array<T, 11> ef{};
            df.fill(std::numeric_limits<Real<T>>::quiet_NaN());
            ef.fill(T{std::numeric_limits<Real<T>>::quiet_NaN()});
            const auto factor =
                Take(asc::LapackPositiveDefiniteTridiagonalView<T>::Create(
                    Vector(df, n), Vector(ef, n == 0 ? 0 : n - 1)));
            ExecuteCase(test, provider, fixture, factor, b, x, nrhs);
            for (asc::extent_t i = 0; i < n; ++i) {
              ASC_DENSE_TEST_EQ(test, df[static_cast<std::size_t>(i + 1)],
                                fixture.df[static_cast<std::size_t>(i + 1)]);
              if (i + 1 < n) {
                ASC_DENSE_TEST_EQ(test, ef[static_cast<std::size_t>(i + 1)],
                                  fixture.ef[static_cast<std::size_t>(i + 1)]);
              }
            }
          }
        }
      }
    }
  }
}
template <typename R>
bool Same(R a, R b) {
  return a == b || (std::isnan(a) && std::isnan(b));
}
template <typename T>
void DirectScalar(Real<T> a, bool supplied, T& x, Real<T>& condition,
                  Real<T>& ferr, Real<T>& berr, lapack_int& info) {
  const lapack_int one = 1;
  const char fact = supplied ? 'F' : 'N';
  const T off{};
  T ef{};
  Real<T> df = supplied ? a : Real<T>{-7};
  const T rhs{a};
  std::array<T, 2> work{};
  Real<T> real_work{};
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_sptsvx(&fact, &one, &one, &a, &off, &df, &ef, &rhs, &one, &x, &one,
                  &condition, &ferr, &berr, work.data(), &info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dptsvx(&fact, &one, &one, &a, &off, &df, &ef, &rhs, &one, &x, &one,
                  &condition, &ferr, &berr, work.data(), &info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    LAPACK_cptsvx(&fact, &one, &one, &a, &off, &df, &ef, &rhs, &one, &x, &one,
                  &condition, &ferr, &berr, work.data(), &real_work, &info);
  } else {
    LAPACK_zptsvx(&fact, &one, &one, &a, &off, &df, &ef, &rhs, &one, &x, &one,
                  &condition, &ferr, &berr, work.data(), &real_work, &info);
  }
}

template <typename T, typename F>
void ExtremeCase(TestContext& test,
                 const asc::ReferenceLapackProvider& provider,
                 asc_ptrfs_test::Fixture<T>& fixture, F factor, bool supplied,
                 asc::DenseBlasLayout layout) {
  Rhs<T> b(1, 1, layout);
  Rhs<T> x(1, 1, layout);
  b.At(0, 0) = T{fixture.d[1]};
  x.At(0, 0) = T{-17};
  std::array<Real<T>, 3> ferr{-3, -3, -3};
  std::array<Real<T>, 3> berr{-5, -5, -5};
  Real<T> condition = -7;
  const auto fv = Vector(ferr, 1);
  const auto bv = Vector(berr, 1);
  const auto plan = Take(asc::QueryPtsvxWorkspace(
      provider, fixture.Original(), factor, std::as_const(b).View(), x.View(),
      condition, fv, bv));
  asc_ptrfs_test::Storage<T> storage;
  const auto workspace = storage.Workspace(plan);
  asc::LapackReport report;
  const auto status =
      asc::Ptsvx(provider, fixture.Original(), factor, std::as_const(b).View(),
                 x.View(), condition, fv, bv, plan, workspace, report);
  const auto value = ToWide(x.At(0, 0));
  T direct_x{-17};
  Real<T> direct_condition = -7;
  Real<T> direct_ferr = -3;
  Real<T> direct_berr = -5;
  lapack_int direct_info = std::numeric_limits<lapack_int>::min();
  DirectScalar(fixture.d[1], supplied, direct_x, direct_condition, direct_ferr,
               direct_berr, direct_info);
  const auto direct = ToWide(direct_x);
  ASC_DENSE_TEST_CHECK(test, Same(value.real(), direct.real()) &&
                                 Same(value.imag(), direct.imag()));
  ASC_DENSE_TEST_CHECK(test, Same(condition, direct_condition) &&
                                 Same(fv.data()[0], direct_ferr) &&
                                 Same(bv.data()[0], direct_berr));
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-99), direct_info);
  std::printf(
      "PTSVX direct X=(%La,%La) RCOND=%La FERR=%La BERR=%La INFO=%lld\n",
      direct.real(), direct.imag(), static_cast<long double>(direct_condition),
      static_cast<long double>(direct_ferr),
      static_cast<long double>(direct_berr),
      static_cast<long long>(direct_info));
  std::printf(
      "PTSVX bytes=%zu supplied=%d layout=%d a=%La X=(%La,%La) RCOND=%La "
      "FERR=%La BERR=%La INFO=%lld status=%d outcome=%d\n",
      sizeof(T), static_cast<int>(supplied), static_cast<int>(layout),
      static_cast<long double>(fixture.d[1]), value.real(), value.imag(),
      static_cast<long double>(condition),
      static_cast<long double>(fv.data()[0]),
      static_cast<long double>(bv.data()[0]),
      static_cast<long long>(report.native_info.value_or(-99)),
      static_cast<int>(status.code()), static_cast<int>(report.outcome));
  ASC_DENSE_TEST_CHECK(test, std::abs(value - Wide{1}) <=
                                 64 * std::numeric_limits<Real<T>>::epsilon());
  ASC_DENSE_TEST_CHECK(test,
                       std::abs(static_cast<long double>(condition) - 1) <=
                           64 * std::numeric_limits<Real<T>>::epsilon());
  ASC_DENSE_TEST_CHECK(test, std::isfinite(fv.data()[0]) && fv.data()[0] >= 0);
  ASC_DENSE_TEST_CHECK(test, std::isfinite(bv.data()[0]) && bv.data()[0] >= 0);
  storage.Guards(test, workspace);
}
template <typename T>
void Extreme(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  using R = Real<T>;
  for (const R a :
       {std::numeric_limits<R>::denorm_min(),
        2 * std::numeric_limits<R>::denorm_min(),
        std::numeric_limits<R>::min() / 8, std::numeric_limits<R>::max()}) {
    for (const auto layout : {asc::DenseBlasLayout::kColumnMajor,
                              asc::DenseBlasLayout::kRowMajor}) {
      asc_ptrfs_test::Fixture<T> fixture(1, 0, kLower);
      fixture.d[1] = a;
      fixture.df[1] = a;
      ExtremeCase(test, provider, fixture, fixture.Factor(provider), true,
                  layout);
      std::array<R, 3> df{-7, -7, -7};
      std::array<T, 3> ef{};
      const auto output =
          Take(asc::LapackPositiveDefiniteTridiagonalView<T>::Create(
              Vector(df, 1), Vector(ef, 0)));
      ExtremeCase(test, provider, fixture, output, false, layout);
    }
  }
}
template <typename T, typename F>
void Aliases(TestContext& test, const asc::ReferenceLapackProvider& provider,
             asc::LapackPositiveDefiniteTridiagonalView<const T> original,
             F factor, const Rhs<T>& b, Rhs<T>& x, Real<T>& condition,
             std::array<Real<T>, 4>& ferr, asc::DenseBlasVectorView<Real<T>> fv,
             asc::DenseBlasVectorView<Real<T>> bv,
             const asc::LapackWorkspacePlan& plan,
             const asc::LapackWorkspace& workspace, asc::LapackReport& report) {
  ASC_DENSE_TEST_EQ(test,
                    asc::QueryPtsvxWorkspace(provider, original, factor,
                                             std::as_const(b).View(), x.View(),
                                             condition, fv, fv)
                        .status()
                        .code(),
                    asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test,
                    asc::QueryPtsvxWorkspace(provider, original, factor,
                                             std::as_const(x).View(), x.View(),
                                             condition, fv, bv)
                        .status()
                        .code(),
                    asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test,
                    asc::QueryPtsvxWorkspace(provider, original, factor,
                                             std::as_const(b).View(), x.View(),
                                             ferr[1], fv, bv)
                        .status()
                        .code(),
                    asc::ErrorCode::kInvalidArgument);
  auto alias_workspace = workspace;
  alias_workspace.regions[static_cast<std::size_t>(
      asc::LapackWorkspaceKind::kLayoutConversion)] = {
      x.data.data(), sizeof(x.data), asc::MemorySpace::kHost};
  ASC_DENSE_TEST_EQ(
      test,
      asc::Ptsvx(provider, original, factor, std::as_const(b).View(), x.View(),
                 condition, fv, bv, plan, alias_workspace, report)
          .code(),
      asc::ErrorCode::kInvalidArgument);
}
template <typename T, typename F>
void PreflightCase(TestContext& test,
                   const asc::ReferenceLapackProvider& provider,
                   asc_ptrfs_test::Fixture<T>& fixture, F factor) {
  Rhs<T> b(3, 2, asc_tridiagonal_test::kRow);
  Rhs<T> x(3, 2, asc_tridiagonal_test::kColumn);
  asc_ptrfs_test::Initialize(fixture, b, x);
  std::array<Real<T>, 4> ferr{-3, -3, -3, -3};
  std::array<Real<T>, 4> berr{-5, -5, -5, -5};
  Real<T> condition = -7;
  const auto fv = Vector(ferr, 2);
  const auto bv = Vector(berr, 2);
  const auto original = fixture.Original();
  const auto plan = Take(asc::QueryPtsvxWorkspace(provider, original, factor,
                                                  std::as_const(b).View(),
                                                  x.View(), condition, fv, bv));
  asc_ptrfs_test::Storage<T> storage;
  const auto workspace = storage.Workspace(plan);
  const auto before = storage;
  const auto old_x = x.data;
  fixture.df[1] = std::numeric_limits<Real<T>>::quiet_NaN();
  ASC_DENSE_TEST_CHECK(
      test, asc::QueryPtsvxWorkspace(provider, original, factor,
                                     std::as_const(b).View(), x.View(),
                                     condition, fv, bv)
                .ok());
  asc::LapackReport report;
  for (std::size_t i = 0; i < workspace.regions.size(); ++i) {
    if (workspace.regions[i].size() == 0) {
      continue;
    }
    auto short_workspace = workspace;
    short_workspace.regions[i] = {workspace.regions[i].data(),
                                  workspace.regions[i].size() - 1,
                                  asc::MemorySpace::kHost};
    const auto status =
        asc::Ptsvx(provider, original, factor, std::as_const(b).View(),
                   x.View(), condition, fv, bv, plan, short_workspace, report);
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
  }
  Rhs<T> changed(3, 2, asc_tridiagonal_test::kRow);
  const auto stale =
      asc::Ptsvx(provider, original, factor, std::as_const(b).View(),
                 changed.View(), condition, fv, bv, plan, workspace, report);
  ASC_DENSE_TEST_EQ(test, stale.code(), asc::ErrorCode::kInvalidState);
  ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
  Aliases(test, provider, original, factor, b, x, condition, ferr, fv, bv, plan,
          workspace, report);
  if constexpr (std::is_same_v<
                    F,
                    asc::ReferencePositiveDefiniteTridiagonalFactorView<T>>) {
    ASC_DENSE_TEST_EQ(
        test,
        asc::Ptsvx(provider, original, factor, std::as_const(b).View(),
                   x.View(), condition, fv, bv, plan, workspace, report)
            .code(),
        asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_CHECK(
        test, !report.called_provider && report.diagnostic_index == 0);
  }
  ASC_DENSE_TEST_EQ(test, x.data, old_x);
  ASC_DENSE_TEST_EQ(test, condition, Real<T>{-7});
  ASC_DENSE_TEST_EQ(test, storage.native.scalar, before.native.scalar);
  ASC_DENSE_TEST_EQ(test, storage.native.real, before.native.real);
  ASC_DENSE_TEST_EQ(test, storage.native.packed, before.native.packed);
  ASC_DENSE_TEST_EQ(test, storage.estimates, before.estimates);
  for (std::size_t i = 0; i < ferr.size(); ++i) {
    ASC_DENSE_TEST_EQ(test, ferr[i], Real<T>{-3});
    ASC_DENSE_TEST_EQ(test, berr[i], Real<T>{-5});
  }
}
template <typename T>
void Preflight(TestContext& test,
               const asc::ReferenceLapackProvider& provider) {
  asc_ptrfs_test::Fixture<T> fixture(3, 0, kUpper);
  PreflightCase(test, provider, fixture, fixture.Factor(provider));
  fixture.triangle = kLower;
  const auto output =
      Take(asc::LapackPositiveDefiniteTridiagonalView<T>::Create(
          Vector(fixture.df, 3), Vector(fixture.ef, 2)));
  PreflightCase(test, provider, fixture, output);
}
void Counts(TestContext& test) {
  namespace counts = asc::internal_positive_tridiagonal_expert_counts;
  for (const asc::extent_t limit :
       {static_cast<asc::extent_t>(std::numeric_limits<std::int32_t>::max()),
        std::numeric_limits<asc::extent_t>::max()}) {
    ASC_DENSE_TEST_CHECK(
        test, counts::Solve(limit / 2, 1, limit / 2, false, limit).ok());
    ASC_DENSE_TEST_EQ(
        test, counts::Solve(limit / 2 + 1, 1, limit, false, limit).code(),
        asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_CHECK(test,
                         counts::Solve(limit - 1, 0, limit, false, limit).ok());
    ASC_DENSE_TEST_EQ(test, counts::Solve(limit, 0, limit, true, limit).code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_CHECK(test,
                         counts::Solve(limit - 1, 1, limit, true, limit).ok());
    ASC_DENSE_TEST_EQ(test, counts::Solve(1, limit, 1, true, limit).code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_CHECK(test, counts::Solve(0, limit, 1, false, limit).ok());
  }
}

template <typename T, typename F>
int ConcurrentCalls(const asc::ReferenceLapackProvider& provider,
                    asc_ptrfs_test::Fixture<T>& fixture, F factor,
                    const asc::LapackWorkspacePlan& plan, std::size_t worker) {
  Rhs<T> b(5, 2, asc_tridiagonal_test::kRow);
  Rhs<T> x(5, 2, asc_tridiagonal_test::kColumn);
  std::array<Real<T>, 4> ferr{};
  std::array<Real<T>, 4> berr{};
  Real<T> condition = -7;
  const auto fv = Vector(ferr, 2);
  const auto bv = Vector(berr, 2);
  asc_ptrfs_test::Storage<T> storage;
  const auto workspace = storage.Workspace(plan);
  asc::LapackReport report;
  const auto multiplier = static_cast<Real<T>>(worker + 1);
  int failures = 0;
  for (int pass = 0; pass < 32; ++pass) {
    asc_ptrfs_test::Initialize(fixture, b, x);
    for (asc::extent_t j = 0; j < 2; ++j) {
      for (asc::extent_t i = 0; i < 5; ++i) {
        b.At(i, j) *= multiplier;
        x.At(i, j) = T{-17};
      }
    }
    const bool invalid = pass == 31;
    if (invalid) {
      if constexpr (std::is_same_v<
                        F, asc::ReferencePositiveDefiniteTridiagonalFactorView<
                               T>>) {
        fixture.df[worker + 1] = -1;
      } else {
        fixture.d[worker + 1] = -1;
        for (std::size_t i = 1; i < 5; ++i) {
          fixture.e[i] = T{};
        }
      }
    }
    const auto status = asc::Ptsvx(provider, fixture.Original(), factor,
                                   std::as_const(b).View(), x.View(), condition,
                                   fv, bv, plan, workspace, report);
    if (invalid) {
      const bool called = !std::is_same_v<
          F, asc::ReferencePositiveDefiniteTridiagonalFactorView<T>>;
      if (status.code() != asc::ErrorCode::kNumerical ||
          report.called_provider != called ||
          report.diagnostic_index != static_cast<asc::extent_t>(worker) ||
          (called ? report.native_info != static_cast<asc::extent_t>(worker + 1)
                  : report.native_info.has_value())) {
        ++failures;
      }
      for (asc::extent_t j = 0; j < 2; ++j) {
        for (asc::extent_t i = 0; i < 5; ++i) {
          if (x.At(i, j) != T{-17}) {
            ++failures;
          }
        }
      }
      continue;
    }
    if (!status.ok() || !report.called_provider || report.native_info != 0 ||
        report.output_validity != asc::LapackOutputValidity::kComplete ||
        !std::isfinite(condition) || condition <= 0 ||
        report.diagnostic_index.has_value()) {
      ++failures;
    }
    for (asc::extent_t j = 0; j < 2; ++j) {
      if (!std::isfinite(fv.data()[j]) || !std::isfinite(bv.data()[j]) ||
          fv.data()[j] < 0 || bv.data()[j] < 0) {
        ++failures;
      }
      for (asc::extent_t i = 0; i < 5; ++i) {
        const auto expected = ToWide(asc_ptrfs_test::Exact<T>(i, j)) *
                              static_cast<long double>(multiplier);
        if (std::abs(ToWide(x.At(i, j)) - expected) >
            64 * std::numeric_limits<Real<T>>::epsilon() * std::abs(expected)) {
          ++failures;
        }
      }
    }
  }
  return failures;
}
template <typename T>
void Concurrent(TestContext& test,
                const asc::ReferenceLapackProvider& provider) {
  // Allocation auditing is disabled while threads run; each worker owns its
  // original/factor arrays, context, workspace and report. Only plans are
  // shared.
  asc_ptrfs_test::Fixture<T> fixture(5, 0, kUpper);
  Rhs<T> b(5, 2, asc_tridiagonal_test::kRow);
  Rhs<T> x(5, 2, asc_tridiagonal_test::kColumn);
  std::array<Real<T>, 4> ferr{};
  std::array<Real<T>, 4> berr{};
  Real<T> condition = -7;
  const auto generated =
      Take(asc::LapackPositiveDefiniteTridiagonalView<T>::Create(
          Vector(fixture.df, 5), Vector(fixture.ef, 4)));
  const auto fresh_plan = Take(asc::QueryPtsvxWorkspace(
      provider, fixture.Original(), generated, std::as_const(b).View(),
      x.View(), condition, Vector(ferr, 2), Vector(berr, 2)));
  const auto supplied_plan = Take(asc::QueryPtsvxWorkspace(
      provider, fixture.Original(), fixture.Factor(provider),
      std::as_const(b).View(), x.View(), condition, Vector(ferr, 2),
      Vector(berr, 2)));
  std::array<std::thread, 4> threads;
  std::array<int, 4> failures{};
  for (std::size_t i = 0; i < threads.size(); ++i) {
    threads[i] = std::thread([&, i] {
      const auto context = Take(asc::ReferenceLapackProvider::Create(
          asc::ExecutionContext::Serial()));
      asc_ptrfs_test::Fixture<T> supplied(5, 0, kUpper);
      asc_ptrfs_test::Fixture<T> fresh(5, 0, kLower);
      const auto output =
          Take(asc::LapackPositiveDefiniteTridiagonalView<T>::Create(
              Vector(fresh.df, 5), Vector(fresh.ef, 4)));
      failures[i] = ConcurrentCalls<T>(
          context, supplied, supplied.Factor(context), supplied_plan, i);
      failures[i] += ConcurrentCalls<T>(context, fresh, output, fresh_plan, i);
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
  for (const auto count : failures) {
    ASC_DENSE_TEST_EQ(test, count, 0);
  }
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider,
         bool extreme) {
  if (extreme) {
    Extreme<T>(test, provider);
  } else {
    Ordinary<T>(test, provider);
    Preflight<T>(test, provider);
    Counts(test);
    Concurrent<T>(test, provider);
  }
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard normal_return;
  if (argc < 2 || argc > 3) {
    return 2;
  }
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const std::string_view scalar = argv[1];
  const bool extreme = argc == 3;
  if (scalar == "s") {
    Run<float>(test, provider, extreme);
  } else if (scalar == "d") {
    Run<double>(test, provider, extreme);
  } else if (scalar == "c") {
    Run<std::complex<float>>(test, provider, extreme);
  } else if (scalar == "z") {
    Run<std::complex<double>>(test, provider, extreme);
  } else {
    return 2;
  }
  return test.Finish();
}
