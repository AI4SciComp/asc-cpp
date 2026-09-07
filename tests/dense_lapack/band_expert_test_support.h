#ifndef ASC_TESTS_DENSE_LAPACK_BAND_EXPERT_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_BAND_EXPERT_TEST_SUPPORT_H_

#include <array>
#include <cstddef>
#include <vector>

#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_band_expert.h"
#include "asc/dense/providers/lapack_cholesky_driver.h"
#include "band_cholesky_test_support.h"
#include "band_refinement_test_support.h"

namespace asc_band_driver_test {
inline constexpr std::array<asc::MemorySpace, 7> kHostSpaces{
    asc::MemorySpace::kHost, asc::MemorySpace::kHost, asc::MemorySpace::kHost,
    asc::MemorySpace::kHost, asc::MemorySpace::kHost, asc::MemorySpace::kHost,
    asc::MemorySpace::kHost};
template <typename T>
struct Fixture {
  using Real = asc::DenseBlasRealType<T>;
  asc_band_refinement_test::Fixture<T> data;
  std::vector<Real> scales;
  asc::extent_t scale_count;
  Real reciprocal_condition = -401;
  asc::LapackCholeskyEquilibration equilibration =
      asc::LapackCholeskyEquilibration::kNone;
  char mode;
  std::array<asc::MemorySpace, 7> spaces = kHostSpaces;
  asc::stride_t scale_stride = 1;

  Fixture(asc::extent_t n, asc::extent_t kd, asc::extent_t nrhs,
          asc::DenseBlasTriangle triangle,
          const std::array<asc::DenseBlasLayout, 4>& layouts, char fact)
      : data(n, kd, nrhs, triangle, layouts),
        scales(static_cast<std::size_t>(n + 2), Real{-409}),
        scale_count(n),
        mode(fact) {}
  auto Scales() {
    return asc_band_test::Take(asc::DenseBlasVectorView<Real>::Create(
        scales.data() + 1, scale_count, scale_stride,
        {scales.data(), scales.size() * sizeof(Real), spaces[6]}));
  }
  [[nodiscard]] auto ConstScales() const {
    return asc_band_test::Take(asc::DenseBlasVectorView<const Real>::Create(
        scales.data() + 1, scale_count, scale_stride,
        {scales.data(), scales.size() * sizeof(Real), spaces[6]}));
  }
  auto Query(const asc::ReferenceLapackProvider& provider) {
    if (mode == 'N') {
      return asc::QueryPbsvxWorkspace(
          provider, data.a.ConstView(spaces[0]), data.af.View(spaces[1]),
          data.Rhs(spaces[2]), data.x.View(spaces[3]), data.Forward(spaces[4]),
          data.Backward(spaces[5]), reciprocal_condition);
    }
    if (mode == 'E') {
      return asc::QueryPbsvxEquilibratedWorkspace(
          provider, data.a.View(spaces[0]), data.af.View(spaces[1]),
          equilibration, Scales(), data.b.View(spaces[2]),
          data.x.View(spaces[3]), data.Forward(spaces[4]),
          data.Backward(spaces[5]), reciprocal_condition);
    }
    return asc::QueryPbsvxFactoredWorkspace(
        provider, data.a.ConstView(spaces[0]), data.af.ConstView(spaces[1]),
        equilibration, ConstScales(), data.b.View(spaces[2]),
        data.x.View(spaces[3]), data.Forward(spaces[4]),
        data.Backward(spaces[5]), reciprocal_condition);
  }
  auto Execute(const asc::ReferenceLapackProvider& provider,
               const asc::LapackWorkspacePlan& plan,
               const asc::LapackWorkspace& workspace,
               asc::LapackReport& report) {
    if (mode == 'N') {
      return asc::Pbsvx(provider, data.a.ConstView(spaces[0]),
                        data.af.View(spaces[1]), data.Rhs(spaces[2]),
                        data.x.View(spaces[3]), data.Forward(spaces[4]),
                        data.Backward(spaces[5]), reciprocal_condition, plan,
                        workspace, report);
    }
    if (mode == 'E') {
      return asc::PbsvxEquilibrated(
          provider, data.a.View(spaces[0]), data.af.View(spaces[1]),
          equilibration, Scales(), data.b.View(spaces[2]),
          data.x.View(spaces[3]), data.Forward(spaces[4]),
          data.Backward(spaces[5]), reciprocal_condition, plan, workspace,
          report);
    }
    return asc::PbsvxFactored(provider, data.a.ConstView(spaces[0]),
                              data.af.ConstView(spaces[1]), equilibration,
                              ConstScales(), data.b.View(spaces[2]),
                              data.x.View(spaces[3]), data.Forward(spaces[4]),
                              data.Backward(spaces[5]), reciprocal_condition,
                              plan, workspace, report);
  }
};
}  // namespace asc_band_driver_test
#endif  // ASC_TESTS_DENSE_LAPACK_BAND_EXPERT_TEST_SUPPORT_H_
