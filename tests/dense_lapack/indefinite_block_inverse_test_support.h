#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_BLOCK_INVERSE_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_BLOCK_INVERSE_TEST_SUPPORT_H_
#include <array>
#include <cstddef>
#include <string_view>
#include <type_traits>

#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite_block_inverse.h"
#include "indefinite_test_support.h"
namespace asc_block_inverse_test {
// Selected once by main before any workers; read-only throughout each run.
inline asc::extent_t g_block_size = 0;
inline bool Select(std::string_view value) {
  if (value == "driver") {
    g_block_size = 0;
    return true;
  }
  if (value == "1") {
    g_block_size = 1;
    return true;
  }
  if (value == "2") {
    g_block_size = 2;
    return true;
  }
  if (value == "3") {
    g_block_size = 3;
    return true;
  }
  if (value == "64") {
    g_block_size = 64;
    return true;
  }
  return false;
}
template <typename T>
asc::extent_t Block(bool hermitian) {
  if (g_block_size != 0) {
    return g_block_size;
  }
  return hermitian || std::is_same_v<T, float> ? 64 : 1;
}
template <typename T>
asc::extent_t Entries(asc::extent_t order, bool hermitian) {
  const auto nb = Block<T>(hermitian);
  return order == 0 ? 0 : (order + nb + 1) * (nb + 3);
}

template <typename T>
auto Query(const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasTriangle triangle, bool hermitian,
           asc::DenseBlasMatrixView<T> factors,
           asc::RawLapackPivotView pivots) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return g_block_size == 0
                 ? asc::QueryHetri2Workspace(provider, triangle, factors,
                                             pivots)
                 : asc::QueryHetri2xWorkspace(provider, triangle, g_block_size,
                                              factors, pivots);
    }
  }
  return g_block_size == 0
             ? asc::QuerySytri2Workspace(provider, triangle, factors, pivots)
             : asc::QuerySytri2xWorkspace(provider, triangle, g_block_size,
                                          factors, pivots);
}
template <typename T>
asc::Status Inverse(const asc::ReferenceLapackProvider& provider,
                    asc::DenseBlasTriangle triangle, bool hermitian,
                    asc::DenseBlasMatrixView<T> factors,
                    asc::RawLapackPivotView pivots,
                    const asc::LapackWorkspacePlan& plan,
                    const asc::LapackWorkspace& workspace,
                    asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return g_block_size == 0
                 ? asc::Hetri2(provider, triangle, factors, pivots, plan,
                               workspace, report)
                 : asc::Hetri2x(provider, triangle, g_block_size, factors,
                                pivots, plan, workspace, report);
    }
  }
  return g_block_size == 0
             ? asc::Sytri2(provider, triangle, factors, pivots, plan, workspace,
                           report)
             : asc::Sytri2x(provider, triangle, g_block_size, factors, pivots,
                            plan, workspace, report);
}
using asc_indefinite_test::kHost;
using asc_indefinite_test::kLayout;
using asc_indefinite_test::kPivot;
using asc_indefinite_test::kScalar;
using asc_indefinite_test::Take;
using asc_indefinite_test::TestContext;
using asc_indefinite_test::Value;
template <typename T>
struct Scratch {
  std::array<T, 17000> scalar{};
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

}  // namespace asc_block_inverse_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_BLOCK_INVERSE_TEST_SUPPORT_H_
