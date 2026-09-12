#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <limits>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite_rk.h"
#include "factorization_support.h"
#include "normal_return_guard.h"
namespace {
using installed_internal::Take;
using installed_internal::Value;
using installed_internal::Wide;
using installed_internal::Widen;
constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;
constexpr auto kLower = asc::DenseBlasTriangle::kLower;
constexpr auto kColumn = installed_internal::kColumn;
constexpr auto kRow = installed_internal::kRow;
constexpr auto kHost = installed_internal::kHost;
bool Check(bool pass, const char* text) {
  if (!pass) {
    std::fprintf(stderr, "check failed: %s\n", text);
  }
  return pass;
}
bool EqualBytes(const void* a, const void* b, std::size_t size) {
  return std::memcmp(a, b, size) == 0;
}
Wide Adjoint(Wide value, bool hermitian) {
  return hermitian ? std::conj(value) : value;
}
template <typename T>
auto QueryFactor(const asc::ReferenceLapackProvider& provider,
                 asc::DenseBlasTriangle triangle, bool hermitian, bool blocked,
                 asc::DenseBlasMatrixView<T> matrix,
                 asc::DenseBlasVectorView<T> off_diagonal,
                 asc::DenseBlasVectorView<asc::index_t> pivots) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return blocked ? asc::QueryHetrfRkWorkspace(provider, triangle, matrix,
                                                  off_diagonal, pivots)
                     : asc::QueryHetf2RkWorkspace(provider, triangle, matrix,
                                                  off_diagonal, pivots);
    }
  }
  return blocked ? asc::QuerySytrfRkWorkspace(provider, triangle, matrix,
                                              off_diagonal, pivots)
                 : asc::QuerySytf2RkWorkspace(provider, triangle, matrix,
                                              off_diagonal, pivots);
}

template <typename T>
asc::Status Factor(const asc::ReferenceLapackProvider& provider,
                   asc::DenseBlasTriangle triangle, bool hermitian,
                   bool blocked, asc::DenseBlasMatrixView<T> matrix,
                   asc::DenseBlasVectorView<T> off_diagonal,
                   asc::DenseBlasVectorView<asc::index_t> pivots,
                   const asc::LapackWorkspacePlan& plan,
                   const asc::LapackWorkspace& workspace,
                   asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return blocked ? asc::HetrfRk(provider, triangle, matrix, off_diagonal,
                                    pivots, plan, workspace, report)
                     : asc::Hetf2Rk(provider, triangle, matrix, off_diagonal,
                                    pivots, plan, workspace, report);
    }
  }
  return blocked ? asc::SytrfRk(provider, triangle, matrix, off_diagonal,
                                pivots, plan, workspace, report)
                 : asc::Sytf2Rk(provider, triangle, matrix, off_diagonal,
                                pivots, plan, workspace, report);
}

template <typename T>
struct Scratch {
  std::array<T, 4500> scalar{};
  std::array<T, 5000> packing{};
  alignas(16) std::array<std::byte, 1024> integers{};
  asc::LapackWorkspace workspace;
  Scratch(const asc::LapackWorkspacePlan& plan, asc::extent_t n, bool blocked,
          bool hermitian, int mode) {
    scalar.fill(Value<T>(-641, 17));
    packing.fill(Value<T>(-643, 19));
    integers.fill(std::byte{0x59});
    constexpr auto kScalar =
        static_cast<std::size_t>(asc::LapackWorkspaceKind::kScalar);
    constexpr auto kLayout =
        static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);
    constexpr auto kPivot =
        static_cast<std::size_t>(asc::LapackWorkspaceKind::kInteger);
    for (const auto kind : {kScalar, kLayout, kPivot}) {
      const auto& region = plan.regions[kind];
      auto entries = region.preferred_entries;
      if (kind == kScalar && blocked && n != 0) {
        const std::array<asc::extent_t, 4> sizes{
            1, (hermitian ? 2 : 8) * n, (hermitian ? 1 : 7) * n, 64 * n};
        entries = sizes[static_cast<std::size_t>(mode)];
      }
      if (entries == 0) {
        continue;
      }
      const auto bytes = static_cast<std::size_t>(entries) * region.entry_bytes;
      if (kind == kScalar) {
        workspace.regions[kind] = {scalar.data() + 1, bytes, kHost};
      } else if (kind == kLayout) {
        workspace.regions[kind] = {packing.data() + 1, bytes, kHost};
      } else {
        workspace.regions[kind] = {integers.data() + 16, bytes, kHost};
      }
    }
  }
  [[nodiscard]] bool Guards() const {
    constexpr auto kScalar =
        static_cast<std::size_t>(asc::LapackWorkspaceKind::kScalar);
    constexpr auto kLayout =
        static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);
    constexpr auto kPivot =
        static_cast<std::size_t>(asc::LapackWorkspaceKind::kInteger);
    bool pass = scalar.front() == Value<T>(-641, 17) &&
                packing.front() == Value<T>(-643, 19);
    for (std::size_t i = 1 + workspace.regions[kScalar].size() / sizeof(T);
         i < scalar.size(); ++i) {
      pass = (scalar[i] == Value<T>(-641, 17)) && pass;
    }
    for (std::size_t i = 1 + workspace.regions[kLayout].size() / sizeof(T);
         i < packing.size(); ++i) {
      pass = (packing[i] == Value<T>(-643, 19)) && pass;
    }
    for (std::size_t i = 0; i < integers.size(); ++i) {
      if (i < 16 || i >= 16 + workspace.regions[kPivot].size()) {
        pass = (integers[i] == std::byte{0x59}) && pass;
      }
    }
    return Check(pass, "workspace guards");
  }
};

