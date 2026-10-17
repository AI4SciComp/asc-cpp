#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

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
#include "asc/dense/providers/lapack_indefinite_condition.h"
#include "asc/dense/providers/lapack_indefinite_driver.h"
#include "asc/dense/providers/lapack_indefinite_refinement.h"
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
constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;
constexpr auto kLower = asc::DenseBlasTriangle::kLower;
constexpr auto kFamily = asc::LapackFactorFamily::kBunchKaufman;
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

template <typename T, bool Hermitian>
struct Case {
  using Real = asc::DenseBlasRealType<T>;
  asc::DenseBlasTriangle triangle;
  Matrix<T, 3, 3> a;
  Matrix<T, 3, 3> af;
  Matrix<T, 3, 2> b;
  Matrix<T, 3, 2> x;
  std::array<asc::index_t, 5> pivots{-109, -109, -109, -109, -109};
  std::array<Real, 4> ferr{-113, -113, -113, -113};
  std::array<Real, 4> berr{-127, -127, -127, -127};
  Real rcond = -131;

  static T Original(std::size_t i, std::size_t j) {
    // An independent 2-by-2 indefinite D block and a negative scalar block.
    // Complex SY uses the same off-diagonal; HE uses its conjugate below.
    if (i == 2 && j == 2) {
      return T{-4};
    }
    if (i < 2 && j < 2 && i != j) {
      const auto off = Value<T>(2, 0.5);
      return Hermitian && i > j ? Conjugate(off) : off;
    }
    return T{};
  }

  static T Expected(std::size_t i, std::size_t j) {
    return Value<T>(static_cast<double>(1 + i + j),
                    static_cast<double>(1 + 2 * i + j) / 4);
  }

  Case(asc::DenseBlasTriangle tri,
       const std::array<asc::DenseBlasLayout, 4>& layouts)
      : triangle(tri),
        a{{}, layouts[0]},
        af{{}, layouts[1]},
        b{{}, layouts[2]},
        x{{}, layouts[3]} {
    a.data.fill(Value<T>(-137));
    af.data.fill(Value<T>(-139));
    b.data.fill(Value<T>(-149));
    x.data.fill(Value<T>(-151));
    for (std::size_t i = 0; i < 3; ++i) {
      for (std::size_t j = 0; j < 3; ++j) {
        if (Selected(i, j)) {
          a.At(i, j) = Original(i, j);
          if constexpr (Hermitian) {
            if (i == j) {
              a.At(i, j).imag(77);  // Ignored original component.
            }
          }
          af.At(i, j) = a.At(i, j);
        }
      }
      for (std::size_t j = 0; j < 2; ++j) {
        T value{};
        for (std::size_t k = 0; k < 3; ++k) {
          value += Original(i, k) * Expected(k, j);
        }
        b.At(i, j) = value;
        x.At(i, j) = value;
      }
    }
  }

  [[nodiscard]] bool Selected(std::size_t i, std::size_t j) const {
    return triangle == kUpper ? i <= j : i >= j;
  }

  auto PivotView() {
    return Take(asc::DenseBlasVectorView<asc::index_t>::Create(
        pivots.data() + 1, 3, 1, {pivots.data(), sizeof(pivots), kHost}));
  }
  auto Raw() {
    return Take(asc::RawLapackPivotView::Create(
        pivots.data() + 1, 3, kFamily, {pivots.data(), sizeof(pivots), kHost}));
  }
  static auto ErrorView(std::array<Real, 4>& values) {
    return Take(asc::DenseBlasVectorView<Real>::Create(
        values.data() + 1, 2, 1, {values.data(), sizeof(values), kHost}));
  }

  [[nodiscard]] bool Solution() const {
    for (std::size_t i = 0; i < 3; ++i) {
      for (std::size_t j = 0; j < 2; ++j) {
        Wide residual = -Widen(b.At(i, j));
        long double scale = std::abs(residual);
        for (std::size_t k = 0; k < 3; ++k) {
          const auto product = Widen(Original(i, k)) * Widen(x.At(k, j));
          residual += product;
          scale += std::abs(product);
        }
        if (!Near<T>(residual, 0, scale) ||
            !Near<T>(Widen(x.At(i, j)), Widen(Expected(i, j)), 8)) {
          return false;
        }
      }
    }
    return true;
  }

