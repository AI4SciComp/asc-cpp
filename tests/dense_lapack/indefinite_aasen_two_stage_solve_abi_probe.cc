#include <array>
#include <complex>
#include <cstdio>
#include <limits>
#include <vector>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "../dense/test_support.h"
#include "indefinite_aasen_two_stage_solve_fixture.h"
#include "indefinite_aasen_two_stage_solve_native.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"
#if defined(ASC_AASEN_TWO_STAGE_SOLVE_SSYTRS_AA_2STAGE_HEADER)
#include <type_traits>
#define ssytrs_aa_2stage_ emitted_ssytrs_aa_2stage
#include ASC_AASEN_TWO_STAGE_SOLVE_SSYTRS_AA_2STAGE_HEADER
#undef ssytrs_aa_2stage_
#endif
#if defined(ASC_AASEN_TWO_STAGE_SOLVE_DSYTRS_AA_2STAGE_HEADER)
#define dsytrs_aa_2stage_ emitted_dsytrs_aa_2stage
#include ASC_AASEN_TWO_STAGE_SOLVE_DSYTRS_AA_2STAGE_HEADER
#undef dsytrs_aa_2stage_
#endif
#if defined(ASC_AASEN_TWO_STAGE_SOLVE_CSYTRS_AA_2STAGE_HEADER)
#define csytrs_aa_2stage_ emitted_csytrs_aa_2stage
#include ASC_AASEN_TWO_STAGE_SOLVE_CSYTRS_AA_2STAGE_HEADER
#undef csytrs_aa_2stage_
#endif
#if defined(ASC_AASEN_TWO_STAGE_SOLVE_ZSYTRS_AA_2STAGE_HEADER)
#define zsytrs_aa_2stage_ emitted_zsytrs_aa_2stage
#include ASC_AASEN_TWO_STAGE_SOLVE_ZSYTRS_AA_2STAGE_HEADER
#undef zsytrs_aa_2stage_
#endif
#if defined(ASC_AASEN_TWO_STAGE_SOLVE_CHETRS_AA_2STAGE_HEADER)
#define chetrs_aa_2stage_ emitted_chetrs_aa_2stage
#include ASC_AASEN_TWO_STAGE_SOLVE_CHETRS_AA_2STAGE_HEADER
#undef chetrs_aa_2stage_
#endif
#if defined(ASC_AASEN_TWO_STAGE_SOLVE_ZHETRS_AA_2STAGE_HEADER)
#define zhetrs_aa_2stage_ emitted_zhetrs_aa_2stage
#include ASC_AASEN_TWO_STAGE_SOLVE_ZHETRS_AA_2STAGE_HEADER
#undef zhetrs_aa_2stage_
#endif
namespace {
#if defined(ASC_AASEN_TWO_STAGE_SOLVE_SSYTRS_AA_2STAGE_HEADER)
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
    std::is_same_v<typename Sig<decltype(LAPACK_ssytrs_aa_2stage_base)>::Type,
                   typename Sig<decltype(emitted_ssytrs_aa_2stage)>::Type>);
static_assert(
    std::is_same_v<typename Sig<decltype(LAPACK_dsytrs_aa_2stage_base)>::Type,
                   typename Sig<decltype(emitted_dsytrs_aa_2stage)>::Type>);
static_assert(
    std::is_same_v<typename Sig<decltype(LAPACK_csytrs_aa_2stage_base)>::Type,
                   typename Sig<decltype(emitted_csytrs_aa_2stage)>::Type>);
static_assert(
    std::is_same_v<typename Sig<decltype(LAPACK_zsytrs_aa_2stage_base)>::Type,
                   typename Sig<decltype(emitted_zsytrs_aa_2stage)>::Type>);
static_assert(
    std::is_same_v<typename Sig<decltype(LAPACK_chetrs_aa_2stage_base)>::Type,
                   typename Sig<decltype(emitted_chetrs_aa_2stage)>::Type>);
static_assert(
    std::is_same_v<typename Sig<decltype(LAPACK_zhetrs_aa_2stage_base)>::Type,
                   typename Sig<decltype(emitted_zhetrs_aa_2stage)>::Type>);
