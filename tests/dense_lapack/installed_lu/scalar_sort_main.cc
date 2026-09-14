#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
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
#include "asc/dense/providers/lapack_scalar_sort.h"
#include "normal_return_guard.h"
namespace {
template <typename T>
T Take(asc::Result<T> value) {
  if (!value.ok()) {
    std::abort();
  }
  return std::move(*value);
}
template <typename T>
auto Bits(T value) {
  return std::bit_cast<
      std::conditional_t<sizeof(T) == 4, std::uint32_t, std::uint64_t>>(value);
}
template <typename T>
T Value(std::size_t i, std::size_t n, unsigned pattern, std::size_t seed) {
  const T tiny = std::numeric_limits<T>::denorm_min();
  const T small = std::numeric_limits<T>::min();
  const T large = std::numeric_limits<T>::max();
  const T infinity = std::numeric_limits<T>::infinity();
  const T nan = std::numeric_limits<T>::quiet_NaN();
  const std::array<T, 10> special{-large, -small, -tiny, -T{0},    0,
                                  tiny,   small,  large, infinity, -infinity};
  const std::size_t midpoint =
      n / 2;  // Integer index, intentionally rounded down.
  switch (pattern) {
    case 0:
      return static_cast<T>(i) - static_cast<T>(midpoint);
    case 1:
      return static_cast<T>(n - i) - static_cast<T>(midpoint);
    case 2:
      return (i + seed) % 2 != 0 ? -T{0} : T{0};
    case 3:
      return static_cast<T>((i * 37 + seed * 17) % 7) - 3;
    case 4:
      return special[(i * 37 + seed * 17) % special.size()];
    case 5:
      return (i + seed) % 5 == 0 ? nan : special[(i + seed) % special.size()];
    case 6:
      return i == 0 || i == n / 2 || i + 1 == n
                 ? nan
                 : static_cast<T>((i * 37 + seed) % 101) - 50;
    default:
      return static_cast<T>(i <= n / 2 ? i : n - i) * (seed % 2 != 0 ? -1 : 1);
  }
}
asc::LapackSortOrder UnknownOrder() {
  // Fixed uint8_t underlying type permits255; exercise unknown-enumerator
  // rejection. NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  return static_cast<asc::LapackSortOrder>(255);
}
template <typename T>
bool Case(const asc::ReferenceLapackProvider& provider, asc::extent_t n,
          asc::LapackSortOrder order, unsigned pattern, unsigned seed) {
  std::array<T, 259> values;
  values.fill(T{79});
  values.front() = -T{71};
  values.back() = -T{73};
  bool nan = false;
  for (asc::extent_t i = 0; i < n; ++i) {
    const auto index = static_cast<std::size_t>(i);
    values[index + 1] =
        Value<T>(index, static_cast<std::size_t>(n), pattern, seed);
    nan = nan || std::isnan(values[index + 1]);
  }
  const auto before = values;
  const auto view = Take(asc::DenseBlasVectorView<T>::Create(
      values.data() + 1, n, 1,
      {values.data(), sizeof(values), asc::MemorySpace::kHost}));
  asc::LapackReport report;
  const auto status = asc::Lasrt(provider, order, view, report);
  if (status.code() !=
          (nan ? asc::ErrorCode::kNumerical : asc::ErrorCode::kOk) ||
      report.called_provider != (n != 0) ||
      report.native_info.has_value() != (n != 0) ||
      (report.native_info.has_value() && *report.native_info != 0) ||
      report.native_argument.has_value() ||
      report.output_validity != asc::LapackOutputValidity::kComplete ||
      report.outcome != (nan ? asc::LapackOutcome::kAccuracyWarning
                             : asc::LapackOutcome::kSuccess)) {
    return false;
  }
  std::array<decltype(Bits(T{})), 257> original{};
  std::array<decltype(Bits(T{})), 257> actual{};
  for (asc::extent_t i = 0; i < n; ++i) {
    const auto index = static_cast<std::size_t>(i);
    original[index] = Bits(before[index + 1]);
    actual[index] = Bits(values[index + 1]);
    if (!nan && i > 0 &&
        !(order == asc::LapackSortOrder::kIncreasing
              ? values[index] <= values[index + 1]
              : values[index] >= values[index + 1])) {
      return false;
    }
  }
  std::sort(original.begin(), original.begin() + n);
  std::sort(actual.begin(), actual.begin() + n);
  if (original != actual) {
    return false;
  }
  for (std::size_t i = 0; i < values.size(); ++i) {
    if ((i == 0 || i > static_cast<std::size_t>(n)) &&
        Bits(values[i]) != Bits(before[i])) {
      return false;
    }
  }
  const auto sorted = values;
  if (asc::Lasrt(provider, UnknownOrder(), view, report).code() !=
          asc::ErrorCode::kInvalidArgument ||
      report.called_provider || report.native_info.has_value()) {
    return false;
  }
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (Bits(values[i]) != Bits(sorted[i])) {
      return false;
    }
  }
  return true;
}
template <typename T>
bool Run(const asc::ReferenceLapackProvider& provider) {
  bool passed = true;
  for (asc::extent_t n : {0, 1, 2, 5, 20, 21, 22, 33, 65, 257}) {
    for (auto order : {asc::LapackSortOrder::kIncreasing,
                       asc::LapackSortOrder::kDecreasing}) {
      for (unsigned pattern = 0; pattern < 8; ++pattern) {
        for (unsigned seed = 0; seed < 4; ++seed) {
          passed = Case<T>(provider, n, order, pattern, seed) && passed;
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
  std::printf("1280 public scalar sort cases: %s\n",
              single && dual ? "passed" : "failed");
  return single && dual ? 0 : 1;
}
