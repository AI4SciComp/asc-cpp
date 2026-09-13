#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <limits>
#include <string_view>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "../dense/test_support.h"
#include "asc/dense/blas.h"
#include "indefinite_aasen_driver_native.h"
#include "indefinite_aasen_solve_fixture.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"
#if defined(ASC_AASEN_DRIVER_SSYSV_AA_HEADER)
#define ssysv_aa_ emitted_ssysv_aa
#include ASC_AASEN_DRIVER_SSYSV_AA_HEADER
#undef ssysv_aa_
#endif
#if defined(ASC_AASEN_DRIVER_DSYSV_AA_HEADER)
#define dsysv_aa_ emitted_dsysv_aa
#include ASC_AASEN_DRIVER_DSYSV_AA_HEADER
#undef dsysv_aa_
#endif
#if defined(ASC_AASEN_DRIVER_CSYSV_AA_HEADER)
#define csysv_aa_ emitted_csysv_aa
#include ASC_AASEN_DRIVER_CSYSV_AA_HEADER
#undef csysv_aa_
#endif
#if defined(ASC_AASEN_DRIVER_ZSYSV_AA_HEADER)
#define zsysv_aa_ emitted_zsysv_aa
#include ASC_AASEN_DRIVER_ZSYSV_AA_HEADER
#undef zsysv_aa_
#endif
#if defined(ASC_AASEN_DRIVER_CHESV_AA_HEADER)
#define chesv_aa_ emitted_chesv_aa
#include ASC_AASEN_DRIVER_CHESV_AA_HEADER
#undef chesv_aa_
#endif
#if defined(ASC_AASEN_DRIVER_ZHESV_AA_HEADER)
#define zhesv_aa_ emitted_zhesv_aa
#include ASC_AASEN_DRIVER_ZHESV_AA_HEADER
#undef zhesv_aa_
#endif
namespace {
#if defined(ASC_AASEN_DRIVER_SSYSV_AA_HEADER)
template <typename T>
struct Arg {
  using Type = T;
};
template <typename T>
struct Arg<T*> {
  using Type = std::remove_const_t<T>*;
};
template <typename T>
struct Sig;
template <typename R, typename... A>
struct Sig<R(A...)> {
  using Type = R(typename Arg<A>::Type...);
};
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_ssysv_aa_base)>::Type,
                             typename Sig<decltype(emitted_ssysv_aa)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_dsysv_aa_base)>::Type,
                             typename Sig<decltype(emitted_dsysv_aa)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_csysv_aa_base)>::Type,
                             typename Sig<decltype(emitted_csysv_aa)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_zsysv_aa_base)>::Type,
                             typename Sig<decltype(emitted_zsysv_aa)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_chesv_aa_base)>::Type,
                             typename Sig<decltype(emitted_chesv_aa)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_zhesv_aa_base)>::Type,
                             typename Sig<decltype(emitted_zhesv_aa)>::Type>);