template <typename T, std::size_t N>
void Initialize(installed_internal::Matrix<T, N, N>& a, std::array<T, N + 2>& e,
                std::array<asc::index_t, N + 2>& pivots,
                std::array<Wide, N * N>& full, bool hermitian,
                asc::DenseBlasTriangle triangle, bool singular) {
  a.data.fill(Value<T>(-601, 11));
  e.fill(Value<T>(-607, 13));
  pivots.fill(-613);
  const auto selected = [triangle](std::size_t i, std::size_t j) {
    return triangle == kUpper ? i <= j : i >= j;
  };
  for (std::size_t i = 0; i < N; ++i) {
    for (std::size_t j = i; j < N; ++j) {
      T value{};
      if (i == j) {
        value =
            Value<T>(i >= 2 || N == 1 ? 4 : 0, hermitian || i < 2 ? 0 : 0.125);
      } else if (i + j == 1) {
        value = Value<T>(3, 4);
      }
      if constexpr (N == 3) {
        if (i != j) {
          const bool strong = triangle == kUpper ? j == 1 : i == 1;
          double magnitude = 1;
          if (strong) {
            magnitude = 4;
          } else if (j - i == 2) {
            magnitude = 0.5;
          }
          value = Value<T>(magnitude, 0.125);
        } else {
          value = T{};
        }
      }
      if (singular && (i == N - 1 || j == N - 1)) {
        value = T{};
      }
      full[i * N + j] = Widen(value);
      full[j * N + i] = Adjoint(Widen(value), hermitian);
    }
  }
  for (std::size_t i = 0; i < N; ++i) {
    for (std::size_t j = 0; j < N; ++j) {
      if (selected(i, j)) {
        a.At(i, j) = Value<T>(static_cast<double>(full[i * N + j].real()),
                              static_cast<double>(full[i * N + j].imag()));
        if constexpr (asc::DenseBlasComplex<T>) {
          if (hermitian && i == j) {
            a.At(i, j).imag(
                std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN());
          }
        }
      } else {
        a.At(i, j) = Value<T>(std::numeric_limits<double>::quiet_NaN());
      }
    }
  }
}

template <std::size_t N>
bool CheckReport(const asc::Status& status, const asc::LapackReport& report,
                 bool singular) {
  bool pass = true;
  const bool positive = singular && N != 0;
  pass = Check(status.code() == (positive ? asc::ErrorCode::kNumerical
                                          : asc::ErrorCode::kOk),
               "factor status") &&
         pass;
  pass = Check(report.called_provider == (N != 0) &&
                   report.native_info.has_value() == (N != 0),
               "native call provenance") &&
         pass;
  pass = Check(report.output_validity ==
                   (positive ? asc::LapackOutputValidity::kDocumentedPartial
                             : asc::LapackOutputValidity::kComplete),
               "factor output validity") &&
         pass;
  pass = Check(report.factor_family == asc::LapackFactorFamily::kRook,
               "RK pivot protocol") &&
         pass;
  if (positive) {
    pass =
        Check(report.native_info.value_or(0) > 0 &&
                  report.diagnostic_index == report.native_info.value_or(0) - 1,
              "positive INFO and diagnostic") &&
        pass;
  } else if constexpr (N != 0) {
    pass = Check(report.native_info == 0, "zero INFO") && pass;
  }
  return pass;
}