  [[nodiscard]] bool Factor() const {
    const asc::index_t paired = triangle == kUpper ? -1 : -2;
    if (pivots[1] != paired || pivots[2] != paired || pivots[3] != 3) {
      return false;
    }
    // Here U/L is identity, so the published selected D coefficients alone
    // reconstruct the original full matrix without a provider-based oracle.
    for (std::size_t i = 0; i < 3; ++i) {
      for (std::size_t j = 0; j < 3; ++j) {
        if (Selected(i, j) && af.At(i, j) != Original(i, j)) {
          return false;
        }
      }
    }
    return true;
  }

  [[nodiscard]] bool Diagnostics() const {
    const long double expected = std::abs(Widen(Value<T>(2, 0.5))) / 4;
    if (!Near<T>(Wide{rcond, 0}, expected, 1)) {
      return false;
    }
    for (std::size_t j = 1; j <= 2; ++j) {
      if (!std::isfinite(ferr[j]) || ferr[j] < 0 || !std::isfinite(berr[j]) ||
          berr[j] < 0) {
        return false;
      }
    }
    return true;
  }

  [[nodiscard]] bool Guards(const Case& before) const {
    if (a.data != before.a.data || b.data != before.b.data ||
        !af.PaddingEquals(before.af.data) || !x.PaddingEquals(before.x.data) ||
        pivots.front() != -109 || pivots.back() != -109 ||
        ferr.front() != -113 || ferr.back() != -113 || berr.front() != -127 ||
        berr.back() != -127) {
      return false;
    }
    for (std::size_t i = 0; i < 3; ++i) {
      for (std::size_t j = 0; j < 3; ++j) {
        if (!Selected(i, j) && af.At(i, j) != before.af.At(i, j)) {
          return false;
        }
      }
    }
    return true;
  }
};

template <typename T, bool Hermitian>
bool Direct(const asc::ReferenceLapackProvider& provider,
            Case<T, Hermitian>& c) {
  auto query = [&] {
    if constexpr (Hermitian) {
      return asc::QueryHesvWorkspace(provider, c.triangle, c.af.View(),
                                     c.PivotView(), c.x.View());
    } else {
      return asc::QuerySysvWorkspace(provider, c.triangle, c.af.View(),
                                     c.PivotView(), c.x.View());
    }
  };
  const auto plan = Take(query());
  Workspace<T> work(plan);
  asc::LapackReport report;
  auto call = [&] {
    if constexpr (Hermitian) {
      return asc::Hesv(provider, c.triangle, c.af.View(), c.PivotView(),
                       c.x.View(), plan, work.storage.workspace, report);
    } else {
      return asc::Sysv(provider, c.triangle, c.af.View(), c.PivotView(),
                       c.x.View(), plan, work.storage.workspace, report);
    }
  };
  return Succeeded(call(), report) && work.Guards();
}

template <typename T, bool Hermitian>
bool Condition(const asc::ReferenceLapackProvider& provider,
               Case<T, Hermitian>& c) {
  using Real = asc::DenseBlasRealType<T>;
  auto query = [&] {
    if constexpr (Hermitian) {
      return asc::QueryHeconWorkspace(provider, c.triangle, c.af.ConstView(),
                                      c.Raw(), Real{4}, c.rcond);
    } else {
      return asc::QuerySyconWorkspace(provider, c.triangle, c.af.ConstView(),
                                      c.Raw(), Real{4}, c.rcond);
    }
  };
  const auto plan = Take(query());
  Workspace<T> work(plan);
  asc::LapackReport report;
  auto call = [&] {
    if constexpr (Hermitian) {
      return asc::Hecon(provider, c.triangle, c.af.ConstView(), c.Raw(),
                        Real{4}, c.rcond, plan, work.storage.workspace, report);
    } else {
      return asc::Sycon(provider, c.triangle, c.af.ConstView(), c.Raw(),
                        Real{4}, c.rcond, plan, work.storage.workspace, report);
    }
  };
  return Succeeded(call(), report) && work.Guards();
}

