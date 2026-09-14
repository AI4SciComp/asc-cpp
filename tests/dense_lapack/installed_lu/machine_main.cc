
#include <array>
#include <cstdio>
#include <cstdlib>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_machine.h"
#include "machine_numerical.h"
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
bool Case(const asc::ReferenceLapackProvider& provider, T a, T b) {
  std::array<T, 3> small{71, a, 73};
  std::array<T, 3> large{71, b, 73};
  const auto sv = View(small);
  const auto lv = View(large);
  asc::LapackReport report;
  for (int repeat = 0; repeat < 2; ++repeat) {
    const auto status = asc::Labad(provider, sv, lv, report);
    if (!status.ok() || !report.called_provider ||
        report.native_info.has_value() ||
        report.output_validity != asc::LapackOutputValidity::kComplete ||
        report.outcome != asc::LapackOutcome::kSuccess ||
        asc_machine_numerical::Bits(small[1]) !=
            asc_machine_numerical::Bits(a) ||
        asc_machine_numerical::Bits(large[1]) !=
            asc_machine_numerical::Bits(b)) {
      return false;
    }
    for (const auto* v : {&small, &large}) {
      if ((*v)[0] != 71 || (*v)[2] != 73) {
        return false;
      }
    }
  }
  const auto rejected = asc::Labad(provider, sv, sv, report);
  return rejected.code() == asc::ErrorCode::kInvalidArgument &&
         !report.called_provider && !report.native_info.has_value() &&
         report.output_validity == asc::LapackOutputValidity::kUnchanged &&
         asc_machine_numerical::Bits(small[1]) ==
             asc_machine_numerical::Bits(a) &&
         asc_machine_numerical::Bits(large[1]) ==
             asc_machine_numerical::Bits(b);
}
template <typename T>
bool Run(const asc::ReferenceLapackProvider& provider) {
  bool pass = true;
  for (T a : asc_machine_numerical::Values<T>()) {
    for (T b : asc_machine_numerical::Values<T>()) {
      pass = Case(provider, a, b) && pass;
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
      "800 public LABAD representation fixtures, repeated calls and alias "
      "rejection: %s\n",
      single && dual ? "passed" : "failed");
  return single && dual ? 0 : 1;
}
