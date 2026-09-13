#ifndef ASC_TESTS_DENSE_LAPACK_LU_BAND_FACTOR_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_LU_BAND_FACTOR_TEST_SUPPORT_H_

#include <string_view>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu_band.h"

namespace asc_lu_band_test {
// Set once by the test process before running its unchanged numerical oracles.
inline bool g_unblocked = false;
inline bool SelectFactor(int argc, char** argv) {
  if (argc == 2) {
    g_unblocked = false;
    return true;
  }
  if (argc == 3 && std::string_view(argv[2]) == "gbtf2") {
    g_unblocked = true;
    return true;
  }
  return false;
}

template <typename T>
asc::Result<asc::LapackWorkspacePlan> QueryFactor(
    const asc::ReferenceLapackProvider& provider, asc::LapackLuBandView<T> band,
    asc::DenseBlasVectorView<asc::index_t> pivots) {
  return g_unblocked ? asc::QueryGbtf2Workspace(provider, band, pivots)
                     : asc::QueryGbtrfWorkspace(provider, band, pivots);
}

template <typename T>
asc::Status Factor(const asc::ReferenceLapackProvider& provider,
                   asc::LapackLuBandView<T> band,
                   asc::DenseBlasVectorView<asc::index_t> pivots,
                   const asc::LapackWorkspacePlan& plan,
                   const asc::LapackWorkspace& workspace,
                   asc::LapackReport& report) {
  return g_unblocked
             ? asc::Gbtf2(provider, band, pivots, plan, workspace, report)
             : asc::Gbtrf(provider, band, pivots, plan, workspace, report);
}
}  // namespace asc_lu_band_test

#endif  // ASC_TESTS_DENSE_LAPACK_LU_BAND_FACTOR_TEST_SUPPORT_H_
