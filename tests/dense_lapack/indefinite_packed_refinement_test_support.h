#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_REFINEMENT_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_REFINEMENT_TEST_SUPPORT_H_
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdlib>
#include <limits>

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite_packed_refinement.h"
#include "indefinite_packed_solve_test_support.h"
#include "indefinite_test_support.h"
#include "tests/dense/test_support.h"

namespace asc_packed_refinement_test {
namespace base = asc_indefinite_test;
namespace solve = asc_packed_solve_test;
using base::EqualBytes;
using base::kColumn;
using base::kHost;
using base::kLower;
using base::kPivot;
using base::kRow;
using base::kScalar;
using base::kUpper;
using base::Take;
using base::TestContext;
using base::ToWide;
using base::Value;
using base::Wide;
inline constexpr auto kLayout = base::kLayout;
inline constexpr auto kReal =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kReal);

template <typename T>
struct Scratch {
  std::array<T, 4500> scalar{};
  std::array<asc::DenseBlasRealType<T>, 140> real{};
  std::array<T, 5200> packed{};
  alignas(16) std::array<std::byte, 2 * 67 * sizeof(asc::index_t) + 32> pivot{};
  Scratch() {
    scalar.fill(Value<T>(-107, 11));
    real.fill(asc::DenseBlasRealType<T>{-113});
    packed.fill(Value<T>(-109, 13));
    pivot.fill(std::byte{0x5a});
  }
  asc::LapackWorkspace Workspace(const asc::LapackWorkspacePlan& plan,
                                 asc::extent_t scalar_entries = -1) {
    asc::LapackWorkspace workspace;
    for (const auto role : {kScalar, kPivot, kLayout}) {
      const auto& requirement = plan.regions[role];
      const auto entries = role == kScalar && scalar_entries >= 0
                               ? scalar_entries
                               : requirement.preferred_entries;
      if (entries == 0) {
        continue;
      }
      const auto bytes =
          static_cast<std::size_t>(entries) * requirement.entry_bytes;
      if (role == kScalar) {
        if (bytes > (scalar.size() - 2) * sizeof(T)) {
          std::abort();
        }
        workspace.regions[role] = {scalar.data() + 1, bytes, kHost};
      } else if (role == kLayout) {
        if (bytes > (packed.size() - 2) * sizeof(T)) {
          std::abort();
        }
        workspace.regions[role] = {packed.data() + 1, bytes, kHost};
      } else {
        if (bytes > pivot.size() - 32) {
          std::abort();
        }
        workspace.regions[role] = {pivot.data() + 16, bytes, kHost};
      }
    }
    const auto real_entries = plan.regions[kReal].preferred_entries;
    if (real_entries < 0 ||
        static_cast<std::size_t>(real_entries) > real.size() - 2) {
      std::abort();
    }
    if (real_entries != 0) {
      workspace.regions[kReal] = {
          real.data() + 1,
          static_cast<std::size_t>(real_entries) * sizeof(real[0]), kHost};
    }
    return workspace;
  }
  void Guards(TestContext& test, const asc::LapackWorkspace& workspace) const {
    ASC_DENSE_TEST_EQ(test, real.front(), asc::DenseBlasRealType<T>{-113});
    for (std::size_t i = 1 + workspace.regions[kReal].size() / sizeof(real[0]);
         i < real.size(); ++i) {
      ASC_DENSE_TEST_EQ(test, real[i], asc::DenseBlasRealType<T>{-113});
    }
    ASC_DENSE_TEST_EQ(test, scalar.front(), Value<T>(-107, 11));
    ASC_DENSE_TEST_EQ(test, packed.front(), Value<T>(-109, 13));
    for (std::size_t i = 1 + workspace.regions[kScalar].size() / sizeof(T);
         i < scalar.size(); ++i) {
      ASC_DENSE_TEST_EQ(test, scalar[i], Value<T>(-107, 11));
    }
    for (std::size_t i = 1 + workspace.regions[kLayout].size() / sizeof(T);
         i < packed.size(); ++i) {
      ASC_DENSE_TEST_EQ(test, packed[i], Value<T>(-109, 13));
    }
    for (std::size_t i = 0; i < 16; ++i) {
      ASC_DENSE_TEST_EQ(test, pivot[i], std::byte{0x5a});
    }
    for (std::size_t i = 16 + workspace.regions[kPivot].size();
         i < pivot.size(); ++i) {
      ASC_DENSE_TEST_EQ(test, pivot[i], std::byte{0x5a});
    }
  }
};
template <typename T>
struct Fixture {
  using Real = asc::DenseBlasRealType<T>;
  solve::Fixture<T> system;
  asc::DenseBlasLayout original_layout;
  asc::DenseBlasLayout solution_layout;
  std::array<T, 5000> original;
  std::array<T, 700> x;
  std::array<T, 700> initial_x;
  std::array<Real, 5> ferr;
  std::array<Real, 5> berr;