#endif
namespace base = asc_indefinite_rook_test;
namespace aa = asc_aasen_solve_test;
constexpr lapack_int kGuard = std::numeric_limits<lapack_int>::max() - 73;
template <typename T>
lapack_int GuardedCall(base::TestContext& test, bool he, char triangle,
                       lapack_int order, lapack_int columns, T* a,
                       lapack_int leading, lapack_int* pivots, T* b,
                       lapack_int rhs_leading, T* work, lapack_int entries) {
  std::array<char, 3> uplo{'a', triangle, 'z'};
  std::array<lapack_int, 3> n{kGuard, order, kGuard};
  std::array<lapack_int, 3> nrhs{kGuard, columns, kGuard};
  std::array<lapack_int, 3> lda{kGuard, leading, kGuard};
  std::array<lapack_int, 3> ldb{kGuard, rhs_leading, kGuard};
  std::array<lapack_int, 3> lwork{kGuard, entries, kGuard};
  std::array<lapack_int, 3> info{kGuard, std::numeric_limits<lapack_int>::min(),
                                 kGuard};
  const auto before_uplo = uplo;
  const auto before_n = n;
  const auto before_nrhs = nrhs;
  const auto before_lda = lda;
  const auto before_ldb = ldb;
  const auto before_lwork = lwork;
  asc_aasen_driver_test::NativePointers(he, &uplo[1], &n[1], &nrhs[1], a,
                                        &lda[1], pivots, b, &ldb[1], work,
                                        &lwork[1], &info[1]);
  ASC_DENSE_TEST_EQ(test, uplo, before_uplo);
  ASC_DENSE_TEST_EQ(test, n, before_n);
  ASC_DENSE_TEST_EQ(test, nrhs, before_nrhs);
  ASC_DENSE_TEST_EQ(test, lda, before_lda);
  ASC_DENSE_TEST_EQ(test, ldb, before_ldb);
  ASC_DENSE_TEST_EQ(test, lwork, before_lwork);
  ASC_DENSE_TEST_EQ(test, info.front(), kGuard);
  ASC_DENSE_TEST_EQ(test, info.back(), kGuard);
  return info[1];
}
template <typename T>
void CheckQuery(base::TestContext& test, bool contract, int minimum,
                int recommendation, const T& output,
                const aa::Sample<T>& sample) {
  if (contract) {
    // The driver recommendation must cover its executed factor dependency.
    // Complex SY returns zero at N=0, below the dependency minimum one.
    ASC_DENSE_TEST_CHECK(test, std::isfinite(std::real(output)) &&
                                   std::real(output) >= minimum &&
                                   std::imag(output) == 0);
  } else {
    ASC_DENSE_TEST_EQ(test, output, base::Value<T>(recommendation));
  }
  ASC_DENSE_TEST_CHECK(
      test, base::EqualBytes(sample.b.data(), sample.original_b.data(),
                             sizeof(sample.b)));
}
template <typename T>
void Probe(base::TestContext& test, bool he, char tri, int n, int nrhs,
           bool query, bool contract, bool oversized, bool singular) {
  aa::Sample<T> sample(n, nrhs, he, tri == 'U' ? base::kUpper : base::kLower,
                       base::kColumn, base::kColumn, 0, singular);
  const bool active = n != 0;
  const auto factor_before = sample.a;
  std::array<lapack_int, 69> pivots;
  pivots.fill(kGuard);
  for (int i = 0; i < n; ++i) {
    pivots[i + 1] = sample.pivots[i + 1];
  }
  const auto pivots_before = pivots;
  const bool complex_symmetric = asc::DenseBlasComplex<T> && !he;
  const int minimum = std::max({1, 2 * n, 3 * n - 2});
  const int recommendation =
      complex_symmetric ? 65 * n : std::max(minimum, n <= 1 ? 1 : 65 * n);
  const int entries = (oversized ? std::max(minimum, recommendation) : minimum);
  std::array<T, 4365> work;
  work.fill(base::Value<T>(-761, 23));
  const auto info =
      GuardedCall(test, he, tri, n, nrhs, sample.a.data() + 1, sample.Ld(),
                  pivots.data() + 1, sample.b.data() + 1, sample.Ldb(),
                  work.data() + 1, query ? -1 : entries);
  ASC_DENSE_TEST_EQ(test, pivots.front(), kGuard);
  for (std::size_t i = static_cast<std::size_t>(n) + 1; i < pivots.size();
       ++i) {
    ASC_DENSE_TEST_EQ(test, pivots[i], kGuard);
  }
  if (query || !active) {
    ASC_DENSE_TEST_EQ(test, pivots, pivots_before);
    ASC_DENSE_TEST_CHECK(test,
                         base::EqualBytes(sample.a.data(), factor_before.data(),
                                          sizeof(factor_before)));
  } else {
    for (int i = 0; i < n; ++i) {
      sample.pivots[i + 1] = pivots[i + 1];
    }
    sample.Reconstruction(test);
  }
  ASC_DENSE_TEST_EQ(test, work.front(), base::Value<T>(-761, 23));
  for (std::size_t i = static_cast<std::size_t>(query ? 1 : entries) + 1;
       i < work.size(); ++i) {
    ASC_DENSE_TEST_EQ(test, work[i], base::Value<T>(-761, 23));
  }
  if (query) {
    ASC_DENSE_TEST_EQ(test, info, 0);
    CheckQuery(test, contract, minimum, recommendation, work[1], sample);
  } else {
    ASC_DENSE_TEST_EQ(test, work[1], base::Value<T>(recommendation));
    if (active && nrhs != 0 && singular) {
      ASC_DENSE_TEST_EQ(test, info, n);
      if (info > 0 && info <= n) {
        const auto diagonal = n == 1 ? sample.a[1] : work[n + info - 1];
        ASC_DENSE_TEST_EQ(test, diagonal, T{});
      }
    } else {
      ASC_DENSE_TEST_EQ(test, info, 0);
      sample.Solution(test);
    }
    if (!active || nrhs == 0) {
      ASC_DENSE_TEST_CHECK(
          test, base::EqualBytes(sample.b.data(), sample.original_b.data(),
                                 sizeof(sample.b)));
    }
  }
  sample.RhsGuards(test);
  sample.Guards(test);
  std::printf(
      "Aasen driver native real_bytes=%zu complex=%d he=%d tri=%c n=%d nrhs=%d "
      "query=%d singular=%d INFO=%lld\n",
      sizeof(asc::DenseBlasRealType<T>), asc::DenseBlasComplex<T>, he, tri, n,
      nrhs, query, singular, static_cast<long long>(info));
}
template <typename T>
void Run(base::TestContext& test, bool he, bool contract, int& cases) {
  for (char tri : {'U', 'L'}) {
    for (int n : {0, 1, 2, 3, 7, 67}) {
      for (int nrhs : {0, 1, 2, 3}) {
        Probe<T>(test, he, tri, n, nrhs, true, contract, false, false);
        ++cases;
        if (!contract) {
          for (bool oversized : {false, true}) {
            Probe<T>(test, he, tri, n, nrhs, false, false, oversized, false);
            ++cases;
          }
        }
      }
    }
    if (!contract) {
      for (int n : {1, 3, 7, 67}) {
        for (int nrhs : {1, 2, 3}) {
          Probe<T>(test, he, tri, n, nrhs, false, false, false, true);
          ++cases;
        }
      }
    }
  }
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  const bool contract =
      argc == 2 && std::string_view(argv[1]) == "workspace_contract";
  if (argc > 2 || (argc == 2 && !contract)) {
    return 2;
  }
  base::TestContext test;
  int cases = 0;
  Run<float>(test, false, contract, cases);
  Run<double>(test, false, contract, cases);
  Run<std::complex<float>>(test, false, contract, cases);
  Run<std::complex<double>>(test, false, contract, cases);
  Run<std::complex<float>>(test, true, contract, cases);
  Run<std::complex<double>>(test, true, contract, cases);
  std::printf("Aasen driver native cases=%d failed=%d\n", cases, test.Finish());
  return test.Finish();
}
