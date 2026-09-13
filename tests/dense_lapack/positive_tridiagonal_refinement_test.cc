
#include <algorithm>
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
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_positive_tridiagonal_refinement.h"
#include "installed_lu/normal_return_guard.h"
#include "internal_positive_tridiagonal_refinement_counts.h"
#include "internal_tridiagonal.h"
#include "positive_tridiagonal_refinement_test_support.h"
#include "tridiagonal_test_support.h"

namespace {
using asc_tridiagonal_test::kColumn;
using asc_tridiagonal_test::kHost;
using asc_tridiagonal_test::kRow;
using asc_tridiagonal_test::Rhs;
using asc_tridiagonal_test::Take;
using asc_tridiagonal_test::TestContext;
using asc_tridiagonal_test::ToWide;
using asc_tridiagonal_test::Value;
using asc_tridiagonal_test::Vector;
using asc_tridiagonal_test::Wide;
using asc_tridiagonal_test::WithoutAllocation;
constexpr auto kLower = asc::DenseBlasTriangle::kLower;
constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;
template <typename T>
using Real = asc::DenseBlasRealType<T>;

using asc_ptrfs_test::Exact;
using asc_ptrfs_test::Fixture;
using asc_ptrfs_test::Initialize;
using asc_ptrfs_test::Storage;
template <typename T>
void OrdinaryCase(TestContext& test,
                  const asc::ReferenceLapackProvider& provider,
                  const Fixture<T>& fixture, asc::extent_t nrhs,
                  asc::DenseBlasLayout b_layout,
                  asc::DenseBlasLayout x_layout) {
  Rhs<T> rhs(fixture.n, nrhs, b_layout);
  Rhs<T> solution(fixture.n, nrhs, x_layout);
  Initialize(fixture, rhs, solution);
  const auto original = fixture.Original();
  const auto factor = fixture.Factor(provider);
  const auto rhs_before = rhs.data;
  std::array<Real<T>, 5> ferr{};
  std::array<Real<T>, 5> berr{};
  ferr.fill(Real<T>{-81});
  berr.fill(Real<T>{-83});
  const auto f = Vector(ferr, nrhs);
  const auto b = Vector(berr, nrhs);
  const auto plan = Take(WithoutAllocation(test, [&] {
    return asc::QueryPtrfsWorkspace(provider, original, factor,
                                    std::as_const(rhs).View(), solution.View(),
                                    f, b);
  }));
  Storage<T> storage;
  const auto workspace = storage.Workspace(plan);
  asc::LapackReport report;
  for (int pass = 0; pass < 2; ++pass) {
    const auto status = WithoutAllocation(test, [&] {
      return asc::Ptrfs(provider, original, factor, std::as_const(rhs).View(),
                        solution.View(), f, b, plan, workspace, report);
    });
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_EQ(test, report.called_provider, fixture.n > 0 && nrhs > 0);
    ASC_DENSE_TEST_EQ(test, report.native_info.has_value(),
                      report.called_provider);
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(0), 0);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kComplete);
    const auto epsilon = std::numeric_limits<Real<T>>::epsilon();
    for (asc::extent_t j = 0; j < nrhs; ++j) {
      long double error = 0;
      long double norm = 0;
      for (asc::extent_t i = 0; i < fixture.n; ++i) {
        error = std::max(error, std::abs(ToWide(solution.At(i, j)) -
                                         ToWide(Exact<T>(i, j))));
        norm = std::max(norm, std::abs(ToWide(solution.At(i, j))));
      }
      const auto at = static_cast<std::size_t>(j + 1);
      ASC_DENSE_TEST_CHECK(test, error <= 64 * epsilon * std::max(1.0L, norm));
      ASC_DENSE_TEST_CHECK(test, std::isfinite(ferr[at]) && ferr[at] >= 0 &&
                                     ferr[at] <= 256 * epsilon);
      ASC_DENSE_TEST_CHECK(test, std::isfinite(berr[at]) && berr[at] >= 0 &&
                                     berr[at] <= 64 * epsilon);
      ASC_DENSE_TEST_CHECK(
          test, error <= (ferr[at] + 8 * epsilon) * std::max(1.0L, norm));
      if (fixture.n == 0) {
        ASC_DENSE_TEST_EQ(test, ferr[at], Real<T>{0});
        ASC_DENSE_TEST_EQ(test, berr[at], Real<T>{0});
      }
    }
  }
  ASC_DENSE_TEST_EQ(test, rhs.data, rhs_before);
  ASC_DENSE_TEST_EQ(test, ferr.front(), Real<T>{-81});
  ASC_DENSE_TEST_EQ(test, berr.front(), Real<T>{-83});
  for (std::size_t i = static_cast<std::size_t>(nrhs + 1); i < ferr.size();
       ++i) {
    ASC_DENSE_TEST_EQ(test, ferr[i], Real<T>{-81});
    ASC_DENSE_TEST_EQ(test, berr[i], Real<T>{-83});
  }
  solution.Guards(test);
  storage.Guards(test, workspace);
}
template <typename T>
void Ordinary(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  for (const auto triangle : {kLower, kUpper}) {
    for (const asc::extent_t n : {0, 1, 2, 5, 9}) {
      for (const int exponent : {-8, 0, 8}) {
        const Fixture<T> fixture(n, exponent, triangle);
        for (const asc::extent_t nrhs : {0, 1, 3}) {
          for (const auto b : {kColumn, kRow}) {
            for (const auto x : {kColumn, kRow}) {
              OrdinaryCase(test, provider, fixture, nrhs, b, x);
            }
          }
        }
      }
    }
  }
}

