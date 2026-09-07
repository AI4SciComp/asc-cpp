#ifndef ASC_TESTS_DENSE_LAPACK_LU_REFINEMENT_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_LU_REFINEMENT_TEST_SUPPORT_H_

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <utility>

#include "../allocation_observation.h"
#include "../dense/allocation_probe.h"
#include "../dense/test_support.h"
#include "allocation_audit.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu_refinement.h"

namespace asc_refinement_test {
using asc_dense_test::TestContext;
using Wide = std::complex<long double>;
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;
constexpr auto kNone = asc::DenseBlasTranspose::kNone;
constexpr auto kTranspose = asc::DenseBlasTranspose::kTranspose;
constexpr auto kConjugate = asc::DenseBlasTranspose::kConjugateTranspose;
constexpr auto kScalar =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kScalar);
constexpr auto kReal =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kReal);
constexpr auto kInteger =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kInteger);
constexpr auto kPivots =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kPivotConversion);
constexpr auto kLayout =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);

template <typename T>
T Take(asc::Result<T> result) {
  if (!result.ok()) {
    std::fprintf(stderr, "Unexpected setup failure %d\n",
                 static_cast<int>(result.status().code()));
    std::abort();
  }
  return std::move(*result);
}

template <typename Operation>
auto WithoutAllocation(TestContext& test, Operation operation) {
  asc_dense_test::AllocationProbe cpp_probe;
  asc_lapack_test::BeginAllocationAudit();
  auto result = operation();
  const auto c_calls = asc_lapack_test::EndAllocationAudit();
  const auto cpp_calls = cpp_probe.count();
  ASC_DENSE_TEST_EQ(test, c_calls, 0U);
  ASC_DENSE_TEST_CHECK(test,
                       asc_test::ProcessAllocationCountMatches(cpp_calls, 0));
  return result;
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

inline long double Magnitude(Wide value) {
  return std::abs(value.real()) + std::abs(value.imag());
}

template <typename T, std::size_t Size>
auto Vector(std::array<T, Size>& storage, asc::extent_t size) {
  return Take(asc::DenseBlasVectorView<T>::Create(
      storage.data() + 1, size, 1, {storage.data(), sizeof(storage), kHost}));
}

template <typename T>
struct Sample {
  using Real = asc::DenseBlasRealType<T>;
  asc::extent_t n;
  asc::extent_t nrhs;
  unsigned int layouts;
  std::array<T, 84> a{};
  std::array<T, 84> af{};
  std::array<T, 44> b{};
  std::array<T, 44> x{};
  std::array<asc::index_t, 10> pivots{};
  std::array<Real, 5> ferr{};
  std::array<Real, 5> berr{};
  std::array<T, 26> scalar{};
  std::array<Real, 10> real{};
  alignas(16) std::array<std::byte, 144> integer{};
  alignas(16) std::array<std::byte, 80> converted{};
  std::array<T, 201> packed{};

  Sample(asc::extent_t order, asc::extent_t count, unsigned int mask)
      : n(order), nrhs(count), layouts(mask) {
    a.fill(Value<T>(-71));
    af.fill(Value<T>(-73));
    b.fill(Value<T>(-79));
    x.fill(Value<T>(-83));
    pivots.fill(-89);
    ferr.fill(-97);
    berr.fill(-101);
    scalar.fill(Value<T>(-103));
    real.fill(-107);
    integer.fill(std::byte{0x5a});
    converted.fill(std::byte{0x6b});
    packed.fill(Value<T>(-109));
  }

  [[nodiscard]] asc::DenseBlasLayout Layout(unsigned int index) const {
    return (layouts & (1U << index)) == 0 ? kColumn : kRow;
  }
  [[nodiscard]] asc::extent_t Ld(unsigned int index) const {
    return index < 2 || Layout(index) == kColumn ? 10 : 5;
  }
  [[nodiscard]] std::size_t Offset(unsigned int index, asc::extent_t i,
                                   asc::extent_t j) const {
    return static_cast<std::size_t>(
        1 + (Layout(index) == kColumn ? j * Ld(index) + i : i * Ld(index) + j));
  }
  template <typename E, std::size_t Size>
  auto Matrix(std::array<E, Size>& storage, unsigned int index) const {
    return Take(asc::DenseBlasMatrixView<E>::Create(
        storage.data() + 1, n, index < 2 ? n : nrhs, Layout(index), Ld(index),
        {storage.data(), sizeof(storage), kHost}));
  }
  template <std::size_t Size>
  [[nodiscard]] auto Matrix(const std::array<T, Size>& storage,
                            unsigned int index) const {
    return Take(asc::DenseBlasMatrixView<const T>::Create(
        storage.data() + 1, n, index < 2 ? n : nrhs, Layout(index), Ld(index),
        {storage.data(), sizeof(storage), kHost}));
  }
  [[nodiscard]] asc::RawLapackPivotView Pivots() const {
    return Take(asc::RawLapackPivotView::Create(
        pivots.data() + 1, n, asc::LapackFactorFamily::kLuPartialPivot,
        {pivots.data(), sizeof(pivots), kHost}));
  }
  [[nodiscard]] auto Query(const asc::ReferenceLapackProvider& provider,
                           asc::DenseBlasTranspose trans) {
    return asc::QueryGerfsWorkspace(
        provider, trans, Matrix(std::as_const(a), 0),
        Matrix(std::as_const(af), 1), Pivots(), Matrix(std::as_const(b), 2),
        Matrix(x, 3), Vector(ferr, nrhs), Vector(berr, nrhs));
  }
  [[nodiscard]] auto Execute(const asc::ReferenceLapackProvider& provider,
                             asc::DenseBlasTranspose trans,
                             const asc::LapackWorkspacePlan& plan,
                             const asc::LapackWorkspace& workspace,
                             asc::LapackReport& report) {
    return asc::Gerfs(provider, trans, Matrix(std::as_const(a), 0),
                      Matrix(std::as_const(af), 1), Pivots(),
                      Matrix(std::as_const(b), 2), Matrix(x, 3),
                      Vector(ferr, nrhs), Vector(berr, nrhs), plan, workspace,
                      report);
  }
  [[nodiscard]] asc::LapackWorkspace Workspace(
      const asc::LapackWorkspacePlan& plan) {
    asc::LapackWorkspace workspace;
    const auto bytes = [&](std::size_t role) {
      return static_cast<std::size_t>(plan.regions[role].minimum_entries) *
             plan.regions[role].entry_bytes;
    };
    workspace.regions[kScalar] = {scalar.data() + 1, bytes(kScalar), kHost};
    workspace.regions[kReal] = {real.data() + 1, bytes(kReal), kHost};
    workspace.regions[kInteger] = {integer.data() + 8, bytes(kInteger), kHost};
    workspace.regions[kPivots] = {converted.data() + 8, bytes(kPivots), kHost};
    workspace.regions[kLayout] = {packed.data() + 1, bytes(kLayout), kHost};
    return workspace;
  }

  void Guards(TestContext& test, const asc::LapackWorkspacePlan& plan) const {
    ASC_DENSE_TEST_EQ(test, ferr.front(), Real{-97});
    ASC_DENSE_TEST_EQ(test, ferr.back(), Real{-97});
    ASC_DENSE_TEST_EQ(test, berr.front(), Real{-101});
    ASC_DENSE_TEST_EQ(test, berr.back(), Real{-101});
    ASC_DENSE_TEST_EQ(test, scalar.front(), Value<T>(-103));
    ASC_DENSE_TEST_EQ(test, real.front(), Real{-107});
    ASC_DENSE_TEST_EQ(test, packed.front(), Value<T>(-109));
    for (std::size_t i = 1 + plan.regions[kScalar].minimum_entries;
         i < scalar.size(); ++i) {
      ASC_DENSE_TEST_EQ(test, scalar[i], Value<T>(-103));
    }
    for (std::size_t i = 1 + plan.regions[kReal].minimum_entries;
         i < real.size(); ++i) {
      ASC_DENSE_TEST_EQ(test, real[i], Real{-107});
    }
    for (std::size_t i = 1 + plan.regions[kLayout].minimum_entries;
         i < packed.size(); ++i) {
      ASC_DENSE_TEST_EQ(test, packed[i], Value<T>(-109));
    }
    for (std::size_t i = 0; i < integer.size(); ++i) {
      const auto& integer_region = plan.regions[kInteger];
      const auto& pivot_region = plan.regions[kPivots];
      if (i < 8 || i >= 8 + integer_region.minimum_entries *
                                integer_region.entry_bytes) {
        ASC_DENSE_TEST_EQ(test, integer[i], std::byte{0x5a});
      }
      if (i < converted.size() &&
          (i < 8 ||
           i >= 8 + pivot_region.minimum_entries * pivot_region.entry_bytes)) {
        ASC_DENSE_TEST_EQ(test, converted[i], std::byte{0x6b});
      }
    }
  }
};

template <typename T>
Wide OriginalOp(const Sample<T>& sample, asc::DenseBlasTranspose trans,
                asc::extent_t i, asc::extent_t j) {
  const Wide value = ToWide(sample.a[sample.Offset(0, trans == kNone ? i : j,
                                                   trans == kNone ? j : i)]);
  return trans == kConjugate ? std::conj(value) : value;
}

template <typename T>
long double BackwardError(const Sample<T>& sample,
                          asc::DenseBlasTranspose trans, asc::extent_t column) {
  long double maximum = 0;
  for (asc::extent_t i = 0; i < sample.n; ++i) {
    const Wide b = ToWide(sample.b[sample.Offset(2, i, column)]);
    Wide residual = b;
    long double denominator = Magnitude(b);
    for (asc::extent_t j = 0; j < sample.n; ++j) {
      const Wide a = OriginalOp(sample, trans, i, j);
      const Wide x = ToWide(sample.x[sample.Offset(3, j, column)]);
      residual -= a * x;
      denominator += Magnitude(a) * Magnitude(x);
    }
    maximum = std::max(maximum, Magnitude(residual) / denominator);
  }
  return maximum;
}

template <typename T>
T TrueSolution(asc::extent_t row, asc::extent_t column) {
  return Value<T>(0.25L * (1 + row + column), 0.1L * (1 + row - column));
}

template <typename T>
void Prepare(TestContext& test, const asc::ReferenceLapackProvider& provider,
             asc::DenseBlasTranspose trans, int exponent, Sample<T>& sample) {
  std::array<T, 80> column_factors{};
  const long double scale = std::ldexp(1.0L, exponent);
  for (asc::extent_t i = 0; i < sample.n; ++i) {
    const auto permuted = sample.n - 1 - i;
    for (asc::extent_t j = 0; j < sample.n; ++j) {
      const T value =
          Value<T>(scale * (permuted == j ? 5 + j : 0.2L * (1 + permuted - j)),
                   scale * (permuted == j ? 0.3L : 0.1L * (permuted + j)));
      sample.a[sample.Offset(0, i, j)] = value;
      column_factors[j * 10 + i] = value;
    }
  }
  const auto factors = Take(asc::DenseBlasMatrixView<T>::Create(
      column_factors.data(), sample.n, sample.n, kColumn, 10,
      {column_factors.data(), sizeof(column_factors), kHost}));
  auto pivots = Vector(sample.pivots, sample.n);
  const auto factor_plan =
      Take(asc::QueryGetrfWorkspace(provider, factors, pivots));
  alignas(16) std::array<std::byte, 64> work{};
  asc::LapackWorkspace workspace;
  workspace.regions[kInteger] = {work.data(), sizeof(work), kHost};
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, asc::Getrf(provider, factors, pivots, factor_plan,
                                        workspace, report)
                                 .ok());
  for (asc::extent_t i = 0; i < sample.n; ++i) {
    for (asc::extent_t j = 0; j < sample.n; ++j) {
      sample.af[sample.Offset(1, i, j)] = column_factors[j * 10 + i];
    }
  }
  for (asc::extent_t j = 0; j < sample.nrhs; ++j) {
    for (asc::extent_t i = 0; i < sample.n; ++i) {
      Wide value = 0;
      for (asc::extent_t k = 0; k < sample.n; ++k) {
        value +=
            OriginalOp(sample, trans, i, k) * ToWide(TrueSolution<T>(k, j));
      }
      sample.b[sample.Offset(2, i, j)] = Value<T>(value.real(), value.imag());
      // Deliberately perturbed approximate solution exercises actual iterative
      // updates, instead of merely passing a correct solution to the routine.
      const auto epsilon =
          std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon();
      sample.x[sample.Offset(3, i, j)] =
          TrueSolution<T>(i, j) + Value<T>(512 * epsilon, -256 * epsilon);
    }
  }
}

}  // namespace asc_refinement_test

#endif  // ASC_TESTS_DENSE_LAPACK_LU_REFINEMENT_TEST_SUPPORT_H_
