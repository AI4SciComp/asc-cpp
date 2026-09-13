#ifndef ASC_DENSE_LAPACK_INTERNAL_ROBUST_PPSVX_KERNEL_H_
#define ASC_DENSE_LAPACK_INTERNAL_ROBUST_PPSVX_KERNEL_H_

#include <algorithm>
#include <complex>
#include <limits>

#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "internal_robust_ppsvx_number.h"

namespace asc::internal_robust_ppsvx {

// All arrays are disjoint slices of the caller's explicit scratch. Both
// triangles are represented internally as the lower packed factor L.
template <typename T>
struct Engine {
  using Real = DenseBlasRealType<T>;
  using Scalar = Binary<Real>;
  using Value = Number<Real>;

  extent_t n;
  extent_t nrhs;
  Value* a;
  Value* af;
  Value* b;
  Value* x;
  Value* s;
  Value* temporary;
  Value* residual;
  Value* weight;
  Value* rows;
  Value* ferr;
  Value* berr;
  Scalar rcond;
  bool valid = true;
  bool scaled = false;
  bool accurate = true;
  Scalar unfloored_backward;

  Engine(extent_t order, extent_t right_sides, Value* scratch)
      : n(order),
        nrhs(right_sides),
        a(scratch),
        af(a + n * (n + 1) / 2),
        b(af + n * (n + 1) / 2),
        x(b + n * nrhs),
        s(x + n * nrhs),
        temporary(s + n),
        residual(temporary + n),
        weight(residual + n),
        rows(weight + n),
        ferr(rows + n),
        berr(ferr + nrhs) {}

  [[nodiscard]] extent_t Offset(extent_t i, extent_t j) const {
    return j * n - j * (j - 1) / 2 + i - j;
  }

  [[nodiscard]] Value Coefficient(extent_t i, extent_t j) const {
    return i >= j ? a[Offset(i, j)] : Conjugate(a[Offset(j, i)]);
  }

  Real Narrow(Scalar value) {
    const Real result = value.Narrow(valid);
    // Publication may round subnormals, but cannot silently discard a
    // coefficient or introduce relative loss beyond the stated roundoff model.
    if (Abs(value - Scalar(result)) >
        Abs(value) * Scalar(8 * std::numeric_limits<Real>::epsilon())) {
      valid = false;
    }
    return result;
  }

  T Narrow(Value value) {
    const Real real = Narrow(value.real);
    const Real imaginary = Narrow(value.imaginary);
    if constexpr (DenseBlasComplex<T>) {
      return {real, imaginary};
    } else {
      valid = valid && imaginary == 0;
      return real;
    }
  }

  Value Round(Value value) { return Value(Narrow(value)); }

  bool Equilibrate() {
    Scalar smallest;
    Scalar largest;
    Scalar maximum;
    for (extent_t i = 0; i < n; ++i) {
      const auto diagonal = a[Offset(i, i)].real;
      if (!diagonal.finite() || diagonal.fraction <= 0) {
        return false;
      }
      s[i] = Round(Value(Scalar(1) / Sqrt(diagonal)));
      largest = Max(largest, s[i].real);
      smallest = i == 0 || s[i].real < smallest ? s[i].real : smallest;
      maximum = Max(maximum, diagonal);
    }
    const Scalar small = Scalar(std::numeric_limits<Real>::min()) /
                         Scalar(std::numeric_limits<Real>::epsilon());
    scaled = smallest / largest < Scalar(Real{0.1}) || maximum < small ||
             Scalar(1) / small < maximum;
    if (scaled) {
      for (extent_t j = 0; j < n; ++j) {
        for (extent_t i = j; i < n; ++i) {
          a[Offset(i, j)] = Round(a[Offset(i, j)] * s[i].real * s[j].real);
        }
      }
    }
    return valid;
  }

  // Return the failing pivot, or n on successful packed Cholesky.
  extent_t Factor() {
    for (extent_t j = 0; j < n; ++j) {
      auto diagonal = a[Offset(j, j)].real;
      for (extent_t k = 0; k < j; ++k) {
        const auto entry = af[Offset(j, k)];
        diagonal = diagonal - entry.real * entry.real -
                   entry.imaginary * entry.imaginary;
      }
      if (!diagonal.finite() || diagonal.fraction <= 0) {
        return j;
      }
      af[Offset(j, j)] = Round(Value(Sqrt(diagonal)));
      for (extent_t i = j + 1; i < n; ++i) {
        auto entry = a[Offset(i, j)];
        for (extent_t k = 0; k < j; ++k) {
          entry = entry - af[Offset(i, k)] * Conjugate(af[Offset(j, k)]);
        }
        af[Offset(i, j)] = Round(entry / af[Offset(j, j)]);
      }
    }
    return n;
  }

  void Solve(Value* vector) {
    for (extent_t i = 0; i < n; ++i) {
      for (extent_t j = 0; j < i; ++j) {
        vector[i] = vector[i] - af[Offset(i, j)] * vector[j];
      }
      vector[i] = vector[i] / af[Offset(i, i)];
    }
    for (extent_t i = n; i-- > 0;) {
      for (extent_t j = i + 1; j < n; ++j) {
        vector[i] = vector[i] - Conjugate(af[Offset(j, i)]) * vector[j];
      }
      vector[i] = vector[i] / Conjugate(af[Offset(i, i)]);
      valid = valid && vector[i].finite();
    }
  }