template <typename T, bool Hermitian>
bool Refine(const asc::ReferenceLapackProvider& provider,
            Case<T, Hermitian>& c) {
  const auto ferr = c.ErrorView(c.ferr);
  const auto berr = c.ErrorView(c.berr);
  auto query = [&] {
    if constexpr (Hermitian) {
      return asc::QueryHerfsWorkspace(provider, c.triangle, c.a.ConstView(),
                                      c.af.ConstView(), c.Raw(),
                                      c.b.ConstView(), c.x.View(), ferr, berr);
    } else {
      return asc::QuerySyrfsWorkspace(provider, c.triangle, c.a.ConstView(),
                                      c.af.ConstView(), c.Raw(),
                                      c.b.ConstView(), c.x.View(), ferr, berr);
    }
  };
  const auto plan = Take(query());
  Workspace<T> work(plan);
  asc::LapackReport report;
  auto call = [&] {
    if constexpr (Hermitian) {
      return asc::Herfs(provider, c.triangle, c.a.ConstView(), c.af.ConstView(),
                        c.Raw(), c.b.ConstView(), c.x.View(), ferr, berr, plan,
                        work.storage.workspace, report);
    } else {
      return asc::Syrfs(provider, c.triangle, c.a.ConstView(), c.af.ConstView(),
                        c.Raw(), c.b.ConstView(), c.x.View(), ferr, berr, plan,
                        work.storage.workspace, report);
    }
  };
  return Succeeded(call(), report) && work.Guards();
}

template <typename T, bool Hermitian>
auto QueryExpert(const asc::ReferenceLapackProvider& provider,
                 Case<T, Hermitian>& c, bool factored) {
  const auto ferr = c.ErrorView(c.ferr);
  const auto berr = c.ErrorView(c.berr);
  if constexpr (Hermitian) {
    return factored
               ? asc::QueryHesvxFactoredWorkspace(
                     provider, c.triangle, c.a.ConstView(), c.af.ConstView(),
                     c.Raw(), c.b.ConstView(), c.x.View(), c.rcond, ferr, berr)
               : asc::QueryHesvxWorkspace(provider, c.triangle, c.a.ConstView(),
                                          c.af.View(), c.PivotView(),
                                          c.b.ConstView(), c.x.View(), c.rcond,
                                          ferr, berr);
  } else {
    return factored
               ? asc::QuerySysvxFactoredWorkspace(
                     provider, c.triangle, c.a.ConstView(), c.af.ConstView(),
                     c.Raw(), c.b.ConstView(), c.x.View(), c.rcond, ferr, berr)
               : asc::QuerySysvxWorkspace(provider, c.triangle, c.a.ConstView(),
                                          c.af.View(), c.PivotView(),
                                          c.b.ConstView(), c.x.View(), c.rcond,
                                          ferr, berr);
  }
}

