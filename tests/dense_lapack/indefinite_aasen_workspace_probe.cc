#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <limits>
#include <string_view>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "../dense/test_support.h"
#include "asc/dense/blas.h"
#include "indefinite_aasen_native.h"
#include "installed_lu/normal_return_guard.h"

namespace {
using asc_dense_test::TestContext;
constexpr lapack_int kGuard = std::numeric_limits<lapack_int>::max() - 73;

template <typename T>
void Probe(TestContext& test, bool he, char tri, lapack_int n, bool query,
           bool fidelity) {
  const bool complex_symmetric = asc::DenseBlasComplex<T> && !he;
  const lapack_int minimum =
      n <= 1 && !complex_symmetric ? 1 : std::max<lapack_int>(1, 2 * n);
  const lapack_int preferred = n <= 1 && !complex_symmetric ? 1 : 65 * n;
  std::array<T, 7> a;
  std::array<T, 132> work;
  std::array<lapack_int, 4> pivots;
  a.fill(T{-71});
  work.fill(T{-73});
  pivots.fill(kGuard);
  a[1] = T{4};
  a[2] = T{1};
  a[3] = T{1};
  a[4] = T{9};
  const auto before_a = a;
  const auto before_pivots = pivots;
  lapack_int info = std::numeric_limits<lapack_int>::min();
  const lapack_int entries = std::max(minimum, preferred);
  asc_aasen_test::Native(he, tri, n, a.data() + 1, 2, pivots.data() + 1,
                         work.data() + 1, query ? -1 : entries, info);
  ASC_DENSE_TEST_EQ(test, info, 0);
  if (fidelity) {
    ASC_DENSE_TEST_EQ(test, work[1],
                      T{static_cast<asc::DenseBlasRealType<T>>(preferred)});
  } else {
    // An optimal LWORK must satisfy the same documented native lower bound.
    // CSY/ZSY N=0 currently return zero, which fails this requirement.
    const auto optimal = std::real(work[1]);
    ASC_DENSE_TEST_CHECK(test, std::isfinite(optimal) && optimal >= minimum &&
                                   std::floor(optimal) == optimal &&
                                   std::imag(work[1]) == 0);
  }
  ASC_DENSE_TEST_EQ(test, work.front(), T{-73});
  for (std::size_t i = static_cast<std::size_t>(query ? 1 : entries) + 1;
       i < work.size(); ++i) {
    ASC_DENSE_TEST_EQ(test, work[i], T{-73});
  }
  ASC_DENSE_TEST_EQ(test, a.front(), before_a.front());
  ASC_DENSE_TEST_EQ(test, a.back(), before_a.back());
  ASC_DENSE_TEST_EQ(test, pivots.front(), kGuard);
  for (std::size_t i = static_cast<std::size_t>(query ? 0 : n) + 1;
       i < pivots.size(); ++i) {
    ASC_DENSE_TEST_EQ(test, pivots[i], kGuard);
  }
  if (query || n == 0) {
    ASC_DENSE_TEST_EQ(test, a, before_a);
    ASC_DENSE_TEST_EQ(test, pivots, before_pivots);
  }
  std::printf(
      "Aasen WORK real_bytes=%zu complex=%d he=%d tri=%c n=%lld query=%d "
      "minimum=%lld returned=%Lg mode=%s\n",
      sizeof(asc::DenseBlasRealType<T>),
      static_cast<int>(asc::DenseBlasComplex<T>), static_cast<int>(he), tri,
      static_cast<long long>(n), static_cast<int>(query),
      static_cast<long long>(minimum),
      static_cast<long double>(std::real(work[1])),
      fidelity ? "fidelity" : "contract");
}
template <typename T>
void Run(TestContext& test, bool he, bool fidelity, int& cases) {
  for (char tri : {'U', 'L'}) {
    for (lapack_int n : {0, 1, 2}) {
      for (bool query : {false, true}) {
        Probe<T>(test, he, tri, n, query, fidelity);
        ++cases;
      }
    }
  }
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 2 || (std::string_view(argv[1]) != "contract" &&
                    std::string_view(argv[1]) != "fidelity")) {
    return 2;
  }
  const bool fidelity = std::string_view(argv[1]) == "fidelity";
  TestContext test;
  int cases = 0;
  Run<float>(test, false, fidelity, cases);
  Run<double>(test, false, fidelity, cases);
  Run<std::complex<float>>(test, false, fidelity, cases);
  Run<std::complex<double>>(test, false, fidelity, cases);
  Run<std::complex<float>>(test, true, fidelity, cases);
  Run<std::complex<double>>(test, true, fidelity, cases);
  std::printf("Aasen native WORK cases=%d\n", cases);
  return test.Finish();
}
