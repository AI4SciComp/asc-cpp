#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <limits>
#include <string_view>
#include <type_traits>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_driver.h"
#include "asc/dense/providers/lapack_cholesky_packed_robust.h"
#include "factorization_support.h"
#include "normal_return_guard.h"

namespace {
// The independent extreme oracle needs exponent headroom for inverse products.
// This is a test-platform requirement, not a claim that long double is always
// wider. The admitted GNU11 x86_64 profile satisfies it. Other oracle profiles
// must be reviewed before they can receive extreme numerical verification.
static_assert(std::numeric_limits<long double>::max_exponent >= 4096 &&
                  std::numeric_limits<long double>::min_exponent <= -4096,
              "Robust PPSVX extreme oracle requires a wider exponent range");
using installed_internal::Take;
constexpr auto kHost = asc::MemorySpace::kHost;
enum class Mode : std::uint8_t { kNew, kEquilibrate, kFactored, kScaled };

struct Checks {
  int checks = 0;
  int failures = 0;
  void Expect(bool value) {
    ++checks;
    if (!value) {
      ++failures;
    }
  }
};

char Fact(Mode mode) {
  if (mode == Mode::kNew) {
    return 'N';
  }
  return mode == Mode::kEquilibrate ? 'E' : 'F';
}

asc::LapackCholeskyEquilibration Equilibration(Mode mode) {
  return mode == Mode::kScaled ? asc::LapackCholeskyEquilibration::kDiagonal
                               : asc::LapackCholeskyEquilibration::kNone;
}

template <typename T>
struct Values {
  using Real = asc::DenseBlasRealType<T>;
  std::array<T, 3> original;
  std::array<T, 3> factor;
  std::array<T, 3> rhs;
  std::array<T, 3> solution{T{31}, T{-1}, T{31}};
  std::array<Real, 3> scales{Real{43}, Real{1}, Real{43}};
  std::array<Real, 3> forward{Real{37}, Real{-1}, Real{37}};
  std::array<Real, 3> backward{Real{41}, Real{-1}, Real{41}};
  Real rcond = std::numeric_limits<Real>::quiet_NaN();
  asc::LapackCholeskyEquilibration equilibration =
      asc::LapackCholeskyEquilibration::kNone;

  explicit Values(Real norm)
      : original{T{19}, T{norm}, T{19}},
        factor{T{23}, T{std::sqrt(norm)}, T{23}},
        rhs{T{29}, T{norm}, T{29}} {}
};

template <typename T>
struct Views {
  using Real = asc::DenseBlasRealType<T>;
  asc::DenseBlasPackedMatrixView<T> a;
  asc::DenseBlasPackedMatrixView<T> af;
  asc::DenseBlasMatrixView<T> b;
  asc::DenseBlasMatrixView<T> x;
  asc::DenseBlasVectorView<Real> s;
  asc::DenseBlasVectorView<Real> f;
  asc::DenseBlasVectorView<Real> e;

