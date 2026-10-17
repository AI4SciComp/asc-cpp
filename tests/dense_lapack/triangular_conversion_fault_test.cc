#include <complex>
#include <limits>
#include <string_view>

#include "../../src/dense/lapack/internal_tridiagonal.h"
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "installed_lu/normal_return_guard.h"
#include "triangular_conversion_faults.h"
#include "triangular_conversion_test_support.h"
namespace {
namespace tc = asc_triangular_conversion_test;
namespace fault = asc_triangular_conversion_fault;
using fault::Mode;
lapack_int ExpectedInfo(Mode mode) {
  switch (mode) {
    case Mode::kNegative:
      return -1;
    case Mode::kRange:
    case Mode::kShortWidthInfo:
      return 1;
    case Mode::kExcess:
      return 2;
    case Mode::kMaximum:
      return std::numeric_limits<lapack_int>::max();
    case Mode::kMinimum:
      return std::numeric_limits<lapack_int>::min();
    default:
      return 0;
  }
}
template <typename Input>
void Case(tc::TestContext& test, const asc::ReferenceLapackProvider& provider,
          asc::extent_t n, asc::DenseBlasTriangle triangle,
          asc::DenseBlasLayout il, asc::DenseBlasLayout ol, Mode mode) {
  tc::Problem<Input> p(n, triangle, il, ol);
  tc::Initialize(p);
  const auto before = p;
  const auto plan = tc::Take(p.Query(provider));
  tc::Scratch<Input> scratch;
  const auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  fault::Reset(mode);
  const auto status = tc::WithoutAllocation(
      test, [&] { return p.Run(provider, plan, workspace, report); });
  ASC_DENSE_TEST_EQ(test, fault::Calls(), n > 0 ? 1U : 0U);
  ASC_DENSE_TEST_EQ(test, fault::UnexpectedNativeResults(), 0U);
  const bool provider_error = mode == Mode::kNegative ||
                              mode == Mode::kExcess || mode == Mode::kMaximum ||
                              mode == Mode::kMinimum;
  const bool range = mode == Mode::kRange || mode == Mode::kShortWidthInfo;
  const bool nonfinite = mode == Mode::kUnwrittenOutput ||
                         mode == Mode::kNanOutput ||
                         mode == Mode::kInfiniteOutput;
  auto expected = asc::ErrorCode::kOk;
  if (n > 0 && provider_error) {
    expected = asc::ErrorCode::kProvider;
  } else if (n > 0 && (range || nonfinite)) {
    expected = asc::ErrorCode::kNumerical;
  }
  ASC_DENSE_TEST_EQ(test, status.code(), expected);
  ASC_DENSE_TEST_EQ(test, report.native_info.has_value(), n > 0);
  if (n > 0) {
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(43),
                      ExpectedInfo(mode));
  }
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    n > 0 && (provider_error || range)
                        ? asc::LapackOutputValidity::kUnchanged
                        : asc::LapackOutputValidity::kComplete);
  if (n == 0 || provider_error || range) {
    ASC_DENSE_TEST_EQ(test, p.output.data, before.output.data);
  } else {
    for (asc::extent_t j = 0; j < n; ++j) {
      for (asc::extent_t i = 0; i < n; ++i) {
        const auto wide = tc::ToWide(before.input.At(i, j));
        auto value = before.output.At(i, j);
        if (tc::Selected(triangle, i, j)) {
          value = tc::Value<tc::Output<Input>>(wide.real(), wide.imag());
          if (mode == Mode::kUnwrittenOutput) {
            value = tc::Value<tc::Output<Input>>(
                std::numeric_limits<float>::quiet_NaN(),
                std::numeric_limits<float>::quiet_NaN());
          } else if (i == 0 && j == 0 && mode == Mode::kNanOutput) {
            value = tc::Value<tc::Output<Input>>(
                std::numeric_limits<float>::quiet_NaN(), 0);
          } else if (i == 0 && j == 0 && mode == Mode::kInfiniteOutput) {
            value = tc::Value<tc::Output<Input>>(
                std::numeric_limits<float>::infinity(), 0);
          } else if (i == 0 && j == 0 && mode == Mode::kZeroOutput) {
            value = tc::Value<tc::Output<Input>>(-0.0L, 0);
          }
        }
        ASC_DENSE_TEST_CHECK(test, tc::Same(p.output.At(i, j), value));
      }
    }
  }
  ASC_DENSE_TEST_EQ(test, p.input.data, before.input.data);
  p.output.Guards(test);
  scratch.Guards(test, workspace);
}
template <typename Input>
void Run(tc::TestContext& test) {
  const auto provider = tc::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  for (asc::extent_t n : {0, 1, 5}) {
    for (auto triangle : {tc::kUpper, tc::kLower}) {
      for (auto il : {tc::kColumn, tc::kRow}) {
        for (auto ol : {tc::kColumn, tc::kRow}) {
          for (auto mode :
               {Mode::kPass, Mode::kNegative, Mode::kRange, Mode::kExcess,
                Mode::kMaximum, Mode::kMinimum, Mode::kUnwrittenInfo,
                Mode::kShortWidthInfo, Mode::kUnwrittenOutput, Mode::kNanOutput,
                Mode::kInfiniteOutput, Mode::kZeroOutput}) {
            Case<Input>(test, provider, n, triangle, il, ol, mode);
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
  tc::TestContext test;
  const std::string_view scalar(argv[1]);
  if (scalar == "d") {
    Run<double>(test);
  } else if (scalar == "z") {
    Run<std::complex<double>>(test);
  } else {
    return 2;
  }
  return test.Finish();
}
