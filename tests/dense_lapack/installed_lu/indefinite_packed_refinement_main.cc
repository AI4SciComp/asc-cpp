#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <limits>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite_packed.h"
#include "asc/dense/providers/lapack_indefinite_packed_refinement.h"
#include "factorization_support.h"
#include "normal_return_guard.h"

namespace {
using installed_internal::Take;
using installed_internal::Value;
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;
constexpr auto kLower = asc::DenseBlasTriangle::kLower;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;
constexpr auto kPacking =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);
constexpr auto kInteger =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kInteger);

struct Checks {
  int workflows = 0;
  int failures = 0;
  void Expect(bool value, const char* message) {
    if (!value) {
      if (failures < 20) {
        std::fprintf(stderr, "Packed refinement consumer: %s (workflow %d)\n",
                     message, workflows);
      }
      ++failures;
    }
  }
};

// Bitwise input immutability includes ignored NaN imaginary diagonals.
bool SameBytes(const void* first, const void* second, std::size_t bytes) {
  return std::memcmp(first, second, bytes) == 0;
}

// Enumerate the public physical storage without adapter indexing helpers.
template <typename Visit>
void Slots(asc::extent_t n, asc::DenseBlasTriangle triangle,
           asc::DenseBlasLayout layout, const Visit& visit) {
  std::size_t slot = 0;
  for (asc::extent_t major = 0; major < n; ++major) {
    for (asc::extent_t minor = 0; minor < n; ++minor) {
      const auto i = layout == kColumn ? minor : major;
      const auto j = layout == kColumn ? major : minor;
      if (triangle == kUpper ? i <= j : i >= j) {
        visit(slot++, i, j);
      }
    }
  }
}

template <typename T>
auto Query(const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasTriangle triangle, bool hermitian,
           asc::DenseBlasPackedMatrixView<T> matrix,
           asc::DenseBlasVectorView<asc::index_t> pivots) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::QueryHptrfWorkspace(provider, triangle, matrix, pivots);
    }
  }
  return asc::QuerySptrfWorkspace(provider, triangle, matrix, pivots);
}

template <typename T>
asc::Status Factor(const asc::ReferenceLapackProvider& provider,
                   asc::DenseBlasTriangle triangle, bool hermitian,
                   asc::DenseBlasPackedMatrixView<T> matrix,
                   asc::DenseBlasVectorView<asc::index_t> pivots,
                   const asc::LapackWorkspacePlan& plan,
                   const asc::LapackWorkspace& workspace,
                   asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::Hptrf(provider, triangle, matrix, pivots, plan, workspace,
                        report);
    }
  }
  return asc::Sptrf(provider, triangle, matrix, pivots, plan, workspace,
                    report);
}

template <typename T>
std::array<T, 18> Initialize(std::array<T, 18>& data, asc::extent_t n,
                             asc::DenseBlasTriangle triangle,
                             asc::DenseBlasLayout layout, bool hermitian,
                             int exponent, bool singular) {
  const auto scale = std::ldexp(1.0, exponent);
  // A block diagonal D with zero-diagonal nonsingular 2x2 blocks forces
  // genuine paired pivots. There are no triangular multipliers: A = D.
  // Check every factor slot against this independent matrix equation.
  Slots(n, triangle, layout, [&](std::size_t slot, auto i, auto j) {
    T value{};
    if (!singular) {
      if (i == j && (n == 1 || i == 2)) {
        value = Value<T>(3 * scale, hermitian ? 0 : scale / 4);
      } else if ((i == 0 && j == 1) || (i == 1 && j == 0) ||
                 (i == 3 && j == 4) || (i == 4 && j == 3)) {
        value = Value<T>(scale, scale / 4);
        if (hermitian && i < j) {
          value = installed_internal::Conjugate(value);
        }
      }
    }
    data[slot + 1] = value;
  });
  const auto expected = data;
  // The Hermitian API ignores the input imaginary diagonal, even a NaN.
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      Slots(n, triangle, layout, [&](std::size_t slot, auto i, auto j) {
        if (i == j) {
          data[slot + 1].imag(
              std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN());
        }
      });
    }
  }
  return expected;
}