  Fixture(int n, int nrhs, bool hermitian, asc::DenseBlasTriangle triangle,
          asc::DenseBlasLayout a_layout, asc::DenseBlasLayout af_layout,
          asc::DenseBlasLayout b_layout, asc::DenseBlasLayout x_layout,
          int exponent, int kind)
      : system(n, nrhs, hermitian, triangle, af_layout, b_layout, exponent,
               kind),
        original_layout(a_layout),
        solution_layout(x_layout) {
    original.fill(Value<T>(-281, 29));
    std::size_t slot = 1;
    // Independently enumerate each public packed layout, not adapter indices.
    for (int major = 0; major < n; ++major) {
      for (int minor = 0; minor < n; ++minor) {
        const int i = a_layout == kColumn ? minor : major;
        const int j = a_layout == kColumn ? major : minor;
        if (system.a.Selected(i, j)) {
          const auto value = system.a.full[i * n + j];
          original[slot] = Value<T>(value.real(), value.imag());
          if constexpr (asc::DenseBlasComplex<T>) {
            if (hermitian && i == j) {
              original[slot].imag(std::numeric_limits<Real>::quiet_NaN());
            }
          }
          ++slot;
        }
      }
    }
    x.fill(Value<T>(-283, 31));
    for (int j = 0; j < nrhs; ++j) {
      for (int i = 0; i < n; ++i) {
        x[Offset(i, j)] = system.Solution(i, j) * Real{1.0625};
      }
    }
    initial_x = x;
    ferr.fill(Real{-293});
    berr.fill(Real{-307});
  }
  [[nodiscard]] int Leading() const {
    return solution_layout == kColumn ? system.a.n + 2 : system.nrhs + 2;
  }
  [[nodiscard]] std::size_t Offset(int i, int j) const {
    return 1 + static_cast<std::size_t>(solution_layout == kColumn
                                            ? j * Leading() + i
                                            : i * Leading() + j);
  }
  [[nodiscard]] auto Original() const {
    return Take(asc::DenseBlasPackedMatrixView<const T>::Create(
        original.data() + 1, system.a.n, original_layout,
        {original.data(), sizeof(original), kHost}));
  }
  [[nodiscard]] auto Rhs() const {
    return Take(asc::DenseBlasMatrixView<const T>::Create(
        system.rhs.data() + 1, system.a.n, system.nrhs, system.rhs_layout,
        system.Leading(), {system.rhs.data(), sizeof(system.rhs), kHost}));
  }
  auto Solution() {
    return Take(asc::DenseBlasMatrixView<T>::Create(
        x.data() + 1, system.a.n, system.nrhs, solution_layout, Leading(),
        {x.data(), sizeof(x), kHost}));
  }
  auto Forward() {
    return Take(asc::DenseBlasVectorView<Real>::Create(
        ferr.data() + 1, system.nrhs, 1, {ferr.data(), sizeof(ferr), kHost}));
  }
  auto Backward() {
    return Take(asc::DenseBlasVectorView<Real>::Create(
        berr.data() + 1, system.nrhs, 1, {berr.data(), sizeof(berr), kHost}));
  }
  void Prepare(TestContext& test,
               const asc::ReferenceLapackProvider& provider) {
    system.Prepare(test, provider);
  }
  void Padding(TestContext& test) const {
    auto expected = initial_x;
    for (int j = 0; j < system.nrhs; ++j) {
      for (int i = 0; i < system.a.n; ++i) {
        expected[Offset(i, j)] = x[Offset(i, j)];
      }
    }
    ASC_DENSE_TEST_CHECK(test,
                         EqualBytes(x.data(), expected.data(), sizeof(x)));
    for (std::size_t i = 0; i < ferr.size(); ++i) {
      if (i == 0 || i > static_cast<std::size_t>(system.nrhs)) {
        ASC_DENSE_TEST_EQ(test, ferr[i], Real{-293});
        ASC_DENSE_TEST_EQ(test, berr[i], Real{-307});
      }
    }
    ASC_DENSE_TEST_EQ(test, original.front(), Value<T>(-281, 29));
    for (std::size_t i =
             1 + static_cast<std::size_t>(system.a.n) * (system.a.n + 1) / 2;
         i < original.size(); ++i) {
      ASC_DENSE_TEST_EQ(test, original[i], Value<T>(-281, 29));
    }
    system.UnchangedInputs(test);
    ASC_DENSE_TEST_EQ(test, system.rhs, system.original_rhs);
    system.a.Guards(test);
  }
  void Mathematics(TestContext& test) const {
    const long double tolerance =
        256 * std::max(1, system.a.n) * std::numeric_limits<Real>::epsilon();
    for (int j = 0; j < system.nrhs; ++j) {
      ASC_DENSE_TEST_CHECK(test,
                           std::isfinite(ferr[j + 1]) && ferr[j + 1] >= 0);
      ASC_DENSE_TEST_CHECK(test,
                           std::isfinite(berr[j + 1]) && berr[j + 1] >= 0);
      if (system.a.n == 0) {
        ASC_DENSE_TEST_EQ(test, ferr[j + 1], Real{0});
        ASC_DENSE_TEST_EQ(test, berr[j + 1], Real{0});
        continue;
      }
      long double error = 0;
      long double norm_x = 0;
      long double backward = 0;
      long double diagnostic = 0;
      // The pinned LAMCH assumes rounding: E=epsilon/2, S=TINY for these
      // IEEE types. Independently evaluate the documented safe-minimum BERR
      // expression in wide arithmetic, while retaining the unregularized
      // solution residual as a separate quality requirement.
      const long double safe1 = static_cast<long double>(system.a.n + 1) *
                                std::numeric_limits<Real>::min();
      const long double safe2 =
          safe1 / (std::numeric_limits<Real>::epsilon() / 2);

      for (int i = 0; i < system.a.n; ++i) {
        const auto value = ToWide(x[Offset(i, j)]);
        const auto difference = Abs1(value - ToWide(system.Solution(i, j)));
        ASC_DENSE_TEST_CHECK(test, std::isfinite(difference));
        error = std::max(error, difference);
        norm_x = std::max(norm_x, Abs1(value));
        auto residual = ToWide(system.original_rhs[system.Offset(i, j)]);
        long double denominator = Abs1(residual);
        for (int k = 0; k < system.a.n; ++k) {
          const auto entry = system.a.full[i * system.a.n + k];
          const auto solution = ToWide(x[Offset(k, j)]);
          residual -= entry * solution;
          denominator += Abs1(entry) * Abs1(solution);
        }
        ASC_DENSE_TEST_CHECK(test,
                             std::isfinite(Abs1(residual)) && denominator > 0);
        const long double ratio = Abs1(residual) / denominator;
        backward = std::max(backward, ratio);
        const long double guarded =
            denominator > safe2
                ? ratio
                : (Abs1(residual) + safe1) / (denominator + safe1);
        diagnostic = std::max(diagnostic, guarded);
      }
      ASC_DENSE_TEST_CHECK(test, norm_x > 0 && error / norm_x <= tolerance);
      ASC_DENSE_TEST_CHECK(test, backward <= tolerance);
      ASC_DENSE_TEST_CHECK(test,
                           std::abs(diagnostic - berr[j + 1]) <= tolerance);
      ASC_DENSE_TEST_CHECK(test, error / norm_x <= ferr[j + 1] + tolerance);
    }
  }
  static long double Abs1(Wide value) {
    return std::abs(value.real()) + std::abs(value.imag());
  }
};

