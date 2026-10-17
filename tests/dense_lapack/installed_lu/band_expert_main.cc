#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>

#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_band_condition.h"
#include "asc/dense/providers/lapack_cholesky_band_driver.h"
#include "asc/dense/providers/lapack_cholesky_band_equilibration.h"
#include "asc/dense/providers/lapack_cholesky_band_expert.h"
#include "asc/dense/providers/lapack_cholesky_band_refinement.h"
#include "asc/dense/providers/lapack_cholesky_driver.h"
#include "factorization_support.h"
#include "normal_return_guard.h"

namespace {
using namespace installed_internal;  // NOLINT(google-build-using-namespace)
unsigned successful_calls = 0;
unsigned completed_workflows = 0;
bool CheckedSuccess(const asc::Status& status,
                    const asc::LapackReport& report) {
  const bool success = Succeeded(status, report);
  successful_calls += success ? 1U : 0U;
  return success;
}
constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;
constexpr auto kLower = asc::DenseBlasTriangle::kLower;
constexpr auto kNone = asc::LapackCholeskyEquilibration::kNone;

template <typename T, std::size_t N, std::size_t Kd>
struct Band {
  static constexpr std::size_t kLeading = Kd + 3;
  std::array<T, N * kLeading + 2> data{};
  asc::DenseBlasLayout layout;
  asc::DenseBlasTriangle triangle;

