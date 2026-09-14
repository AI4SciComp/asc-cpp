#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string_view>

#include "../../src/dense/lapack/internal_layout.h"
#include "../../src/dense/lapack/internal_triangular_storage_counts.h"
#include "../../src/dense/lapack/internal_tridiagonal.h"
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "installed_lu/normal_return_guard.h"
#include "triangular_storage_conversion_faults.h"
#include "triangular_storage_conversion_test_support.h"

namespace {
namespace support = asc_triangular_storage_test;
namespace fault = asc_triangular_storage_fault;
using fault::Mode;

lapack_int Info(Mode mode, bool pack) {
  switch (mode) {
    case Mode::kOmitInfo:
      return std::numeric_limits<lapack_int>::max();
    case Mode::kPositive:
      return 1;
    case Mode::kNegativeOne:
      return -1;
    case Mode::kNegativeTwo:
      return -2;
    case Mode::kNegativeLeading:
      return pack ? -4 : -5;
    case Mode::kMinimum:
      return std::numeric_limits<lapack_int>::min();
    case Mode::kShortZero: {
      lapack_int info = std::numeric_limits<lapack_int>::max();
      const std::int32_t zero = 0;
      std::memcpy(&info, &zero, sizeof(zero));
      return info;
    }
    default:
      return 0;
  }
}

void Counts(support::TestContext& test) {
  using asc::internal_triangular_storage_counts::Count;
  for (const asc::extent_t limit :
       {asc::extent_t{std::numeric_limits<std::int32_t>::max()},
        std::numeric_limits<asc::extent_t>::max()}) {
    const bool lp64 = limit == std::numeric_limits<std::int32_t>::max();
    for (bool pack : {false, true}) {
      asc::extent_t bound = 3037000499;
      if (lp64) {
        bound = pack ? 65535 : 46340;
      }
      for (asc::extent_t n : {asc::extent_t{0}, asc::extent_t{1}, bound}) {
        const auto result = Count(n, n == 0 ? 1 : n, pack, limit);
        ASC_DENSE_TEST_CHECK(test, result.ok());
        if (result.ok()) {
          ASC_DENSE_TEST_EQ(test, result->dense, n * n);
          ASC_DENSE_TEST_EQ(test, result->packed, n * (n + 1) / 2);
        }
      }
      ASC_DENSE_TEST_EQ(
          test, Count(bound + 1, bound + 1, pack, limit).status().code(),
          asc::ErrorCode::kOverflow);
      ASC_DENSE_TEST_EQ(test, Count(limit, limit, pack, limit).status().code(),
                        asc::ErrorCode::kOverflow);
      ASC_DENSE_TEST_EQ(test, Count(-1, 1, pack, limit).status().code(),
                        asc::ErrorCode::kInvalidArgument);
      ASC_DENSE_TEST_EQ(test, Count(2, 1, pack, limit).status().code(),
                        asc::ErrorCode::kInvalidArgument);
      ASC_DENSE_TEST_CHECK(test, Count(1, limit, pack, limit).ok());
      if (lp64) {
        ASC_DENSE_TEST_EQ(test,
                          Count(1, limit + 1, pack, limit).status().code(),
                          asc::ErrorCode::kOverflow);
      }
    }
  }
  ASC_DENSE_TEST_CHECK(test, !Count(0, 1, true, 0).ok());
  // Aggregate byte arithmetic is tested on plan metadata, never on a pretend
  // backing array. The two regions fit alone but exceed the common byte limit.
  const auto provider = support::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  support::Problem<double> empty(0, 0, support::kColumn, support::kColumn,
                                 support::kUpper, true, 0);
  auto plan = support::Take(empty.Query(provider));
  plan.total_byte_limit = 100;
  plan.regions[support::kScratch] = {12, 12, 8, 8};
  plan.regions[support::kLayout] = {1, 1, 8, 8};
  ASC_DENSE_TEST_EQ(test, asc::internal_lapack_layout::CheckTotal(plan).code(),
                    asc::ErrorCode::kOverflow);
}

template <typename T>
void Case(support::TestContext& test,
          const asc::ReferenceLapackProvider& provider, asc::DenseBlasLayout dl,
          asc::DenseBlasLayout pl, asc::DenseBlasTriangle triangle, bool pack,
          Mode mode) {
  support::Problem<T> problem(3, 2, dl, pl, triangle, pack, 0);
  auto expected = problem;
  const auto info = Info(mode, pack);
  if (info == 0) {
    for (asc::extent_t i = 0; i < problem.n; ++i) {
      for (asc::extent_t j = 0; j < problem.n; ++j) {
        if (support::Selected(triangle, i, j)) {
          T value = support::Fixture<T>(i, j, 0);
          if (mode == Mode::kOmitValues) {
            value = T{};
          } else if (mode == Mode::kWrongValues) {
            value = T{42};
          }
          if (pack) {
            expected.packed[1 + support::PackedIndex(problem.n, triangle, pl, i,
                                                     j)] = value;
          } else {
            expected.dense[expected.DenseIndex(i, j)] = value;
          }
        }
      }
    }
  }
  const auto plan = support::Take(problem.Query(provider));
  support::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  fault::Reset(mode);
  const auto status = support::WithoutAllocation(
      test, [&] { return problem.Run(provider, plan, workspace, report); });
  ASC_DENSE_TEST_EQ(test, fault::Calls(), std::size_t{1});
  ASC_DENSE_TEST_CHECK(test, fault::BaselinePassed());
  ASC_DENSE_TEST_EQ(
      test, status.code(),
      info == 0 ? asc::ErrorCode::kOk : asc::ErrorCode::kProvider);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_CHECK(test, report.native_info.has_value());
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(73), info);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    info == 0 ? asc::LapackOutputValidity::kComplete
                              : asc::LapackOutputValidity::kUnusable);
  const bool argument =
      info < 0 && info != std::numeric_limits<lapack_int>::min();
  ASC_DENSE_TEST_EQ(test, report.native_argument.has_value(), argument);
  if (argument) {
    ASC_DENSE_TEST_EQ(test, report.native_argument.value_or(73), -info);
  }
  auto outcome = asc::LapackOutcome::kSuccess;
  if (info < 0) {
    outcome = asc::LapackOutcome::kProviderArgument;
  } else if (info > 0) {
    outcome = asc::LapackOutcome::kPartialResult;
  }
  ASC_DENSE_TEST_EQ(test, report.outcome, outcome);
  ASC_DENSE_TEST_CHECK(test, support::Same(problem.dense, expected.dense));
  ASC_DENSE_TEST_CHECK(test, support::Same(problem.packed, expected.packed));
  scratch.Guards(test, workspace);
}

template <typename T>
void Run(support::TestContext& test) {
  const auto provider = support::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  for (auto dl : {support::kRow, support::kColumn}) {
    for (auto pl : {support::kRow, support::kColumn}) {
      for (auto triangle : {support::kUpper, support::kLower}) {
        for (bool pack : {false, true}) {
          for (auto mode :
               {Mode::kPass, Mode::kOmitInfo, Mode::kPositive,
                Mode::kNegativeOne, Mode::kNegativeTwo, Mode::kNegativeLeading,
                Mode::kMinimum, Mode::kShortZero, Mode::kOmitValues,
                Mode::kWrongValues}) {
            Case<T>(test, provider, dl, pl, triangle, pack, mode);
          }
        }
      }
    }
  }
}
}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard guard;
  if (argc != 2) {
    return 2;
  }
  support::TestContext test;
  const std::string_view scalar(argv[1]);
  if (scalar == "counts") {
    Counts(test);
  } else if (scalar == "s") {
    Run<float>(test);
  } else if (scalar == "d") {
    Run<double>(test);
  } else if (scalar == "c") {
    Run<std::complex<float>>(test);
  } else if (scalar == "z") {
    Run<std::complex<double>>(test);
  } else {
    return 2;
  }
  return test.Finish();
}
