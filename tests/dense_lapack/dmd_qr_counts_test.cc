#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <type_traits>
#include <vector>

#include "../../src/dense/lapack/internal_dmd_qr_counts.h"
#include "../../src/dense/lapack/internal_tridiagonal.h"
#include "asc/core/types.h"
#include "installed_lu/normal_return_guard.h"

namespace {
template <typename T>
struct RealType {
  using type = T;
};
template <typename R>
struct RealType<std::complex<R>> {
  using type = R;
};
template <typename T>
using Real = typename RealType<T>::type;
template <typename T>
constexpr bool kComplex = !std::is_same_v<T, Real<T>>;
template <typename T>
struct Data {
  lapack_int m, n;
  std::vector<T> f, x, y, z, b, v, s, eigen;
  std::vector<Real<T>> imaginary, residual;
  std::array<T, 4> work{};
  std::array<Real<T>, 4> real{};
  std::array<lapack_int, 4> integer{};
  lapack_int rank = std::numeric_limits<lapack_int>::min();
  lapack_int info = std::numeric_limits<lapack_int>::min();
  Data(lapack_int rows, lapack_int columns)
      : m(rows),
        n(columns),
        f(m * n),
        x(m * n),
        y(m * n),
        z(m * n),
        b(m * n),
        v(m * n),
        s(m * n),
        eigen(n),
        imaginary(n),
        residual(n) {}
};
template <typename T>
void NativeQuery(Data<T>& d, char vectors, char extra, bool orthogonal,
                 lapack_int svd) {
  const char scaling = 'N';
  const char residuals = 'N';
  const char triangle = 'N';
  const char q = orthogonal ? 'Q' : 'N';
  const lapack_int rank = -1;
  const lapack_int count = -1;
  const Real<T> tolerance = 0;
#define REAL_CALL(P)                                                           \
  LAPACK_##P##gedmdq(                                                          \
      &scaling, &vectors, &residuals, &q, &triangle, &extra, &svd, &d.m, &d.n, \
      d.f.data(), &d.m, d.x.data(), &d.m, d.y.data(), &d.m, &rank, &tolerance, \
      &d.rank, d.eigen.data(), d.imaginary.data(), d.z.data(), &d.m,           \
      d.residual.data(), d.b.data(), &d.m, d.v.data(), &d.m, d.s.data(), &d.m, \
      d.work.data(), &count, d.integer.data(), &count, &d.info)
#define COMPLEX_CALL(P)                                                        \
  LAPACK_##P##gedmdq(                                                          \
      &scaling, &vectors, &residuals, &q, &triangle, &extra, &svd, &d.m, &d.n, \
      d.f.data(), &d.m, d.x.data(), &d.m, d.y.data(), &d.m, &rank, &tolerance, \
      &d.rank, d.eigen.data(), d.z.data(), &d.m, d.residual.data(),            \
      d.b.data(), &d.m, d.v.data(), &d.m, d.s.data(), &d.m, d.work.data(),     \
      &count, d.real.data(), &count, d.integer.data(), &count, &d.info)
  if constexpr (std::is_same_v<T, float>) {
    REAL_CALL(s);
  } else if constexpr (std::is_same_v<T, double>) {
    REAL_CALL(d);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    COMPLEX_CALL(c);
  } else {
    COMPLEX_CALL(z);
  }
