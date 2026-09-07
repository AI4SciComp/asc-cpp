#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>
#include <string_view>
#include <vector>

#include "../../src/dense/lapack/internal_band_abi.h"
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_band.h"
#include "band_cholesky_native_test_support.h"
#include "band_cholesky_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
using namespace asc_band_test;  // NOLINT(google-build-using-namespace)

template <typename T>
std::vector<T> ColumnCopy(const BandData<T>& band) {
  std::vector<T> result(static_cast<std::size_t>(band.n * band.ld),
                        Value<T>(-79, 31));
  for (asc::extent_t i = 0; i < band.n; ++i) {
    for (asc::extent_t j = 0; j < band.n; ++j) {
      if (band.Selected(i, j)) {
        const auto offset = band.triangle == kUpper ? band.kd + i - j : i - j;
        result[static_cast<std::size_t>(j * band.ld + offset)] =
            band.values[band.Index(i, j)];
      }
    }
  }
  return result;
}

template <typename T>
void FactorCase(TestContext& test, const asc::ReferenceLapackProvider& provider,
                asc::extent_t n, asc::extent_t kd,
                asc::DenseBlasTriangle triangle, asc::DenseBlasLayout layout,
                bool blocked, bool nan, asc::extent_t failed_index) {
  BandData<T> band(n, kd, triangle, layout);
  // Make a diagonal matrix. Its failure index and all successful leading
  // factors are known independently of the direct provider comparison.
  for (asc::extent_t i = 0; i < n; ++i) {
    for (asc::extent_t j = 0; j < n; ++j) {
      if (band.Selected(i, j)) {
        band.values[band.Index(i, j)] =
            Value<T>(i == j ? 4 : 0, i == j ? 53 : 0);
      }
    }
  }
  const auto bad = nan ? std::numeric_limits<long double>::quiet_NaN() : -1.0L;
  band.values[band.Index(failed_index, failed_index)] = Value<T>(bad, 53);
  const auto before = band.values;
  auto direct = ColumnCopy(band);
  lapack_int size = static_cast<lapack_int>(n);
  lapack_int width = static_cast<lapack_int>(kd);
  lapack_int ld = static_cast<lapack_int>(band.ld);
  char uplo = triangle == kUpper ? 'U' : 'L';
  std::array<lapack_int, 3> info{137, 117, 139};
  WithoutAllocation(test, [&] {
    if (blocked) {
      Native<T>::kFactor(&uplo, &size, &width, direct.data(), &ld, &info[1], 1);
    } else {
      Native<T>::kUnblocked(&uplo, &size, &width, direct.data(), &ld, &info[1],
                            1);
    }
    return true;
  });
  const auto plan =
      Take(blocked ? asc::QueryPbtrfWorkspace(provider, band.View())
                   : asc::QueryPbtf2Workspace(provider, band.View()));
  Storage<T> storage(plan);
  auto workspace = storage.View();
  asc::LapackReport report;
  const auto status = WithoutAllocation(test, [&] {
    return blocked ? asc::Pbtrf(provider, band.View(), plan, workspace, report)
                   : asc::Pbtf2(provider, band.View(), plan, workspace, report);
  });
  ASC_DENSE_TEST_EQ(test, info.front(), 137);
  ASC_DENSE_TEST_EQ(test, info.back(), 139);
  const asc::extent_t expected_info =
      nan && (!blocked || kd <= 64) ? 0 : failed_index + 1;
  ASC_DENSE_TEST_EQ(test, info[1], expected_info);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), info[1]);
  ASC_DENSE_TEST_EQ(test, status.ok(), expected_info == 0);
  if (expected_info != 0) {
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      asc::LapackOutcome::kNotPositiveDefinite);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kDocumentedPartial);
    ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-1), failed_index);
  } else {
    // Raw NaN INFO=0 is fidelity evidence only, not mathematical success.
    ASC_DENSE_TEST_CHECK(
        test,
        std::isnan(ToWide(band.values[band.Index(failed_index, failed_index)])
                       .real()));
  }
  for (asc::extent_t i = 0; i < n; ++i) {
    for (asc::extent_t j = 0; j < n; ++j) {
      if (band.Selected(i, j)) {
        const auto offset = triangle == kUpper ? kd + i - j : i - j;
        const auto& reference =
            direct[static_cast<std::size_t>(j * band.ld + offset)];
        ASC_DENSE_TEST_CHECK(
            test, SameScalarBytes(reference, band.values[band.Index(i, j)]));
      }
    }
  }
  for (asc::extent_t i = 0; i < failed_index; ++i) {
    ASC_DENSE_TEST_EQ(test, band.values[band.Index(i, i)], Value<T>(2));
  }
  band.CheckPadding(test, before);
  storage.Check(test);
}

template <typename T>
void MakeRawBand(BandData<T>& band) {
  const auto triangle = band.triangle;
  for (asc::extent_t i = 0; i < band.n; ++i) {
    for (asc::extent_t j = 0; j < band.n; ++j) {
      if (band.Selected(i, j)) {
        band.values[band.Index(i, j)] =
            Value<T>(i == j ? -2 : 0.125L, i == j ? 0.5L : -0.25L);
      }
    }
  }
  // Raw triangular factors may have negative/nonreal diagonals. Reconstruct
  // their actual Hermitian product; do not silently normalize those components.
  for (asc::extent_t i = 0; i < band.n; ++i) {
    for (asc::extent_t j = 0; j < band.n; ++j) {
      Wide value{};
      for (asc::extent_t k = 0; k < band.n; ++k) {
        value += triangle == kUpper
                     ? std::conj(band.Factor(k, i)) * band.Factor(k, j)
                     : band.Factor(i, k) * std::conj(band.Factor(j, k));
      }
      band.original[static_cast<std::size_t>(i * band.n + j)] = value;
    }
  }
}

