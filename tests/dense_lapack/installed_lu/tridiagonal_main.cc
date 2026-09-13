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
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu_condition.h"
#include "asc/dense/providers/lapack_tridiagonal.h"
#include "asc/dense/providers/lapack_tridiagonal_condition.h"
#include "asc/dense/providers/lapack_tridiagonal_driver.h"
#include "asc/dense/providers/lapack_tridiagonal_refinement.h"
#include "factorization_support.h"
#include "normal_return_guard.h"

namespace {
using installed_internal::Conjugate;
using installed_internal::kColumn;
using installed_internal::kHost;
using installed_internal::kRow;
using installed_internal::Matrix;
using installed_internal::Near;
using installed_internal::Scratch;
using installed_internal::Succeeded;
using installed_internal::Take;
using installed_internal::Value;
using installed_internal::Wide;
using installed_internal::Widen;
constexpr auto kScalar =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kScalar);
constexpr auto kPacking =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);
constexpr auto kInteger =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kInteger);
constexpr auto kReal =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kReal);

template <typename T>
struct Workspace {
  Scratch<T> storage;
  std::array<asc::DenseBlasRealType<T>, 16> real{};
  alignas(std::max_align_t) std::array<std::byte, 128> integer{};

  explicit Workspace(const asc::LapackWorkspacePlan& plan) {
    storage.scalar.fill(Value<T>(-101));
    storage.packing.fill(Value<T>(-103));
    real.fill(-107);
    integer.fill(std::byte{0x6b});
    storage.workspace = {};
    for (const auto kind : {kScalar, kPacking, kInteger, kReal}) {
      const auto& region = plan.regions[kind];
      const auto count = static_cast<std::size_t>(region.preferred_entries);
      const auto bytes = count * region.entry_bytes;
      if (bytes == 0) {
        continue;
      }
      if (kind == kScalar && count < storage.scalar.size() - 1) {
        storage.workspace.regions[kind] = {storage.scalar.data() + 1, bytes,
                                           kHost};
      } else if (kind == kPacking && count < storage.packing.size() - 1) {
        storage.workspace.regions[kind] = {storage.packing.data() + 1, bytes,
                                           kHost};
      } else if (kind == kInteger && bytes < integer.size() - 16) {
        storage.workspace.regions[kind] = {integer.data() + 16, bytes, kHost};
      } else if (kind == kReal && count < real.size() - 1) {
        storage.workspace.regions[kind] = {real.data() + 1, bytes, kHost};
      } else {
        std::abort();
      }
    }
  }

  [[nodiscard]] bool Guards() const {
    const auto& regions = storage.workspace.regions;
    for (std::size_t i = 0; i < storage.scalar.size(); ++i) {
      if ((i == 0 || i > regions[kScalar].size() / sizeof(T)) &&
          storage.scalar[i] != Value<T>(-101)) {
        return false;
      }
    }
    for (std::size_t i = 0; i < storage.packing.size(); ++i) {
      if ((i == 0 || i > regions[kPacking].size() / sizeof(T)) &&
          storage.packing[i] != Value<T>(-103)) {
        return false;
      }
    }
    for (std::size_t i = 0; i < real.size(); ++i) {
      if ((i == 0 || i > regions[kReal].size() / sizeof(real[0])) &&
          real[i] != -107) {
        return false;
      }
    }
    for (std::size_t i = 0; i < integer.size(); ++i) {
      if ((i < 16 || i >= 16 + regions[kInteger].size()) &&
          integer[i] != std::byte{0x6b}) {
        return false;
      }
    }
    return true;
  }
};

template <typename T, std::size_t N>
auto Slice(std::array<T, N>& values) {
  return Take(asc::DenseBlasVectorView<T>::Create(
      values.data() + 1, N - 2, 1, {values.data(), sizeof(values), kHost}));
}

template <typename T, std::size_t N>
auto Slice(const std::array<T, N>& values) {
  return Take(asc::DenseBlasVectorView<const T>::Create(
      values.data() + 1, N - 2, 1, {values.data(), sizeof(values), kHost}));
}

template <typename T>
struct Tridiagonal {
  std::array<T, 4> lower{};
  std::array<T, 5> diagonal{};
  std::array<T, 4> upper{};
  std::array<T, 3> second{};

