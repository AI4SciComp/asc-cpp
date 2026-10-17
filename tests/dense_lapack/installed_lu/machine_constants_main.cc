
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_machine.h"
#include "machine_constants_numerical.h"
#include "normal_return_guard.h"
namespace {
template <typename T>
T Take(asc::Result<T> result) {
  if (!result.ok()) {
    std::abort();
  }
  return std::move(*result);
}
template <typename T>
auto View(std::array<T, 3>& values) {
  return Take(asc::DenseBlasVectorView<T>::Create(
      values.data() + 1, 1, 1,
      {values.data(), sizeof(values), asc::MemorySpace::kHost}));
}
template <typename T>
bool Case(const asc::ReferenceLapackProvider& provider,
          asc_machine_constants_numerical::Case<T> fixture, T old) {
  std::array<T, 3> output{71, old, 73};
  const auto view = View(output);
  asc::LapackReport report;
  for (int repeat = 0; repeat < 2; ++repeat) {
    const auto status = asc::Lamch(provider, fixture.parameter, view, report);
    if (!status.ok() || !report.called_provider ||
        report.native_info.has_value() ||
        report.output_validity != asc::LapackOutputValidity::kComplete ||
        report.outcome != asc::LapackOutcome::kSuccess ||
        asc_machine_constants_numerical::Bits(output[1]) !=
            asc_machine_constants_numerical::Bits(fixture.expected) ||
        output[0] != 71 || output[2] != 73) {
      return false;
    }
  }
  const auto before = output;
  const auto rejected = asc::Lamch(
      provider, static_cast<asc::LapackMachineParameter>(255), view, report);
  return rejected.code() == asc::ErrorCode::kInvalidArgument &&
         !report.called_provider && !report.native_info.has_value() &&
         report.output_validity == asc::LapackOutputValidity::kUnchanged &&
         output == before;
}

template <typename T>
bool Run(const asc::ReferenceLapackProvider& provider) {
  bool pass = true;
  for (const auto fixture : asc_machine_constants_numerical::Cases<T>()) {
    for (const T old : {T{-19}, -T{0}, std::numeric_limits<T>::quiet_NaN()}) {
      pass = Case(provider, fixture, old) && pass;
    }
  }
  return pass;
}

}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard guard;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const bool single = Run<float>(provider);
  const bool dual = Run<double>(provider);
  std::printf(
      "60 public LAMCH selector/output fixtures, repeated calls and invalid "
      "parameter "
      "rejection: %s\n",
      single && dual ? "passed" : "failed");
  return single && dual ? 0 : 1;
}
