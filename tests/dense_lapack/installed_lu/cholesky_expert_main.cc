#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <initializer_list>
#include <limits>

#include "asc/core/execution.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_condition.h"
#include "asc/dense/providers/lapack_cholesky_driver.h"
#include "asc/dense/providers/lapack_cholesky_equilibration.h"
#include "asc/dense/providers/lapack_cholesky_refinement.h"
#include "factorization_support.h"
#include "normal_return_guard.h"

namespace {
using installed_internal::kColumn;
using installed_internal::kHost;
using installed_internal::kRow;
using installed_internal::Matrix;
using installed_internal::Near;
using installed_internal::Scratch;
using installed_internal::Succeeded;
using installed_internal::Take;
using installed_internal::Value;
using installed_internal::Vector;
using installed_internal::Wide;
using installed_internal::Widen;
using Triangle = asc::DenseBlasTriangle;
using Equed = asc::LapackCholeskyEquilibration;

template <typename T>
struct Case {
  using Real = asc::DenseBlasRealType<T>;
  Matrix<T, 2, 2> a;
  Matrix<T, 2, 2> af;
  Matrix<T, 2, 2> b;
  Matrix<T, 2, 2> x;
  std::array<Real, 2> ferr{71, 72};
  std::array<Real, 2> berr{73, 74};
  std::array<Real, 2> scales{75, 76};
  std::array<Real, 16> real_work{};
  Real rcond = 77;
  Scratch<T> scratch;

  Case(asc::DenseBlasLayout layout, asc::DenseBlasLayout rhs_layout, Real scale)
      : a{{}, layout}, af{{}, rhs_layout}, b{{}, rhs_layout}, x{{}, layout} {
    a.data.fill(Value<T>(-91));
    af.data.fill(Value<T>(-92));
    b.data.fill(Value<T>(-93));
    x.data.fill(Value<T>(-94));
    for (std::size_t row = 0; row < 2; ++row) {
      for (std::size_t column = 0; column < 2; ++column) {
        a.At(row, column) =
            row == column ? Value<T>(row == 0 ? 4 : 16) * scale : T{};
        b.At(row, column) = Value<T>(row == 0 ? 4 : 16) * scale *
                            Value<T>(static_cast<double>(1 + row + column));
      }
    }
    scratch.workspace
        .regions[static_cast<std::size_t>(asc::LapackWorkspaceKind::kReal)] = {
        real_work.data(), sizeof(real_work), kHost};
  }

