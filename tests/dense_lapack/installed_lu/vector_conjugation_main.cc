#include <array>
#include <bit>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <type_traits>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_vector_conjugation.h"
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
auto Bits(T value) {
  return std::bit_cast<
      std::conditional_t<sizeof(T) == 4, std::uint32_t, std::uint64_t>>(value);
}
template <typename T>
std::array<T, 8> Values() {
  return {0,
          -T{0},
          std::numeric_limits<T>::denorm_min(),
          1,
          -3,
          std::numeric_limits<T>::max(),
          std::numeric_limits<T>::infinity(),
          std::numeric_limits<T>::quiet_NaN()};
}
template <typename T>
bool Case(const asc::ReferenceLapackProvider& provider, asc::extent_t n,
          asc::stride_t increment, T real, T imaginary) {
  using Complex = std::complex<T>;
  using Unsigned = decltype(Bits(T{}));
  constexpr Unsigned kSign = Unsigned{1} << (sizeof(T) * 8 - 1);
  const auto magnitude = increment < 0 ? -increment : increment;
  const auto size = n == 0 ? 0 : 1 + (n - 1) * magnitude;
  std::array<Complex, 128> values;
  values.fill(Complex{71, 73});
  for (asc::extent_t i = 0; i < n; ++i) {
    values[static_cast<std::size_t>(i * magnitude) + 1] =
        Complex{real, imaginary};
  }
  auto expected = values;
  for (asc::extent_t i = 0; i < n; ++i) {
    const auto index = static_cast<std::size_t>(i * magnitude) + 1;
    expected[index].imag(
        std::bit_cast<T>(Bits(expected[index].imag()) ^ kSign));
  }
  const auto view = Take(asc::DenseBlasVectorView<Complex>::Create(
      values.data() + 1, size, 1,
      {values.data(), sizeof(values), asc::MemorySpace::kHost}));
  asc::LapackReport report;
  if (!asc::Lacgv(provider, n, increment, view, report).ok() ||
      report.called_provider != (n != 0) || report.native_info.has_value() ||
      report.native_argument.has_value() ||
      report.output_validity != asc::LapackOutputValidity::kComplete ||
      report.outcome != asc::LapackOutcome::kSuccess) {
    return false;
  }
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (Bits(values[i].real()) != Bits(expected[i].real()) ||
        Bits(values[i].imag()) != Bits(expected[i].imag())) {
      return false;
    }
  }
  if (asc::Lacgv(provider, -1, increment, view, report).code() !=
          asc::ErrorCode::kInvalidArgument ||
      report.called_provider || report.native_info.has_value()) {
    return false;
  }
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (Bits(values[i].real()) != Bits(expected[i].real()) ||
        Bits(values[i].imag()) != Bits(expected[i].imag())) {
      return false;
    }
  }
  return true;
}
template <typename T>
bool Run(const asc::ReferenceLapackProvider& provider) {
  bool passed = true;
  for (T real : Values<T>()) {
    for (T imaginary : Values<T>()) {
      for (asc::extent_t n : {0, 1, 2, 5, 13}) {
        for (asc::stride_t increment : {-5, -2, -1, 0, 1, 2, 5}) {
          passed = Case(provider, n, increment, real, imaginary) && passed;
        }
      }
    }
  }
  return passed;
}
}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard guard;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const bool single = Run<float>(provider);
  const bool dual = Run<double>(provider);
  std::printf("4480 public vector conjugation cases: %s\n",
              single && dual ? "passed" : "failed");
  return single && dual ? 0 : 1;
}