template <typename T>
struct Inputs {
  using Real = asc::DenseBlasRealType<T>;
  asc::DenseBlasPackedMatrixView<const T> a;
  asc::DenseBlasPackedMatrixView<const T> af;
  asc::RawLapackPivotView pivots;
  asc::DenseBlasMatrixView<const T> b;
  asc::DenseBlasMatrixView<T> x;
  asc::DenseBlasVectorView<Real> ferr;
  asc::DenseBlasVectorView<Real> berr;
  bool hermitian;
  asc::DenseBlasTriangle triangle;
  auto Query(const asc::ReferenceLapackProvider& provider) const {
    if constexpr (asc::DenseBlasComplex<T>) {
      if (hermitian) {
        return asc::QueryHprfsWorkspace(provider, triangle, a, af, pivots, b, x,
                                        ferr, berr);
      }
    }
    return asc::QuerySprfsWorkspace(provider, triangle, a, af, pivots, b, x,
                                    ferr, berr);
  }
  asc::Status Execute(const asc::ReferenceLapackProvider& provider,
                      const asc::LapackWorkspacePlan& plan,
                      const asc::LapackWorkspace& work,
                      asc::LapackReport& report) const {
    if constexpr (asc::DenseBlasComplex<T>) {
      if (hermitian) {
        return asc::Hprfs(provider, triangle, a, af, pivots, b, x, ferr, berr,
                          plan, work, report);
      }
    }
    return asc::Sprfs(provider, triangle, a, af, pivots, b, x, ferr, berr, plan,
                      work, report);
  }
};

template <typename T>
struct Scratch {
  using Real = asc::DenseBlasRealType<T>;
  static constexpr auto kScalar =
      static_cast<std::size_t>(asc::LapackWorkspaceKind::kScalar);
  static constexpr auto kReal =
      static_cast<std::size_t>(asc::LapackWorkspaceKind::kReal);
  std::array<T, 17> scalar;
  std::array<Real, 13> real;
  std::array<T, 62> packing;
  alignas(std::max_align_t) std::array<std::byte, 112> integers;
  Scratch() {
    scalar.fill(T{-83});
    real.fill(Real{-87});
    packing.fill(T{-89});
    integers.fill(std::byte{0x61});
  }
  asc::LapackWorkspace Workspace(const asc::LapackWorkspacePlan& plan) {
    asc::LapackWorkspace work;
    for (auto role : {kScalar, kReal, kPacking, kInteger}) {
      void* address = nullptr;
      std::size_t capacity = 0;
      if (role == kScalar) {
        address = scalar.data() + 1;
        capacity = sizeof(T) * (scalar.size() - 2);
      }
      if (role == kReal) {
        address = real.data() + 1;
        capacity = sizeof(Real) * (real.size() - 2);
      }
      if (role == kPacking) {
        address = packing.data() + 1;
        capacity = sizeof(T) * (packing.size() - 2);
      }
      if (role == kInteger) {
        address = integers.data() + 16;
        capacity = integers.size() - 32;
      }
      const auto bytes =
          static_cast<std::size_t>(plan.regions[role].minimum_entries) *
          plan.regions[role].entry_bytes;
      if (bytes > capacity) {
        std::abort();
      }
      if (bytes != 0) {
        work.regions[role] = {address, bytes, kHost};
      }
    }
    return work;
  }
  void Guards(Checks& checks, const asc::LapackWorkspace& work) const {
    for (std::size_t i = 0; i < scalar.size(); ++i) {
      if (i == 0 || i > work.regions[kScalar].size() / sizeof(T)) {
        checks.Expect(scalar[i] == T{-83}, "scalar guard");
      }
    }
    for (std::size_t i = 0; i < real.size(); ++i) {
      if (i == 0 || i > work.regions[kReal].size() / sizeof(Real)) {
        checks.Expect(real[i] == Real{-87}, "private errors/RWORK guard");
      }
    }
    for (std::size_t i = 0; i < packing.size(); ++i) {
      if (i == 0 || i > work.regions[kPacking].size() / sizeof(T)) {
        checks.Expect(packing[i] == T{-89}, "packing guard");
      }
    }
    for (std::size_t i = 0; i < integers.size(); ++i) {
      if (i < 16 || i >= 16 + work.regions[kInteger].size()) {
        checks.Expect(integers[i] == std::byte{0x61}, "native INTEGER guard");
      }
    }
  }
};

