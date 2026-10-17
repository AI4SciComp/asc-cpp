#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <span>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_triangular.h"
#include "factorization_support.h"
#include "normal_return_guard.h"

namespace {
using installed_internal::Take;
using installed_internal::Value;
using installed_internal::Wide;
using installed_internal::Widen;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;
constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;
constexpr auto kLower = asc::DenseBlasTriangle::kLower;
constexpr auto kUnit = asc::DenseBlasDiagonal::kUnit;
constexpr auto kNonUnit = asc::DenseBlasDiagonal::kNonUnit;
constexpr auto kNone = asc::DenseBlasTranspose::kNone;
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kLayout =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);

class Checks {
 public:
  void Expect(bool condition, const char* message) {
    ++checks_;
    if (!condition) {
      if (failures_ < 30) {
        std::fprintf(stderr, "Triangular public consumer: %s\n", message);
      }
      ++failures_;
    }
  }
  void Workflow() { ++workflows_; }
  [[nodiscard]] int Finish() const {
    std::printf(
        "Triangular public consumer: %zu workflows, %zu checks, "
        "%zu failures\n",
        workflows_, checks_, failures_);
    return failures_ == 0 ? 0 : 1;
  }

 private:
  std::size_t checks_ = 0;
  std::size_t workflows_ = 0;
  std::size_t failures_ = 0;
};

template <typename T, std::size_t Size>
bool SameBytes(const std::array<T, Size>& left,
               const std::array<T, Size>& right) {
  const auto actual = std::as_bytes(std::span(left));
  const auto expected = std::as_bytes(std::span(right));
  return std::equal(actual.begin(), actual.end(), expected.begin(),
                    expected.end());
}

struct Profile {
  asc::extent_t n;
  asc::DenseBlasLayout layout;
  asc::DenseBlasTriangle triangle;
  asc::DenseBlasDiagonal diagonal;
  int exponent;
};

template <typename T>
struct Matrix {
  std::array<T, 5000> values;
  asc::extent_t rows;
  asc::extent_t columns;
  asc::DenseBlasLayout layout;
  asc::extent_t leading;
  Matrix(asc::extent_t m, asc::extent_t n, asc::DenseBlasLayout selected)
      : rows(m),
        columns(n),
        layout(selected),
        leading((layout == kRow ? columns : rows) + 3) {
    values.fill(Value<T>(std::numeric_limits<double>::quiet_NaN()));
  }
  [[nodiscard]] std::size_t Offset(asc::extent_t i, asc::extent_t j) const {
    return static_cast<std::size_t>(
        1 + (layout == kRow ? i * leading + j : j * leading + i));
  }
  T& At(asc::extent_t i, asc::extent_t j) { return values[Offset(i, j)]; }
  [[nodiscard]] T At(asc::extent_t i, asc::extent_t j) const {
    return values[Offset(i, j)];
  }
  auto View() {
    return Take(asc::DenseBlasMatrixView<T>::Create(
        values.data() + 1, rows, columns, layout, leading,
        {values.data(), sizeof(values), kHost}));
  }
  auto ConstView() { return asc::DenseBlasMatrixView<const T>(View()); }
};

bool Selected(asc::extent_t i, asc::extent_t j, const Profile& profile) {
  return (profile.triangle == kUpper ? i <= j : i >= j) &&
         (i != j || profile.diagonal == kNonUnit);
}

template <typename T>
T Coefficient(asc::extent_t i, asc::extent_t j, const Profile& profile) {
  const double scale = std::ldexp(1.0, profile.exponent);
  if (i == j) {
    return Value<T>(profile.diagonal == kUnit ? 1 : 2 * scale);
  }
  if ((profile.triangle == kUpper && j == i + 1) ||
      (profile.triangle == kLower && i == j + 1)) {
    return Value<T>(0.25 * scale, 0.125 * scale);
  }
  return T{};
}

template <typename T>
Matrix<T> Triangular(const Profile& profile) {
  Matrix<T> a(profile.n, profile.n, profile.layout);
  for (asc::extent_t j = 0; j < profile.n; ++j) {
    for (asc::extent_t i = 0; i < profile.n; ++i) {
      if (Selected(i, j, profile)) {
        a.At(i, j) = Coefficient<T>(i, j, profile);
      }
    }
  }
  return a;
}

template <typename T>
void Unchanged(Checks& checks, const Matrix<T>& actual, const Matrix<T>& before,
               const Profile& profile, bool whole) {
  auto expected = before.values;
  if (!whole) {
    for (asc::extent_t j = 0; j < actual.columns; ++j) {
      for (asc::extent_t i = 0; i < actual.rows; ++i) {
        if (Selected(i, j, profile)) {
          expected[actual.Offset(i, j)] = actual.At(i, j);
        }
      }
    }
  }
  checks.Expect(SameBytes(actual.values, expected),
                "bitwise preserved ignored triangle/unit diagonal/padding");
}

