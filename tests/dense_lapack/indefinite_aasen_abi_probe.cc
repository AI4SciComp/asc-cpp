#include <algorithm>
#include <array>
#include <complex>
#include <cstdio>
#include <limits>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "../dense/test_support.h"
#include "asc/dense/blas.h"
#include "indefinite_aasen_fixture.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"
#if defined(ASC_AASEN_SSYTRF_AA_HEADER)
#define ssytrf_aa_ emitted_ssytrf_aa
#include ASC_AASEN_SSYTRF_AA_HEADER
#undef ssytrf_aa_
#endif
#if defined(ASC_AASEN_DSYTRF_AA_HEADER)
#define dsytrf_aa_ emitted_dsytrf_aa
#include ASC_AASEN_DSYTRF_AA_HEADER
#undef dsytrf_aa_
#endif
#if defined(ASC_AASEN_CSYTRF_AA_HEADER)
#define csytrf_aa_ emitted_csytrf_aa
#include ASC_AASEN_CSYTRF_AA_HEADER
#undef csytrf_aa_
#endif
#if defined(ASC_AASEN_ZSYTRF_AA_HEADER)
#define zsytrf_aa_ emitted_zsytrf_aa
#include ASC_AASEN_ZSYTRF_AA_HEADER
#undef zsytrf_aa_
#endif
#if defined(ASC_AASEN_CHETRF_AA_HEADER)
#define chetrf_aa_ emitted_chetrf_aa
#include ASC_AASEN_CHETRF_AA_HEADER
#undef chetrf_aa_
#endif
#if defined(ASC_AASEN_ZHETRF_AA_HEADER)
#define zhetrf_aa_ emitted_zhetrf_aa
#include ASC_AASEN_ZHETRF_AA_HEADER
#undef zhetrf_aa_
#endif
namespace {
#if defined(ASC_AASEN_SSYTRF_AA_HEADER)
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
    std::is_same_v<typename Sig<decltype(LAPACK_ssytrf_aa_base)>::Type,
                   typename Sig<decltype(emitted_ssytrf_aa)>::Type>);
static_assert(
    std::is_same_v<typename Sig<decltype(LAPACK_dsytrf_aa_base)>::Type,
                   typename Sig<decltype(emitted_dsytrf_aa)>::Type>);
static_assert(
    std::is_same_v<typename Sig<decltype(LAPACK_csytrf_aa_base)>::Type,
                   typename Sig<decltype(emitted_csytrf_aa)>::Type>);
static_assert(
    std::is_same_v<typename Sig<decltype(LAPACK_zsytrf_aa_base)>::Type,
                   typename Sig<decltype(emitted_zsytrf_aa)>::Type>);
static_assert(
    std::is_same_v<typename Sig<decltype(LAPACK_chetrf_aa_base)>::Type,
                   typename Sig<decltype(emitted_chetrf_aa)>::Type>);
static_assert(
    std::is_same_v<typename Sig<decltype(LAPACK_zhetrf_aa_base)>::Type,
                   typename Sig<decltype(emitted_zhetrf_aa)>::Type>);
