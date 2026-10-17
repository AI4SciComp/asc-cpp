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
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_positive_tridiagonal.h"
#include "asc/dense/providers/lapack_positive_tridiagonal_driver.h"
#include "installed_lu/normal_return_guard.h"
#include "internal_positive_tridiagonal_counts.h"
#include "internal_tridiagonal.h"
#include "positive_tridiagonal_refinement_test_support.h"
#include "tridiagonal_test_support.h"

namespace {
using asc_ptrfs_test::Exact;
using asc_ptrfs_test::Fixture;
using asc_ptrfs_test::Initialize;
using asc_ptrfs_test::kLower;
using asc_tridiagonal_test::kColumn;
using asc_tridiagonal_test::kHost;
using asc_tridiagonal_test::kRow;
using asc_tridiagonal_test::Rhs;
using asc_tridiagonal_test::Scratch;
using asc_tridiagonal_test::Take;
using asc_tridiagonal_test::TestContext;
using asc_tridiagonal_test::ToWide;
using asc_tridiagonal_test::Vector;
using asc_tridiagonal_test::Wide;
using asc_tridiagonal_test::WithoutAllocation;
template <typename T>
using Real = asc::DenseBlasRealType<T>;

template <typename T>
auto Matrix(Fixture<T>& fixture) {
  return Take(asc::LapackPositiveDefiniteTridiagonalView<T>::Create(
      Vector(fixture.d, fixture.n),
      Vector(fixture.e, fixture.n == 0 ? 0 : fixture.n - 1)));
}

template <typename T>
void Solution(TestContext& test, const Rhs<T>& rhs) {
  for (asc::extent_t j = 0; j < rhs.columns; ++j) {
    for (asc::extent_t i = 0; i < rhs.rows; ++i) {
      const auto expected = ToWide(Exact<T>(i, j));
      ASC_DENSE_TEST_CHECK(test,
                           std::abs(ToWide(rhs.At(i, j)) - expected) <=
                               64 * std::numeric_limits<Real<T>>::epsilon() *
                                   std::abs(expected));
    }
  }
}

template <typename T>
void OrdinaryCase(TestContext& test,
                  const asc::ReferenceLapackProvider& provider, asc::extent_t n,
                  asc::extent_t nrhs, int exponent,
                  asc::DenseBlasLayout layout) {
  Fixture<T> fixture(n, exponent, kLower);
  Rhs<T> rhs(n, nrhs, layout);
  Rhs<T> unused(n, nrhs, layout);
  const auto plan = Take(WithoutAllocation(test, [&] {
    return asc::QueryPtsvWorkspace(provider, Matrix(fixture), rhs.View());
  }));
  Scratch<T> storage;
  const auto workspace = storage.Workspace(plan);
  for (int pass = 0; pass < 2; ++pass) {
    fixture = Fixture<T>(n, exponent + pass, kLower);
    Initialize(fixture, rhs, unused);
    const auto original = fixture;
    asc::LapackReport report;
    const auto status = WithoutAllocation(test, [&] {
      return asc::Ptsv(provider, Matrix(fixture), rhs.View(), plan, workspace,
                       report);
    });
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_EQ(test, report.called_provider, n > 0);
    ASC_DENSE_TEST_EQ(test, report.native_info.has_value(), n > 0);
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(0), 0);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kComplete);
    Solution(test, rhs);
    for (asc::extent_t i = 0; i < n; ++i) {
      const auto at = static_cast<std::size_t>(i + 1);
      ASC_DENSE_TEST_EQ(test, fixture.d[at], original.df[at]);
      if (i + 1 < n) {
        ASC_DENSE_TEST_EQ(test, fixture.e[at], original.ef[at]);
      }
    }
    const auto factors =
        Take(asc::ReferencePositiveDefiniteTridiagonalFactorView<T>::FromRaw(
            provider, kLower, Vector(std::as_const(fixture.d), n),
            Vector(std::as_const(fixture.e), n == 0 ? 0 : n - 1)));
    Initialize(original, rhs, unused);
    const auto reuse =
        Take(asc::QueryPttrsWorkspace(provider, factors, rhs.View()));
    Scratch<T> reuse_storage;
    ASC_DENSE_TEST_CHECK(test,
                         asc::Pttrs(provider, factors, rhs.View(), reuse,
                                    reuse_storage.Workspace(reuse), report)
                             .ok());
    Solution(test, rhs);
    rhs.Guards(test);
    storage.Guards(test, workspace);
    ASC_DENSE_TEST_EQ(test, fixture.d.front(), original.d.front());
    ASC_DENSE_TEST_EQ(test, fixture.e.front(), original.e.front());
    for (std::size_t i = static_cast<std::size_t>(n + 1); i < fixture.d.size();
         ++i) {
      ASC_DENSE_TEST_EQ(test, fixture.d[i], original.d[i]);
    }
    for (std::size_t i = static_cast<std::size_t>(n == 0 ? 1 : n);
         i < fixture.e.size(); ++i) {
      ASC_DENSE_TEST_EQ(test, fixture.e[i], original.e[i]);
    }
  }
}

