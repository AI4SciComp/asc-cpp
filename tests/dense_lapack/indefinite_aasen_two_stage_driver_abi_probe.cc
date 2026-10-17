#include <algorithm>
#include <array>
#include <complex>
#include <cstdio>
#include <limits>
#include <string_view>
#include <vector>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "../dense/test_support.h"
#include "asc/dense/blas.h"
#include "indefinite_aasen_two_stage_driver_native.h"
#include "indefinite_aasen_two_stage_fixture.h"
#include "indefinite_aasen_two_stage_solve_fixture.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"
#if defined(ASC_AASEN_TWO_STAGE_DRIVER_CHESV_AA_2STAGE_HEADER)
#include <type_traits>
#define chesv_aa_2stage_ emitted_chesv_aa_2stage
#include ASC_AASEN_TWO_STAGE_DRIVER_CHESV_AA_2STAGE_HEADER
#undef chesv_aa_2stage_
#endif
#if defined(ASC_AASEN_TWO_STAGE_DRIVER_CSYSV_AA_2STAGE_HEADER)
#define csysv_aa_2stage_ emitted_csysv_aa_2stage
#include ASC_AASEN_TWO_STAGE_DRIVER_CSYSV_AA_2STAGE_HEADER
#undef csysv_aa_2stage_
#endif
#if defined(ASC_AASEN_TWO_STAGE_DRIVER_DSYSV_AA_2STAGE_HEADER)
#define dsysv_aa_2stage_ emitted_dsysv_aa_2stage
#include ASC_AASEN_TWO_STAGE_DRIVER_DSYSV_AA_2STAGE_HEADER
#undef dsysv_aa_2stage_
#endif
#if defined(ASC_AASEN_TWO_STAGE_DRIVER_SSYSV_AA_2STAGE_HEADER)
#define ssysv_aa_2stage_ emitted_ssysv_aa_2stage
#include ASC_AASEN_TWO_STAGE_DRIVER_SSYSV_AA_2STAGE_HEADER
#undef ssysv_aa_2stage_
#endif
#if defined(ASC_AASEN_TWO_STAGE_DRIVER_ZHESV_AA_2STAGE_HEADER)
#define zhesv_aa_2stage_ emitted_zhesv_aa_2stage
#include ASC_AASEN_TWO_STAGE_DRIVER_ZHESV_AA_2STAGE_HEADER
#undef zhesv_aa_2stage_
#endif
#if defined(ASC_AASEN_TWO_STAGE_DRIVER_ZSYSV_AA_2STAGE_HEADER)
#define zsysv_aa_2stage_ emitted_zsysv_aa_2stage
#include ASC_AASEN_TWO_STAGE_DRIVER_ZSYSV_AA_2STAGE_HEADER
#undef zsysv_aa_2stage_
#endif
namespace {
#if defined(ASC_AASEN_TWO_STAGE_DRIVER_CHESV_AA_2STAGE_HEADER)
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
    std::is_same_v<typename Sig<decltype(LAPACK_chesv_aa_2stage_base)>::Type,
                   typename Sig<decltype(emitted_chesv_aa_2stage)>::Type>);
static_assert(
    std::is_same_v<typename Sig<decltype(LAPACK_csysv_aa_2stage_base)>::Type,
                   typename Sig<decltype(emitted_csysv_aa_2stage)>::Type>);
static_assert(
    std::is_same_v<typename Sig<decltype(LAPACK_dsysv_aa_2stage_base)>::Type,
                   typename Sig<decltype(emitted_dsysv_aa_2stage)>::Type>);
static_assert(
    std::is_same_v<typename Sig<decltype(LAPACK_ssysv_aa_2stage_base)>::Type,
                   typename Sig<decltype(emitted_ssysv_aa_2stage)>::Type>);
static_assert(
    std::is_same_v<typename Sig<decltype(LAPACK_zhesv_aa_2stage_base)>::Type,
                   typename Sig<decltype(emitted_zhesv_aa_2stage)>::Type>);
static_assert(
    std::is_same_v<typename Sig<decltype(LAPACK_zsysv_aa_2stage_base)>::Type,
                   typename Sig<decltype(emitted_zsysv_aa_2stage)>::Type>);