  [[nodiscard]] bool Selected(std::size_t i, std::size_t j) const {
    return triangle == kUpper ? i <= j && j - i <= Kd : j <= i && i - j <= Kd;
  }
  [[nodiscard]] std::size_t Offset(std::size_t i, std::size_t j) const {
    if (layout == kColumn) {
      return 1 + j * kLeading + (triangle == kUpper ? Kd - (j - i) : i - j);
    }
    return 1 + i * kLeading + (triangle == kUpper ? j - i : Kd - (i - j));
  }
  T& At(std::size_t i, std::size_t j) { return data[Offset(i, j)]; }
  [[nodiscard]] Wide Factor(std::size_t i, std::size_t j) const {
    return Selected(i, j) ? Widen(data[Offset(i, j)]) : Wide{};
  }
  auto View() {
    return Take(asc::LapackPositiveDefiniteBandView<T>::Create(
        data.data() + 1, N, Kd, triangle, layout, kLeading,
        {data.data(), sizeof(data), kHost}));
  }
  auto ConstView() {
    return Take(asc::LapackPositiveDefiniteBandView<const T>::Create(
        data.data() + 1, N, Kd, triangle, layout, kLeading,
        {data.data(), sizeof(data), kHost}));
  }
  [[nodiscard]] bool PaddingEquals(const decltype(data)& old) const {
    auto expected = old;
    for (std::size_t i = 0; i < N; ++i) {
      for (std::size_t j = 0; j < N; ++j) {
        if (Selected(i, j)) {
          expected[Offset(i, j)] = data[Offset(i, j)];
        }
      }
    }
    return expected == data;
  }
};

template <typename T>
using Real = asc::DenseBlasRealType<T>;

template <typename T>
Wide Phase(std::size_t i) {
  if constexpr (asc::DenseBlasComplex<T>) {
    return i == 1 ? Wide{0, 1} : Wide{1, 0};
  } else {
    return {1, 0};
  }
}

long double Diagonal(std::size_t i, bool scaled) {
  constexpr std::array<long double, 3> kScaling{0.0625L, 1.L, 16.L};
  return scaled ? kScaling[i] : 1.L;
}

template <typename T>
Wide Original(std::size_t i, std::size_t j, bool scaled) {
  constexpr std::array<std::array<int, 3>, 3> kMatrix{
      {{{4, 1, 0}}, {{1, 3, 1}}, {{0, 1, 2}}}};
  return static_cast<long double>(kMatrix[i][j]) * Phase<T>(i) *
         std::conj(Phase<T>(j)) * Diagonal(i, scaled) * Diagonal(j, scaled);
}

template <typename T>
long double Condition(bool scaled, std::array<Real<T>, 3> scales = {1, 1, 1}) {
  // Exact inverse of the unscaled real matrix is the numerator below /18.
  constexpr std::array<std::array<int, 3>, 3> kInverse{
      {{{5, -2, 1}}, {{-2, 8, -4}}, {{1, -4, 11}}}};
  long double norm = 0;
  long double inverse_norm = 0;
  for (std::size_t j = 0; j < 3; ++j) {
    long double column = 0;
    long double inverse_column = 0;
    for (std::size_t i = 0; i < 3; ++i) {
      column += std::abs(Original<T>(i, j, scaled)) * scales[i] * scales[j];
      inverse_column += std::abs(static_cast<long double>(kInverse[i][j])) /
                        (18 * Diagonal(i, scaled) * Diagonal(j, scaled) *
                         scales[i] * scales[j]);
    }
    norm = std::max(norm, column);
    inverse_norm = std::max(inverse_norm, inverse_column);
  }
  return 1 / (norm * inverse_norm);
}

template <typename T>
void Fill(Band<T, 3, 1>& band, bool scaled) {
  band.data.fill(Value<T>(-71, 29));
  for (std::size_t i = 0; i < 3; ++i) {
    for (std::size_t j = 0; j < 3; ++j) {
      if (band.Selected(i, j)) {
        const Wide value = Original<T>(i, j, scaled);
        band.At(i, j) = Value<T>(value.real(), i == j ? 73 : value.imag());
      }
    }
  }
}

template <typename T>
void Fill(Matrix<T, 3, 2>& rhs, bool scaled) {
  rhs.data.fill(Value<T>(-79, 31));
  for (std::size_t i = 0; i < 3; ++i) {
    for (std::size_t j = 0; j < 2; ++j) {
      Wide value{};
      for (std::size_t k = 0; k < 3; ++k) {
        value += Original<T>(i, k, scaled) * Widen(Value<T>(1 + k + j, 0.5));
      }
      rhs.At(i, j) = Value<T>(value.real(), value.imag());
    }
  }
}

template <typename T>
bool Solution(const Matrix<T, 3, 2>& x, bool scaled) {
  for (std::size_t i = 0; i < 3; ++i) {
    for (std::size_t j = 0; j < 2; ++j) {
      Wide residual{};
      long double denominator = 0;
      for (std::size_t k = 0; k < 3; ++k) {
        const Wide expected = Widen(Value<T>(1 + k + j, 0.5));
        residual += Original<T>(i, k, scaled) * (Widen(x.At(k, j)) - expected);
        denominator += std::abs(Original<T>(i, k, scaled)) * std::abs(expected);
      }
      if (!Near<T>(residual, {}, denominator) ||
          !Near<T>(Widen(x.At(i, j)), Widen(Value<T>(1 + i + j, 0.5)), 8)) {
        return false;
      }
    }
  }
  return true;
}

template <typename T>
bool Reconstruct(const Band<T, 3, 1>& factor, bool scaled) {
  for (std::size_t i = 0; i < 3; ++i) {
    for (std::size_t j = 0; j < 3; ++j) {
      Wide product{};
      for (std::size_t k = 0; k < 3; ++k) {
        product += factor.triangle == kLower
                       ? factor.Factor(i, k) * std::conj(factor.Factor(j, k))
                       : std::conj(factor.Factor(k, i)) * factor.Factor(k, j);
      }
      if (!Near<T>(product, Original<T>(i, j, scaled),
                   8 * Diagonal(i, scaled) * Diagonal(j, scaled))) {
        return false;
      }
    }
  }
  return true;
}

template <typename T>
struct Work {
  std::array<T, 130> scalar{};
  std::array<T, 66> packing{};
  std::array<Real<T>, 18> real{};
  alignas(std::max_align_t) std::array<std::byte, 80> integer{};
  asc::LapackWorkspace view;