template <typename T>
struct Workspace {
  std::array<T, 5002> values;
  asc::LapackWorkspace workspace;
  explicit Workspace(const asc::LapackWorkspacePlan& plan) {
    values.fill(Value<T>(-4096, 1024));
    const auto count = plan.regions[kLayout].minimum_entries;
    if (count < 0 || count > 5000) {
      std::fprintf(stderr, "Invalid query workspace capacity\n");
      std::abort();
    }
    if (count != 0) {
      workspace.regions[kLayout] = {values.data() + 1,
                                    static_cast<std::size_t>(count) * sizeof(T),
                                    kHost};
    }
  }
  void Guards(Checks& checks, const asc::LapackWorkspacePlan& plan) const {
    const auto count =
        static_cast<std::size_t>(plan.regions[kLayout].minimum_entries);
    checks.Expect(values.front() == Value<T>(-4096, 1024),
                  "workspace front guard");
    checks.Expect(std::all_of(values.begin() + 1 + count, values.end(),
                              [](T x) { return x == Value<T>(-4096, 1024); }),
                  "workspace tail guards");
  }
};

void Report(Checks& checks, const asc::Status& status,
            const asc::LapackReport& report, asc::extent_t n) {
  checks.Expect(status.ok(), "successful status");
  checks.Expect(report.called_provider == (n != 0),
                "real call versus empty local return");
  checks.Expect(
      n == 0 ? !report.native_info.has_value() : report.native_info == 0,
      "raw INFO absent only for local return");
  checks.Expect(
      report.outcome == asc::LapackOutcome::kSuccess &&
          report.output_validity == asc::LapackOutputValidity::kComplete,
      "successful report validity");
  checks.Expect(!report.diagnostic_index && !report.native_argument &&
                    !report.factor_family,
                "no fabricated diagnostics/factor family");
}

template <typename T>
void CheckInverse(Checks& checks, const Matrix<T>& a, const Profile& profile) {
  const Wide diagonal = Widen(Coefficient<T>(0, 0, profile));
  const Wide edge =
      Widen(Coefficient<T>(profile.triangle == kUpper ? 0 : 1,
                           profile.triangle == kUpper ? 1 : 0, profile));
  const Wide ratio = -edge / diagonal;
  // A=d*I+e*S, S nilpotent: A^-1=sum((-e/d)^k*S^k)/d.
  // These well-conditioned profiles have |e/d|<0.3. Accumulated absolute
  // row error is bounded relative to ||A^-1||_infinity, including tiny entries.
  const long double norm_bound =
      1 / (std::abs(diagonal) * (1 - std::abs(ratio)));
  long double max_error = 0;
  for (asc::extent_t i = 0; i < profile.n; ++i) {
    long double row_error = 0;
    for (asc::extent_t j = 0; j < profile.n; ++j) {
      if (!Selected(i, j, profile)) {
        continue;
      }
      const auto distance = i < j ? j - i : i - j;
      Wide expected = 1.0L / diagonal;
      for (asc::extent_t k = 0; k < distance; ++k) {
        expected *= ratio;
      }
      const auto actual = Widen(a.At(i, j));
      checks.Expect(std::isfinite(std::abs(actual)), "finite inverse entry");
      row_error += std::abs(actual - expected);
    }
    max_error = std::max(max_error, row_error);
  }
  const auto epsilon =
      std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon();
  checks.Expect(
      std::isfinite(max_error) &&
          max_error <=
              16 * epsilon * std::max<asc::extent_t>(1, profile.n) * norm_bound,
      "analytic inverse infinity-norm forward error");
}

template <typename T>
void Inverse(Checks& checks, const asc::ReferenceLapackProvider& provider,
             const Profile& profile, bool blocked) {
  auto a = Triangular<T>(profile);
  const auto before = a;
  const auto plan =
      Take(blocked ? asc::QueryTrtriWorkspace(provider, profile.triangle,
                                              profile.diagonal, a.View())
                   : asc::QueryTrti2Workspace(provider, profile.triangle,
                                              profile.diagonal, a.View()));
  Unchanged(checks, a, before, profile, true);
  checks.Expect(plan.regions[kLayout].minimum_entries ==
                    (profile.layout == kRow ? profile.n * profile.n : 0),
                "exact inverse packing formula");
  Workspace<T> workspace(plan);
  asc::LapackReport report;
  const auto status =
      blocked ? asc::Trtri(provider, profile.triangle, profile.diagonal,
                           a.View(), plan, workspace.workspace, report)
              : asc::Trti2(provider, profile.triangle, profile.diagonal,
                           a.View(), plan, workspace.workspace, report);
  Report(checks, status, report, profile.n);
  Unchanged(checks, a, before, profile, false);
  CheckInverse(checks, a, profile);
  workspace.Guards(checks, plan);
  checks.Workflow();
}

template <typename T>
T Solution(asc::extent_t i, asc::extent_t j) {
  return Value<T>(static_cast<double>(1 + i % 3 - j),
                  static_cast<double>(j - i % 2) * 0.5);
}

