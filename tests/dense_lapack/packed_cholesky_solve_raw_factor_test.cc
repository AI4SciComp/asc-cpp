#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <initializer_list>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_packed_solve.h"
#include "factorization_support.h"
#include "normal_return_guard.h"
#include "test_support.h"

namespace {
using asc_dense_test::TestContext;
using installed_internal::Take;
using installed_internal::Value;
using installed_internal::Widen;
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kPacking =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);

// A scalar raw factor d represents |d|^2, even for a negative or imaginary d.
// PPTRS does not certify that the supplied factor came from a successful PPTRF.
// Zero d follows the pinned TPSV arithmetic and still reports native INFO=0.
template <typename T>
void Observe(TestContext& test, const asc::ReferenceLapackProvider& provider,
             T diagonal, T input, asc::DenseBlasTriangle triangle,
             asc::DenseBlasLayout a_layout, asc::DenseBlasLayout b_layout) {
  std::array<T, 8> factors;
  std::array<T, 8> rhs;
  std::array<T, 8> packing;
  factors.fill(T{-11});
  rhs.fill(T{-13});
  packing.fill(T{-17});
  factors[1] = diagonal;
  rhs[1] = input;
  const auto old_factors = factors;
  const auto a = Take(asc::DenseBlasPackedMatrixView<const T>::Create(
      factors.data() + 1, 1, a_layout,
      {factors.data(), sizeof(factors), kHost}));
  const auto b = Take(asc::DenseBlasMatrixView<T>::Create(
      rhs.data() + 1, 1, 1, b_layout, 3, {rhs.data(), sizeof(rhs), kHost}));
  const auto plan = Take(asc::QueryPptrsWorkspace(provider, triangle, a, b));
  const std::size_t entries = static_cast<std::size_t>(a_layout == kRow) +
                              static_cast<std::size_t>(b_layout == kRow);
  ASC_DENSE_TEST_EQ(test, plan.regions[kPacking].minimum_entries,
                    static_cast<asc::extent_t>(entries));
  asc::LapackWorkspace workspace;
  if (entries != 0) {
    workspace.regions[kPacking] = {packing.data() + 1, entries * sizeof(T),
                                   kHost};
  }
  asc::LapackReport report;
  const auto status =
      asc::Pptrs(provider, triangle, a, b, plan, workspace, report);
  ASC_DENSE_TEST_CHECK(test, installed_internal::Succeeded(status, report));
  ASC_DENSE_TEST_CHECK(test, factors == old_factors);
  for (std::size_t i = 0; i < rhs.size(); ++i) {
    if (i != 1) {
      ASC_DENSE_TEST_EQ(test, rhs[i], T{-13});
    }
    if (i == 0 || i > entries) {
      ASC_DENSE_TEST_EQ(test, packing[i], T{-17});
    }
  }
  const auto actual = Widen(rhs[1]);
  if (diagonal == T{}) {
    ASC_DENSE_TEST_CHECK(
        test, !std::isfinite(actual.real()) || !std::isfinite(actual.imag()));
  } else {
    // All inputs and scalar answers are exact dyadics. This independent
    // |d|^2 equation needs no tolerance or comparison against a provider call.
    const auto expected = Widen(input) / std::norm(Widen(diagonal));
    ASC_DENSE_TEST_CHECK(test, actual == expected);
  }
}

template <typename T>
void Scalar(TestContext& test, const asc::ReferenceLapackProvider& provider,
            std::size_t& profiles) {
  const std::array<T, 3> diagonals{T{}, T{-2}, Value<T>(0, 2)};
  const std::size_t count = asc::DenseBlasComplex<T> ? 3 : 2;
  for (std::size_t i = 0; i < count; ++i) {
    for (T input : {T{}, T{1}}) {
      for (auto triangle :
           {asc::DenseBlasTriangle::kUpper, asc::DenseBlasTriangle::kLower}) {
        for (auto a_layout : {kColumn, kRow}) {
          for (auto b_layout : {kColumn, kRow}) {
            Observe(test, provider, diagonals[i], input, triangle, a_layout,
                    b_layout);
            ++profiles;
          }
        }
      }
    }
  }
}
}  // namespace

int main() {
  const asc_lapack_test::NormalReturnGuard normal_return;
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  std::size_t profiles = 0;
  Scalar<float>(test, provider, profiles);
  Scalar<double>(test, provider, profiles);
  Scalar<std::complex<float>>(test, provider, profiles);
  Scalar<std::complex<double>>(test, provider, profiles);
  ASC_DENSE_TEST_EQ(test, profiles, 160U);
  std::printf("Packed Cholesky solve raw-factor profiles: %zu\n", profiles);
  return test.Finish();
}
