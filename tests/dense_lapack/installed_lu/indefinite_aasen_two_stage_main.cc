#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <span>
#include <vector>

#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite_aasen_two_stage.h"
#include "factorization_support.h"
#include "normal_return_guard.h"
namespace {
using installed_internal::Take;
using installed_internal::Value;
constexpr auto kHost = installed_internal::kHost;
constexpr auto kColumn = installed_internal::kColumn;
constexpr auto kRow = installed_internal::kRow;
constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;
constexpr auto kLower = asc::DenseBlasTriangle::kLower;
bool Check(bool pass, const char* message) {
  if (!pass) {
    std::fprintf(stderr, "check failed: %s\n", message);
  }
  return pass;
}
// Ignored storage must preserve its bytes, including NaN/sign payloads.
// Compare byte spans explicitly; this is not floating-point value equality.
template <typename T>
bool EqualBytes(const T& actual, const T& expected) {
  const auto a = std::as_bytes(std::span{&actual, 1});
  const auto b = std::as_bytes(std::span{&expected, 1});
  return std::equal(a.begin(), a.end(), b.begin());
}
template <typename T>
auto Query(const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasTriangle triangle, bool he,
           asc::DenseBlasMatrixView<T> matrix, asc::DenseBlasVectorView<T> band,
           asc::DenseBlasVectorView<asc::index_t> pivots,
           asc::DenseBlasVectorView<asc::index_t> band_pivots) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (he) {
      return asc::QueryHetrfAa2StageWorkspace(provider, triangle, matrix, band,
                                              pivots, band_pivots);
    }
  }
  return asc::QuerySytrfAa2StageWorkspace(provider, triangle, matrix, band,
                                          pivots, band_pivots);
}
template <typename T>
asc::Status Factor(const asc::ReferenceLapackProvider& provider,
                   asc::DenseBlasTriangle triangle, bool he,
                   asc::DenseBlasMatrixView<T> matrix,
                   asc::DenseBlasVectorView<T> band,
                   asc::DenseBlasVectorView<asc::index_t> pivots,
                   asc::DenseBlasVectorView<asc::index_t> band_pivots,
                   const asc::LapackWorkspacePlan& plan,
                   const asc::LapackWorkspace& workspace,
                   asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (he) {
      return asc::HetrfAa2Stage(provider, triangle, matrix, band, pivots,
                                band_pivots, plan, workspace, report);
    }
  }
  return asc::SytrfAa2Stage(provider, triangle, matrix, band, pivots,
                            band_pivots, plan, workspace, report);
}
template <typename T>
struct Scratch {
  std::array<T, 12866> scalar{};
  std::array<T, 4491> packing{};
  alignas(16) std::array<std::byte, 1120> integers{};
  asc::LapackWorkspace workspace;
  Scratch(const asc::LapackWorkspacePlan& plan, int mode, asc::extent_t n) {
    scalar.fill(T{-71});
    packing.fill(T{-73});
    integers.fill(std::byte{0x59});
    for (std::size_t kind = 0; kind < plan.regions.size(); ++kind) {
      const auto& region = plan.regions[kind];
      auto entries = region.preferred_entries;
      if (kind == static_cast<std::size_t>(asc::LapackWorkspaceKind::kScalar)) {
        if (mode == 0) {
          entries = region.minimum_entries;
        } else if (mode == 1) {
          entries = std::min(entries, std::max(region.minimum_entries, 3 * n));
        }
      }
      if (entries == 0) {
        continue;
      }
      const auto bytes = static_cast<std::size_t>(entries) * region.entry_bytes;
      if (kind == static_cast<std::size_t>(asc::LapackWorkspaceKind::kScalar)) {
        workspace.regions[kind] = {scalar.data() + 1, bytes, kHost};
      } else if (kind == static_cast<std::size_t>(
                             asc::LapackWorkspaceKind::kLayoutConversion)) {
        workspace.regions[kind] = {packing.data() + 1, bytes, kHost};
      } else if (kind ==
                 static_cast<std::size_t>(asc::LapackWorkspaceKind::kInteger)) {
        workspace.regions[kind] = {integers.data() + 16, bytes, kHost};
      }
    }
  }
  [[nodiscard]] bool Guards() const {
    const auto scalar_bytes = workspace
                                  .regions[static_cast<std::size_t>(
                                      asc::LapackWorkspaceKind::kScalar)]
                                  .size();
    const auto packing_bytes =
        workspace
            .regions[static_cast<std::size_t>(
                asc::LapackWorkspaceKind::kLayoutConversion)]
            .size();
    const auto integer_bytes = workspace
                                   .regions[static_cast<std::size_t>(
                                       asc::LapackWorkspaceKind::kInteger)]
                                   .size();
    bool pass = scalar.front() == T{-71} && packing.front() == T{-73};
    for (std::size_t i = 1 + scalar_bytes / sizeof(T); i < scalar.size(); ++i) {
      pass = (scalar[i] == T{-71}) && pass;
    }
    for (std::size_t i = 1 + packing_bytes / sizeof(T); i < packing.size();
         ++i) {
      pass = (packing[i] == T{-73}) && pass;
    }
    for (std::size_t i = 0; i < integers.size(); ++i) {
      if (i < 16 || i >= 16 + integer_bytes) {
        pass = (integers[i] == std::byte{0x59}) && pass;
      }
    }
    return Check(pass, "caller workspace guards");
  }
};
template <typename T, std::size_t N>
T Original(std::size_t i, std::size_t j, bool he) {
  T value{};
  if (i + j == 1) {
    value = Value<T>(2, .5);
  } else if (i == j && (N == 1 || (i >= 2 && (N != 3 || i != 2)))) {
    value = Value<T>(4, he ? 0 : 1);
  }
  return he && i > j ? installed_internal::Conjugate(value) : value;
}
template <std::size_t N>
bool Congruence(const std::array<installed_internal::Wide, N * N>& band,
                const std::array<installed_internal::Wide, N * N>& factor,
                const std::array<asc::index_t, N + 2>& pivots, bool he, int nb,
                std::array<installed_internal::Wide, N * N>& product) {
  const auto adjoint = [he](installed_internal::Wide value) {
    return he ? std::conj(value) : value;
  };
  std::array<installed_internal::Wide, N * N> intermediate{};
  bool pass = true;
  for (std::size_t i = 0; i < N; ++i) {
    for (std::size_t k = 0; k <= i; ++k) {
      for (std::size_t j = 0; j < N; ++j) {
        intermediate[i * N + j] += factor[i * N + k] * band[k * N + j];
      }
    }
    for (std::size_t j = 0; j < N; ++j) {
      for (std::size_t k = 0; k <= j; ++k) {
        product[i * N + j] +=
            intermediate[i * N + k] * adjoint(factor[j * N + k]);
      }
    }
  }
  for (std::size_t remaining = N; remaining > 0; --remaining) {
    const auto i = remaining - 1;
    const auto p = pivots[i + 1] - 1;
    if (!Check(p >= static_cast<asc::index_t>(i) &&
                   p < static_cast<asc::index_t>(N),
               "outer pivot range")) {
      return false;
    }
    pass = Check(i >= static_cast<std::size_t>(nb) ||
                     p == static_cast<asc::index_t>(i),
                 "identity prefix") &&
           pass;
    for (std::size_t j = 0; j < N; ++j) {
      std::swap(product[i * N + j], product[p * N + j]);
    }
    for (std::size_t j = 0; j < N; ++j) {
      std::swap(product[j * N + i], product[j * N + p]);
    }
  }
  return pass;
}
template <typename T, std::size_t N>
bool Verify(const installed_internal::Matrix<T, N, N>& a,
            const std::array<T, (N + 1) * (N + 1)>& before,
            const std::vector<T>& tb, std::size_t ldtb, std::size_t nb,
            const std::array<asc::index_t, N + 2>& pivots,
            const std::array<asc::index_t, N + 2>& band_pivots, bool he,
            asc::DenseBlasTriangle tri) {
  using installed_internal::Wide;
  const auto adjoint = [he](Wide value) {
    return he ? std::conj(value) : value;
  };
  std::array<Wide, N * N> band{};
  std::array<Wide, N * N> factor{};
  std::array<Wide, N * N> product{};
  bool pass = true;
  for (std::size_t i = 0; i < N; ++i) {
    factor[i * N + i] = 1;
    for (std::size_t j = i; j < std::min(N, i + 2 * nb + 1); ++j) {
      band[i * N + j] =
          installed_internal::Widen(tb[1 + j * ldtb + 2 * nb + i - j]);
    }
    for (std::size_t j = nb; j < i; ++j) {
      factor[i * N + j] =
          tri == kUpper ? adjoint(installed_internal::Widen(a.At(j - nb, i)))
                        : installed_internal::Widen(a.At(i, j - nb));
    }
  }
  for (std::size_t remaining = N; remaining > 0; --remaining) {
    const auto j = remaining - 1;
    for (std::size_t i = j + 1; i < std::min(N, j + nb + 1); ++i) {
      const auto multiplier =
          installed_internal::Widen(tb[1 + j * ldtb + 2 * nb + i - j]);
      for (std::size_t k = 0; k < N; ++k) {
        band[i * N + k] += multiplier * band[j * N + k];
      }
    }
    const auto p = band_pivots[j + 1] - 1;
    if (!Check(p >= static_cast<asc::index_t>(j) &&
                   p < static_cast<asc::index_t>(std::min(N, j + nb + 1)),
               "band pivot range")) {
      return false;
    }
    for (std::size_t k = 0; k < N; ++k) {
      std::swap(band[j * N + k], band[p * N + k]);
    }
  }
  pass = Congruence<N>(band, factor, pivots, he, nb, product) && pass;
  for (std::size_t i = 0; i < N; ++i) {
    for (std::size_t j = 0; j < N; ++j) {
      const auto expected = installed_internal::Widen(Original<T, N>(i, j, he));
      pass = Check(installed_internal::Near<T>(product[i * N + j], expected,
                                               4 * N),
                   "two-stage factor reconstruction") &&
             pass;
      if (tri == kUpper ? i > j : i < j) {
        pass = Check(EqualBytes(a.At(i, j), before[a.Offset(i, j)]),
                     "ignored triangle") &&
               pass;
      }
    }
  }
  pass = Check(a.PaddingEquals(before), "matrix padding") && pass;
  return Check(pivots.front() == -67 && pivots.back() == -67 &&
                   band_pivots.front() == -67 && band_pivots.back() == -67 &&
                   tb.front() == T{-79} && tb.back() == T{-79},
               "persistent output guards") &&
         pass;
}
template <typename T, std::size_t N>
bool Case(const asc::ReferenceLapackProvider& provider, bool he,
          asc::DenseBlasTriangle tri, asc::DenseBlasLayout layout,
          int band_mode, int work_mode) {
  installed_internal::Matrix<T, N, N> a{{}, layout};
  a.data.fill(Value<T>(-61, 17));
  for (std::size_t i = 0; i < N; ++i) {
    for (std::size_t j = 0; j < N; ++j) {
      if (tri == kUpper ? i <= j : i >= j) {
        a.At(i, j) = Original<T, N>(i, j, he);
        if constexpr (asc::DenseBlasComplex<T>) {
          if (he && i == j) {
            a.At(i, j).imag(
                std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN());
          }
        }
      }
    }
  }
  const auto before = a.data;
  const int ldtb = std::array{4, 10, 577}[band_mode];
  std::vector<T> tb(N * ldtb + 2, T{-79});
  std::array<asc::index_t, N + 2> pivots;
  std::array<asc::index_t, N + 2> band_pivots;
  pivots.fill(-67);
  band_pivots.fill(-67);
  const auto p = Take(asc::DenseBlasVectorView<asc::index_t>::Create(
      pivots.data() + 1, N, 1, {pivots.data(), sizeof(pivots), kHost}));
  const auto q = Take(asc::DenseBlasVectorView<asc::index_t>::Create(
      band_pivots.data() + 1, N, 1,
      {band_pivots.data(), sizeof(band_pivots), kHost}));
  const auto band = Take(asc::DenseBlasVectorView<T>::Create(
      tb.data() + 1, N * ldtb, 1, {tb.data(), tb.size() * sizeof(T), kHost}));
  const auto plan = Take(Query(provider, tri, he, a.View(), band, p, q));
  Scratch<T> scratch(plan, work_mode, N);
  asc::LapackReport report;
  const auto status = Factor(provider, tri, he, a.View(), band, p, q, plan,
                             scratch.workspace, report);
  constexpr bool kSingular = N == 3;
  bool pass =
      Check(status.ok() == !kSingular && report.called_provider == (N != 0) &&
                report.native_info.has_value() == (N != 0) &&
                report.factor_family == asc::LapackFactorFamily::kAasen &&
                report.output_validity ==
                    (kSingular ? asc::LapackOutputValidity::kDocumentedPartial
                               : asc::LapackOutputValidity::kComplete),
            "two-stage report");
  if constexpr (N != 0) {
    pass = Check(report.native_info.value_or(-1) == (kSingular ? N : 0),
                 "native status") &&
           pass;
    const int nb =
        std::min({192, (ldtb - 1) / 3, std::array{1, 3, 192}[work_mode]});
    pass = Check(tb[1] == Value<T>(nb), "retained block width") && pass;
    if (status.ok() || status.code() == asc::ErrorCode::kNumerical) {
      pass =
          Verify(a, before, tb, ldtb, nb, pivots, band_pivots, he, tri) && pass;
    }
  } else {
    pass = Check(EqualBytes(a.data, before) && pivots.front() == -67 &&
                     pivots.back() == -67 && band_pivots.front() == -67 &&
                     band_pivots.back() == -67 && tb.front() == T{-79} &&
                     tb.back() == T{-79},
                 "empty noncall") &&
           pass;
  }
  return scratch.Guards() && pass;
}
template <typename T>
bool Run(const asc::ReferenceLapackProvider& provider, bool he, int& cases) {
  bool pass = true;
  for (const auto tri : {kUpper, kLower}) {
    for (const auto layout : {kColumn, kRow}) {
      for (const int band : {0, 1, 2}) {
        for (const int work : {0, 1, 2}) {
          pass = Case<T, 0>(provider, he, tri, layout, band, work) && pass;
          pass = Case<T, 1>(provider, he, tri, layout, band, work) && pass;
          pass = Case<T, 3>(provider, he, tri, layout, band, work) && pass;
          pass = Case<T, 67>(provider, he, tri, layout, band, work) && pass;
          cases += 4;
        }
      }
    }
  }
  return pass;
}
}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int cases = 0;
  bool pass = true;
  pass = Run<float>(provider, false, cases) && pass;
  pass = Run<double>(provider, false, cases) && pass;
  pass = Run<std::complex<float>>(provider, false, cases) && pass;
  pass = Run<std::complex<double>>(provider, false, cases) && pass;
  pass = Run<std::complex<float>>(provider, true, cases) && pass;
  pass = Run<std::complex<double>>(provider, true, cases) && pass;
  std::printf("Installed two-stage Aasen cases=%d passed=%d\n", cases,
              static_cast<int>(pass));
  return pass ? 0 : 1;
}