  Tridiagonal() {
    lower.fill(T{-137});
    diagonal.fill(T{-137});
    upper.fill(T{-137});
    second.fill(T{-137});
  }
  auto View() {
    return Take(asc::LapackTridiagonalView<T>::Create(
        Slice(lower), Slice(diagonal), Slice(upper)));
  }
  [[nodiscard]] auto ConstView() const {
    return Take(asc::LapackTridiagonalView<const T>::Create(
        Slice(lower), Slice(diagonal), Slice(upper)));
  }
  auto Factors() {
    return Take(
        asc::LapackTridiagonalLuStorage<T>::Create(View(), Slice(second)));
  }
  [[nodiscard]] auto ConstFactors() const {
    return Take(asc::LapackTridiagonalLuStorage<const T>::Create(
        ConstView(), Slice(second)));
  }
  [[nodiscard]] bool Guards() const {
    return lower.front() == T{-137} && lower.back() == T{-137} &&
           diagonal.front() == T{-137} && diagonal.back() == T{-137} &&
           upper.front() == T{-137} && upper.back() == T{-137} &&
           second.front() == T{-137} && second.back() == T{-137};
  }
  bool operator==(const Tridiagonal&) const = default;
};

int successful_calls = 0;

bool Completed(const asc::Status& status, const asc::LapackReport& report) {
  if (!Succeeded(status, report) || report.factor_family.has_value()) {
    std::fprintf(stderr, "GT call failed: status=%d INFO=%lld\n",
                 static_cast<int>(status.code()),
                 static_cast<long long>(report.native_info.value_or(-999)));
    return false;
  }
  ++successful_calls;
  return true;
}

template <typename T>
struct Problem {
  using Real = asc::DenseBlasRealType<T>;
  asc::DenseBlasTranspose operation;
  double scale;
  Tridiagonal<T> original;
  Tridiagonal<T> factors;
  Matrix<T, 3, 2> b;
  Matrix<T, 3, 2> x;
  std::array<asc::index_t, 5> pivots{-139, -139, -139, -139, -139};
  std::array<Real, 4> ferr{-149, -149, -149, -149};
  std::array<Real, 4> berr{-151, -151, -151, -151};
  Real rcond = -157;

