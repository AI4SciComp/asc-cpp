#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_machine.h"
#include "installed_lu/normal_return_guard.h"
#include "machine_constants_faults.h"
#include "machine_constants_test_support.h"

namespace {
namespace constants = asc_machine_constants_test;
namespace numerical = asc_machine_constants_numerical;
namespace fault = asc_machine_constants_fault;
using fault::Mode;

template <typename T>
void Run(constants::TestContext& test) {
  const auto provider = constants::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  for (const auto fixture : numerical::Cases<T>()) {
    for (const auto mode :
         {Mode::kPass, Mode::kZero, Mode::kWrongSign, Mode::kNan,
          Mode::kInfinite, Mode::kChangeSelector, Mode::kWrongFinite}) {
      constants::Output<T> output;
      const auto before = output.values;
      const auto view = output.View();
      asc::LapackReport report;
      fault::Reset(mode);
      const auto status = constants::WithoutAllocation(test, [&] {
        return asc::Lamch(provider, fixture.parameter, view, report);
      });
      ASC_DENSE_TEST_EQ(test, fault::Calls(), 1U);
      ASC_DENSE_TEST_EQ(test, fault::UnexpectedNativeResults(), 0U);
      ASC_DENSE_TEST_CHECK(test, report.called_provider);
      ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
      const bool complete = mode == Mode::kPass || mode == Mode::kWrongFinite;
      ASC_DENSE_TEST_EQ(
          test, status.code(),
          complete ? asc::ErrorCode::kOk : asc::ErrorCode::kProvider);
      ASC_DENSE_TEST_EQ(test, report.outcome,
                        complete ? asc::LapackOutcome::kSuccess
                                 : asc::LapackOutcome::kPartialResult);
      ASC_DENSE_TEST_EQ(test, report.output_validity,
                        complete ? asc::LapackOutputValidity::kComplete
                                 : asc::LapackOutputValidity::kUnusable);
      if (!complete) {
        ASC_DENSE_TEST_EQ(test, output.values, before);
      } else if (mode == Mode::kPass) {
        ASC_DENSE_TEST_EQ(test, numerical::Bits(output.values[1]),
                          numerical::Bits(fixture.expected));
      } else {
        ASC_DENSE_TEST_EQ(test, output.values[1],
                          fixture.selector == 'M' ? T{-0.5} : T{0.5});
        ASC_DENSE_TEST_CHECK(test, numerical::Bits(output.values[1]) !=
                                       numerical::Bits(fixture.expected));
      }
      ASC_DENSE_TEST_EQ(test, output.values[0], T{71});
      ASC_DENSE_TEST_EQ(test, output.values[2], T{73});
    }
  }
}
}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard guard;
  if (argc != 2) {
    return 2;
  }
  constants::TestContext test;
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    Run<float>(test);
  } else if (scalar == "d") {
    Run<double>(test);
  } else {
    return 2;
  }
  return test.Finish();
}
