#include "lu_aux_info_test_support.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/status.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "lu_aux_info_faults.h"

namespace asc_lu_aux_info_test {
namespace {
std::size_t g_cases = 0;
}  // namespace
std::array<char, 16> Name(char scalar, std::string_view routine) {
  std::array<char, 16> name{};
  name[0] = scalar;
  std::copy(routine.begin(), routine.end(), name.begin() + 1);
  return name;
}
asc::LapackReport DirtyReport() {
  asc::LapackReport report;
  report.native_info = 701;
  report.native_argument = 702;
  report.diagnostic_index = 703;
  return report;
}
std::size_t Cases() { return g_cases; }
void CheckInfo(TestContext& test, const asc::ReferenceLapackProvider& provider,
               const asc::Status& status, const asc::LapackReport& report,
               bool called, Fault fault, std::string_view routine,
               std::int64_t actual, asc::LapackOutcome normal,
               asc::LapackOutputValidity validity) {
  ++g_cases;
  const auto observed = asc_lapack_test::ObserveLuAuxInfo();
  const bool defect = called && fault != Fault::kNone;
  ASC_DENSE_TEST_EQ(test, observed.calls, called ? 1U : 0U);
  ASC_DENSE_TEST_EQ(test, observed.actual, actual);
  ASC_DENSE_TEST_EQ(test, report.called_provider, called);
  ASC_DENSE_TEST_EQ(test, report.provider, provider.identity());
  ASC_DENSE_TEST_EQ(test, std::string_view(report.routine.data()), routine);
  ASC_DENSE_TEST_CHECK(test, !report.native_argument.has_value());
  if (defect) {
    ASC_DENSE_TEST_EQ(test, observed.incoming, kSentinel);
    ASC_DENSE_TEST_EQ(test, observed.outgoing, kSentinel);
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
    ASC_DENSE_TEST_EQ(test, report.native_info, kSentinel);
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      asc::LapackOutcome::kProviderArgument);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kUnusable);
    ASC_DENSE_TEST_CHECK(test, !report.diagnostic_index.has_value());
  } else {
    ASC_DENSE_TEST_EQ(
        test, status.code(),
        actual == 0 ? asc::ErrorCode::kOk : asc::ErrorCode::kNumerical);
    if (called) {
      ASC_DENSE_TEST_EQ(test, report.native_info, actual);
    } else {
      ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
    }
    ASC_DENSE_TEST_EQ(test, report.outcome, normal);
    ASC_DENSE_TEST_EQ(test, report.output_validity, validity);
  }
  std::printf(
      "LU_AUX_RESULT routine=%.*s calls=%zu actual=%lld incoming=%lld "
      "outgoing=%lld status=%d\n",
      static_cast<int>(routine.size()), routine.data(), observed.calls,
      static_cast<long long>(observed.actual),
      static_cast<long long>(observed.incoming),
      static_cast<long long>(observed.outgoing),
      static_cast<int>(status.code()));
}
}  // namespace asc_lu_aux_info_test
