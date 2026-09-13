#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_rk_driver_faults.h"
#include "indefinite_rk_driver_test_support.h"
#include "indefinite_rk_test_support.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"
#include "lapack_build_config.h"

namespace {
namespace base = asc_indefinite_rook_test;
namespace faults = asc_rk_driver_fault_test;
using base::TestContext;
using faults::Fault;

std::size_t Offset(asc::DenseBlasLayout storage, int i, int j) {
  return 1U + (storage == base::kColumn ? static_cast<std::size_t>(j) * 3 + i
                                        : static_cast<std::size_t>(i) * 3 + j);
}

bool Outcome(TestContext& test, Fault fault, int n, const asc::Status& result,
             const asc::LapackReport& report) {
  const bool pass =
      fault == Fault::kPass ||
      (ASC_LAPACK_INTEGER_BITS == 32 &&
       (fault == Fault::kWrite32BitInfo || fault == Fault::kWrite32BitPivots));
  ASC_DENSE_TEST_EQ(test, faults::Calls(), 1U);
  ASC_DENSE_TEST_EQ(test, faults::LastNativeInfo(), 0);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.factor_family, asc::LapackFactorFamily::kRook);
  ASC_DENSE_TEST_EQ(test, result.code(),
                    pass ? asc::ErrorCode::kOk : asc::ErrorCode::kProvider);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    pass ? asc::LapackOutputValidity::kComplete
                         : asc::LapackOutputValidity::kUnusable);
  std::int64_t expected_info = 0;
  if (fault == Fault::kOmitInfo || (fault == Fault::kWrite32BitInfo && !pass)) {
    expected_info = ASC_LAPACK_INTEGER_BITS == 32
                        ? std::int64_t{std::numeric_limits<std::int32_t>::min()}
                        : std::numeric_limits<std::int64_t>::min();
  } else if (fault == Fault::kNegativeInfo) {
    expected_info = -10;
  } else if (fault == Fault::kPositiveInfo) {
    expected_info = n + 1;
  } else if (fault == Fault::kInconsistentPositiveInfo) {
    expected_info = 1;
  }
  ASC_DENSE_TEST_CHECK(test, report.native_info.has_value());
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(17), expected_info);
  return pass;
}

template <typename T>
void Guards(TestContext& test, const std::array<T, 12>& a,
            const std::array<T, 12>& old_a, const std::array<T, 12>& b,
            const std::array<T, 12>& old_b,
            const std::array<asc::index_t, 4>& pivots,
            asc::DenseBlasLayout layout, asc::DenseBlasLayout rhs_layout, int n,
            int nrhs, asc::DenseBlasTriangle triangle) {
  // Direct native buffers may change on a defect, but gaps and ignored data
  // remain guarded in both layouts. Factor-only calls never reference B.
  for (std::size_t k = 0; k < a.size(); ++k) {
    bool selected_a = false;
    bool selected_b = false;
    for (int j = 0; j < n; ++j) {
      for (int i = 0; i < n; ++i) {
        selected_a =
            selected_a || (k == Offset(layout, i, j) &&
                           (triangle == base::kUpper ? i <= j : i >= j));
      }
    }
    for (int j = 0; j < nrhs; ++j) {
      for (int i = 0; i < n; ++i) {
        selected_b = selected_b || k == Offset(rhs_layout, i, j);
      }
    }
    if (!selected_a) {
      ASC_DENSE_TEST_CHECK(test, base::EqualBytes(&a[k], &old_a[k], sizeof(T)));
    }
    if (!selected_b) {
      ASC_DENSE_TEST_EQ(test, b[k], old_b[k]);
    }
  }
  ASC_DENSE_TEST_EQ(test, pivots.front(), -97);
  for (std::size_t k = static_cast<std::size_t>(n) + 1; k < pivots.size();
       ++k) {
    ASC_DENSE_TEST_EQ(test, pivots[k], -97);
  }
}