template <typename T, bool Hermitian>
bool Expert(const asc::ReferenceLapackProvider& provider, Case<T, Hermitian>& c,
            bool factored, bool reject = false) {
  const auto before = c;
  const auto plan = Take(QueryExpert(provider, c, factored));
  Workspace<T> work(plan);
  if (reject) {
    auto& region = work.storage.workspace.regions[kInteger];
    region = {region.data(), 1, kHost};
  }
  const auto ferr = c.ErrorView(c.ferr);
  const auto berr = c.ErrorView(c.berr);
  asc::LapackReport report;
  auto call = [&] {
    if constexpr (Hermitian) {
      return factored
                 ? asc::HesvxFactored(
                       provider, c.triangle, c.a.ConstView(), c.af.ConstView(),
                       c.Raw(), c.b.ConstView(), c.x.View(), c.rcond, ferr,
                       berr, plan, work.storage.workspace, report)
                 : asc::Hesvx(provider, c.triangle, c.a.ConstView(),
                              c.af.View(), c.PivotView(), c.b.ConstView(),
                              c.x.View(), c.rcond, ferr, berr, plan,
                              work.storage.workspace, report);
    } else {
      return factored
                 ? asc::SysvxFactored(
                       provider, c.triangle, c.a.ConstView(), c.af.ConstView(),
                       c.Raw(), c.b.ConstView(), c.x.View(), c.rcond, ferr,
                       berr, plan, work.storage.workspace, report)
                 : asc::Sysvx(provider, c.triangle, c.a.ConstView(),
                              c.af.View(), c.PivotView(), c.b.ConstView(),
                              c.x.View(), c.rcond, ferr, berr, plan,
                              work.storage.workspace, report);
    }
  };
  const auto status = call();
  if (reject) {
    return status.code() == asc::ErrorCode::kInvalidArgument &&
           !report.called_provider && !report.native_info.has_value() &&
           report.outcome == asc::LapackOutcome::kNotRun &&
           c.a.data == before.a.data && c.af.data == before.af.data &&
           c.b.data == before.b.data && c.x.data == before.x.data &&
           c.pivots == before.pivots && c.ferr == before.ferr &&
           c.berr == before.berr && c.rcond == before.rcond && work.Guards();
  }
  return Succeeded(status, report) && work.Guards() &&
         (!factored ||
          (c.af.data == before.af.data && c.pivots == before.pivots));
}

template <typename T, bool Hermitian>
bool Workflow(const asc::ReferenceLapackProvider& provider,
              asc::DenseBlasTriangle triangle,
              const std::array<asc::DenseBlasLayout, 4>& layouts) {
  Case<T, Hermitian> c(triangle, layouts);
  const auto before = c;
  if (!Direct(provider, c) || !c.Factor() || !c.Solution() ||
      !c.Guards(before)) {
    return false;
  }
  const auto factors = c.af.data;
  const auto pivots = c.pivots;
  if (!Condition(provider, c)) {
    return false;
  }
  for (std::size_t i = 0; i < 3; ++i) {
    for (std::size_t j = 0; j < 2; ++j) {
      c.x.At(i, j) += Value<T>(0.25, 0.125);
    }
  }
  if (!Refine(provider, c) || !c.Solution() || !c.Diagnostics() ||
      c.af.data != factors || c.pivots != pivots || !c.Guards(before) ||
      !Expert(provider, c, false) || !c.Solution() || !c.Factor() ||
      !c.Diagnostics() || !c.Guards(before)) {
    return false;
  }
  for (std::size_t i = 0; i < 3; ++i) {
    for (std::size_t j = 0; j < 2; ++j) {
      c.x.At(i, j) = Value<T>(-157);
    }
  }
  return Expert(provider, c, true) && c.Solution() && c.Factor() &&
         c.Diagnostics() && c.Guards(before) && Expert(provider, c, true, true);
}

template <typename T, bool Hermitian>
bool All(const asc::ReferenceLapackProvider& provider, int& cases) {
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto a : {kColumn, kRow}) {
      for (const auto af : {kColumn, kRow}) {
        for (const auto b : {kColumn, kRow}) {
          for (const auto x : {kColumn, kRow}) {
            if (!Workflow<T, Hermitian>(provider, triangle, {a, af, b, x})) {
              std::fprintf(stderr,
                           "Expert consumer failed: HE=%d U=%d "
                           "layouts=%d/%d/%d/%d\n",
                           Hermitian, triangle == kUpper, static_cast<int>(a),
                           static_cast<int>(af), static_cast<int>(b),
                           static_cast<int>(x));
              return false;
            }
            ++cases;
          }
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
  int cases = 0;
  if (!All<float, false>(provider, cases) ||
      !All<double, false>(provider, cases) ||
      !All<std::complex<float>, false>(provider, cases) ||
      !All<std::complex<double>, false>(provider, cases) ||
      !All<std::complex<float>, true>(provider, cases) ||
      !All<std::complex<double>, true>(provider, cases)) {
    return 1;
  }
  std::printf(
      "Indefinite expert public consumer: %d workflows, %d actual "
      "numerical calls, %d rejected executions; 24 exact routes, "
      "both triangles, 16 independent layouts, VX N/F.\n",
      cases, 5 * cases, cases);
  return 0;
}