  Views(Values<T>& v, asc::DenseBlasLayout layout)
      : a(Take(asc::DenseBlasPackedMatrixView<T>::Create(
            v.original.data() + 1, 1, layout,
            {v.original.data(), sizeof(v.original), kHost}))),
        af(Take(asc::DenseBlasPackedMatrixView<T>::Create(
            v.factor.data() + 1, 1, layout,
            {v.factor.data(), sizeof(v.factor), kHost}))),
        b(Take(asc::DenseBlasMatrixView<T>::Create(
            v.rhs.data() + 1, 1, 1, layout, 3,
            {v.rhs.data(), sizeof(v.rhs), kHost}))),
        x(Take(asc::DenseBlasMatrixView<T>::Create(
            v.solution.data() + 1, 1, 1, layout, 3,
            {v.solution.data(), sizeof(v.solution), kHost}))),
        s(Take(asc::DenseBlasVectorView<Real>::Create(
            v.scales.data() + 1, 1, 1,
            {v.scales.data(), sizeof(v.scales), kHost}))),
        f(Take(asc::DenseBlasVectorView<Real>::Create(
            v.forward.data() + 1, 1, 1,
            {v.forward.data(), sizeof(v.forward), kHost}))),
        e(Take(asc::DenseBlasVectorView<Real>::Create(
            v.backward.data() + 1, 1, 1,
            {v.backward.data(), sizeof(v.backward), kHost}))) {}
};

template <typename T>
asc::Result<asc::LapackWorkspacePlan> Query(
    const asc::ReferenceLapackProvider& provider,
    asc::DenseBlasTriangle triangle, Mode mode, const Views<T>& v,
    const Values<T>& values) {
  if (mode == Mode::kNew) {
    return asc::QueryRobustPpsvxWorkspace(provider, triangle, v.a, v.af, v.b,
                                          v.x, v.f, v.e, values.rcond);
  }
  if (mode == Mode::kEquilibrate) {
    return asc::QueryRobustPpsvxEquilibratedWorkspace(
        provider, triangle, v.a, v.af, values.equilibration, v.s, v.b, v.x, v.f,
        v.e, values.rcond);
  }
  return asc::QueryRobustPpsvxFactoredWorkspace(provider, triangle, v.a, v.af,
                                                Equilibration(mode), v.s, v.b,
                                                v.x, v.f, v.e, values.rcond);
}

template <typename T>
asc::Status Execute(const asc::ReferenceLapackProvider& provider,
                    asc::DenseBlasTriangle triangle, Mode mode,
                    const Views<T>& v, Values<T>& values,
                    const asc::LapackWorkspacePlan& plan,
                    const asc::LapackWorkspace& workspace,
                    asc::LapackReport& report) {
  if (mode == Mode::kNew) {
    return asc::RobustPpsvx(provider, triangle, v.a, v.af, v.b, v.x, v.f, v.e,
                            values.rcond, plan, workspace, report);
  }
  if (mode == Mode::kEquilibrate) {
    return asc::RobustPpsvxEquilibrated(
        provider, triangle, v.a, v.af, values.equilibration, v.s, v.b, v.x, v.f,
        v.e, values.rcond, plan, workspace, report);
  }
  return asc::RobustPpsvxFactored(provider, triangle, v.a, v.af,
                                  Equilibration(mode), v.s, v.b, v.x, v.f, v.e,
                                  values.rcond, plan, workspace, report);
}

template <typename T>
void Mathematics(asc::DenseBlasRealType<T> norm, Mode mode,
                 const Values<T>& actual, bool success,
                 const asc::LapackReport& report, Checks& checks) {
  using Real = asc::DenseBlasRealType<T>;
  // Original A=B=[norm] has condition one and exact solution one. For the
  // scalar refinement, a 64-epsilon relative solution error bounds residual
  // and weight terms by128*epsilon plus2*SAFE1/norm. The latter is omitted
  // after equilibration to order one. These finite mathematical bounds are
  // evaluated in long double; no native overflowing intermediate is reused.
  const long double eps = std::numeric_limits<Real>::epsilon();
  const long double safe = 2.0L * std::numeric_limits<Real>::min();
  const bool scaled =
      mode == Mode::kEquilibrate &&
      actual.equilibration == asc::LapackCholeskyEquilibration::kDiagonal;
  const long double bound = 128 * eps + (scaled ? 0 : 2 * safe / norm);
  const auto solution =
      static_cast<std::complex<long double>>(actual.solution[1]);
  const bool correct =
      std::isfinite(std::abs(solution)) &&
      std::abs(solution - std::complex<long double>{1, 0}) <= 64 * eps &&
      std::isfinite(actual.rcond) && std::abs(actual.rcond - 1) <= 64 * eps &&
      std::isfinite(actual.forward[1]) && actual.forward[1] >= 0 &&
      actual.forward[1] <= bound && std::isfinite(actual.backward[1]) &&
      actual.backward[1] >= 0 && actual.backward[1] <= 1;
  checks.Expect(success && correct &&
                report.output_validity == asc::LapackOutputValidity::kComplete);
  if (!correct || !success) {
    std::printf(
        "Required scalar robust PPSVX failure: FACT=%c supplied_scaled=%d "
        "A=%La "
        "X=(%La,%La) RCOND=%La FERR=%La finite_bound=%La BERR=%La "
        "first_party_route\n",
        Fact(mode), mode == Mode::kScaled ? 1 : 0,
        static_cast<long double>(norm), solution.real(), solution.imag(),
        static_cast<long double>(actual.rcond),
        static_cast<long double>(actual.forward[1]), bound,
        static_cast<long double>(actual.backward[1]));
  }
}

template <typename T>
void Guards(const Values<T>& v, Checks& checks) {
  using Real = asc::DenseBlasRealType<T>;
  checks.Expect(v.original.front() == T{19} && v.original.back() == T{19} &&
                v.factor.front() == T{23} && v.factor.back() == T{23} &&
                v.rhs.front() == T{29} && v.rhs.back() == T{29} &&
                v.solution.front() == T{31} && v.solution.back() == T{31});
  checks.Expect(v.scales.front() == Real{43} && v.scales.back() == Real{43} &&
                v.forward.front() == Real{37} && v.forward.back() == Real{37} &&
                v.backward.front() == Real{41} &&
                v.backward.back() == Real{41});
}

template <typename T>
void Case(const asc::ReferenceLapackProvider& provider,
          asc::DenseBlasTriangle triangle, asc::DenseBlasLayout layout,
          Mode mode, asc::DenseBlasRealType<T> norm, Checks& checks) {
  using Real = asc::DenseBlasRealType<T>;
  const Real root = std::sqrt(norm);
  checks.Expect(std::isfinite(norm) && norm > 0 && std::isfinite(root) &&
                root > 0 &&
                std::abs(static_cast<long double>(root) * root / norm - 1) <=
                    2 * std::numeric_limits<Real>::epsilon());
  Values<T> actual(norm);
  const Views<T> views(actual, layout);
  const auto plan = Take(Query(provider, triangle, mode, views, actual));
  alignas(std::max_align_t) std::array<std::byte, 65536> storage{};
  asc::LapackWorkspace workspace;
  workspace
      .regions[static_cast<std::size_t>(asc::LapackWorkspaceKind::kScratch)] = {
      storage.data(), storage.size(), kHost};
  asc::LapackReport report;
  const auto status =
      Execute(provider, triangle, mode, views, actual, plan, workspace, report);
  checks.Expect(!report.called_provider && !report.native_info.has_value());
  checks.Expect(
      std::string_view(report.routine.data()).starts_with("asc_robust_"));
  Guards(actual, checks);
  Mathematics(norm, mode, actual, status.ok(), report, checks);
}

template <typename T>
int Run(const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  const std::array norms{Real{1},
                         std::numeric_limits<Real>::min(),
                         std::numeric_limits<Real>::max() / 4,
                         std::numeric_limits<Real>::denorm_min(),
                         2 * std::numeric_limits<Real>::denorm_min(),
                         std::numeric_limits<Real>::max()};
  Checks checks;
  for (const auto triangle :
       {asc::DenseBlasTriangle::kUpper, asc::DenseBlasTriangle::kLower}) {
    for (const auto layout : {asc::DenseBlasLayout::kRowMajor,
                              asc::DenseBlasLayout::kColumnMajor}) {
      for (const auto mode :
           {Mode::kNew, Mode::kEquilibrate, Mode::kFactored, Mode::kScaled}) {
        for (const Real norm : norms) {
          Case<T>(provider, triangle, layout, mode, norm, checks);
        }
      }
    }
  }
  std::printf(
      "Packed Cholesky required scalar expert: %d checks, %d failures\n",
      checks.checks, checks.failures);
  return checks.failures == 0 ? 0 : 1;
}
}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard guard;
  if (argc != 2) {
    return 2;
  }
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    return Run<float>(provider);
  }
  if (scalar == "d") {
    return Run<double>(provider);
  }
  if (scalar == "c") {
    return Run<std::complex<float>>(provider);
  }
  if (scalar == "z") {
    return Run<std::complex<double>>(provider);
  }
  return 2;
}