#undef REAL_CALL
#undef COMPLEX_CALL
}
template <typename T>
bool Case(lapack_int m, lapack_int n, char vectors, char extra, bool orthogonal,
          lapack_int svd) {
  Data<T> d(m, n);
  const auto expected = asc::internal_dmd_qr_counts::Query<Real<T>>(
      m, n, static_cast<int>(svd), vectors, extra == 'E', orthogonal,
      kComplex<T>, std::numeric_limits<lapack_int>::max());
  if (!expected.ok()) {
    return false;
  }
  NativeQuery(d, vectors, extra, orthogonal, svd);
  // Compare finite floating encodings directly; do not narrow untrusted query
  // values to integers. The expected arithmetic independently checks bounds.
  const bool scalar =
      std::isfinite(std::real(d.work[0])) &&
      std::real(d.work[0]) == static_cast<Real<T>>(expected->query_scalar) &&
      std::isfinite(std::real(d.work[1])) &&
      std::real(d.work[1]) == static_cast<Real<T>>(expected->query_preferred);
  const bool real =
      !kComplex<T> || (std::isfinite(d.real[0]) &&
                       d.real[0] == static_cast<Real<T>>(expected->query_real));
  const bool ok =
      d.info == 0 && scalar && real && d.integer[0] == expected->integer;
  if (!ok) {
    std::printf(
        "GEDMDQ query complex=%d scalar_bytes=%zu m=%lld n=%lld vectors=%c "
        "extra=%c Q=%d svd=%lld INFO=%lld WORK=%.18Lg/%.18Lg "
        "expected=%lld/%lld RWORK=%.18Lg expected=%lld IWORK=%lld "
        "expected=%lld\n",
        static_cast<int>(kComplex<T>), sizeof(T), static_cast<long long>(m),
        static_cast<long long>(n), vectors, extra, static_cast<int>(orthogonal),
        static_cast<long long>(svd), static_cast<long long>(d.info),
        static_cast<long double>(std::real(d.work[0])),
        static_cast<long double>(std::real(d.work[1])),
        static_cast<long long>(expected->query_scalar),
        static_cast<long long>(expected->query_preferred),
        static_cast<long double>(d.real[0]),
        static_cast<long long>(expected->query_real),
        static_cast<long long>(d.integer[0]),
        static_cast<long long>(expected->integer));
  }
  return ok;
}
template <typename T>
bool Run() {
  bool ok = true;
  std::size_t cases = 0;
  for (lapack_int n : {2, 4, 17, 129}) {
    for (lapack_int m : {n, n + 5, 2 * n}) {
      for (lapack_int svd : {1, 2, 3, 4}) {
        for (char vectors : {'N', 'V', 'F', 'Q'}) {
          for (char extra : {'N', 'E'}) {
            for (bool orthogonal : {false, true}) {
              ok = Case<T>(m, n, vectors, extra, orthogonal, svd) && ok;
              ++cases;
            }
          }
        }
      }
    }
  }
  std::printf(
      "GEDMDQ exact queries complex=%d scalar_bytes=%zu integer_bytes=%zu "
      "cases=%zu passed=%d\n",
      static_cast<int>(kComplex<T>), sizeof(T), sizeof(lapack_int), cases,
      static_cast<int>(ok));
  return ok;
}
bool Boundaries() {
  namespace counts = asc::internal_dmd_qr_counts;
  const asc::extent_t lp = std::numeric_limits<std::int32_t>::max();
  const asc::extent_t ilp = std::numeric_limits<std::int64_t>::max();
  bool ok = true;
  for (auto limit : {lp, ilp}) {
    ok = !counts::Query<double>(3, 4, 1, 'V', false, true, false, limit).ok() &&
         ok;
    ok = !counts::Query<double>(4, 4, 0, 'V', false, true, false, limit).ok() &&
         ok;
    ok = !counts::Query<double>(4, 4, 1, 'X', false, true, false, limit).ok() &&
         ok;
    ok = !counts::Query<double>(limit, 2, 1, 'V', false, true, false, limit)
              .ok() &&
         ok;
    for (asc::extent_t n : {0, 1}) {
      const auto empty =
          counts::Query<double>(n, n, 1, 'N', false, false, false, limit);
      ok = empty.ok() && empty->scalar == 0 && empty->preferred == 0 && ok;
    }
  }
  ok = !counts::ExecutionBounds<double>(60000000, 40, 1, 'V', false, true,
                                        false, lp)
            .ok() &&
       ok;
  ok = counts::ExecutionBounds<double>(60000000, 40, 1, 'V', false, true, false,
                                       ilp)
           .ok() &&
       ok;
  return ok;
}
}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard normal_return;
  bool ok = Boundaries();
  ok = Run<float>() && ok;
  ok = Run<double>() && ok;
  ok = Run<std::complex<float>>() && ok;
  ok = Run<std::complex<double>>() && ok;
  return ok ? 0 : 1;
}
