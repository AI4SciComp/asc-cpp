#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>

#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_general_band.h"
#include "asc/dense/providers/lapack_lu_band_condition.h"
#include "asc/dense/providers/lapack_lu_band_driver.h"
#include "asc/dense/providers/lapack_lu_band_equilibration.h"
#include "asc/dense/providers/lapack_lu_band_expert.h"
#include "asc/dense/providers/lapack_lu_band_refinement.h"
#include "asc/dense/providers/lapack_lu_condition.h"
#include "asc/dense/providers/lapack_lu_driver.h"
#include "asc/dense/providers/lapack_lu_equilibration.h"
#include "factorization_support.h"
#include "normal_return_guard.h"

namespace {
using namespace installed_internal;  // NOLINT(google-build-using-namespace)
constexpr auto kNoTrans = asc::DenseBlasTranspose::kNone;
constexpr auto kTranspose = asc::DenseBlasTranspose::kTranspose;
constexpr auto kConjugate = asc::DenseBlasTranspose::kConjugateTranspose;
unsigned successful_calls = 0;
unsigned completed_workflows = 0;

bool Success(const asc::Status& status, const asc::LapackReport& report) {
  const bool ok = Succeeded(status, report);
  successful_calls += ok ? 1U : 0U;
  if (!ok) {
    std::fprintf(stderr, "GB public call failed: status=%d outcome=%d\n",
                 static_cast<int>(status.code()),
                 static_cast<int>(report.outcome));
  }
  return ok;
}

template <typename T>
using Real = asc::DenseBlasRealType<T>;

template <typename T>
Wide Phase(std::size_t i) {
  if constexpr (asc::DenseBlasComplex<T>) {
    return i == 1 ? Wide{0, 1} : Wide{1, 0};
  }
  return {1, 0};
}

template <typename T>
Wide Original(std::size_t i, std::size_t j, bool scaled) {
  constexpr std::array<std::array<int, 3>, 3> kA{
      {{{0, 2, 0}}, {{1, 3, 4}}, {{0, 5, 6}}}};
  constexpr std::array<long double, 3> kRows{0.0625L, 1.L, 16.L};
  constexpr std::array<long double, 3> kColumns{8.L, 0.125L, 1.L};
  return static_cast<long double>(kA[i][j]) * Phase<T>(i) *
         std::conj(Phase<T>(j)) * (scaled ? kRows[i] * kColumns[j] : 1.L);
}

template <typename T>
Wide Operated(std::size_t i, std::size_t j, bool scaled,
              asc::DenseBlasTranspose operation) {
  if (operation == kNoTrans) {
    return Original<T>(i, j, scaled);
  }
  const Wide value = Original<T>(j, i, scaled);
  return operation == kConjugate ? std::conj(value) : value;
}

template <typename T>
Wide Inverse(std::size_t i, std::size_t j, bool scaled,
             asc::DenseBlasTranspose operation) {
  // A0 inverse is the exact integer numerator /12. Diagonal row/column
  // scaling is inverted in the opposite order; phase factors are unitary.
  constexpr std::array<std::array<int, 3>, 3> kNumerator{
      {{{2, 12, -8}}, {{6, 0, 0}}, {{-5, 0, 2}}}};
  constexpr std::array<long double, 3> kRows{0.0625L, 1.L, 16.L};
  constexpr std::array<long double, 3> kColumns{8.L, 0.125L, 1.L};
  if (operation != kNoTrans) {
    std::swap(i, j);
  }
  const Wide value = static_cast<long double>(kNumerator[i][j]) / 12 *
                     Phase<T>(i) * std::conj(Phase<T>(j)) /
                     (scaled ? kColumns[i] * kRows[j] : 1.L);
  return operation == kConjugate ? std::conj(value) : value;
}

template <typename T, bool Expanded>
struct Band {
  static constexpr std::size_t kLeading = Expanded ? 6 : 5;
  static constexpr std::size_t kDiagonal = Expanded ? 2 : 1;
  std::array<T, 3 * kLeading + 2> data{};

