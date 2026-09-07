#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu_condition.h"
#include "asc/dense/providers/lapack_lu_driver.h"
#include "asc/dense/providers/lapack_lu_refinement.h"

namespace {
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;
using Wide = std::complex<long double>;

template <typename T>
T Take(asc::Result<T> value) {
  if (!value.ok()) {
    std::abort();
  }
  return std::move(*value);
}

template <typename T>
T Value(double real, double imaginary = 0) {
  using Real = asc::DenseBlasRealType<T>;
  if constexpr (asc::DenseBlasComplex<T>) {
    return {static_cast<Real>(real), static_cast<Real>(imaginary)};
  } else {
    return static_cast<T>(real);
  }
}

template <typename T>
Wide Widen(T value) {
  if constexpr (asc::DenseBlasComplex<T>) {
    return {value.real(), value.imag()};
  } else {
    return {value, 0};
  }
}

template <typename T>
struct Matrix {
  std::array<T, 4> data{};
  asc::DenseBlasLayout layout;

  T& At(std::size_t row, std::size_t column) {
    return data[layout == kColumn ? 2 * column + row : 2 * row + column];
  }
  auto View() {
    return Take(asc::DenseBlasMatrixView<T>::Create(
        data.data(), 2, 2, layout, 2, {data.data(), sizeof(data), kHost}));
  }
  auto ConstView() { return asc::DenseBlasMatrixView<const T>(View()); }
};

template <typename T, std::size_t N>
auto Vector(std::array<T, N>& data) {
  return Take(asc::DenseBlasVectorView<T>::Create(
      data.data(), N, 1, {data.data(), sizeof(data), kHost}));
}

template <typename T, std::size_t N>
auto ConstVector(std::array<T, N>& data) {
  return asc::DenseBlasVectorView<const T>(Vector(data));
}

template <typename T>
struct Scratch {
  using Real = asc::DenseBlasRealType<T>;
  std::array<T, 16> scalars{};
  std::array<Real, 8> reals{};
  std::array<T, 16> packing{};
  alignas(std::max_align_t) std::array<std::byte, 32> integers{};

