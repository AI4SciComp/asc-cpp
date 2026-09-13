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
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_positive_tridiagonal.h"
#include "asc/dense/providers/lapack_positive_tridiagonal_condition.h"
#include "installed_lu/normal_return_guard.h"
#include "internal_positive_tridiagonal_condition_counts.h"
#include "internal_tridiagonal.h"
#include "tridiagonal_test_support.h"

namespace {
using asc_tridiagonal_test::kHost;
using asc_tridiagonal_test::kReal;
using asc_tridiagonal_test::Scratch;
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
template <typename T>
using Factor = asc::ReferencePositiveDefiniteTridiagonalFactorView<T>;

// Independent dense inversion is deliberately bounded to ordinary systems.
// It neither calls a LAPACK solve nor repeats PTCON's comparison recurrence.
using Dense = std::array<std::array<Wide, 9>, 9>;
long double Norm(const Dense& matrix, std::size_t n) {
  long double result = 0;
  for (std::size_t j = 0; j < n; ++j) {
    long double sum = 0;
    for (std::size_t i = 0; i < n; ++i) {
      sum += std::abs(matrix[i][j]);
    }
    result = std::max(result, sum);
  }
  return result;
}
long double ReciprocalCondition(Dense matrix, std::size_t n) {
  if (n == 0) {
    return 1;
  }
  const auto norm = Norm(matrix, n);
  Dense inverse{};
  for (std::size_t i = 0; i < n; ++i) {
    inverse[i][i] = 1;
  }
  for (std::size_t k = 0; k < n; ++k) {
    std::size_t pivot = k;
    for (std::size_t i = k + 1; i < n; ++i) {
      if (std::abs(matrix[i][k]) > std::abs(matrix[pivot][k])) {
        pivot = i;
      }
    }
    std::swap(matrix[k], matrix[pivot]);
    std::swap(inverse[k], inverse[pivot]);
    const auto diagonal = matrix[k][k];
    for (std::size_t j = 0; j < n; ++j) {
      matrix[k][j] /= diagonal;
      inverse[k][j] /= diagonal;
    }
    for (std::size_t i = 0; i < n; ++i) {
      if (i == k) {
        continue;
      }
      const auto multiplier = matrix[i][k];
      for (std::size_t j = 0; j < n; ++j) {
        matrix[i][j] -= multiplier * matrix[k][j];
        inverse[i][j] -= multiplier * inverse[k][j];
      }
    }
  }
  return 1 / (norm * Norm(inverse, n));
}
template <typename T>
struct Fixture {
  std::array<Real<T>, 11> d{};
  std::array<T, 11> e{};
  asc::extent_t n;
  Dense matrix{};
  Fixture(asc::extent_t order, int exponent, asc::DenseBlasTriangle triangle)
      : n(order) {
    d.fill(Real<T>{-83});
    e.fill(Value<T>(-85, 7));
    const auto scale = std::ldexp(1.0L, exponent);
    for (asc::extent_t i = 0; i < n; ++i) {
      const auto at = static_cast<std::size_t>(i + 1);
      d[at] = static_cast<Real<T>>(scale * (2 + i % 3));
      if (i + 1 < n) {
        const auto lower = Value<T>(i % 2 == 0 ? 0.25L : -0.5L, 0.125L);
        if constexpr (asc::DenseBlasComplex<T>) {
          e[at] = triangle == kLower ? lower : std::conj(lower);
        } else {
          e[at] = lower;
        }
      }
    }
    // Build L*D*L^H explicitly. Upper E is the documented conjugate encoding.
    Dense lower{};
    for (std::size_t i = 0; i < static_cast<std::size_t>(n); ++i) {
      lower[i][i] = 1;
      if (i > 0) {
        lower[i][i - 1] =
            triangle == kLower ? ToWide(e[i]) : std::conj(ToWide(e[i]));
      }
    }
    for (std::size_t i = 0; i < static_cast<std::size_t>(n); ++i) {
      for (std::size_t j = 0; j < static_cast<std::size_t>(n); ++j) {
        for (std::size_t k = 0; k < static_cast<std::size_t>(n); ++k) {
          matrix[i][j] += lower[i][k] * static_cast<long double>(d[k + 1]) *
                          std::conj(lower[j][k]);
        }
      }
    }
  }
  [[nodiscard]] auto View(const asc::ReferenceLapackProvider& provider,
                          asc::DenseBlasTriangle triangle) const {
    return Take(Factor<T>::FromRaw(provider, triangle, Vector(d, n),
                                   Vector(e, n == 0 ? 0 : n - 1)));
  }
};

template <typename T>
void Ordinary(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  for (const auto triangle : {kLower, kUpper}) {
    for (const asc::extent_t n : {0, 1, 2, 5, 9}) {
      for (const int exponent : {-8, 0, 8}) {
        const Fixture<T> fixture(n, exponent, triangle);
        const auto factor = fixture.View(provider, triangle);
        const Real<T> norm = static_cast<Real<T>>(
            Norm(fixture.matrix, static_cast<std::size_t>(n)));
        const auto expected =
            ReciprocalCondition(fixture.matrix, static_cast<std::size_t>(n));
        Real<T> output = -19;
        const auto plan = Take(WithoutAllocation(test, [&] {
          return asc::QueryPtconWorkspace(provider, factor, norm, output);
        }));
        Scratch<T> scratch;
        const auto workspace = scratch.Workspace(plan);
        asc::LapackReport report;
        const auto status = WithoutAllocation(test, [&] {
          return asc::Ptcon(provider, factor, norm, output, plan, workspace,
                            report);
        });
        ASC_DENSE_TEST_CHECK(test, status.ok());
        ASC_DENSE_TEST_CHECK(test, report.called_provider);
        ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-99), 0);
        ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
        ASC_DENSE_TEST_EQ(test, report.output_validity,
                          asc::LapackOutputValidity::kComplete);
        ASC_DENSE_TEST_CHECK(
            test, std::abs(output - expected) <=
                      256 * std::numeric_limits<Real<T>>::epsilon() * expected);
        scratch.Guards(test, workspace);
        // Nonzero norm magnitude is not a plan dependency; scaling the caller's
        // supplied norm alone must change the raw estimate reciprocally.
        if (n > 0) {
          const auto reused = asc::Ptcon(provider, factor, Real<T>{2} * norm,
                                         output, plan, workspace, report);
          ASC_DENSE_TEST_CHECK(test, reused.ok());
          ASC_DENSE_TEST_CHECK(
              test,
              std::abs(output - expected / 2) <=
                  256 * std::numeric_limits<Real<T>>::epsilon() * expected);
        }
      }
    }
  }
}