template <typename T, std::size_t N>
bool FormFactors(const installed_internal::Matrix<T, N, N>& a,
                 const std::array<T, N + 2>& e,
                 const std::array<asc::index_t, N + 2>& pivots, bool hermitian,
                 asc::DenseBlasTriangle triangle,
                 std::array<Wide, N * N>& triangular,
                 std::array<Wide, N * N>& td) {
  bool pass = true;
  const auto selected = [triangle](std::size_t i, std::size_t j) {
    return triangle == kUpper ? i <= j : i >= j;
  };
  bool valid = true;
  for (std::size_t i = 0; i < N; ++i) {
    const auto raw = pivots[i + 1];
    valid = Check(raw != 0 && raw >= -static_cast<asc::index_t>(N) &&
                      raw <= static_cast<asc::index_t>(N),
                  "pivot bounds") &&
            valid;
    for (std::size_t j = 0; j < N; ++j) {
      if (i == j) {
        triangular[i * N + j] = Wide{1, 0};
      } else if (selected(i, j)) {
        triangular[i * N + j] = Widen(a.At(i, j));
      }
    }
  }
  if (!valid) {
    return false;
  }
  for (std::size_t j = 0; j < N;) {
    const bool pair = pivots[j + 1] < 0;
    if (pair && !Check(j + 1 < N && pivots[j + 2] < 0,
                       "complete adjacent negative pair")) {
      return false;
    }
    for (std::size_t i = 0; i < N; ++i) {
      td[i * N + j] = triangular[i * N + j] * Widen(a.At(j, j));
      if (pair) {
        const auto off = Widen(e[triangle == kUpper ? j + 2 : j + 1]);
        const auto upper = triangle == kUpper ? off : Adjoint(off, hermitian);
        td[i * N + j] += triangular[i * N + j + 1] * Adjoint(upper, hermitian);
        td[i * N + j + 1] =
            triangular[i * N + j] * upper +
            triangular[i * N + j + 1] * Widen(a.At(j + 1, j + 1));
      }
    }
    const auto lower_index = pair ? j + 2 : j + 1;
    pass = Check(e[triangle == kUpper ? j + 1 : lower_index] == T{},
                 "structural-zero E") &&
           pass;
    if (pair) {
      pass = Check(a.At(triangle == kUpper ? j : j + 1,
                        triangle == kUpper ? j + 1 : j) == T{},
                   "separate D offdiagonal") &&
             pass;
    }
    j += pair ? 2 : 1;
  }
  return pass;
}

template <typename T, std::size_t N>
bool Reconstruction(const std::array<Wide, N * N>& full,
                    const std::array<asc::index_t, N + 2>& pivots,
                    const std::array<Wide, N * N>& triangular,
                    const std::array<Wide, N * N>& td, bool hermitian,
                    asc::DenseBlasTriangle triangle) {
  bool pass = true;
  std::array<Wide, N * N> reconstructed{};
  for (std::size_t i = 0; i < N; ++i) {
    for (std::size_t j = 0; j < N; ++j) {
      for (std::size_t k = 0; k < N; ++k) {
        reconstructed[i * N + j] +=
            td[i * N + k] * Adjoint(triangular[j * N + k], hermitian);
      }
    }
  }
  for (std::size_t step = 0; step < N; ++step) {
    const auto i = triangle == kUpper ? step : N - 1 - step;
    const auto raw = pivots[i + 1];
    const auto target = static_cast<std::size_t>((raw < 0 ? -raw : raw) - 1);
    for (std::size_t j = 0; j < N; ++j) {
      std::swap(reconstructed[i * N + j], reconstructed[target * N + j]);
    }
    for (std::size_t j = 0; j < N; ++j) {
      std::swap(reconstructed[j * N + i], reconstructed[j * N + target]);
    }
  }
  long double error = 0;
  long double norm = 0;
  for (std::size_t i = 0; i < N * N; ++i) {
    pass = Check(std::isfinite(reconstructed[i].real()) &&
                     std::isfinite(reconstructed[i].imag()),
                 "finite reconstruction") &&
           pass;
    error = std::max(error, std::abs(reconstructed[i] - full[i]));
    norm = std::max(norm, std::abs(full[i]));
  }
  pass =
      Check(error <=
                32 * std::max<std::size_t>(N, 1) *
                    std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon() *
                    norm,
            "factor reconstruction residual") &&
      pass;
  return pass;
}

