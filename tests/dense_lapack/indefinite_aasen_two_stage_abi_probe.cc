#include <algorithm>
#include <array>
#include <complex>
#include <cstdio>
#include <limits>
#include <string_view>
#include <type_traits>
#include <vector>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "asc/dense/blas.h"
#include "indefinite_aasen_two_stage_fixture.h"
#include "indefinite_aasen_two_stage_native.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"
#if defined(ASC_AASEN_TWO_STAGE_SSYTRF_AA_2STAGE_HEADER)
#define ssytrf_aa_2stage_ emitted_ssytrf_aa_2stage
#include ASC_AASEN_TWO_STAGE_SSYTRF_AA_2STAGE_HEADER
#undef ssytrf_aa_2stage_
#endif
#if defined(ASC_AASEN_TWO_STAGE_DSYTRF_AA_2STAGE_HEADER)
#define dsytrf_aa_2stage_ emitted_dsytrf_aa_2stage
#include ASC_AASEN_TWO_STAGE_DSYTRF_AA_2STAGE_HEADER
#undef dsytrf_aa_2stage_
#endif
#if defined(ASC_AASEN_TWO_STAGE_CSYTRF_AA_2STAGE_HEADER)
#define csytrf_aa_2stage_ emitted_csytrf_aa_2stage
#include ASC_AASEN_TWO_STAGE_CSYTRF_AA_2STAGE_HEADER
#undef csytrf_aa_2stage_
#endif
#if defined(ASC_AASEN_TWO_STAGE_ZSYTRF_AA_2STAGE_HEADER)
#define zsytrf_aa_2stage_ emitted_zsytrf_aa_2stage
#include ASC_AASEN_TWO_STAGE_ZSYTRF_AA_2STAGE_HEADER
#undef zsytrf_aa_2stage_
#endif
#if defined(ASC_AASEN_TWO_STAGE_CHETRF_AA_2STAGE_HEADER)
#define chetrf_aa_2stage_ emitted_chetrf_aa_2stage
#include ASC_AASEN_TWO_STAGE_CHETRF_AA_2STAGE_HEADER
#undef chetrf_aa_2stage_
#endif
#if defined(ASC_AASEN_TWO_STAGE_ZHETRF_AA_2STAGE_HEADER)
#define zhetrf_aa_2stage_ emitted_zhetrf_aa_2stage
#include ASC_AASEN_TWO_STAGE_ZHETRF_AA_2STAGE_HEADER
#undef zhetrf_aa_2stage_
#endif
#include "../dense/test_support.h"
namespace {
#if defined(ASC_AASEN_TWO_STAGE_SSYTRF_AA_2STAGE_HEADER)
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
template <typename R, typename... Args>
struct Sig<R(Args...)> {
  using Type = R(typename Arg<Args>::Type...);
};
static_assert(
    std::is_same_v<typename Sig<decltype(LAPACK_ssytrf_aa_2stage_base)>::Type,
                   typename Sig<decltype(emitted_ssytrf_aa_2stage)>::Type>);
static_assert(
    std::is_same_v<typename Sig<decltype(LAPACK_dsytrf_aa_2stage_base)>::Type,
                   typename Sig<decltype(emitted_dsytrf_aa_2stage)>::Type>);
static_assert(
    std::is_same_v<typename Sig<decltype(LAPACK_csytrf_aa_2stage_base)>::Type,
                   typename Sig<decltype(emitted_csytrf_aa_2stage)>::Type>);
static_assert(
    std::is_same_v<typename Sig<decltype(LAPACK_zsytrf_aa_2stage_base)>::Type,
                   typename Sig<decltype(emitted_zsytrf_aa_2stage)>::Type>);
static_assert(
    std::is_same_v<typename Sig<decltype(LAPACK_chetrf_aa_2stage_base)>::Type,
                   typename Sig<decltype(emitted_chetrf_aa_2stage)>::Type>);
static_assert(
    std::is_same_v<typename Sig<decltype(LAPACK_zhetrf_aa_2stage_base)>::Type,
                   typename Sig<decltype(emitted_zhetrf_aa_2stage)>::Type>);
#endif
namespace aa = asc_aasen_two_stage_test;
namespace base = asc_indefinite_rook_test;
constexpr lapack_int kPivotGuard = std::numeric_limits<lapack_int>::max() - 71;

template <typename T>
void Probe(base::TestContext& test, bool he, char tri, int n, int band_mode,
           int work_mode, bool singular) {
  const int minimum = asc::DenseBlasComplex<T> && !he ? 0 : 1;
  const int ltb = std::max(minimum, n * std::array{4, 10, 577}[band_mode]);
  const int lwork = std::max(minimum, n * std::array{1, 3, 192}[work_mode]);
  aa::Sample<T> sample(n, he, tri == 'U', singular);
  std::vector<T> tb(static_cast<std::size_t>(ltb + 2), T{-73});
  std::vector<T> work(static_cast<std::size_t>(lwork + 2), T{-79});
  std::vector<lapack_int> p(static_cast<std::size_t>(n + 2), kPivotGuard);
  std::vector<lapack_int> q(p.size(), kPivotGuard);
  std::array<lapack_int, 3> info{
      kPivotGuard, std::numeric_limits<lapack_int>::min(), kPivotGuard};
  aa::Native(he, tri, n, sample.a.data() + 1, sample.lda, tb.data() + 1, ltb,
             p.data() + 1, q.data() + 1, work.data() + 1, lwork, info[1]);
  ASC_DENSE_TEST_EQ(test, info[1], singular ? n : 0);
  ASC_DENSE_TEST_EQ(test, info.front(), kPivotGuard);
  ASC_DENSE_TEST_EQ(test, info.back(), kPivotGuard);
  ASC_DENSE_TEST_EQ(test, tb.front(), T{-73});
  ASC_DENSE_TEST_EQ(test, tb.back(), T{-73});
  ASC_DENSE_TEST_EQ(test, work.front(), T{-79});
  ASC_DENSE_TEST_EQ(test, work.back(), T{-79});
  ASC_DENSE_TEST_EQ(test, p.front(), kPivotGuard);
  ASC_DENSE_TEST_EQ(test, p.back(), kPivotGuard);
  ASC_DENSE_TEST_EQ(test, q.front(), kPivotGuard);
  ASC_DENSE_TEST_EQ(test, q.back(), kPivotGuard);
  sample.Guards(test);
  if (n != 0) {
    const int nb = std::min({192, (ltb / n - 1) / 3, lwork / n});
    ASC_DENSE_TEST_EQ(test, tb[1], base::Value<T>(nb));
    aa::Reconstruction(test, n, he, tri == 'U', sample.a.data() + 1, sample.lda,
                       tb.data() + 1, ltb / n, nb, p.data() + 1, q.data() + 1,
                       sample.full);
    if (singular) {
      ASC_DENSE_TEST_EQ(test, tb[1 + (n - 1) * (ltb / n) + 2 * nb], T{});
    }
  } else {
    ASC_DENSE_TEST_CHECK(test, sample.a == sample.before);
    ASC_DENSE_TEST_EQ(test, tb[1], T{-73});
    ASC_DENSE_TEST_EQ(test, work[1], T{-79});
  }
  std::printf(
      "Two-stage ABI he=%d tri=%c n=%d band=%d work=%d singular=%d INFO=%lld\n",
      he, tri, n, band_mode, work_mode, singular,
      static_cast<long long>(info[1]));
}

template <typename T>
void Query(base::TestContext& test, bool he, char tri, int n, int query_mode) {
  using Real = asc::DenseBlasRealType<T>;
  const bool tb_query = query_mode != 1;
  const bool work_query = query_mode != 0;
  const bool complex_symmetric = asc::DenseBlasComplex<T> && !he;
  const int ltb = tb_query ? -1 : std::max(1, 4 * n);
  const int lwork = work_query ? -1 : std::max(1, n);
  T a{-83};
  std::array<T, 3> tb{T{-73}, T{-73}, T{-73}};
  std::array<T, 3> work{T{-79}, T{-79}, T{-79}};
  lapack_int p = kPivotGuard;
  lapack_int q = kPivotGuard;
  std::array<lapack_int, 3> info{
      kPivotGuard, std::numeric_limits<lapack_int>::min(), kPivotGuard};
  aa::Native(he, tri, n, &a, std::max(1, n), tb.data() + 1, ltb, &p, &q,
             work.data() + 1, lwork, info[1]);
  ASC_DENSE_TEST_EQ(test, info[1], 0);
  ASC_DENSE_TEST_EQ(test, info.front(), kPivotGuard);
  ASC_DENSE_TEST_EQ(test, info.back(), kPivotGuard);
  ASC_DENSE_TEST_EQ(test, a, T{-83});
  ASC_DENSE_TEST_EQ(test, p, kPivotGuard);
  ASC_DENSE_TEST_EQ(test, q, kPivotGuard);
  ASC_DENSE_TEST_EQ(test, tb.front(), T{-73});
  ASC_DENSE_TEST_EQ(test, tb.back(), T{-73});
  ASC_DENSE_TEST_EQ(test, work.front(), T{-79});
  ASC_DENSE_TEST_EQ(test, work.back(), T{-79});
  const int expected_tb = std::max(complex_symmetric ? 0 : 1, 577 * n);
  const int expected_work = std::max(complex_symmetric ? 0 : 1, 192 * n);
  if (tb_query) {
    const auto value = base::ToWide(tb[1]);
    ASC_DENSE_TEST_EQ(test, value.imag(), 0);
    ASC_DENSE_TEST_CHECK(test, value.real() >= expected_tb);
    ASC_DENSE_TEST_CHECK(
        test,
        value.real() <=
            expected_tb * (1.L + 2 * std::numeric_limits<Real>::epsilon()));
  } else {
    ASC_DENSE_TEST_EQ(test, tb[1], T{-73});
  }
  if (work_query) {
    const auto value = base::ToWide(work[1]);
    ASC_DENSE_TEST_EQ(test, value.imag(), 0);
    ASC_DENSE_TEST_CHECK(test, value.real() >= expected_work);
    ASC_DENSE_TEST_CHECK(
        test,
        value.real() <=
            expected_work * (1.L + 2 * std::numeric_limits<Real>::epsilon()));
  } else {
    ASC_DENSE_TEST_EQ(test, work[1], T{-79});
  }
  std::printf("Two-stage query he=%d tri=%c n=%d mode=%d TB=%.0Lf WORK=%.0Lf\n",
              he, tri, n, query_mode, base::ToWide(tb[1]).real(),
              base::ToWide(work[1]).real());
}

template <typename T>
void Run(base::TestContext& test, bool he, bool query, int& cases) {
  for (char tri : {'U', 'L'}) {
    if (query) {
      for (int n : {0, 1, 2, 7, 65, 193, 30001, 30003}) {
        for (int mode : {0, 1, 2}) {
          Query<T>(test, he, tri, n, mode);
          ++cases;
        }
      }
    } else {
      for (int band : {0, 1, 2}) {
        for (int work : {0, 1, 2}) {
          for (int n : {0, 1, 2, 7, 65, 193}) {
            Probe<T>(test, he, tri, n, band, work, false);
            ++cases;
          }
          for (int n : {1, 3, 7}) {
            Probe<T>(test, he, tri, n, band, work, true);
            ++cases;
          }
        }
      }
    }
  }
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 1 &&
      (argc != 2 || std::string_view(argv[1]) != "workspace_contract")) {
    return 2;
  }
  base::TestContext test;
  int cases = 0;
  Run<float>(test, false, argc == 2, cases);
  Run<double>(test, false, argc == 2, cases);
  Run<std::complex<float>>(test, false, argc == 2, cases);
  Run<std::complex<double>>(test, false, argc == 2, cases);
  Run<std::complex<float>>(test, true, argc == 2, cases);
  Run<std::complex<double>>(test, true, argc == 2, cases);
  std::printf("Two-stage native cases=%d\n", cases);
  return test.Finish();
}
