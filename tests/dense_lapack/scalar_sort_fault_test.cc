#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string_view>

#include "../../src/dense/lapack/internal_scalar_sort_counts.h"
#include "../../src/dense/lapack/internal_tridiagonal.h"
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_scalar_sort.h"
#include "installed_lu/normal_return_guard.h"
#include "scalar_sort_faults.h"
#include "scalar_sort_test_support.h"
namespace {
namespace sort = asc_scalar_sort_test;
namespace fault = asc_scalar_sort_fault;
using fault::Mode;
asc::LapackSortOrder UnknownOrder() {
  // Fixed uint8_t underlying type permits255; exercise unknown-enumerator
  // rejection. NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  return static_cast<asc::LapackSortOrder>(255);
}
void Counts(sort::TestContext& test) {
  using asc::internal_scalar_sort_counts::Count;
  for (asc::extent_t limit :
       {static_cast<asc::extent_t>(std::numeric_limits<std::int32_t>::max()),
        std::numeric_limits<asc::extent_t>::max()}) {
    const auto bound = std::min(limit / 2, asc::extent_t{1} << 32);
    for (asc::extent_t n : {asc::extent_t{0}, asc::extent_t{1}, bound}) {
      ASC_DENSE_TEST_CHECK(test, Count(n, limit).ok());
    }
    ASC_DENSE_TEST_EQ(test, Count(bound + 1, limit).code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(test, Count(limit, limit).code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(test, Count(-1, limit).code(),
                      asc::ErrorCode::kInvalidArgument);
  }
  ASC_DENSE_TEST_CHECK(test, !Count(0, 0).ok());
}
lapack_int ExpectedInfo(Mode mode) {
  switch (mode) {
    case Mode::kOmitInfo:
      return std::numeric_limits<lapack_int>::max();
    case Mode::kPositive:
      return 1;
    case Mode::kNegativeOne:
      return -1;
    case Mode::kNegativeTwo:
      return -2;
    case Mode::kMinimum:
      return std::numeric_limits<lapack_int>::min();
    case Mode::kShortZero: {
      lapack_int value = std::numeric_limits<lapack_int>::max();
      const std::int32_t zero = 0;
      std::memcpy(&value, &zero, sizeof(zero));
      return value;
    }
    default:
      return 0;
  }
}
template <typename T>
void Contents(sort::TestContext& test, const std::array<T, 5>& data,
              asc::LapackSortOrder order, Mode mode) {
  const bool warning = mode == Mode::kNan;
  ASC_DENSE_TEST_EQ(test, data.front(), T{71});
  ASC_DENSE_TEST_EQ(test, data.back(), T{73});
  if (warning) {
    ASC_DENSE_TEST_CHECK(test, std::isnan(data[1]));
  } else if (mode == Mode::kSortedWrong) {
    ASC_DENSE_TEST_EQ(test, data[1], T{42});
    ASC_DENSE_TEST_EQ(test, data[2], T{42});
    ASC_DENSE_TEST_EQ(test, data[3], T{42});
  } else if (mode == Mode::kOmitValues) {
    ASC_DENSE_TEST_EQ(test, data[1], T{3});
    ASC_DENSE_TEST_EQ(test, data[2], T{1});
    ASC_DENSE_TEST_EQ(test, data[3], T{2});
  } else {
    ASC_DENSE_TEST_EQ(test, data[2], T{2});
    ASC_DENSE_TEST_EQ(test, data[3],
                      order == asc::LapackSortOrder::kIncreasing ? T{3} : T{1});
  }
}
template <typename T>
void FaultCase(sort::TestContext& test,
               const asc::ReferenceLapackProvider& provider,
               asc::LapackSortOrder order, Mode mode) {
  std::array<T, 5> data{71, 3, 1, 2, 73};
  const auto view = sort::Take(asc::DenseBlasVectorView<T>::Create(
      data.data() + 1, 3, 1, {data.data(), sizeof(data), sort::kHost}));
  asc::LapackReport report;
  fault::Reset(mode);
  const auto status = sort::WithoutAllocation(
      test, [&] { return asc::Lasrt(provider, order, view, report); });
  const bool accepted = mode == Mode::kPass || mode == Mode::kSortedWrong ||
                        (mode == Mode::kShortZero && sizeof(lapack_int) == 4);
  const bool warning = mode == Mode::kNan;
  ASC_DENSE_TEST_EQ(test, status.code(),
                    accepted  ? asc::ErrorCode::kOk
                    : warning ? asc::ErrorCode::kNumerical
                              : asc::ErrorCode::kProvider);
  ASC_DENSE_TEST_EQ(test, fault::Calls(), std::size_t{1});
  ASC_DENSE_TEST_EQ(test, fault::UnexpectedNativeResults(), std::size_t{0});
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_CHECK(test, report.native_info.has_value());
  if (report.native_info.has_value()) {
    ASC_DENSE_TEST_EQ(test, *report.native_info, ExpectedInfo(mode));
  }
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    accepted || warning ? asc::LapackOutputValidity::kComplete
                                        : asc::LapackOutputValidity::kUnusable);
  const bool negative = mode == Mode::kNegativeOne ||
                        mode == Mode::kNegativeTwo || mode == Mode::kMinimum;
  ASC_DENSE_TEST_EQ(test, report.outcome,
                    accepted   ? asc::LapackOutcome::kSuccess
                    : warning  ? asc::LapackOutcome::kAccuracyWarning
                    : negative ? asc::LapackOutcome::kProviderArgument
                               : asc::LapackOutcome::kPartialResult);
  ASC_DENSE_TEST_EQ(test, report.native_argument.has_value(),
                    mode == Mode::kNegativeOne || mode == Mode::kNegativeTwo);
  if (report.native_argument.has_value()) {
    ASC_DENSE_TEST_EQ(test, *report.native_argument,
                      mode == Mode::kNegativeOne ? 1 : 2);
  }
  Contents(test, data, order, mode);
}
template <typename T>
void Validation(sort::TestContext& test,
                const asc::ReferenceLapackProvider& provider) {
  std::array<T, 8> data{71, 3, 1, 2, 73, 79, 83, 89};
  const auto before = data;
  asc::LapackReport report;
  for (unsigned mode = 0; mode < 4; ++mode) {
    auto space = sort::kHost;
    if (mode == 2) {
      space = asc::MemorySpace::kDevice;
    } else if (mode == 3) {
      space = asc::MemorySpace::kManaged;
    }
    const auto view = sort::Take(asc::DenseBlasVectorView<T>::Create(
        data.data() + 1, 3, mode == 1 ? 2 : 1,
        {data.data(), sizeof(data), space}));
    const auto order =
        mode == 0 ? UnknownOrder() : asc::LapackSortOrder::kIncreasing;
    report.called_provider = true;
    report.native_info = 79;
    fault::Reset(Mode::kPass);
    const auto status = sort::WithoutAllocation(
        test, [&] { return asc::Lasrt(provider, order, view, report); });
    ASC_DENSE_TEST_EQ(test, status.code(),
                      mode == 0   ? asc::ErrorCode::kInvalidArgument
                      : mode == 1 ? asc::ErrorCode::kShape
                                  : asc::ErrorCode::kMemoryAccess);
    ASC_DENSE_TEST_EQ(test, fault::Calls(), std::size_t{0});
    ASC_DENSE_TEST_CHECK(test, !report.called_provider);
    ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
    ASC_DENSE_TEST_CHECK(test, data == before);
  }
  const auto alias = sort::Take(asc::DenseBlasVectorView<T>::Create(
      reinterpret_cast<T*>(&report), 1, 1,
      {&report, sizeof(report), sort::kHost}));
  report.called_provider = true;
  report.native_info = 79;
  fault::Reset(Mode::kPass);
  ASC_DENSE_TEST_EQ(
      test,
      asc::Lasrt(provider, asc::LapackSortOrder::kIncreasing, alias, report)
          .code(),
      asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, fault::Calls(), std::size_t{0});
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, *report.native_info, 79);
  const auto empty = sort::Take(asc::DenseBlasVectorView<T>::Create(
      data.data(), 0, 1, {data.data(), sizeof(data), sort::kHost}));
  ASC_DENSE_TEST_CHECK(
      test,
      asc::Lasrt(provider, asc::LapackSortOrder::kIncreasing, empty, report)
          .ok());
  ASC_DENSE_TEST_EQ(test, fault::Calls(), std::size_t{0});
  ASC_DENSE_TEST_CHECK(test, !report.called_provider);
  ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
  ASC_DENSE_TEST_CHECK(test, data == before);
}
template <typename T>
void Run(sort::TestContext& test) {
  const auto provider = sort::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  for (auto order : sort::kOrders) {
    for (auto mode :
         {Mode::kPass, Mode::kOmitInfo, Mode::kPositive, Mode::kNegativeOne,
          Mode::kNegativeTwo, Mode::kMinimum, Mode::kShortZero, Mode::kUnsorted,
          Mode::kSortedWrong, Mode::kNan, Mode::kOmitValues}) {
      FaultCase<T>(test, provider, order, mode);
    }
  }
  Validation<T>(test, provider);
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard guard;
  if (argc != 2) {
    return 2;
  }
  const std::string_view scalar(argv[1]);
  sort::TestContext test;
  if (scalar == "counts") {
    Counts(test);
  } else if (scalar == "s") {
    Run<float>(test);
  } else if (scalar == "d") {
    Run<double>(test);
  } else {
    return 2;
  }
  return test.Finish();
}