template <typename T>
struct Sample {
  using Real = asc::DenseBlasRealType<T>;
  using Wide = std::complex<long double>;
  asc::extent_t n;
  asc::extent_t nrhs;
  bool hermitian;
  asc::DenseBlasTriangle triangle;
  asc::DenseBlasLayout a_layout;
  asc::DenseBlasLayout af_layout;
  asc::DenseBlasLayout b_layout;
  asc::DenseBlasLayout x_layout;
  std::array<T, 18> a;
  std::array<T, 18> af;
  std::array<T, 18> expected_af;
  std::array<asc::index_t, 7> pivots;
  std::array<T, 42> b;
  std::array<T, 42> x;
  std::array<T, 42> initial_x;
  std::array<Real, 5> ferr;
  std::array<Real, 5> berr;
  std::array<Wide, 25> full{};
  Sample(asc::extent_t order, asc::extent_t columns, bool he,
         asc::DenseBlasTriangle uplo, int layouts, int exponent)
      : n(order),
        nrhs(columns),
        hermitian(he),
        triangle(uplo),
        a_layout((layouts & 1) != 0 ? kRow : kColumn),
        af_layout((layouts & 2) != 0 ? kRow : kColumn),
        b_layout((layouts & 4) != 0 ? kRow : kColumn),
        x_layout((layouts & 8) != 0 ? kRow : kColumn) {
    a.fill(T{-71});
    af.fill(T{-71});
    pivots.fill(-79);
    const auto original =
        Initialize(a, n, triangle, a_layout, he, exponent, false);
    expected_af = Initialize(af, n, triangle, af_layout, he, exponent, false);
    Slots(n, triangle, a_layout, [&](std::size_t slot, auto i, auto j) {
      const auto value = installed_internal::Widen(original[slot + 1]);
      full[i * n + j] = value;
      full[j * n + i] = he ? std::conj(value) : value;
    });
    b.fill(T{-97});
    x.fill(T{-101});
    ferr.fill(Real{-103});
    berr.fill(Real{-107});
    for (asc::extent_t j = 0; j < nrhs; ++j) {
      for (asc::extent_t i = 0; i < n; ++i) {
        Wide sum{};
        for (asc::extent_t k = 0; k < n; ++k) {
          sum += full[i * n + k] * installed_internal::Widen(Known(k, j));
        }
        b[Offset(b_layout, i, j)] = Value<T>(sum.real(), sum.imag());
        x[Offset(x_layout, i, j)] = Known(i, j) * Real{1.0625};
      }
    }
    initial_x = x;
  }
  static T Known(asc::extent_t i, asc::extent_t j) {
    return Value<T>(static_cast<long double>(i + j + 1) / 4,
                    static_cast<long double>((i + 2 * j) % 3) / 8);
  }
  [[nodiscard]] asc::extent_t Leading(asc::DenseBlasLayout layout) const {
    return layout == kColumn ? n + 2 : nrhs + 2;
  }
  [[nodiscard]] std::size_t Offset(asc::DenseBlasLayout layout, asc::extent_t i,
                                   asc::extent_t j) const {
    return 1 + static_cast<std::size_t>(layout == kColumn
                                            ? j * Leading(layout) + i
                                            : i * Leading(layout) + j);
  }
  Inputs<T> Views() {
    return {Take(asc::DenseBlasPackedMatrixView<const T>::Create(
                a.data() + 1, n, a_layout, {a.data(), sizeof(a), kHost})),
            Take(asc::DenseBlasPackedMatrixView<const T>::Create(
                af.data() + 1, n, af_layout, {af.data(), sizeof(af), kHost})),
            Take(asc::RawLapackPivotView::Create(
                pivots.data() + 1, n, asc::LapackFactorFamily::kBunchKaufman,
                {pivots.data(), sizeof(pivots), kHost})),
            Take(asc::DenseBlasMatrixView<const T>::Create(
                b.data() + 1, n, nrhs, b_layout, Leading(b_layout),
                {b.data(), sizeof(b), kHost})),
            Take(asc::DenseBlasMatrixView<T>::Create(
                x.data() + 1, n, nrhs, x_layout, Leading(x_layout),
                {x.data(), sizeof(x), kHost})),
            Take(asc::DenseBlasVectorView<Real>::Create(
                ferr.data() + 1, nrhs, 1, {ferr.data(), sizeof(ferr), kHost})),
            Take(asc::DenseBlasVectorView<Real>::Create(
                berr.data() + 1, nrhs, 1, {berr.data(), sizeof(berr), kHost})),
            hermitian,
            triangle};
  }
  void Prepare(Checks& checks, const asc::ReferenceLapackProvider& provider) {
    const auto matrix = Take(asc::DenseBlasPackedMatrixView<T>::Create(
        af.data() + 1, n, af_layout, {af.data(), sizeof(af), kHost}));
    const auto pivot_view = Take(asc::DenseBlasVectorView<asc::index_t>::Create(
        pivots.data() + 1, n, 1, {pivots.data(), sizeof(pivots), kHost}));
    const auto plan =
        Take(Query(provider, triangle, hermitian, matrix, pivot_view));
    Scratch<T> scratch;
    const auto work = scratch.Workspace(plan);
    asc::LapackReport report;
    const auto status = Factor(provider, triangle, hermitian, matrix,
                               pivot_view, plan, work, report);
    checks.Expect(
        status.ok() &&
            report.output_validity == asc::LapackOutputValidity::kComplete &&
            report.called_provider == (n != 0),
        "factor producer completion");
    checks.Expect(af == expected_af, "independent block-diagonal factors");
    for (asc::extent_t i = 0; i < n; ++i) {
      asc::index_t expected = i + 1;
      if (n != 1 && i != 2) {
        expected = -((i < 2 ? 1 : 4) + (triangle == kLower ? 1 : 0));
      }
      checks.Expect(pivots[i + 1] == expected, "classic paired pivot encoding");
    }
    scratch.Guards(checks, work);
  }
  static long double Abs1(Wide value) {
    return std::abs(value.real()) + std::abs(value.imag());
  }
  void Mathematics(Checks& checks) const {
    const long double tolerance = 256 * std::numeric_limits<Real>::epsilon();
    auto padded = initial_x;
    for (asc::extent_t j = 0; j < nrhs; ++j) {
      checks.Expect(std::isfinite(ferr[j + 1]) && ferr[j + 1] >= 0 &&
                        std::isfinite(berr[j + 1]) && berr[j + 1] >= 0,
                    "finite nonnegative estimates");
      if (n == 0) {
        checks.Expect(ferr[j + 1] == 0 && berr[j + 1] == 0,
                      "empty error outputs");
        continue;
      }
      long double forward = 0;
      long double norm_x = 0;
      long double backward = 0;
      for (asc::extent_t i = 0; i < n; ++i) {
        const auto actual =
            installed_internal::Widen(x[Offset(x_layout, i, j)]);
        padded[Offset(x_layout, i, j)] = x[Offset(x_layout, i, j)];
        forward = std::max(
            forward, Abs1(actual - installed_internal::Widen(Known(i, j))));
        norm_x = std::max(norm_x, Abs1(actual));
        auto residual = installed_internal::Widen(b[Offset(b_layout, i, j)]);
        long double denominator = Abs1(residual);
        for (asc::extent_t k = 0; k < n; ++k) {
          const auto solution =
              installed_internal::Widen(x[Offset(x_layout, k, j)]);
          residual -= full[i * n + k] * solution;
          denominator += Abs1(full[i * n + k]) * Abs1(solution);
        }
        backward = std::max(backward, Abs1(residual) / denominator);
      }
      checks.Expect(norm_x > 0 && std::isfinite(forward) &&
                        forward / norm_x <= tolerance &&
                        forward / norm_x <= ferr[j + 1] + tolerance,
                    "independent forward error");
      checks.Expect(std::isfinite(backward) && backward <= tolerance &&
                        std::abs(backward - berr[j + 1]) <= tolerance,
                    "independent residual and BERR");
    }
    checks.Expect(x == padded, "solution padding");
    for (std::size_t j = 0; j < ferr.size(); ++j) {
      if (j == 0 || j > static_cast<std::size_t>(nrhs)) {
        checks.Expect(ferr[j] == Real{-103} && berr[j] == Real{-107},
                      "public error guards");
      }
    }
  }
};

