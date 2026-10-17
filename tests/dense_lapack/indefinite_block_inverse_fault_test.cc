#include <array>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_block_inverse_faults.h"
#include "indefinite_block_inverse_test_support.h"
#include "indefinite_test_support.h"
#include "installed_lu/normal_return_guard.h"
#include "lapack_build_config.h"
namespace {
namespace base = asc_indefinite_test;
namespace faults = asc_block_inverse_fault_test;
using base::TestContext;
using faults::Fault;

bool Valid(Fault fault) {
  return fault == Fault::kPass ||
         (fault == Fault::kWrite32BitInfo && ASC_LAPACK_INTEGER_BITS == 32);
}
void Outcome(TestContext& test, Fault fault, const asc::Status& status,
             const asc::LapackReport& report, int expected_info) {
  const bool valid = Valid(fault);
  ASC_DENSE_TEST_EQ(test, faults::Calls(), 1U);
  ASC_DENSE_TEST_CHECK(test, faults::SeedWasFullWidth());
  ASC_DENSE_TEST_EQ(test, faults::LastNativeInfo(), expected_info);
  ASC_DENSE_TEST_CHECK(
      test, report.called_provider && report.native_info.has_value());
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-999),
                    faults::LastPublishedInfo());
  ASC_DENSE_TEST_EQ(test, status.code(),
                    valid ? (expected_info == 0 ? asc::ErrorCode::kOk
                                                : asc::ErrorCode::kNumerical)
                          : asc::ErrorCode::kProvider);
  if (valid) {
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      expected_info == 0 ? asc::LapackOutcome::kSuccess
                                         : asc::LapackOutcome::kSingular);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      expected_info == 0
                          ? asc::LapackOutputValidity::kComplete
                          : asc::LapackOutputValidity::kDocumentedPartial);
    if (expected_info > 0) {
      ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-1),
                        expected_info - 1);
    }
  } else {
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kUnusable);
    if (fault == Fault::kNegativeInfo) {
      ASC_DENSE_TEST_EQ(test, report.native_argument.value_or(0), 4);
    }
  }
}

template <typename T>
void Publication(TestContext& test, bool hermitian,
                 asc::DenseBlasTriangle triangle, asc::DenseBlasLayout layout,
                 int mode, Fault fault, int n, int expected_info,
                 const std::array<T, 20>& a, const std::array<T, 20>& before,
                 const asc_block_inverse_test::Scratch<T>& scratch) {
  auto offset = [layout](int i, int j) {
    return 1U + static_cast<std::size_t>(layout == base::kColumn ? j * 5 + i
                                                                 : i * 5 + j);
  };
  const bool converted = asc_block_inverse_test::g_block_size != 0 ||
                         n > asc_block_inverse_test::Block<T>(hermitian);
  if ((expected_info > 0 && (mode != 4 || !converted)) ||
      (!Valid(fault) && layout == base::kRow)) {
    ASC_DENSE_TEST_CHECK(test,
                         base::EqualBytes(a.data(), before.data(), sizeof(a)));
  } else if (mode == 4) {
    const auto off = triangle == base::kUpper ? offset(0, 1) : offset(2, 1);
    ASC_DENSE_TEST_EQ(test, a[off], T{});
    ASC_DENSE_TEST_EQ(test, scratch.scalar[2], base::Value<T>(3, 4));
  } else if (mode == 0) {
    ASC_DENSE_TEST_EQ(test, a[1], T{0.25});
  }
  for (std::size_t k = 0; k < a.size(); ++k) {
    const int relative = static_cast<int>(k) - 1;
    const int i = layout == base::kColumn ? relative % 5 : relative / 5;
    const int j = layout == base::kColumn ? relative / 5 : relative % 5;
    if (k == 0 || i >= n || j >= n ||
        (triangle == base::kUpper ? i > j : i < j)) {
      ASC_DENSE_TEST_EQ(test, a[k], before[k]);
    }
  }
}

template <typename T>
void Case(TestContext& test, const asc::ReferenceLapackProvider& provider,
          bool hermitian, asc::DenseBlasTriangle triangle,
          asc::DenseBlasLayout layout, int mode, Fault fault) {
  int n = 1;
  if (mode == 2) {
    n = 2;
  }
  if (mode == 3 || mode == 4) {
    n = 3;
  }
  int expected_info = mode == 1 ? 1 : 0;
  if (mode == 3 || mode == 4) {
    expected_info = triangle == base::kUpper ? 3 : 1;
  }
  std::array<T, 20> a;
  a.fill(base::Value<T>(-503, 19));
  auto offset = [layout](int i, int j) {
    return 1U + static_cast<std::size_t>(layout == base::kColumn ? j * 5 + i
                                                                 : i * 5 + j);
  };
  for (int j = 0; j < n; ++j) {
    for (int i = 0; i < n; ++i) {
      if (triangle == base::kUpper ? i <= j : i >= j) {
        a[offset(i, j)] = T{};
      }
    }
  }
  if (mode == 0) {
    a[1] = T{4};
  }
  if (mode == 2) {
    a[triangle == base::kUpper ? offset(0, 1) : offset(1, 0)] =
        base::Value<T>(3, 4);
  }
  if (mode == 3) {
    a[offset(1, 1)] = T{4};
  }
  const asc::index_t paired_pivot = triangle == base::kUpper ? -1 : -2;
  std::array<asc::index_t, 5> pivots{-509, mode == 2 ? paired_pivot : 1,
                                     mode == 2 ? paired_pivot : 2, 3, -509};
  if (mode == 4) {
    if (triangle == base::kUpper) {
      pivots[1] = -1;
      pivots[2] = -1;
      a[offset(0, 1)] = base::Value<T>(3, 4);
    } else {
      pivots[2] = -3;
      pivots[3] = -3;
      a[offset(2, 1)] = base::Value<T>(3, 4);
    }
  }
  auto matrix = base::Matrix(a, n, n, layout, 5);
  const auto raw = base::Raw(pivots, n);
  const auto before = a;
  const auto pivot_before = pivots;
  const auto plan = base::Take(asc_block_inverse_test::Query(
      provider, triangle, hermitian, matrix, raw));
  asc_block_inverse_test::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  faults::SetFault(fault);
  const auto status = base::WithoutAllocation(test, [&] {
    return asc_block_inverse_test::Inverse(
        provider, triangle, hermitian, matrix, raw, plan, workspace, report);
  });
  Outcome(test, fault, status, report, expected_info);
  ASC_DENSE_TEST_EQ(test, pivots, pivot_before);
  Publication(test, hermitian, triangle, layout, mode, fault, n, expected_info,
              a, before, scratch);
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
      for (const int mode : {0, 1, 2, 3, 4}) {
        for (const Fault fault :
             {Fault::kPass, Fault::kOmitInfo, Fault::kWrite32BitInfo,
              Fault::kNegativeInfo, Fault::kOutOfRangeInfo,
              Fault::kContradictoryInfo, Fault::kWrongIndex,
              Fault::kChangedPivot}) {
          Case<T>(test, provider, hermitian, triangle, layout, mode, fault);
          ++cases;
        }
      }
    }
  }
  std::printf("block inverse fault cases=%d integer_bits=%d\n", cases,
              ASC_LAPACK_INTEGER_BITS);
  return test.Finish();
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 3 || !asc_block_inverse_test::Select(argv[2])) {
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