  [[nodiscard]] bool Solution(const Matrix<T, 2, 2>& original_a,
                              const Matrix<T, 2, 2>& original_b) const {
    for (std::size_t row = 0; row < 2; ++row) {
      for (std::size_t column = 0; column < 2; ++column) {
        Wide residual = -Widen(original_b.At(row, column));
        long double scale = std::abs(residual);
        for (std::size_t inner = 0; inner < 2; ++inner) {
          const auto product =
              Widen(original_a.At(row, inner)) * Widen(x.At(inner, column));
          residual += product;
          scale += std::abs(product);
        }
        if (!Near<T>(residual, 0, scale) ||
            !Near<T>(Widen(x.At(row, column)), 1 + row + column, 4)) {
          return false;
        }
      }
    }
    return std::isfinite(rcond) && rcond > 0 && rcond <= 1 &&
           std::isfinite(ferr[0]) && std::isfinite(ferr[1]) && ferr[0] >= 0 &&
           ferr[1] >= 0 && std::isfinite(berr[0]) && std::isfinite(berr[1]) &&
           berr[0] >= 0 && berr[1] >= 0;
  }
};

template <typename T>
bool NewAndRefined(const asc::ReferenceLapackProvider& provider,
                   Triangle triangle, asc::DenseBlasLayout layout,
                   asc::DenseBlasLayout rhs_layout) {
  using Real = asc::DenseBlasRealType<T>;
  Case<T> c(layout, rhs_layout, Real{1});
  const auto original_a = c.a;
  const auto original_b = c.b;
  const auto original_af = c.af.data;
  const auto original_x = c.x.data;
  asc::LapackReport report;
  const auto plan = asc::QueryPosvxWorkspace(
      provider, triangle, c.a.ConstView(), c.af.View(), c.b.ConstView(),
      c.x.View(), Vector(c.ferr), Vector(c.berr), c.rcond);
  if (!plan.ok() ||
      !Succeeded(asc::Posvx(provider, triangle, c.a.ConstView(), c.af.View(),
                            c.b.ConstView(), c.x.View(), Vector(c.ferr),
                            Vector(c.berr), c.rcond, *plan, c.scratch.workspace,
                            report),
                 report) ||
      !c.Solution(original_a, original_b) ||
      !Near<T>(Widen(c.af.At(0, 0)), 2, 2) ||
      !Near<T>(Widen(c.af.At(1, 1)), 4, 4)) {
    return false;
  }
  const auto factor = c.af.data;
  const auto condition = asc::QueryPoconWorkspace(
      provider, triangle, c.af.ConstView(), Real{16}, c.rcond);
  if (!condition.ok() ||
      !Succeeded(asc::Pocon(provider, triangle, c.af.ConstView(), Real{16},
                            c.rcond, *condition, c.scratch.workspace, report),
                 report) ||
      !Near<T>(Wide{c.rcond, 0}, 0.25L, 1)) {
    return false;
  }
  for (std::size_t row = 0; row < 2; ++row) {
    for (std::size_t column = 0; column < 2; ++column) {
      c.x.At(row, column) += Value<T>(0.125);
    }
  }
  const auto refinement = asc::QueryPorfsWorkspace(
      provider, triangle, c.a.ConstView(), c.af.ConstView(), c.b.ConstView(),
      c.x.View(), Vector(c.ferr), Vector(c.berr));
  if (!refinement.ok() ||
      !Succeeded(
          asc::Porfs(provider, triangle, c.a.ConstView(), c.af.ConstView(),
                     c.b.ConstView(), c.x.View(), Vector(c.ferr),
                     Vector(c.berr), *refinement, c.scratch.workspace, report),
          report)) {
    return false;
  }
  return c.Solution(original_a, original_b) && c.a.data == original_a.data &&
         c.b.data == original_b.data && c.af.data == factor &&
         c.af.PaddingEquals(original_af) && c.x.PaddingEquals(original_x);
}

template <typename T>
bool Equilibrated(const asc::ReferenceLapackProvider& provider,
                  Triangle triangle, asc::DenseBlasLayout layout,
                  asc::DenseBlasLayout rhs_layout) {
  using Real = asc::DenseBlasRealType<T>;
  Case<T> c(layout, rhs_layout, std::numeric_limits<Real>::min() * Real{64});
  const auto original_a = c.a;
  const auto original_b = c.b;
  const auto old_x = c.x.data;
  Equed equed = Equed::kNone;
  asc::LapackReport report;
  const auto plan = asc::QueryPosvxEquilibratedWorkspace(
      provider, triangle, c.a.View(), c.af.View(), equed, Vector(c.scales),
      c.b.View(), c.x.View(), Vector(c.ferr), Vector(c.berr), c.rcond);
  if (!plan.ok() ||
      !Succeeded(
          asc::PosvxEquilibrated(provider, triangle, c.a.View(), c.af.View(),
                                 equed, Vector(c.scales), c.b.View(),
                                 c.x.View(), Vector(c.ferr), Vector(c.berr),
                                 c.rcond, *plan, c.scratch.workspace, report),
          report) ||
      equed != Equed::kDiagonal || !c.Solution(original_a, original_b)) {
    return false;
  }
  for (std::size_t row = 0; row < 2; ++row) {
    const auto scale = static_cast<long double>(c.scales[row]);
    if (!Near<T>(Widen(c.a.At(row, row)),
                 Widen(original_a.At(row, row)) * scale * scale, 4)) {
      return false;
    }
    for (std::size_t column = 0; column < 2; ++column) {
      if (!Near<T>(Widen(c.b.At(row, column)),
                   Widen(original_b.At(row, column)) * scale,
                   std::abs(Widen(c.b.At(row, column))))) {
        return false;
      }
    }
  }
  c.b = original_b;
  const auto scaled_a = c.a.data;
  const auto factor = c.af.data;
  const auto scales = c.scales;
  const auto supplied = asc::QueryPosvxFactoredWorkspace(
      provider, triangle, c.a.ConstView(), c.af.ConstView(), equed,
      Vector(c.scales), c.b.View(), c.x.View(), Vector(c.ferr), Vector(c.berr),
      c.rcond);
  if (!supplied.ok() ||
      !Succeeded(asc::PosvxFactored(provider, triangle, c.a.ConstView(),
                                    c.af.ConstView(), equed, Vector(c.scales),
                                    c.b.View(), c.x.View(), Vector(c.ferr),
                                    Vector(c.berr), c.rcond, *supplied,
                                    c.scratch.workspace, report),
                 report)) {
    return false;
  }
  return !report.factor_family.has_value() && c.scales == scales &&
         c.a.data == scaled_a && c.af.data == factor &&
         c.Solution(original_a, original_b) && c.x.PaddingEquals(old_x) &&
         c.a.PaddingEquals(original_a.data) &&
         c.b.PaddingEquals(original_b.data);
}

template <typename T>
bool Scales(const asc::ReferenceLapackProvider& provider,
            asc::DenseBlasLayout layout, bool radix) {
  using Real = asc::DenseBlasRealType<T>;
  Case<T> c(layout, layout, Real{1});
  const auto original = c.a.data;
  Real scond = 81;
  Real amax = 82;
  const auto plan =
      radix ? asc::QueryPoequbWorkspace(provider, c.a.ConstView(),
                                        Vector(c.scales), scond, amax)
            : asc::QueryPoequWorkspace(provider, c.a.ConstView(),
                                       Vector(c.scales), scond, amax);
  if (!plan.ok()) {
    return false;
  }
  asc::LapackReport report;
  const auto status =
      radix ? asc::Poequb(provider, c.a.ConstView(), Vector(c.scales), scond,
                          amax, *plan, c.scratch.workspace, report)
            : asc::Poequ(provider, c.a.ConstView(), Vector(c.scales), scond,
                         amax, *plan, c.scratch.workspace, report);
  return Succeeded(status, report) && c.scales[0] == Real{0.5} &&
         c.scales[1] == Real{0.25} && scond == Real{0.5} && amax == Real{16} &&
         c.a.data == original;
}

template <typename T>
bool All(const asc::ReferenceLapackProvider& provider) {
  for (const auto layout : {kColumn, kRow}) {
    for (const bool radix : {false, true}) {
      if (!Scales<T>(provider, layout, radix)) {
        return false;
      }
    }
    for (const auto rhs_layout : {kColumn, kRow}) {
      for (const auto triangle : {Triangle::kUpper, Triangle::kLower}) {
        if (!NewAndRefined<T>(provider, triangle, layout, rhs_layout) ||
            !Equilibrated<T>(provider, triangle, layout, rhs_layout)) {
          return false;
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
  if (!All<float>(provider) || !All<double>(provider) ||
      !All<std::complex<float>>(provider) ||
      !All<std::complex<double>>(provider)) {
    std::fprintf(stderr, "Installed Cholesky expert consumer failed.\n");
    return 1;
  }
  std::puts(
      "Installed POCON/PORFS/POSVX/POEQU/POEQUB: all four scalars, both "
      "triangles/layouts and N/E/F driver modes passed.");
  return 0;
}