template <typename T>
void Check(TestContext& test, const asc::ReferenceLapackProvider& provider,
           bool hermitian, asc::DenseBlasTriangle triangle,
           asc::DenseBlasLayout layout, asc::DenseBlasLayout rhs_layout, int n,
           int nrhs, bool preferred, Fault fault) {
  std::array<T, 12> a;
  std::array<T, 12> b;
  std::array<T, 4> e;
  e.fill(T{-101});
  std::array<asc::index_t, 4> pivots;
  a.fill(base::Value<T>(-91, 7));
  b.fill(base::Value<T>(-93, 9));
  pivots.fill(-97);
  for (int j = 0; j < n; ++j) {
    for (int i = 0; i < n; ++i) {
      const bool selected = triangle == base::kUpper ? i <= j : i >= j;
      if (selected) {
        a[Offset(layout, i, j)] = i == j ? T{4} : T{1};
        if constexpr (asc::DenseBlasComplex<T>) {
          if (hermitian && i == j) {
            a[Offset(layout, i, j)].imag(
                std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN());
          }
        }
      }
    }
  }
  for (int j = 0; j < nrhs; ++j) {
    for (int i = 0; i < n; ++i) {
      b[Offset(rhs_layout, i, j)] = T{2};
    }
  }
  const auto old_a = a;
  const auto old_b = b;
  const auto old_pivots = pivots;
  auto matrix = base::Matrix(a, n, n, layout, 3);
  auto rhs = base::Matrix(b, n, nrhs, rhs_layout, 3);
  auto pivot = base::Pivots(pivots, n);
  const auto extra = asc_rk_test::OffDiagonal(e, n);
  const auto plan = base::Take(asc_rk_driver_test::Query(
      provider, triangle, hermitian, matrix, extra, pivot, rhs));
  base::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan, preferred ? -1 : 1);
  asc::LapackReport report;
  faults::SetFault(fault);
  const auto result = base::WithoutAllocation(test, [&] {
    return asc_rk_driver_test::Driver(provider, triangle, hermitian, matrix,
                                      extra, pivot, rhs, plan, workspace,
                                      report);
  });
  const bool pass = Outcome(test, fault, n, result, report);
  if (!pass) {
    ASC_DENSE_TEST_EQ(test, pivots, old_pivots);
    if (hermitian || layout == base::kRow) {
      ASC_DENSE_TEST_CHECK(test,
                           base::EqualBytes(a.data(), old_a.data(), sizeof(a)));
    }
    if (rhs_layout == base::kRow) {
      ASC_DENSE_TEST_EQ(test, b, old_b);
    }
  } else {
    for (int i = 0; i < n; ++i) {
      ASC_DENSE_TEST_EQ(test, pivots[static_cast<std::size_t>(i) + 1], i + 1);
    }
    for (int j = 0; j < nrhs; ++j) {
      for (int i = 0; i < n; ++i) {
        const auto expected = base::Value<T>(2.0L / (n + 3));
        ASC_DENSE_TEST_CHECK(
            test,
            std::abs(b[Offset(rhs_layout, i, j)] - expected) <=
                16 * std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon());
      }
    }
  }
  Guards(test, a, old_a, b, old_b, pivots, layout, rhs_layout, n, nrhs,
         triangle);
  ASC_DENSE_TEST_EQ(test, e.front(), T{-101});
  for (std::size_t i = static_cast<std::size_t>(n) + 1; i < e.size(); ++i) {
    ASC_DENSE_TEST_EQ(test, e[i], T{-101});
  }
  scratch.Guards(test, workspace);
}

template <typename T>
int Run(bool hermitian) {
  TestContext test;
  const auto provider = base::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int cases = 0;
  for (const auto triangle : {base::kUpper, base::kLower}) {
    for (const auto layout : {base::kColumn, base::kRow}) {
      for (const auto rhs_layout : {base::kColumn, base::kRow}) {
        for (const int n : {1, 2}) {
          for (const int nrhs : {0, 2}) {
            for (const bool preferred : {false, true}) {
              for (const auto fault :
                   {Fault::kPass, Fault::kOmitInfo, Fault::kWrite32BitInfo,
                    Fault::kNegativeInfo, Fault::kPositiveInfo,
                    Fault::kOmitPivots, Fault::kWrite32BitPivots,
                    Fault::kZeroPivot, Fault::kOutOfBoundsPivot,
                    Fault::kUnpairedPivot, Fault::kOmitWork, Fault::kNanWork,
                    Fault::kOmitE, Fault::kBadE,
                    Fault::kInconsistentPositiveInfo, Fault::kIncorrectWork}) {
                Check<T>(test, provider, hermitian, triangle, layout,
                         rhs_layout, n, nrhs, preferred, fault);
                ++cases;
              }
            }
          }
        }
      }
    }
  }
  std::printf("rook driver native fault cases=%d integer_bits=%d\n", cases,
              ASC_LAPACK_INTEGER_BITS);
  return test.Finish();
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 2) {
    return 2;
  }
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    return Run<float>(false);
  }
  if (scalar == "d") {
    return Run<double>(false);
  }
  if (scalar == "c") {
    return Run<std::complex<float>>(false);
  }
  if (scalar == "z") {
    return Run<std::complex<double>>(false);
  }
  if (scalar == "ch") {
    return Run<std::complex<float>>(true);
  }
  if (scalar == "zh") {
    return Run<std::complex<double>>(true);
  }
  return 2;
}
