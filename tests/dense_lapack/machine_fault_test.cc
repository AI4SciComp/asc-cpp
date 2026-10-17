#include <array>
#include <limits>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_machine.h"
#include "installed_lu/normal_return_guard.h"
#include "machine_faults.h"
#include "machine_test_support.h"

namespace {
namespace machine = asc_machine_test;
namespace numerical = asc_machine_numerical;
namespace fault = asc_machine_fault;
using fault::Mode;

template <typename T>
void Run(machine::TestContext& test) {
  const auto provider = machine::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const T nan = std::numeric_limits<T>::quiet_NaN();
  for (const auto pair : {std::array<T, 2>{1, 2}, std::array<T, 2>{T{0}, -T{0}},
                          std::array<T, 2>{nan, -nan}}) {
    for (const auto mode : {Mode::kPass, Mode::kSmallBit, Mode::kLargeBit,
                            Mode::kBothBits, Mode::kSwap}) {
      machine::Operand<T> small;
      machine::Operand<T> large;
      small.values[1] = pair[0];
      large.values[1] = pair[1];
      const auto sv = small.View();
      const auto lv = large.View();
      asc::LapackReport report;
      fault::Reset(mode);
      const auto status = machine::WithoutAllocation(
          test, [&] { return asc::Labad(provider, sv, lv, report); });
      ASC_DENSE_TEST_EQ(test, fault::Calls(), 1U);
      ASC_DENSE_TEST_EQ(test, fault::UnexpectedNativeResults(), 0U);
      ASC_DENSE_TEST_CHECK(test, report.called_provider);
      ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
      ASC_DENSE_TEST_EQ(test, status.code(),
                        mode == Mode::kPass ? asc::ErrorCode::kOk
                                            : asc::ErrorCode::kProvider);
      ASC_DENSE_TEST_EQ(test, report.outcome,
                        mode == Mode::kPass
                            ? asc::LapackOutcome::kSuccess
                            : asc::LapackOutcome::kPartialResult);
      ASC_DENSE_TEST_EQ(test, report.output_validity,
                        mode == Mode::kPass
                            ? asc::LapackOutputValidity::kComplete
                            : asc::LapackOutputValidity::kUnusable);
      ASC_DENSE_TEST_EQ(test, numerical::Bits(small.values[1]),
                        numerical::Bits(pair[0]));
      ASC_DENSE_TEST_EQ(test, numerical::Bits(large.values[1]),
                        numerical::Bits(pair[1]));
      for (const auto* operand : {&small, &large}) {
        ASC_DENSE_TEST_EQ(test, operand->values[0], T{71});
        ASC_DENSE_TEST_EQ(test, operand->values[2], T{73});
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
  machine::TestContext test;
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
