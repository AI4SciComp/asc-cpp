#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_TWO_STAGE_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_TWO_STAGE_TEST_SUPPORT_H_
#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <vector>

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite_aasen_two_stage.h"
#include "indefinite_rook_test_support.h"
namespace asc_aasen_two_stage_test {
namespace base = asc_indefinite_rook_test;
template <typename T>
auto Query(const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasTriangle tri, bool he, asc::DenseBlasMatrixView<T> a,
           asc::DenseBlasVectorView<T> tb,
           asc::DenseBlasVectorView<asc::index_t> p,
           asc::DenseBlasVectorView<asc::index_t> q) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (he) {
      return asc::QueryHetrfAa2StageWorkspace(provider, tri, a, tb, p, q);
    }
  }
  return asc::QuerySytrfAa2StageWorkspace(provider, tri, a, tb, p, q);
}
template <typename T>
asc::Status Factor(const asc::ReferenceLapackProvider& provider,
                   asc::DenseBlasTriangle tri, bool he,
                   asc::DenseBlasMatrixView<T> a,
                   asc::DenseBlasVectorView<T> tb,
                   asc::DenseBlasVectorView<asc::index_t> p,
                   asc::DenseBlasVectorView<asc::index_t> q,
                   const asc::LapackWorkspacePlan& plan,
                   const asc::LapackWorkspace& workspace,
                   asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (he) {
      return asc::HetrfAa2Stage(provider, tri, a, tb, p, q, plan, workspace,
                                report);
    }
  }
  return asc::SytrfAa2Stage(provider, tri, a, tb, p, q, plan, workspace,
                            report);
}
template <typename T>
auto Vector(std::vector<T>& values, asc::extent_t count) {
  return base::Take(asc::DenseBlasVectorView<T>::Create(
      values.data() + 1, count, 1,
      {values.data(), values.size() * sizeof(T), base::kHost}));
}
template <typename T>
struct Scratch {
  std::vector<T> scalar;
  std::vector<T> packed;
  alignas(16) std::array<std::byte, 4096> integers;
  asc::LapackWorkspace workspace;
  Scratch(const asc::LapackWorkspacePlan& plan, asc::extent_t entries)
      : scalar(static_cast<std::size_t>(entries + 2), base::Value<T>(-107, 11)),
        packed(static_cast<std::size_t>(
                   plan.regions[base::kLayout].preferred_entries + 2),
               base::Value<T>(-109, 13)) {
    integers.fill(std::byte{0x5a});
    const auto bytes =
        static_cast<std::size_t>(plan.regions[base::kPivot].preferred_entries) *
        plan.regions[base::kPivot].entry_bytes;
    if (bytes > integers.size() - 32) {
      std::abort();
    }
    if (entries != 0) {
      workspace.regions[base::kScalar] = {
          scalar.data() + 1, static_cast<std::size_t>(entries) * sizeof(T),
          base::kHost};
    }
    if (packed.size() > 2) {
      workspace.regions[base::kLayout] = {
          packed.data() + 1, (packed.size() - 2) * sizeof(T), base::kHost};
    }
    if (bytes != 0) {
      workspace.regions[base::kPivot] = {integers.data() + 16, bytes,
                                         base::kHost};
    }
  }
  void Guards(base::TestContext& test) const {
    ASC_DENSE_TEST_EQ(test, scalar.front(), base::Value<T>(-107, 11));
    ASC_DENSE_TEST_EQ(test, scalar.back(), base::Value<T>(-107, 11));
    ASC_DENSE_TEST_EQ(test, packed.front(), base::Value<T>(-109, 13));
    ASC_DENSE_TEST_EQ(test, packed.back(), base::Value<T>(-109, 13));
    for (std::size_t i = 0; i < 16; ++i) {
      ASC_DENSE_TEST_EQ(test, integers[i], std::byte{0x5a});
    }
    for (std::size_t i = 16 + workspace.regions[base::kPivot].size();
         i < integers.size(); ++i) {
      ASC_DENSE_TEST_EQ(test, integers[i], std::byte{0x5a});
    }
  }
};
}  // namespace asc_aasen_two_stage_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_TWO_STAGE_TEST_SUPPORT_H_