  static T Entry(std::size_t i, std::size_t j, double multiplier) {
    // det(A)=-66; adj(A)=[-2,-16,12;-32,8,-6;28,-7,-3].
    // Independent row/column unit phases preserve both modulus norms.
    constexpr double kA[3][3] = {{1, 2, 0}, {4, 5, 6}, {0, 7, 8}};
    if constexpr (asc::DenseBlasComplex<T>) {
      const std::array<T, 3> row{T{1}, T{0, 1}, T{-1}};
      const std::array<T, 3> column{T{0, 1}, T{1}, T{0, -1}};
      return Value<T>(multiplier * kA[i][j]) * row[i] * column[j];
    } else {
      return Value<T>(multiplier * kA[i][j]);
    }
  }
  static T Expected(std::size_t i, std::size_t j) {
    return Value<T>(static_cast<double>(1 + i + j),
                    static_cast<double>(1 + i + 2 * j) / 4);
  }
  static T Operand(std::size_t i, std::size_t j, double multiplier,
                   asc::DenseBlasTranspose op) {
    if (op == asc::DenseBlasTranspose::kNone) {
      return Entry(i, j, multiplier);
    }
    const T value = Entry(j, i, multiplier);
    return op == asc::DenseBlasTranspose::kConjugateTranspose ? Conjugate(value)
                                                              : value;
  }
  void FillRhs(Matrix<T, 3, 2>& rhs, asc::DenseBlasTranspose op) const {
    rhs.data.fill(T{-163});
    for (std::size_t i = 0; i < 3; ++i) {
      for (std::size_t j = 0; j < 2; ++j) {
        rhs.At(i, j) = T{};
        for (std::size_t k = 0; k < 3; ++k) {
          rhs.At(i, j) += Operand(i, k, scale, op) * Expected(k, j);
        }
      }
    }
  }
  Problem(asc::DenseBlasTranspose op, double multiplier,
          asc::DenseBlasLayout b_layout, asc::DenseBlasLayout x_layout)
      : operation(op), scale(multiplier), b{{}, b_layout}, x{{}, x_layout} {
    for (std::size_t i = 0; i < 3; ++i) {
      original.diagonal[i + 1] = Entry(i, i, scale);
      if (i < 2) {
        original.lower[i + 1] = Entry(i + 1, i, scale);
        original.upper[i + 1] = Entry(i, i + 1, scale);
      }
    }
    factors = original;
    FillRhs(b, operation);
    x.data.fill(T{-167});
  }
  [[nodiscard]] auto RawPivots() const {
    return Take(asc::ReferenceTridiagonalPivotView::Create(Slice(pivots)));
  }
  [[nodiscard]] bool Solution(const Matrix<T, 3, 2>& actual,
                              asc::DenseBlasTranspose op) const {
    Matrix<T, 3, 2> rhs{{}, kColumn};
    FillRhs(rhs, op);
    for (std::size_t i = 0; i < 3; ++i) {
      for (std::size_t j = 0; j < 2; ++j) {
        if (!Near<T>(Widen(actual.At(i, j)), Widen(Expected(i, j)), 32)) {
          return false;
        }
        Wide residual = -Widen(rhs.At(i, j)) / static_cast<long double>(scale);
        for (std::size_t k = 0; k < 3; ++k) {
          residual += Widen(Operand(i, k, scale, op)) /
                      static_cast<long double>(scale) * Widen(actual.At(k, j));
        }
        if (!Near<T>(residual, Wide{}, 64)) {
          return false;
        }
      }
    }
    return true;
  }
  [[nodiscard]] bool Reconstruct() const {
    if (pivots[1] != 2 || pivots[3] != 3) {
      return false;  // This fixture requires an actual adjacent interchange.
    }
    std::array<std::array<Wide, 3>, 3> restored{};
    for (std::size_t i = 0; i < 3; ++i) {
      restored[i][i] = Widen(factors.diagonal[i + 1]);
      if (i < 2) {
        restored[i][i + 1] = Widen(factors.upper[i + 1]);
      }
    }
    restored[0][2] = Widen(factors.second[1]);
    for (std::size_t end = 2; end > 0; --end) {
      const auto i = end - 1;
      for (std::size_t j = 0; j < 3; ++j) {
        restored[i + 1][j] += Widen(factors.lower[i + 1]) * restored[i][j];
      }
      if (pivots[i + 1] == static_cast<asc::index_t>(i + 2)) {
        std::swap(restored[i], restored[i + 1]);
      } else if (pivots[i + 1] != static_cast<asc::index_t>(i + 1)) {
        return false;
      }
    }
    for (std::size_t i = 0; i < 3; ++i) {
      for (std::size_t j = 0; j < 3; ++j) {
        if (!Near<T>(restored[i][j] / static_cast<long double>(scale),
                     Widen(Entry(i, j, 1)), 32)) {
          return false;
        }
      }
    }
    return true;
  }
  [[nodiscard]] bool Diagnostics() const {
    for (std::size_t j = 1; j < 3; ++j) {
      if (!std::isfinite(ferr[j]) || ferr[j] < 0 || !std::isfinite(berr[j]) ||
          berr[j] < 0 || berr[j] > 32 * std::numeric_limits<Real>::epsilon()) {
        return false;
      }
    }
    return true;
  }
  [[nodiscard]] bool Guards(const Problem& before) const {
    return original == before.original && factors.Guards() &&
           b.data == before.b.data && x.PaddingEquals(before.x.data) &&
           pivots.front() == -139 && pivots.back() == -139 &&
           ferr.front() == -149 && ferr.back() == -149 &&
           berr.front() == -151 && berr.back() == -151;
  }
  [[nodiscard]] bool SameOutputs(const Problem& before) const {
    return original == before.original && factors == before.factors &&
           b.data == before.b.data && x.data == before.x.data &&
           pivots == before.pivots && ferr == before.ferr &&
           berr == before.berr && rcond == before.rcond;
  }
};