  [[nodiscard]] std::size_t Offset(std::size_t i, std::size_t j) const {
    return 1 + j * kLeading + kDiagonal + i - j;
  }
  T& At(std::size_t i, std::size_t j) { return data[Offset(i, j)]; }
  [[nodiscard]] const T& At(std::size_t i, std::size_t j) const {
    return data[Offset(i, j)];
  }
  auto View() {
    if constexpr (Expanded) {
      return Take(asc::LapackLuBandView<T>::Create(
          data.data() + 1, 3, 3, 1, 1, kLeading,
          {data.data(), sizeof(data), kHost}));
    } else {
      return Take(asc::ReferenceGeneralBandView<T>::Create(
          data.data() + 1, 3, 3, 1, 1, kLeading,
          {data.data(), sizeof(data), kHost}));
    }
  }
  auto ConstView() {
    if constexpr (Expanded) {
      return Take(asc::LapackLuBandView<const T>::Create(
          data.data() + 1, 3, 3, 1, 1, kLeading,
          {data.data(), sizeof(data), kHost}));
    } else {
      return Take(asc::ReferenceGeneralBandView<const T>::Create(
          data.data() + 1, 3, 3, 1, 1, kLeading,
          {data.data(), sizeof(data), kHost}));
    }
  }
  void Fill(bool scaled) {
    data.fill(Value<T>(-71, 29));
    for (std::size_t i = 0; i < 3; ++i) {
      for (std::size_t j = 0; j < 3; ++j) {
        if (i <= j + 1 && j <= i + 1) {
          const Wide value = Original<T>(i, j, scaled);
          At(i, j) = Value<T>(value.real(), value.imag());
        }
      }
    }
  }
  [[nodiscard]] bool PaddingEquals(const decltype(data)& old) const {
    auto expected = old;
    for (std::size_t j = 0; j < 3; ++j) {
      if constexpr (Expanded) {
        for (std::size_t row = 0; row < 4; ++row) {
          expected[1 + j * kLeading + row] = data[1 + j * kLeading + row];
        }
      } else {
        for (std::size_t i = 0; i < 3; ++i) {
          if (i <= j + 1 && j <= i + 1) {
            expected[Offset(i, j)] = At(i, j);
          }
        }
      }
    }
    return expected == data;
  }
};

template <typename T>
struct Work {
  std::array<T, 66> scalar{};
  std::array<T, 66> packing{};
  std::array<Real<T>, 18> real{};
  alignas(std::max_align_t) std::array<std::byte, 80> integers{};
  asc::LapackWorkspace view;

  Work() {
    scalar.fill(Value<T>(-83, 37));
    packing.fill(Value<T>(-89, 41));
    real.fill(Real<T>{-97});
    integers.fill(std::byte{0x5a});
    view.regions[static_cast<std::size_t>(asc::LapackWorkspaceKind::kScalar)] =
        {scalar.data() + 1, 64 * sizeof(T), kHost};
    view.regions[static_cast<std::size_t>(asc::LapackWorkspaceKind::kReal)] = {
        real.data() + 1, 16 * sizeof(Real<T>), kHost};
    view.regions[static_cast<std::size_t>(asc::LapackWorkspaceKind::kInteger)] =
        {integers.data() + 16, 48, kHost};
    view.regions[static_cast<std::size_t>(
        asc::LapackWorkspaceKind::kLayoutConversion)] = {packing.data() + 1,
                                                         64 * sizeof(T), kHost};
  }
  [[nodiscard]] bool Guards() const {
    return scalar.front() == Value<T>(-83, 37) &&
           scalar.back() == scalar.front() &&
           packing.front() == Value<T>(-89, 41) &&
           packing.back() == packing.front() && real.front() == Real<T>{-97} &&
           real.back() == real.front() &&
           std::all_of(integers.begin(), integers.begin() + 16,
                       [](std::byte b) { return b == std::byte{0x5a}; }) &&
           std::all_of(integers.end() - 16, integers.end(),
                       [](std::byte b) { return b == std::byte{0x5a}; });
  }
};

template <typename T>
struct Case {
  Band<T, false> a;
  Band<T, true> af;
  Matrix<T, 3, 2> b;
  Matrix<T, 3, 2> x;
  std::array<asc::index_t, 3> pivots{};
  std::array<Real<T>, 3> rows{};
  std::array<Real<T>, 3> columns{};
  std::array<Real<T>, 2> ferr{};
  std::array<Real<T>, 2> berr{};
  asc::LapackSolveStatistics<Real<T>> statistics;
  asc::LapackReport report;
  Work<T> work;
  bool scaled;
  asc::DenseBlasTranspose operation;