#endif
namespace base = asc_indefinite_rook_test;
namespace aa = asc_aasen_two_stage_solve_test;
constexpr lapack_int kGuard = std::numeric_limits<lapack_int>::max() - 73;
template <typename T>
void GuardedCall(base::TestContext& test, aa::Sample<T>& sample) {
  std::array<char, 3> tri{'a', sample.original.upper ? 'U' : 'L', 'z'};
  std::array<lapack_int, 3> n{kGuard, sample.n, kGuard};
  std::array<lapack_int, 3> nrhs{kGuard, sample.nrhs, kGuard};
  std::array<lapack_int, 3> lda{kGuard, sample.original.lda, kGuard};
  std::array<lapack_int, 3> ltb{kGuard, sample.ltb, kGuard};
  std::array<lapack_int, 3> ldb{kGuard, sample.ldb, kGuard};
  std::array<lapack_int, 3> info{kGuard, std::numeric_limits<lapack_int>::min(),
                                 kGuard};
  const auto before_tri = tri;
  const auto before_n = n;
  const auto before_nrhs = nrhs;
  const auto before_lda = lda;
  const auto before_ltb = ltb;
  const auto before_ldb = ldb;
  std::vector<lapack_int> p(sample.p.begin(), sample.p.end());
  std::vector<lapack_int> q(sample.q.begin(), sample.q.end());
  const auto before_p = p;
  const auto before_q = q;
  const auto before_a = sample.a;
  const auto before_tb = sample.tb;
  aa::NativePointers(sample.original.hermitian, &tri[1], &n[1], &nrhs[1],
                     sample.a.data() + 1, &lda[1], sample.tb.data() + 1,
                     &ltb[1], p.data() + 1, q.data() + 1, sample.b.data() + 1,
                     &ldb[1], &info[1]);
  ASC_DENSE_TEST_EQ(test, tri, before_tri);
  ASC_DENSE_TEST_EQ(test, n, before_n);
  ASC_DENSE_TEST_EQ(test, nrhs, before_nrhs);
  ASC_DENSE_TEST_EQ(test, lda, before_lda);
  ASC_DENSE_TEST_EQ(test, ltb, before_ltb);
  ASC_DENSE_TEST_EQ(test, ldb, before_ldb);
  ASC_DENSE_TEST_EQ(test, info.front(), kGuard);
  ASC_DENSE_TEST_EQ(test, info.back(), kGuard);
  ASC_DENSE_TEST_EQ(test, info[1], 0);
  ASC_DENSE_TEST_EQ(test, p, before_p);
  ASC_DENSE_TEST_EQ(test, q, before_q);
  ASC_DENSE_TEST_CHECK(test, base::EqualBytes(sample.a.data(), before_a.data(),
                                              sample.a.size() * sizeof(T)));
  ASC_DENSE_TEST_CHECK(test,
                       base::EqualBytes(sample.tb.data(), before_tb.data(),
                                        sample.tb.size() * sizeof(T)));
  sample.RhsGuards(test);
  if (sample.n && sample.nrhs) {
    sample.Solution(test);
  } else {
    ASC_DENSE_TEST_CHECK(
        test, base::EqualBytes(sample.b.data(), sample.before_b.data(),
                               sample.b.size() * sizeof(T)));
  }
}
template <typename T>
int Run(base::TestContext& test, bool he) {
  int cases = 0;
  for (bool upper : {false, true}) {
    for (int n : {0, 1, 2, 3, 7, 67, 193}) {
      for (int nrhs : {0, 1, 3}) {
        for (int band : {0, 1, 2}) {
          for (int work : {0, 1, 2}) {
            aa::Sample<T> sample(n, nrhs, he, upper, false, false, band, work);
            if (n && nrhs) {
              sample.Produce(test, nrhs == 3);
            }
            GuardedCall(test, sample);
            ++cases;
          }
        }
      }
    }
    for (int nrhs : {31, 32, 33}) {
      aa::Sample<T> sample(7, nrhs, he, upper, false, false, 0, 0);
      sample.Produce(test, true);
      GuardedCall(test, sample);
      ++cases;
    }
  }
  return cases;
}
}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  base::TestContext test;
  int cases = 0;
  cases += Run<float>(test, false);
  cases += Run<double>(test, false);
  cases += Run<std::complex<float>>(test, false);
  cases += Run<std::complex<double>>(test, false);
  cases += Run<std::complex<float>>(test, true);
  cases += Run<std::complex<double>>(test, true);
  std::printf("Two-stage Aasen native solve cases=%d\n", cases);
  return test.Finish();
}
