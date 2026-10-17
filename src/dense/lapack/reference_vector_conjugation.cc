#include <array>
#include <complex>
#include <type_traits>

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_vector_conjugation.h"
#include "internal_tridiagonal.h"
#include "internal_vector_conjugation_counts.h"

namespace asc {
namespace {
template <typename T>
Status Execute(const ReferenceLapackProvider& provider, extent_t n,
               stride_t increment, DenseBlasVectorView<std::complex<T>> storage,
               LapackReport& report) {
  namespace checked = internal_tridiagonal;
  const std::array spans{storage.reachable_storage(),
                         checked::ObjectStorage(provider),
                         checked::ObjectStorage(report)};
  Status status = checked::Disjoint(spans);
  if (!status.ok()) {
    return status;
  }
  checked::StartReport(provider, std::is_same_v<T, float> ? "clacgv" : "zlacgv",
                       report);
  const auto count = internal_vector_conjugation_counts::Storage(
      n, increment, checked::kLimit);
  if (!count.ok()) {
    return count.status();
  }
  if (storage.size() != *count || storage.increment() != 1) {
    return Status(ErrorCode::kShape);
  }
  status = checked::Access(provider, storage.reachable_storage());
  if (!status.ok()) {
    return status;
  }
  if (n == 0) {
    return checked::Complete(report);
  }
  const lapack_int native_n = static_cast<lapack_int>(n);
  const lapack_int native_increment =
      n == 1 ? 1 : static_cast<lapack_int>(increment);
  report.called_provider = true;
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_clacgv(&native_n, storage.data(), &native_increment);
  } else {
    LAPACK_zlacgv(&native_n, storage.data(), &native_increment);
  }
  return checked::Complete(report);
}
}  // namespace
Status Lacgv(const ReferenceLapackProvider& provider, extent_t n,
             stride_t increment,
             DenseBlasVectorView<std::complex<float>> storage,
             LapackReport& report) {
  return Execute(provider, n, increment, storage, report);
}
Status Lacgv(const ReferenceLapackProvider& provider, extent_t n,
             stride_t increment,
             DenseBlasVectorView<std::complex<double>> storage,
             LapackReport& report) {
  return Execute(provider, n, increment, storage, report);
}
}  // namespace asc
