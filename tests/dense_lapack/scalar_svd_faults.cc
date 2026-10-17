#include "scalar_svd_faults.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <type_traits>

#include "../../src/dense/lapack/internal_scalar_svd_prototypes.h"
#include "installed_lu/scalar_svd_numerical.h"
namespace asc_scalar_svd_fault {
namespace {
thread_local Mode g_mode = Mode::kPass;
thread_local std::size_t g_calls = 0;
thread_local std::size_t g_unexpected = 0;
std::size_t OmittedOutput() {
  switch (g_mode) {
    case Mode::kOmitSmall:
      return 0;
    case Mode::kOmitLarge:
      return 1;
    case Mode::kOmitRightSine:
      return 2;
    case Mode::kOmitRightCosine:
      return 3;
    case Mode::kOmitLeftSine:
      return 4;
    case Mode::kOmitLeftCosine:
      return 5;
    default:
      return 6;
  }
}
template <typename T>
void InjectUncheckedTuple(const std::array<T*, 6>& v) {
  switch (g_mode) {
    case Mode::kWrongFinite:
      *v[0] = 1;
      *v[1] = 2;
      *v[2] = 0;
      *v[3] = 1;
      *v[4] = 0;
      *v[5] = 1;
      break;
    case Mode::kZeroTuple:
      for (T* value : v) {
        *value = 0;
      }
      break;
    default:
      break;
  }
}
template <typename T>
void Inject(T& f, T& g, T& h, const std::array<T*, 6>& v) {
  InjectUncheckedTuple(v);
  const T nan = std::numeric_limits<T>::quiet_NaN();
  const T infinity = std::numeric_limits<T>::infinity();
  constexpr T kLimit = T{1} + T{16} * std::numeric_limits<T>::epsilon();
  switch (g_mode) {
    case Mode::kNanSmall:
      *v[0] = nan;
      break;
    case Mode::kNanLarge:
      *v[1] = nan;
      break;
    case Mode::kNanRightSine:
      *v[2] = nan;
      break;
    case Mode::kNanRightCosine:
      *v[3] = nan;
      break;
    case Mode::kNanLeftSine:
      *v[4] = nan;
      break;
    case Mode::kNanLeftCosine:
      *v[5] = nan;
      break;
    case Mode::kLargeRightSine:
      *v[2] = 2;
      break;
    case Mode::kLargeRightCosine:
      *v[3] = 2;
      break;
    case Mode::kLargeLeftSine:
      *v[4] = 2;
      break;
    case Mode::kLargeLeftCosine:
      *v[5] = 2;
      break;
    case Mode::kInfiniteSmall:
      *v[0] = infinity;
      break;
    case Mode::kPositiveInfiniteLarge:
      *v[1] = infinity;
      break;
    case Mode::kNegativeInfiniteLarge:
      *v[1] = -infinity;
      break;
    case Mode::kInfiniteCoefficient:
      *v[3] = infinity;
      break;
    case Mode::kAtCoefficientLimit:
      *v[5] = kLimit;
      break;
    case Mode::kBeyondCoefficientLimit:
      *v[5] = std::nextafter(kLimit, infinity);
      break;
    case Mode::kChangeF:
      f = -f;
      break;
    case Mode::kChangeG:
      g = -g;
      break;
    case Mode::kChangeH:
      h = -h;
      break;
    case Mode::kUnsorted:
      *v[0] = 17;
      *v[1] = 1;
      break;
    case Mode::kWrongFinite:
    case Mode::kZeroTuple:
    case Mode::kPass:
    case Mode::kOmitSmall:
    case Mode::kOmitLarge:
    case Mode::kOmitRightSine:
    case Mode::kOmitRightCosine:
    case Mode::kOmitLeftSine:
    case Mode::kOmitLeftCosine:
      break;
  }
}
template <typename T, typename Function>
void Observe(Function real, T* f, T* g, T* h, T* small, T* large, T* sr, T* cr,
             T* sl, T* cl) {
  ++g_calls;
  std::array<T, 3> ff{71, *f, 73};
  std::array<T, 3> gg{71, *g, 73};
  std::array<T, 3> hh{71, *h, 73};
  std::array<std::array<T, 3>, 6> result{};
  for (auto& v : result) {
    v = {71, -19, 73};
  }
  real(&ff[1], &gg[1], &hh[1], &result[0][1], &result[1][1], &result[2][1],
       &result[3][1], &result[4][1], &result[5][1]);
  bool intact = asc_scalar_svd_numerical::Bits(ff[1]) ==
                    asc_scalar_svd_numerical::Bits(*f) &&
                asc_scalar_svd_numerical::Bits(gg[1]) ==
                    asc_scalar_svd_numerical::Bits(*g) &&
                asc_scalar_svd_numerical::Bits(hh[1]) ==
                    asc_scalar_svd_numerical::Bits(*h);
  for (const auto* v : {&ff, &gg, &hh}) {
    intact = intact && (*v)[0] == 71 && (*v)[2] == 73;
  }
  for (const auto& v : result) {
    intact = intact && v[0] == 71 && v[2] == 73;
  }
  if (!intact || !asc_scalar_svd_numerical::Accurate(
                     *f, *g, *h, result[0][1], result[1][1], result[2][1],
                     result[3][1], result[4][1], result[5][1])) {
    ++g_unexpected;
  }
  const std::array<T*, 6> outputs{small, large, sr, cr, sl, cl};
  for (std::size_t i = 0; i < outputs.size(); ++i) {
    if (i != OmittedOutput()) {
      *outputs[i] = result[i][1];
    }
  }
  Inject(*f, *g, *h, outputs);
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
extern "C" decltype(ASC_LAPACK_slasv2) __real_slasv2_;
extern "C" void __wrap_slasv2_(float* f, float* g, float* h, float* small,
                               float* large, float* sr, float* cr, float* sl,
                               float* cl) {
  Observe(__real_slasv2_, f, g, h, small, large, sr, cr, sl, cl);
}
static_assert(
    std::is_same_v<decltype(__wrap_slasv2_), decltype(ASC_LAPACK_slasv2)>);
extern "C" decltype(ASC_LAPACK_dlasv2) __real_dlasv2_;
extern "C" void __wrap_dlasv2_(double* f, double* g, double* h, double* small,
                               double* large, double* sr, double* cr,
                               double* sl, double* cl) {
  Observe(__real_dlasv2_, f, g, h, small, large, sr, cr, sl, cl);
}
static_assert(
    std::is_same_v<decltype(__wrap_dlasv2_), decltype(ASC_LAPACK_dlasv2)>);
// NOLINTEND(bugprone-reserved-identifier)
}  // namespace asc_scalar_svd_fault
