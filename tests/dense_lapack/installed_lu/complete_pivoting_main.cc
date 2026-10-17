#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <span>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_complete_pivoting.h"
#include "normal_return_guard.h"

namespace {
struct TestContext {
  int failures = 0;
  void Check(bool condition, int line) {
    if (!condition) {
      ++failures;
      std::fprintf(stderr, "Installed complete-pivoting failure at %d\n", line);
    }
  }
};
#define ASC_C2_CHECK(test, expression) \
  (test).Check(static_cast<bool>(expression), __LINE__)
#define ASC_C2_EQ(test, actual, expected) \
  (test).Check((actual) == (expected), __LINE__)
namespace support {
using Wide = std::complex<long double>;
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kScratch =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kScratch);
constexpr auto kInteger =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kInteger);
template <typename T>
T Take(asc::Result<T> result) {
  if (!result.ok()) {
    std::fprintf(stderr, "Installed setup failed\n");
    std::abort();
  }
  return std::move(*result);
}
template <typename T>
T Value(long double real, long double imaginary = 0) {
  using Real = asc::DenseBlasRealType<T>;
  if constexpr (asc::DenseBlasComplex<T>) {
    return {static_cast<Real>(real), static_cast<Real>(imaginary)};
  } else {
    return static_cast<T>(real);
  }
}
template <typename T>
Wide ToWide(T value) {
  if constexpr (asc::DenseBlasComplex<T>) {
    return {value.real(), value.imag()};
  } else {
    return {value, 0};
  }
}
template <typename T, std::size_t N>
auto Vector(std::array<T, N>& values, asc::extent_t count) {
  return Take(asc::DenseBlasVectorView<T>::Create(
      values.data() + 1, count, 1, {values.data(), sizeof(values), kHost}));
}
template <typename T, std::size_t N>
auto Vector(const std::array<T, N>& values, asc::extent_t count) {
  return Take(asc::DenseBlasVectorView<const T>::Create(
      values.data() + 1, count, 1, {values.data(), sizeof(values), kHost}));
}
template <typename T>
bool Same(const T& a, const T& b) {
  const auto x = std::as_bytes(std::span(&a, 1));
  const auto y = std::as_bytes(std::span(&b, 1));
  return std::equal(x.begin(), x.end(), y.begin());
}

template <typename T>
T Coefficient(asc::extent_t n, asc::extent_t i, asc::extent_t j,
              asc::extent_t seed, int exponent) {
  const auto row = (i + seed) % n;
  const auto column = (n - j + seed) % n;
  const auto real =
      row == column
          ? static_cast<long double>(4 * n + row + 1)
          : static_cast<long double>((row + 2 * column + seed) % 7 - 3) / 8;
  const auto imaginary =
      static_cast<long double>((2 * row + column + seed) % 5 - 2) / 16;
  return Value<T>(std::scalbn(real, exponent),
                  std::scalbn(imaginary, exponent));
}

