#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <string_view>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_rook_condition_native.h"
#include "indefinite_rook_condition_test_support.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
namespace base = asc_indefinite_rook_test;
using asc_rook_condition_test::Condition;
using asc_rook_condition_test::Native;
using asc_rook_condition_test::Query;
using base::TestContext;

template <typename T>
struct Sample {
  using Real = asc::DenseBlasRealType<T>;
  int n;
  asc::DenseBlasTriangle triangle;
  asc::DenseBlasLayout layout;
  std::array<T, 90> a{};
  std::array<asc::index_t, 10> pivots{};
  Sample(int order, asc::DenseBlasTriangle tri, asc::DenseBlasLayout storage,
         Real scale)
      : n(order), triangle(tri), layout(storage) {
    a.fill(base::Value<T>(-109, 13));
    pivots.fill(-127);
    for (int j = 0; j < n; ++j) {
      for (int i = 0; i < n; ++i) {
        if ((triangle == base::kUpper && i <= j) ||
            (triangle == base::kLower && i >= j)) {
          const int index =
              layout == base::kColumn ? j * (n + 2) + i : i * (n + 2) + j;
          a[1U + static_cast<std::size_t>(index)] = i == j ? T{scale} : T{};
        }
      }
    }
  }
};

template <typename T>
void Fidelity(TestContext& test, bool hermitian, int order,
              asc::DenseBlasTriangle triangle, asc::DenseBlasRealType<T> scale,
              asc::DenseBlasRealType<T> condition) {
  using Real = asc::DenseBlasRealType<T>;
  std::array<T, 49> native_a{};
  std::array<T, 14> work{};
  std::array<lapack_int, 7> ipiv{};
  std::array<lapack_int, 7> iwork{};
  for (int i = 0; i < order; ++i) {
    native_a[static_cast<std::size_t>(i) *
             static_cast<std::size_t>(order + 1)] = T{scale};
    ipiv[static_cast<std::size_t>(i)] = i + 1;
  }
  lapack_int n = order;
  lapack_int lda = order;
  lapack_int info = std::numeric_limits<lapack_int>::min();
  Real native_condition = -17;
  char uplo = triangle == base::kUpper ? 'U' : 'L';
  Native(hermitian, &uplo, &n, native_a.data(), &lda, ipiv.data(), &scale,
         &native_condition, work.data(), iwork.data(), &info);
  ASC_DENSE_TEST_EQ(test, info, 0);
  ASC_DENSE_TEST_CHECK(
      test, condition == native_condition ||
                (std::isnan(condition) && std::isnan(native_condition)));
}

template <typename T>
void IdentityFactor(TestContext& test, const Sample<T>& sample,
                    asc::DenseBlasRealType<T> scale) {
  for (int j = 0; j < sample.n; ++j) {
    ASC_DENSE_TEST_EQ(test, sample.pivots[1U + static_cast<std::size_t>(j)],
                      j + 1);
    for (int i = 0; i < sample.n; ++i) {
      if ((sample.triangle == base::kUpper && i <= j) ||
          (sample.triangle == base::kLower && i >= j)) {
        const int offset = sample.layout == base::kColumn
                               ? j * (sample.n + 2) + i
                               : i * (sample.n + 2) + j;
        const T actual = sample.a[1U + static_cast<std::size_t>(offset)];
        ASC_DENSE_TEST_EQ(test, actual, i == j ? T{scale} : T{});
      }
    }
  }
}

