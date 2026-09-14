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
#include "asc/dense/providers/lapack_scalar_svd.h"
#include "normal_return_guard.h"
#include "scalar_svd_numerical.h"

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
bool Case(const asc::ReferenceLapackProvider& provider, T f, T g, T h) {
  std::array<std::array<T, 3>, 6> values{};
  for (auto& item : values) {
    item = {71, -19, 73};
  }
  const std::array views{View(values[0]), View(values[1]), View(values[2]),
                         View(values[3]), View(values[4]), View(values[5])};
  asc::LapackReport report;
  for (int repeat = 0; repeat < 2; ++repeat) {
    const auto status =
        asc::Lasv2(provider, f, g, h, views[0], views[1], views[2], views[3],
                   views[4], views[5], report);
    const auto& v = values;
    const bool finite = std::isfinite(v[1][1]);
    if (status.code() !=
            (finite ? asc::ErrorCode::kOk : asc::ErrorCode::kNumerical) ||
        !report.called_provider || report.native_info.has_value() ||
        report.native_argument.has_value() ||
        report.diagnostic_index.has_value() ||
        report.factor_family.has_value() ||
        report.output_validity != asc::LapackOutputValidity::kComplete ||
        report.outcome != (finite ? asc::LapackOutcome::kSuccess
                                  : asc::LapackOutcome::kAccuracyWarning) ||
        !asc_scalar_svd_numerical::Accurate(f, g, h, v[0][1], v[1][1], v[2][1],
                                            v[3][1], v[4][1], v[5][1])) {
      return false;
    }
    for (const auto& item : v) {
      if (item[0] != 71 || item[2] != 73) {
        return false;
      }
    }
  }
  return true;
}

template <typename T>
bool Controls(const asc::ReferenceLapackProvider& provider) {
  std::array<std::array<T, 3>, 6> values{};
  for (auto& item : values) {
    item = {71, -19, 73};
  }
  auto views = std::array{View(values[0]), View(values[1]), View(values[2]),
                          View(values[3]), View(values[4]), View(values[5])};
  asc::LapackReport report;
  const auto execute = [&](T f, T g, T h) {
    return asc::Lasv2(provider, f, g, h, views[0], views[1], views[2], views[3],
                      views[4], views[5], report);
  };
  for (const auto tuple :
       {std::array{std::numeric_limits<T>::quiet_NaN(), T{1}, T{1}},
        std::array{std::numeric_limits<T>::infinity(), T{1},
                   std::numeric_limits<T>::infinity()}}) {
    const auto before = values;
    if (execute(tuple[0], tuple[1], tuple[2]).code() !=
            asc::ErrorCode::kNumerical ||
        report.called_provider || values != before ||
        report.output_validity != asc::LapackOutputValidity::kUnchanged) {
      return false;
    }
  }
  const auto saved = views[5];
  views[5] = views[0];
  const auto before = values;
  if (execute(T{3}, T{4}, T{5}).code() != asc::ErrorCode::kInvalidArgument ||
      report.called_provider || values != before) {
    return false;
  }
  views[5] = saved;
  values[0][1] = 3;
  values[1][1] = 4;
  values[2][1] = 5;
  const auto status = execute(values[0][1], values[1][1], values[2][1]);
  const auto& v = values;
  return status.ok() && report.called_provider &&
         asc_scalar_svd_numerical::Accurate(T{3}, T{4}, T{5}, v[0][1], v[1][1],
                                            v[2][1], v[3][1], v[4][1], v[5][1]);
}

template <typename T>
bool Run(const asc::ReferenceLapackProvider& provider) {
  bool pass = Controls<T>(provider);
  asc_scalar_svd_numerical::ForEach<T>(
      [&](T f, T g, T h) { pass = Case(provider, f, g, h) && pass; });
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
      "77824 public real scalar_svd fixtures, repeated outputs and "
      "nonfinite "
      "rejection: %s\n",
      single && dual ? "passed" : "failed");
  return single && dual ? 0 : 1;
}