template <typename T>
struct Problem {
  using Real = asc::DenseBlasRealType<T>;
  std::array<T, 160> dense;
  std::array<T, 10> rhs;
  std::array<asc::index_t, 10> rows;
  std::array<asc::index_t, 10> columns;
  std::array<Real, 3> scale{Real{-71}, Real{-73}, Real{-79}};
  asc::extent_t n;
  asc::extent_t leading;
  asc::DenseBlasLayout layout;
  bool zero;
  Problem(asc::extent_t order, asc::extent_t padding,
          asc::DenseBlasLayout storage, asc::extent_t seed, int exponent,
          bool singular = false, int rhs_kind = 1)
      : n(order),
        leading(std::max(asc::extent_t{1}, order) + padding),
        layout(storage),
        zero(singular) {
    dense.fill(Value<T>(-83, 23));
    rhs.fill(Value<T>(-89, 29));
    rows.fill(-97);
    columns.fill(-101);
    for (asc::extent_t j = 0; j < n; ++j) {
      for (asc::extent_t i = 0; i < n; ++i) {
        At(i, j) = zero ? T{} : Coefficient<T>(n, i, j, seed, exponent);
      }
    }
    for (asc::extent_t i = 0; i < n; ++i) {
      Wide value{};
      if (zero) {
        Real base = Real{1};
        if (rhs_kind == 0) {
          base = Small();
        } else if (rhs_kind != 1) {
          base = std::numeric_limits<Real>::max() / Real{32};
        }
        const Real real = base * static_cast<Real>(i + 1);
        value = ToWide(Value<T>(real, real / 2));
      } else {
        for (asc::extent_t j = 0; j < n; ++j) {
          value += ToWide(At(i, j)) * Exact(j);
        }
      }
      rhs[static_cast<std::size_t>(i + 1)] =
          Value<T>(value.real(), value.imag());
    }
  }
  static Real Small() {
    return std::numeric_limits<Real>::min() /
           std::numeric_limits<Real>::epsilon();
  }
  static Wide Exact(asc::extent_t j) {
    return ToWide(Value<T>(static_cast<long double>(j + 1) / 8,
                           static_cast<long double>(j + 2) / 16));
  }
  [[nodiscard]] std::size_t Index(asc::extent_t i, asc::extent_t j) const {
    return static_cast<std::size_t>(
        1 + (layout == kColumn ? j * leading + i : i * leading + j));
  }
  T& At(asc::extent_t i, asc::extent_t j) { return dense[Index(i, j)]; }
  [[nodiscard]] T At(asc::extent_t i, asc::extent_t j) const {
    return dense[Index(i, j)];
  }
  auto Matrix() {
    return Take(asc::DenseBlasMatrixView<T>::Create(
        dense.data() + 1, n, n, layout, leading,
        {dense.data(), sizeof(dense), kHost}));
  }
  [[nodiscard]] auto Matrix() const {
    return Take(asc::DenseBlasMatrixView<const T>::Create(
        dense.data() + 1, n, n, layout, leading,
        {dense.data(), sizeof(dense), kHost}));
  }
  auto Query(const asc::ReferenceLapackProvider& provider) {
    return asc::QueryGetc2Workspace(provider, Matrix(), Vector(rows, n),
                                    Vector(columns, n));
  }
  auto Factor(const asc::ReferenceLapackProvider& provider,
              const asc::LapackWorkspacePlan& plan,
              const asc::LapackWorkspace& workspace,
              asc::LapackReport& report) {
    return asc::Getc2(provider, Matrix(), Vector(rows, n), Vector(columns, n),
                      plan, workspace, report);
  }
  [[nodiscard]] auto View(const asc::LapackReport& report) const {
    return asc::ReferenceCompletePivotLuFactorView<T>::Create(
        Matrix(), Vector(rows, n), Vector(columns, n), report);
  }
  void Guards(TestContext& test) const {
    std::array<bool, 160> active{};
    for (asc::extent_t j = 0; j < n; ++j) {
      for (asc::extent_t i = 0; i < n; ++i) {
        active[Index(i, j)] = true;
      }
    }
    for (std::size_t i = 0; i < dense.size(); ++i) {
      if (!active[i]) {
        ASC_C2_EQ(test, dense[i], Value<T>(-83, 23));
      }
    }
    for (std::size_t i = 0; i < rhs.size(); ++i) {
      if (i == 0 || i > static_cast<std::size_t>(n)) {
        ASC_C2_EQ(test, rhs[i], Value<T>(-89, 29));
        ASC_C2_EQ(test, rows[i], -97);
        ASC_C2_EQ(test, columns[i], -101);
      }
    }
    ASC_C2_EQ(test, scale.front(), Real{-71});
    ASC_C2_EQ(test, scale.back(), Real{-79});
  }
};