template <typename T>
auto Query(const asc::ReferenceLapackProvider& provider, Fixture<T>& sample) {
  const auto& a = sample.system.a;
  if constexpr (asc::DenseBlasComplex<T>) {
    if (a.hermitian) {
      return asc::QueryHprfsWorkspace(provider, a.triangle, sample.Original(),
                                      a.ConstView(), sample.system.Pivots(),
                                      sample.Rhs(), sample.Solution(),
                                      sample.Forward(), sample.Backward());
    }
  }
  return asc::QuerySprfsWorkspace(provider, a.triangle, sample.Original(),
                                  a.ConstView(), sample.system.Pivots(),
                                  sample.Rhs(), sample.Solution(),
                                  sample.Forward(), sample.Backward());
}
template <typename T>
asc::Status Refine(const asc::ReferenceLapackProvider& provider,
                   Fixture<T>& sample, const asc::LapackWorkspacePlan& plan,
                   const asc::LapackWorkspace& workspace,
                   asc::LapackReport& report) {
  const auto& a = sample.system.a;
  if constexpr (asc::DenseBlasComplex<T>) {
    if (a.hermitian) {
      return asc::Hprfs(provider, a.triangle, sample.Original(), a.ConstView(),
                        sample.system.Pivots(), sample.Rhs(), sample.Solution(),
                        sample.Forward(), sample.Backward(), plan, workspace,
                        report);
    }
  }
  return asc::Sprfs(provider, a.triangle, sample.Original(), a.ConstView(),
                    sample.system.Pivots(), sample.Rhs(), sample.Solution(),
                    sample.Forward(), sample.Backward(), plan, workspace,
                    report);
}
}  // namespace asc_packed_refinement_test
#endif
