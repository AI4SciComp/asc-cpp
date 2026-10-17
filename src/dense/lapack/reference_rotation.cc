#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string_view>
#include <type_traits>

#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_rotation.h"
#include "internal_rotation_prototypes.h"
#include "internal_tridiagonal.h"

namespace asc {
namespace {
namespace checked = internal_tridiagonal;

template <typename T, bool Positive>
constexpr std::string_view Routine() {
  if constexpr (Positive) {
    return std::is_same_v<T, float> ? "slartgp" : "dlartgp";
  } else {
    return std::is_same_v<T, float> ? "slartg" : "dlartg";
  }
}

template <typename T>
auto Bits(T value) {
  return std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
}

template <typename T, bool Positive>
void Native(T& f, T& g, T& c, T& s, T& r) {
  if constexpr (Positive && std::is_same_v<T, float>) {
    LAPACK_slartgp(&f, &g, &c, &s, &r);
  } else if constexpr (Positive) {
    LAPACK_dlartgp(&f, &g, &c, &s, &r);
  } else if constexpr (std::is_same_v<T, float>) {
    ASC_LAPACK_slartg(&f, &g, &c, &s, &r);
  } else {
    ASC_LAPACK_dlartg(&f, &g, &c, &s, &r);
  }
}

template <typename T, bool Positive = false>
Status Execute(const ReferenceLapackProvider& provider, T f, T g,
               DenseBlasVectorView<T> cosine, DenseBlasVectorView<T> sine,
               DenseBlasVectorView<T> rotated, LapackReport& report) {
  const std::array operands{cosine.reachable_storage(),
                            sine.reachable_storage(),
                            rotated.reachable_storage()};
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
  checked::StartReport(provider, Routine<T, Positive>(), report);
  status = checked::Disjoint(operands);
  if (!status.ok()) {
    return status;
  }
  for (const auto output : {cosine, sine, rotated}) {
    status = checked::Vector(provider, output, 1);
    if (!status.ok()) {
      return status;
    }
  }
  if (!std::isfinite(f) || !std::isfinite(g)) {
    report.outcome = LapackOutcome::kAccuracyWarning;
    return Status(ErrorCode::kNumerical);
  }
  const auto original_f = Bits(f);
  const auto original_g = Bits(g);
  T c = std::numeric_limits<T>::quiet_NaN();
  T s = c;
  T r = c;
  report.called_provider = true;
  Native<T, Positive>(f, g, c, s, r);
  const bool invalid_sign = Positive ? r < 0 : c < 0;
  if (Bits(f) != original_f || Bits(g) != original_g || !std::isfinite(c) ||
      invalid_sign || std::abs(c) > 1 || !std::isfinite(s) || std::abs(s) > 1 ||
      std::isnan(r)) {
    report.outcome = LapackOutcome::kPartialResult;
    report.output_validity = LapackOutputValidity::kUnusable;
    return Status(ErrorCode::kProvider);
  }
  cosine.data()[0] = c;
  sine.data()[0] = s;
  rotated.data()[0] = r;
  status = checked::Complete(report);
  if (!std::isfinite(r)) {
    report.outcome = LapackOutcome::kAccuracyWarning;
    return Status(ErrorCode::kNumerical);
  }
  return status;
}
}  // namespace

Status Lartg(const ReferenceLapackProvider& provider, float f, float g,
             DenseBlasVectorView<float> cosine, DenseBlasVectorView<float> sine,
             DenseBlasVectorView<float> rotated, LapackReport& report) {
  return Execute(provider, f, g, cosine, sine, rotated, report);
}

Status Lartg(const ReferenceLapackProvider& provider, double f, double g,
             DenseBlasVectorView<double> cosine,
             DenseBlasVectorView<double> sine,
             DenseBlasVectorView<double> rotated, LapackReport& report) {
  return Execute(provider, f, g, cosine, sine, rotated, report);
}
Status Lartgp(const ReferenceLapackProvider& provider, float f, float g,
              DenseBlasVectorView<float> cosine,
              DenseBlasVectorView<float> sine,
              DenseBlasVectorView<float> rotated, LapackReport& report) {
  return Execute<float, true>(provider, f, g, cosine, sine, rotated, report);
}

Status Lartgp(const ReferenceLapackProvider& provider, double f, double g,
              DenseBlasVectorView<double> cosine,
              DenseBlasVectorView<double> sine,
              DenseBlasVectorView<double> rotated, LapackReport& report) {
  return Execute<double, true>(provider, f, g, cosine, sine, rotated, report);
}

}  // namespace asc