void Counts(TestContext& test) {
  for (const asc::extent_t limit :
       {static_cast<asc::extent_t>(std::numeric_limits<std::int32_t>::max()),
        std::numeric_limits<asc::extent_t>::max()}) {
    namespace counts = asc::internal_positive_tridiagonal_condition_counts;
    ASC_DENSE_TEST_CHECK(test, counts::Condition(limit - 1, true, limit).ok());
    ASC_DENSE_TEST_EQ(test, counts::Condition(limit, true, limit).code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_CHECK(test, counts::Condition(limit, false, limit).ok());
    ASC_DENSE_TEST_CHECK(test, counts::Condition(0, true, limit).ok());
    ASC_DENSE_TEST_EQ(test, counts::Condition(-1, false, limit).code(),
                      asc::ErrorCode::kOverflow);
  }
}

template <typename T>
void InvalidNorm(TestContext& test,
                 const asc::ReferenceLapackProvider& provider, Factor<T> factor,
                 const Real<T>& output) {
  for (const Real<T> invalid :
       {Real<T>{-1}, std::numeric_limits<Real<T>>::infinity(),
        std::numeric_limits<Real<T>>::quiet_NaN()}) {
    ASC_DENSE_TEST_EQ(
        test,
        asc::QueryPtconWorkspace(provider, factor, invalid, output)
            .status()
            .code(),
        asc::ErrorCode::kInvalidArgument);
  }
}

template <typename T>
void Preflight(TestContext& test,
               const asc::ReferenceLapackProvider& provider) {
  Fixture<T> fixture(2, 0, kLower);
  const auto factor = fixture.View(provider, kLower);
  Real<T> output = -19;
  const Real<T> norm = 4;
  const auto plan =
      Take(asc::QueryPtconWorkspace(provider, factor, norm, output));
  Scratch<T> scratch;
  const auto original_work = scratch.real;
  const auto workspace = scratch.Workspace(plan);
  auto short_workspace = workspace;
  short_workspace.regions[kReal] = {workspace.regions[kReal].data(),
                                    workspace.regions[kReal].size() - 1, kHost};
  asc::LapackReport report;
  const auto status =
      asc::Ptcon(provider, factor, norm, output, plan, short_workspace, report);
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, output, Real<T>{-19});
  ASC_DENSE_TEST_CHECK(
      test, !report.called_provider && !report.native_info.has_value());
  const auto stale =
      asc::Ptcon(provider, factor, Real<T>{0}, output, plan, workspace, report);
  ASC_DENSE_TEST_EQ(test, stale.code(), asc::ErrorCode::kInvalidState);
  ASC_DENSE_TEST_EQ(test, output, Real<T>{-19});
  InvalidNorm<T>(test, provider, factor, output);
  for (const Real<T> invalid :
       {Real<T>{0}, Real<T>{-1}, std::numeric_limits<Real<T>>::infinity(),
        std::numeric_limits<Real<T>>::quiet_NaN()}) {
    fixture.d[1] = invalid;
    ASC_DENSE_TEST_CHECK(
        test, asc::QueryPtconWorkspace(provider, factor, norm, output).ok());
    ASC_DENSE_TEST_EQ(
        test,
        asc::Ptcon(provider, factor, norm, output, plan, workspace, report)
            .code(),
        asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_EQ(test, output, Real<T>{-19});
    ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-99), 0);
    ASC_DENSE_TEST_CHECK(test, !report.called_provider);
  }
  fixture.d[1] = Real<T>{2};
  for (const auto invalid : {std::numeric_limits<Real<T>>::infinity(),
                             std::numeric_limits<Real<T>>::quiet_NaN()}) {
    fixture.e[1] = Value<T>(invalid);
    ASC_DENSE_TEST_CHECK(
        test, asc::QueryPtconWorkspace(provider, factor, norm, output).ok());
    ASC_DENSE_TEST_EQ(
        test,
        asc::Ptcon(provider, factor, norm, output, plan, workspace, report)
            .code(),
        asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_EQ(test, output, Real<T>{-19});
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      asc::LapackOutcome::kAccuracyWarning);
    ASC_DENSE_TEST_CHECK(test, !report.called_provider);
  }
  ASC_DENSE_TEST_CHECK(
      test,
      !asc::QueryPtconWorkspace(provider, factor, norm, fixture.d[1]).ok());
  auto alias_workspace = workspace;
  alias_workspace.regions[kReal] = {fixture.d.data() + 1,
                                    workspace.regions[kReal].size(), kHost};
  ASC_DENSE_TEST_CHECK(test, !asc::Ptcon(provider, factor, norm, output, plan,
                                         alias_workspace, report)
                                  .ok());
  ASC_DENSE_TEST_CHECK(test, !report.called_provider);
  ASC_DENSE_TEST_EQ(test, output, Real<T>{-19});
  ASC_DENSE_TEST_CHECK(test, scratch.real == original_work);
  // Zero-norm quick return must not inspect even invalid old factors.
  fixture.d[1] = std::numeric_limits<Real<T>>::quiet_NaN();
  fixture.e[1] = Value<T>(std::numeric_limits<Real<T>>::quiet_NaN());
  const auto quick =
      Take(asc::QueryPtconWorkspace(provider, factor, Real<T>{0}, output));
  ASC_DENSE_TEST_CHECK(
      test,
      asc::Ptcon(provider, factor, Real<T>{0}, output, quick, {}, report).ok());
  ASC_DENSE_TEST_EQ(test, output, Real<T>{0});
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-99), 0);
}

