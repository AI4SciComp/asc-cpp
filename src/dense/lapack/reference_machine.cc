#include <array>
#include <bit>
#include <cstddef>
#include <cstring>
#include <type_traits>

#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_machine.h"
#include "internal_machine_prototypes.h"
#include "internal_tridiagonal.h"

namespace asc {
namespace {
namespace checked = internal_tridiagonal;

template <typename T>
Status Execute(const ReferenceLapackProvider& provider,
               DenseBlasVectorView<T> small, DenseBlasVectorView<T> large,
               LapackReport& report) {
  const std::array operands{small.reachable_storage(),
                            large.reachable_storage()};
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
  checked::StartReport(provider, std::is_same_v<T, float> ? "slabad" : "dlabad",
                       report);
  status = checked::Disjoint(operands);
  if (!status.ok()) {
    return status;
  }
  for (const auto operand : {small, large}) {
    status = checked::Vector(provider, operand, 1);
    if (!status.ok()) {
      return status;
    }
  }
  T native_small;
  T native_large;
  std::memcpy(&native_small, small.data(), sizeof(T));
  std::memcpy(&native_large, large.data(), sizeof(T));
  using Representation = std::array<std::byte, sizeof(T)>;
  const auto original_small = std::bit_cast<Representation>(native_small);
  const auto original_large = std::bit_cast<Representation>(native_large);
  report.called_provider = true;
  if constexpr (std::is_same_v<T, float>) {
    ASC_LAPACK_slabad(&native_small, &native_large);
  } else {
    ASC_LAPACK_dlabad(&native_small, &native_large);
  }
  if (std::bit_cast<Representation>(native_small) != original_small ||
      std::bit_cast<Representation>(native_large) != original_large) {
    report.outcome = LapackOutcome::kPartialResult;
    report.output_validity = LapackOutputValidity::kUnusable;
    return Status(ErrorCode::kProvider);
  }
  return checked::Complete(report);
}
}  // namespace

Status Labad(const ReferenceLapackProvider& provider,
             DenseBlasVectorView<float> small, DenseBlasVectorView<float> large,
             LapackReport& report) {
  return Execute(provider, small, large, report);
}

Status Labad(const ReferenceLapackProvider& provider,
             DenseBlasVectorView<double> small,
             DenseBlasVectorView<double> large, LapackReport& report) {
  return Execute(provider, small, large, report);
}

}  // namespace asc
