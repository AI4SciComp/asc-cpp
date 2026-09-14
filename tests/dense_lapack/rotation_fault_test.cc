#include <cmath>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_rotation.h"
#include "installed_lu/normal_return_guard.h"
#include "rotation_faults.h"
#include "rotation_test_support.h"

namespace {
namespace rotation = asc_rotation_test;
namespace fault = asc_rotation_fault;
using fault::Mode;
using rotation::TestContext;

template <typename T>
void Run(TestContext& test) {
  const auto provider = rotation::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  for (const auto mode :
       {Mode::kPass, Mode::kOmitCosine, Mode::kOmitSine, Mode::kOmitRotated,
        Mode::kNegativeCosine, Mode::kLargeCosine, Mode::kNanCosine,
        Mode::kInfiniteCosine, Mode::kLargeSine, Mode::kNanSine,
        Mode::kInfiniteSine, Mode::kNanRotated, Mode::kInfiniteRotated,
        Mode::kNegativeInfiniteRotated, Mode::kChangeF, Mode::kChangeG,
        Mode::kWrongFinite, Mode::kZeroTuple}) {
    rotation::Output<T> c;
    rotation::Output<T> s;
    rotation::Output<T> r;
    const auto before = c.values;
    const auto cv = c.View();
    const auto sv = s.View();
    const auto rv = r.View();
    asc::LapackReport report;
    fault::Reset(mode);
    const auto status = rotation::WithoutAllocation(test, [&] {
      return asc::Lartg(provider, T{3}, T{4}, cv, sv, rv, report);
    });
    ASC_DENSE_TEST_EQ(test, fault::Calls(), 1U);
    ASC_DENSE_TEST_EQ(test, fault::UnexpectedNativeResults(), 0U);
    ASC_DENSE_TEST_CHECK(test, report.called_provider);
    ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
    const bool warning = mode == Mode::kInfiniteRotated ||
                         mode == Mode::kNegativeInfiniteRotated;
    const bool complete = warning || mode == Mode::kPass ||
                          mode == Mode::kWrongFinite ||
                          mode == Mode::kZeroTuple;
    if (!complete) {
      ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
      ASC_DENSE_TEST_EQ(test, report.outcome,
                        asc::LapackOutcome::kPartialResult);
      ASC_DENSE_TEST_EQ(test, report.output_validity,
                        asc::LapackOutputValidity::kUnusable);
      ASC_DENSE_TEST_EQ(test, c.values, before);
      ASC_DENSE_TEST_EQ(test, s.values, before);
      ASC_DENSE_TEST_EQ(test, r.values, before);
    } else {
      ASC_DENSE_TEST_EQ(
          test, status.code(),
          warning ? asc::ErrorCode::kNumerical : asc::ErrorCode::kOk);
      ASC_DENSE_TEST_EQ(test, report.outcome,
                        warning ? asc::LapackOutcome::kAccuracyWarning
                                : asc::LapackOutcome::kSuccess);
      ASC_DENSE_TEST_EQ(test, report.output_validity,
                        asc::LapackOutputValidity::kComplete);
      if (warning) {
        ASC_DENSE_TEST_CHECK(test, std::isinf(r.values[1]));
        ASC_DENSE_TEST_EQ(test, std::signbit(r.values[1]),
                          mode == Mode::kNegativeInfiniteRotated);
      } else if (mode == Mode::kPass) {
        ASC_DENSE_TEST_CHECK(
            test, asc_rotation_numerical::Accurate(T{3}, T{4}, c.values[1],
                                                   s.values[1], r.values[1]));
      } else {
        const bool zero = mode == Mode::kZeroTuple;
        ASC_DENSE_TEST_EQ(test, c.values[1], zero ? T{0} : T{0.5});
        ASC_DENSE_TEST_EQ(test, s.values[1], zero ? T{0} : T{0.25});
        ASC_DENSE_TEST_EQ(test, r.values[1], zero ? T{0} : T{17});
        ASC_DENSE_TEST_CHECK(
            test, !asc_rotation_numerical::Accurate(T{3}, T{4}, c.values[1],
                                                    s.values[1], r.values[1]));
      }
    }
    for (const auto* output : {&c, &s, &r}) {
      ASC_DENSE_TEST_EQ(test, output->values[0], T{71});
      ASC_DENSE_TEST_EQ(test, output->values[2], T{73});
    }
  }
}
}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard guard;
  if (argc != 2) {
    return 2;
  }
  TestContext test;
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