template <typename T>
bool FactorSolve(const asc::ReferenceLapackProvider& provider, Problem<T>& p) {
  asc::LapackReport report;
  const auto factor_plan = Take(
      asc::QueryGttrfWorkspace(provider, p.factors.Factors(), Slice(p.pivots)));
  Workspace<T> factor_work(factor_plan);
  if (!Completed(asc::Gttrf(provider, p.factors.Factors(), Slice(p.pivots),
                            factor_plan, factor_work.storage.workspace, report),
                 report) ||
      !factor_work.Guards() || !p.Reconstruct()) {
    return false;
  }
  const auto factor = Take(asc::ReferenceTridiagonalLuFactorView<T>::Create(
      provider, p.factors.ConstFactors(), p.RawPivots(), report));
  const auto old = p;
  auto rhs = p.b;
  const auto plan =
      Take(asc::QueryGttrsWorkspace(provider, p.operation, factor, rhs.View()));
  Workspace<T> work(plan);
  return Completed(asc::Gttrs(provider, p.operation, factor, rhs.View(), plan,
                              work.storage.workspace, report),
                   report) &&
         work.Guards() && p.Solution(rhs, p.operation) &&
         rhs.PaddingEquals(p.b.data) && p.SameOutputs(old);
}

template <typename T>
bool Simple(const asc::ReferenceLapackProvider& provider, const Problem<T>& p) {
  auto matrix = p.original;
  Matrix<T, 3, 2> rhs{{}, p.x.layout};
  p.FillRhs(rhs, asc::DenseBlasTranspose::kNone);
  const auto before = rhs.data;
  const auto plan =
      Take(asc::QueryGtsvWorkspace(provider, matrix.View(), rhs.View()));
  Workspace<T> work(plan);
  asc::LapackReport report;
  return Completed(asc::Gtsv(provider, matrix.View(), rhs.View(), plan,
                             work.storage.workspace, report),
                   report) &&
         work.Guards() && matrix.Guards() && rhs.PaddingEquals(before) &&
         p.Solution(rhs, asc::DenseBlasTranspose::kNone);
}

template <typename T>
bool Condition(const asc::ReferenceLapackProvider& provider, Problem<T>& p) {
  using Real = asc::DenseBlasRealType<T>;
  for (const auto norm :
       {asc::LapackConditionNorm::kOne, asc::LapackConditionNorm::kInfinity}) {
    const bool one = norm == asc::LapackConditionNorm::kOne;
    const Real anorm = static_cast<Real>((one ? 14 : 15) * p.scale);
    const auto before = p;
    const auto plan =
        Take(asc::QueryGtconWorkspace(provider, norm, p.factors.ConstFactors(),
                                      p.RawPivots(), anorm, p.rcond));
    Workspace<T> work(plan);
    asc::LapackReport report;
    if (!Completed(
            asc::Gtcon(provider, norm, p.factors.ConstFactors(), p.RawPivots(),
                       anorm, p.rcond, plan, work.storage.workspace, report),
            report) ||
        !work.Guards() || p.factors != before.factors ||
        p.pivots != before.pivots) {
      return false;
    }
    // Analytic inverse above gives ||A^-1||_1=31/33 and infinity=23/33.
    // This is an estimator-quality check, not a certificate of exact RCOND.
    const long double truth = one ? 33.L / 434 : 33.L / 345;
    if (!std::isfinite(p.rcond) || p.rcond < truth / 4 || p.rcond > truth * 4) {
      return false;
    }
  }
  return true;
}

template <typename T>
bool Refine(const asc::ReferenceLapackProvider& provider, Problem<T>& p) {
  for (std::size_t i = 0; i < 3; ++i) {
    for (std::size_t j = 0; j < 2; ++j) {
      p.x.At(i, j) = Problem<T>::Expected(i, j) + Value<T>(0.25, 0.125);
    }
  }
  const auto before = p;
  const auto plan = Take(asc::QueryGtrfsWorkspace(
      provider, p.operation, p.original.ConstView(), p.factors.ConstFactors(),
      p.RawPivots(), p.b.ConstView(), p.x.View(), Slice(p.ferr),
      Slice(p.berr)));
  Workspace<T> work(plan);
  asc::LapackReport report;
  return Completed(
             asc::Gtrfs(provider, p.operation, p.original.ConstView(),
                        p.factors.ConstFactors(), p.RawPivots(),
                        p.b.ConstView(), p.x.View(), Slice(p.ferr),
                        Slice(p.berr), plan, work.storage.workspace, report),
             report) &&
         work.Guards() && p.factors == before.factors &&
         p.pivots == before.pivots && p.Solution(p.x, p.operation) &&
         p.Diagnostics();
}

