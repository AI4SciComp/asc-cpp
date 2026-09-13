#include <cmath>
#include <complex>
#include <cstdint>
#include <limits>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/providers/lapack.h"
#include "installed_lu/normal_return_guard.h"
#include "precision_conversion_faults.h"
#include "precision_conversion_test_support.h"
namespace {
using asc_conversion_test::kColumn;
using asc_conversion_test::kRow;
using asc_conversion_test::Output;
using asc_conversion_test::Problem;
using asc_conversion_test::Scratch;
using asc_conversion_test::Take;
using asc_conversion_test::TestContext;
using asc_conversion_test::ToWide;
using asc_conversion_test::Value;
using asc_conversion_test::WithoutAllocation;
namespace fault = asc_conversion_fault;
void CheckInfo(TestContext& test, const asc::ReferenceLapackProvider& provider,
               fault::Mode mode, const asc::LapackReport& report) {
  std::int64_t expected = 0;
  if (mode == fault::Mode::kNegativeInfo) {
    expected = -1;
  } else if (mode == fault::Mode::kOneInfo) {
    expected = 1;
  } else if (mode == fault::Mode::kExcessInfo) {
    expected = 2;
  } else if (mode == fault::Mode::kUnwrittenInfo ||
             mode == fault::Mode::kPartialInfo) {
    expected = provider.identity().integer_abi == asc::LapackIntegerAbi::kIlp64
                   ? std::numeric_limits<std::int64_t>::min()
                   : std::numeric_limits<std::int32_t>::min();
  }
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-999), expected);
  ASC_DENSE_TEST_EQ(test, report.native_argument.has_value(),
                    mode == fault::Mode::kNegativeInfo);
}
template <typename Input>
int Run() {
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  for (auto il : {kColumn, kRow}) {
    for (auto ol : {kColumn, kRow}) {
      for (auto mode : {fault::Mode::kPass, fault::Mode::kNegativeInfo,
                        fault::Mode::kOneInfo, fault::Mode::kExcessInfo,
                        fault::Mode::kUnwrittenInfo, fault::Mode::kPartialInfo,
                        fault::Mode::kUnwrittenOutput}) {
        Problem<Input> p(2, 3, il, ol);
        for (asc::extent_t i = 0; i < 2; ++i) {
          for (asc::extent_t j = 0; j < 3; ++j) {
            p.input.At(i, j) = Value<Input>(i + j + 1, 0.25L);
          }
        }
        const auto before = p;
        const auto plan = Take(p.Query(provider));
        Scratch<Input> scratch;
        const auto workspace = scratch.Workspace(plan);
        asc::LapackReport report;
        fault::Reset(mode);
        const auto status = WithoutAllocation(
            test, [&] { return p.Run(provider, plan, workspace, report); });
        ASC_DENSE_TEST_EQ(test, fault::Calls(), 1U);
        ASC_DENSE_TEST_CHECK(test, report.called_provider);
        CheckInfo(test, provider, mode, report);
        ASC_DENSE_TEST_EQ(test, p.input.data, before.input.data);
        if (mode == fault::Mode::kPass) {
          ASC_DENSE_TEST_CHECK(test, status.ok());
          ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-999), 0);
          for (asc::extent_t i = 0; i < 2; ++i) {
            for (asc::extent_t j = 0; j < 3; ++j) {
              ASC_DENSE_TEST_EQ(test, p.output.At(i, j),
                                Value<Output<Input>>(i + j + 1, 0.25L));
            }
          }
        } else if (mode == fault::Mode::kUnwrittenOutput) {
          ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
          ASC_DENSE_TEST_EQ(test, report.outcome,
                            asc::LapackOutcome::kAccuracyWarning);
          ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-999), 0);
          ASC_DENSE_TEST_CHECK(test,
                               std::isnan(ToWide(p.output.At(1, 2)).real()));
        } else {
          const bool range_failure = mode == fault::Mode::kOneInfo &&
                                     sizeof(Input) > sizeof(Output<Input>);
          ASC_DENSE_TEST_EQ(test, status.code(),
                            range_failure ? asc::ErrorCode::kNumerical
                                          : asc::ErrorCode::kProvider);
          if (range_failure) {
            ASC_DENSE_TEST_EQ(test, report.outcome,
                              asc::LapackOutcome::kAccuracyWarning);
          }
          ASC_DENSE_TEST_EQ(test, p.output.data, before.output.data);
          ASC_DENSE_TEST_EQ(test, report.output_validity,
                            asc::LapackOutputValidity::kUnchanged);
        }
        p.output.Guards(test);
        scratch.Guards(test, workspace);
      }
    }
  }
  return test.Finish();
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard guard;
  if (argc != 2) {
    return 2;
  }
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    return Run<float>();
  }
  if (scalar == "d") {
    return Run<double>();
  }
  if (scalar == "c") {
    return Run<std::complex<float>>();
  }
  if (scalar == "z") {
    return Run<std::complex<double>>();
  }
  return 2;
}
