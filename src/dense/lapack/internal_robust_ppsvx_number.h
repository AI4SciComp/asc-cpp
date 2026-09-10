#ifndef ASC_DENSE_LAPACK_INTERNAL_ROBUST_PPSVX_NUMBER_H_
#define ASC_DENSE_LAPACK_INTERNAL_ROBUST_PPSVX_NUMBER_H_

#include <cmath>
#include <complex>
#include <cstdint>
#include <limits>

namespace asc::internal_robust_ppsvx {

// Original working-precision arithmetic with an explicit exponent. This is
// neither extended significand precision nor a change to the FP environment.
template <typename Real>
struct Binary {
  Real fraction = 0;
  std::int64_t exponent = 0;

  Binary() = default;
  explicit Binary(Real value, std::int64_t power = 0) {
    constexpr auto kLimit = std::numeric_limits<std::int64_t>::max() / 16;
    if (!std::isfinite(value) || power < -kLimit || power > kLimit) {
      fraction = std::numeric_limits<Real>::quiet_NaN();
      return;
    }
    if (value != 0) {
      int adjustment = 0;
      fraction = std::frexp(value, &adjustment);
      exponent = power + adjustment;
      if (exponent < -kLimit || exponent > kLimit) {
        fraction = std::numeric_limits<Real>::quiet_NaN();
        exponent = 0;
      }
    }
  }

  [[nodiscard]] bool finite() const { return std::isfinite(fraction); }

  [[nodiscard]] Real Narrow(bool& valid) const {
    if (!finite() || exponent < std::numeric_limits<int>::min() ||
        exponent > std::numeric_limits<int>::max()) {
      valid = false;
      return 0;
    }
    const Real result = std::scalbn(fraction, static_cast<int>(exponent));
    if (!std::isfinite(result) || (result == 0 && fraction != 0)) {
      valid = false;
      return 0;
    }
    return result;
  }
};

template <typename Real>
Binary<Real> operator-(Binary<Real> value) {
  value.fraction = -value.fraction;
  return value;
}

template <typename Real>
Binary<Real> operator+(Binary<Real> a, Binary<Real> b) {
  if (!a.finite() || !b.finite()) {
    return Binary<Real>(std::numeric_limits<Real>::quiet_NaN());
  }
  if (a.fraction == 0) {
    return b;
  }
  if (b.fraction == 0) {
    return a;
  }
  if (a.exponent < b.exponent) {
    return b + a;
  }
  const auto difference = a.exponent - b.exponent;
  // A term below one quarter ulp is absorbed by working-precision addition.
  // It remains represented independently in its source operand.
  if (difference > std::numeric_limits<Real>::digits + 2) {
    return a;
  }
  return Binary<Real>(
      a.fraction + std::scalbn(b.fraction, -static_cast<int>(difference)),
      a.exponent);
}

template <typename Real>
Binary<Real> operator-(Binary<Real> a, Binary<Real> b) {
  return a + -b;
}

template <typename Real>
Binary<Real> operator*(Binary<Real> a, Binary<Real> b) {
  return Binary<Real>(a.fraction * b.fraction, a.exponent + b.exponent);
}

template <typename Real>
Binary<Real> operator/(Binary<Real> a, Binary<Real> b) {
  if (b.fraction == 0) {
    return Binary<Real>(std::numeric_limits<Real>::quiet_NaN());
  }
  return Binary<Real>(a.fraction / b.fraction, a.exponent - b.exponent);
}

template <typename Real>
Binary<Real> Abs(Binary<Real> value) {
  value.fraction = std::abs(value.fraction);
  return value;
}

template <typename Real>
bool operator<(Binary<Real> a, Binary<Real> b) {
  return (a - b).fraction < 0;
}

template <typename Real>
bool operator>(Binary<Real> a, Binary<Real> b) {
  return b < a;
}

template <typename Real>
Binary<Real> Max(Binary<Real> a, Binary<Real> b) {
  return a < b ? b : a;
}

template <typename Real>
Binary<Real> Sqrt(Binary<Real> value) {
  if (value.exponent % 2 != 0) {
    value.fraction *= 2;
    --value.exponent;
  }
  return Binary<Real>(std::sqrt(value.fraction), value.exponent / 2);
}

template <typename Real>
struct Number {
  Binary<Real> real;
  Binary<Real> imaginary;

  Number() = default;
  explicit Number(Real value) : real(value) {}
  explicit Number(Binary<Real> value) : real(value) {}
  explicit Number(std::complex<Real> value)
      : real(value.real()), imaginary(value.imag()) {}
  Number(Binary<Real> a, Binary<Real> b) : real(a), imaginary(b) {}

  [[nodiscard]] bool finite() const {
    return real.finite() && imaginary.finite();
  }
};

template <typename Real>
Number<Real> operator+(Number<Real> a, Number<Real> b) {
  return {a.real + b.real, a.imaginary + b.imaginary};
}

template <typename Real>
Number<Real> operator-(Number<Real> a, Number<Real> b) {
  return {a.real - b.real, a.imaginary - b.imaginary};
}

template <typename Real>
Number<Real> operator*(Number<Real> a, Number<Real> b) {
  return {a.real * b.real - a.imaginary * b.imaginary,
          a.real * b.imaginary + a.imaginary * b.real};
}

template <typename Real>
Number<Real> operator*(Number<Real> a, Binary<Real> b) {
  return {a.real * b, a.imaginary * b};
}

template <typename Real>
Number<Real> Conjugate(Number<Real> value) {
  return {value.real, -value.imaginary};
}

template <typename Real>
Binary<Real> Abs1(Number<Real> value) {
  return Abs(value.real) + Abs(value.imaginary);
}

template <typename Real>
Binary<Real> Magnitude(Number<Real> value) {
  return Sqrt(value.real * value.real + value.imaginary * value.imaginary);
}

template <typename Real>
Number<Real> operator/(Number<Real> a, Number<Real> b) {
  const auto denominator = b.real * b.real + b.imaginary * b.imaginary;
  const auto numerator = a * Conjugate(b);
  return {numerator.real / denominator, numerator.imaginary / denominator};
}

}  // namespace asc::internal_robust_ppsvx

#endif  // ASC_DENSE_LAPACK_INTERNAL_ROBUST_PPSVX_NUMBER_H_