#endif
namespace base = asc_indefinite_rook_test;
namespace aa = asc_aasen_two_stage_solve_test;
namespace factor = asc_aasen_two_stage_test;
namespace driver = asc_aasen_two_stage_driver_test;
constexpr lapack_int kGuard = std::numeric_limits<lapack_int>::max() - 73;
template <typename T>
lapack_int GuardedCall(base::TestContext& test, bool he, char triangle,
                       lapack_int order, lapack_int columns, T* a,
                       lapack_int leading, T* tb, lapack_int band_entries,
                       lapack_int* p, lapack_int* q, T* b,
                       lapack_int rhs_leading, T* work, lapack_int entries) {
  std::array<char, 3> tri{'a', triangle, 'z'};
  std::array<lapack_int, 3> n{kGuard, order, kGuard};
  std::array<lapack_int, 3> nrhs{kGuard, columns, kGuard};
  std::array<lapack_int, 3> lda{kGuard, leading, kGuard};
  std::array<lapack_int, 3> ltb{kGuard, band_entries, kGuard};
  std::array<lapack_int, 3> ldb{kGuard, rhs_leading, kGuard};
  std::array<lapack_int, 3> lwork{kGuard, entries, kGuard};
  std::array<lapack_int, 3> info{kGuard, std::numeric_limits<lapack_int>::min(),
                                 kGuard};
  const auto before_tri = tri;
  const auto before_n = n;
  const auto before_nrhs = nrhs;
  const auto before_lda = lda;
  const auto before_ltb = ltb;
  const auto before_ldb = ldb;
  const auto before_lwork = lwork;
  driver::NativePointers(he, &tri[1], &n[1], &nrhs[1], a, &lda[1], tb, &ltb[1],
                         p, q, b, &ldb[1], work, &lwork[1], &info[1]);
  ASC_DENSE_TEST_EQ(test, tri, before_tri);
  ASC_DENSE_TEST_EQ(test, n, before_n);
  ASC_DENSE_TEST_EQ(test, nrhs, before_nrhs);
  ASC_DENSE_TEST_EQ(test, lda, before_lda);
  ASC_DENSE_TEST_EQ(test, ltb, before_ltb);
  ASC_DENSE_TEST_EQ(test, ldb, before_ldb);
  ASC_DENSE_TEST_EQ(test, lwork, before_lwork);
  ASC_DENSE_TEST_EQ(test, info.front(), kGuard);
  ASC_DENSE_TEST_EQ(test, info.back(), kGuard);
  return info[1];
}
template <typename T>
T Recommendation(int n, bool he, bool band) {
  using Real = asc::DenseBlasRealType<T>;
  const bool complex_symmetric = asc::DenseBlasComplex<T> && !he;
  const int count = std::max(complex_symmetric ? 0 : 1, (band ? 577 : 192) * n);
  Real value = static_cast<Real>(count);
  if constexpr (sizeof(Real) == 4) {
    // Exact pinned SROUNDUP_LWORK; CSY's TB query uses CMPLX directly.
    if ((!band || !complex_symmetric) && static_cast<int>(value) < count) {
      value *= Real{1} + std::numeric_limits<Real>::epsilon();
    }
  }
  return T{value};
}
template <typename T>
void CheckFactor(base::TestContext& test, const aa::Sample<T>& sample,
                 const std::vector<T>& tb, const std::vector<lapack_int>& p,
                 const std::vector<lapack_int>& q, int ltb, int lwork) {
  const int n = sample.n;
  const int nb = std::min({192, (ltb / n - 1) / 3, lwork / n});
  ASC_DENSE_TEST_EQ(test, tb[1], T{static_cast<asc::DenseBlasRealType<T>>(nb)});
  for (int i = 0; i < n; ++i) {
    ASC_DENSE_TEST_CHECK(test, p[i + 1] >= i + 1 && p[i + 1] <= n);
    ASC_DENSE_TEST_CHECK(test, i >= nb || p[i + 1] == i + 1);
    ASC_DENSE_TEST_CHECK(
        test, q[i + 1] >= i + 1 && q[i + 1] <= std::min(n, i + nb + 1));
  }
  if (sample.nrhs == 3) {
    factor::Reconstruction(test, n, sample.original.hermitian,
                           sample.original.upper, sample.a.data() + 1,
                           sample.original.lda, tb.data() + 1, ltb / n, nb,
                           p.data() + 1, q.data() + 1, sample.original.full);
  }
  if (sample.singular) {
    ASC_DENSE_TEST_EQ(test, tb[1 + (n - 1) * (ltb / n) + 2 * nb], T{});
  }
}
template <typename T>
void Probe(base::TestContext& test, bool he, bool upper, int n, int nrhs,
           int band_mode, int work_mode, bool singular) {
  aa::Sample<T> sample(n, nrhs, he, upper, false, false, band_mode, work_mode,
                       singular);
  const int minimum = asc::DenseBlasComplex<T> && !he ? 0 : 1;
  const int ltb = std::max(minimum, sample.ltb);
  const int lwork = std::max(minimum, sample.work_entries);
  // Even a native empty CSY driver writes its two first internal-query slots.
  std::vector<T> tb(static_cast<std::size_t>(std::max(1, ltb) + 2), T{-73});
  std::vector<T> work(static_cast<std::size_t>(std::max(1, lwork) + 2), T{-79});
  std::vector<lapack_int> p(static_cast<std::size_t>(n + 2), kGuard);
  std::fill_n(p.data() + 1, n, std::numeric_limits<lapack_int>::min());
  auto q = p;
  const auto before_p = p;
  const auto before_a = sample.a;
  const auto info = GuardedCall(
      test, he, upper ? 'U' : 'L', n, nrhs, sample.a.data() + 1,
      sample.original.lda, tb.data() + 1, ltb, p.data() + 1, q.data() + 1,
      sample.b.data() + 1, sample.ldb, work.data() + 1, lwork);
  ASC_DENSE_TEST_EQ(test, info, singular ? n : 0);
  ASC_DENSE_TEST_EQ(test, p.front(), kGuard);
  ASC_DENSE_TEST_EQ(test, p.back(), kGuard);
  ASC_DENSE_TEST_EQ(test, q.front(), kGuard);
  ASC_DENSE_TEST_EQ(test, q.back(), kGuard);
  ASC_DENSE_TEST_EQ(test, tb.front(), T{-73});
  ASC_DENSE_TEST_EQ(test, tb.back(), T{-73});
  ASC_DENSE_TEST_EQ(test, work.front(), T{-79});
  ASC_DENSE_TEST_EQ(test, work.back(), T{-79});
  ASC_DENSE_TEST_EQ(test, work[1], Recommendation<T>(n, he, false));
  if (n != 0) {
    CheckFactor(test, sample, tb, p, q, ltb, lwork);
  } else {
    ASC_DENSE_TEST_EQ(test, p, before_p);
    ASC_DENSE_TEST_EQ(test, q, before_p);
    ASC_DENSE_TEST_CHECK(test,
                         base::EqualBytes(sample.a.data(), before_a.data(),
                                          sample.a.size() * sizeof(T)));
    ASC_DENSE_TEST_EQ(test, tb[1], Recommendation<T>(n, he, true));
  }
  if (n != 0 && nrhs != 0 && !singular) {
    sample.Solution(test);
  } else {
    ASC_DENSE_TEST_CHECK(
        test, base::EqualBytes(sample.b.data(), sample.before_b.data(),
                               sample.b.size() * sizeof(T)));
  }
  sample.original.a = sample.a;
  sample.original.Guards(test);
  sample.RhsGuards(test);
  std::printf(
      "Two-stage driver native real_bytes=%zu complex=%d he=%d upper=%d n=%d "
      "nrhs=%d band=%d work=%d singular=%d INFO=%lld\n",
      sizeof(asc::DenseBlasRealType<T>), asc::DenseBlasComplex<T>, he, upper, n,
      nrhs, band_mode, work_mode, singular, static_cast<long long>(info));
}
template <typename T>
void Capacity(base::TestContext& test, const T& output, int required) {
  using Real = asc::DenseBlasRealType<T>;
  const auto value = base::ToWide(output);
  ASC_DENSE_TEST_EQ(test, value.imag(), 0);
  ASC_DENSE_TEST_CHECK(test, value.real() >= required);
  ASC_DENSE_TEST_CHECK(
      test, value.real() <=
                required * (1.L + 2 * std::numeric_limits<Real>::epsilon()));
}
template <typename T>
void Query(base::TestContext& test, bool he, char tri, int n, int mode,
           bool contract) {
  const bool tb_query = mode != 1;
  const bool work_query = mode != 0;
  const int minimum = asc::DenseBlasComplex<T> && !he ? 0 : 1;
  std::array<T, 3> a{T{-83}, T{-83}, T{-83}};
  std::array<T, 3> b{T{-85}, T{-85}, T{-85}};
  std::array<T, 3> tb{T{-73}, T{-73}, T{-73}};
  std::array<T, 3> work{T{-79}, T{-79}, T{-79}};
  std::array<lapack_int, 3> p{kGuard, kGuard, kGuard};
  auto q = p;
  const auto info =
      GuardedCall(test, he, tri, n, 3, a.data() + 1, std::max(1, n),
                  tb.data() + 1, tb_query ? -1 : std::max(minimum, 4 * n),
                  p.data() + 1, q.data() + 1, b.data() + 1, std::max(1, n),
                  work.data() + 1, work_query ? -1 : std::max(minimum, n));
  ASC_DENSE_TEST_EQ(test, info, 0);
  for (auto value : a) {
    ASC_DENSE_TEST_EQ(test, value, T{-83});
  }
  for (auto value : b) {
    ASC_DENSE_TEST_EQ(test, value, T{-85});
  }
  for (auto value : p) {
    ASC_DENSE_TEST_EQ(test, value, kGuard);
  }
  for (auto value : q) {
    ASC_DENSE_TEST_EQ(test, value, kGuard);
  }
  ASC_DENSE_TEST_EQ(test, tb.front(), T{-73});
  ASC_DENSE_TEST_EQ(test, tb.back(), T{-73});
  ASC_DENSE_TEST_EQ(test, work.front(), T{-79});
  ASC_DENSE_TEST_EQ(test, work.back(), T{-79});
  // Both slots are written by the internal dual query, regardless of mode.
  ASC_DENSE_TEST_EQ(test, tb[1], Recommendation<T>(n, he, true));
  ASC_DENSE_TEST_EQ(test, work[1], Recommendation<T>(n, he, false));
  if (contract && tb_query) {
    Capacity(test, tb[1], std::max(minimum, 577 * n));
  }
  if (contract && work_query) {
    Capacity(test, work[1], std::max(minimum, 192 * n));
  }
  std::printf(
      "Two-stage driver query real_bytes=%zu complex=%d he=%d tri=%c n=%d "
      "mode=%d TB=%.0Lf WORK=%.0Lf\n",
      sizeof(asc::DenseBlasRealType<T>), asc::DenseBlasComplex<T>, he, tri, n,
      mode, base::ToWide(tb[1]).real(), base::ToWide(work[1]).real());
}
template <typename T>
int Ordinary(base::TestContext& test, bool he) {
  int cases = 0;
  for (bool upper : {false, true}) {
    for (int n : {0, 1, 2, 3, 7, 67, 193}) {
      for (int nrhs : {0, 1, 3}) {
        for (int band : {0, 1, 2}) {
          for (int work : {0, 1, 2}) {
            Probe<T>(test, he, upper, n, nrhs, band, work, false);
            ++cases;
          }
        }
      }
    }
    for (int nrhs : {31, 32, 33}) {
      Probe<T>(test, he, upper, 7, nrhs, 0, 0, false);
      ++cases;
    }
    for (int n : {1, 3, 67}) {
      for (int nrhs : {0, 3}) {
        for (int capacity : {0, 2}) {
          Probe<T>(test, he, upper, n, nrhs, capacity, capacity, true);
          ++cases;
        }
      }
    }
  }
  return cases;
}
template <typename T>
int Queries(base::TestContext& test, bool he, bool contract) {
  int cases = 0;
  for (char tri : {'U', 'L'}) {
    for (int n : {0, 1, 2, 7, 65, 193, 30001, 30003}) {
      for (int mode : {0, 1, 2}) {
        Query<T>(test, he, tri, n, mode, contract);
        ++cases;
      }
    }
  }
  return cases;
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 1 &&
      (argc != 2 || std::string_view(argv[1]) != "workspace_contract")) {
    return 2;
  }
  base::TestContext test;
  int ordinary = 0;
  if (argc == 1) {
    ordinary += Ordinary<float>(test, false);
    ordinary += Ordinary<double>(test, false);
    ordinary += Ordinary<std::complex<float>>(test, false);
    ordinary += Ordinary<std::complex<double>>(test, false);
    ordinary += Ordinary<std::complex<float>>(test, true);
    ordinary += Ordinary<std::complex<double>>(test, true);
  }
  int queries = 0;
  queries += Queries<float>(test, false, argc == 2);
  queries += Queries<double>(test, false, argc == 2);
  queries += Queries<std::complex<float>>(test, false, argc == 2);
  queries += Queries<std::complex<double>>(test, false, argc == 2);
  queries += Queries<std::complex<float>>(test, true, argc == 2);
  queries += Queries<std::complex<double>>(test, true, argc == 2);
  std::printf(
      "Two-stage Aasen native driver ordinary=%d query=%d contract=%d\n",
      ordinary, queries, static_cast<int>(argc == 2));
  return test.Finish();
}
