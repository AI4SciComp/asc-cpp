#include "scalar_sort_faults.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

#include "../../src/dense/lapack/internal_tridiagonal.h"
namespace asc_scalar_sort_fault {
namespace {
thread_local Mode g_mode = Mode::kPass;
thread_local std::size_t g_calls = 0;
thread_local std::size_t g_unexpected = 0;
template <typename T, typename Call>
void Wrap(const char* id, const lapack_int* n, T* data, lapack_int* info,
          std::size_t length, Call call) {
  ++g_calls;
  if (*n != 3 || (*id != 'I' && *id != 'D') || length != 1) {
    ++g_unexpected;
    call(id, n, data, info, length);
    return;
  }
  const lapack_int incoming = *info;
  const std::array<T, 3> before{data[0], data[1], data[2]};
  call(id, n, data, info, length);
  const bool increasing = *id == 'I';
  if (*info != 0 || data[0] != (increasing ? T{1} : T{3}) || data[1] != T{2} ||
      data[2] != (increasing ? T{3} : T{1}) ||
      incoming != std::numeric_limits<lapack_int>::max()) {
    ++g_unexpected;
  }
  switch (g_mode) {
    case Mode::kPass:
      break;
    case Mode::kOmitInfo:
      *info = incoming;
      break;
    case Mode::kPositive:
      *info = 1;
      break;
    case Mode::kNegativeOne:
      *info = -1;
      break;
    case Mode::kNegativeTwo:
      *info = -2;
      break;
    case Mode::kMinimum:
      *info = std::numeric_limits<lapack_int>::min();
      break;
    case Mode::kShortZero: {
      *info = incoming;
      const std::int32_t zero = 0;
      std::memcpy(info, &zero, sizeof(zero));
      break;
    }
    case Mode::kUnsorted:
      data[0] = increasing ? T{4} : T{0};
      break;
    case Mode::kSortedWrong:
      data[0] = data[1] = data[2] = T{42};
      break;
    case Mode::kNan:
      data[0] = std::numeric_limits<T>::quiet_NaN();
      break;
    case Mode::kOmitValues:
      for (std::size_t i = 0; i < before.size(); ++i) {
        data[i] = before[i];
      }
      break;
  }
}
}  // namespace
void Reset(Mode mode) {
  g_mode = mode;
  g_calls = 0;
  g_unexpected = 0;
}
std::size_t Calls() { return g_calls; }
std::size_t UnexpectedNativeResults() { return g_unexpected; }
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" decltype(LAPACK_slasrt_base) __real_slasrt_;
extern "C" void __wrap_slasrt_(const char* id, const lapack_int* n, float* data,
                               lapack_int* info, std::size_t length) {
  Wrap(id, n, data, info, length, __real_slasrt_);
}
extern "C" decltype(LAPACK_dlasrt_base) __real_dlasrt_;
extern "C" void __wrap_dlasrt_(const char* id, const lapack_int* n,
                               double* data, lapack_int* info,
                               std::size_t length) {
  Wrap(id, n, data, info, length, __real_dlasrt_);
}
static_assert(
    std::is_same_v<decltype(__wrap_slasrt_), decltype(LAPACK_slasrt_base)>);
static_assert(
    std::is_same_v<decltype(__wrap_dlasrt_), decltype(LAPACK_dlasrt_base)>);
// NOLINTEND(bugprone-reserved-identifier)
}  // namespace asc_scalar_sort_fault