  void Condition() {
    Scalar norm;
    std::fill_n(rows, n, Value{});
    for (extent_t i = 0; i < n; ++i) {
      Scalar sum;
      for (extent_t j = 0; j < n; ++j) {
        sum = sum + Magnitude(Coefficient(i, j));
      }
      norm = Max(norm, sum);
    }
    for (extent_t j = 0; j < n; ++j) {
      std::fill_n(temporary, n, Value{});
      temporary[j] = Value(Real{1});
      Solve(temporary);
      for (extent_t i = 0; i < n; ++i) {
        rows[i].real = rows[i].real + Magnitude(temporary[i]);
      }
    }
    Scalar inverse_norm;
    for (extent_t i = 0; i < n; ++i) {
      inverse_norm = Max(inverse_norm, rows[i].real);
    }
    rcond = Scalar(1) / (norm * inverse_norm);
    valid = valid && rcond.finite();
  }

  [[nodiscard]] Scalar Gamma() const {
    const Scalar error = Scalar(static_cast<Real>(n + 1)) *
                         Scalar(8 * std::numeric_limits<Real>::epsilon());
    return error / (Scalar(1) - error);
  }

  Scalar Residual(extent_t right_side, const Value* solution) {
    Scalar backward;
    unfloored_backward = Scalar{};
    const Scalar safe = Scalar(static_cast<Real>(n + 1)) *
                        Scalar(std::numeric_limits<Real>::min());
    const Scalar threshold =
        safe / Scalar(std::numeric_limits<Real>::epsilon());
    for (extent_t i = 0; i < n; ++i) {
      const auto rhs = b[right_side * n + i];
      auto difference = rhs;
      auto denominator = Abs1(rhs);
      for (extent_t j = 0; j < n; ++j) {
        const auto coefficient = Coefficient(i, j);
        difference = difference - coefficient * solution[j];
        denominator = denominator + Abs1(coefficient) * Abs1(solution[j]);
      }
      residual[i] = difference;
      const auto absolute = Abs1(difference);
      if (denominator.fraction != 0) {
        unfloored_backward = Max(unfloored_backward, absolute / denominator);
      } else {
        valid = valid && absolute.fraction == 0;
      }
      const Scalar floor = denominator < threshold ? safe : Scalar{};
      backward = Max(backward, (absolute + floor) / (denominator + floor));
      // The expanded exponent arithmetic has no working-type underflow in
      // this weight or its inverse application. An artificial normal_min
      // floor would create an unrepresentable positive error for exact zero
      // right sides at large matrix scales. The rounding-error model gives
      // zero weight for that exact homogeneous equation.
      weight[i] = Value(absolute + Gamma() * denominator);
    }
    return backward;
  }

  void Forward(extent_t right_side) {
    std::fill_n(rows, n, Value{});
    for (extent_t j = 0; j < n; ++j) {
      std::fill_n(temporary, n, Value{});
      temporary[j] = weight[j];
      Solve(temporary);
      for (extent_t i = 0; i < n; ++i) {
        rows[i].real = rows[i].real + Abs1(temporary[i]);
      }
    }
    Scalar error;
    Scalar norm;
    for (extent_t i = 0; i < n; ++i) {
      const Scalar scale = scaled ? s[i].real : Scalar(1);
      error = Max(error, rows[i].real * scale);
      norm = Max(norm, Abs1(x[right_side * n + i]));
    }
    if (norm.fraction == 0) {
      // Zero solution norm uses an absolute estimate, as documented.
      ferr[right_side] = Value(error);
    } else {
      ferr[right_side] = Value(error / norm);
    }
    valid = valid && ferr[right_side].finite();
  }

  void Solutions() {
    valid = valid && Gamma().finite() && Gamma().fraction > 0;
    for (extent_t j = 0; j < nrhs; ++j) {
      auto* solution = x + j * n;
      std::copy_n(b + j * n, n, solution);
      Solve(solution);
      for (int iteration = 0; iteration < 3; ++iteration) {
        static_cast<void>(Residual(j, solution));
        if (unfloored_backward < Scalar(std::numeric_limits<Real>::epsilon())) {
          break;
        }
        Solve(residual);
        for (extent_t i = 0; i < n; ++i) {
          solution[i] = solution[i] + residual[i];
        }
      }
      for (extent_t i = 0; i < n; ++i) {
        const Scalar scale = scaled ? s[i].real : Scalar(1);
        solution[i] = Round(solution[i] * scale);
        temporary[i] = solution[i] / Value(scale);
      }
      berr[j] = Value(Residual(j, temporary));
      accurate = accurate && !(Gamma() < unfloored_backward);
      Forward(j);
    }
  }
};

}  // namespace asc::internal_robust_ppsvx

#endif  // ASC_DENSE_LAPACK_INTERNAL_ROBUST_PPSVX_KERNEL_H_
