#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <limits>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "../dense/test_support.h"
#include "asc/dense/blas.h"
#include "installed_lu/normal_return_guard.h"

namespace {
using asc_dense_test::TestContext;
template <typename T>
struct Probe {
  using Real = asc::DenseBlasRealType<T>;
  char uplo;
  char fact = 'N';
  const lapack_int n = 1;
  const lapack_int nrhs = 1;
  const lapack_int ld = 1;
  lapack_int lwork = 128;
  std::array<lapack_int, 3> info{101, -103, 107};
  std::array<lapack_int, 3> pivots{109, 1, 113};
  std::array<lapack_int, 3> integer{127, -131, 137};
  std::array<T, 3> a{T{-139}, T{2}, T{-149}};
  std::array<T, 3> af{T{-151}, T{2}, T{-157}};
  std::array<T, 3> b{T{-163}, T{6}, T{-167}};
  std::array<T, 3> x{T{-173}, T{1}, T{-179}};
  std::array<T, 130> work{};
  std::array<Real, 3> extra{Real{-181}, Real{}, Real{-191}};
  std::array<Real, 3> ferr{Real{-193}, Real{-197}, Real{-199}};
  std::array<Real, 3> berr{Real{-211}, Real{-223}, Real{-227}};
  std::array<Real, 3> condition{Real{-229}, Real{-233}, Real{-239}};
  const Real norm = 2;
  explicit Probe(char triangle) : uplo(triangle) { work.fill(T{-241}); }
  void Check(TestContext& test) const {
    ASC_DENSE_TEST_EQ(test, info.front(), 101);
    ASC_DENSE_TEST_EQ(test, info.back(), 107);
    ASC_DENSE_TEST_EQ(test, info[1], 0);
    ASC_DENSE_TEST_EQ(test, pivots.front(), 109);
    ASC_DENSE_TEST_EQ(test, pivots.back(), 113);
    ASC_DENSE_TEST_EQ(test, pivots[1], 1);
    ASC_DENSE_TEST_EQ(test, integer.front(), 127);
    ASC_DENSE_TEST_EQ(test, integer.back(), 137);
    ASC_DENSE_TEST_EQ(test, a.front(), T{-139});
    ASC_DENSE_TEST_EQ(test, a.back(), T{-149});
    ASC_DENSE_TEST_EQ(test, af.front(), T{-151});
    ASC_DENSE_TEST_EQ(test, af.back(), T{-157});
    ASC_DENSE_TEST_EQ(test, b.front(), T{-163});
    ASC_DENSE_TEST_EQ(test, b.back(), T{-167});
    ASC_DENSE_TEST_EQ(test, x.front(), T{-173});
    ASC_DENSE_TEST_EQ(test, x.back(), T{-179});
    ASC_DENSE_TEST_EQ(test, extra.front(), -181);
    ASC_DENSE_TEST_EQ(test, extra.back(), -191);
    ASC_DENSE_TEST_EQ(test, ferr.front(), -193);
    ASC_DENSE_TEST_EQ(test, ferr.back(), -199);
    ASC_DENSE_TEST_EQ(test, berr.front(), -211);
    ASC_DENSE_TEST_EQ(test, berr.back(), -227);
    ASC_DENSE_TEST_EQ(test, condition.front(), -229);
    ASC_DENSE_TEST_EQ(test, condition.back(), -239);
    ASC_DENSE_TEST_EQ(test, work.front(), T{-241});
    ASC_DENSE_TEST_EQ(test, work.back(), T{-241});
  }
};
void TestS(TestContext& test) {
  for (const char uplo : {'U', 'L'}) {
    Probe<float> v(uplo);
    LAPACK_ssycon(&v.uplo, &v.n, v.af.data() + 1, &v.ld, v.pivots.data() + 1,
                  &v.norm, v.condition.data() + 1, v.work.data() + 1,
                  v.integer.data() + 1, v.info.data() + 1);
    v.Check(test);
    ASC_DENSE_TEST_CHECK(test, std::abs(v.condition[1] - 1) <=
                                   16 * std::numeric_limits<float>::epsilon());
    LAPACK_ssyrfs(&v.uplo, &v.n, &v.nrhs, v.a.data() + 1, &v.ld,
                  v.af.data() + 1, &v.ld, v.pivots.data() + 1, v.b.data() + 1,
                  &v.ld, v.x.data() + 1, &v.ld, v.ferr.data() + 1,
                  v.berr.data() + 1, v.work.data() + 1, v.integer.data() + 1,
                  v.info.data() + 1);
    v.Check(test);
    ASC_DENSE_TEST_CHECK(test, std::abs(v.x[1] - float{3}) <=
                                   16 * std::numeric_limits<float>::epsilon());
    v.lwork = -1;
    LAPACK_ssysv(&v.uplo, &v.n, &v.nrhs, v.a.data() + 1, &v.ld,
                 v.pivots.data() + 1, v.b.data() + 1, &v.ld, v.work.data() + 1,
                 &v.lwork, v.info.data() + 1);
    v.Check(test);
    ASC_DENSE_TEST_EQ(test, v.b[1], float{6});
    ASC_DENSE_TEST_EQ(test, v.work[1], float{64});
    v.lwork = 128;
    LAPACK_ssysv(&v.uplo, &v.n, &v.nrhs, v.a.data() + 1, &v.ld,
                 v.pivots.data() + 1, v.b.data() + 1, &v.ld, v.work.data() + 1,
                 &v.lwork, v.info.data() + 1);
    v.Check(test);
    ASC_DENSE_TEST_EQ(test, v.b[1], float{3});
    v.b[1] = float{6};
    for (const char fact : {'N', 'F'}) {
      v.fact = fact;
      v.lwork = -1;
      LAPACK_ssysvx(&v.fact, &v.uplo, &v.n, &v.nrhs, v.a.data() + 1, &v.ld,
                    v.af.data() + 1, &v.ld, v.pivots.data() + 1, v.b.data() + 1,
                    &v.ld, v.x.data() + 1, &v.ld, v.condition.data() + 1,
                    v.ferr.data() + 1, v.berr.data() + 1, v.work.data() + 1,
                    &v.lwork, v.integer.data() + 1, v.info.data() + 1);
      v.Check(test);
      const float preferred = fact == 'N' ? 64 : 3;
      ASC_DENSE_TEST_EQ(test, v.work[1], float{preferred});
      v.lwork = 128;
      LAPACK_ssysvx(&v.fact, &v.uplo, &v.n, &v.nrhs, v.a.data() + 1, &v.ld,
                    v.af.data() + 1, &v.ld, v.pivots.data() + 1, v.b.data() + 1,
                    &v.ld, v.x.data() + 1, &v.ld, v.condition.data() + 1,
                    v.ferr.data() + 1, v.berr.data() + 1, v.work.data() + 1,
                    &v.lwork, v.integer.data() + 1, v.info.data() + 1);
      v.Check(test);
      ASC_DENSE_TEST_CHECK(test,
                           std::abs(v.x[1] - float{3}) <=
                               16 * std::numeric_limits<float>::epsilon());
    }
  }
}
void TestD(TestContext& test) {
  for (const char uplo : {'U', 'L'}) {
    Probe<double> v(uplo);
    LAPACK_dsycon(&v.uplo, &v.n, v.af.data() + 1, &v.ld, v.pivots.data() + 1,
                  &v.norm, v.condition.data() + 1, v.work.data() + 1,
                  v.integer.data() + 1, v.info.data() + 1);
    v.Check(test);
    ASC_DENSE_TEST_CHECK(test, std::abs(v.condition[1] - 1) <=
                                   16 * std::numeric_limits<double>::epsilon());
    LAPACK_dsyrfs(&v.uplo, &v.n, &v.nrhs, v.a.data() + 1, &v.ld,
                  v.af.data() + 1, &v.ld, v.pivots.data() + 1, v.b.data() + 1,
                  &v.ld, v.x.data() + 1, &v.ld, v.ferr.data() + 1,
                  v.berr.data() + 1, v.work.data() + 1, v.integer.data() + 1,
                  v.info.data() + 1);
    v.Check(test);
    ASC_DENSE_TEST_CHECK(test, std::abs(v.x[1] - double{3}) <=
                                   16 * std::numeric_limits<double>::epsilon());
    v.lwork = -1;
    LAPACK_dsysv(&v.uplo, &v.n, &v.nrhs, v.a.data() + 1, &v.ld,
                 v.pivots.data() + 1, v.b.data() + 1, &v.ld, v.work.data() + 1,
                 &v.lwork, v.info.data() + 1);
    v.Check(test);
    ASC_DENSE_TEST_EQ(test, v.b[1], double{6});
    ASC_DENSE_TEST_EQ(test, v.work[1], double{64});
    v.lwork = 128;
    LAPACK_dsysv(&v.uplo, &v.n, &v.nrhs, v.a.data() + 1, &v.ld,
                 v.pivots.data() + 1, v.b.data() + 1, &v.ld, v.work.data() + 1,
                 &v.lwork, v.info.data() + 1);
    v.Check(test);
    ASC_DENSE_TEST_EQ(test, v.b[1], double{3});
    v.b[1] = double{6};
    for (const char fact : {'N', 'F'}) {
      v.fact = fact;
      v.lwork = -1;
      LAPACK_dsysvx(&v.fact, &v.uplo, &v.n, &v.nrhs, v.a.data() + 1, &v.ld,
                    v.af.data() + 1, &v.ld, v.pivots.data() + 1, v.b.data() + 1,
                    &v.ld, v.x.data() + 1, &v.ld, v.condition.data() + 1,
                    v.ferr.data() + 1, v.berr.data() + 1, v.work.data() + 1,
                    &v.lwork, v.integer.data() + 1, v.info.data() + 1);
      v.Check(test);
      const double preferred = fact == 'N' ? 64 : 3;
      ASC_DENSE_TEST_EQ(test, v.work[1], double{preferred});
      v.lwork = 128;
      LAPACK_dsysvx(&v.fact, &v.uplo, &v.n, &v.nrhs, v.a.data() + 1, &v.ld,
                    v.af.data() + 1, &v.ld, v.pivots.data() + 1, v.b.data() + 1,
                    &v.ld, v.x.data() + 1, &v.ld, v.condition.data() + 1,
                    v.ferr.data() + 1, v.berr.data() + 1, v.work.data() + 1,
                    &v.lwork, v.integer.data() + 1, v.info.data() + 1);
      v.Check(test);
      ASC_DENSE_TEST_CHECK(test,
                           std::abs(v.x[1] - double{3}) <=
                               16 * std::numeric_limits<double>::epsilon());
    }
  }
}
void TestC(TestContext& test) {
  for (const char uplo : {'U', 'L'}) {
    Probe<std::complex<float>> v(uplo);
    LAPACK_csycon(&v.uplo, &v.n, v.af.data() + 1, &v.ld, v.pivots.data() + 1,
                  &v.norm, v.condition.data() + 1, v.work.data() + 1,
                  v.info.data() + 1);
    v.Check(test);
    ASC_DENSE_TEST_CHECK(test, std::abs(v.condition[1] - 1) <=
                                   16 * std::numeric_limits<float>::epsilon());
    LAPACK_csyrfs(&v.uplo, &v.n, &v.nrhs, v.a.data() + 1, &v.ld,
                  v.af.data() + 1, &v.ld, v.pivots.data() + 1, v.b.data() + 1,
                  &v.ld, v.x.data() + 1, &v.ld, v.ferr.data() + 1,
                  v.berr.data() + 1, v.work.data() + 1, v.extra.data() + 1,
                  v.info.data() + 1);
    v.Check(test);
    ASC_DENSE_TEST_CHECK(test, std::abs(v.x[1] - std::complex<float>{3}) <=
                                   16 * std::numeric_limits<float>::epsilon());
    v.lwork = -1;
    LAPACK_csysv(&v.uplo, &v.n, &v.nrhs, v.a.data() + 1, &v.ld,
                 v.pivots.data() + 1, v.b.data() + 1, &v.ld, v.work.data() + 1,
                 &v.lwork, v.info.data() + 1);
    v.Check(test);
    ASC_DENSE_TEST_EQ(test, v.b[1], std::complex<float>{6});
    ASC_DENSE_TEST_EQ(test, v.work[1], std::complex<float>{64});
    v.lwork = 128;
    LAPACK_csysv(&v.uplo, &v.n, &v.nrhs, v.a.data() + 1, &v.ld,
                 v.pivots.data() + 1, v.b.data() + 1, &v.ld, v.work.data() + 1,
                 &v.lwork, v.info.data() + 1);
    v.Check(test);
    ASC_DENSE_TEST_EQ(test, v.b[1], std::complex<float>{3});
    v.b[1] = std::complex<float>{6};
    for (const char fact : {'N', 'F'}) {
      v.fact = fact;
      v.lwork = -1;
      LAPACK_csysvx(&v.fact, &v.uplo, &v.n, &v.nrhs, v.a.data() + 1, &v.ld,
                    v.af.data() + 1, &v.ld, v.pivots.data() + 1, v.b.data() + 1,
                    &v.ld, v.x.data() + 1, &v.ld, v.condition.data() + 1,
                    v.ferr.data() + 1, v.berr.data() + 1, v.work.data() + 1,
                    &v.lwork, v.extra.data() + 1, v.info.data() + 1);
      v.Check(test);
      const float preferred = fact == 'N' ? 64 : 2;
      ASC_DENSE_TEST_EQ(test, v.work[1], std::complex<float>{preferred});
      v.lwork = 128;
      LAPACK_csysvx(&v.fact, &v.uplo, &v.n, &v.nrhs, v.a.data() + 1, &v.ld,
                    v.af.data() + 1, &v.ld, v.pivots.data() + 1, v.b.data() + 1,
                    &v.ld, v.x.data() + 1, &v.ld, v.condition.data() + 1,
                    v.ferr.data() + 1, v.berr.data() + 1, v.work.data() + 1,
                    &v.lwork, v.extra.data() + 1, v.info.data() + 1);
      v.Check(test);
      ASC_DENSE_TEST_CHECK(test,
                           std::abs(v.x[1] - std::complex<float>{3}) <=
                               16 * std::numeric_limits<float>::epsilon());
    }
  }
}
void TestCH(TestContext& test) {
  for (const char uplo : {'U', 'L'}) {
    Probe<std::complex<float>> v(uplo);
    LAPACK_checon(&v.uplo, &v.n, v.af.data() + 1, &v.ld, v.pivots.data() + 1,
                  &v.norm, v.condition.data() + 1, v.work.data() + 1,
                  v.info.data() + 1);
    v.Check(test);
    ASC_DENSE_TEST_CHECK(test, std::abs(v.condition[1] - 1) <=
                                   16 * std::numeric_limits<float>::epsilon());
    LAPACK_cherfs(&v.uplo, &v.n, &v.nrhs, v.a.data() + 1, &v.ld,
                  v.af.data() + 1, &v.ld, v.pivots.data() + 1, v.b.data() + 1,
                  &v.ld, v.x.data() + 1, &v.ld, v.ferr.data() + 1,
                  v.berr.data() + 1, v.work.data() + 1, v.extra.data() + 1,
                  v.info.data() + 1);
    v.Check(test);
    ASC_DENSE_TEST_CHECK(test, std::abs(v.x[1] - std::complex<float>{3}) <=
                                   16 * std::numeric_limits<float>::epsilon());
    v.lwork = -1;
    LAPACK_chesv(&v.uplo, &v.n, &v.nrhs, v.a.data() + 1, &v.ld,
                 v.pivots.data() + 1, v.b.data() + 1, &v.ld, v.work.data() + 1,
                 &v.lwork, v.info.data() + 1);
    v.Check(test);
    ASC_DENSE_TEST_EQ(test, v.b[1], std::complex<float>{6});
    ASC_DENSE_TEST_EQ(test, v.work[1], std::complex<float>{64});
    v.lwork = 128;
    LAPACK_chesv(&v.uplo, &v.n, &v.nrhs, v.a.data() + 1, &v.ld,
                 v.pivots.data() + 1, v.b.data() + 1, &v.ld, v.work.data() + 1,
                 &v.lwork, v.info.data() + 1);
    v.Check(test);
    ASC_DENSE_TEST_EQ(test, v.b[1], std::complex<float>{3});
    v.b[1] = std::complex<float>{6};
    for (const char fact : {'N', 'F'}) {
      v.fact = fact;
      v.lwork = -1;
      LAPACK_chesvx(&v.fact, &v.uplo, &v.n, &v.nrhs, v.a.data() + 1, &v.ld,
                    v.af.data() + 1, &v.ld, v.pivots.data() + 1, v.b.data() + 1,
                    &v.ld, v.x.data() + 1, &v.ld, v.condition.data() + 1,
                    v.ferr.data() + 1, v.berr.data() + 1, v.work.data() + 1,
                    &v.lwork, v.extra.data() + 1, v.info.data() + 1);
      v.Check(test);
      const float preferred = fact == 'N' ? 64 : 2;
      ASC_DENSE_TEST_EQ(test, v.work[1], std::complex<float>{preferred});
      v.lwork = 128;
      LAPACK_chesvx(&v.fact, &v.uplo, &v.n, &v.nrhs, v.a.data() + 1, &v.ld,
                    v.af.data() + 1, &v.ld, v.pivots.data() + 1, v.b.data() + 1,
                    &v.ld, v.x.data() + 1, &v.ld, v.condition.data() + 1,
                    v.ferr.data() + 1, v.berr.data() + 1, v.work.data() + 1,
                    &v.lwork, v.extra.data() + 1, v.info.data() + 1);
      v.Check(test);
      ASC_DENSE_TEST_CHECK(test,
                           std::abs(v.x[1] - std::complex<float>{3}) <=
                               16 * std::numeric_limits<float>::epsilon());
    }
  }
}
void TestZ(TestContext& test) {
  for (const char uplo : {'U', 'L'}) {
    Probe<std::complex<double>> v(uplo);
    LAPACK_zsycon(&v.uplo, &v.n, v.af.data() + 1, &v.ld, v.pivots.data() + 1,
                  &v.norm, v.condition.data() + 1, v.work.data() + 1,
                  v.info.data() + 1);
    v.Check(test);
    ASC_DENSE_TEST_CHECK(test, std::abs(v.condition[1] - 1) <=
                                   16 * std::numeric_limits<double>::epsilon());
    LAPACK_zsyrfs(&v.uplo, &v.n, &v.nrhs, v.a.data() + 1, &v.ld,
                  v.af.data() + 1, &v.ld, v.pivots.data() + 1, v.b.data() + 1,
                  &v.ld, v.x.data() + 1, &v.ld, v.ferr.data() + 1,
                  v.berr.data() + 1, v.work.data() + 1, v.extra.data() + 1,
                  v.info.data() + 1);
    v.Check(test);
    ASC_DENSE_TEST_CHECK(test, std::abs(v.x[1] - std::complex<double>{3}) <=
                                   16 * std::numeric_limits<double>::epsilon());
    v.lwork = -1;
    LAPACK_zsysv(&v.uplo, &v.n, &v.nrhs, v.a.data() + 1, &v.ld,
                 v.pivots.data() + 1, v.b.data() + 1, &v.ld, v.work.data() + 1,
                 &v.lwork, v.info.data() + 1);
    v.Check(test);
    ASC_DENSE_TEST_EQ(test, v.b[1], std::complex<double>{6});
    ASC_DENSE_TEST_EQ(test, v.work[1], std::complex<double>{64});
    v.lwork = 128;
    LAPACK_zsysv(&v.uplo, &v.n, &v.nrhs, v.a.data() + 1, &v.ld,
                 v.pivots.data() + 1, v.b.data() + 1, &v.ld, v.work.data() + 1,
                 &v.lwork, v.info.data() + 1);
    v.Check(test);
    ASC_DENSE_TEST_EQ(test, v.b[1], std::complex<double>{3});
    v.b[1] = std::complex<double>{6};
    for (const char fact : {'N', 'F'}) {
      v.fact = fact;
      v.lwork = -1;
      LAPACK_zsysvx(&v.fact, &v.uplo, &v.n, &v.nrhs, v.a.data() + 1, &v.ld,
                    v.af.data() + 1, &v.ld, v.pivots.data() + 1, v.b.data() + 1,
                    &v.ld, v.x.data() + 1, &v.ld, v.condition.data() + 1,
                    v.ferr.data() + 1, v.berr.data() + 1, v.work.data() + 1,
                    &v.lwork, v.extra.data() + 1, v.info.data() + 1);
      v.Check(test);
      const double preferred = fact == 'N' ? 64 : 2;
      ASC_DENSE_TEST_EQ(test, v.work[1], std::complex<double>{preferred});
      v.lwork = 128;
      LAPACK_zsysvx(&v.fact, &v.uplo, &v.n, &v.nrhs, v.a.data() + 1, &v.ld,
                    v.af.data() + 1, &v.ld, v.pivots.data() + 1, v.b.data() + 1,
                    &v.ld, v.x.data() + 1, &v.ld, v.condition.data() + 1,
                    v.ferr.data() + 1, v.berr.data() + 1, v.work.data() + 1,
                    &v.lwork, v.extra.data() + 1, v.info.data() + 1);
      v.Check(test);
      ASC_DENSE_TEST_CHECK(test,
                           std::abs(v.x[1] - std::complex<double>{3}) <=
                               16 * std::numeric_limits<double>::epsilon());
    }
  }
}
void TestZH(TestContext& test) {
  for (const char uplo : {'U', 'L'}) {
    Probe<std::complex<double>> v(uplo);
    LAPACK_zhecon(&v.uplo, &v.n, v.af.data() + 1, &v.ld, v.pivots.data() + 1,
                  &v.norm, v.condition.data() + 1, v.work.data() + 1,
                  v.info.data() + 1);
    v.Check(test);
    ASC_DENSE_TEST_CHECK(test, std::abs(v.condition[1] - 1) <=
                                   16 * std::numeric_limits<double>::epsilon());
    LAPACK_zherfs(&v.uplo, &v.n, &v.nrhs, v.a.data() + 1, &v.ld,
                  v.af.data() + 1, &v.ld, v.pivots.data() + 1, v.b.data() + 1,
                  &v.ld, v.x.data() + 1, &v.ld, v.ferr.data() + 1,
                  v.berr.data() + 1, v.work.data() + 1, v.extra.data() + 1,
                  v.info.data() + 1);
    v.Check(test);
    ASC_DENSE_TEST_CHECK(test, std::abs(v.x[1] - std::complex<double>{3}) <=
                                   16 * std::numeric_limits<double>::epsilon());
    v.lwork = -1;
    LAPACK_zhesv(&v.uplo, &v.n, &v.nrhs, v.a.data() + 1, &v.ld,
                 v.pivots.data() + 1, v.b.data() + 1, &v.ld, v.work.data() + 1,
                 &v.lwork, v.info.data() + 1);
    v.Check(test);
    ASC_DENSE_TEST_EQ(test, v.b[1], std::complex<double>{6});
    ASC_DENSE_TEST_EQ(test, v.work[1], std::complex<double>{64});
    v.lwork = 128;
    LAPACK_zhesv(&v.uplo, &v.n, &v.nrhs, v.a.data() + 1, &v.ld,
                 v.pivots.data() + 1, v.b.data() + 1, &v.ld, v.work.data() + 1,
                 &v.lwork, v.info.data() + 1);
    v.Check(test);
    ASC_DENSE_TEST_EQ(test, v.b[1], std::complex<double>{3});
    v.b[1] = std::complex<double>{6};
    for (const char fact : {'N', 'F'}) {
      v.fact = fact;
      v.lwork = -1;
      LAPACK_zhesvx(&v.fact, &v.uplo, &v.n, &v.nrhs, v.a.data() + 1, &v.ld,
                    v.af.data() + 1, &v.ld, v.pivots.data() + 1, v.b.data() + 1,
                    &v.ld, v.x.data() + 1, &v.ld, v.condition.data() + 1,
                    v.ferr.data() + 1, v.berr.data() + 1, v.work.data() + 1,
                    &v.lwork, v.extra.data() + 1, v.info.data() + 1);
      v.Check(test);
      const double preferred = fact == 'N' ? 64 : 2;
      ASC_DENSE_TEST_EQ(test, v.work[1], std::complex<double>{preferred});
      v.lwork = 128;
      LAPACK_zhesvx(&v.fact, &v.uplo, &v.n, &v.nrhs, v.a.data() + 1, &v.ld,
                    v.af.data() + 1, &v.ld, v.pivots.data() + 1, v.b.data() + 1,
                    &v.ld, v.x.data() + 1, &v.ld, v.condition.data() + 1,
                    v.ferr.data() + 1, v.berr.data() + 1, v.work.data() + 1,
                    &v.lwork, v.extra.data() + 1, v.info.data() + 1);
      v.Check(test);
      ASC_DENSE_TEST_CHECK(test,
                           std::abs(v.x[1] - std::complex<double>{3}) <=
                               16 * std::numeric_limits<double>::epsilon());
    }
  }
}
}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  TestContext test;
  TestS(test);
  TestD(test);
  TestC(test);
  TestCH(test);
  TestZ(test);
  TestZH(test);
  std::printf(
      "24 exact typed expert routines; 96 guarded foreign calls; integer "
      "bits=%zu\n",
      sizeof(lapack_int) * 8);
  return test.Finish();
}
