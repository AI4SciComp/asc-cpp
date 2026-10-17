#include <array>
#include <cmath>
#include <limits>
#include <type_traits>

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_scalar_sort.h"
#include "internal_scalar_sort_counts.h"
#include "internal_tridiagonal.h"
namespace asc {
namespace {
template <typename T>
Status Output(LapackSortOrder order, DenseBlasVectorView<T> values,
              LapackReport& report) {
  bool nan = false;
  bool ordered = true;
  T previous{};
  for (extent_t i = 0; i < values.size(); ++i) {
    const T value = values.data()[i];
    nan = nan || std::isnan(value);
    if (i != 0 && (order == LapackSortOrder::kIncreasing ? value < previous
                                                       : value > previous)) {
      ordered = false;
    }
    previous = value;
  }
  if (nan) {
    report.outcome = LapackOutcome::kAccuracyWarning;
    report.output_validity = LapackOutputValidity::kComplete;
    return Status(ErrorCode::kNumerical);
  }
  if (!ordered) {
    report.outcome = LapackOutcome::kPartialResult;
    report.output_validity = LapackOutputValidity::kUnusable;
    return Status(ErrorCode::kProvider);
  }
  return internal_tridiagonal::Complete(report);
}
template <typename T>
Status Execute(const ReferenceLapackProvider& provider, LapackSortOrder order,
               DenseBlasVectorView<T> values, LapackReport& report) {
  namespace checked = internal_tridiagonal;
  const std::array spans{values.reachable_storage(),
                         checked::ObjectStorage(provider),
                         checked::ObjectStorage(report)};
  Status status = checked::Disjoint(spans);
  if (!status.ok()) {
    return status;
  }
  checked::StartReport(provider, std::is_same_v<T, float> ? "slasrt" : "dlasrt",
                       report);
  if (order != LapackSortOrder::kIncreasing &&
      order != LapackSortOrder::kDecreasing) {
    return Status(ErrorCode::kInvalidArgument);
  }
  status = internal_scalar_sort_counts::Count(values.size(), checked::kLimit);
  if (!status.ok()) {
    return status;
  }
  if (values.increment() != 1) {
    return Status(ErrorCode::kShape);
  }
  status = checked::Access(provider, values.reachable_storage());
  if (!status.ok()) {
    return status;
  }
  if (values.size() == 0) {
    return checked::Complete(report);
  }
  const char id = order == LapackSortOrder::kIncreasing ? 'I' : 'D';
  const lapack_int n = static_cast<lapack_int>(values.size());
  lapack_int info = std::numeric_limits<lapack_int>::max();
  report.called_provider = true;
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_slasrt(&id, &n, values.data(), &info);
  } else {
    LAPACK_dlasrt(&id, &n, values.data(), &info);
  }
  report.native_info = info;
  if (info != 0) {
    return checked::ProviderDefect(info, report);
  }
  return Output(order, values, report);
}
}  // namespace
Status Lasrt(const ReferenceLapackProvider& provider, LapackSortOrder order,
             DenseBlasVectorView<float> values, LapackReport& report) {
  return Execute(provider, order, values, report);
}
Status Lasrt(const ReferenceLapackProvider& provider, LapackSortOrder order,
             DenseBlasVectorView<double> values, LapackReport& report) {
  return Execute(provider, order, values, report);
}
}  // namespace asc