template <typename T>
void Case(Checks& checks, const asc::ReferenceLapackProvider& provider,
          Sample<T> sample) {
  ++checks.workflows;
  sample.Prepare(checks, provider);
  const auto before = sample;
  const auto values = sample.Views();
  const auto plan = Take(values.Query(provider));
  Scratch<T> scratch;
  const auto work = scratch.Workspace(plan);
  const bool active = sample.n != 0 && sample.nrhs != 0;
  checks.Expect(
      plan.regions[Scratch<T>::kReal].minimum_entries ==
          (active ? (asc::DenseBlasComplex<T> ? sample.n : 0) + 2 * sample.nrhs
                  : 0),
      "exact private error/RWORK count");
  asc::LapackReport report;
  const auto status = values.Execute(provider, plan, work, report);
  checks.Expect(status.ok() && report.output_validity ==
                                   asc::LapackOutputValidity::kComplete,
                "refinement completion");
  checks.Expect(report.called_provider == active &&
                    report.native_info.has_value() == active &&
                    report.native_info.value_or(0) == 0,
                "refinement native call boundary");
  checks.Expect(!report.factor_family, "no factor certificate from refinement");
  sample.Mathematics(checks);
  checks.Expect(SameBytes(sample.a.data(), before.a.data(), sizeof(sample.a)) &&
                    sample.af == before.af && sample.pivots == before.pivots &&
                    sample.b == before.b,
                "immutable AP/AFP/IPIV/B");
  scratch.Guards(checks, work);
  const auto saved_scratch = scratch;
  const auto saved = sample;
  auto stale_plan = plan;
  ++stale_plan.regions[Scratch<T>::kReal].minimum_entries;
  const auto rejected = values.Execute(provider, stale_plan, work, report);
  checks.Expect(rejected.code() == asc::ErrorCode::kInvalidState &&
                    !report.called_provider && !report.native_info,
                "stale plan rejected");
  checks.Expect(sample.x == saved.x && sample.ferr == saved.ferr &&
                    sample.berr == saved.berr &&
                    scratch.scalar == saved_scratch.scalar &&
                    scratch.real == saved_scratch.real &&
                    scratch.packing == saved_scratch.packing &&
                    scratch.integers == saved_scratch.integers,
                "stale plan preserves all outputs and workspace");
}

template <typename T>
void Run(Checks& checks, const asc::ReferenceLapackProvider& provider,
         bool hermitian) {
  for (asc::extent_t n : {0, 1, 2, 5}) {
    for (asc::extent_t nrhs : {0, 1, 3}) {
      for (auto triangle : {kUpper, kLower}) {
        for (int layouts = 0; layouts < 16; ++layouts) {
          for (int exponent : {-20, 0, 20}) {
            Case(checks, provider,
                 Sample<T>(n, nrhs, hermitian, triangle, layouts, exponent));
          }
        }
      }
    }
  }
}
}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard normal_return;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  Checks checks;
  Run<float>(checks, provider, false);
  Run<double>(checks, provider, false);
  Run<std::complex<float>>(checks, provider, false);
  Run<std::complex<double>>(checks, provider, false);
  Run<std::complex<float>>(checks, provider, true);
  Run<std::complex<double>>(checks, provider, true);
  checks.Expect(checks.workflows == 6912, "all public workflows executed");
  std::printf(
      "packed refinement installed consumer: %d workflows, %d failures\n",
      checks.workflows, checks.failures);
  return checks.failures == 0 ? 0 : 1;
}
