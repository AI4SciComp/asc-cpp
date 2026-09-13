#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_positive_tridiagonal.h"
#include "asc/dense/providers/lapack_positive_tridiagonal_expert.h"
#include "normal_return_guard.h"

namespace {
constexpr auto kHost = asc::MemorySpace::kHost;
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
T Conjugate(T value) {
  if constexpr (asc::DenseBlasComplex<T>) {
    return std::conj(value);
  } else {
    return value;
  }
}
template <typename T, std::size_t N>
auto Vector(std::array<T, N>& array) {
  return Take(asc::DenseBlasVectorView<T>::Create(
      array.data(), N, 1, {array.data(), sizeof(array), kHost}));
}
template <typename T>
struct Expert {
  using Real = asc::DenseBlasRealType<T>;
  std::array<Real, 2> d{};
  std::array<T, 1> e{};
  std::array<T, 8> b{}, x{};
  std::array<Real, 2> ferr{}, berr{}, real{};
  std::array<T, 4> scalar{};
  std::array<T, 9> packed{};
  std::array<Real, 4> estimates{};
  asc::DenseBlasLayout b_layout, x_layout;
  Expert(const std::array<T, 4>& matrix, asc::DenseBlasLayout b_format,
         asc::DenseBlasLayout x_format)
      : b_layout(b_format), x_layout(x_format) {
    d = {static_cast<Real>(std::real(matrix[0])),
         static_cast<Real>(std::real(matrix[3]))};
    e[0] = matrix[2];
  }
  static std::size_t Offset(int i, int j, asc::DenseBlasLayout layout) {
    return static_cast<std::size_t>(
        layout == asc::DenseBlasLayout::kRowMajor ? 4 * i + j : 4 * j + i);
  }
  static T Exact(int i, int j, int pass) {
    return Value<T>(1 + i + 2 * j + pass, 0.5 + i - j);
  }
  void Initialize(const std::array<T, 4>& matrix, int pass) {
    b.fill(Value<T>(-77));
    x.fill(Value<T>(-79));
    ferr.fill(Real{-81});
    berr.fill(Real{-83});
    for (int i = 0; i < 2; ++i) {
      for (int j = 0; j < 2; ++j) {
        T value{};
        for (int k = 0; k < 2; ++k) {
          value += matrix[2 * static_cast<std::size_t>(i) +
                          static_cast<std::size_t>(k)] *
                   Exact(k, j, pass);
        }
        b[Offset(i, j, b_layout)] = value;
        x[Offset(i, j, x_layout)] =
            Value<T>(std::numeric_limits<Real>::quiet_NaN());
      }
    }
  }
  [[nodiscard]] bool Check(int pass) const {
    const Real epsilon = std::numeric_limits<Real>::epsilon();
    for (int j = 0; j < 2; ++j) {
      for (int i = 0; i < 2; ++i) {
        if (std::abs(x[Offset(i, j, x_layout)] - Exact(i, j, pass)) >
            128 * epsilon * std::abs(Exact(i, j, pass))) {
          return false;
        }
      }
      const auto at = static_cast<std::size_t>(j);
      if (!std::isfinite(ferr[at]) || ferr[at] < 0 ||
          ferr[at] > 256 * epsilon || !std::isfinite(berr[at]) ||
          berr[at] < 0 || berr[at] > 128 * epsilon) {
        return false;
      }
    }
    return std::ranges::all_of(std::array{2U, 3U, 6U, 7U}, [&](auto i) {
      return b[i] == Value<T>(-77) && x[i] == Value<T>(-79);
    });
  }
};
template <typename T, std::size_t N>
bool Region(const asc::LapackWorkspacePlan& plan,
            asc::LapackWorkspace& workspace, asc::LapackWorkspaceKind kind,
            std::array<T, N>& storage) {
  const auto role = static_cast<std::size_t>(kind);
  const auto& requirement = plan.regions[role];
  if (requirement.minimum_entries == 0) {
    return true;
  }
  if (requirement.entry_bytes == 0 ||
      static_cast<std::size_t>(requirement.minimum_entries) >
          sizeof(storage) / requirement.entry_bytes) {
    return false;
  }
  workspace.regions[role] = {
      storage.data(),
      static_cast<std::size_t>(requirement.minimum_entries) *
          requirement.entry_bytes,
      kHost};
  return true;
}
template <typename T, typename F>
bool SolveTwice(const asc::ReferenceLapackProvider& provider, F factor,
                const std::array<T, 4>& matrix, asc::DenseBlasLayout b_layout,
                asc::DenseBlasLayout x_layout) {
  Expert<T> data(matrix, b_layout, x_layout);
  const auto original =
      Take(asc::LapackPositiveDefiniteTridiagonalView<const T>::Create(
          Vector(data.d), Vector(data.e)));
  const auto rhs = Take(asc::DenseBlasMatrixView<const T>::Create(
      data.b.data(), 2, 2, b_layout, 4,
      {data.b.data(), sizeof(data.b), kHost}));
  const auto solution = Take(asc::DenseBlasMatrixView<T>::Create(
      data.x.data(), 2, 2, x_layout, 4,
      {data.x.data(), sizeof(data.x), kHost}));
  const auto ferr = Vector(data.ferr);
  const auto berr = Vector(data.berr);
  asc::DenseBlasRealType<T> condition = -7;
  const auto plan = Take(asc::QueryPtsvxWorkspace(
      provider, original, factor, rhs, solution, condition, ferr, berr));
  asc::LapackWorkspace workspace;
  using Kind = asc::LapackWorkspaceKind;
  if (!Region(plan, workspace, Kind::kScalar, data.scalar) ||
      !Region(plan, workspace, Kind::kReal, data.real) ||
      !Region(plan, workspace, Kind::kLayoutConversion, data.packed) ||
      !Region(plan, workspace, Kind::kScratch, data.estimates)) {
    return false;
  }
  for (int pass = 0; pass < 2; ++pass) {
    data.Initialize(matrix, pass);
    const auto b_before = data.b;
    asc::LapackReport report;
    if (!asc::Ptsvx(provider, original, factor, rhs, solution, condition, ferr,
                    berr, plan, workspace, report)
             .ok() ||
        !report.called_provider || report.native_info != 0 ||
        report.output_validity != asc::LapackOutputValidity::kComplete ||
        report.factor_family.has_value() || !std::isfinite(condition) ||
        condition <= 0 || !data.Check(pass) || data.b != b_before) {
      return false;
    }
  }
  return true;
}
template <typename T>
bool Reuse(const asc::ReferenceLapackProvider& provider,
           asc::ReferencePositiveDefiniteTridiagonalFactorView<T> factor,
           const std::array<T, 4>& matrix) {
  for (const auto b :
       {asc::DenseBlasLayout::kColumnMajor, asc::DenseBlasLayout::kRowMajor}) {
    for (const auto x : {asc::DenseBlasLayout::kColumnMajor,
                         asc::DenseBlasLayout::kRowMajor}) {
      if (!SolveTwice<T>(provider, factor, matrix, b, x)) {
        return false;
      }
    }
  }
  return true;
}
template <typename T>
bool Run(const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  // Independently constructed ordinary Hermitian matrix. Factors are obtained
  // through FACT=N, then reused through FACT=F with independent right-hand
  // sides.
  std::array<Real, 2> d{4, 5};
  std::array<T, 1> e{Value<T>(0.5, 0.25)};
  const std::array<T, 4> original{T{4}, Conjugate(e[0]), e[0], T{5}};
  const auto matrix =
      Take(asc::LapackPositiveDefiniteTridiagonalView<T>::Create(Vector(d),
                                                                 Vector(e)));
  // Typed mutable factors select FACT=N; old output numbers are ignored.
  d.fill(std::numeric_limits<Real>::quiet_NaN());
  e.fill(Value<T>(std::numeric_limits<Real>::quiet_NaN()));
  if (!SolveTwice<T>(provider, matrix, original,
                     asc::DenseBlasLayout::kRowMajor,
                     asc::DenseBlasLayout::kColumnMajor)) {
    return false;
  }
  const auto lower =
      Take(asc::ReferencePositiveDefiniteTridiagonalFactorView<T>::FromRaw(
          provider, asc::DenseBlasTriangle::kLower, Vector(d), Vector(e)));
  if (!Reuse(provider, lower, original)) {
    return false;
  }
  asc::HostMemoryResource resource;
  auto upper =
      Take(asc::ReferencePositiveDefiniteTridiagonalFactor<T>::CopyFrom(
          provider, lower, asc::DenseBlasTriangle::kUpper, resource));
  d.fill(-1);
  e.fill(Value<T>(99, -99));
  if (!Reuse(provider, Take(upper.view(provider)), original)) {
    return false;
  }
  std::printf(
      "PTSVX scalar_bytes=%zu: known solutions and finite estimates, borrowed "
      "lower/owned upper reuse, independent padded B/X layouts\n",
      sizeof(T));
  return true;
}
}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard normal_return;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  return Run<float>(provider) && Run<double>(provider) &&
                 Run<std::complex<float>>(provider) &&
                 Run<std::complex<double>>(provider)
             ? 0
             : 1;
}
