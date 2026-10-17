#include <array>
#include <cmath>
#include <cstddef>
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
#include "asc/dense/providers/lapack_scalar_norm.h"
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
bool Run(const asc::ReferenceLapackProvider& provider, int exponent, bool three,
         bool negative_x, bool negative_y, bool negative_z) {
  const T x = std::ldexp(negative_x ? -T{3} : T{3}, exponent);
  const T y = std::ldexp(negative_y ? -T{4} : T{4}, exponent);
  const T z = std::ldexp(negative_z ? -T{12} : T{12}, exponent);
  const T expected = std::ldexp(three ? T{13} : T{5}, exponent);
  std::array<T, 3> output{T{71}, T{-19}, T{73}};
  const auto view = Take(asc::DenseBlasVectorView<T>::Create(
      output.data() + 1, 1, 1,
      {output.data(), sizeof(output), asc::MemorySpace::kHost}));
  asc::LapackReport report;
  const auto status = three ? asc::Lapy3(provider, x, y, z, view, report)
                            : asc::Lapy2(provider, x, y, view, report);
  if (!status.ok() || !report.called_provider ||
      report.native_info.has_value() ||
      report.output_validity != asc::LapackOutputValidity::kComplete ||
      !std::isfinite(output[1]) || output[1] < 0 ||
      std::abs(output[1] / expected - 1) >
          4 * std::numeric_limits<T>::epsilon() ||
      output[0] != T{71} || output[2] != T{73}) {
    return false;
  }
  const T infinity = std::numeric_limits<T>::infinity();
  const auto warning =
      three ? asc::Lapy3(provider, infinity, T{0}, T{0}, view, report)
            : asc::Lapy2(provider, infinity, T{0}, view, report);
  if (warning.code() != asc::ErrorCode::kNumerical || !std::isinf(output[1]) ||
      report.native_info.has_value() ||
      report.outcome != asc::LapackOutcome::kAccuracyWarning) {
    return false;
  }
  const auto wrong = Take(asc::DenseBlasVectorView<T>::Create(
      output.data() + 1, 0, 1,
      {output.data(), sizeof(output), asc::MemorySpace::kHost}));
  const auto rejected = three ? asc::Lapy3(provider, x, y, z, wrong, report)
                              : asc::Lapy2(provider, x, y, wrong, report);
  return rejected.code() == asc::ErrorCode::kShape && !report.called_provider &&
         !report.native_info.has_value() && std::isinf(output[1]) &&
         output[0] == T{71} && output[2] == T{73};
}
}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard guard;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  bool passed = true;
  std::size_t count = 0;
  for (bool three : {false, true}) {
    for (bool nx : {false, true}) {
      for (bool ny : {false, true}) {
        for (bool nz : {false, true}) {
          for (int exponent : {-100, -20, 0, 20, 100}) {
            passed =
                Run<float>(provider, exponent, three, nx, ny, nz) && passed;
            passed =
                Run<double>(provider, exponent, three, nx, ny, nz) && passed;
            count += 2;
          }
          for (int exponent : {-1000, 1000}) {
            passed =
                Run<double>(provider, exponent, three, nx, ny, nz) && passed;
            ++count;
          }
        }
      }
    }
  }
  std::printf("%zu public scalar norm cases: %s\n", count,
              passed ? "passed" : "failed");
  return passed ? 0 : 1;
}
