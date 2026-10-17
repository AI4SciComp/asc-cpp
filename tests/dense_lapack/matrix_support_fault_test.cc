#include <array>
#include <complex>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "installed_lu/normal_return_guard.h"
#include "matrix_support_faults.h"
#include "matrix_support_test_support.h"
namespace {
namespace support = asc_matrix_support_test;
namespace fault = asc_matrix_support_fault;
namespace numerical = asc_matrix_support_numerical;
using fault::Mode;
using support::TestContext;
template <typename T, bool Rows>
void Case(TestContext& test, const asc::ReferenceLapackProvider& provider,
          bool row, Mode mode) {
  support::Matrix<T> matrix(3, 5, row, 2);
  matrix.Fill(16, T{-3});
  const auto before = numerical::Bits(matrix.data);
  const auto view = matrix.View();
  const auto plan = support::Take(support::Query<T, Rows>(provider, view));
  support::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  std::array<asc::index_t, 3> extent{71, -19, 73};
  asc::LapackReport report;
  fault::Reset(mode);
  const auto status = support::WithoutAllocation(test, [&] {
    return support::Execute<T, Rows>(provider, view, extent[1], plan, workspace,
                                     report);
  });
  ASC_DENSE_TEST_EQ(test, fault::Calls(), 1U);
  ASC_DENSE_TEST_EQ(test, fault::UnexpectedNativeResults(), 0U);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
  const bool accepted = mode == Mode::kPass || mode == Mode::kWrongInRange;
  ASC_DENSE_TEST_EQ(test, status.code(),
                    accepted ? asc::ErrorCode::kOk : asc::ErrorCode::kProvider);
  ASC_DENSE_TEST_EQ(test, report.outcome,
                    accepted ? asc::LapackOutcome::kSuccess
                             : asc::LapackOutcome::kPartialResult);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    accepted ? asc::LapackOutputValidity::kComplete
                             : asc::LapackOutputValidity::kUnusable);
  if (!accepted) {
    ASC_DENSE_TEST_EQ(test, extent[1], -19);
  } else if (mode == Mode::kPass) {
    ASC_DENSE_TEST_EQ(test, extent[1], Rows ? 3 : 5);
  } else {
    ASC_DENSE_TEST_EQ(test, extent[1], 0);
    ASC_DENSE_TEST_CHECK(
        test, extent[1] != numerical::Expected<Rows>(matrix.data.data() + 2, 3,
                                                     5, matrix.leading, row));
  }
  ASC_DENSE_TEST_EQ(test, extent[0], 71);
  ASC_DENSE_TEST_EQ(test, extent[2], 73);
  ASC_DENSE_TEST_EQ(test, numerical::Bits(matrix.data), before);
  ASC_DENSE_TEST_CHECK(test, scratch.Guards(workspace));
}
template <typename T, bool Rows>
void Run(TestContext& test) {
  const auto provider = support::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  for (bool row : {false, true}) {
    for (Mode mode :
         {Mode::kPass, Mode::kNegative, Mode::kTooLarge, Mode::kChangeM,
          Mode::kChangeN, Mode::kChangeLeading, Mode::kWrongInRange}) {
      Case<T, Rows>(test, provider, row, mode);
    }
  }
}

template <typename T>
void Dispatch(TestContext& test, std::string_view operation) {
  if (operation == "ilalc") {
    Run<T, false>(test);
  } else {
    Run<T, true>(test);
  }
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard guard;
  if (argc != 3) {
    return 2;
  }
  const std::string_view operation(argv[1]);
  const std::string_view scalar(argv[2]);
  if (operation != "ilalc" && operation != "ilalr") {
    return 2;
  }
  TestContext test;
  if (scalar == "s") {
    Dispatch<float>(test, operation);
  } else if (scalar == "d") {
    Dispatch<double>(test, operation);
  } else if (scalar == "c") {
    Dispatch<std::complex<float>>(test, operation);
  } else if (scalar == "z") {
    Dispatch<std::complex<double>>(test, operation);
  } else {
    return 2;
  }
  return test.Finish();
}
