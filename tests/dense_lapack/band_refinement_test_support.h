#ifndef ASC_TESTS_DENSE_LAPACK_BAND_REFINEMENT_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_BAND_REFINEMENT_TEST_SUPPORT_H_

#include <array>
#include <cstddef>
#include <vector>

#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_band_refinement.h"
#include "band_cholesky_test_support.h"

namespace asc_band_refinement_test {
inline constexpr std::array<asc::MemorySpace, 6> kHostSpaces{
    asc::MemorySpace::kHost, asc::MemorySpace::kHost, asc::MemorySpace::kHost,
    asc::MemorySpace::kHost, asc::MemorySpace::kHost, asc::MemorySpace::kHost};

template <typename T>
struct Fixture {
  using Real = asc::DenseBlasRealType<T>;
  asc_band_test::BandData<T> a;
  asc_band_test::BandData<T> af;
  asc_band_test::RhsData<T> b;
  asc_band_test::RhsData<T> x;
  std::vector<Real> ferr;
  std::vector<Real> berr;
  asc::extent_t ferr_count;
  asc::extent_t berr_count;
  asc::stride_t ferr_stride = 1;
  asc::stride_t berr_stride = 1;

  Fixture(asc::extent_t n, asc::extent_t kd, asc::extent_t nrhs,
          asc::DenseBlasTriangle triangle,
          const std::array<asc::DenseBlasLayout, 4>& layouts)
      : a(n, kd, triangle, layouts[0]),
        af(n, kd, triangle, layouts[1]),
        b(a, nrhs, layouts[2]),
        x(a, nrhs, layouts[3]),
        ferr(static_cast<std::size_t>(nrhs + 2), Real{-317}),
        berr(static_cast<std::size_t>(nrhs + 2), Real{-331}),
        ferr_count(nrhs),
        berr_count(nrhs) {}
  [[nodiscard]] auto Rhs(asc::MemorySpace space = asc_band_test::kHost) const {
    return asc_band_test::Take(asc::DenseBlasMatrixView<const T>::Create(
        b.values.data() + 1, b.n, b.count, b.layout, b.ld,
        {b.values.data(), b.values.size() * sizeof(T), space}));
  }
  auto Forward(asc::MemorySpace space = asc_band_test::kHost) {
    return asc_band_test::Take(asc::DenseBlasVectorView<Real>::Create(
        ferr.data() + 1, ferr_count, ferr_stride,
        {ferr.data(), ferr.size() * sizeof(Real), space}));
  }
  auto Backward(asc::MemorySpace space = asc_band_test::kHost) {
    return asc_band_test::Take(asc::DenseBlasVectorView<Real>::Create(
        berr.data() + 1, berr_count, berr_stride,
        {berr.data(), berr.size() * sizeof(Real), space}));
  }
  auto Query(const asc::ReferenceLapackProvider& provider,
             const std::array<asc::MemorySpace, 6>& spaces = kHostSpaces) {
    return asc::QueryPbrfsWorkspace(provider, a.ConstView(spaces[0]),
                                    af.ConstView(spaces[1]), Rhs(spaces[2]),
                                    x.View(spaces[3]), Forward(spaces[4]),
                                    Backward(spaces[5]));
  }
  asc::Status Execute(
      const asc::ReferenceLapackProvider& provider,
      const asc::LapackWorkspacePlan& plan,
      const asc::LapackWorkspace& workspace, asc::LapackReport& report,
      const std::array<asc::MemorySpace, 6>& spaces = kHostSpaces) {
    return asc::Pbrfs(provider, a.ConstView(spaces[0]), af.ConstView(spaces[1]),
                      Rhs(spaces[2]), x.View(spaces[3]), Forward(spaces[4]),
                      Backward(spaces[5]), plan, workspace, report);
  }
};
}  // namespace asc_band_refinement_test
#endif  // ASC_TESTS_DENSE_LAPACK_BAND_REFINEMENT_TEST_SUPPORT_H_