template <typename T>
void RawSolve(TestContext& test, const asc::ReferenceLapackProvider& provider,
              asc::DenseBlasTriangle triangle, asc::DenseBlasLayout layout,
              asc::DenseBlasLayout rhs_layout, int mode) {
  BandData<T> band(2, 1, triangle, layout);
  MakeRawBand(band);
  RhsData<T> rhs(band, 2, rhs_layout);
  if (mode != 0) {
    const auto bad =
        mode == 1 ? 0.0L : std::numeric_limits<long double>::quiet_NaN();
    band.values[band.Index(0, 0)] = Value<T>(bad);
    // Nonzero RHS forces TBSV's guarded division for a zero raw diagonal.
    for (asc::extent_t i = 0; i < 2; ++i) {
      for (asc::extent_t j = 0; j < 2; ++j) {
        rhs.values[rhs.Index(i, j)] = Value<T>(1);
      }
    }
  }
  const auto band_before = band.values;
  const auto rhs_before = rhs.values;
  auto direct_factor = ColumnCopy(band);
  std::array<T, 4> direct_rhs{};
  for (asc::extent_t i = 0; i < 2; ++i) {
    for (asc::extent_t j = 0; j < 2; ++j) {
      direct_rhs[static_cast<std::size_t>(i + 2 * j)] =
          rhs.values[rhs.Index(i, j)];
    }
  }
  const lapack_int n = 2;
  const lapack_int kd = 1;
  const lapack_int nrhs = 2;
  const lapack_int ld = static_cast<lapack_int>(band.ld);
  const lapack_int ldb = 2;
  const char uplo = triangle == kUpper ? 'U' : 'L';
  std::array<lapack_int, 3> info{137, 117, 139};
  WithoutAllocation(test, [&] {
    Native<T>::kSolve(&uplo, &n, &kd, &nrhs, direct_factor.data(), &ld,
                      direct_rhs.data(), &ldb, &info[1], 1);
    return true;
  });
  const auto plan =
      Take(asc::QueryPbtrsWorkspace(provider, band.ConstView(), rhs.View()));
  Storage<T> storage(plan);
  auto workspace = storage.View();
  asc::LapackReport report;
  const auto status = WithoutAllocation(test, [&] {
    return asc::Pbtrs(provider, band.ConstView(), rhs.View(), plan, workspace,
                      report);
  });
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_EQ(test, info[1], 0);
  ASC_DENSE_TEST_EQ(test, info.front(), 137);
  ASC_DENSE_TEST_EQ(test, info.back(), 139);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), 0);
  bool nonfinite = false;
  for (asc::extent_t i = 0; i < 2; ++i) {
    for (asc::extent_t j = 0; j < 2; ++j) {
      const auto& reference = direct_rhs[static_cast<std::size_t>(i + 2 * j)];
      const auto& actual = rhs.values[rhs.Index(i, j)];
      ASC_DENSE_TEST_CHECK(test, SameScalarBytes(reference, actual));
      nonfinite = nonfinite || !std::isfinite(std::abs(ToWide(actual)));
    }
  }
  if (mode == 0) {
    rhs.Check(test, band, rhs_before);
  } else {
    ASC_DENSE_TEST_CHECK(test, nonfinite);
  }
  ASC_DENSE_TEST_CHECK(test, SameBytes(band_before, band.values));
  storage.Check(test);
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto layout : {kColumn, kRow}) {
      for (const bool blocked : {false, true}) {
        for (const asc::extent_t index : {0, 5}) {
          FactorCase<T>(test, provider, 6, 2, triangle, layout, blocked, false,
                        index);
        }
        for (const asc::extent_t index : {0, 47, 95}) {
          FactorCase<T>(test, provider, 96, 65, triangle, layout, blocked,
                        false, index);
        }
        // Exact block-boundary failures with untouched trailing imaginary
        // diagonal sentinels beyond f+kd. Zero off-diagonals still invoke
        // HER/HERK's source-defined diagonal normalization.
        for (const asc::extent_t index : {31, 32, 63, 64}) {
          FactorCase<T>(test, provider, 136, 65, triangle, layout, blocked,
                        false, index);
        }
        for (const asc::extent_t kd : {0, 65}) {
          FactorCase<T>(test, provider, 1, kd, triangle, layout, blocked, true,
                        0);
        }
      }
      for (const auto rhs_layout : {kColumn, kRow}) {
        for (const int mode : {0, 1, 2}) {
          RawSolve<T>(test, provider, triangle, layout, rhs_layout, mode);
        }
      }
    }
  }
}
}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 2) {
    return 2;
  }
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    Run<float>(test, provider);
  } else if (scalar == "d") {
    Run<double>(test, provider);
  } else if (scalar == "c") {
    Run<std::complex<float>>(test, provider);
  } else if (scalar == "z") {
    Run<std::complex<double>>(test, provider);
  } else {
    return 2;
  }
  return test.Finish();
}
