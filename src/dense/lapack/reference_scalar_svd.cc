#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <limits>
#include <type_traits>

#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_scalar_svd.h"
#include "internal_scalar_svd_prototypes.h"
#include "internal_tridiagonal.h"
namespace asc {
namespace {
namespace checked = internal_tridiagonal;
template <typename T>
auto Bits(T value) {
  return std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
}
template <typename T>
Status Validate(const ReferenceLapackProvider& provider,
                std::array<DenseBlasVectorView<T>, 6> outputs,
                LapackReport& report) {
  const std::array operands{
      outputs[0].reachable_storage(), outputs[1].reachable_storage(),
      outputs[2].reachable_storage(), outputs[3].reachable_storage(),
      outputs[4].reachable_storage(), outputs[5].reachable_storage()};
  const std::array metadata{checked::ObjectStorage(provider),
                            checked::ObjectStorage(report)};
  Status status = checked::Disjoint(metadata);
  if (!status.ok()) {
    return status;
  }
  for (const auto memory : metadata) {
    for (const auto operand : operands) {
      if (checked::Overlap(memory, operand)) {
        return Status(ErrorCode::kInvalidArgument);
      }
    }
  }
  checked::StartReport(provider, std::is_same_v<T, float> ? "slasv2" : "dlasv2",
                       report);
  status = checked::Disjoint(operands);
  if (!status.ok()) {
    return status;
  }
  for (const auto output : outputs) {
    status = checked::Vector(provider, output, 1);
    if (!status.ok()) {
      return status;
    }
  }
  return Status{};
}
template <typename T>
Status Execute(const ReferenceLapackProvider& provider, T f, T g, T h,
               std::array<DenseBlasVectorView<T>, 6> outputs,
               LapackReport& report) {
  Status status = Validate(provider, outputs, report);
  if (!status.ok()) {
    return status;
  }
  int infinities = 0;
  for (T value : {f, g, h}) {
    if (std::isnan(value)) {
      report.outcome = LapackOutcome::kAccuracyWarning;
      return Status(ErrorCode::kNumerical);
    }
    if (std::isinf(value)) {
      ++infinities;
    }
  }
  if (infinities > 1) {
    report.outcome = LapackOutcome::kAccuracyWarning;
    return Status(ErrorCode::kNumerical);
  }
  const auto original_f = Bits(f);
  const auto original_g = Bits(g);
  const auto original_h = Bits(h);
  std::array<T, 6> result{};
  result.fill(std::numeric_limits<T>::quiet_NaN());
  report.called_provider = true;
  if constexpr (std::is_same_v<T, float>) {
    ASC_LAPACK_slasv2(&f, &g, &h, result.data(), &result[1], &result[2],
                      &result[3], &result[4], &result[5]);
  } else {
    ASC_LAPACK_dlasv2(&f, &g, &h, result.data(), &result[1], &result[2],
                      &result[3], &result[4], &result[5]);
  }
  bool invalid = Bits(f) != original_f || Bits(g) != original_g ||
                 Bits(h) != original_h || !std::isfinite(result[0]) ||
                 std::isnan(result[1]) ||
                 std::abs(result[0]) > std::abs(result[1]);
  constexpr T kCoefficientLimit =
      T{1} + T{16} * std::numeric_limits<T>::epsilon();
  for (std::size_t i = 2; i < result.size(); ++i) {
    invalid = invalid || !std::isfinite(result[i]) ||
              std::abs(result[i]) > kCoefficientLimit;
  }
  if (invalid) {
    report.outcome = LapackOutcome::kPartialResult;
    report.output_validity = LapackOutputValidity::kUnusable;
    return Status(ErrorCode::kProvider);
  }
  for (std::size_t i = 0; i < outputs.size(); ++i) {
    outputs[i].data()[0] = result[i];
  }
  status = checked::Complete(report);
  if (!std::isfinite(result[1])) {
    report.outcome = LapackOutcome::kAccuracyWarning;
    return Status(ErrorCode::kNumerical);
  }
  return status;
}
}  // namespace
Status Lasv2(const ReferenceLapackProvider& provider, float f, float g, float h,
             DenseBlasVectorView<float> small, DenseBlasVectorView<float> large,
             DenseBlasVectorView<float> right_sine,
             DenseBlasVectorView<float> right_cosine,
             DenseBlasVectorView<float> left_sine,
             DenseBlasVectorView<float> left_cosine, LapackReport& report) {
  return Execute(provider, f, g, h,
                 std::array{small, large, right_sine, right_cosine, left_sine,
                            left_cosine},
                 report);
}
Status Lasv2(const ReferenceLapackProvider& provider, double f, double g,
             double h, DenseBlasVectorView<double> small,
             DenseBlasVectorView<double> large,
             DenseBlasVectorView<double> right_sine,
             DenseBlasVectorView<double> right_cosine,
             DenseBlasVectorView<double> left_sine,
             DenseBlasVectorView<double> left_cosine, LapackReport& report) {
  return Execute(provider, f, g, h,
                 std::array{small, large, right_sine, right_cosine, left_sine,
                            left_cosine},
                 report);
}
}  // namespace asc