template <typename T>
void FillRhs(Matrix<T>& b, const Profile& profile,
             asc::DenseBlasTranspose operation) {
  for (asc::extent_t j = 0; j < b.columns; ++j) {
    for (asc::extent_t i = 0; i < b.rows; ++i) {
      T sum{};
      for (asc::extent_t k = 0; k < profile.n; ++k) {
        T a = operation == kNone ? Coefficient<T>(i, k, profile)
                                 : Coefficient<T>(k, i, profile);
        if (operation == asc::DenseBlasTranspose::kConjugateTranspose) {
          a = installed_internal::Conjugate(a);
        }
        sum += a * Solution<T>(k, j);
      }
      b.At(i, j) = sum;
    }
  }
}

template <typename T>
void CheckSolution(Checks& checks, const Matrix<T>& b, const Matrix<T>& before,
                   const Profile& profile) {
  auto expected = before.values;
  long double error = 0;
  long double norm = 0;
  for (asc::extent_t i = 0; i < b.rows; ++i) {
    long double row_error = 0;
    long double row_norm = 0;
    for (asc::extent_t j = 0; j < b.columns; ++j) {
      const auto actual = Widen(b.At(i, j));
      const auto solution = Widen(Solution<T>(i, j));
      checks.Expect(std::isfinite(std::abs(actual)), "finite solution entry");
      row_error += std::abs(actual - solution);
      row_norm += std::abs(solution);
      expected[b.Offset(i, j)] = b.At(i, j);
    }
    error = std::max(error, row_error);
    norm = std::max(norm, row_norm);
  }
  const auto epsilon =
      std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon();
  checks.Expect(
      std::isfinite(error) &&
          error <= 16 * epsilon * std::max<asc::extent_t>(1, profile.n) * norm,
      "known-solution infinity-norm forward error");
  checks.Expect(SameBytes(b.values, expected),
                "whole RHS padding remains bitwise unchanged");
}

template <typename T>
void Solve(Checks& checks, const asc::ReferenceLapackProvider& provider,
           const Profile& profile, asc::DenseBlasLayout layout,
           asc::DenseBlasTranspose operation, asc::extent_t nrhs) {
  auto a = Triangular<T>(profile);
  const auto original_a = a;
  Matrix<T> b(profile.n, nrhs, layout);
  FillRhs(b, profile, operation);
  const auto original_b = b;
  const auto plan = Take(asc::QueryTrtrsWorkspace(provider, profile.triangle,
                                                  profile.diagonal, operation,
                                                  a.ConstView(), b.View()));
  Unchanged(checks, a, original_a, profile, true);
  checks.Expect(SameBytes(b.values, original_b.values),
                "query preserves RHS bytes");
  const auto count =
      nrhs == 0 ? 0
                : (profile.layout == kRow ? profile.n * profile.n : 0) +
                      (layout == kRow ? profile.n * nrhs : 0);
  checks.Expect(plan.regions[kLayout].minimum_entries == count,
                "exact independent-layout solve formula");
  Workspace<T> workspace(plan);
  asc::LapackReport report;
  const auto status =
      asc::Trtrs(provider, profile.triangle, profile.diagonal, operation,
                 a.ConstView(), b.View(), plan, workspace.workspace, report);
  Report(checks, status, report, profile.n);
  Unchanged(checks, a, original_a, profile, true);
  CheckSolution(checks, b, original_b, profile);
  workspace.Guards(checks, plan);
  checks.Workflow();
}

template <typename T>
void Scalar(Checks& checks, const asc::ReferenceLapackProvider& provider) {
  for (const asc::extent_t n : {0, 1, 2, 5, 64, 65, 67}) {
    for (auto layout : {kColumn, kRow}) {
      for (auto triangle : {kUpper, kLower}) {
        for (auto diagonal : {kUnit, kNonUnit}) {
          for (const int exponent : {-20, 0, 20}) {
            // Unit diagonal is fixed at one, so scale its offdiagonal only
            // in the exponent-zero fixture to keep the stated conditioning.
            if (diagonal == kUnit && exponent != 0) {
              continue;
            }
            const Profile profile{n, layout, triangle, diagonal, exponent};
            Inverse<T>(checks, provider, profile, false);
            Inverse<T>(checks, provider, profile, true);
            for (auto rhs_layout : {kColumn, kRow}) {
              for (auto operation :
                   {kNone, asc::DenseBlasTranspose::kTranspose,
                    asc::DenseBlasTranspose::kConjugateTranspose}) {
                Solve<T>(checks, provider, profile, rhs_layout, operation, 3);
                Solve<T>(checks, provider, profile, rhs_layout, operation, 0);
              }
            }
          }
        }
      }
    }
  }
}
}  // namespace

int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  Checks checks;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  Scalar<float>(checks, provider);
  Scalar<double>(checks, provider);
  Scalar<std::complex<float>>(checks, provider);
  Scalar<std::complex<double>>(checks, provider);
  return checks.Finish();
}
