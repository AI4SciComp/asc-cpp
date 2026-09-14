#include <array>
#include <cmath>
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
#include "asc/dense/providers/lapack_rotation.h"
#include "normal_return_guard.h"
#include "rotation_numerical.h"

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
bool Case(const asc::ReferenceLapackProvider& provider, T f, T g) {
  std::array<T, 3> c{71, -19, 73};
  auto s = c;
  auto r = c;
  const auto cv = View(c);
  const auto sv = View(s);
  const auto rv = View(r);
  asc::LapackReport report;
  for (int repeat = 0; repeat < 2; ++repeat) {
    const auto status = asc::Lartg(provider, f, g, cv, sv, rv, report);
    if (status.code() != (std::isfinite(r[1]) ? asc::ErrorCode::kOk
                                              : asc::ErrorCode::kNumerical) ||
        !report.called_provider || report.native_info.has_value() ||
        report.output_validity != asc::LapackOutputValidity::kComplete ||
        report.outcome != (std::isfinite(r[1])
                               ? asc::LapackOutcome::kSuccess
                               : asc::LapackOutcome::kAccuracyWarning) ||
        !asc_rotation_numerical::Accurate(f, g, c[1], s[1], r[1])) {
      return false;
    }
    for (const auto* values : {&c, &s, &r}) {
      if ((*values)[0] != 71 || (*values)[2] != 73) {
        return false;
      }
    }
  }
  const auto cb = c;
  const auto sb = s;
  const auto rb = r;
  const auto rejected = asc::Lartg(provider, std::numeric_limits<T>::infinity(),
                                   T{0}, cv, sv, rv, report);
  return rejected.code() == asc::ErrorCode::kNumerical &&
         !report.called_provider && !report.native_info.has_value() &&
         report.output_validity == asc::LapackOutputValidity::kUnchanged &&
         c == cb && s == sb && r == rb;
}

template <typename T>
bool Run(const asc::ReferenceLapackProvider& provider) {
  bool pass = true;
  for (const T f : asc_rotation_numerical::Values<T>()) {
    for (const T g : asc_rotation_numerical::Values<T>()) {
      pass = Case(provider, f, g) && pass;
    }
  }
  return Case(provider, T{3}, T{4}) && pass;
}
}  // namespace

int main() {
  const asc_lapack_test::NormalReturnGuard guard;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const bool single = Run<float>(provider);
  const bool dual = Run<double>(provider);
  std::printf(
      "2050 public real rotation fixtures, repeated outputs and nonfinite "
      "rejection: %s\n",
      single && dual ? "passed" : "failed");
  return single && dual ? 0 : 1;
}
