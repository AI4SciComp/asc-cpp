#include <algorithm>
#include <cmath>
#include <complex>
#include <concepts>
#include <cstdint>
#include <type_traits>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"

namespace asc {
namespace internal_dense_blas_level1 {

template <typename Element>
using Vector = DenseBlasVectorView<Element>;

Status ValidateContext(const ExecutionContext& context) {
  if (context.backend() != Backend::kSerial) {
    return Status(ErrorCode::kUnsupported,
                  "Dense CPU BLAS requires serial execution");
  }
  if (!context.CanAccess(MemorySpace::kHost)) {
    return Status(ErrorCode::kMemoryAccess,
                  "Serial execution cannot access host memory");
  }
  return Status::Ok();
}

template <typename Element>
Status ValidateHost(Vector<Element> vector) {
  if (vector.memory_space() != MemorySpace::kHost) {
    return Status(ErrorCode::kMemoryAccess,
                  "Dense CPU BLAS requires host storage");
  }
  if (vector.size() != 0 && vector.data() == nullptr) {
    return Status(ErrorCode::kMemoryAccess,
                  "A nonempty BLAS vector has a null pointer");
  }
  return Status::Ok();
}

template <typename Element>
Status ValidateExactSize(Vector<Element> vector, extent_t expected,
                         const char* message) {
  Status status = ValidateHost(vector);
  if (!status.ok()) {
    return status;
  }
  if (vector.size() != expected) {
    return Status(ErrorCode::kShape, message);
  }
  return Status::Ok();
}

template <typename LeftElement, typename RightElement>
bool SameDescriptor(Vector<LeftElement> left, Vector<RightElement> right) {
  return static_cast<const void*>(left.data()) ==
             static_cast<const void*>(right.data()) &&
         left.size() == right.size() && left.increment() == right.increment() &&
         left.memory_space() == right.memory_space();
}

template <typename LeftElement, typename RightElement>
bool Overlap(Vector<LeftElement> left, Vector<RightElement> right) {
  const ConstMemoryView left_span = left.reachable_storage();
  const ConstMemoryView right_span = right.reachable_storage();
  if (left_span.size() == 0 || right_span.size() == 0) {
    return false;
  }
  const auto left_begin = reinterpret_cast<std::uintptr_t>(left_span.data());
  const auto right_begin = reinterpret_cast<std::uintptr_t>(right_span.data());
  const std::uintptr_t left_end = left_begin + left_span.size();
  const std::uintptr_t right_end = right_begin + right_span.size();
  return left_begin < right_end && right_begin < left_end;
}

template <typename Element>
decltype(auto) At(Vector<Element> vector, index_t index) {
  return vector.data()[index * vector.increment()];
}

template <typename LeftElement, typename RightElement>
Status ValidatePair(Vector<LeftElement> left, Vector<RightElement> right) {
  Status left_status = ValidateHost(left);
  if (!left_status.ok()) {
    return left_status;
  }
  Status right_status = ValidateHost(right);
  if (!right_status.ok()) {
    return right_status;
  }
  if (left.size() != right.size()) {
    return Status(ErrorCode::kShape, "BLAS vector sizes do not match");
  }
  return Status::Ok();
}

template <typename Element>
Status RealRotg(const ExecutionContext& context, Vector<Element> a,
                Vector<Element> b, Vector<Element> c, Vector<Element> s) {
  Status context_status = ValidateContext(context);
  if (!context_status.ok()) {
    return context_status;
  }
  for (const auto vector : {a, b, c, s}) {
    Status status =
        ValidateExactSize(vector, 1, "Rotg operands must have size one");
    if (!status.ok()) {
      return status;
    }
  }
  if (Overlap(a, b) || Overlap(a, c) || Overlap(a, s) || Overlap(b, c) ||
      Overlap(b, s) || Overlap(c, s)) {
    return Status(ErrorCode::kInvalidArgument,
                  "Rotg rejects overlapping scalar operands");
  }

  const Element input_a = At(a, 0);
  const Element input_b = At(b, 0);
  if (input_a == Element{0} && input_b == Element{0}) {
    At(a, 0) = Element{0};
    At(b, 0) = Element{0};
    At(c, 0) = Element{1};
    At(s, 0) = Element{0};
    return Status::Ok();
  }

  const Element roe = std::abs(input_a) > std::abs(input_b) ? input_a : input_b;
  const Element magnitude = std::hypot(input_a, input_b);
  const Element r = std::copysign(magnitude, roe);
  const Element cosine = input_a / r;
  const Element sine = input_b / r;
  Element z = Element{1};
  if (std::abs(input_a) > std::abs(input_b)) {
    z = sine;
  } else if (cosine != Element{0}) {
    z = Element{1} / cosine;
  }
  At(a, 0) = r;
  At(b, 0) = z;
  At(c, 0) = cosine;
  At(s, 0) = sine;
  return Status::Ok();
}

template <typename Real>
Status ComplexRotg(const ExecutionContext& context,
                   Vector<std::complex<Real>> a,
                   Vector<const std::complex<Real>> b, Vector<Real> c,
                   Vector<std::complex<Real>> s) {
  Status context_status = ValidateContext(context);
  if (!context_status.ok()) {
    return context_status;
  }
  for (const Status& status :
       {ValidateExactSize(a, 1, "Rotg operands must have size one"),
        ValidateExactSize(b, 1, "Rotg operands must have size one"),
        ValidateExactSize(c, 1, "Rotg operands must have size one"),
        ValidateExactSize(s, 1, "Rotg operands must have size one")}) {
    if (!status.ok()) {
      return status;
    }
  }
  if (Overlap(a, b) || Overlap(a, c) || Overlap(a, s) || Overlap(b, c) ||
      Overlap(b, s) || Overlap(c, s)) {
    return Status(ErrorCode::kInvalidArgument,
                  "Rotg rejects overlapping scalar operands");
  }

  const std::complex<Real> input_a = At(a, 0);
  const std::complex<Real> input_b = At(b, 0);
  const Real absolute_a = std::abs(input_a);
  if (absolute_a == Real{0}) {
    At(a, 0) = input_b;
    At(c, 0) = Real{0};
    At(s, 0) = std::complex<Real>{1, 0};
    return Status::Ok();
  }
  const Real norm = std::hypot(absolute_a, std::abs(input_b));
  const std::complex<Real> alpha = input_a / absolute_a;
  At(a, 0) = alpha * norm;
  At(c, 0) = absolute_a / norm;
  At(s, 0) = alpha * std::conj(input_b) / norm;
  return Status::Ok();
}

template <typename Element>
Status RotmgImpl(  // NOLINT(readability-function-size)
    const ExecutionContext& context, Vector<Element> d1, Vector<Element> d2,
    Vector<Element> x1, Vector<const Element> y1, Vector<Element> parameters) {
  Status context_status = ValidateContext(context);
  if (!context_status.ok()) {
    return context_status;
  }
  for (const Status& status :
       {ValidateExactSize(d1, 1, "Rotmg scalar operands must have size one"),
        ValidateExactSize(d2, 1, "Rotmg scalar operands must have size one"),
        ValidateExactSize(x1, 1, "Rotmg scalar operands must have size one"),
        ValidateExactSize(y1, 1, "Rotmg scalar operands must have size one"),
        ValidateExactSize(parameters, 5,
                          "Rotmg parameters must have size five")}) {
    if (!status.ok()) {
      return status;
    }
  }
  if (parameters.increment() != 1) {
    return Status(ErrorCode::kInvalidArgument,
                  "Rotmg parameters must be contiguous");
  }
  if (Overlap(d1, d2) || Overlap(d1, x1) || Overlap(d1, y1) ||
      Overlap(d1, parameters) || Overlap(d2, x1) || Overlap(d2, y1) ||
      Overlap(d2, parameters) || Overlap(x1, y1) || Overlap(x1, parameters) ||
      Overlap(y1, parameters)) {
    return Status(ErrorCode::kInvalidArgument,
                  "Rotmg rejects overlapping operands");
  }

  constexpr Element kGamma = Element{4096};
  constexpr Element kGammaSquared = kGamma * kGamma;
  constexpr Element kInverseGammaSquared = Element{1} / kGammaSquared;
  Element value_d1 = At(d1, 0);
  Element value_d2 = At(d2, 0);
  Element value_x1 = At(x1, 0);
  const Element value_y1 = At(y1, 0);
  Element flag = Element{-1};
  Element h11 = Element{0};
  Element h12 = Element{0};
  Element h21 = Element{0};
  Element h22 = Element{0};

  if (value_d1 < Element{0}) {
    value_d1 = Element{0};
    value_d2 = Element{0};
    value_x1 = Element{0};
  } else {
    const Element p2 = value_d2 * value_y1;
    if (p2 == Element{0}) {
      At(parameters, 0) = Element{-2};
      return Status::Ok();
    }
    const Element p1 = value_d1 * value_x1;
    const Element q2 = p2 * value_y1;
    const Element q1 = p1 * value_x1;
    if (std::abs(q1) > std::abs(q2)) {
      h21 = -value_y1 / value_x1;
      h12 = p2 / p1;
      const Element u = Element{1} - h12 * h21;
      if (u > Element{0}) {
        flag = Element{0};
        value_d1 /= u;
        value_d2 /= u;
        value_x1 *= u;
      } else {
        value_d1 = Element{0};
        value_d2 = Element{0};
        value_x1 = Element{0};
      }
    } else if (q2 < Element{0}) {
      value_d1 = Element{0};
      value_d2 = Element{0};
      value_x1 = Element{0};
    } else {
      flag = Element{1};
      h11 = p1 / p2;
      h22 = value_x1 / value_y1;
      const Element u = Element{1} + h11 * h22;
      const Element temporary = value_d2 / u;
      value_d2 = value_d1 / u;
      value_d1 = temporary;
      value_x1 = value_y1 * u;
    }

    if (value_d1 != Element{0}) {
      while (std::isfinite(value_d1) &&
             (value_d1 <= kInverseGammaSquared || value_d1 >= kGammaSquared)) {
        if (flag == Element{0}) {
          h11 = Element{1};
          h22 = Element{1};
        } else {
          h21 = Element{-1};
          h12 = Element{1};
        }
        flag = Element{-1};
        if (value_d1 <= kInverseGammaSquared) {
          value_d1 *= kGammaSquared;
          value_x1 /= kGamma;
          h11 /= kGamma;
          h12 /= kGamma;
        } else {
          value_d1 /= kGammaSquared;
          value_x1 *= kGamma;
          h11 *= kGamma;
          h12 *= kGamma;
        }
      }
    }
    if (value_d2 != Element{0}) {
      while (std::isfinite(value_d2) &&
             (std::abs(value_d2) <= kInverseGammaSquared ||
              std::abs(value_d2) >= kGammaSquared)) {
        if (flag == Element{0}) {
          h11 = Element{1};
          h22 = Element{1};
        } else {
          h21 = Element{-1};
          h12 = Element{1};
        }
        flag = Element{-1};
        if (std::abs(value_d2) <= kInverseGammaSquared) {
          value_d2 *= kGammaSquared;
          h21 /= kGamma;
          h22 /= kGamma;
        } else {
          value_d2 /= kGammaSquared;
          h21 *= kGamma;
          h22 *= kGamma;
        }
      }
    }
  }

  At(d1, 0) = value_d1;
  At(d2, 0) = value_d2;
  At(x1, 0) = value_x1;
  At(parameters, 0) = flag;
  if (flag < Element{0}) {
    At(parameters, 1) = h11;
    At(parameters, 2) = h21;
    At(parameters, 3) = h12;
    At(parameters, 4) = h22;
  } else if (flag == Element{0}) {
    At(parameters, 2) = h21;
    At(parameters, 3) = h12;
  } else {
    At(parameters, 1) = h11;
    At(parameters, 4) = h22;
  }
  return Status::Ok();
}

template <typename Element, typename Real>
Status RotImpl(const ExecutionContext& context, Vector<Element> x,
               Vector<Element> y, Real c, Real s) {
  Status context_status = ValidateContext(context);
  if (!context_status.ok()) {
    return context_status;
  }
  Status pair_status = ValidatePair(x, y);
  if (!pair_status.ok()) {
    return pair_status;
  }
  if (Overlap(x, y)) {
    return Status(ErrorCode::kInvalidArgument,
                  "Rot rejects overlapping vectors");
  }
  for (index_t index = 0; index < x.size(); ++index) {
    const Element old_x = At(x, index);
    const Element old_y = At(y, index);
    At(x, index) = c * old_x + s * old_y;
    At(y, index) = c * old_y - s * old_x;
  }
  return Status::Ok();
}

template <typename Element>
Status RotmImpl(const ExecutionContext& context, Vector<Element> x,
                Vector<Element> y, Vector<const Element> parameters) {
  Status context_status = ValidateContext(context);
  if (!context_status.ok()) {
    return context_status;
  }
  Status pair_status = ValidatePair(x, y);
  if (!pair_status.ok()) {
    return pair_status;
  }
  Status parameter_status =
      ValidateExactSize(parameters, 5, "Rotm parameters must have size five");
  if (!parameter_status.ok()) {
    return parameter_status;
  }
  if (parameters.increment() != 1) {
    return Status(ErrorCode::kInvalidArgument,
                  "Rotm parameters must be contiguous");
  }
  if (Overlap(x, y) || Overlap(x, parameters) || Overlap(y, parameters)) {
    return Status(ErrorCode::kInvalidArgument,
                  "Rotm rejects overlapping operands");
  }

  const Element flag = At(parameters, 0);
  if (flag == Element{-2}) {
    return Status::Ok();
  }
  for (index_t index = 0; index < x.size(); ++index) {
    const Element old_x = At(x, index);
    const Element old_y = At(y, index);
    if (flag < Element{0}) {
      At(x, index) = old_x * At(parameters, 1) + old_y * At(parameters, 3);
      At(y, index) = old_x * At(parameters, 2) + old_y * At(parameters, 4);
    } else if (flag == Element{0}) {
      At(x, index) = old_x + old_y * At(parameters, 3);
      At(y, index) = old_x * At(parameters, 2) + old_y;
    } else {
      At(x, index) = old_x * At(parameters, 1) + old_y;
      At(y, index) = -old_x + old_y * At(parameters, 4);
    }
  }
  return Status::Ok();
}

template <typename Element>
Status SwapImpl(const ExecutionContext& context, Vector<Element> x,
                Vector<Element> y) {
  Status context_status = ValidateContext(context);
  if (!context_status.ok()) {
    return context_status;
  }
  Status pair_status = ValidatePair(x, y);
  if (!pair_status.ok()) {
    return pair_status;
  }
  if (SameDescriptor(x, y)) {
    return Status::Ok();
  }
  if (Overlap(x, y)) {
    return Status(ErrorCode::kInvalidArgument,
                  "Swap rejects partially overlapping vectors");
  }
  for (index_t index = 0; index < x.size(); ++index) {
    std::swap(At(x, index), At(y, index));
  }
  return Status::Ok();
}

template <typename Alpha, typename Element>
Status ScalImpl(const ExecutionContext& context, Alpha alpha,
                Vector<Element> destination) {
  Status context_status = ValidateContext(context);
  if (!context_status.ok()) {
    return context_status;
  }
  Status status = ValidateHost(destination);
  if (!status.ok()) {
    return status;
  }
  for (index_t index = 0; index < destination.size(); ++index) {
    At(destination, index) *= alpha;
  }
  return Status::Ok();
}

template <typename Element>
Status CopyImpl(const ExecutionContext& context, Vector<const Element> source,
                Vector<Element> destination) {
  Status context_status = ValidateContext(context);
  if (!context_status.ok()) {
    return context_status;
  }
  Status pair_status = ValidatePair(source, destination);
  if (!pair_status.ok()) {
    return pair_status;
  }
  if (SameDescriptor(source, destination)) {
    return Status::Ok();
  }
  if (Overlap(source, destination)) {
    return Status(ErrorCode::kInvalidArgument,
                  "Copy rejects partially overlapping vectors");
  }
  for (index_t index = 0; index < source.size(); ++index) {
    At(destination, index) = At(source, index);
  }
  return Status::Ok();
}

template <typename Element>
Status AxpyImpl(const ExecutionContext& context, Element alpha,
                Vector<const Element> source, Vector<Element> destination) {
  Status context_status = ValidateContext(context);
  if (!context_status.ok()) {
    return context_status;
  }
  Status pair_status = ValidatePair(source, destination);
  if (!pair_status.ok()) {
    return pair_status;
  }
  if (Overlap(source, destination) && !SameDescriptor(source, destination)) {
    return Status(ErrorCode::kInvalidArgument,
                  "Axpy rejects partially overlapping vectors");
  }
  if (alpha == Element{0}) {
    return Status::Ok();
  }
  for (index_t index = 0; index < source.size(); ++index) {
    At(destination, index) += alpha * At(source, index);
  }
  return Status::Ok();
}

template <typename Input, typename Output>
Status ValidateReduction(const ExecutionContext& context,
                         Vector<const Input> left, Vector<const Input> right,
                         Vector<Output> result) {
  Status context_status = ValidateContext(context);
  if (!context_status.ok()) {
    return context_status;
  }
  Status pair_status = ValidatePair(left, right);
  if (!pair_status.ok()) {
    return pair_status;
  }
  Status result_status = ValidateExactSize(
      result, 1, "A BLAS reduction result must have size one");
  if (!result_status.ok()) {
    return result_status;
  }
  if (Overlap(left, result) || Overlap(right, result)) {
    return Status(ErrorCode::kInvalidArgument,
                  "A BLAS reduction result cannot overlap an input");
  }
  return Status::Ok();
}

template <typename Input, typename Output>
Status ValidateReduction(const ExecutionContext& context,
                         Vector<const Input> operand, Vector<Output> result) {
  Status context_status = ValidateContext(context);
  if (!context_status.ok()) {
    return context_status;
  }
  Status operand_status = ValidateHost(operand);
  if (!operand_status.ok()) {
    return operand_status;
  }
  Status result_status = ValidateExactSize(
      result, 1, "A BLAS reduction result must have size one");
  if (!result_status.ok()) {
    return result_status;
  }
  if (Overlap(operand, result)) {
    return Status(ErrorCode::kInvalidArgument,
                  "A BLAS reduction result cannot overlap its input");
  }
  return Status::Ok();
}

template <typename Input, typename Output>
Status DotImpl(const ExecutionContext& context, Vector<const Input> left,
               Vector<const Input> right, Vector<Output> result,
               Output initial = Output{0}) {
  Status status = ValidateReduction(context, left, right, result);
  if (!status.ok()) {
    return status;
  }
  Output sum = initial;
  for (index_t index = 0; index < left.size(); ++index) {
    sum += static_cast<Output>(At(left, index)) *
           static_cast<Output>(At(right, index));
  }
  At(result, 0) = sum;
  return Status::Ok();
}

Status SdsdotImpl(const ExecutionContext& context, float bias,
                  Vector<const float> left, Vector<const float> right,
                  Vector<float> result) {
  Status status = ValidateReduction(context, left, right, result);
  if (!status.ok()) {
    return status;
  }
  double sum = static_cast<double>(bias);
  for (index_t index = 0; index < left.size(); ++index) {
    sum += static_cast<double>(At(left, index)) *
           static_cast<double>(At(right, index));
  }
  At(result, 0) = static_cast<float>(sum);
  return Status::Ok();
}

template <typename Element, bool Conjugate>
Status ComplexDotImpl(const ExecutionContext& context,
                      Vector<const Element> left, Vector<const Element> right,
                      Vector<Element> result) {
  Status status = ValidateReduction(context, left, right, result);
  if (!status.ok()) {
    return status;
  }
  Element sum{0, 0};
  for (index_t index = 0; index < left.size(); ++index) {
    if constexpr (Conjugate) {
      sum += std::conj(At(left, index)) * At(right, index);
    } else {
      sum += At(left, index) * At(right, index);
    }
  }
  At(result, 0) = sum;
  return Status::Ok();
}

template <typename Real>
void AddScaledSquare(Real value, Real& scale, Real& sum_squares) {
  const Real absolute = std::abs(value);
  if (std::isnan(absolute)) {
    scale = absolute;
    sum_squares = absolute;
    return;
  }
  if (std::isinf(absolute)) {
    scale = absolute;
    sum_squares = Real{1};
    return;
  }
  if (std::isinf(scale) || std::isnan(scale)) {
    return;
  }
  if (absolute == Real{0}) {
    return;
  }
  if (scale < absolute) {
    const Real ratio = scale / absolute;
    sum_squares = Real{1} + sum_squares * ratio * ratio;
    scale = absolute;
  } else {
    const Real ratio = absolute / scale;
    sum_squares += ratio * ratio;
  }
}

template <typename Element, typename Real>
Status Nrm2Impl(const ExecutionContext& context, Vector<const Element> operand,
                Vector<Real> result) {
  Status status = ValidateReduction(context, operand, result);
  if (!status.ok()) {
    return status;
  }
  Real scale = Real{0};
  Real sum_squares = Real{1};
  for (index_t index = 0; index < operand.size(); ++index) {
    if constexpr (std::same_as<Element, Real>) {
      AddScaledSquare(At(operand, index), scale, sum_squares);
    } else {
      AddScaledSquare(At(operand, index).real(), scale, sum_squares);
      AddScaledSquare(At(operand, index).imag(), scale, sum_squares);
    }
  }
  At(result, 0) = scale == Real{0} ? Real{0} : scale * std::sqrt(sum_squares);
  return Status::Ok();
}

template <typename Element, typename Real>
Status AsumImpl(const ExecutionContext& context, Vector<const Element> operand,
                Vector<Real> result) {
  Status status = ValidateReduction(context, operand, result);
  if (!status.ok()) {
    return status;
  }
  Real sum = Real{0};
  for (index_t index = 0; index < operand.size(); ++index) {
    if constexpr (std::same_as<Element, Real>) {
      sum += std::abs(At(operand, index));
    } else {
      sum += std::abs(At(operand, index).real()) +
             std::abs(At(operand, index).imag());
    }
  }
  At(result, 0) = sum;
  return Status::Ok();
}

template <typename Element>
auto BlasMagnitude(const Element& value) {
  if constexpr (std::is_arithmetic_v<Element>) {
    return std::abs(value);
  } else {
    return std::abs(value.real()) + std::abs(value.imag());
  }
}

template <typename Element>
Status IamaxImpl(const ExecutionContext& context, Vector<const Element> operand,
                 Vector<index_t> result) {
  Status status = ValidateReduction(context, operand, result);
  if (!status.ok()) {
    return status;
  }
  if (operand.size() == 0) {
    At(result, 0) = index_t{-1};
    return Status::Ok();
  }
  index_t maximum_index = 0;
  auto maximum = BlasMagnitude(At(operand, 0));
  for (index_t index = 1; index < operand.size(); ++index) {
    const auto magnitude = BlasMagnitude(At(operand, index));
    if (magnitude > maximum) {
      maximum = magnitude;
      maximum_index = index;
    }
  }
  At(result, 0) = maximum_index;
  return Status::Ok();
}

}  // namespace internal_dense_blas_level1

using internal_dense_blas_level1::Vector;

Status Rotg(const ExecutionContext& context, Vector<float> a, Vector<float> b,
            Vector<float> c, Vector<float> s) {
  return internal_dense_blas_level1::RealRotg(context, a, b, c, s);
}

Status Rotg(const ExecutionContext& context, Vector<double> a, Vector<double> b,
            Vector<double> c, Vector<double> s) {
  return internal_dense_blas_level1::RealRotg(context, a, b, c, s);
}

Status Rotg(const ExecutionContext& context, Vector<std::complex<float>> a,
            Vector<const std::complex<float>> b, Vector<float> c,
            Vector<std::complex<float>> s) {
  return internal_dense_blas_level1::ComplexRotg(context, a, b, c, s);
}

Status Rotg(const ExecutionContext& context, Vector<std::complex<double>> a,
            Vector<const std::complex<double>> b, Vector<double> c,
            Vector<std::complex<double>> s) {
  return internal_dense_blas_level1::ComplexRotg(context, a, b, c, s);
}

Status Rotmg(const ExecutionContext& context, Vector<float> d1,
             Vector<float> d2, Vector<float> x1, Vector<const float> y1,
             Vector<float> parameters) {
  return internal_dense_blas_level1::RotmgImpl(context, d1, d2, x1, y1,
                                               parameters);
}

Status Rotmg(const ExecutionContext& context, Vector<double> d1,
             Vector<double> d2, Vector<double> x1, Vector<const double> y1,
             Vector<double> parameters) {
  return internal_dense_blas_level1::RotmgImpl(context, d1, d2, x1, y1,
                                               parameters);
}

Status Rot(const ExecutionContext& context, Vector<float> x, Vector<float> y,
           float c, float s) {
  return internal_dense_blas_level1::RotImpl(context, x, y, c, s);
}

Status Rot(const ExecutionContext& context, Vector<double> x, Vector<double> y,
           double c, double s) {
  return internal_dense_blas_level1::RotImpl(context, x, y, c, s);
}

Status Rot(const ExecutionContext& context, Vector<std::complex<float>> x,
           Vector<std::complex<float>> y, float c, float s) {
  return internal_dense_blas_level1::RotImpl(context, x, y, c, s);
}

Status Rot(const ExecutionContext& context, Vector<std::complex<double>> x,
           Vector<std::complex<double>> y, double c, double s) {
  return internal_dense_blas_level1::RotImpl(context, x, y, c, s);
}

Status Rotm(const ExecutionContext& context, Vector<float> x, Vector<float> y,
            Vector<const float> parameters) {
  return internal_dense_blas_level1::RotmImpl(context, x, y, parameters);
}

Status Rotm(const ExecutionContext& context, Vector<double> x, Vector<double> y,
            Vector<const double> parameters) {
  return internal_dense_blas_level1::RotmImpl(context, x, y, parameters);
}

Status Swap(const ExecutionContext& context, Vector<float> x, Vector<float> y) {
  return internal_dense_blas_level1::SwapImpl(context, x, y);
}

Status Swap(const ExecutionContext& context, Vector<double> x,
            Vector<double> y) {
  return internal_dense_blas_level1::SwapImpl(context, x, y);
}

Status Swap(const ExecutionContext& context, Vector<std::complex<float>> x,
            Vector<std::complex<float>> y) {
  return internal_dense_blas_level1::SwapImpl(context, x, y);
}

Status Swap(const ExecutionContext& context, Vector<std::complex<double>> x,
            Vector<std::complex<double>> y) {
  return internal_dense_blas_level1::SwapImpl(context, x, y);
}

Status Scal(const ExecutionContext& context, float alpha,
            Vector<float> destination) {
  return internal_dense_blas_level1::ScalImpl(context, alpha, destination);
}

Status Scal(const ExecutionContext& context, double alpha,
            Vector<double> destination) {
  return internal_dense_blas_level1::ScalImpl(context, alpha, destination);
}

Status Scal(const ExecutionContext& context, std::complex<float> alpha,
            Vector<std::complex<float>> destination) {
  return internal_dense_blas_level1::ScalImpl(context, alpha, destination);
}

Status Scal(const ExecutionContext& context, std::complex<double> alpha,
            Vector<std::complex<double>> destination) {
  return internal_dense_blas_level1::ScalImpl(context, alpha, destination);
}

Status Scal(const ExecutionContext& context, float alpha,
            Vector<std::complex<float>> destination) {
  return internal_dense_blas_level1::ScalImpl(context, alpha, destination);
}

Status Scal(const ExecutionContext& context, double alpha,
            Vector<std::complex<double>> destination) {
  return internal_dense_blas_level1::ScalImpl(context, alpha, destination);
}

Status Copy(const ExecutionContext& context, Vector<const float> source,
            Vector<float> destination) {
  return internal_dense_blas_level1::CopyImpl(context, source, destination);
}

Status Copy(const ExecutionContext& context, Vector<const double> source,
            Vector<double> destination) {
  return internal_dense_blas_level1::CopyImpl(context, source, destination);
}

Status Copy(const ExecutionContext& context,
            Vector<const std::complex<float>> source,
            Vector<std::complex<float>> destination) {
  return internal_dense_blas_level1::CopyImpl(context, source, destination);
}

Status Copy(const ExecutionContext& context,
            Vector<const std::complex<double>> source,
            Vector<std::complex<double>> destination) {
  return internal_dense_blas_level1::CopyImpl(context, source, destination);
}

Status Axpy(const ExecutionContext& context, float alpha,
            Vector<const float> source, Vector<float> destination) {
  return internal_dense_blas_level1::AxpyImpl(context, alpha, source,
                                              destination);
}

Status Axpy(const ExecutionContext& context, double alpha,
            Vector<const double> source, Vector<double> destination) {
  return internal_dense_blas_level1::AxpyImpl(context, alpha, source,
                                              destination);
}

Status Axpy(const ExecutionContext& context, std::complex<float> alpha,
            Vector<const std::complex<float>> source,
            Vector<std::complex<float>> destination) {
  return internal_dense_blas_level1::AxpyImpl(context, alpha, source,
                                              destination);
}

Status Axpy(const ExecutionContext& context, std::complex<double> alpha,
            Vector<const std::complex<double>> source,
            Vector<std::complex<double>> destination) {
  return internal_dense_blas_level1::AxpyImpl(context, alpha, source,
                                              destination);
}

Status Dot(const ExecutionContext& context, Vector<const float> left,
           Vector<const float> right, Vector<float> result) {
  return internal_dense_blas_level1::DotImpl(context, left, right, result);
}

Status Dot(const ExecutionContext& context, Vector<const double> left,
           Vector<const double> right, Vector<double> result) {
  return internal_dense_blas_level1::DotImpl(context, left, right, result);
}

Status Dot(const ExecutionContext& context, float bias,
           Vector<const float> left, Vector<const float> right,
           Vector<float> result) {
  return internal_dense_blas_level1::SdsdotImpl(context, bias, left, right,
                                                result);
}

Status Dot(const ExecutionContext& context,
           DenseBlasDotAccumulation accumulation, Vector<const float> left,
           Vector<const float> right, Vector<double> result) {
  if (accumulation != DenseBlasDotAccumulation::kDouble) {
    return Status(ErrorCode::kInvalidArgument,
                  "The BLAS dot accumulation mode is not recognized");
  }
  return internal_dense_blas_level1::DotImpl(context, left, right, result);
}

Status Dotu(const ExecutionContext& context,
            Vector<const std::complex<float>> left,
            Vector<const std::complex<float>> right,
            Vector<std::complex<float>> result) {
  return internal_dense_blas_level1::ComplexDotImpl<std::complex<float>, false>(
      context, left, right, result);
}

Status Dotu(const ExecutionContext& context,
            Vector<const std::complex<double>> left,
            Vector<const std::complex<double>> right,
            Vector<std::complex<double>> result) {
  return internal_dense_blas_level1::ComplexDotImpl<std::complex<double>,
                                                    false>(context, left, right,
                                                           result);
}

Status Dotc(const ExecutionContext& context,
            Vector<const std::complex<float>> left,
            Vector<const std::complex<float>> right,
            Vector<std::complex<float>> result) {
  return internal_dense_blas_level1::ComplexDotImpl<std::complex<float>, true>(
      context, left, right, result);
}

Status Dotc(const ExecutionContext& context,
            Vector<const std::complex<double>> left,
            Vector<const std::complex<double>> right,
            Vector<std::complex<double>> result) {
  return internal_dense_blas_level1::ComplexDotImpl<std::complex<double>, true>(
      context, left, right, result);
}

Status Nrm2(const ExecutionContext& context, Vector<const float> operand,
            Vector<float> result) {
  return internal_dense_blas_level1::Nrm2Impl(context, operand, result);
}

Status Nrm2(const ExecutionContext& context, Vector<const double> operand,
            Vector<double> result) {
  return internal_dense_blas_level1::Nrm2Impl(context, operand, result);
}

Status Nrm2(const ExecutionContext& context,
            Vector<const std::complex<float>> operand, Vector<float> result) {
  return internal_dense_blas_level1::Nrm2Impl(context, operand, result);
}

Status Nrm2(const ExecutionContext& context,
            Vector<const std::complex<double>> operand, Vector<double> result) {
  return internal_dense_blas_level1::Nrm2Impl(context, operand, result);
}

Status Asum(const ExecutionContext& context, Vector<const float> operand,
            Vector<float> result) {
  return internal_dense_blas_level1::AsumImpl(context, operand, result);
}

Status Asum(const ExecutionContext& context, Vector<const double> operand,
            Vector<double> result) {
  return internal_dense_blas_level1::AsumImpl(context, operand, result);
}

Status Asum(const ExecutionContext& context,
            Vector<const std::complex<float>> operand, Vector<float> result) {
  return internal_dense_blas_level1::AsumImpl(context, operand, result);
}

Status Asum(const ExecutionContext& context,
            Vector<const std::complex<double>> operand, Vector<double> result) {
  return internal_dense_blas_level1::AsumImpl(context, operand, result);
}

Status Iamax(const ExecutionContext& context, Vector<const float> operand,
             Vector<index_t> result) {
  return internal_dense_blas_level1::IamaxImpl(context, operand, result);
}

Status Iamax(const ExecutionContext& context, Vector<const double> operand,
             Vector<index_t> result) {
  return internal_dense_blas_level1::IamaxImpl(context, operand, result);
}

Status Iamax(const ExecutionContext& context,
             Vector<const std::complex<float>> operand,
             Vector<index_t> result) {
  return internal_dense_blas_level1::IamaxImpl(context, operand, result);
}

Status Iamax(const ExecutionContext& context,
             Vector<const std::complex<double>> operand,
             Vector<index_t> result) {
  return internal_dense_blas_level1::IamaxImpl(context, operand, result);
}

}  // namespace asc
