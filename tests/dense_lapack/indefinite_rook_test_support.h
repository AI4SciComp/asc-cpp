#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_ROOK_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_ROOK_TEST_SUPPORT_H_

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
#include "asc/dense/providers/lapack_indefinite.h"
#include "asc/dense/providers/lapack_indefinite_rook.h"

namespace asc_indefinite_rook_test {
using asc_dense_test::TestContext;
using Wide = std::complex<long double>;
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;
constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;
constexpr auto kLower = asc::DenseBlasTriangle::kLower;
constexpr auto kSymmetric = asc::LapackBunchKaufmanSymmetry::kSymmetric;
constexpr auto kHermitian = asc::LapackBunchKaufmanSymmetry::kHermitian;
constexpr auto kBlock = asc::LapackFactorFamily::kRook;
constexpr std::size_t kScalar =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kScalar);
constexpr std::size_t kPivot =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kInteger);
constexpr std::size_t kLayout =
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

inline Wide Adjoint(Wide value, bool hermitian) {
  return hermitian ? std::conj(value) : value;
}

// Object-byte rollback oracle, deliberately not floating-point equality:
// ignored NaNs and signed-zero payloads must remain byte-identical.
inline bool EqualBytes(const void* a, const void* b, std::size_t bytes) {
  return std::memcmp(a, b, bytes) == 0;
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

template <typename T, std::size_t Size>
auto Matrix(std::array<T, Size>& data, asc::extent_t rows,
            asc::extent_t columns, asc::DenseBlasLayout layout,
            asc::extent_t ld) {
  return Take(asc::DenseBlasMatrixView<T>::Create(
      data.data() + 1, rows, columns, layout, ld,
      {data.data(), sizeof(data), kHost}));
}

template <typename T, std::size_t Size>
auto Matrix(const std::array<T, Size>& data, asc::extent_t rows,
            asc::extent_t columns, asc::DenseBlasLayout layout,
            asc::extent_t ld) {
  return Take(asc::DenseBlasMatrixView<const T>::Create(
      data.data() + 1, rows, columns, layout, ld,
      {data.data(), sizeof(data), kHost}));
}

template <std::size_t Size>
auto Pivots(std::array<asc::index_t, Size>& data, asc::extent_t n) {
  return Take(asc::DenseBlasVectorView<asc::index_t>::Create(
      data.data() + 1, n, 1, {data.data(), sizeof(data), kHost}));
}

template <std::size_t Size>
auto Raw(const std::array<asc::index_t, Size>& data, asc::extent_t n) {
  return Take(asc::RawLapackPivotView::Create(
      data.data() + 1, n, kBlock, {data.data(), sizeof(data), kHost}));
}

template <typename T>
auto QueryFactor(const asc::ReferenceLapackProvider& provider,
                 asc::DenseBlasTriangle triangle, bool hermitian, bool blocked,
                 asc::DenseBlasMatrixView<T> matrix,
                 asc::DenseBlasVectorView<asc::index_t> pivots) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return blocked ? asc::QueryHetrfRookWorkspace(provider, triangle, matrix,
                                                    pivots)
                     : asc::QueryHetf2RookWorkspace(provider, triangle, matrix,
                                                    pivots);
    }
  }
  return blocked
             ? asc::QuerySytrfRookWorkspace(provider, triangle, matrix, pivots)
             : asc::QuerySytf2RookWorkspace(provider, triangle, matrix, pivots);
}

template <typename T>
asc::Status Factor(const asc::ReferenceLapackProvider& provider,
                   asc::DenseBlasTriangle triangle, bool hermitian,
                   bool blocked, asc::DenseBlasMatrixView<T> matrix,
                   asc::DenseBlasVectorView<asc::index_t> pivots,
                   const asc::LapackWorkspacePlan& plan,
                   const asc::LapackWorkspace& workspace,
                   asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return blocked ? asc::HetrfRook(provider, triangle, matrix, pivots, plan,
                                      workspace, report)
                     : asc::Hetf2Rook(provider, triangle, matrix, pivots, plan,
                                      workspace, report);
    }
  }
  return blocked ? asc::SytrfRook(provider, triangle, matrix, pivots, plan,
                                  workspace, report)
                 : asc::Sytf2Rook(provider, triangle, matrix, pivots, plan,
                                  workspace, report);
}

template <typename T>
auto QuerySolve(const asc::ReferenceLapackProvider& provider, bool hermitian,
                const asc::ReferenceRookFactorView<T>& factor,
                asc::DenseBlasMatrixView<T> rhs) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::QueryHetrsRookWorkspace(provider, factor, rhs);
    }
  }
  return asc::QuerySytrsRookWorkspace(provider, factor, rhs);
}

template <typename T>
asc::Status Solve(const asc::ReferenceLapackProvider& provider, bool hermitian,
                  const asc::ReferenceRookFactorView<T>& factor,
                  asc::DenseBlasMatrixView<T> rhs,
                  const asc::LapackWorkspacePlan& plan,
                  const asc::LapackWorkspace& workspace,
                  asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::HetrsRook(provider, factor, rhs, plan, workspace, report);
    }
  }
  return asc::SytrsRook(provider, factor, rhs, plan, workspace, report);
}

template <typename T>
struct Scratch {
  std::array<T, 4500> scalar{};
  std::array<T, 5200> packed{};
  alignas(16) std::array<std::byte, 592> pivot{};
  Scratch() {
    scalar.fill(Value<T>(-107, 11));
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
    return workspace;
  }
  void Guards(TestContext& test, const asc::LapackWorkspace& workspace) const {
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

}  // namespace asc_indefinite_rook_test

#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_ROOK_TEST_SUPPORT_H_