  asc::LapackWorkspace View() {
    asc::LapackWorkspace result;
    result.regions[static_cast<std::size_t>(
        asc::LapackWorkspaceKind::kScalar)] = {scalars.data(), sizeof(scalars),
                                               kHost};
    result.regions[static_cast<std::size_t>(asc::LapackWorkspaceKind::kReal)] =
        {reals.data(), sizeof(reals), kHost};
    result.regions[static_cast<std::size_t>(
        asc::LapackWorkspaceKind::kInteger)] = {integers.data(),
                                                sizeof(integers), kHost};
    result.regions[static_cast<std::size_t>(
        asc::LapackWorkspaceKind::kLayoutConversion)] = {
        packing.data(), sizeof(packing), kHost};
    return result;
  }
};

bool Completed(const asc::Status& status, const asc::LapackReport& report) {
  const bool complete = status.ok() && report.called_provider &&
                        report.native_info == 0 &&
                        report.outcome == asc::LapackOutcome::kSuccess;
  if (!complete) {
    std::fprintf(stderr, "Execution failed: status=%d called=%d outcome=%d\n",
                 static_cast<int>(status.code()),
                 static_cast<int>(report.called_provider),
                 static_cast<int>(report.outcome));
  }
  return complete;
}

bool Failure(const char* stage) {
  std::fprintf(stderr, "Installed advanced LU failed at %s\n", stage);
  return false;
}

template <typename T>
T OpAt(Matrix<T>& matrix, asc::DenseBlasTranspose transpose, std::size_t row,
       std::size_t column) {
  if (transpose == asc::DenseBlasTranspose::kNone) {
    return matrix.At(row, column);
  }
  const auto transposed_row = column;
  const auto transposed_column = row;
  const T value = matrix.At(transposed_row, transposed_column);
  if constexpr (asc::DenseBlasComplex<T>) {
    return transpose == asc::DenseBlasTranspose::kConjugateTranspose
               ? std::conj(value)
               : value;
  } else {
    return value;
  }
}

template <typename T>
T Expected(std::size_t row, std::size_t column) {
  return Value<T>(
      1 + 2 * static_cast<double>(row) + static_cast<double>(column),
      0.25 * static_cast<double>(row + column + 1));
}

template <typename T>
bool SolutionMatches(Matrix<T>& solution) {
  const auto tolerance =
      128 * std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon();
  for (std::size_t row = 0; row < 2; ++row) {
    for (std::size_t column = 0; column < 2; ++column) {
      const auto expected = Widen(Expected<T>(row, column));
      if (std::abs(Widen(solution.At(row, column)) - expected) >
          tolerance * std::abs(expected)) {
        std::fprintf(stderr, "Solution mismatch at (%zu,%zu): error=%Lg\n", row,
                     column,
                     std::abs(Widen(solution.At(row, column)) - expected));
        return false;
      }
    }
  }
  return true;
}

long double Abs1(Wide value) {
  return std::abs(value.real()) + std::abs(value.imag());
}

long double Ratio(long double numerator, long double denominator) {
  if (denominator == 0) {
    return numerator == 0 ? 0 : std::numeric_limits<long double>::infinity();
  }
  return numerator / denominator;
}

template <typename T>
bool RefinementColumnMatches(Matrix<T>& a, Matrix<T>& b, Matrix<T>& x,
                             asc::DenseBlasTranspose transpose,
                             std::size_t column, asc::DenseBlasRealType<T> ferr,
                             asc::DenseBlasRealType<T> berr) {
  using Real = asc::DenseBlasRealType<T>;
  long double norm_x = 0;
  long double norm_error = 0;
  long double backward = 0;
  for (std::size_t row = 0; row < 2; ++row) {
    Wide residual = Widen(b.At(row, column));
    Wide exact_rhs{};
    long double denominator = Abs1(residual);
    for (std::size_t inner = 0; inner < 2; ++inner) {
      const Wide coefficient = Widen(OpAt(a, transpose, row, inner));
      residual -= coefficient * Widen(x.At(inner, column));
      exact_rhs += coefficient * Widen(Expected<T>(inner, column));
      denominator += Abs1(coefficient) * Abs1(Widen(x.At(inner, column)));
    }
    // This dyadic fixture forms B exactly in every supported scalar type.
    if (exact_rhs != Widen(b.At(row, column))) {
      return Failure("independent exact RHS");
    }
    backward = std::max(backward, Ratio(Abs1(residual), denominator));
    norm_x = std::max(norm_x, Abs1(Widen(x.At(row, column))));
    norm_error = std::max(norm_error, Abs1(Widen(x.At(row, column)) -
                                           Widen(Expected<T>(row, column))));
  }
  // GERFS stops on rounded componentwise backward error, not a fixed forward
  // error. Eight epsilons bound the two-term complex residual's rounding;
  // the real transpose fixture has kappa_infinity = 8193. Its finite FERR
  // estimate must cover the independently observed normwise forward error.
  const long double residual_bound = 8 * std::numeric_limits<Real>::epsilon();
  const auto forward = Ratio(norm_error, norm_x);
  if (!std::isfinite(ferr) || !std::isfinite(berr) || ferr < 0 || berr < 0 ||
      backward > residual_bound || berr > residual_bound || forward > ferr) {
    std::fprintf(stderr,
                 "Refinement RHS %zu: forward=%Lg FERR=%Lg backward=%Lg "
                 "BERR=%Lg bound=%Lg\n",
                 column, forward, static_cast<long double>(ferr), backward,
                 static_cast<long double>(berr), residual_bound);
    return false;
  }
  return true;
}

template <typename T>
bool AccuracyMatches(Matrix<T>& a, Matrix<T>& b, Matrix<T>& x,
                     asc::DenseBlasTranspose transpose,
                     const std::array<asc::DenseBlasRealType<T>, 2>& ferr,
                     const std::array<asc::DenseBlasRealType<T>, 2>& berr,
                     bool scaled) {
  return RefinementColumnMatches(a, b, x, transpose, 0, ferr[0], berr[0]) &&
         RefinementColumnMatches(a, b, x, transpose, 1, ferr[1], berr[1]) &&
         (scaled || SolutionMatches(x));
}

template <typename T>
bool Condition(const asc::ReferenceLapackProvider& provider,
               Matrix<T>& original, Matrix<T>& factors) {
  using Real = asc::DenseBlasRealType<T>;
  const Wide a = Widen(original.At(0, 0));
  const Wide b = Widen(original.At(0, 1));
  const Wide c = Widen(original.At(1, 0));
  const Wide d = Widen(original.At(1, 1));
  const auto determinant = std::abs(a * d - b * c);
  Scratch<T> scratch;
  for (const auto norm :
       {asc::LapackConditionNorm::kOne, asc::LapackConditionNorm::kInfinity}) {
    const bool one = norm == asc::LapackConditionNorm::kOne;
    const auto anorm =
        one ? std::max(std::abs(a) + std::abs(c), std::abs(b) + std::abs(d))
            : std::max(std::abs(a) + std::abs(b), std::abs(c) + std::abs(d));
    const auto inverse_norm =
        (one ? std::max(std::abs(d) + std::abs(c), std::abs(b) + std::abs(a))
             : std::max(std::abs(d) + std::abs(b), std::abs(c) + std::abs(a))) /
        determinant;
    const auto exact = 1 / (anorm * inverse_norm);
    Real rcond = -1;
    const auto plan = asc::QueryGeconWorkspace(
        provider, norm, factors.ConstView(), static_cast<Real>(anorm), rcond);
    asc::LapackReport report;
    if (!plan.ok() ||
        !Completed(asc::Gecon(provider, norm, factors.ConstView(),
                              static_cast<Real>(anorm), rcond, *plan,
                              scratch.View(), report),
                   report) ||
        !std::isfinite(rcond) || rcond < exact / 4 || rcond > exact * 4) {
      return Failure("GECON");
    }
  }
  return true;
}

template <typename T>
void Initialize(Matrix<T>& a, Matrix<T>& b, asc::DenseBlasTranspose transpose,
                bool scaled) {
  const auto first_scale = Value<T>(scaled ? 0.015625 : 1);
  const auto second_scale = Value<T>(scaled ? 64 : 1);
  a.At(0, 0) = first_scale * Value<T>(1, 0.25);
  a.At(0, 1) = first_scale * Value<T>(0.5, -0.125);
  a.At(1, 0) = second_scale * Value<T>(0.5, 0.125);
  a.At(1, 1) = second_scale * Value<T>(1, -0.25);
  for (std::size_t row = 0; row < 2; ++row) {
    for (std::size_t column = 0; column < 2; ++column) {
      b.At(row, column) = OpAt(a, transpose, row, 0) * Expected<T>(0, column) +
                          OpAt(a, transpose, row, 1) * Expected<T>(1, column);
    }
  }
}

asc::RawLapackPivotView RawPivots(std::array<asc::index_t, 2>& pivots) {
  return Take(asc::RawLapackPivotView::Create(
      pivots.data(), 2, asc::LapackFactorFamily::kLuPartialPivot,
      {pivots.data(), sizeof(pivots), kHost}));
}

template <typename T>
void Perturb(Matrix<T>& solution) {
  for (auto& value : solution.data) {
    value *= asc::DenseBlasRealType<T>{0.99};
  }
}

template <typename T>
struct Diagnostics {
  using Real = asc::DenseBlasRealType<T>;
  std::array<Real, 2> forward{};
  std::array<Real, 2> backward{};
  std::array<Real, 2> rows{};
  std::array<Real, 2> columns{};
  asc::LapackSolveStatistics<Real> statistics;
};

template <typename T>
bool Exercise(const asc::ReferenceLapackProvider& provider,
              const std::array<asc::DenseBlasLayout, 4>& layouts,
              asc::DenseBlasTranspose transpose, bool scaled) {
  Matrix<T> a{{}, layouts[0]};
  Matrix<T> af{{}, layouts[1]};
  Matrix<T> b{{}, layouts[2]};
  Matrix<T> x{{}, layouts[3]};
  Initialize(a, b, transpose, scaled);
  auto original_a = a;
  auto original_b = b;
  std::array<asc::index_t, 2> pivots{};
  auto [ferr, berr, rows, columns, statistics] = Diagnostics<T>{};
  auto pivot_view = Vector(pivots);
  auto f = Vector(ferr);
  auto e = Vector(berr);
  Scratch<T> scratch;
  auto workspace = scratch.View();
  asc::LapackReport report;
  auto plan = asc::QueryGesvxWorkspace(provider, transpose, a.ConstView(),
                                       af.View(), pivot_view, b.ConstView(),
                                       x.View(), f, e, statistics);
  if (!plan.ok() ||
      !Completed(asc::Gesvx(provider, transpose, a.ConstView(), af.View(),
                            pivot_view, b.ConstView(), x.View(), f, e,
                            statistics, *plan, workspace, report),
                 report) ||
      !AccuracyMatches(original_a, original_b, x, transpose, ferr, berr,
                       scaled) ||
      a.data != original_a.data || b.data != original_b.data ||
      !Condition(provider, a, af)) {
    return Failure("GESVX FACT=N or condition estimate");
  }
  const auto original_af = af.data;
  const auto original_pivots = pivots;
  const auto raw = RawPivots(pivots);
  Perturb(x);
  plan = asc::QueryGerfsWorkspace(provider, transpose, a.ConstView(),
                                  af.ConstView(), raw, b.ConstView(), x.View(),
                                  f, e);
  if (!plan.ok() ||
      !Completed(
          asc::Gerfs(provider, transpose, a.ConstView(), af.ConstView(), raw,
                     b.ConstView(), x.View(), f, e, *plan, workspace, report),
          report) ||
      !AccuracyMatches(a, b, x, transpose, ferr, berr, scaled) ||
      a.data != original_a.data || af.data != original_af ||
      b.data != original_b.data || pivots != original_pivots) {
    return Failure("GERFS");
  }
  asc::LapackEquilibration equed = asc::LapackEquilibration::kNone;
  plan = asc::QueryGesvxEquilibratedWorkspace(
      provider, transpose, a.View(), af.View(), pivot_view, equed, Vector(rows),
      Vector(columns), b.View(), x.View(), f, e, statistics);
  if (!plan.ok() ||
      !Completed(asc::GesvxEquilibrated(
                     provider, transpose, a.View(), af.View(), pivot_view,
                     equed, Vector(rows), Vector(columns), b.View(), x.View(),
                     f, e, statistics, *plan, workspace, report),
                 report) ||
      !AccuracyMatches(original_a, original_b, x, transpose, ferr, berr,
                       scaled) ||
      (scaled && equed == asc::LapackEquilibration::kNone)) {
    return Failure("GESVX FACT=E");
  }
  b.data = original_b.data;
  const auto scaled_a = a.data;
  const auto scaled_af = af.data;
  plan = asc::QueryGesvxFactoredWorkspace(
      provider, transpose, a.ConstView(), af.ConstView(), raw, equed,
      ConstVector(rows), ConstVector(columns), b.View(), x.View(), f, e,
      statistics);
  return plan.ok() &&
         Completed(asc::GesvxFactored(
                       provider, transpose, a.ConstView(), af.ConstView(), raw,
                       equed, ConstVector(rows), ConstVector(columns), b.View(),
                       x.View(), f, e, statistics, *plan, workspace, report),
                   report) &&
         AccuracyMatches(original_a, original_b, x, transpose, ferr, berr,
                         scaled) &&
         a.data == scaled_a && af.data == scaled_af &&
         statistics.reciprocal_condition > 0 && std::isfinite(ferr[0]) &&
         std::isfinite(ferr[1]) && berr[0] >= 0 && berr[1] >= 0;
}

template <typename T>
bool AllModes(const asc::ReferenceLapackProvider& provider) {
  for (unsigned mask = 0; mask < 16; ++mask) {
    std::array<asc::DenseBlasLayout, 4> layouts{};
    for (unsigned operand = 0; operand < layouts.size(); ++operand) {
      layouts[operand] = (mask & (1U << operand)) != 0 ? kRow : kColumn;
    }
    for (const auto transpose :
         {asc::DenseBlasTranspose::kNone, asc::DenseBlasTranspose::kTranspose,
          asc::DenseBlasTranspose::kConjugateTranspose}) {
      if (!Exercise<T>(provider, layouts, transpose, false) ||
          !Exercise<T>(provider, layouts, transpose, true)) {
        std::fprintf(stderr,
                     "scalar_bytes=%zu complex=%d layouts=%u trans=%d\n",
                     sizeof(T), static_cast<int>(asc::DenseBlasComplex<T>),
                     mask, static_cast<int>(transpose));
        return false;
      }
    }
  }
  return true;
}
}  // namespace

int main() {
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  return AllModes<float>(provider) && AllModes<double>(provider) &&
                 AllModes<std::complex<float>>(provider) &&
                 AllModes<std::complex<double>>(provider)
             ? 0
             : 1;
}