template <typename T>
void Failures(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  for (const auto layout : {kColumn, kRow}) {
    for (const asc::extent_t nrhs : {0, 2}) {
      for (const std::size_t bad : {1, 3, 5}) {
        Fixture<T> fixture(5, 0, kLower);
        for (std::size_t i = 1; i <= 5; ++i) {
          fixture.d[i] = Real<T>{2};
          fixture.e[i] = T{};
        }
        fixture.d[bad] = Real<T>{-1};
        Rhs<T> rhs(5, nrhs, layout);
        const auto before = rhs.data;
        const auto plan = Take(
            asc::QueryPtsvWorkspace(provider, Matrix(fixture), rhs.View()));
        Scratch<T> storage;
        const auto workspace = storage.Workspace(plan);
        asc::LapackReport report;
        const auto status = asc::Ptsv(provider, Matrix(fixture), rhs.View(),
                                      plan, workspace, report);
        ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
        ASC_DENSE_TEST_CHECK(test, report.called_provider);
        ASC_DENSE_TEST_EQ(test, report.native_info.value_or(0),
                          static_cast<std::int64_t>(bad));
        ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-1),
                          static_cast<asc::extent_t>(bad - 1));
        ASC_DENSE_TEST_EQ(test, report.output_validity,
                          asc::LapackOutputValidity::kDocumentedPartial);
        ASC_DENSE_TEST_EQ(test, rhs.data, before);
        storage.Guards(test, workspace);
      }
    }
  }
}

template <typename T>
void Preflight(TestContext& test,
               const asc::ReferenceLapackProvider& provider) {
  for (const auto layout : {kColumn, kRow}) {
    Fixture<T> fixture(2, 0, kLower);
    fixture.d[1] = std::numeric_limits<Real<T>>::quiet_NaN();
    Rhs<T> rhs(2, 3, layout);
    const auto plan =
        Take(asc::QueryPtsvWorkspace(provider, Matrix(fixture), rhs.View()));
    Scratch<T> storage;
    auto workspace = storage.Workspace(plan);
    const auto before = rhs.data;
    asc::LapackReport report;
    constexpr auto kLayout = asc::internal_tridiagonal::kLayout;
    workspace.regions[kLayout] = {workspace.regions[kLayout].data(),
                                  workspace.regions[kLayout].size() - 1, kHost};
    const auto status = asc::Ptsv(provider, Matrix(fixture), rhs.View(), plan,
                                  workspace, report);
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
    ASC_DENSE_TEST_EQ(test, rhs.data, before);
    auto stale = plan;
    ++stale.regions[kLayout].minimum_entries;
    ASC_DENSE_TEST_EQ(test,
                      asc::Ptsv(provider, Matrix(fixture), rhs.View(), stale,
                                storage.Workspace(plan), report)
                          .code(),
                      asc::ErrorCode::kInvalidState);
  }
}

