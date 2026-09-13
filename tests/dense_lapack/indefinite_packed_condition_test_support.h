#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_CONDITION_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_CONDITION_TEST_SUPPORT_H_

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdlib>
#include <limits>

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite_packed_condition.h"
#include "indefinite_packed_fixture.h"
#include "indefinite_packed_test_support.h"
#include "indefinite_test_support.h"
#include "tests/dense/test_support.h"

namespace asc_packed_condition_test {
namespace base = asc_indefinite_test;
namespace factor = asc_packed_indefinite_test;
using base::EqualBytes;
using base::kColumn;
using base::kHost;
using base::kLower;
using base::kPivot;
using base::kRow;
using base::kScalar;
using base::kUpper;
inline constexpr auto kLayout = base::kLayout;
using base::Take;
using base::TestContext;
using base::ToWide;
using base::Value;
using base::Wide;

// Real SPCON needs simultaneous n IPIV and n IWORK entries. Retain two
// 16-byte guards around the largest admitted test case at true ILP64 width.
template <typename T>
struct Scratch {
  std::array<T, 4500> scalar{};
  std::array<T, 5200> packed{};
  alignas(16) std::array<std::byte, 2 * 67 * sizeof(asc::index_t) + 32> pivot{};
  Scratch() {
    scalar.fill(Value<T>(-107, 11));
    packed.fill(Value<T>(-109, 13));
    pivot.fill(std::byte{0x5a});
  }
  asc::LapackWorkspace Workspace(const asc::LapackWorkspacePlan& plan,
                                 asc::extent_t scalar_entries = -1) {
    asc::LapackWorkspace workspace;
    for (const auto role : {kScalar, kPivot, kLayout}) {
      const auto& requirement = plan.regions[role];
      const auto entries = role == kScalar && scalar_entries >= 0
                               ? scalar_entries
                               : requirement.preferred_entries;
      if (entries == 0) {
        continue;
      }
      const auto bytes =
          static_cast<std::size_t>(entries) * requirement.entry_bytes;
      if (role == kScalar) {
        if (bytes > (scalar.size() - 2) * sizeof(T)) {
          std::abort();
        }
        workspace.regions[role] = {scalar.data() + 1, bytes, kHost};
      } else if (role == kLayout) {
        if (bytes > (packed.size() - 2) * sizeof(T)) {
          std::abort();
        }
        workspace.regions[role] = {packed.data() + 1, bytes, kHost};
      } else {
        if (bytes > pivot.size() - 32) {
          std::abort();
        }
        workspace.regions[role] = {pivot.data() + 16, bytes, kHost};
      }
    }
    return workspace;
  }
  void Guards(TestContext& test, const asc::LapackWorkspace& workspace) const {
    ASC_DENSE_TEST_EQ(test, scalar.front(), Value<T>(-107, 11));
    ASC_DENSE_TEST_EQ(test, packed.front(), Value<T>(-109, 13));
    for (std::size_t i = 1 + workspace.regions[kScalar].size() / sizeof(T);
         i < scalar.size(); ++i) {
      ASC_DENSE_TEST_EQ(test, scalar[i], Value<T>(-107, 11));
    }
    for (std::size_t i = 1 + workspace.regions[kLayout].size() / sizeof(T);
         i < packed.size(); ++i) {
      ASC_DENSE_TEST_EQ(test, packed[i], Value<T>(-109, 13));
    }
    for (std::size_t i = 0; i < 16; ++i) {
      ASC_DENSE_TEST_EQ(test, pivot[i], std::byte{0x5a});
    }
    for (std::size_t i = 16 + workspace.regions[kPivot].size();
         i < pivot.size(); ++i) {
      ASC_DENSE_TEST_EQ(test, pivot[i], std::byte{0x5a});
    }
  }
};
template <typename T>
auto Query(const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasTriangle triangle, bool hermitian,
           asc::DenseBlasPackedMatrixView<const T> factors,
           asc::RawLapackPivotView pivots, asc::DenseBlasRealType<T> norm,
           const asc::DenseBlasRealType<T>& rcond) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::QueryHpconWorkspace(provider, triangle, factors, pivots, norm,
                                      rcond);
    }
  }
  return asc::QuerySpconWorkspace(provider, triangle, factors, pivots, norm,
                                  rcond);
}

