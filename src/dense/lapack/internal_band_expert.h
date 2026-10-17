#ifndef ASC_DENSE_LAPACK_INTERNAL_BAND_EXPERT_H_
#define ASC_DENSE_LAPACK_INTERNAL_BAND_EXPERT_H_

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <type_traits>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "internal_band_abi.h"
#include "internal_layout.h"
#include "internal_workspace_context.h"

// ASC-authored packing/admission logic shared only by the new band experts.
// The separate amended factor/solve dependency remains independently frozen.
namespace asc::internal_band_expert {

inline bool Overlap(ConstMemoryView a, ConstMemoryView b) {
  if (a.size() == 0 || b.size() == 0) {
    return false;
  }
  const auto first = reinterpret_cast<std::uintptr_t>(a.data());
  const auto second = reinterpret_cast<std::uintptr_t>(b.data());
  return first <= second ? second - first < a.size()
                         : first - second < b.size();
}
template <typename T>
ConstMemoryView ObjectStorage(const T& value) {
  return {&value, sizeof(value), MemorySpace::kHost};
}
inline Status CheckMetadata(const ReferenceLapackProvider& provider,
                            const LapackWorkspacePlan& plan,
                            const LapackWorkspace& workspace,
                            LapackReport& report,
                            std::span<const ConstMemoryView> operands) {
  const std::array metadata{ObjectStorage(provider), ObjectStorage(plan),
                            ObjectStorage(workspace), ObjectStorage(report)};
  for (std::size_t i = 0; i < metadata.size(); ++i) {
    for (std::size_t j = i + 1; j < metadata.size(); ++j) {
      if (Overlap(metadata[i], metadata[j])) {
        return Status(ErrorCode::kInvalidArgument);
      }
    }
    for (const auto operand : operands) {
      if (Overlap(metadata[i], operand)) {
        return Status(ErrorCode::kInvalidArgument);
      }
    }
    for (const auto& region : workspace.regions) {
      if (Overlap(metadata[i], region)) {
        return Status(ErrorCode::kInvalidArgument);
      }
    }
  }
  return Status::Ok();
}
inline Status Admit(const ReferenceLapackProvider& provider,
                    MemorySpace space) {
  if ((space != MemorySpace::kHost && space != MemorySpace::kPinnedHost) ||
      !provider.context().CanAccess(space)) {
    return Status(ErrorCode::kMemoryAccess);
  }
  return Status::Ok();
}
template <typename T>
extent_t LeadingDimension(LapackPositiveDefiniteBandView<T> band) {
  return band.layout() == DenseBlasLayout::kColumnMajor
             ? band.storage().leading_dimension()
             : band.bandwidth() + 1;
}
template <typename T>
Status AddBandPacking(LapackPositiveDefiniteBandView<T> band,
                      LapackWorkspacePlan& plan) {
  if (band.layout() == DenseBlasLayout::kColumnMajor) {
    return Status::Ok();
  }
  const extent_t width = band.bandwidth() + 1;
  if (band.order() > std::numeric_limits<extent_t>::max() / width) {
    return Status(ErrorCode::kOverflow);
  }
  const extent_t count = band.order() * width;
  auto& region = plan.regions[internal_lapack_layout::kRegion];
  if (count > std::numeric_limits<extent_t>::max() - region.minimum_entries) {
    return Status(ErrorCode::kOverflow);
  }
  const extent_t total = count + region.minimum_entries;
  region = {total, total, sizeof(T), alignof(T)};
  return internal_lapack_layout::CheckTotal(plan);
}
inline Status ValidatePlan(const ReferenceLapackProvider& provider,
                           const LapackWorkspacePlan& expected,
                           const LapackWorkspacePlan& supplied,
                           const LapackWorkspace& workspace,
                           std::span<const ConstMemoryView> operands) {
  if (expected.total_byte_limit != supplied.total_byte_limit) {
    return Status(ErrorCode::kInvalidState);
  }
  for (std::size_t i = 0; i < expected.regions.size(); ++i) {
    const auto& a = expected.regions[i];
    const auto& b = supplied.regions[i];
    if (a.minimum_entries != b.minimum_entries ||
        a.preferred_entries != b.preferred_entries ||
        a.entry_bytes != b.entry_bytes || a.alignment != b.alignment) {
      return Status(ErrorCode::kInvalidState);
    }
  }
  return internal_lapack_workspace::Validate(
      provider, supplied, expected.identity, workspace, operands);
}
// Visit only selected coordinates, never the corner/padding slots.
template <typename T, typename Function>
void Visit(LapackPositiveDefiniteBandView<T> band, Function function) {
  const bool upper = band.triangle() == DenseBlasTriangle::kUpper;
  for (extent_t j = 0; j < band.order(); ++j) {
    const extent_t first = upper ? j - std::min(band.bandwidth(), j) : j;
    const extent_t last =
        upper ? j : j + std::min(band.bandwidth(), band.order() - 1 - j);
    for (extent_t i = first; i <= last; ++i) {
      function(i, j);
    }
  }
}
template <typename T>
extent_t Offset(LapackPositiveDefiniteBandView<T> band, extent_t i,
                extent_t j) {
  const bool upper = band.triangle() == DenseBlasTriangle::kUpper;
  const auto ld = band.storage().leading_dimension();
  if (band.layout() == DenseBlasLayout::kColumnMajor) {
    return j * ld + (upper ? band.bandwidth() - (j - i) : i - j);
  }
  return i * ld + (upper ? j - i : band.bandwidth() - (i - j));
}
template <typename T>
T* PackBand(LapackPositiveDefiniteBandView<T> band,
            std::remove_const_t<T>*& cursor, bool hermitian_input) {
  if (band.layout() == DenseBlasLayout::kColumnMajor || band.order() == 0) {
    return band.storage().data();
  }
  auto* result = cursor;
  const auto width = band.bandwidth() + 1;
  const bool upper = band.triangle() == DenseBlasTriangle::kUpper;
  Visit(band, [&](extent_t i, extent_t j) {
    const auto physical = upper ? band.bandwidth() - (j - i) : i - j;
    if constexpr (DenseBlasComplex<std::remove_const_t<T>>) {
      if (hermitian_input && i == j) {
        result[j * width + physical] = std::remove_const_t<T>(
            band.storage().data()[Offset(band, i, j)].real(), 0);
        return;
      }
    }
    // Raw factors retain every selected complex component.
    result[j * width + physical] = band.storage().data()[Offset(band, i, j)];
  });
  cursor += band.order() * width;
  return result;
}
template <typename T>
void UnpackBand(const T* packed, LapackPositiveDefiniteBandView<T> band,
                extent_t normalized_diagonal_prefix) {
  if (band.layout() == DenseBlasLayout::kColumnMajor) {
    return;
  }
  const auto width = band.bandwidth() + 1;
  const bool upper = band.triangle() == DenseBlasTriangle::kUpper;
  Visit(band, [&](extent_t i, extent_t j) {
    const auto physical = upper ? band.bandwidth() - (j - i) : i - j;
    if constexpr (DenseBlasComplex<T>) {
      if (i == j) {
        auto& destination = band.storage().data()[Offset(band, i, j)];
        const auto& source = packed[j * width + physical];
        destination.real(source.real());
        if (i < normalized_diagonal_prefix) {
          destination.imag(source.imag());
        }
        return;
      }
    }
    band.storage().data()[Offset(band, i, j)] = packed[j * width + physical];
  });
}
inline void StartReport(const ReferenceLapackProvider& provider,
                        std::string_view name, LapackReport& report) {
  report = {};
  report.provider = provider.identity();
  std::copy(name.begin(), name.end(), report.routine.begin());
}
inline Status InterpretInfo(lapack_int info, extent_t order, bool factor,
                            LapackReport& report) {
  report.native_info = info;
  if (info == 0) {
    report.outcome = LapackOutcome::kSuccess;
    report.output_validity = LapackOutputValidity::kComplete;
    return Status::Ok();
  }
  if (info < 0) {
    report.outcome = LapackOutcome::kProviderArgument;
    report.output_validity = LapackOutputValidity::kUnusable;
    if (static_cast<std::int64_t>(info) !=
        std::numeric_limits<std::int64_t>::min()) {
      report.native_argument = -static_cast<std::int64_t>(info);
    }
    return Status(ErrorCode::kProvider);
  }
  if (factor && info <= order) {
    report.outcome = LapackOutcome::kNotPositiveDefinite;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    report.diagnostic_index = static_cast<index_t>(info) - 1;
    return Status(ErrorCode::kNumerical);
  }
  report.outcome = LapackOutcome::kPartialResult;
  report.output_validity = LapackOutputValidity::kUnusable;
  return Status(ErrorCode::kProvider);
}

inline Status CheckOperands(const ReferenceLapackProvider& provider,
                            std::span<const ConstMemoryView> operands) {
  for (std::size_t i = 0; i < operands.size(); ++i) {
    auto status = Admit(provider, operands[i].space());
    if (!status.ok()) {
      return status;
    }
    for (std::size_t j = i + 1; j < operands.size(); ++j) {
      if (Overlap(operands[i], operands[j])) {
        return Status(ErrorCode::kInvalidArgument);
      }
    }
  }
  return Status::Ok();
}

}  // namespace asc::internal_band_expert

#endif  // ASC_DENSE_LAPACK_INTERNAL_BAND_EXPERT_H_
