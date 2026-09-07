#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <numbers>

#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_svd_least_squares.h"
#include "factorization_support.h"
#include "normal_return_guard.h"

namespace {
using installed_internal::Conjugate;
using installed_internal::kColumn;
using installed_internal::kHost;
using installed_internal::kRow;
using installed_internal::Matrix;
using installed_internal::Near;
using installed_internal::Take;
using installed_internal::Value;
using installed_internal::Vector;
using installed_internal::Wide;
using installed_internal::Widen;

enum class Driver : std::uint8_t { kGelss, kGelsd };

template <typename T>
struct Scratch {
  std::array<T, 5000> scalar{};
  std::array<T, 64> packing{};
  std::array<asc::DenseBlasRealType<T>, 4096> real{};
  std::array<asc::DenseBlasRealType<T>, 4> staging{};
  std::array<std::int32_t, 128> integer32{};
  std::array<std::int64_t, 128> integer64{};

  asc::LapackWorkspace View(const asc::LapackWorkspacePlan& plan,
                            bool preferred) {
    using Kind = asc::LapackWorkspaceKind;
    asc::LapackWorkspace result;
    const auto put = [&]<typename Element, std::size_t N>(
                         Kind kind, std::array<Element, N>& storage) {
      const auto slot = static_cast<std::size_t>(kind);
      const auto& region = plan.regions[slot];
      const auto count =
          preferred ? region.preferred_entries : region.minimum_entries;
      if (count < 0 || static_cast<std::size_t>(count) > N ||
          (count != 0 && region.entry_bytes != sizeof(Element))) {
        std::abort();
      }
      result.regions[slot] = {storage.data(),
                              static_cast<std::size_t>(count) * sizeof(Element),
                              kHost};
    };
    put(Kind::kScalar, scalar);
    put(Kind::kReal, real);
    put(Kind::kScratch, staging);
    put(Kind::kLayoutConversion, packing);
    if (plan.regions[static_cast<std::size_t>(Kind::kInteger)].entry_bytes ==
        sizeof(std::int32_t)) {
      put(Kind::kInteger, integer32);
    } else {
      put(Kind::kInteger, integer64);
    }
    return result;
  }
};

template <typename T>
T Phase(std::size_t index) {
  const std::array<T, 4> phases{Value<T>(1), Value<T>(0, 1), Value<T>(-1),
                                Value<T>(0, -1)};
  if constexpr (asc::DenseBlasComplex<T>) {
    return phases[index % 4];
  } else {
    return index % 2 == 0 ? T{1} : T{-1};
  }
}

// Independent fixtures, before unitary row/column phases:
// 3x2: A=[1,0;0,1;1,1], b=[3,0,0], x=[2,-1], ||r||^2=3.
// 2x3: A=[1,0,1;0,2,0], b=[2,6], x=[1,3,1], r=0.
// 3x4: A=[1,0,1,0;0,2,0,2;1,0,1,0], b=[3,12,1],
//       x=[1,3,1,3], ||r||^2=2. Its two repeated column pairs span
//       a known two-dimensional nullspace; rank is exactly two.
template <std::size_t Rows, std::size_t Columns>
double Coefficient(std::size_t row, std::size_t column) {
  if constexpr (Columns == 2) {
    return row == 2 || row == column ? 1 : 0;
  } else {
    if (row % 2 != column % 2) {
      return 0;
    }
    return row == 1 ? 2 : 1;
  }
}

template <std::size_t Rows, std::size_t Columns>
double Rhs(std::size_t row) {
  if constexpr (Columns == 2) {
    return row == 0 ? 3 : 0;
  } else if constexpr (Rows == 2) {
    return row == 0 ? 2 : 6;
  } else {
    constexpr std::array<double, 3> kRhs{3, 12, 1};
    return kRhs[row];
  }
}

template <std::size_t Columns>
long double Solution(std::size_t row) {
  if constexpr (Columns == 2) {
    return row == 0 ? 2 : -1;
  } else {
    return row % 2 == 0 ? 1 : 3;
  }
}

template <std::size_t Rows, std::size_t Columns>
constexpr std::array<long double, 3> SingularValues() {
  if constexpr (Columns == 2) {
    return {std::numbers::sqrt3_v<long double>, 1, 0};
  } else if constexpr (Rows == 2) {
    return {2, std::numbers::sqrt2_v<long double>, 0};
  } else {
    return {2 * std::numbers::sqrt2_v<long double>, 2, 0};
  }
}

template <std::size_t Rows, std::size_t Columns>
constexpr long double ResidualSquared() {
  if constexpr (Rows == 2) {
    return 0;
  } else {
    return Columns == 2 ? 3 : 2;
  }
}

template <typename T, std::size_t Rows, std::size_t Columns>
bool Mathematics(const Matrix<T, Rows, Columns>& original,
                 const Matrix<T, std::max(Rows, Columns), 2>& before,
                 const Matrix<T, std::max(Rows, Columns), 2>& result,
                 const std::array<asc::DenseBlasRealType<T>,
                                  std::min(Rows, Columns)>& singular) {
  constexpr std::size_t kCount = std::min(Rows, Columns);
  constexpr auto kExpectedS = SingularValues<Rows, Columns>();
  for (std::size_t i = 0; i < kCount; ++i) {
    if (!Near<T>(Wide{singular[i], 0}, Wide{kExpectedS[i], 0}, 16) ||
        singular[i] < 0 || (i > 0 && singular[i] > singular[i - 1])) {
      return false;
    }
  }
  for (std::size_t h = 0; h < 2; ++h) {
    const long double multiplier = h + 1;
    std::array<Wide, Rows> residual{};
    long double squared_norm = 0;
    for (std::size_t i = 0; i < Rows; ++i) {
      residual[i] = -Widen(before.At(i, h));
      for (std::size_t j = 0; j < Columns; ++j) {
        residual[i] += Widen(original.At(i, j)) * Widen(result.At(j, h));
      }
      squared_norm += std::norm(residual[i]);
    }
    constexpr long double kResidualSquared = ResidualSquared<Rows, Columns>();
    if (!Near<T>(Wide{squared_norm, 0},
                 Wide{kResidualSquared * multiplier * multiplier, 0}, 64)) {
      return false;
    }
    for (std::size_t j = 0; j < Columns; ++j) {
      Wide gradient{};
      for (std::size_t i = 0; i < Rows; ++i) {
        gradient += std::conj(Widen(original.At(i, j))) * residual[i];
      }
      const Wide expected =
          Widen(Conjugate(Phase<T>(j + 1))) * Solution<Columns>(j) * multiplier;
      if (!Near<T>(gradient, {}, 64) ||
          !Near<T>(Widen(result.At(j, h)), expected, 32)) {
        return false;
      }
    }
    if constexpr (Columns > Rows) {
      // A null vector is conj(column_phase)*[1,0,-1,0] (and, for
      // the four-column case, conj(column_phase)*[0,1,0,-1]).
      for (std::size_t j = 2; j < Columns; ++j) {
        const Wide null_component =
            Widen(Phase<T>(j - 1)) * Widen(result.At(j - 2, h)) -
            Widen(Phase<T>(j + 1)) * Widen(result.At(j, h));
        if (!Near<T>(null_component, {}, 32)) {
          return false;
        }
      }
    } else {
      // These are transformed residual coordinates, not the original r.
      if (!Near<T>(Wide{std::norm(Widen(result.At(Columns, h))), 0},
                   Wide{kResidualSquared * multiplier * multiplier, 0}, 64)) {
        return false;
      }
    }
  }
  return true;
}

template <typename T, std::size_t Rows, std::size_t Columns>
bool Solve(const asc::ReferenceLapackProvider& provider, Driver driver,
           asc::DenseBlasLayout a_layout, asc::DenseBlasLayout b_layout,
           bool preferred) {
  using Real = asc::DenseBlasRealType<T>;
  Matrix<T, Rows, Columns> a{{}, a_layout};
  Matrix<T, std::max(Rows, Columns), 2> b{{}, b_layout};
  a.data.fill(T{-73});
  b.data.fill(T{-79});
  for (std::size_t i = 0; i < Rows; ++i) {
    for (std::size_t j = 0; j < Columns; ++j) {
      a.At(i, j) = Phase<T>(i) *
                   static_cast<Real>(Coefficient<Rows, Columns>(i, j)) *
                   Phase<T>(j + 1);
    }
    for (std::size_t h = 0; h < 2; ++h) {
      b.At(i, h) =
          Phase<T>(i) * static_cast<Real>(Rhs<Rows, Columns>(i) * (h + 1));
    }
  }
  const auto original = a;
  const auto before_b = b;
  std::array<Real, std::min(Rows, Columns)> singular;
  singular.fill(Real{-89});
  const auto before_s = singular;
  asc::LapackReport report;
  constexpr Real kCutoff = Real{0.001};
  const auto query =
      driver == Driver::kGelss
          ? asc::QueryGelssWorkspace(provider, a.View(), b.View(),
                                     Vector(singular), kCutoff, report)
          : asc::QueryGelsdWorkspace(provider, a.View(), b.View(),
                                     Vector(singular), kCutoff, report);
  if (!query.ok() || !report.called_provider || report.native_info != 0 ||
      report.outcome != asc::LapackOutcome::kSuccess ||
      report.output_validity != asc::LapackOutputValidity::kUnchanged ||
      a.data != original.data || b.data != before_b.data ||
      singular != before_s) {
    return false;
  }
  Scratch<T> scratch;
  const auto work = scratch.View(*query, preferred);
  asc::index_t rank = -91;
  const auto execute = [&](const asc::LapackWorkspace& workspace) {
    return driver == Driver::kGelss
               ? asc::Gelss(provider, a.View(), b.View(), Vector(singular),
                            kCutoff, rank, *query, workspace, report)
               : asc::Gelsd(provider, a.View(), b.View(), Vector(singular),
                            kCutoff, rank, *query, workspace, report);
  };
  auto short_work = work;
  short_work
      .regions[static_cast<std::size_t>(asc::LapackWorkspaceKind::kScalar)] = {
      scratch.scalar.data(), 0, kHost};
  if (execute(short_work).code() != asc::ErrorCode::kInvalidArgument ||
      report.called_provider || report.native_info.has_value() ||
      report.outcome != asc::LapackOutcome::kNotRun ||
      report.output_validity != asc::LapackOutputValidity::kUnchanged ||
      rank != -91 || a.data != original.data || b.data != before_b.data ||
      singular != before_s) {
    return false;
  }
  if (!execute(work).ok() || rank != 2 || !report.called_provider ||
      report.native_info != 0 ||
      report.outcome != asc::LapackOutcome::kRankDecision ||
      report.output_validity != asc::LapackOutputValidity::kComplete ||
      !a.PaddingEquals(original.data) || !b.PaddingEquals(before_b.data)) {
    return false;
  }
  return Mathematics(original, before_b, b, singular);
}

template <typename T>
bool Run(const asc::ReferenceLapackProvider& provider) {
  bool passed = true;
  for (const auto driver : {Driver::kGelss, Driver::kGelsd}) {
    for (const auto a_layout : {kColumn, kRow}) {
      for (const auto b_layout : {kColumn, kRow}) {
        for (const bool preferred : {false, true}) {
          passed =
              Solve<T, 3, 2>(provider, driver, a_layout, b_layout, preferred) &&
              passed;
          passed =
              Solve<T, 2, 3>(provider, driver, a_layout, b_layout, preferred) &&
              passed;
          passed =
              Solve<T, 3, 4>(provider, driver, a_layout, b_layout, preferred) &&
              passed;
        }
      }
    }
  }
  return passed;
}
}  // namespace

int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const bool s = Run<float>(provider);
  const bool d = Run<double>(provider);
  const bool c = Run<std::complex<float>>(provider);
  const bool z = Run<std::complex<double>>(provider);
  if (!s || !d || !c || !z) {
    std::fprintf(stderr, "SVD_LS_PUBLIC_CONSUMER failed s=%d d=%d c=%d z=%d\n",
                 static_cast<int>(s), static_cast<int>(d), static_cast<int>(c),
                 static_cast<int>(z));
    return 1;
  }
  // Passing solutions do not close these required upstream source gates.
  std::puts(
      "SVD_LS_PUBLIC_CONSUMER passed_profiles=192 "
      "checks=singular_values,solution,optimality,nullspace,report,"
      "padding,rollback");
  std::puts(
      "required_upstream_gates_open=GELSD_nonzero_A_NRHS0,"
      "SC_GELSD_tree_boundary,GELSS_Path2a_A_output");
  return 0;
}