template <typename T>
void Concurrent(TestContext& test,
                const asc::ReferenceLapackProvider& provider) {
  const Fixture<T> fixture(5, 0, kLower);
  const auto factor = fixture.View(provider, kLower);
  const Real<T> norm = static_cast<Real<T>>(Norm(fixture.matrix, 5));
  const auto expected = ReciprocalCondition(fixture.matrix, 5);
  Real<T> sample = -19;
  const auto plan =
      Take(asc::QueryPtconWorkspace(provider, factor, norm, sample));
  std::array<std::thread, 4> threads;
  std::array<int, 4> failures{};
  for (std::size_t i = 0; i < threads.size(); ++i) {
    threads[i] = std::thread([&, i] {
      const auto context = Take(asc::ReferenceLapackProvider::Create(
          asc::ExecutionContext::Serial()));
      Scratch<T> scratch;
      const auto workspace = scratch.Workspace(plan);
      Real<T> output = -19;
      asc::LapackReport report;
      for (int pass = 0; pass < 32; ++pass) {
        const Real<T> multiplier = static_cast<Real<T>>(i + 1);
        const auto status = asc::Ptcon(context, factor, multiplier * norm,
                                       output, plan, workspace, report);
        if (!status.ok() || !report.called_provider ||
            report.native_info.value_or(-99) != 0 ||
            std::abs(output - expected / multiplier) >
                256 * std::numeric_limits<Real<T>>::epsilon() * expected) {
          ++failures[i];
        }
      }
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
  for (auto failed : failures) {
    ASC_DENSE_TEST_EQ(test, failed, 0);
  }
}

template <typename T>
void Extreme(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  using R = Real<T>;
  // Exact scalar condition is one for every positive finite D=ANORM, without
  // needing a wide reciprocal oracle or a reconstructed square-root factor.
  for (const R value :
       {std::numeric_limits<R>::denorm_min(),
        R{2} * std::numeric_limits<R>::denorm_min(),
        std::numeric_limits<R>::min() / R{8}, std::numeric_limits<R>::max()}) {
    for (const auto triangle : {kLower, kUpper}) {
      Fixture<T> fixture(1, 0, triangle);
      fixture.d[1] = value;
      const auto factor = fixture.View(provider, triangle);
      R output = -19;
      const auto plan =
          Take(asc::QueryPtconWorkspace(provider, factor, value, output));
      Scratch<T> scratch;
      asc::LapackReport report;
      const auto status = asc::Ptcon(provider, factor, value, output, plan,
                                     scratch.Workspace(plan), report);
      R direct = -19;
      R work = 0;
      T off{};
      lapack_int n = 1;
      lapack_int info = std::numeric_limits<lapack_int>::min();
      if constexpr (std::is_same_v<T, float>) {
        LAPACK_sptcon(&n, &value, &off, &value, &direct, &work, &info);
      } else if constexpr (std::is_same_v<T, double>) {
        LAPACK_dptcon(&n, &value, &off, &value, &direct, &work, &info);
      } else if constexpr (std::is_same_v<T, std::complex<float>>) {
        LAPACK_cptcon(&n, &value, &off, &value, &direct, &work, &info);
      } else {
        LAPACK_zptcon(&n, &value, &off, &value, &direct, &work, &info);
      }
      std::printf(
          "PTCON bytes=%zu triangle=%d d=anorm=%La rcond=%La direct=%La "
          "INFO=%lld direct_INFO=%lld status=%d outcome=%d\n",
          sizeof(T), static_cast<int>(triangle),
          static_cast<long double>(value), static_cast<long double>(output),
          static_cast<long double>(direct),
          static_cast<long long>(report.native_info.value_or(-99)),
          static_cast<long long>(info), static_cast<int>(status.code()),
          static_cast<int>(report.outcome));
      ASC_DENSE_TEST_EQ(test, output, direct);
      ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-99), info);
      ASC_DENSE_TEST_CHECK(test,
                           std::isfinite(output) &&
                               std::abs(output - R{1}) <=
                                   R{16} * std::numeric_limits<R>::epsilon());
    }
  }
}
template <typename T>
int Scalar(const asc::ReferenceLapackProvider& provider, bool extreme) {
  TestContext test;
  if (extreme) {
    Extreme<T>(test, provider);
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
