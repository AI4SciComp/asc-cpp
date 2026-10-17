#include "rotation_faults.h"

#include <array>
#include <cstddef>
#include <limits>
#include <type_traits>

#include "../../src/dense/lapack/internal_rotation_prototypes.h"
#include "installed_lu/rotation_numerical.h"

namespace asc_rotation_fault {
namespace {
thread_local Mode g_mode = Mode::kPass;
thread_local std::size_t g_calls = 0;
thread_local std::size_t g_unexpected = 0;

template <typename T>
void Inject(T& f, T& g, T& c, T& s, T& r) {
  const T nan = std::numeric_limits<T>::quiet_NaN();
  const T infinity = std::numeric_limits<T>::infinity();
  switch (g_mode) {
    case Mode::kNegativeCosine:
      c = -1;
      break;
    case Mode::kLargeCosine:
      c = 2;
      break;
    case Mode::kNanCosine:
      c = nan;
      break;
    case Mode::kInfiniteCosine:
      c = infinity;
      break;
    case Mode::kLargeSine:
      s = -2;
      break;
    case Mode::kNanSine:
      s = nan;
      break;
    case Mode::kInfiniteSine:
      s = infinity;
      break;
    case Mode::kNanRotated:
      r = nan;
      break;
    case Mode::kInfiniteRotated:
      r = infinity;
      break;
    case Mode::kNegativeInfiniteRotated:
      r = -infinity;
      break;
    case Mode::kChangeF:
      f = -f;
      break;
    case Mode::kChangeG:
      g = -g;
      break;
    case Mode::kWrongFinite:
      c = T{0.5};
      s = T{0.25};
      r = 17;
      break;
    case Mode::kZeroTuple:
      c = 0;
      s = 0;
      r = 0;
      break;
    case Mode::kPass:
    case Mode::kOmitCosine:
    case Mode::kOmitSine:
    case Mode::kOmitRotated:
      break;
  }
}

template <typename T, typename Function>
void Observe(Function real, T* f, T* g, T* c, T* s, T* r) {
  ++g_calls;
  std::array<T, 3> ff{71, *f, 73};
  std::array<T, 3> gg{71, *g, 73};
  std::array<T, 3> cc{71, -7, 73};
  auto ss = cc;
  auto rr = cc;
  real(&ff[1], &gg[1], &cc[1], &ss[1], &rr[1]);
  bool intact =
      asc_rotation_numerical::Bits(ff[1]) == asc_rotation_numerical::Bits(*f) &&
      asc_rotation_numerical::Bits(gg[1]) == asc_rotation_numerical::Bits(*g);
  for (const auto* values : {&ff, &gg, &cc, &ss, &rr}) {
    intact = intact && (*values)[0] == 71 && (*values)[2] == 73;
  }
  if (!intact ||
      !asc_rotation_numerical::Accurate(*f, *g, cc[1], ss[1], rr[1])) {
    ++g_unexpected;
  }
  if (g_mode != Mode::kOmitCosine) {
    *c = cc[1];
  }
  if (g_mode != Mode::kOmitSine) {
    *s = ss[1];
  }
  if (g_mode != Mode::kOmitRotated) {
    *r = rr[1];
  }
  Inject(*f, *g, *c, *s, *r);
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
extern "C" decltype(ASC_LAPACK_slartg) __real_slartg_;
extern "C" void __wrap_slartg_(float* f, float* g, float* c, float* s,
                               float* r) {
  Observe(__real_slartg_, f, g, c, s, r);
}
static_assert(
    std::is_same_v<decltype(__wrap_slartg_), decltype(ASC_LAPACK_slartg)>);
extern "C" decltype(ASC_LAPACK_dlartg) __real_dlartg_;
extern "C" void __wrap_dlartg_(double* f, double* g, double* c, double* s,
                               double* r) {
  Observe(__real_dlartg_, f, g, c, s, r);
}
static_assert(
    std::is_same_v<decltype(__wrap_dlartg_), decltype(ASC_LAPACK_dlartg)>);
// NOLINTEND(bugprone-reserved-identifier)
}  // namespace asc_rotation_fault