template <typename T>
bool Expert(const asc::ReferenceLapackProvider& provider, Problem<T>& p,
            bool factored, bool reject = false) {
  const auto before = p;
  auto plan = Take(
      factored ? asc::QueryGtsvxFactoredWorkspace(
                     provider, p.operation, p.original.ConstView(),
                     p.factors.ConstFactors(), p.RawPivots(), p.b.ConstView(),
                     p.x.View(), p.rcond, Slice(p.ferr), Slice(p.berr))
               : asc::QueryGtsvxWorkspace(
                     provider, p.operation, p.original.ConstView(),
                     p.factors.Factors(), Slice(p.pivots), p.b.ConstView(),
                     p.x.View(), p.rcond, Slice(p.ferr), Slice(p.berr)));
  Workspace<T> work(plan);
  const auto old_work = work;
  if (reject) {
    ++plan.regions[kInteger].preferred_entries;
  }
  asc::LapackReport report;
  const auto status =
      factored
          ? asc::GtsvxFactored(provider, p.operation, p.original.ConstView(),
                               p.factors.ConstFactors(), p.RawPivots(),
                               p.b.ConstView(), p.x.View(), p.rcond,
                               Slice(p.ferr), Slice(p.berr), plan,
                               work.storage.workspace, report)
          : asc::Gtsvx(provider, p.operation, p.original.ConstView(),
                       p.factors.Factors(), Slice(p.pivots), p.b.ConstView(),
                       p.x.View(), p.rcond, Slice(p.ferr), Slice(p.berr), plan,
                       work.storage.workspace, report);
  if (reject) {
    return status.code() == asc::ErrorCode::kInvalidState &&
           !report.called_provider && !report.native_info.has_value() &&
           report.outcome == asc::LapackOutcome::kNotRun &&
           report.output_validity == asc::LapackOutputValidity::kUnchanged &&
           p.SameOutputs(before) &&
           work.storage.scalar == old_work.storage.scalar &&
           work.storage.packing == old_work.storage.packing &&
           work.integer == old_work.integer && work.real == old_work.real;
  }
  return Completed(status, report) && work.Guards() && p.Reconstruct() &&
         p.Solution(p.x, p.operation) && p.Diagnostics() &&
         (!factored ||
          (p.factors == before.factors && p.pivots == before.pivots));
}

template <typename T>
bool All(const asc::ReferenceLapackProvider& provider, int& workflows) {
  for (const auto op :
       {asc::DenseBlasTranspose::kNone, asc::DenseBlasTranspose::kTranspose,
        asc::DenseBlasTranspose::kConjugateTranspose}) {
    for (const double scale : {0x1p-20, 1.0, 0x1p20}) {
      for (const auto b_layout : {kColumn, kRow}) {
        for (const auto x_layout : {kColumn, kRow}) {
          Problem<T> p(op, scale, b_layout, x_layout);
          const auto before = p;
          if (!FactorSolve(provider, p) || !Simple(provider, p) ||
              !Condition(provider, p) || !Refine(provider, p) ||
              !Expert(provider, p, false) || !Expert(provider, p, true) ||
              !Expert(provider, p, true, true) || !p.Guards(before)) {
            std::fprintf(
                stderr, "GT workflow failed: op=%d scale=%g layouts=%d/%d\n",
                static_cast<int>(op), scale, static_cast<int>(b_layout),
                static_cast<int>(x_layout));
            return false;
          }
          ++workflows;
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
  int workflows = 0;
  if (!All<float>(provider, workflows) || !All<double>(provider, workflows) ||
      !All<std::complex<float>>(provider, workflows) ||
      !All<std::complex<double>>(provider, workflows) || workflows != 144 ||
      successful_calls != 1152) {
    return 1;
  }
  std::printf(
      "GT_PUBLIC workflows=%d routes=24 calls=%d rejections=%d "
      "math_gates_remain_open=1\n",
      workflows, successful_calls, workflows);
  return 0;
}
