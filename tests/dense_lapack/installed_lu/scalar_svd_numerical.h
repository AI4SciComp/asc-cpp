#ifndef ASC_TESTS_DENSE_LAPACK_INSTALLED_LU_SCALAR_SVD_NUMERICAL_H_
#define ASC_TESTS_DENSE_LAPACK_INSTALLED_LU_SCALAR_SVD_NUMERICAL_H_
#include <algorithm>
#include <cmath>
#include <limits>

#include "rotation_numerical.h"
namespace asc_scalar_svd_numerical {
using asc_rotation_numerical::Bits;
using asc_rotation_numerical::Values;
template <typename T>
bool InfiniteAccurate(T f, T g, T h, T sm, T big, T sr, T cr, T sl, T cl) {
  const long double eps = std::numeric_limits<T>::epsilon();
  const long double quantum = std::numeric_limits<T>::denorm_min();
  if (!std::isinf(big)) {
    return false;
  }
  long double expected = 0;
  if (std::isinf(f)) {
    expected = std::abs(static_cast<long double>(h));
  } else if (std::isinf(h)) {
    expected = std::abs(static_cast<long double>(f));
  }
  if (std::abs(std::abs(static_cast<long double>(sm)) - expected) >
      64 * eps * expected + 4 * quantum) {
    return false;
  }
  if (std::isinf(f)) {
    return std::abs(cr) == 1 && sr == 0 && std::abs(cl) == 1 && sl == 0 &&
           std::signbit(big) == std::signbit(cr * cl * std::copysign(T{1}, f));
  }
  if (std::isinf(h)) {
    return cr == 0 && std::abs(sr) == 1 && cl == 0 && std::abs(sl) == 1 &&
           std::signbit(big) == std::signbit(sr * sl * std::copysign(T{1}, h));
  }
  return cr == 0 && std::abs(sr) == 1 && std::abs(cl) == 1 && sl == 0 &&
         std::signbit(big) == std::signbit(sr * cl * std::copysign(T{1}, g));
}
template <typename T>
bool Accurate(T f, T g, T h, T sm, T big, T sr, T cr, T sl, T cl) {
  const long double eps = std::numeric_limits<T>::epsilon();
  const long double quantum = std::numeric_limits<T>::denorm_min();
  const long double srw = sr;
  const long double crw = cr;
  const long double slw = sl;
  const long double clw = cl;
  for (T c : {sr, cr, sl, cl}) {
    if (!std::isfinite(c) ||
        std::abs(static_cast<long double>(c)) > 1 + 16 * eps) {
      return false;
    }
  }
  if (std::abs(srw * srw + crw * crw - 1) > 64 * eps ||
      std::abs(slw * slw + clw * clw - 1) > 64 * eps || !std::isfinite(sm) ||
      std::isnan(big) || std::abs(sm) > std::abs(big)) {
    return false;
  }
  if (std::isinf(f) || std::isinf(g) || std::isinf(h)) {
    return InfiniteAccurate(f, g, h, sm, big, sr, cr, sl, cl);
  }
  const long double fw = f;
  const long double gw = g;
  const long double hw = h;
  // Independent symmetric 2x2 eigenvalue and determinant formulas, in a range
  // wide enough for squares/products of all binary32/binary64 inputs.
  const long double a = fw * fw + gw * gw;
  const long double c = hw * hw;
  const long double b = gw * hw;
  const long double expected_big =
      std::sqrt((a + c + std::hypot(a - c, 2 * b)) / 2);
  const long double expected_small =
      expected_big == 0 ? 0 : std::abs(fw * hw) / expected_big;
  if (std::abs(std::abs(static_cast<long double>(sm)) - expected_small) >
      64 * eps * expected_small + 4 * quantum) {
    return false;
  }
  if (std::isinf(big)) {
    // The source expressly permits overflow within a few ulps of the boundary.
    // The strict initial gate accepts only norms at/above max; any below-max
    // overflow remains a recorded comparison for separate disposition.
    if (expected_big < std::numeric_limits<T>::max()) {
      return false;
    }
  } else if (std::abs(std::abs(static_cast<long double>(big)) - expected_big) >
             64 * eps * expected_big + 4 * quantum) {
    return false;
  }
  const long double scale =
      std::max({std::abs(fw), std::abs(gw), std::abs(hw)});
  if (scale == 0) {
    return sm == 0 && big == 0;
  }
  const long double fn = fw / scale;
  const long double gn = gw / scale;
  const long double hn = hw / scale;
  const long double m00 = clw * (fn * crw + gn * srw) + slw * hn * srw;
  const long double m01 = clw * (-fn * srw + gn * crw) + slw * hn * crw;
  const long double m10 = -slw * (fn * crw + gn * srw) + clw * hn * srw;
  const long double m11 = -slw * (-fn * srw + gn * crw) + clw * hn * crw;
  const long double tolerance =
      64 * eps * expected_big / scale + 4 * quantum / scale;
  return std::abs(m01) <= tolerance && std::abs(m10) <= tolerance &&
         std::abs(m00 -
                  std::copysign(expected_big, static_cast<long double>(big)) /
                      scale) <= tolerance &&
         std::abs(m11 - static_cast<long double>(sm) / scale) <= tolerance;
}

template <typename T, typename Function>
void ForEach(Function function) {
  for (T f : Values<T>()) {
    for (T g : Values<T>()) {
      for (T h : Values<T>()) {
        function(f, g, h);
      }
    }
  }
  for (T infinity : {std::numeric_limits<T>::infinity(),
                     -std::numeric_limits<T>::infinity()}) {
    for (T a : Values<T>()) {
      for (T b : Values<T>()) {
        function(infinity, a, b);
        function(a, infinity, b);
        function(a, b, infinity);
      }
    }
  }
}
}  // namespace asc_scalar_svd_numerical
#endif  // ASC_TESTS_DENSE_LAPACK_INSTALLED_LU_SCALAR_SVD_NUMERICAL_H_
