#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string_view>
#include <type_traits>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_dmd.h"
#include "asc/dense/providers/lapack_dmd_qr.h"
#include "dmd_test_support.h"
#include "installed_lu/normal_return_guard.h"
namespace {
using asc_dmd_test::Take;
using asc_dmd_test::TestContext;
template <typename T>
struct RealType {
  using type = T;
};
template <typename T>
struct RealType<std::complex<T>> {
  using type = T;
};
template <typename T>
using Real = typename RealType<T>::type;
template <typename T>
constexpr bool kComplex = !std::is_same_v<T, Real<T>>;
constexpr asc::index_t kRows = 5, kColumns = 4, kLd = 7;
template <typename T>
T Eigenvalue(int i) {
  if constexpr (kComplex<T>) {
    return {static_cast<Real<T>>(2 + i), static_cast<Real<T>>(1 - 2 * i)};
  } else {
    return static_cast<T>(2 + i);
  }
}
template <typename T>
struct Data {
  std::array<T, 35> f{}, x{}, y{}, z{}, b{}, v{}, s{};
  std::array<std::complex<Real<T>>, 4> eigen{};
  std::array<Real<T>, 4> residual{}, singular{};
  std::array<T, 4> tau{};
  asc::index_t rank = -73, info = -79;
};
template <typename T>
auto Vector(std::array<T, 4>& values, asc::extent_t n) {
  return Take(asc::DenseBlasVectorView<T>::Create(
      values.data(), n, 1,
      {values.data(), sizeof(values), asc::MemorySpace::kHost}));
}
template <typename T>
asc::LapackDmdQrBuffers<T> Buffers(Data<T>& d) {
  const auto matrix = [](auto& values, asc::extent_t m, asc::extent_t n) {
    return Take(asc::DenseBlasMatrixView<T>::Create(
        values.data(), m, n, asc::DenseBlasLayout::kColumnMajor, kLd,
        {values.data(), sizeof(values), asc::MemorySpace::kHost}));
  };
  return {matrix(d.f, kRows, kColumns),
          matrix(d.x, kColumns, kColumns - 1),
          matrix(d.y, kColumns, kColumns),
          matrix(d.z, kRows, kColumns - 1),
          matrix(d.b, kColumns, kColumns - 1),
          matrix(d.v, kColumns - 1, kColumns - 1),
          matrix(d.s, kColumns - 1, kColumns - 1),
          Vector(d.eigen, kColumns - 1),
          Vector(d.singular, kColumns - 1),
          Vector(d.residual, kColumns - 1),
          Vector(d.tau, kColumns)};
}
template <typename T>
bool Check(const Data<T>& d, const std::array<T, 35>& original, char vectors,
           char extra) {
  using Wide = std::complex<long double>;
  const long double tolerance = 512 * std::numeric_limits<Real<T>>::epsilon();
  bool ok = d.info == 0 && d.rank == 2;
  bool first = false;
  bool second = false;
  for (int j = 0; j < 2; ++j) {
    Wide eigen = d.eigen[j];
    first = first || std::abs(eigen - Wide(Eigenvalue<T>(0))) < tolerance;
    second = second || std::abs(eigen - Wide(Eigenvalue<T>(1))) < tolerance;
    long double norm = 0;
    long double residual = 0;
    for (int i = 0; i < kRows; ++i) {
      Wide mode{};
      if (vectors == 'V') {
        mode = d.z[j * kLd + i];
      }
      if (vectors == 'F') {
        for (int k = 0; k < 2; ++k) {
          mode += Wide(d.z[k * kLd + i]) * Wide(d.v[j * kLd + k]);
        }
      }
      if (vectors == 'Q') {
        for (int k = 0; k < kColumns; ++k) {
          mode += Wide(d.f[k * kLd + i]) * Wide(d.z[j * kLd + k]);
        }
      }
      const Wide diagonal = i < 2 ? Wide(Eigenvalue<T>(i)) : Wide{};
      norm += std::norm(mode);
      residual += std::norm((diagonal - eigen) * mode);
      if (extra != 'N') {
        Wide actual{};
        Wide pod{};
        for (int k = 0; k < kColumns; ++k) {
          actual += Wide(d.f[k * kLd + i]) * Wide(d.b[j * kLd + k]);
          Wide basis = d.x[j * kLd + k];
          if (extra == 'E') {
            basis = 0;
            for (int h = 0; h < 2; ++h) {
              basis += Wide(d.x[h * kLd + k]) * Wide(d.v[j * kLd + h]);
            }
          }
          pod += Wide(d.f[k * kLd + i]) * basis;
        }
        ok = std::abs(actual - diagonal * pod) < tolerance && ok;
      }
    }
    if (vectors != 'N') {
      ok = std::isfinite(norm) && std::abs(norm - 1) < tolerance &&
           std::sqrt(residual) < tolerance && ok;
    }
    if (vectors == 'V') {
      ok = std::isfinite(d.residual[j]) &&
           std::abs(d.residual[j] - std::sqrt(residual)) < tolerance && ok;
    }
  }
  for (int j = 0; j < kColumns; ++j) {
    for (int i = 0; i < kRows; ++i) {
      Wide restored{};
      for (int k = 0; k < kColumns; ++k) {
        restored += Wide(d.f[k * kLd + i]) * Wide(d.y[j * kLd + k]);
      }
      ok = std::abs(restored - Wide(original[j * kLd + i])) <
               tolerance *
                   std::max(1.0L, std::abs(Wide(original[j * kLd + i]))) &&
           ok;
    }
  }
  return ok && first && second;
}
template <typename T>
int Run() {
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  std::size_t cases = 0;
  for (int svd : {1, 2, 3, 4}) {
    constexpr std::array<char, 4> kVectors{'N', 'V', 'F', 'Q'};
    constexpr std::array<char, 4> kScaling{'N', 'S', 'C', 'Y'};
    constexpr std::array<char, 4> kExtra{'N', 'R', 'E', 'E'};
    for (std::size_t mode = 0; mode < kVectors.size(); ++mode) {
      const auto vectors = kVectors[mode];
      const auto scaling = kScaling[mode];
      const auto extra = kExtra[mode];
      for (bool preferred : {false, true}) {
        Data<T> d;
        for (int j = 0; j < kColumns; ++j) {
          for (int i = 0; i < 2; ++i) {
            T value{1};
            for (int h = 0; h < j; ++h) {
              value *= Eigenvalue<T>(i);
            }
            d.f[j * kLd + i] = value;
          }
        }
        const auto original = d.f;
        const asc::LapackDmdQrOptions options{
            static_cast<asc::LapackDmdScaling>(scaling),
            static_cast<asc::LapackDmdQrVectors>(vectors),
            static_cast<asc::LapackDmdExtra>(extra),
            static_cast<asc::LapackDmdSvd>(svd),
            vectors == 'V',
            true,
            true,
            2};
        const auto buffers = Buffers(d);
        const auto tolerance =
            Real<T>{32} * std::numeric_limits<Real<T>>::epsilon();
        const auto plan = Take(
            asc::QueryGedmdqWorkspace(provider, options, buffers, tolerance));
        ASC_DENSE_TEST_CHECK(test, d.f == original);
        asc_dmd_test::Scratch<T> storage(plan, preferred);
        asc::LapackReport report;
        const auto status =
            asc::Gedmdq(provider, options, buffers, tolerance, d.rank, plan,
                        storage.workspace, report);
        d.info = report.native_info.value_or(-79);
        ASC_DENSE_TEST_CHECK(test, status.ok());
        ASC_DENSE_TEST_CHECK(test, report.called_provider);
        const bool mathematical = Check(d, original, vectors, extra);
        ASC_DENSE_TEST_CHECK(test, mathematical);
        storage.Guards(test);
        std::printf(
            "ASC GEDMDQ complex=%d bytes=%zu mode=%c%c%c svd=%d preferred=%d "
            "INFO=%lld K=%lld mathematical=%d\n",
            static_cast<int>(kComplex<T>), sizeof(T), scaling, vectors, extra,
            svd, static_cast<int>(preferred), static_cast<long long>(d.info),
            static_cast<long long>(d.rank), static_cast<int>(mathematical));
        ++cases;
      }
    }
  }
  std::printf("ASC GEDMDQ required mathematical cases=%zu\n", cases);
  return test.Finish();
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard guard;
  if (argc != 2) {
    return 2;
  }
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    return Run<float>();
  }
  if (scalar == "d") {
    return Run<double>();
  }
  if (scalar == "c") {
    return Run<std::complex<float>>();
  }
  if (scalar == "z") {
    return Run<std::complex<double>>();
  }
  return 2;
}