template <typename T>
struct Scratch {
  std::array<T, 164> scalar;
  alignas(std::max_align_t) std::array<std::byte, 192> integer;
  std::size_t scalar_entries = 0;
  std::size_t integer_bytes = 0;
  Scratch() {
    scalar.fill(Value<T>(-103, 31));
    integer.fill(std::byte{0x5a});
  }
  auto Workspace(const asc::LapackWorkspacePlan& plan) {
    asc::LapackWorkspace workspace;
    scalar_entries =
        static_cast<std::size_t>(plan.regions[kScratch].minimum_entries);
    integer_bytes =
        static_cast<std::size_t>(plan.regions[kInteger].minimum_entries) *
        plan.regions[kInteger].entry_bytes;
    if (scalar_entries != 0) {
      workspace.regions[kScratch] = {scalar.data() + 1,
                                     scalar_entries * sizeof(T), kHost};
    }
    if (integer_bytes != 0) {
      workspace.regions[kInteger] = {integer.data() + 16, integer_bytes, kHost};
    }
    return workspace;
  }
  void Guards(TestContext& test) const {
    for (std::size_t i = 0; i < scalar.size(); ++i) {
      if (i == 0 || i > scalar_entries) {
        ASC_C2_EQ(test, scalar[i], Value<T>(-103, 31));
      }
    }
    for (std::size_t i = 0; i < integer.size(); ++i) {
      if (i < 16 || i >= 16 + integer_bytes) {
        ASC_C2_EQ(test, integer[i], std::byte{0x5a});
      }
    }
  }
};

}  // namespace support
using support::Take;
using support::ToWide;
using support::Value;
using support::Vector;
using support::Wide;
template <typename Operation>
auto Observe(TestContext& /*test*/, bool /*audit*/, Operation operation) {
  return operation();
}
asc::LapackReport PriorReport() {
  asc::LapackReport report;
  report.routine.fill('Q');
  report.called_provider = true;
  report.native_info = 79;
  report.native_argument = 31;
  report.diagnostic_index = 17;
  report.factor_family = asc::LapackFactorFamily::kLuPartialPivot;
  report.outcome = asc::LapackOutcome::kSingular;
  report.output_validity = asc::LapackOutputValidity::kUnusable;
  return report;
}

void Report(TestContext& test, const asc::ReferenceLapackProvider& provider,
            const asc::LapackReport& report, asc::extent_t n, bool factor,
            bool warning) {
  ASC_C2_EQ(test, report.provider, provider.identity());
  ASC_C2_EQ(test, report.called_provider, n != 0);
  ASC_C2_CHECK(test, !report.factor_family.has_value());
  ASC_C2_CHECK(test, !report.native_argument.has_value());
  ASC_C2_EQ(test, report.native_info.has_value(), factor && n != 0);
  if (factor && n != 0) {
    ASC_C2_EQ(test, report.native_info, warning ? n : 0);
  }
  ASC_C2_EQ(test, report.diagnostic_index.has_value(), factor && warning);
  if (factor && warning) {
    ASC_C2_EQ(test, report.diagnostic_index, n - 1);
  }
  ASC_C2_EQ(test, report.output_validity, asc::LapackOutputValidity::kComplete);
  ASC_C2_EQ(test, report.outcome,
            warning ? asc::LapackOutcome::kAccuracyWarning
                    : asc::LapackOutcome::kSuccess);
}

