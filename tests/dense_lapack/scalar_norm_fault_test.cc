#include <array>
#include <cmath>
#include <limits>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "installed_lu/normal_return_guard.h"
#include "scalar_norm_faults.h"
#include "scalar_norm_test_support.h"
namespace {
namespace norm = asc_scalar_norm_test;
namespace fault = asc_scalar_norm_fault;
using fault::Mode;
using norm::TestContext;
template <typename T>
T Expected(Mode mode, bool three) {
  switch (mode) {
    case Mode::kZero:
      return 0;
    case Mode::kNegativeZero:
      return -T{0};
    case Mode::kNegative:
      return -1;
    case Mode::kTiny:
      return std::numeric_limits<T>::denorm_min();
    case Mode::kMaximum:
      return std::numeric_limits<T>::max();
    case Mode::kInfinity:
      return std::numeric_limits<T>::infinity();
    case Mode::kNegativeInfinity:
      return -std::numeric_limits<T>::infinity();
    case Mode::kNan:
      return std::numeric_limits<T>::quiet_NaN();
    case Mode::kPass:
      return three ? T{13} : T{5};
  }
  return T{};
}
template <typename T>
void Faults(TestContext& test, const asc::ReferenceLapackProvider& provider,
            bool three) {
  for (auto mode : {Mode::kPass, Mode::kZero, Mode::kNegativeZero,
                    Mode::kNegative, Mode::kTiny, Mode::kMaximum,
                    Mode::kInfinity, Mode::kNegativeInfinity, Mode::kNan}) {
    norm::Output<T> output;
    const auto before = output.values;
    const auto view = output.View();
    asc::LapackReport report;
    fault::Reset(mode);
    const auto status = norm::WithoutAllocation(test, [&] {
      return norm::Execute(provider, T{3}, T{4}, T{12}, three, view, report);
    });
    ASC_DENSE_TEST_EQ(test, fault::Calls(), 1U);
    ASC_DENSE_TEST_EQ(test, fault::UnexpectedNativeResults(), 0U);
    ASC_DENSE_TEST_CHECK(test, report.called_provider);
    ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
    const T expected = Expected<T>(mode, three);
    if (expected < 0) {
      ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
      ASC_DENSE_TEST_EQ(test, output.values, before);
      ASC_DENSE_TEST_EQ(test, report.outcome,
                        asc::LapackOutcome::kPartialResult);
      ASC_DENSE_TEST_EQ(test, report.output_validity,
                        asc::LapackOutputValidity::kUnchanged);
    } else {
      ASC_DENSE_TEST_EQ(test, status.code(),
                        std::isfinite(expected) ? asc::ErrorCode::kOk
                                                : asc::ErrorCode::kNumerical);
      ASC_DENSE_TEST_EQ(test, norm::Bits(output.values[1]),
                        norm::Bits(expected));
      ASC_DENSE_TEST_EQ(test, report.outcome,
                        std::isfinite(expected)
                            ? asc::LapackOutcome::kSuccess
                            : asc::LapackOutcome::kAccuracyWarning);
      ASC_DENSE_TEST_EQ(test, report.output_validity,
                        asc::LapackOutputValidity::kComplete);
    }
    ASC_DENSE_TEST_EQ(test, output.values[0], T{71});
    ASC_DENSE_TEST_EQ(test, output.values[2], T{73});
  }
}
template <typename T>
void Invalid(TestContext& test, const asc::ReferenceLapackProvider& provider,
             bool three) {
  norm::Output<T> output;
  const auto before = output.values;
  for (int size : {0, 1, 2}) {
    for (int increment : {-1, 1, 2}) {
      if (size == 1 && increment == 1) {
        continue;
      }
      std::array<T, 8> data{};
      data.fill(T{-37});
      const auto view = norm::Take(asc::DenseBlasVectorView<T>::Create(
          data.data() + 2, size, increment,
          {data.data(), sizeof(data), norm::kHost}));
      asc::LapackReport report;
      fault::Reset(Mode::kPass);
      ASC_DENSE_TEST_EQ(
          test,
          norm::Execute(provider, T{3}, T{4}, T{12}, three, view, report)
              .code(),
          asc::ErrorCode::kShape);
      ASC_DENSE_TEST_EQ(test, fault::Calls(), 0U);
      ASC_DENSE_TEST_CHECK(
          test, !report.called_provider && !report.native_info.has_value());
      for (T value : data) {
        ASC_DENSE_TEST_EQ(test, value, T{-37});
      }
    }
  }
  for (auto space : {asc::MemorySpace::kDevice, asc::MemorySpace::kManaged}) {
    const auto view = norm::Take(asc::DenseBlasVectorView<T>::Create(
        output.values.data() + 1, 1, 1,
        {output.values.data(), sizeof(output.values), space}));
    asc::LapackReport report;
    fault::Reset(Mode::kPass);
    ASC_DENSE_TEST_EQ(
        test,
        norm::Execute(provider, T{3}, T{4}, T{12}, three, view, report).code(),
        asc::ErrorCode::kMemoryAccess);
    ASC_DENSE_TEST_EQ(test, fault::Calls(), 0U);
    ASC_DENSE_TEST_EQ(test, output.values, before);
  }
  asc::LapackReport report;
  report.called_provider = true;
  report.native_info = 79;
  const auto aliased = norm::Take(asc::DenseBlasVectorView<T>::Create(
      reinterpret_cast<T*>(&report), 1, 1,
      {&report, sizeof(report), norm::kHost}));
  fault::Reset(Mode::kPass);
  ASC_DENSE_TEST_EQ(
      test,
      norm::Execute(provider, T{3}, T{4}, T{12}, three, aliased, report).code(),
      asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, fault::Calls(), 0U);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(0), 79);
}
template <typename T>
void Run(TestContext& test, bool three) {
  const auto provider = norm::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  Faults<T>(test, provider, three);
  Invalid<T>(test, provider, three);
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard guard;
  if (argc != 2) {
    return 2;
  }
  const std::string_view scalar(argv[1]);
  TestContext test;
  if (scalar == "s2" || scalar == "s3") {
    Run<float>(test, scalar == "s3");
  } else if (scalar == "d2" || scalar == "d3") {
    Run<double>(test, scalar == "d3");
  } else {
    return 2;
  }
  return test.Finish();
}