template <typename T>
int ConcurrentCalls(const asc::ReferenceLapackProvider& provider,
                    const asc::LapackWorkspacePlan& plan, std::size_t worker) {
  int failures = 0;
  const auto multiplier = static_cast<Real<T>>(worker + 1);
  for (int pass = 0; pass < 32; ++pass) {
    Fixture<T> fixture(5, 0, kLower);
    Rhs<T> rhs(5, 3, kRow);
    Rhs<T> unused(5, 3, kRow);
    Initialize(fixture, rhs, unused);
    for (asc::extent_t j = 0; j < 3; ++j) {
      for (asc::extent_t i = 0; i < 5; ++i) {
        rhs.At(i, j) *= multiplier;
      }
    }
    if (pass == 31) {
      fixture.e.fill(T{});
      fixture.d.fill(Real<T>{2});
      fixture.d[worker + 1] = Real<T>{-1};
    }
    const auto before = rhs.data;
    Scratch<T> storage;
    asc::LapackReport report;
    const auto status = asc::Ptsv(provider, Matrix(fixture), rhs.View(), plan,
                                  storage.Workspace(plan), report);
    if (pass == 31) {
      if (status.code() != asc::ErrorCode::kNumerical ||
          report.native_info.value_or(-1) !=
              static_cast<std::int64_t>(worker + 1) ||
          report.diagnostic_index.value_or(-1) !=
              static_cast<asc::extent_t>(worker) ||
          rhs.data != before) {
        ++failures;
      }
    } else {
      if (!status.ok() || !report.called_provider || report.native_info != 0 ||
          report.output_validity != asc::LapackOutputValidity::kComplete) {
        ++failures;
      }
      for (asc::extent_t j = 0; j < 3; ++j) {
        for (asc::extent_t i = 0; i < 5; ++i) {
          const auto expected =
              ToWide(Exact<T>(i, j)) * static_cast<long double>(multiplier);
          if (std::abs(ToWide(rhs.At(i, j)) - expected) >
              64 * std::numeric_limits<Real<T>>::epsilon() *
                  std::abs(expected)) {
            ++failures;
          }
        }
      }
    }
  }
  return failures;
}

