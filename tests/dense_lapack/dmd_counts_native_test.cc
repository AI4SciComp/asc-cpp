#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <optional>
#include <type_traits>
#include <vector>

#include "../../src/dense/lapack/internal_dmd_eigen_query_counts.h"
#include "../../src/dense/lapack/internal_dmd_query_counts.h"
#include "../../src/dense/lapack/internal_dmd_svd_query_counts.h"
#include "../../src/dense/lapack/internal_tridiagonal.h"
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
constexpr bool kIsComplex = !std::is_same_v<T, Real<T>>;
template <typename T>
struct Data {
  lapack_int m, n;
  std::vector<T> x, y, z, b, w, s, eigen;
  std::vector<Real<T>> imaginary, residual;
  std::array<T, 4> scalar{};
  std::array<Real<T>, 4> real{};
  std::array<lapack_int, 4> integer{};
  lapack_int rank = std::numeric_limits<lapack_int>::min();
  lapack_int info = std::numeric_limits<lapack_int>::min();
  Data(lapack_int rows, lapack_int columns)
      : m(rows),
        n(columns),
        x(m * n),
        y(m * n),
        z(m * n),
        b(m * n),
        w(m * n),
        s(m * n),
        eigen(n),
        imaginary(n),
        residual(n) {}
};
template <typename T>
void Query(char vectors, char extra, lapack_int svd, Data<T>& d) {
  const char scaling = 'N';
  const char residuals = 'N';
  const lapack_int rank = -1;
  const lapack_int m = d.m;
  const lapack_int n = d.n;
  const lapack_int ld = d.m;
  const lapack_int scalar_count = -1;
  const lapack_int real_count = -1;
  const lapack_int integer_count = -1;
  const auto tolerance = Real<T>{32} * std::numeric_limits<Real<T>>::epsilon();
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_sgedmd(&scaling, &vectors, &residuals, &extra, &svd, &m, &n,
                  d.x.data(), &ld, d.y.data(), &ld, &rank, &tolerance, &d.rank,
                  d.eigen.data(), d.imaginary.data(), d.z.data(), &ld,
                  d.residual.data(), d.b.data(), &ld, d.w.data(), &ld,
                  d.s.data(), &ld, d.scalar.data(), &scalar_count,
                  d.integer.data(), &integer_count, &d.info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dgedmd(&scaling, &vectors, &residuals, &extra, &svd, &m, &n,
                  d.x.data(), &ld, d.y.data(), &ld, &rank, &tolerance, &d.rank,
                  d.eigen.data(), d.imaginary.data(), d.z.data(), &ld,
                  d.residual.data(), d.b.data(), &ld, d.w.data(), &ld,
                  d.s.data(), &ld, d.scalar.data(), &scalar_count,
                  d.integer.data(), &integer_count, &d.info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    LAPACK_cgedmd(&scaling, &vectors, &residuals, &extra, &svd, &m, &n,
                  d.x.data(), &ld, d.y.data(), &ld, &rank, &tolerance, &d.rank,
                  d.eigen.data(), d.z.data(), &ld, d.residual.data(),
                  d.b.data(), &ld, d.w.data(), &ld, d.s.data(), &ld,
                  d.scalar.data(), &scalar_count, d.real.data(), &real_count,
                  d.integer.data(), &integer_count, &d.info);
  } else {
    LAPACK_zgedmd(&scaling, &vectors, &residuals, &extra, &svd, &m, &n,
                  d.x.data(), &ld, d.y.data(), &ld, &rank, &tolerance, &d.rank,
                  d.eigen.data(), d.z.data(), &ld, d.residual.data(),
                  d.b.data(), &ld, d.w.data(), &ld, d.s.data(), &ld,
                  d.scalar.data(), &scalar_count, d.real.data(), &real_count,
                  d.integer.data(), &integer_count, &d.info);
  }
}
template <typename T>
std::optional<lapack_int> DecodeCount(T value) {
  const auto real = std::real(value);
  const auto bound =
      std::ldexp(Real<T>{1}, std::numeric_limits<lapack_int>::digits);
  if (std::imag(value) != 0 || !std::isfinite(real) || real < 1 ||
      real >= bound || std::trunc(real) != real) {
    return std::nullopt;
  }
  return static_cast<lapack_int>(real);
}

template <typename T>
bool DecodeControls() {
  using R = Real<T>;
  for (const R value :
       {R{0}, R{-1}, R{1.5}, std::numeric_limits<R>::quiet_NaN(),
        std::numeric_limits<R>::infinity(),
        std::ldexp(R{1}, std::numeric_limits<lapack_int>::digits)}) {
    if (DecodeCount(T{value}).has_value()) {
      return false;
    }
  }
  if constexpr (kIsComplex<T>) {
    if (DecodeCount(T{1, 1}).has_value()) {
      return false;
    }
  }
  return DecodeCount(T{1}) == 1 && DecodeCount(T{2}) == 2;
}

template <typename T>
void QuerySvd(bool overwrite_input, Data<T>& d) {
  const char jobu = overwrite_input ? 'O' : 'S';
  const char jobvt = overwrite_input ? 'S' : 'O';
  const lapack_int count = -1;
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_sgesvd(&jobu, &jobvt, &d.m, &d.n, d.x.data(), &d.m,
                  d.residual.data(), d.z.data(), &d.m, d.w.data(), &d.m,
                  d.scalar.data(), &count, &d.info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dgesvd(&jobu, &jobvt, &d.m, &d.n, d.x.data(), &d.m,
                  d.residual.data(), d.z.data(), &d.m, d.w.data(), &d.m,
                  d.scalar.data(), &count, &d.info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    std::vector<Real<T>> real_work(5 * d.n);
    LAPACK_cgesvd(&jobu, &jobvt, &d.m, &d.n, d.x.data(), &d.m,
                  d.residual.data(), d.z.data(), &d.m, d.w.data(), &d.m,
                  d.scalar.data(), &count, real_work.data(), &d.info);
  } else if constexpr (std::is_same_v<T, std::complex<double>>) {
    std::vector<Real<T>> real_work(5 * d.n);
    LAPACK_zgesvd(&jobu, &jobvt, &d.m, &d.n, d.x.data(), &d.m,
                  d.residual.data(), d.z.data(), &d.m, d.w.data(), &d.m,
                  d.scalar.data(), &count, real_work.data(), &d.info);
  }
}

template <typename T>
int SvdProbe(const char* scalar_name) {
  int failures = 0;
  int cases = 0;
  for (lapack_int n : {1, 3, 16, 128}) {
    for (lapack_int extra_rows : {0, 5}) {
      for (const bool overwrite : {false, true}) {
        if (!overwrite && extra_rows != 0) {
          continue;
        }
        Data<T> data(n + extra_rows, n);
        const auto counts = asc::internal_dmd_counts::SvdQuery<Real<T>>(
            data.m, data.n, kIsComplex<T>, overwrite,
            std::numeric_limits<lapack_int>::max());
        if (!counts.ok()) {
          std::abort();
        }
        QuerySvd(overwrite, data);
        const auto actual = DecodeCount(data.scalar[0]);
        const bool okay =
            data.info == 0 && actual && *actual == counts->returned_preferred;
        std::printf(
            "%s GESVD m=%lld n=%lld overwrite_input=%d raw=%lld "
            "returned=%lld info=%lld %s\n",
            scalar_name, static_cast<long long>(data.m),
            static_cast<long long>(n), overwrite,
            static_cast<long long>(counts->raw_preferred),
            static_cast<long long>(actual.value_or(-1)),
            static_cast<long long>(data.info), okay ? "PASS" : "FAIL");
        failures += !okay;
        ++cases;
      }
    }
  }
  std::printf("%s GESVD cases=%d failures=%d\n", scalar_name, cases, failures);
  return failures;
}

template <typename T>
void QueryEigen(bool vectors, Data<T>& d) {
  const char left = 'N';
  const char right = vectors ? 'V' : 'N';
  const lapack_int count = -1;
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_sgeev(&left, &right, &d.n, d.x.data(), &d.m, d.eigen.data(),
                 d.imaginary.data(), d.z.data(), &d.m, d.w.data(), &d.m,
                 d.scalar.data(), &count, &d.info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dgeev(&left, &right, &d.n, d.x.data(), &d.m, d.eigen.data(),
                 d.imaginary.data(), d.z.data(), &d.m, d.w.data(), &d.m,
                 d.scalar.data(), &count, &d.info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    std::vector<Real<T>> real_work(2 * d.n);
    LAPACK_cgeev(&left, &right, &d.n, d.x.data(), &d.m, d.eigen.data(),
                 d.z.data(), &d.m, d.w.data(), &d.m, d.scalar.data(), &count,
                 real_work.data(), &d.info);
  } else if constexpr (std::is_same_v<T, std::complex<double>>) {
    std::vector<Real<T>> real_work(2 * d.n);
    LAPACK_zgeev(&left, &right, &d.n, d.x.data(), &d.m, d.eigen.data(),
                 d.z.data(), &d.m, d.w.data(), &d.m, d.scalar.data(), &count,
                 real_work.data(), &d.info);
  }
}

template <typename T>
int EigenProbe(const char* scalar_name) {
  int failures = 0;
  int cases = 0;
  // Tiny/window and default-REAL logarithmic transitions use real containing
  // matrices. The remaining large thresholds are pure arithmetic tests.
  for (lapack_int n : {1, 15, 16, 30, 128, 180, 182, 589, 590}) {
    for (const bool vectors : {false, true}) {
      Data<T> data(n, n);
      const auto counts = asc::internal_dmd_counts::EigenQuery<Real<T>>(
          n, kIsComplex<T>, vectors, std::numeric_limits<lapack_int>::max());
      if (!counts.ok()) {
        std::abort();
      }
      QueryEigen(vectors, data);
      const auto actual = DecodeCount(data.scalar[0]);
      const bool okay =
          data.info == 0 && actual && *actual == counts->returned_preferred;
      std::printf(
          "%s GEEV n=%lld vectors=%d raw=%lld returned=%lld "
          "info=%lld %s\n",
          scalar_name, static_cast<long long>(n), vectors,
          static_cast<long long>(counts->raw_preferred),
          static_cast<long long>(actual.value_or(-1)),
          static_cast<long long>(data.info), okay ? "PASS" : "FAIL");
      failures += !okay;
      ++cases;
    }
  }
  std::printf("%s GEEV cases=%d failures=%d\n", scalar_name, cases, failures);
  return failures;
}

template <typename T>
int Probe(const char* scalar_name) {
  int failures = (DecodeControls<T>() ? 0 : 1) + SvdProbe<T>(scalar_name) +
                 EigenProbe<T>(scalar_name);
  int cases = 0;
  for (lapack_int n : {1, 3, 16, 128}) {
    // Include both GESDD tall crossovers, not only square and near-square.
    for (lapack_int extra_rows : {lapack_int{0}, lapack_int{5}, 3 * n / 4, n}) {
      const lapack_int m = n + extra_rows;
      for (lapack_int svd = 1; svd <= 4; ++svd) {
        for (const auto mode :
             {std::array{'N', 'N'}, std::array{'V', 'N'}, std::array{'F', 'N'},
              std::array{'N', 'E'}, std::array{'N', 'R'}}) {
          Data<T> data(m, n);
          Query(mode[0], mode[1], svd, data);
          const auto counts = asc::internal_dmd_counts::Query<Real<T>>(
              m, n, static_cast<int>(svd), mode[0] != 'N' || mode[1] == 'E',
              kIsComplex<T>, std::numeric_limits<lapack_int>::max());
          if (!counts.ok()) {
            std::abort();
          }
          const std::array<lapack_int, 3> expected{
              static_cast<lapack_int>(counts->minimum.query_scalar),
              static_cast<lapack_int>(counts->minimum.query_real),
              static_cast<lapack_int>(counts->minimum.integer)};
          const auto scalar_count = DecodeCount(data.scalar[0]);
          const auto real_count = kIsComplex<T> ? DecodeCount(data.real[0])
                                                : std::optional<lapack_int>{0};
          const auto preferred = DecodeCount(data.scalar[1]);
          if (!scalar_count || !real_count || !preferred) {
            std::printf("%s invalid native workspace encoding\n", scalar_name);
            ++failures;
            ++cases;
            continue;
          }
          const std::array<lapack_int, 3> actual{*scalar_count, *real_count,
                                                 data.integer[0]};
          const bool okay = data.info == 0 && actual == expected &&
                            *preferred == counts->query_preferred;
          std::printf(
              "%s m=%lld n=%lld svd=%lld modes=%c%c minimum=%lld,%lld,%lld "
              "expected=%lld,%lld,%lld preferred=%lld expected_preferred=%lld "
              "info=%lld %s\n",
              scalar_name, static_cast<long long>(m), static_cast<long long>(n),
              static_cast<long long>(svd), mode[0], mode[1],
              static_cast<long long>(actual[0]),
              static_cast<long long>(actual[1]),
              static_cast<long long>(actual[2]),
              static_cast<long long>(expected[0]),
              static_cast<long long>(expected[1]),
              static_cast<long long>(expected[2]),
              static_cast<long long>(*preferred),
              static_cast<long long>(counts->query_preferred),
              static_cast<long long>(data.info), okay ? "PASS" : "FAIL");
          failures += !okay;
          ++cases;
        }
      }
    }
  }
  std::printf("%s cases=%d failures=%d\n", scalar_name, cases, failures);
  return failures;
}
}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard guard;
  const int failures = Probe<float>("s") + Probe<double>("d") +
                       Probe<std::complex<float>>("c") +
                       Probe<std::complex<double>>("z");
  return failures == 0 ? 0 : 1;
}