template <typename T>
void Preflight(TestContext& test,
               const asc::ReferenceLapackProvider& provider) {
  Fixture<T> fixture(2, 0, kLower);
  const auto original = fixture.Original();
  const auto factor = fixture.Factor(provider);
  Rhs<T> rhs(2, 3, kRow);
  Rhs<T> solution(2, 3, kColumn);
  Initialize(fixture, rhs, solution);
  const auto x_before = solution.data;
  std::array<Real<T>, 5> ferr{};
  std::array<Real<T>, 5> berr{};
  ferr.fill(Real<T>{-81});
  berr.fill(Real<T>{-83});
  const auto f = Vector(ferr, 3);
  const auto b = Vector(berr, 3);
  const auto plan = Take(asc::QueryPtrfsWorkspace(provider, original, factor,
                                                  std::as_const(rhs).View(),
                                                  solution.View(), f, b));
  Storage<T> storage;
  const auto workspace = storage.Workspace(plan);
  const auto untouched = storage;
  asc::LapackReport report;
  // Invalid factors must not take precedence over a rejected workspace, and
  // queries must not manufacture a factor-value input dependency.
  fixture.df[1] = std::numeric_limits<Real<T>>::quiet_NaN();
  ASC_DENSE_TEST_CHECK(
      test,
      asc::QueryPtrfsWorkspace(provider, original, factor,
                               std::as_const(rhs).View(), solution.View(), f, b)
          .ok());
  for (std::size_t i = 0; i < workspace.regions.size(); ++i) {
    if (workspace.regions[i].size() == 0) {
      continue;
    }
    auto short_workspace = workspace;
    short_workspace.regions[i] = {workspace.regions[i].data(),
                                  workspace.regions[i].size() - 1, kHost};
    const auto status =
        asc::Ptrfs(provider, original, factor, std::as_const(rhs).View(),
                   solution.View(), f, b, plan, short_workspace, report);
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_CHECK(
        test, !report.called_provider && !report.native_info.has_value());
  }
  const auto invalid =
      asc::Ptrfs(provider, original, factor, std::as_const(rhs).View(),
                 solution.View(), f, b, plan, workspace, report);
  ASC_DENSE_TEST_EQ(test, invalid.code(), asc::ErrorCode::kNumerical);
  ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-99), 0);
  ASC_DENSE_TEST_CHECK(test, !report.called_provider);
  Rhs<T> changed(2, 3, kRow);
  const auto stale =
      asc::Ptrfs(provider, original, factor, std::as_const(rhs).View(),
                 changed.View(), f, b, plan, workspace, report);
  ASC_DENSE_TEST_EQ(test, stale.code(), asc::ErrorCode::kInvalidState);
  ASC_DENSE_TEST_CHECK(test,
                       !asc::QueryPtrfsWorkspace(provider, original, factor,
                                                 std::as_const(rhs).View(),
                                                 solution.View(), f, f)
                            .ok());
  ASC_DENSE_TEST_EQ(test, solution.data, x_before);
  ASC_DENSE_TEST_EQ(test, storage.native.scalar, untouched.native.scalar);
  ASC_DENSE_TEST_EQ(test, storage.native.real, untouched.native.real);
  ASC_DENSE_TEST_EQ(test, storage.native.packed, untouched.native.packed);
  ASC_DENSE_TEST_EQ(test, storage.estimates, untouched.estimates);
  for (std::size_t i = 0; i < ferr.size(); ++i) {
    ASC_DENSE_TEST_EQ(test, ferr[i], Real<T>{-81});
    ASC_DENSE_TEST_EQ(test, berr[i], Real<T>{-83});
  }
}
void Counts(TestContext& test) {
  namespace counts = asc::internal_positive_tridiagonal_refinement_counts;
  for (const asc::extent_t limit :
       {static_cast<asc::extent_t>(std::numeric_limits<std::int32_t>::max()),
        std::numeric_limits<asc::extent_t>::max()}) {
    ASC_DENSE_TEST_CHECK(
        test,
        counts::Refine(limit / 2, 1, limit / 2, limit / 2, false, limit).ok());
    ASC_DENSE_TEST_EQ(
        test,
        counts::Refine(limit / 2 + 1, 1, limit, limit, false, limit).code(),
        asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_CHECK(
        test, counts::Refine(limit - 1, 1, limit, limit, true, limit).ok());
    ASC_DENSE_TEST_EQ(
        test, counts::Refine(limit, 1, limit, limit, true, limit).code(),
        asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(test, counts::Refine(1, limit, 1, 1, true, limit).code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_CHECK(test,
                         counts::Refine(0, limit, 1, 1, false, limit).ok());
    ASC_DENSE_TEST_CHECK(
        test, counts::Refine(limit, 0, limit, limit, false, limit).ok());
  }
}
template <typename T>
int ConcurrentCalls(const asc::ReferenceLapackProvider& provider,
                    const Fixture<T>& fixture,
                    const asc::LapackWorkspacePlan& plan, Real<T> multiplier) {
  Rhs<T> rhs(5, 3, kRow);
  Rhs<T> solution(5, 3, kColumn);
  std::array<Real<T>, 5> ferr{};
  std::array<Real<T>, 5> berr{};
  Storage<T> storage;
  const auto workspace = storage.Workspace(plan);
  const auto original = fixture.Original();
  const auto factor = fixture.Factor(provider);
  const auto f = Vector(ferr, 3);
  const auto b = Vector(berr, 3);
  asc::LapackReport report;
  int failures = 0;
  for (int pass = 0; pass < 32; ++pass) {
    Initialize(fixture, rhs, solution);
    for (asc::extent_t j = 0; j < 3; ++j) {
      for (asc::extent_t i = 0; i < 5; ++i) {
        rhs.At(i, j) *= multiplier;
        solution.At(i, j) *= multiplier;
      }
    }
    const auto status =
        asc::Ptrfs(provider, original, factor, std::as_const(rhs).View(),
                   solution.View(), f, b, plan, workspace, report);
    if (!status.ok() || !report.called_provider ||
        report.native_info.value_or(-99) != 0 ||
        report.output_validity != asc::LapackOutputValidity::kComplete) {
      ++failures;
    }
    for (asc::extent_t j = 0; j < 3; ++j) {
      const auto at = static_cast<std::size_t>(j + 1);
      if (!std::isfinite(ferr[at]) || !std::isfinite(berr[at]) ||
          ferr[at] < 0 || berr[at] < 0) {
        ++failures;
      }
      for (asc::extent_t i = 0; i < 5; ++i) {
        const auto expected =
            ToWide(Exact<T>(i, j)) * static_cast<long double>(multiplier);
        if (std::abs(ToWide(solution.At(i, j)) - expected) >
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
  // One immutable factor/plan, independent caller buffers, reports, contexts
  // and workspace. Fault injection and global allocation auditing are inactive.
  const Fixture<T> fixture(5, 0, kUpper);
  Rhs<T> rhs(5, 3, kRow);
  Rhs<T> solution(5, 3, kColumn);
  std::array<Real<T>, 5> ferr{};
  std::array<Real<T>, 5> berr{};
  const auto plan = Take(asc::QueryPtrfsWorkspace(
      provider, fixture.Original(), fixture.Factor(provider),
      std::as_const(rhs).View(), solution.View(), Vector(ferr, 3),
      Vector(berr, 3)));
  std::array<std::thread, 4> threads;
  std::array<int, 4> failures{};
  for (std::size_t i = 0; i < threads.size(); ++i) {
    threads[i] = std::thread([&, i] {
      const auto context = Take(asc::ReferenceLapackProvider::Create(
          asc::ExecutionContext::Serial()));
      failures[i] = ConcurrentCalls<T>(context, fixture, plan,
                                       static_cast<Real<T>>(i + 1));
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
  for (auto failed : failures) {
    ASC_DENSE_TEST_EQ(test, failed, 0);
  }
}
template <typename R>
bool Same(R a, R b) {
  return a == b || (std::isnan(a) && std::isnan(b));
}
template <typename T>
void DirectScalar(Real<T> value, asc::DenseBlasTriangle triangle, T& direct_x,
                  Real<T>& direct_f, Real<T>& direct_b, lapack_int& info) {
  using R = Real<T>;
  const lapack_int one = 1;
  const T off{};
  const T direct_rhs = Value<T>(value);
  R real_work{};
  std::array<T, 2> work{};
  const char uplo = triangle == kLower ? 'L' : 'U';
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_sptrfs(&one, &one, &value, &off, &value, &off, &direct_rhs, &one,
                  &direct_x, &one, &direct_f, &direct_b, work.data(), &info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dptrfs(&one, &one, &value, &off, &value, &off, &direct_rhs, &one,
                  &direct_x, &one, &direct_f, &direct_b, work.data(), &info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    LAPACK_cptrfs(&uplo, &one, &one, &value, &off, &value, &off, &direct_rhs,
                  &one, &direct_x, &one, &direct_f, &direct_b, work.data(),
                  &real_work, &info);
  } else {
    LAPACK_zptrfs(&uplo, &one, &one, &value, &off, &value, &off, &direct_rhs,
                  &one, &direct_x, &one, &direct_f, &direct_b, work.data(),
                  &real_work, &info);
  }
}
template <typename T>
void ExtremeCase(TestContext& test,
                 const asc::ReferenceLapackProvider& provider, Real<T> value,
                 asc::DenseBlasTriangle triangle) {
  using R = Real<T>;
  Fixture<T> fixture(1, 0, triangle);
  fixture.d[1] = value;
  fixture.df[1] = value;
  const auto original = fixture.Original();
  const auto factor = fixture.Factor(provider);
  Rhs<T> rhs(1, 1, kColumn);
  Rhs<T> solution(1, 1, kRow);
  rhs.At(0, 0) = Value<T>(value);
  solution.At(0, 0) = T{1};
  std::array<R, 3> ferr{-81, -81, -81};
  std::array<R, 3> berr{-83, -83, -83};
  const auto f = Vector(ferr, 1);
  const auto b = Vector(berr, 1);
  const auto plan = Take(asc::QueryPtrfsWorkspace(provider, original, factor,
                                                  std::as_const(rhs).View(),
                                                  solution.View(), f, b));
  Storage<T> storage;
  asc::LapackReport report;
  const auto status =
      asc::Ptrfs(provider, original, factor, std::as_const(rhs).View(),
                 solution.View(), f, b, plan, storage.Workspace(plan), report);
  lapack_int info = std::numeric_limits<lapack_int>::min();
  T direct_x{1};
  R direct_f = -81;
  R direct_b = -83;
  DirectScalar(value, triangle, direct_x, direct_f, direct_b, info);
  const R u = std::numeric_limits<R>::epsilon() / R{2};
  const R minimum = std::numeric_limits<R>::min();
  const bool guarded = value <= (R{2} * minimum) / u;
  const R expected_b =
      guarded ? R{1} / (R{1} + value / (R{2} * minimum)) : R{0};
  const R expected_f = R{8} * u + (guarded ? R{4} * (minimum / value) : R{0});
  const Wide x = ToWide(solution.At(0, 0));
  const Wide direct = ToWide(direct_x);
  std::printf(
      "PTRFS bytes=%zu triangle=%d a=%La X=(%La,%La) FERR=%La BERR=%La "
      "direct_X=(%La,%La) direct_FERR=%La direct_BERR=%La INFO=%lld "
      "direct_INFO=%lld "
      "status=%d outcome=%d expected_FERR=%La expected_BERR=%La\n",
      sizeof(T), static_cast<int>(triangle), static_cast<long double>(value),
      x.real(), x.imag(), static_cast<long double>(ferr[1]),
      static_cast<long double>(berr[1]), direct.real(), direct.imag(),
      static_cast<long double>(direct_f), static_cast<long double>(direct_b),
      static_cast<long long>(report.native_info.value_or(-99)),
      static_cast<long long>(info), static_cast<int>(status.code()),
      static_cast<int>(report.outcome), static_cast<long double>(expected_f),
      static_cast<long double>(expected_b));
  ASC_DENSE_TEST_CHECK(
      test, Same(x.real(), direct.real()) && Same(x.imag(), direct.imag()));
  ASC_DENSE_TEST_CHECK(test,
                       Same(ferr[1], direct_f) && Same(berr[1], direct_b));
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-99), info);
  ASC_DENSE_TEST_CHECK(
      test, std::abs(x - Wide{1}) <= 64 * std::numeric_limits<R>::epsilon());
  ASC_DENSE_TEST_CHECK(
      test, std::isfinite(ferr[1]) &&
                std::abs(ferr[1] - expected_f) <=
                    64 * std::numeric_limits<R>::epsilon() * expected_f);
  ASC_DENSE_TEST_CHECK(test, std::isfinite(berr[1]) &&
                                 std::abs(berr[1] - expected_b) <=
                                     64 * std::numeric_limits<R>::epsilon());
}
template <typename T>
int Scalar(const asc::ReferenceLapackProvider& provider, bool extreme) {
  TestContext test;
  if (extreme) {
    using R = Real<T>;
    for (R value : {std::numeric_limits<R>::denorm_min(),
                    R{2} * std::numeric_limits<R>::denorm_min(),
                    std::numeric_limits<R>::min() / R{8},
                    std::numeric_limits<R>::max()}) {
      for (const auto triangle : {kLower, kUpper}) {
        ExtremeCase<T>(test, provider, value, triangle);
      }
    }
  } else {
    Ordinary<T>(test, provider);
    Preflight<T>(test, provider);
    Counts(test);
    Concurrent<T>(test, provider);
  }
  return test.Finish();
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard normal_return;
  if (argc != 2 && argc != 3) {
    return 2;
  }
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const bool extreme = argc == 3 && std::string_view(argv[2]) == "extreme";
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    return Scalar<float>(provider, extreme);
  }
  if (scalar == "d") {
    return Scalar<double>(provider, extreme);
  }
  if (scalar == "c") {
    return Scalar<std::complex<float>>(provider, extreme);
  }
  if (scalar == "z") {
    return Scalar<std::complex<double>>(provider, extreme);
  }
  return 2;
}
