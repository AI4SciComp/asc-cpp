#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_RK_CONDITION_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_RK_CONDITION_TEST_SUPPORT_H_
#include <array>
#include <cstddef>
#include <cstdlib>

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite_rk_condition.h"
#include "indefinite_rook_test_support.h"
namespace asc_rk_condition_test {
using asc_indefinite_rook_test::kHost;
using asc_indefinite_rook_test::kLayout;
using asc_indefinite_rook_test::kPivot;
using asc_indefinite_rook_test::kScalar;
using asc_indefinite_rook_test::TestContext;
using asc_indefinite_rook_test::Value;
// Real CON_3 needs simultaneous n IPIV and n IWORK entries. Retain two
// 16-byte guards around the largest admitted test case at true ILP64 width.
template <typename T>
struct Scratch {
  std::array<T, 4500> scalar{};
  std::array<T, 5200> packed{};
  alignas(16) std::array<std::byte, 2 * 67 * sizeof(asc::index_t) + 32> pivot{};
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
template <typename T>
auto Query(const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasTriangle triangle, bool hermitian,
           asc::DenseBlasMatrixView<const T> a,
           asc::DenseBlasVectorView<const T> extra,
           asc::RawLapackPivotView pivots, asc::DenseBlasRealType<T> norm,
           const asc::DenseBlasRealType<T>& condition) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::QueryHecon3Workspace(provider, triangle, a, extra, pivots,
                                       norm, condition);
    }
  }
  return asc::QuerySycon3Workspace(provider, triangle, a, extra, pivots, norm,
                                   condition);
}

template <typename T>
asc::Status Condition(const asc::ReferenceLapackProvider& provider,
                      asc::DenseBlasTriangle triangle, bool hermitian,
                      asc::DenseBlasMatrixView<const T> a,
                      asc::DenseBlasVectorView<const T> extra,
                      asc::RawLapackPivotView pivots,
                      asc::DenseBlasRealType<T> norm,
                      asc::DenseBlasRealType<T>& condition,
                      const asc::LapackWorkspacePlan& plan,
                      const asc::LapackWorkspace& workspace,
                      asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::Hecon3(provider, triangle, a, extra, pivots, norm, condition,
                         plan, workspace, report);
    }
  }
  return asc::Sycon3(provider, triangle, a, extra, pivots, norm, condition,
                     plan, workspace, report);
}

}  // namespace asc_rk_condition_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_RK_CONDITION_TEST_SUPPORT_H_