template <typename T>
void Check(TestContext& test, const asc::ReferenceLapackProvider& provider,
           Sample<T>& sample, bool hermitian, bool blocked,
           asc::DenseBlasRealType<T> scale, bool fidelity) {
  using Real = asc::DenseBlasRealType<T>;
  auto matrix =
      base::Matrix(sample.a, sample.n, sample.n, sample.layout, sample.n + 2);
  auto pivots = base::Pivots(sample.pivots, sample.n);
  base::Scratch<T> scratch;
  asc::LapackReport report;
  const auto factor_plan = base::Take(base::QueryFactor(
      provider, sample.triangle, hermitian, blocked, matrix, pivots));
  auto workspace = scratch.Workspace(factor_plan);
  ASC_DENSE_TEST_CHECK(
      test, base::Factor(provider, sample.triangle, hermitian, blocked, matrix,
                         pivots, factor_plan, workspace, report)
                .ok());
  IdentityFactor(test, sample, scale);
  const auto saved_a = sample.a;
  const auto saved_pivots = sample.pivots;
  const auto immutable =
      base::Matrix(saved_a, sample.n, sample.n, sample.layout, sample.n + 2);
  const auto raw = base::Raw(sample.pivots, sample.n);
  Real condition = -13;
  const auto plan = base::Take(Query(provider, sample.triangle, hermitian,
                                     immutable, raw, scale, condition));
  scratch = base::Scratch<T>{};
  workspace = scratch.Workspace(plan);
  const auto status = base::WithoutAllocation(test, [&] {
    return Condition(provider, sample.triangle, hermitian, immutable, raw,
                     scale, condition, plan, workspace, report);
  });
  scratch.Guards(test, workspace);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), 0);
  ASC_DENSE_TEST_EQ(test, status.ok(), std::isfinite(condition));
  ASC_DENSE_TEST_CHECK(
      test, base::EqualBytes(sample.a.data(), saved_a.data(), sizeof(saved_a)));
  ASC_DENSE_TEST_CHECK(
      test, base::EqualBytes(sample.pivots.data(), saved_pivots.data(),
                             sizeof(saved_pivots)));
  if (fidelity) {
    Fidelity<T>(test, hermitian, sample.n, sample.triangle, scale, condition);
  } else {
    ASC_DENSE_TEST_CHECK(test, std::isfinite(condition));
    ASC_DENSE_TEST_CHECK(test, std::abs(condition - Real{1}) <=
                                   128 * std::numeric_limits<Real>::epsilon());
  }
  std::printf(
      "n=%d triangle=%d layout=%d blocked=%d scale=%La rcond=%La info=%lld\n",
      sample.n, static_cast<int>(sample.triangle),
      static_cast<int>(sample.layout), static_cast<int>(blocked),
      static_cast<long double>(scale), static_cast<long double>(condition),
      static_cast<long long>(report.native_info.value_or(-1)));
}

template <typename T>
int Run(bool hermitian, bool fidelity) {
  using Real = asc::DenseBlasRealType<T>;
  TestContext test;
  const auto provider = base::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int cases = 0;
  for (const auto scale : {2 * std::numeric_limits<Real>::min(),
                           std::numeric_limits<Real>::min() / 1024,
                           2 * std::numeric_limits<Real>::denorm_min(),
                           std::numeric_limits<Real>::max()}) {
    for (const int n : {1, 7}) {
      for (const auto triangle : {base::kUpper, base::kLower}) {
        for (const auto layout : {base::kColumn, base::kRow}) {
          for (const bool blocked : {false, true}) {
            Sample<T> sample(n, triangle, layout, scale);
            Check(test, provider, sample, hermitian, blocked, scale, fidelity);
            ++cases;
          }
        }
      }
    }
  }
  std::printf("range cases=%d fidelity=%d\n", cases,
              static_cast<int>(fidelity));
  return test.Finish();
}
}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 3) {
    return 2;
  }
  const std::string_view mode(argv[2]);
  if (mode != "mathematical" && mode != "fidelity") {
    return 2;
  }
  const bool fidelity = mode == "fidelity";
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    return Run<float>(false, fidelity);
  }
  if (scalar == "d") {
    return Run<double>(false, fidelity);
  }
  if (scalar == "c") {
    return Run<std::complex<float>>(false, fidelity);
  }
  if (scalar == "z") {
    return Run<std::complex<double>>(false, fidelity);
  }
  if (scalar == "ch") {
    return Run<std::complex<float>>(true, fidelity);
  }
  if (scalar == "zh") {
    return Run<std::complex<double>>(true, fidelity);
  }
  return 2;
}
