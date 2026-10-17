#include "machine_faults.h"

#include <array>
#include <cstddef>
#include <cstring>
#include <type_traits>

#include "../../src/dense/lapack/internal_machine_prototypes.h"
#include "installed_lu/machine_numerical.h"

namespace asc_machine_fault {
namespace {
thread_local Mode g_mode = Mode::kPass;
thread_local std::size_t g_calls = 0;
thread_local std::size_t g_unexpected = 0;

template <typename T>
void FlipBit(T* value) {
  auto bytes = asc_machine_numerical::Bits(*value);
  bytes[0] ^= std::byte{1};
  std::memcpy(value, bytes.data(), sizeof(T));
}

template <typename T, typename Function>
void Observe(Function real, T* small, T* large) {
  ++g_calls;
  std::array<T, 3> s{71, *small, 73};
  std::array<T, 3> l{71, *large, 73};
  real(&s[1], &l[1]);
  if (asc_machine_numerical::Bits(s[1]) !=
          asc_machine_numerical::Bits(*small) ||
      asc_machine_numerical::Bits(l[1]) !=
          asc_machine_numerical::Bits(*large) ||
      s[0] != 71 || s[2] != 73 || l[0] != 71 || l[2] != 73) {
    ++g_unexpected;
  }
  std::memcpy(small, &s[1], sizeof(T));
  std::memcpy(large, &l[1], sizeof(T));
  // Modify only the adapter's live private copies, after a guarded real call.
  if (g_mode == Mode::kSmallBit || g_mode == Mode::kBothBits) {
    FlipBit(small);
  }
  if (g_mode == Mode::kLargeBit || g_mode == Mode::kBothBits) {
    FlipBit(large);
  }
  if (g_mode == Mode::kSwap) {
    const auto old_small = asc_machine_numerical::Bits(*small);
    std::memcpy(small, large, sizeof(T));
    std::memcpy(large, old_small.data(), sizeof(T));
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
extern "C" decltype(ASC_LAPACK_slabad) __real_slabad_;
extern "C" void __wrap_slabad_(float* small, float* large) {
  Observe(__real_slabad_, small, large);
}
static_assert(
    std::is_same_v<decltype(__wrap_slabad_), decltype(ASC_LAPACK_slabad)>);
extern "C" decltype(ASC_LAPACK_dlabad) __real_dlabad_;
extern "C" void __wrap_dlabad_(double* small, double* large) {
  Observe(__real_dlabad_, small, large);
}
static_assert(
    std::is_same_v<decltype(__wrap_dlabad_), decltype(ASC_LAPACK_dlabad)>);
// NOLINTEND(bugprone-reserved-identifier)
}  // namespace asc_machine_fault
