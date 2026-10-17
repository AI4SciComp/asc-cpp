#include <array>
#include <cmath>
#include <string_view>
#include <type_traits>

#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_scalar_norm.h"
#include "internal_tridiagonal.h"
namespace asc {
namespace {
namespace checked = internal_tridiagonal;
template <typename T, bool Three>
constexpr std::string_view Routine() {
  if constexpr (std::is_same_v<T, float>) {
    return Three ? "slapy3" : "slapy2";
  } else {
    return Three ? "dlapy3" : "dlapy2";
  }
}
template <typename T, bool Three>
T Native(T x, T y, T z) {
  if constexpr (std::is_same_v<T, float>) {
    return Three ? LAPACK_slapy3(&x, &y, &z) : LAPACK_slapy2(&x, &y);
  } else {
    return Three ? LAPACK_dlapy3(&x, &y, &z) : LAPACK_dlapy2(&x, &y);
  }
}
template <typename T, bool Three>
Status Execute(const ReferenceLapackProvider& provider, T x, T y, T z,
               DenseBlasVectorView<T> result, LapackReport& report) {
  const std::array spans{result.reachable_storage(),
                         checked::ObjectStorage(provider),
                         checked::ObjectStorage(report)};
  Status status = checked::Disjoint(spans);
  if (!status.ok()) {
    return status;
  }
  checked::StartReport(provider, Routine<T, Three>(), report);
  if (result.size() != 1 || result.increment() != 1) {
    return Status(ErrorCode::kShape);
  }
  status = checked::Access(provider, result.reachable_storage());
  if (!status.ok()) {
    return status;
  }
  report.called_provider = true;
  const T value = Native<T, Three>(x, y, z);
  if (value < 0) {
    report.outcome = LapackOutcome::kPartialResult;
    return Status(ErrorCode::kProvider);
  }
  result.data()[0] = value;
  status = checked::Complete(report);
  if (!std::isfinite(value)) {
    report.outcome = LapackOutcome::kAccuracyWarning;
    return Status(ErrorCode::kNumerical);
  }
  return status;
}
}  // namespace
#define ASC_SCALAR_NORM(TYPE)                                                 \
  Status Lapy2(const ReferenceLapackProvider& provider, TYPE x, TYPE y,       \
               DenseBlasVectorView<TYPE> result, LapackReport& report) {      \
    return Execute<TYPE, false>(provider, x, y, static_cast<TYPE>(0), result, \
                                report);                                      \
  }                                                                           \
  Status Lapy3(const ReferenceLapackProvider& provider, TYPE x, TYPE y,       \
               TYPE z, DenseBlasVectorView<TYPE> result,                      \
               LapackReport& report) {                                        \
    return Execute<TYPE, true>(provider, x, y, z, result, report);            \
  }
ASC_SCALAR_NORM(float)
ASC_SCALAR_NORM(double)
#undef ASC_SCALAR_NORM
}  // namespace asc
