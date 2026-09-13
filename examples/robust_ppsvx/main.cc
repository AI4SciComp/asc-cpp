// Copyright 2026 AI4SciComp contributors
// SPDX-License-Identifier: Apache-2.0

#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_driver.h"
#include "asc/dense/providers/lapack_cholesky_packed_robust.h"

namespace {
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;

template <typename T>
T Take(asc::Result<T> result) {
  if (!result.ok()) {
    std::fprintf(stderr, "Descriptor/query failed: %d\n",
                 static_cast<int>(result.status().code()));
    std::abort();
  }
  return std::move(*result);
}

class Scratch {
 public:
  explicit Scratch(const asc::LapackWorkspacePlan& plan) {
    constexpr auto kIndex =
        static_cast<std::size_t>(asc::LapackWorkspaceKind::kScratch);
    const auto& region = plan.regions[kIndex];
    const auto bytes =
        static_cast<std::size_t>(region.minimum_entries) * region.entry_bytes;
    storage_.resize(bytes + region.alignment);
    void* data = storage_.data();
    auto capacity = storage_.size();
    if (std::align(region.alignment, bytes, data, capacity) == nullptr) {
      std::abort();
    }
    workspace_.regions[kIndex] = {data, bytes, kHost};
  }
  [[nodiscard]] const asc::LapackWorkspace& workspace() const {
    return workspace_;
  }

 private:
  std::vector<std::byte> storage_;
  asc::LapackWorkspace workspace_;
};

template <typename T>
struct Fixture {
  using Real = asc::DenseBlasRealType<T>;
  std::array<T, 3> a{};
  std::array<T, 3> factors{};
  std::array<T, 2> original_b{};
  std::array<T, 2> b{};
  std::array<T, 2> x{};
  std::array<Real, 2> scales{Real{1}, Real{1}};
  std::array<Real, 1> ferr{};
  std::array<Real, 1> berr{};
  Real rcond = 0;
  asc::LapackCholeskyEquilibration equed =
      asc::LapackCholeskyEquilibration::kNone;

  explicit Fixture(asc::DenseBlasTriangle triangle) {
    // Strictly diagonally dominant Hermitian positive definite A. Construct B
    // from the independent solution (1, 2); no solver-derived oracle.
    T off_diagonal{0.5};
    if constexpr (asc::DenseBlasComplex<T>) {
      off_diagonal.imag(Real{0.25});
    }
    T conjugate = off_diagonal;
    if constexpr (asc::DenseBlasComplex<T>) {
      conjugate = std::conj(off_diagonal);
    }
    const bool upper = triangle == asc::DenseBlasTriangle::kUpper;
    a = {T{4}, upper ? off_diagonal : conjugate, T{1024}};
    original_b = {T{4} + Real{2} * off_diagonal, conjugate + T{2048}};
    b = original_b;
  }

  struct Views {
    asc::DenseBlasPackedMatrixView<T> a;
    asc::DenseBlasPackedMatrixView<T> factors;
    asc::DenseBlasMatrixView<T> b;
    asc::DenseBlasMatrixView<T> x;
    asc::DenseBlasVectorView<Real> s;
    asc::DenseBlasVectorView<Real> ferr;
    asc::DenseBlasVectorView<Real> berr;
  };

  Views MakeViews() {
    return {Take(asc::DenseBlasPackedMatrixView<T>::Create(
                a.data(), 2, kRow, {a.data(), sizeof(a), kHost})),
            Take(asc::DenseBlasPackedMatrixView<T>::Create(
                factors.data(), 2, kColumn,
                {factors.data(), sizeof(factors), kHost})),
            Take(asc::DenseBlasMatrixView<T>::Create(
                b.data(), 2, 1, kRow, 1, {b.data(), sizeof(b), kHost})),
            Take(asc::DenseBlasMatrixView<T>::Create(
                x.data(), 2, 1, kColumn, 2, {x.data(), sizeof(x), kHost})),
            Take(asc::DenseBlasVectorView<Real>::Create(
                scales.data(), 2, 1, {scales.data(), sizeof(scales), kHost})),
            Take(asc::DenseBlasVectorView<Real>::Create(
                ferr.data(), 1, 1, {ferr.data(), sizeof(ferr), kHost})),
            Take(asc::DenseBlasVectorView<Real>::Create(
                berr.data(), 1, 1, {berr.data(), sizeof(berr), kHost}))};
  }