template <typename T>
auto Condition(const asc::ReferenceLapackProvider& provider,
               asc::DenseBlasTriangle triangle, bool hermitian,
               asc::DenseBlasPackedMatrixView<const T> factors,
               asc::RawLapackPivotView pivots, asc::DenseBlasRealType<T> norm,
               asc::DenseBlasRealType<T>& rcond,
               const asc::LapackWorkspacePlan& plan,
               const asc::LapackWorkspace& workspace,
               asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::Hpcon(provider, triangle, factors, pivots, norm, rcond, plan,
                        workspace, report);
    }
  }
  return asc::Spcon(provider, triangle, factors, pivots, norm, rcond, plan,
                    workspace, report);
}

template <typename T>
struct Fixture {
  using Real = asc::DenseBlasRealType<T>;
  factor::Sample<T> a;
  Real norm{};
  long double expected = 1;
  Fixture(int n, bool he, asc::DenseBlasTriangle triangle,
          asc::DenseBlasLayout layout, int exponent, int kind)
      : a(n, he, triangle, layout, 0) {
    const long double scale = std::ldexp(1.0L, exponent);
    for (int j = 0; j < n; ++j) {
      for (int i = 0; i < n; ++i) {
        T value{};
        const bool paired =
            kind != 3 && i % 3 != 2 && (i % 3 == 1 || i + 1 < n);
        if (i == j && !paired) {
          const auto diagonal = (i % 2 == 0 ? 2 : -8) * scale;
          value = Value<T>(diagonal, he ? 0 : diagonal / 2);
        } else if (kind != 3 && std::min(i, j) % 3 == 0 &&
                   std::max(i, j) == std::min(i, j) + 1) {
          value = Value<T>(4 * scale, he && i < j ? -2 * scale : 2 * scale);
        }
        if (kind == 1 || (kind == 2 && (i == n / 2 || j == n / 2))) {
          value = T{};
        }
        a.full[i * n + j] = ToWide(value);
        if (a.Selected(i, j)) {
          a.a[a.Offset(i, j)] = value;
          if constexpr (asc::DenseBlasComplex<T>) {
            if (he && i == j) {
              a.a[a.Offset(i, j)].imag(std::numeric_limits<Real>::quiet_NaN());
            }
          }
        }
      }
    }
    a.original = a.a;
    Measure();
  }
  void Measure() {
    long double minimum = std::numeric_limits<long double>::infinity();
    long double maximum = 0;
    for (int j = 0; j < a.n; ++j) {
      long double column = 0;
      for (int i = 0; i < a.n; ++i) {
        column += std::abs(a.full[i * a.n + j]);
      }
      minimum = std::min(minimum, column);
      maximum = std::max(maximum, column);
    }
    norm = static_cast<Real>(maximum);
    // These independent monomial/block-diagonal fixtures have at most one
    // nonzero per column. Inverse one-norm is 1/minimum for nonsingular A.
    expected = maximum == 0 ? 0 : minimum / maximum;
    if (a.n == 0) {
      expected = 1;
    }
  }
  void Prepare(TestContext& test,
               const asc::ReferenceLapackProvider& provider) {
    const auto pivots = base::Pivots(a.pivots, a.n);
    const auto plan = Take(
        factor::Query(provider, a.triangle, a.hermitian, a.View(), pivots));
    base::Scratch<T> scratch;
    const auto workspace = scratch.Workspace(plan);
    asc::LapackReport report;
    const auto status =
        factor::Factor(provider, a.triangle, a.hermitian, a.View(), pivots,
                       plan, workspace, report);
    ASC_DENSE_TEST_CHECK(
        test, status.ok() || status.code() == asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_EQ(test, report.called_provider, a.n != 0);
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(0) == 0,
                      expected != 0 || a.n == 0);
    a.Reconstruction(test);
    a.Guards(test);
    scratch.Guards(test, workspace);
  }
  void Mathematics(TestContext& test, Real actual) const {
    ASC_DENSE_TEST_CHECK(test, std::isfinite(actual));
    if (expected == 0 || a.n == 0) {
      ASC_DENSE_TEST_EQ(test, actual, static_cast<Real>(expected));
    } else {
      ASC_DENSE_TEST_CHECK(
          test, std::abs(static_cast<long double>(actual) - expected) <=
                    128 * std::numeric_limits<Real>::epsilon() * expected);
    }
  }
};
}  // namespace asc_packed_condition_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_CONDITION_TEST_SUPPORT_H_