template <typename T>
void Concurrent(TestContext& test,
                const asc::ReferenceLapackProvider& provider) {
  Fixture<T> fixture(5, 0, kLower);
  Rhs<T> rhs(5, 3, kRow);
  const auto plan =
      Take(asc::QueryPtsvWorkspace(provider, Matrix(fixture), rhs.View()));
  std::array<std::thread, 4> threads;
  std::array<int, 4> failures{};
  for (std::size_t i = 0; i < threads.size(); ++i) {
    threads[i] = std::thread([&, i] {
      const auto context = Take(asc::ReferenceLapackProvider::Create(
          asc::ExecutionContext::Serial()));
      failures[i] = ConcurrentCalls<T>(context, plan, i);
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
  for (const auto failed : failures) {
    ASC_DENSE_TEST_EQ(test, failed, 0);
  }
}

template <typename T>
void Aliases(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  Fixture<T> fixture(2, 0, kLower);
  std::array<T, 8> common{};
  const auto rhs = Take(asc::DenseBlasMatrixView<T>::Create(
      common.data() + 1, 2, 1, kColumn, 2,
      {common.data(), sizeof(common), kHost}));
  const auto matrix =
      Take(asc::LapackPositiveDefiniteTridiagonalView<T>::Create(
          Vector(fixture.d, 2), Vector(common, 1)));
  ASC_DENSE_TEST_EQ(
      test, asc::QueryPtsvWorkspace(provider, matrix, rhs).status().code(),
      asc::ErrorCode::kInvalidArgument);
  const auto independent = Matrix(fixture);
  const auto plan = Take(asc::QueryPtsvWorkspace(provider, independent, rhs));
  asc::LapackWorkspace workspace;
  workspace.regions[asc::internal_tridiagonal::kLayout] = {
      common.data() + 1, 2 * sizeof(T), kHost};
  asc::LapackReport report;
  ASC_DENSE_TEST_EQ(
      test,
      asc::Ptsv(provider, independent, rhs, plan, workspace, report).code(),
      asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
}

void Counts(TestContext& test) {
  namespace counts = asc::internal_positive_tridiagonal_counts;
  for (const asc::extent_t limit :
       {static_cast<asc::extent_t>(std::numeric_limits<std::int32_t>::max()),
        std::numeric_limits<asc::extent_t>::max()}) {
    ASC_DENSE_TEST_CHECK(test, counts::Solve(limit, 0, limit, limit).ok());
    ASC_DENSE_TEST_CHECK(test, !counts::Solve(limit, 1, limit, limit).ok());
    ASC_DENSE_TEST_CHECK(test, !counts::Solve(1, limit, 1, limit).ok());
    ASC_DENSE_TEST_CHECK(test, counts::Solve(1, limit - 1, 1, limit).ok());
  }
}

template <typename T>
lapack_int Direct(Real<T>& d, T& x) {
  const lapack_int n = 1;
  T e{};
  lapack_int info = std::numeric_limits<lapack_int>::min();
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_sptsv(&n, &n, &d, &e, &x, &n, &info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dptsv(&n, &n, &d, &e, &x, &n, &info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    LAPACK_cptsv(&n, &n, &d, &e, &x, &n, &info);
  } else {
    LAPACK_zptsv(&n, &n, &d, &e, &x, &n, &info);
  }
  return info;
}

template <typename T>
void Extreme(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  using R = Real<T>;
  for (const R a :
       {std::numeric_limits<R>::denorm_min(),
        2 * std::numeric_limits<R>::denorm_min(),
        std::numeric_limits<R>::min() / 8, std::numeric_limits<R>::max()}) {
    for (const auto layout : {kColumn, kRow}) {
      Fixture<T> fixture(1, 0, kLower);
      fixture.d[1] = a;
      Rhs<T> rhs(1, 1, layout);
      rhs.At(0, 0) = T{a};
      const auto plan =
          Take(asc::QueryPtsvWorkspace(provider, Matrix(fixture), rhs.View()));
      Scratch<T> storage;
      asc::LapackReport report;
      const auto status = asc::Ptsv(provider, Matrix(fixture), rhs.View(), plan,
                                    storage.Workspace(plan), report);
      R direct_d = a;
      T direct_x{a};
      const auto info = Direct<T>(direct_d, direct_x);
      const auto x = ToWide(rhs.At(0, 0));
      const auto direct = ToWide(direct_x);
      std::printf(
          "PTSV bytes=%zu layout=%d a=%La X=(%La,%La) direct=(%La,%La) "
          "INFO=%lld direct_INFO=%lld status=%d outcome=%d\n",
          sizeof(T), static_cast<int>(layout), static_cast<long double>(a),
          x.real(), x.imag(), direct.real(), direct.imag(),
          static_cast<long long>(report.native_info.value_or(-99)),
          static_cast<long long>(info), static_cast<int>(status.code()),
          static_cast<int>(report.outcome));
      for (const auto pair : {std::array{x.real(), direct.real()},
                              std::array{x.imag(), direct.imag()}}) {
        ASC_DENSE_TEST_CHECK(test, pair[0] == pair[1] || (std::isnan(pair[0]) &&
                                                          std::isnan(pair[1])));
      }
      ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-99), info);
      ASC_DENSE_TEST_EQ(test, info, 0);
      ASC_DENSE_TEST_CHECK(test, std::abs(x - Wide{1}) <=
                                     64 * std::numeric_limits<R>::epsilon());
    }
  }
}

template <typename T>
void Run(TestContext& test, bool extreme) {
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  if (extreme) {
    Extreme<T>(test, provider);
    return;
  }
  for (const asc::extent_t n : {0, 1, 2, 5, 9}) {
    for (const asc::extent_t nrhs : {0, 1, 3}) {
      for (const int exponent : {-8, 0, 8}) {
        for (const auto layout : {kColumn, kRow}) {
          OrdinaryCase<T>(test, provider, n, nrhs, exponent, layout);
        }
      }
    }
  }
  Failures<T>(test, provider);
  Preflight<T>(test, provider);
  Aliases<T>(test, provider);
  Counts(test);
  Concurrent<T>(test, provider);
}
}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard normal_return;
  TestContext test;
  const std::string_view scalar = argc > 1 ? argv[1] : "";
  const bool extreme = argc > 2;
  if (scalar == "s") {
    Run<float>(test, extreme);
  } else if (scalar == "d") {
    Run<double>(test, extreme);
  } else if (scalar == "c") {
    Run<std::complex<float>>(test, extreme);
  } else if (scalar == "z") {
    Run<std::complex<double>>(test, extreme);
  } else {
    return 2;
  }
  return test.Finish();
}