template <typename T, std::size_t N>
bool Padding(const installed_internal::Matrix<T, N, N>& a,
             const std::array<T, (N + 1) * (N + 1)>& original,
             const std::array<T, N + 2>& e,
             const std::array<asc::index_t, N + 2>& pivots,
             asc::DenseBlasTriangle triangle) {
  bool pass = true;
  const auto layout = a.layout;
  const auto selected = [triangle](std::size_t i, std::size_t j) {
    return triangle == kUpper ? i <= j : i >= j;
  };
  for (std::size_t k = 0; k < a.data.size(); ++k) {
    const auto i = layout == kColumn ? k % (N + 1) : k / (N + 1);
    const auto j = layout == kColumn ? k / (N + 1) : k % (N + 1);
    if (i >= N || j >= N || !selected(i, j)) {
      pass = Check(EqualBytes(&a.data[k], &original[k], sizeof(T)),
                   "A opposite triangle and padding") &&
             pass;
    }
  }
  pass =
      Check(e.front() == Value<T>(-607, 13) && e.back() == Value<T>(-607, 13) &&
                pivots.front() == -613 && pivots.back() == -613,
            "E and pivot guards") &&
      pass;
  return pass;
}

template <typename T, std::size_t N>
bool Case(const asc::ReferenceLapackProvider& provider, bool hermitian,
          asc::DenseBlasTriangle triangle, asc::DenseBlasLayout layout,
          bool blocked, bool singular, int mode) {
  installed_internal::Matrix<T, N, N> a{{}, layout};
  std::array<T, N + 2> e{};
  std::array<asc::index_t, N + 2> pivots{};
  std::array<Wide, N * N> full{};
  Initialize<T, N>(a, e, pivots, full, hermitian, triangle, singular);
  const auto original = a.data;
  const auto extra = Take(asc::DenseBlasVectorView<T>::Create(
      e.data() + 1, N, 1, {e.data(), sizeof(e), kHost}));
  const auto pivot = Take(asc::DenseBlasVectorView<asc::index_t>::Create(
      pivots.data() + 1, N, 1, {pivots.data(), sizeof(pivots), kHost}));
  const auto plan = Take(QueryFactor(provider, triangle, hermitian, blocked,
                                     a.View(), extra, pivot));
  bool pass =
      Check(EqualBytes(a.data.data(), original.data(), sizeof(original)),
            "query leaves A unchanged");
  for (auto value : e) {
    pass =
        Check(value == Value<T>(-607, 13), "query leaves E unchanged") && pass;
  }
  for (auto value : pivots) {
    pass = Check(value == -613, "query leaves pivots unchanged") && pass;
  }
  Scratch<T> scratch(plan, static_cast<asc::extent_t>(N), blocked, hermitian,
                     mode);
  asc::LapackReport report;
  const auto status = Factor(provider, triangle, hermitian, blocked, a.View(),
                             extra, pivot, plan, scratch.workspace, report);
  pass = CheckReport<N>(status, report, singular) && pass;
  std::array<Wide, N * N> triangular{};
  std::array<Wide, N * N> td{};
  if (!FormFactors<T, N>(a, e, pivots, hermitian, triangle, triangular, td)) {
    return false;
  }
  pass =
      Reconstruction<T, N>(full, pivots, triangular, td, hermitian, triangle) &&
      pass;
  pass = Padding<T, N>(a, original, e, pivots, triangle) && pass;
  pass = scratch.Guards() && pass;
  return pass;
}

template <typename T, std::size_t N>
bool Size(const asc::ReferenceLapackProvider& provider, bool hermitian,
          int& cases) {
  bool pass = true;
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto layout : {kColumn, kRow}) {
      for (bool blocked : {false, true}) {
        for (bool singular : {false, true}) {
          for (int mode = 0; mode < (blocked ? 4 : 1); ++mode) {
            pass = Case<T, N>(provider, hermitian, triangle, layout, blocked,
                              singular, mode) &&
                   pass;
            ++cases;
          }
        }
      }
    }
  }
  return pass;
}
template <typename T>
bool Run(const asc::ReferenceLapackProvider& provider, bool hermitian,
         int& cases) {
  bool pass = true;
  pass = Size<T, 0>(provider, hermitian, cases) && pass;
  pass = Size<T, 1>(provider, hermitian, cases) && pass;
  pass = Size<T, 2>(provider, hermitian, cases) && pass;
  pass = Size<T, 3>(provider, hermitian, cases) && pass;
  pass = Size<T, 7>(provider, hermitian, cases) && pass;
  pass = Size<T, 67>(provider, hermitian, cases) && pass;
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
  std::printf("Installed RK factors cases=%d pass=%d\n", cases,
              static_cast<int>(pass));
  return pass ? 0 : 1;
}
