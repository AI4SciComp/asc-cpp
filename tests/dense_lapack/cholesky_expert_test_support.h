#ifndef ASC_TESTS_DENSE_LAPACK_CHOLESKY_EXPERT_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_CHOLESKY_EXPERT_TEST_SUPPORT_H_

#include <array>
#include <cstddef>
#include <cstdint>

#include "../dense/test_support.h"
#include "asc/core/contracts.h"
#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "cholesky_test_support.h"

namespace asc_cholesky_expert_test {
using asc_cholesky_test::Take;
using asc_cholesky_test::TestContext;
constexpr auto kScalar =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kScalar);
constexpr auto kReal =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kReal);
constexpr auto kInteger =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kInteger);
constexpr auto kPacking =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);

template <typename T, std::size_t N>
asc::DenseBlasVectorView<T> Vector(std::array<T, N>& storage,
                                   asc::extent_t count) {
  return Take(asc::DenseBlasVectorView<T>::Create(
      storage.data(), count, 1,
      {storage.data(), sizeof(storage), asc::MemorySpace::kHost}));
}

template <typename T>
struct Scratch {
  std::array<T, 256> scalar{};
  std::array<asc::DenseBlasRealType<T>, 128> real{};
  alignas(std::int64_t) std::array<std::byte, 1024> integer{};
  std::array<T, 4 * asc_cholesky_test::kCapacity> packing{};

  asc::LapackWorkspace view(const asc::LapackWorkspacePlan& plan) {
    asc::LapackWorkspace result;
    const std::array<asc::MutableMemoryView, 4> memories{
        asc::MutableMemoryView(scalar.data(), sizeof(scalar),
                               asc::MemorySpace::kHost),
        asc::MutableMemoryView(real.data(), sizeof(real),
                               asc::MemorySpace::kHost),
        asc::MutableMemoryView(integer.data(), sizeof(integer),
                               asc::MemorySpace::kHost),
        asc::MutableMemoryView(packing.data(), sizeof(packing),
                               asc::MemorySpace::kHost)};
    constexpr std::array kKinds{kScalar, kReal, kInteger, kPacking};
    for (std::size_t i = 0; i < kKinds.size(); ++i) {
      const auto kind = kKinds[i];
      const auto bytes =
          static_cast<std::size_t>(plan.regions[kind].minimum_entries) *
          plan.regions[kind].entry_bytes;
      ASC_CHECK(bytes <= memories[i].size());
      result.regions[kind] = {memories[i].data(), bytes,
                              asc::MemorySpace::kHost};
    }
    return result;
  }
};

inline void CheckSuccess(TestContext& test,
                         const asc::ReferenceLapackProvider& provider,
                         const asc::LapackReport& report, bool called) {
  ASC_DENSE_TEST_EQ(test, report.provider, provider.identity());
  ASC_DENSE_TEST_EQ(test, report.called_provider, called);
  ASC_DENSE_TEST_EQ(test, report.native_info.has_value(), called);
  if (called) {
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-9999), 0);
  }
  ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSuccess);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kComplete);
}

}  // namespace asc_cholesky_expert_test

#endif  // ASC_TESTS_DENSE_LAPACK_CHOLESKY_EXPERT_TEST_SUPPORT_H_