template <typename T>
void Reconstruction(TestContext& test, const support::Problem<T>& original,
                    const support::Problem<T>& factors) {
  const auto n = original.n;
  std::array<asc::extent_t, 8> rows{};
  std::array<asc::extent_t, 8> columns{};
  for (asc::extent_t i = 0; i < n; ++i) {
    rows[static_cast<std::size_t>(i)] = i;
    columns[static_cast<std::size_t>(i)] = i;
  }
  for (asc::extent_t i = 0; i < n; ++i) {
    const auto k = static_cast<std::size_t>(i + 1);
    const auto row = factors.rows[k];
    const auto column = factors.columns[k];
    const bool valid =
        row >= i + 1 && row <= n && column >= i + 1 && column <= n;
    ASC_C2_CHECK(test, valid);
    if (!valid) {
      return;
    }
    std::swap(rows[static_cast<std::size_t>(i)],
              rows[static_cast<std::size_t>(row - 1)]);
    std::swap(columns[static_cast<std::size_t>(i)],
              columns[static_cast<std::size_t>(column - 1)]);
  }
  const long double tolerance =
      64 * static_cast<long double>(n) *
      std::numeric_limits<typename support::Problem<T>::Real>::epsilon();
  for (asc::extent_t j = 0; j < n; ++j) {
    for (asc::extent_t i = 0; i < n; ++i) {
      Wide value{};
      long double bound = 0;
      for (asc::extent_t k = 0; k < n; ++k) {
        Wide l{};
        if (i == k) {
          l = Wide{1, 0};
        } else if (i > k) {
          l = ToWide(factors.At(i, k));
        }
        const auto u = k <= j ? ToWide(factors.At(k, j)) : Wide{};
        value += l * u;
        bound += std::abs(l) * std::abs(u);
      }
      const auto row = rows[static_cast<std::size_t>(i)];
      const auto column = columns[static_cast<std::size_t>(j)];
      auto expected = ToWide(original.At(row, column));
      if (original.zero) {
        expected =
            row == column ? Wide{support::Problem<T>::Small(), 0} : Wide{};
      }
      ASC_C2_CHECK(test,
                   std::isfinite(value.real()) && std::isfinite(value.imag()));
      ASC_C2_CHECK(test, std::abs(value - expected) <=
                             tolerance * (bound + std::abs(expected)));
    }
  }
}

template <typename T>
void Residual(TestContext& test, const support::Problem<T>& original,
              const support::Problem<T>& computed) {
  const long double tolerance =
      64 * static_cast<long double>(original.n) *
      std::numeric_limits<typename support::Problem<T>::Real>::epsilon();
  const long double scale = computed.scale[1];
  ASC_C2_CHECK(test, std::isfinite(scale) && scale >= 0 && scale <= 1);
  for (asc::extent_t i = 0; i < original.n; ++i) {
    Wide value{};
    long double bound = 0;
    for (asc::extent_t j = 0; j < original.n; ++j) {
      auto a = ToWide(original.At(i, j));
      if (original.zero) {
        a = i == j ? Wide{support::Problem<T>::Small(), 0} : Wide{};
      }
      const auto x = ToWide(computed.rhs[static_cast<std::size_t>(j + 1)]);
      ASC_C2_CHECK(test, std::isfinite(x.real()) && std::isfinite(x.imag()));
      value += a * x;
      bound += std::abs(a) * std::abs(x);
    }
    const auto rhs =
        scale * ToWide(original.rhs[static_cast<std::size_t>(i + 1)]);
    ASC_C2_CHECK(test,
                 std::abs(value - rhs) <= tolerance * (bound + std::abs(rhs)));
  }
}

