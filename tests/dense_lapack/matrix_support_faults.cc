#include "matrix_support_faults.h"

#include <array>
#include <complex>
#include <cstddef>
#include <type_traits>

#include "../../src/dense/lapack/internal_matrix_support_prototypes.h"
#include "../../src/dense/lapack/internal_tridiagonal.h"
#include "installed_lu/matrix_support_numerical.h"
namespace asc_matrix_support_fault {
namespace {
thread_local Mode g_mode = Mode::kPass;
thread_local std::size_t g_calls = 0;
thread_local std::size_t g_unexpected = 0;
template <bool Rows, typename T, typename Function>
lapack_int Observe(Function real, lapack_int* m, lapack_int* n, T* a,
                   lapack_int* leading) {
  ++g_calls;
  // Only valid bounded fixtures enter the real routine. Mutate its private
  // integer arguments or returned value afterwards; never corrupt caller A.
  std::array<lapack_int, 3> mm{71, *m, 73};
  std::array<lapack_int, 3> nn{71, *n, 73};
  std::array<lapack_int, 3> ld{71, *leading, 73};
  const auto before_m = mm;
  const auto before_n = nn;
  const auto before_ld = ld;
  const auto expected = asc_matrix_support_numerical::Expected<Rows>(
      a, static_cast<int>(*m), static_cast<int>(*n), static_cast<int>(*leading),
      false);
  lapack_int result = real(&mm[1], &nn[1], a, &ld[1]);
  if (mm != before_m || nn != before_n || ld != before_ld ||
      result != expected) {
    ++g_unexpected;
  }
  switch (g_mode) {
    case Mode::kNegative:
      result = -1;
      break;
    case Mode::kTooLarge:
      result = (Rows ? *m : *n) + 1;
      break;
    case Mode::kChangeM:
      ++*m;
      break;
    case Mode::kChangeN:
      ++*n;
      break;
    case Mode::kChangeLeading:
      ++*leading;
      break;
    case Mode::kWrongInRange:
      result = 0;
      break;
    case Mode::kPass:
      break;
  }
  return result;
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
extern "C" decltype(ASC_LAPACK_ilaslc) __real_ilaslc_;
extern "C" lapack_int __wrap_ilaslc_(lapack_int* m, lapack_int* n, float* a,
                                     lapack_int* leading) {
  return Observe<false>(__real_ilaslc_, m, n, a, leading);
}
static_assert(
    std::is_same_v<decltype(__wrap_ilaslc_), decltype(ASC_LAPACK_ilaslc)>);
extern "C" decltype(ASC_LAPACK_ilaslr) __real_ilaslr_;
extern "C" lapack_int __wrap_ilaslr_(lapack_int* m, lapack_int* n, float* a,
                                     lapack_int* leading) {
  return Observe<true>(__real_ilaslr_, m, n, a, leading);
}
static_assert(
    std::is_same_v<decltype(__wrap_ilaslr_), decltype(ASC_LAPACK_ilaslr)>);
extern "C" decltype(ASC_LAPACK_iladlc) __real_iladlc_;
extern "C" lapack_int __wrap_iladlc_(lapack_int* m, lapack_int* n, double* a,
                                     lapack_int* leading) {
  return Observe<false>(__real_iladlc_, m, n, a, leading);
}
static_assert(
    std::is_same_v<decltype(__wrap_iladlc_), decltype(ASC_LAPACK_iladlc)>);
extern "C" decltype(ASC_LAPACK_iladlr) __real_iladlr_;
extern "C" lapack_int __wrap_iladlr_(lapack_int* m, lapack_int* n, double* a,
                                     lapack_int* leading) {
  return Observe<true>(__real_iladlr_, m, n, a, leading);
}
static_assert(
    std::is_same_v<decltype(__wrap_iladlr_), decltype(ASC_LAPACK_iladlr)>);
extern "C" decltype(ASC_LAPACK_ilaclc) __real_ilaclc_;
extern "C" lapack_int __wrap_ilaclc_(lapack_int* m, lapack_int* n,
                                     std::complex<float>* a,
                                     lapack_int* leading) {
  return Observe<false>(__real_ilaclc_, m, n, a, leading);
}
static_assert(
    std::is_same_v<decltype(__wrap_ilaclc_), decltype(ASC_LAPACK_ilaclc)>);
extern "C" decltype(ASC_LAPACK_ilaclr) __real_ilaclr_;
extern "C" lapack_int __wrap_ilaclr_(lapack_int* m, lapack_int* n,
                                     std::complex<float>* a,
                                     lapack_int* leading) {
  return Observe<true>(__real_ilaclr_, m, n, a, leading);
}
static_assert(
    std::is_same_v<decltype(__wrap_ilaclr_), decltype(ASC_LAPACK_ilaclr)>);
extern "C" decltype(ASC_LAPACK_ilazlc) __real_ilazlc_;
extern "C" lapack_int __wrap_ilazlc_(lapack_int* m, lapack_int* n,
                                     std::complex<double>* a,
                                     lapack_int* leading) {
  return Observe<false>(__real_ilazlc_, m, n, a, leading);
}
static_assert(
    std::is_same_v<decltype(__wrap_ilazlc_), decltype(ASC_LAPACK_ilazlc)>);
extern "C" decltype(ASC_LAPACK_ilazlr) __real_ilazlr_;
extern "C" lapack_int __wrap_ilazlr_(lapack_int* m, lapack_int* n,
                                     std::complex<double>* a,
                                     lapack_int* leading) {
  return Observe<true>(__real_ilazlr_, m, n, a, leading);
}
static_assert(
    std::is_same_v<decltype(__wrap_ilazlr_), decltype(ASC_LAPACK_ilazlr)>);
// NOLINTEND(bugprone-reserved-identifier)
}  // namespace asc_matrix_support_fault
