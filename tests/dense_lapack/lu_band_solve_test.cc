#include <array>
#include <complex>
#include <cstdio>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu_band_driver.h"
#include "installed_lu/normal_return_guard.h"
#include "lu_band_expert_test_support.h"
#include "lu_band_test_support.h"
namespace {
namespace support = asc_lu_band_expert_test;
namespace base = asc_lu_band_test;
using support::Take;
using support::TestContext;
template <typename T>
void One(TestContext& test, const asc::ReferenceLapackProvider& provider,
         base::Band<T> band, asc::DenseBlasLayout layout, asc::extent_t nrhs,
         bool singular = false) {
  if (singular) {
    for (asc::extent_t i = 0; i < band.n; ++i) {
      for (asc::extent_t j = 0; j < band.n; ++j) {
        if (i >= j - band.ku && i <= j + band.kl) {
          band.Put(i, j, support::Value<T>(i == j && i != band.n - 1 ? 2 : 0));
        }
      }
    }
  }
  base::Rhs<T> rhs(band, nrhs, layout, asc::DenseBlasTranspose::kNone);
  base::Pivots pivots(band.n);
  const auto before = band.values;
  const auto b_before = rhs.values;
  const auto matrix = band.View();
  const auto swaps = pivots.View();
  const auto b = rhs.View();
  const auto plan = Take(support::WithoutAllocation(test, [&] {
    return asc::QueryGbsvWorkspace(provider, matrix, swaps, b);
  }));
  support::Scratch<T> scratch(plan);
  const auto workspace = scratch.View();
  asc::LapackReport report;
  const auto status = support::WithoutAllocation(test, [&] {
    return asc::Gbsv(provider, matrix, swaps, b, plan, workspace, report);
  });
  ASC_DENSE_TEST_CHECK(
      test, report.called_provider && report.native_info.has_value());
  ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
  band.Padding(test, before);
  pivots.Check(test);
  scratch.Check(test);
  band.Reconstruct(test, pivots);
  if (singular) {
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(0), band.n);
    ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-1), band.n - 1);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kDocumentedPartial);
    ASC_DENSE_TEST_CHECK(test, support::SameBytes(rhs.values, b_before));
  } else {
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), 0);
    rhs.Check(test, band, b_before);
  }
}
template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  const int exponent = sizeof(Real) == sizeof(float) ? 100 : 800;
  std::size_t count = 0;
  for (const auto shape :
       {std::array{0, 0, 0}, std::array{0, 5, 6}, std::array{1, 0, 0},
        std::array{1, 5, 6}, std::array{3, 2, 2}, std::array{7, 2, 1},
        std::array{70, 32, 65}}) {
    for (const int power : {0, -exponent, exponent}) {
      for (const auto layout : {base::kColumn, base::kRow}) {
        for (const asc::extent_t nrhs : {0, 1, 3}) {
          One(test, provider,
              base::Band<T>(shape[0], shape[0], shape[1], shape[2], power),
              layout, nrhs);
          ++count;
        }
      }
    }
  }
  for (const auto layout : {base::kColumn, base::kRow}) {
    for (const asc::extent_t nrhs : {0, 1, 3}) {
      One(test, provider, base::Band<T>(7, 7, 2, 1), layout, nrhs, true);
    }
  }
  std::printf(
      "%zu independent GBSV reconstruction/solve profiles plus six last-pivot "
      "singular retained-B checks\n",
      count);
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