template <typename T>
void Solves(TestContext& test, const asc::ReferenceLapackProvider& provider,
            const support::Problem<T>& original, support::Problem<T>& problem,
            const asc::LapackReport& provenance, bool audit) {
  const auto factor =
      Take(Observe(test, audit, [&] { return problem.View(provenance); }));
  ASC_C2_EQ(test, factor.perturbed(), original.zero && original.n != 0);
  const auto plan = Take(Observe(test, audit, [&] {
    return asc::QueryGesc2Workspace(provider, factor,
                                    Vector(problem.rhs, problem.n),
                                    Vector(problem.scale, 1));
  }));
  const auto matrix_before = problem.dense;
  const auto rows_before = problem.rows;
  const auto columns_before = problem.columns;
  for (int reuse = 0; reuse < 2; ++reuse) {
    problem.rhs = original.rhs;
    problem.scale = original.scale;
    support::Scratch<T> scratch;
    const auto workspace = scratch.Workspace(plan);
    auto report = PriorReport();
    const auto status = Observe(test, audit, [&] {
      return asc::Gesc2(provider, factor, Vector(problem.rhs, problem.n),
                        Vector(problem.scale, 1), plan, workspace, report);
    });
    ASC_C2_CHECK(test, status.ok());
    if (!status.ok()) {
      return;
    }
    Report(test, provider, report, problem.n, false,
           factor.perturbed() || problem.scale[1] < 1);
    if (problem.n == 0) {
      ASC_C2_EQ(test, problem.scale[1], 1);
    }
    Residual(test, original, problem);
    ASC_C2_CHECK(test, support::Same(problem.dense, matrix_before));
    ASC_C2_EQ(test, problem.rows, rows_before);
    ASC_C2_EQ(test, problem.columns, columns_before);
    problem.Guards(test);
    scratch.Guards(test);
  }
}

template <typename T>
void Case(TestContext& test, const asc::ReferenceLapackProvider& provider,
          const support::Problem<T>& original, bool audit = true) {
  auto problem = original;
  const auto plan =
      Take(Observe(test, audit, [&] { return problem.Query(provider); }));
  for (int reuse = 0; reuse < 2; ++reuse) {
    problem = original;
    support::Scratch<T> scratch;
    const auto workspace = scratch.Workspace(plan);
    auto report = PriorReport();
    const auto status = Observe(test, audit, [&] {
      return problem.Factor(provider, plan, workspace, report);
    });
    ASC_C2_CHECK(test, status.ok());
    if (!status.ok()) {
      return;
    }
    Report(test, provider, report, problem.n, true,
           problem.zero && problem.n != 0);
    ASC_C2_CHECK(test, support::Same(problem.rhs, original.rhs));
    ASC_C2_CHECK(test, support::Same(problem.scale, original.scale));
    Reconstruction(test, original, problem);
    problem.Guards(test);
    scratch.Guards(test);
    Solves(test, provider, original, problem, report, audit);
  }
}

template <typename T>
void Numerical(TestContext& test) {
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  std::size_t cases = 0;
  for (auto layout : {support::kRow, support::kColumn}) {
    for (asc::extent_t n : {0, 1, 2, 3, 5, 8}) {
      for (asc::extent_t padding : {0, 1, 3}) {
        for (asc::extent_t seed = 0; seed < 12; ++seed) {
          for (int exponent : {-32, 0, 32}) {
            Case(test, provider,
                 support::Problem<T>(n, padding, layout, seed, exponent));
            ++cases;
          }
        }
      }
    }
    for (asc::extent_t n = 0; n <= 8; ++n) {
      for (asc::extent_t padding : {0, 1, 3}) {
        for (int rhs = 0; rhs < 3; ++rhs) {
          Case(test, provider,
               support::Problem<T>(n, padding, layout, 0, 0, true, rhs));
          ++cases;
        }
      }
    }
    support::Problem<T> stress(1, 2, layout, 0, 0);
    using Real = typename support::Problem<T>::Real;
    stress.At(0, 0) = Value<T>(1);
    stress.rhs[1] = Value<T>(std::numeric_limits<Real>::max(),
                             std::numeric_limits<Real>::max());
    Case(test, provider, stress);
    ++cases;
  }
  ASC_C2_EQ(test, cases, std::size_t{1460});
  std::printf("INSTALLED_COMPLETE_PIVOTING cases=%zu\n", cases);
}

}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard guard;
  TestContext test;
  Numerical<float>(test);
  Numerical<double>(test);
  Numerical<std::complex<float>>(test);
  Numerical<std::complex<double>>(test);
  return test.failures == 0 ? 0 : 1;
}