  Case(asc::DenseBlasLayout b_layout, asc::DenseBlasLayout x_layout, bool scale,
       asc::DenseBlasTranspose op)
      : b{{}, b_layout}, x{{}, x_layout}, scaled(scale), operation(op) {
    a.Fill(scaled);
    af.Fill(scaled);
    x.data.fill(Value<T>(-79, 31));
  }
  auto PivotView() {
    return Take(asc::ReferenceLuBandPivotView::Create(ConstVector(pivots)));
  }
  void FillRhs(asc::DenseBlasTranspose op) {
    b.data.fill(Value<T>(-73, 31));
    for (std::size_t i = 0; i < 3; ++i) {
      for (std::size_t rhs = 0; rhs < 2; ++rhs) {
        Wide value{};
        for (std::size_t k = 0; k < 3; ++k) {
          value +=
              Operated<T>(i, k, scaled, op) * Widen(Value<T>(1 + k + rhs, 0.5));
        }
        b.At(i, rhs) = Value<T>(value.real(), value.imag());
      }
    }
  }
  [[nodiscard]] long double ForwardScale(std::size_t row, std::size_t rhs,
                                         asc::DenseBlasTranspose op) const {
    // The shared Near helper is restricted to well-conditioned fixtures.
    // Use the exact inverse to propagate its unchanged backward-error budget
    // for the explicitly scaled fixture, instead of a fixed absolute bound.
    long double sensitivity = 0;
    for (std::size_t j = 0; j < 3; ++j) {
      long double magnitude = 0;
      for (std::size_t k = 0; k < 3; ++k) {
        magnitude += std::abs(Operated<T>(j, k, scaled, op)) *
                     std::abs(Widen(Value<T>(1 + k + rhs, 0.5)));
      }
      sensitivity += std::abs(Inverse<T>(row, j, scaled, op)) * magnitude;
    }
    return sensitivity;
  }
  [[nodiscard]] bool Solution(const Matrix<T, 3, 2>& solution,
                              asc::DenseBlasTranspose op) const {
    for (std::size_t i = 0; i < 3; ++i) {
      for (std::size_t rhs = 0; rhs < 2; ++rhs) {
        Wide residual{};
        long double denominator = 0;
        for (std::size_t k = 0; k < 3; ++k) {
          const Wide expected = Widen(Value<T>(1 + k + rhs, 0.5));
          residual += Operated<T>(i, k, scaled, op) *
                      (Widen(solution.At(k, rhs)) - expected);
          denominator +=
              std::abs(Operated<T>(i, k, scaled, op)) * std::abs(expected);
        }
        if (!Near<T>(residual, {}, denominator) ||
            !Near<T>(Widen(solution.At(i, rhs)),
                     Widen(Value<T>(1 + i + rhs, 0.5)),
                     scaled ? ForwardScale(i, rhs, op) : 8)) {
          return false;
        }
      }
    }
    return true;
  }
  [[nodiscard]] bool Reconstruct() const {
    std::array<std::array<Wide, 3>, 3> restored{};
    for (std::size_t i = 0; i < 3; ++i) {
      for (std::size_t j = i; j < 3; ++j) {
        restored[i][j] = Widen(af.At(i, j));
      }
    }
    for (int j = 2; j >= 0; --j) {
      if (pivots[j] < j + 1 || pivots[j] > std::min(3, j + 2)) {
        return false;
      }
      if (j < 2) {
        for (std::size_t k = 0; k < 3; ++k) {
          restored[j + 1][k] += Widen(af.At(j + 1, j)) * restored[j][k];
        }
      }
      std::swap(restored[j], restored[pivots[j] - 1]);
    }
    for (std::size_t i = 0; i < 3; ++i) {
      for (std::size_t j = 0; j < 3; ++j) {
        const Wide expected =
            i <= j + 1 && j <= i + 1 ? Widen(a.At(i, j)) : Wide{};
        if (!Near<T>(restored[i][j], expected, 1 + std::abs(expected))) {
          return false;
        }
      }
    }
    return true;
  }
};

template <typename T>
bool Basic(const asc::ReferenceLapackProvider& provider, Case<T>& c) {
  c.FillRhs(kNoTrans);
  const auto af = c.af.data;
  const auto b = c.b.data;
  auto plan = Take(asc::QueryGbsvWorkspace(provider, c.af.View(),
                                           Vector(c.pivots), c.b.View()));
  if (!Success(asc::Gbsv(provider, c.af.View(), Vector(c.pivots), c.b.View(),
                         plan, c.work.view, c.report),
               c.report) ||
      !c.Reconstruct() || !c.Solution(c.b, kNoTrans) ||
      !c.af.PaddingEquals(af) || !c.b.PaddingEquals(b)) {
    return false;
  }
  asc::LapackEquilibrationStatistics<Real<T>> statistics;
  plan =
      Take(asc::QueryGbequWorkspace(provider, c.a.ConstView(), Vector(c.rows),
                                    Vector(c.columns), statistics));
  if (!Success(asc::Gbequ(provider, c.a.ConstView(), Vector(c.rows),
                          Vector(c.columns), statistics, plan, c.work.view,
                          c.report),
               c.report)) {
    return false;
  }
  for (const auto norm :
       {asc::LapackConditionNorm::kOne, asc::LapackConditionNorm::kInfinity}) {
    long double original_norm = 0;
    for (std::size_t i = 0; i < 3; ++i) {
      long double sum = 0;
      for (std::size_t j = 0; j < 3; ++j) {
        sum += std::abs(norm == asc::LapackConditionNorm::kOne
                            ? Original<T>(j, i, c.scaled)
                            : Original<T>(i, j, c.scaled));
      }
      original_norm = std::max(original_norm, sum);
    }
    Real<T> condition = -1;
    plan = Take(asc::QueryGbconWorkspace(
        provider, norm, c.af.ConstView(), c.PivotView(),
        static_cast<Real<T>>(original_norm), condition));
    if (!Success(asc::Gbcon(provider, norm, c.af.ConstView(), c.PivotView(),
                            static_cast<Real<T>>(original_norm), condition,
                            plan, c.work.view, c.report),
                 c.report) ||
        !std::isfinite(condition) || condition <= 0 || condition > 1) {
      return false;
    }
  }
  c.FillRhs(c.operation);
  for (std::size_t i = 0; i < 3; ++i) {
    for (std::size_t j = 0; j < 2; ++j) {
      c.x.At(i, j) = Value<T>(static_cast<double>(1 + i + j) + 0.125, 0.5);
    }
  }
  const auto original_b = c.b.data;
  plan = Take(asc::QueryGbrfsWorkspace(
      provider, c.operation, c.a.ConstView(), c.af.ConstView(), c.PivotView(),
      c.b.ConstView(), c.x.View(), Vector(c.ferr), Vector(c.berr)));
  return Success(asc::Gbrfs(provider, c.operation, c.a.ConstView(),
                            c.af.ConstView(), c.PivotView(), c.b.ConstView(),
                            c.x.View(), Vector(c.ferr), Vector(c.berr), plan,
                            c.work.view, c.report),
                 c.report) &&
         c.Solution(c.x, c.operation) && c.b.data == original_b;
}

template <typename T>
bool Reuse(const asc::ReferenceLapackProvider& provider, Case<T>& c,
           asc::LapackEquilibration equilibration) {
  c.FillRhs(c.operation);
  const auto a = c.a.data;
  const auto af = c.af.data;
  const auto pivots = c.pivots;
  const auto rows = c.rows;
  const auto columns = c.columns;
  const auto plan = Take(asc::QueryGbsvxFactoredWorkspace(
      provider, c.operation, c.a.ConstView(), c.af.ConstView(), c.PivotView(),
      equilibration, ConstVector(c.rows), ConstVector(c.columns), c.b.View(),
      c.x.View(), Vector(c.ferr), Vector(c.berr), c.statistics));
  return Success(asc::GbsvxFactored(provider, c.operation, c.a.ConstView(),
                                    c.af.ConstView(), c.PivotView(),
                                    equilibration, ConstVector(c.rows),
                                    ConstVector(c.columns), c.b.View(),
                                    c.x.View(), Vector(c.ferr), Vector(c.berr),
                                    c.statistics, plan, c.work.view, c.report),
                 c.report) &&
         c.Solution(c.x, c.operation) && c.a.data == a && c.af.data == af &&
         c.pivots == pivots && c.rows == rows && c.columns == columns;
}

template <typename T>
bool Expert(const asc::ReferenceLapackProvider& provider, Case<T>& c) {
  c.FillRhs(c.operation);
  const auto a = c.a.data;
  const auto b = c.b.data;
  auto plan = Take(asc::QueryGbsvxWorkspace(
      provider, c.operation, c.a.ConstView(), c.af.View(), Vector(c.pivots),
      c.b.ConstView(), c.x.View(), Vector(c.ferr), Vector(c.berr),
      c.statistics));
  if (!Success(asc::Gbsvx(provider, c.operation, c.a.ConstView(), c.af.View(),
                          Vector(c.pivots), c.b.ConstView(), c.x.View(),
                          Vector(c.ferr), Vector(c.berr), c.statistics, plan,
                          c.work.view, c.report),
               c.report) ||
      !c.Reconstruct() || !c.Solution(c.x, c.operation) || c.a.data != a ||
      c.b.data != b) {
    return false;
  }
  const auto x = c.x.data;
  const auto different = c.operation == kNoTrans ? kTranspose : kNoTrans;
  const auto stale =
      asc::Gbsvx(provider, different, c.a.ConstView(), c.af.View(),
                 Vector(c.pivots), c.b.ConstView(), c.x.View(), Vector(c.ferr),
                 Vector(c.berr), c.statistics, plan, c.work.view, c.report);
  if (stale.ok() || c.report.called_provider || c.x.data != x ||
      c.a.data != a || c.b.data != b ||
      !Reuse(provider, c, asc::LapackEquilibration::kNone)) {
    return false;
  }
  c.FillRhs(c.operation);
  auto equilibration = asc::LapackEquilibration::kNone;
  plan = Take(asc::QueryGbsvxEquilibratedWorkspace(
      provider, c.operation, c.a.View(), c.af.View(), Vector(c.pivots),
      equilibration, Vector(c.rows), Vector(c.columns), c.b.View(), c.x.View(),
      Vector(c.ferr), Vector(c.berr), c.statistics));
  return Success(asc::GbsvxEquilibrated(
                     provider, c.operation, c.a.View(), c.af.View(),
                     Vector(c.pivots), equilibration, Vector(c.rows),
                     Vector(c.columns), c.b.View(), c.x.View(), Vector(c.ferr),
                     Vector(c.berr), c.statistics, plan, c.work.view, c.report),
                 c.report) &&
         c.Reconstruct() && c.Solution(c.x, c.operation) &&
         c.a.PaddingEquals(a) && c.b.PaddingEquals(b) &&
         Reuse(provider, c, equilibration);
}

template <typename T>
bool VerifierControls() {
  for (const auto op : {kNoTrans, kTranspose, kConjugate}) {
    for (const bool scaled : {false, true}) {
      for (std::size_t i = 0; i < 3; ++i) {
        for (std::size_t j = 0; j < 3; ++j) {
          Wide product{};
          long double magnitude = 0;
          for (std::size_t k = 0; k < 3; ++k) {
            const Wide term =
                Operated<T>(i, k, scaled, op) * Inverse<T>(k, j, scaled, op);
            product += term;
            magnitude += std::abs(term);
          }
          if (std::abs(product - Wide{i == j ? 1.L : 0.L, 0}) >
              64 * std::numeric_limits<long double>::epsilon() * magnitude) {
            return false;
          }
        }
      }
      Case<T> c(kColumn, kColumn, scaled, op);
      for (std::size_t i = 0; i < 3; ++i) {
        for (std::size_t j = 0; j < 2; ++j) {
          c.x.At(i, j) = Value<T>(1 + i + j, 0.5);
        }
      }
      if (!c.Solution(c.x, op)) {
        return false;
      }
      c.x.At(0, 0) += T{1};
      if (c.Solution(c.x, op)) {
        return false;
      }
    }
  }
  return true;
}

template <typename T>
bool Exercise(const asc::ReferenceLapackProvider& provider) {
  if (!VerifierControls<T>()) {
    return false;
  }
  for (const auto b_layout : {kColumn, kRow}) {
    for (const auto x_layout : {kColumn, kRow}) {
      for (const auto operation : {kNoTrans, kTranspose, kConjugate}) {
        for (const bool scaled : {false, true}) {
          Case<T> c(b_layout, x_layout, scaled, operation);
          if (!Basic(provider, c) || !Expert(provider, c) || !c.work.Guards()) {
            std::fprintf(stderr,
                         "GB public workflow failed: bytes=%zu b=%d x=%d op=%d "
                         "scaled=%d\n",
                         sizeof(T), static_cast<int>(b_layout),
                         static_cast<int>(x_layout),
                         static_cast<int>(operation), scaled);
            return false;
          }
          ++completed_workflows;
        }
      }
    }
  }
  return true;
}
}  // namespace

int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  if (!Exercise<float>(provider) || !Exercise<double>(provider) ||
      !Exercise<std::complex<float>>(provider) ||
      !Exercise<std::complex<double>>(provider) || completed_workflows != 96 ||
      successful_calls != 864) {
    return 1;
  }
  std::printf(
      "GB_EXPERT_PUBLIC workflows=%u routes=20 calls=%u stale_rejections=%u "
      "math_gates_remain_open=1\n",
      completed_workflows, successful_calls, completed_workflows);
  return 0;
}
