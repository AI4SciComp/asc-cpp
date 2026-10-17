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
#include "indefinite_aasen_solve_fixture.h"
#include "indefinite_aasen_solve_native.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"
#if defined(ASC_AASEN_SOLVE_SSYTRS_AA_HEADER)
#define ssytrs_aa_ emitted_ssytrs_aa
#include ASC_AASEN_SOLVE_SSYTRS_AA_HEADER
#undef ssytrs_aa_
#endif
#if defined(ASC_AASEN_SOLVE_DSYTRS_AA_HEADER)
#define dsytrs_aa_ emitted_dsytrs_aa
#include ASC_AASEN_SOLVE_DSYTRS_AA_HEADER
#undef dsytrs_aa_
#endif
#if defined(ASC_AASEN_SOLVE_CSYTRS_AA_HEADER)
#define csytrs_aa_ emitted_csytrs_aa
#include ASC_AASEN_SOLVE_CSYTRS_AA_HEADER
#undef csytrs_aa_
#endif
#if defined(ASC_AASEN_SOLVE_ZSYTRS_AA_HEADER)
#define zsytrs_aa_ emitted_zsytrs_aa
#include ASC_AASEN_SOLVE_ZSYTRS_AA_HEADER
#undef zsytrs_aa_
#endif
#if defined(ASC_AASEN_SOLVE_CHETRS_AA_HEADER)
#define chetrs_aa_ emitted_chetrs_aa
#include ASC_AASEN_SOLVE_CHETRS_AA_HEADER
#undef chetrs_aa_
#endif
#if defined(ASC_AASEN_SOLVE_ZHETRS_AA_HEADER)
#define zhetrs_aa_ emitted_zhetrs_aa
#include ASC_AASEN_SOLVE_ZHETRS_AA_HEADER
#undef zhetrs_aa_
#endif
namespace {
#if defined(ASC_AASEN_SOLVE_SSYTRS_AA_HEADER)
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
static_assert(
    std::is_same_v<typename Sig<decltype(LAPACK_ssytrs_aa_base)>::Type,
                   typename Sig<decltype(emitted_ssytrs_aa)>::Type>);
static_assert(
    std::is_same_v<typename Sig<decltype(LAPACK_dsytrs_aa_base)>::Type,
                   typename Sig<decltype(emitted_dsytrs_aa)>::Type>);
static_assert(
    std::is_same_v<typename Sig<decltype(LAPACK_csytrs_aa_base)>::Type,
                   typename Sig<decltype(emitted_csytrs_aa)>::Type>);
static_assert(
    std::is_same_v<typename Sig<decltype(LAPACK_zsytrs_aa_base)>::Type,
                   typename Sig<decltype(emitted_zsytrs_aa)>::Type>);
static_assert(
    std::is_same_v<typename Sig<decltype(LAPACK_chetrs_aa_base)>::Type,
                   typename Sig<decltype(emitted_chetrs_aa)>::Type>);
static_assert(
    std::is_same_v<typename Sig<decltype(LAPACK_zhetrs_aa_base)>::Type,
                   typename Sig<decltype(emitted_zhetrs_aa)>::Type>);
#endif
namespace base = asc_indefinite_rook_test;
namespace aa = asc_aasen_solve_test;
constexpr lapack_int kGuard = std::numeric_limits<lapack_int>::max() - 73;
template <typename T>
lapack_int GuardedCall(base::TestContext& test, bool he, char triangle,
                       lapack_int order, lapack_int columns, const T* a,
                       lapack_int leading, const lapack_int* pivots, T* b,
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
  aa::NativePointers(he, &uplo[1], &n[1], &nrhs[1], a, &lda[1], pivots, b,
                     &ldb[1], work, &lwork[1], &info[1]);
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
void Probe(base::TestContext& test, bool he, char tri, int n, int nrhs,
           bool query, bool contract, bool oversized, bool singular) {
  aa::Sample<T> sample(n, nrhs, he, tri == 'U' ? base::kUpper : base::kLower,
                       base::kColumn, base::kColumn, 0, singular);
  const bool active = n != 0 && nrhs != 0;
  if (active && !query) {
    aa::Produce(test, sample);
  }
  const auto factor_before = sample.a;
  std::array<lapack_int, 69> pivots;
  pivots.fill(kGuard);
  for (int i = 0; i < n; ++i) {
    pivots[i + 1] = sample.pivots[i + 1];
  }
  const auto pivots_before = pivots;
  const bool complex_symmetric = asc::DenseBlasComplex<T> && !he;
  int minimum = active ? 3 * n - 2 : 1;
  if (complex_symmetric) {
    minimum = std::max(1, 3 * n - 2);
  }
  const int recommendation = complex_symmetric ? 3 * n - 2 : minimum;
  const int entries = minimum + (oversized ? 7 : 0);
  std::array<T, 212> work;
  work.fill(base::Value<T>(-761, 23));
  const auto info =
      GuardedCall(test, he, tri, n, nrhs, sample.a.data() + 1, sample.Ld(),
                  pivots.data() + 1, sample.b.data() + 1, sample.Ldb(),
                  work.data() + 1, query ? -1 : entries);
  ASC_DENSE_TEST_EQ(test, pivots, pivots_before);
  ASC_DENSE_TEST_CHECK(test,
                       base::EqualBytes(sample.a.data(), factor_before.data(),
                                        sizeof(factor_before)));
  ASC_DENSE_TEST_EQ(test, work.front(), base::Value<T>(-761, 23));
  for (std::size_t i = static_cast<std::size_t>(query ? 1 : entries) + 1;
       i < work.size(); ++i) {
    ASC_DENSE_TEST_EQ(test, work[i], base::Value<T>(-761, 23));
  }
  if (query) {
    ASC_DENSE_TEST_EQ(test, info, 0);
    if (contract) {
      // A reported minimal LWORK must satisfy the routine's own documented
      // lower bound. CSY/ZSY N=0 currently return -2, not a valid size.
      ASC_DENSE_TEST_CHECK(test, std::isfinite(std::real(work[1])) &&
                                     std::real(work[1]) >= minimum &&
                                     std::imag(work[1]) == 0);
    } else {
      ASC_DENSE_TEST_EQ(test, work[1], base::Value<T>(recommendation));
    }
    ASC_DENSE_TEST_CHECK(
        test, base::EqualBytes(sample.b.data(), sample.original_b.data(),
                               sizeof(sample.b)));
  } else if (!active) {
    ASC_DENSE_TEST_EQ(test, info, 0);
    for (const auto value : work) {
      ASC_DENSE_TEST_EQ(test, value, base::Value<T>(-761, 23));
    }
    ASC_DENSE_TEST_CHECK(
        test, base::EqualBytes(sample.b.data(), sample.original_b.data(),
                               sizeof(sample.b)));
  } else if (singular) {
    ASC_DENSE_TEST_EQ(test, info, n);
    if (info > 0 && info <= n) {
      ASC_DENSE_TEST_EQ(test, work[n + info - 1], T{});
    }
  } else {
    ASC_DENSE_TEST_EQ(test, info, 0);
    sample.Solution(test);
  }
  sample.RhsGuards(test);
  sample.Guards(test);
  std::printf(
      "Aasen solve native real_bytes=%zu complex=%d he=%d tri=%c n=%d nrhs=%d "
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
  std::printf("Aasen solve native cases=%d failed=%d\n", cases, test.Finish());
  return test.Finish();
}