#endif
namespace base = asc_indefinite_rook_test;
constexpr lapack_int kGuard = std::numeric_limits<lapack_int>::max() - 73;
template <typename T>
void NativePointers(bool he, const char* uplo, const lapack_int* n, T* a,
                    const lapack_int* lda, lapack_int* pivots, T* work,
                    const lapack_int* lwork, lapack_int* info) {
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_ssytrf_aa(uplo, n, a, lda, pivots, work, lwork, info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dsytrf_aa(uplo, n, a, lda, pivots, work, lwork, info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (he) {
      LAPACK_chetrf_aa(uplo, n, a, lda, pivots, work, lwork, info);
    } else {
      LAPACK_csytrf_aa(uplo, n, a, lda, pivots, work, lwork, info);
    }
  } else if constexpr (std::is_same_v<T, std::complex<double>>) {
    if (he) {
      LAPACK_zhetrf_aa(uplo, n, a, lda, pivots, work, lwork, info);
    } else {
      LAPACK_zsytrf_aa(uplo, n, a, lda, pivots, work, lwork, info);
    }
  }
}
template <typename T>
lapack_int GuardedCall(base::TestContext& test, bool he, char triangle,
                       lapack_int order, T* a, lapack_int leading,
                       lapack_int* pivots, T* work, lapack_int entries) {
  std::array<char, 3> uplo{'a', triangle, 'z'};
  std::array<lapack_int, 3> n{kGuard, order, kGuard};
  std::array<lapack_int, 3> lda{kGuard, leading, kGuard};
  std::array<lapack_int, 3> lwork{kGuard, entries, kGuard};
  std::array<lapack_int, 3> info{kGuard, std::numeric_limits<lapack_int>::min(),
                                 kGuard};
  const auto before_uplo = uplo;
  const auto before_n = n;
  const auto before_lda = lda;
  const auto before_lwork = lwork;
  NativePointers(he, &uplo[1], &n[1], a, &lda[1], pivots, work, &lwork[1],
                 &info[1]);
  ASC_DENSE_TEST_EQ(test, uplo, before_uplo);
  ASC_DENSE_TEST_EQ(test, n, before_n);
  ASC_DENSE_TEST_EQ(test, lda, before_lda);
  ASC_DENSE_TEST_EQ(test, lwork, before_lwork);
  ASC_DENSE_TEST_EQ(test, info.front(), kGuard);
  ASC_DENSE_TEST_EQ(test, info.back(), kGuard);
  return info[1];
}
template <typename T>
void Probe(base::TestContext& test, bool he, char tri, int order,
           int workspace_mode, bool query) {
  asc_aasen_test::Sample<T> sample(order, he,
                                   tri == 'U' ? base::kUpper : base::kLower,
                                   base::kColumn, 0, false);
  // Native Hermitian factors consume a mathematical Hermitian input. The
  // public original-matrix policy for ignored diagonal imaginary parts is
  // exercised separately by checked-call tests.
  if constexpr (asc::DenseBlasComplex<T>) {
    if (he) {
      for (int i = 0; i < order; ++i) {
        sample.a[sample.Offset(i, i)].imag(0);
      }
    }
  }
  const auto before = sample.a;
  constexpr bool kComplex = asc::DenseBlasComplex<T>;
  const bool special = kComplex && !he;
  const int minimum = order <= 1 && !special ? 1 : std::max(1, 2 * order);
  const int preferred = order <= 1 && !special ? 1 : 65 * order;
  const int entries =
      workspace_mode == 0
          ? minimum
          : std::max(minimum, (workspace_mode == 1 ? 3 : 65) * order);
  std::array<T, 4360> work;
  work.fill(T{-79});
  std::array<lapack_int, 69> pivots;
  pivots.fill(kGuard);
  const lapack_int info =
      GuardedCall(test, he, tri, order, sample.a.data() + 1, sample.Ld(),
                  pivots.data() + 1, work.data() + 1, query ? -1 : entries);
  ASC_DENSE_TEST_EQ(test, info, 0);
  ASC_DENSE_TEST_EQ(test, work[1],
                    T{static_cast<asc::DenseBlasRealType<T>>(preferred)});
  ASC_DENSE_TEST_EQ(test, work.front(), T{-79});
  for (std::size_t i = static_cast<std::size_t>(query ? 1 : entries) + 1;
       i < work.size(); ++i) {
    ASC_DENSE_TEST_EQ(test, work[i], T{-79});
  }
  ASC_DENSE_TEST_EQ(test, pivots.front(), kGuard);
  for (std::size_t i = static_cast<std::size_t>(query ? 0 : order) + 1;
       i < pivots.size(); ++i) {
    ASC_DENSE_TEST_EQ(test, pivots[i], kGuard);
  }
  if (query) {
    ASC_DENSE_TEST_CHECK(
        test, base::EqualBytes(sample.a.data(), before.data(), sizeof(before)));
  } else {
    for (int i = 0; i < order; ++i) {
      sample.pivots[i + 1] = pivots[i + 1];
    }
    sample.Reconstruction(test);
  }
  sample.Guards(test);
  std::printf(
      "Aasen native real_bytes=%zu complex=%d he=%d tri=%c n=%d work=%d "
      "query=%d INFO=%lld\n",
      sizeof(asc::DenseBlasRealType<T>), kComplex, he, tri, order, entries,
      query, static_cast<long long>(info));
}
template <typename T>
void Run(base::TestContext& test, bool he, int& cases) {
  for (char tri : {'U', 'L'}) {
    for (int order : {0, 1, 2, 3, 7, 67}) {
      Probe<T>(test, he, tri, order, 0, true);
      ++cases;
      for (int mode : {0, 1, 2}) {
        Probe<T>(test, he, tri, order, mode, false);
        ++cases;
      }
    }
  }
}
}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  base::TestContext test;
  int cases = 0;
  Run<float>(test, false, cases);
  Run<double>(test, false, cases);
  Run<std::complex<float>>(test, false, cases);
  Run<std::complex<double>>(test, false, cases);
  Run<std::complex<float>>(test, true, cases);
  Run<std::complex<double>>(test, true, cases);
  std::printf("Aasen native cases=%d failed=%d\n", cases, test.Finish());
  return test.Finish();
}