  [[nodiscard]] bool Check(Real multiple, bool produced_factor,
                           const asc::LapackReport& report) const {
    const Real tolerance = 256 * std::numeric_limits<Real>::epsilon();
    return !report.called_provider && !report.native_info &&
           report.factor_family.has_value() == produced_factor &&
           std::isfinite(rcond) && rcond > 0 && rcond <= 1 &&
           std::isfinite(ferr[0]) && ferr[0] >= 0 && std::isfinite(berr[0]) &&
           berr[0] >= 0 && berr[0] <= tolerance &&
           std::abs(x[0] - T{multiple}) <= tolerance * multiple &&
           std::abs(x[1] - T{2 * multiple}) <= tolerance * 2 * multiple;
  }
};

template <typename T>
bool Solve(const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasTriangle triangle, bool equilibrate) {
  using Real = asc::DenseBlasRealType<T>;
  Fixture<T> fixture(triangle);
  const auto v = fixture.MakeViews();
  auto plan =
      Take(equilibrate ? asc::QueryRobustPpsvxEquilibratedWorkspace(
                             provider, triangle, v.a, v.factors, fixture.equed,
                             v.s, v.b, v.x, v.ferr, v.berr, fixture.rcond)
                       : asc::QueryRobustPpsvxWorkspace(
                             provider, triangle, v.a, v.factors, v.b, v.x,
                             v.ferr, v.berr, fixture.rcond));
  Scratch scratch(plan);
  asc::LapackReport report;
  const auto status =
      equilibrate ? asc::RobustPpsvxEquilibrated(
                        provider, triangle, v.a, v.factors, fixture.equed, v.s,
                        v.b, v.x, v.ferr, v.berr, fixture.rcond, plan,
                        scratch.workspace(), report)
                  : asc::RobustPpsvx(provider, triangle, v.a, v.factors, v.b,
                                     v.x, v.ferr, v.berr, fixture.rcond, plan,
                                     scratch.workspace(), report);
  if (!status.ok() || !fixture.Check(Real{1}, true, report) ||
      (equilibrate &&
       fixture.equed != asc::LapackCholeskyEquilibration::kDiagonal)) {
    return false;
  }
  const auto saved_factors = fixture.factors;
  // E publishes D*A*D and its factors. Reuse those arrays and S with a new
  // original-system RHS: the API applies D to that RHS exactly once.
  for (std::size_t i = 0; i < fixture.b.size(); ++i) {
    fixture.b[i] = Real{2} * fixture.original_b[i];
  }
  plan = Take(asc::QueryRobustPpsvxFactoredWorkspace(
      provider, triangle, v.a, v.factors, fixture.equed, v.s, v.b, v.x, v.ferr,
      v.berr, fixture.rcond));
  Scratch reused_scratch(plan);
  const auto reused_status = asc::RobustPpsvxFactored(
      provider, triangle, v.a, v.factors, fixture.equed, v.s, v.b, v.x, v.ferr,
      v.berr, fixture.rcond, plan, reused_scratch.workspace(), report);
  // FACT F consumes a factor; it does not authorize a newly produced factor.
  return reused_status.ok() && fixture.Check(Real{2}, false, report) &&
         fixture.factors == saved_factors;
}

template <typename T>
bool Scalars(const asc::ReferenceLapackProvider& provider) {
  for (const auto triangle :
       {asc::DenseBlasTriangle::kUpper, asc::DenseBlasTriangle::kLower}) {
    for (const bool equilibrate : {false, true}) {
      if (!Solve<T>(provider, triangle, equilibrate)) {
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
  const bool passed = Scalars<float>(provider) && Scalars<double>(provider) &&
                      Scalars<std::complex<float>>(provider) &&
                      Scalars<std::complex<double>>(provider);
  std::puts(passed
                ? "Experimental robust PPSVX: four scalars, N/E/F reuse passed"
                : "Experimental robust PPSVX example failed");
  return passed ? 0 : 1;
}
