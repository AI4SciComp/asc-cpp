#include <array>
#include <cmath>
#include <cstddef>
#include <type_traits>

#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_machine.h"
#include "internal_tridiagonal.h"

namespace asc {
namespace {
namespace checked = internal_tridiagonal;

template <typename T>
Status Execute(const ReferenceLapackProvider& provider,
               LapackMachineParameter parameter, DenseBlasVectorView<T> value,
               LapackReport& report) {
  const auto operand = value.reachable_storage();
  const std::array metadata{checked::ObjectStorage(provider),
                            checked::ObjectStorage(report)};
  Status status = checked::Disjoint(metadata);
  if (!status.ok()) {
    return status;
  }
  for (const auto memory : metadata) {
    if (checked::Overlap(memory, operand)) {
      return Status(ErrorCode::kInvalidArgument);
    }
  }
  checked::StartReport(provider, std::is_same_v<T, float> ? "slamch" : "dlamch",
                       report);
  constexpr std::array kSelectors{'E', 'S', 'B', 'P', 'N',
                                  'R', 'M', 'U', 'L', 'O'};
  const auto index = static_cast<std::size_t>(parameter);
  if (index >= kSelectors.size()) {
    return Status(ErrorCode::kInvalidArgument);
  }
  status = checked::Vector(provider, value, 1);
  if (!status.ok()) {
    return status;
  }
  char selector = kSelectors[index];
  report.called_provider = true;
  const T result = [&] {
    if constexpr (std::is_same_v<T, float>) {
      return LAPACK_slamch(&selector);
    } else {
      return LAPACK_dlamch(&selector);
    }
  }();
  const bool invalid_sign =
      parameter == LapackMachineParameter::kMinimumExponent ? result >= 0
                                                            : result <= 0;
  if (selector != kSelectors[index] || !std::isfinite(result) || invalid_sign) {
    report.outcome = LapackOutcome::kPartialResult;
    report.output_validity = LapackOutputValidity::kUnusable;
    return Status(ErrorCode::kProvider);
  }
  value.data()[0] = result;
  return checked::Complete(report);
}
}  // namespace

Status Lamch(const ReferenceLapackProvider& provider,
             LapackMachineParameter parameter, DenseBlasVectorView<float> value,
             LapackReport& report) {
  return Execute(provider, parameter, value, report);
}

Status Lamch(const ReferenceLapackProvider& provider,
             LapackMachineParameter parameter,
             DenseBlasVectorView<double> value, LapackReport& report) {
  return Execute(provider, parameter, value, report);
}

}  // namespace asc
