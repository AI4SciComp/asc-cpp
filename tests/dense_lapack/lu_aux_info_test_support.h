#ifndef ASC_TESTS_DENSE_LAPACK_LU_AUX_INFO_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_LU_AUX_INFO_TEST_SUPPORT_H_
#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "lu_aux_info_faults.h"
#include "lu_expert_info_test_support.h"

namespace asc_lu_aux_info_test {
using asc_dense_test::TestContext;
using asc_lu_expert_info_test::Matrix;
using asc_lu_expert_info_test::Observe;
using asc_lu_expert_info_test::Take;
using asc_lu_expert_info_test::Value;
using asc_lu_expert_info_test::Wide;
using Fault = asc_lapack_test::LuAuxInfoFault;
constexpr auto kHost = asc_lu_expert_info_test::kHost;
constexpr auto kRow = asc_lu_expert_info_test::kRow;
constexpr auto kColumn = asc_lu_expert_info_test::kColumn;
constexpr auto kScalar = asc_lu_expert_info_test::kScalar;
constexpr auto kLayout = asc_lu_expert_info_test::kLayout;
constexpr auto kInteger = asc_lu_expert_info_test::kInteger;
constexpr auto kReal =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kReal);
constexpr auto kSentinel = asc_lu_expert_info_test::kSentinel;
std::array<char, 16> Name(char scalar, std::string_view routine);
asc::LapackReport DirtyReport();
void CheckInfo(
    TestContext& test, const asc::ReferenceLapackProvider& provider,
    const asc::Status& status, const asc::LapackReport& report, bool called,
    Fault fault, std::string_view routine, std::int64_t actual = 0,
    asc::LapackOutcome normal = asc::LapackOutcome::kSuccess,
    asc::LapackOutputValidity validity = asc::LapackOutputValidity::kComplete);
std::size_t Cases();

template <typename T, std::size_t Size>
auto Vector(std::array<T, Size>& storage, int size) {
  return Take(asc::DenseBlasVectorView<T>::Create(
      storage.data() + 1, size, 1, {storage.data(), sizeof(storage), kHost}));
}

template <typename T>
struct Scratch {
  using Real = asc::DenseBlasRealType<T>;
  std::array<T, 200> scalar;
  std::array<Real, 200> real;
  std::array<T, 512> packing;
  alignas(std::max_align_t) std::array<std::byte, 2048> integers;
  std::array<std::size_t, 4> used{};
  Scratch() {
    scalar.fill(Value<T>(-811, 41));
    real.fill(Real{-821});
    packing.fill(Value<T>(-823, 43));
    integers.fill(std::byte{0x5a});
  }
  auto Workspace(const asc::LapackWorkspacePlan& plan) {
    asc::LapackWorkspace work;
    used = {static_cast<std::size_t>(plan.regions[kScalar].minimum_entries),
            static_cast<std::size_t>(plan.regions[kReal].minimum_entries),
            static_cast<std::size_t>(plan.regions[kLayout].minimum_entries),
            static_cast<std::size_t>(plan.regions[kInteger].minimum_entries) *
                plan.regions[kInteger].entry_bytes};
    if (used[0] >= scalar.size() - 1 || used[1] >= real.size() - 1 ||
        used[2] >= packing.size() - 1 || used[3] > integers.size() - 32) {
      std::abort();
    }
    if (used[0] != 0) {
      work.regions[kScalar] = {scalar.data() + 1, used[0] * sizeof(T), kHost};
    }
    if (used[1] != 0) {
      work.regions[kReal] = {real.data() + 1, used[1] * sizeof(Real), kHost};
    }
    if (used[2] != 0) {
      work.regions[kLayout] = {packing.data() + 1, used[2] * sizeof(T), kHost};
    }
    if (used[3] != 0) {
      work.regions[kInteger] = {integers.data() + 16, used[3], kHost};
    }
    return work;
  }
  void Guards(TestContext& test) const {
    for (std::size_t i = 0; i < scalar.size(); ++i) {
      if (i == 0 || i > used[0]) {
        ASC_DENSE_TEST_EQ(test, scalar[i], Value<T>(-811, 41));
      }
    }
    for (std::size_t i = 0; i < real.size(); ++i) {
      if (i == 0 || i > used[1]) {
        ASC_DENSE_TEST_EQ(test, real[i], Real{-821});
      }
    }
    for (std::size_t i = 0; i < packing.size(); ++i) {
      if (i == 0 || i > used[2]) {
        ASC_DENSE_TEST_EQ(test, packing[i], Value<T>(-823, 43));
      }
    }
    for (std::size_t i = 0; i < integers.size(); ++i) {
      if (i < 16 || i >= 16 + used[3]) {
        ASC_DENSE_TEST_EQ(test, integers[i], std::byte{0x5a});
      }
    }
  }
};
}  // namespace asc_lu_aux_info_test
#endif  // ASC_TESTS_DENSE_LAPACK_LU_AUX_INFO_TEST_SUPPORT_H_