  Work() {
    scalar.fill(Value<T>(-83, 37));
    packing.fill(Value<T>(-89, 41));
    real.fill(Real<T>{-97});
    integer.fill(std::byte{0x5a});
    view.regions[static_cast<std::size_t>(asc::LapackWorkspaceKind::kScalar)] =
        {scalar.data() + 1, 128 * sizeof(T), kHost};
    view.regions[static_cast<std::size_t>(asc::LapackWorkspaceKind::kReal)] = {
        real.data() + 1, 16 * sizeof(Real<T>), kHost};
    view.regions[static_cast<std::size_t>(asc::LapackWorkspaceKind::kInteger)] =
        {integer.data() + 16, 48, kHost};
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
           std::all_of(
               integer.begin(), integer.begin() + 16,
               [](std::byte value) { return value == std::byte{0x5a}; }) &&
           std::all_of(integer.end() - 16, integer.end(), [](std::byte value) {
             return value == std::byte{0x5a};
           });
  }
};

template <typename T>
struct Case {
  Band<T, 3, 1> a;
  Band<T, 3, 1> af;
  Matrix<T, 3, 2> b;
  Matrix<T, 3, 2> x;
  std::array<Real<T>, 3> scales{};
  std::array<Real<T>, 2> ferr{};
  std::array<Real<T>, 2> berr{};
  Real<T> rcond = -1;
  asc::LapackReport report;
  Work<T> work;
  bool scaled;
};

template <typename T>
bool Basic(const asc::ReferenceLapackProvider& provider, Case<T>& c) {
  const auto original = c.a.data;
  const auto input = c.x.data;
  asc::LapackBandEquilibrationStatistics<Real<T>> stats;
  const auto eq = Take(asc::QueryPbequWorkspace(provider, c.a.ConstView(),
                                                Vector(c.scales), stats));
  if (!CheckedSuccess(asc::Pbequ(provider, c.a.ConstView(), Vector(c.scales),
                                 stats, eq, c.work.view, c.report),
                      c.report) ||
      c.a.data != original) {
    return false;
  }
  for (std::size_t i = 0; i < 3; ++i) {
    const long double expected =
        1 / std::sqrt(Original<T>(i, i, c.scaled).real());
    if (!Near<T>(Widen(c.scales[i]), {expected, 0}, expected)) {
      return false;
    }
  }
  const auto old = c.af.data;
  const auto sv =
      Take(asc::QueryPbsvWorkspace(provider, c.af.View(), c.x.View()));
  if (!CheckedSuccess(asc::Pbsv(provider, c.af.View(), c.x.View(), sv,
                                c.work.view, c.report),
                      c.report) ||
      !c.af.PaddingEquals(old) || !c.x.PaddingEquals(input) ||
      !Reconstruct(c.af, c.scaled) || !Solution(c.x, c.scaled)) {
    return false;
  }
  long double norm = 0;
  for (std::size_t j = 0; j < 3; ++j) {
    long double sum = 0;
    for (std::size_t i = 0; i < 3; ++i) {
      sum += std::abs(Original<T>(i, j, c.scaled));
    }
    norm = std::max(norm, sum);
  }
  const auto factor = c.af.data;
  const auto con = Take(asc::QueryPbconWorkspace(
      provider, c.af.ConstView(), static_cast<Real<T>>(norm), c.rcond));
  return CheckedSuccess(
             asc::Pbcon(provider, c.af.ConstView(), static_cast<Real<T>>(norm),
                        c.rcond, con, c.work.view, c.report),
             c.report) &&
         c.af.data == factor &&
         Near<T>(Widen(c.rcond), {Condition<T>(c.scaled), 0},
                 Condition<T>(c.scaled));
}

template <typename T>
bool Refine(const asc::ReferenceLapackProvider& provider, Case<T>& c) {
  for (std::size_t i = 0; i < 3; ++i) {
    for (std::size_t j = 0; j < 2; ++j) {
      c.x.At(i, j) += Value<T>(0.125, 0.0625);
    }
  }
  const auto a = c.a.data;
  const auto af = c.af.data;
  const auto b = c.b.data;
  const auto x = c.x.data;
  const auto plan = Take(asc::QueryPbrfsWorkspace(
      provider, c.a.ConstView(), c.af.ConstView(), c.b.ConstView(), c.x.View(),
      Vector(c.ferr), Vector(c.berr)));
  return CheckedSuccess(asc::Pbrfs(provider, c.a.ConstView(), c.af.ConstView(),
                                   c.b.ConstView(), c.x.View(), Vector(c.ferr),
                                   Vector(c.berr), plan, c.work.view, c.report),
                        c.report) &&
         c.a.data == a && c.af.data == af && c.b.data == b &&
         c.x.PaddingEquals(x) && Solution(c.x, c.scaled);
}

template <typename T>
bool Expert(const asc::ReferenceLapackProvider& provider, Case<T>& c) {
  const auto a = c.a.data;
  const auto b = c.b.data;
  const auto x = c.x.data;
  const auto af = c.af.data;
  auto plan = Take(asc::QueryPbsvxWorkspace(
      provider, c.a.ConstView(), c.af.View(), c.b.ConstView(), c.x.View(),
      Vector(c.ferr), Vector(c.berr), c.rcond));
  if (!CheckedSuccess(
          asc::Pbsvx(provider, c.a.ConstView(), c.af.View(), c.b.ConstView(),
                     c.x.View(), Vector(c.ferr), Vector(c.berr), c.rcond, plan,
                     c.work.view, c.report),
          c.report) ||
      c.a.data != a || c.b.data != b || !c.af.PaddingEquals(af) ||
      !c.x.PaddingEquals(x) || !Reconstruct(c.af, c.scaled) ||
      !Solution(c.x, c.scaled)) {
    return false;
  }
  const auto factored = c.af.data;
  plan = Take(asc::QueryPbsvxFactoredWorkspace(
      provider, c.a.ConstView(), c.af.ConstView(), kNone, ConstVector(c.scales),
      c.b.View(), c.x.View(), Vector(c.ferr), Vector(c.berr), c.rcond));
  return CheckedSuccess(
             asc::PbsvxFactored(provider, c.a.ConstView(), c.af.ConstView(),
                                kNone, ConstVector(c.scales), c.b.View(),
                                c.x.View(), Vector(c.ferr), Vector(c.berr),
                                c.rcond, plan, c.work.view, c.report),
             c.report) &&
         c.a.data == a && c.af.data == factored && c.b.data == b &&
         c.x.PaddingEquals(x) && Solution(c.x, c.scaled);
}

template <typename T>
bool ReuseScaled(const asc::ReferenceLapackProvider& provider, Case<T>& c,
                 asc::LapackCholeskyEquilibration equed) {
  const auto a = c.a.data;
  const auto af = c.af.data;
  const auto scales = c.scales;
  Fill(c.b, c.scaled);
  const auto b = c.b.data;
  const auto x = c.x.data;
  const auto plan = Take(asc::QueryPbsvxFactoredWorkspace(
      provider, c.a.ConstView(), c.af.ConstView(), equed, ConstVector(c.scales),
      c.b.View(), c.x.View(), Vector(c.ferr), Vector(c.berr), c.rcond));
  if (!CheckedSuccess(
          asc::PbsvxFactored(provider, c.a.ConstView(), c.af.ConstView(), equed,
                             ConstVector(c.scales), c.b.View(), c.x.View(),
                             Vector(c.ferr), Vector(c.berr), c.rcond, plan,
                             c.work.view, c.report),
          c.report) ||
      c.a.data != a || c.af.data != af || c.scales != scales ||
      !c.b.PaddingEquals(b) || !c.x.PaddingEquals(x) ||
      !Solution(c.x, c.scaled)) {
    return false;
  }
  auto stale = plan;
  ++stale.regions[static_cast<std::size_t>(asc::LapackWorkspaceKind::kScalar)]
        .minimum_entries;
  const auto rhs = c.b.data;
  const auto solution = c.x.data;
  const auto ferr = c.ferr;
  const auto berr = c.berr;
  const auto rcond = c.rcond;
  const auto scalar_work = c.work.scalar;
  const auto real_work = c.work.real;
  const auto integer_work = c.work.integer;
  const auto packing = c.work.packing;
  const auto status = asc::PbsvxFactored(
      provider, c.a.ConstView(), c.af.ConstView(), equed, ConstVector(c.scales),
      c.b.View(), c.x.View(), Vector(c.ferr), Vector(c.berr), c.rcond, stale,
      c.work.view, c.report);
  return !status.ok() && !c.report.called_provider &&
         c.report.output_validity == asc::LapackOutputValidity::kUnchanged &&
         !c.report.native_info.has_value() && c.a.data == a &&
         c.af.data == af && c.scales == scales && c.b.data == rhs &&
         c.x.data == solution && c.ferr == ferr && c.berr == berr &&
         c.rcond == rcond && c.work.scalar == scalar_work &&
         c.work.real == real_work && c.work.integer == integer_work &&
         c.work.packing == packing;
}

template <typename T>
bool Equilibrated(const asc::ReferenceLapackProvider& provider, Case<T>& c) {
  const auto a = c.a.data;
  const auto af = c.af.data;
  const auto b = c.b.data;
  const auto x = c.x.data;
  auto equed = kNone;
  const auto plan = Take(asc::QueryPbsvxEquilibratedWorkspace(
      provider, c.a.View(), c.af.View(), equed, Vector(c.scales), c.b.View(),
      c.x.View(), Vector(c.ferr), Vector(c.berr), c.rcond));
  if (!CheckedSuccess(
          asc::PbsvxEquilibrated(provider, c.a.View(), c.af.View(), equed,
                                 Vector(c.scales), c.b.View(), c.x.View(),
                                 Vector(c.ferr), Vector(c.berr), c.rcond, plan,
                                 c.work.view, c.report),
          c.report) ||
      !c.a.PaddingEquals(a) || !c.af.PaddingEquals(af) ||
      !c.b.PaddingEquals(b) || !c.x.PaddingEquals(x) ||
      !Solution(c.x, c.scaled) ||
      (c.scaled ? equed == kNone : equed != kNone)) {
    return false;
  }
  const auto expected = equed == kNone ? Condition<T>(c.scaled)
                                       : Condition<T>(c.scaled, c.scales);
  return Near<T>(Widen(c.rcond), {expected, 0}, expected) &&
         ReuseScaled(provider, c, equed);
}

template <typename T>
bool Exercise(const asc::ReferenceLapackProvider& provider) {
  for (const auto triangle : {kUpper, kLower}) {
    for (unsigned layout = 0; layout < 16; ++layout) {
      for (const bool scaled : {false, true}) {
        const auto select = [layout](unsigned bit) {
          return layout & (1U << bit) ? kRow : kColumn;
        };
        Case<T> c{{{}, select(0), triangle},
                  {{}, select(1), triangle},
                  {{}, select(2)},
                  {{}, select(3)},
                  {},
                  {},
                  {},
                  -1,
                  {},
                  {},
                  scaled};
        Fill(c.a, scaled);
        Fill(c.af, scaled);
        Fill(c.b, scaled);
        Fill(c.x, scaled);
        if (!Basic(provider, c)) {
          std::fprintf(stderr, "basic layout=%u scaled=%d\n", layout, scaled);
          return false;
        }
        if (!Refine(provider, c) || !Expert(provider, c) ||
            !Equilibrated(provider, c) || !c.work.Guards()) {
          return false;
        }
        ++completed_workflows;
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
      !Exercise<std::complex<double>>(provider)) {
    return 1;
  }
  if (completed_workflows != 256 || successful_calls != 2048) {
    return 1;
  }
  std::printf(
      "BAND_EXPERT_PUBLIC workflows=%u routes=20 calls=%u rejections=%u "
      "math_gates_remain_open=1\n",
      completed_workflows, successful_calls, completed_workflows);
  return 0;
}
