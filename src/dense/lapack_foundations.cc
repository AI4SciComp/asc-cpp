#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>

#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"

namespace asc {
namespace {

bool IsCpu(MemorySpace space) {
  return space == MemorySpace::kHost || space == MemorySpace::kPinnedHost;
}

bool ValidAbi(LapackIntegerAbi abi) {
  return abi == LapackIntegerAbi::kNative64 || abi == LapackIntegerAbi::kLp64 ||
         abi == LapackIntegerAbi::kIlp64 ||
         abi == LapackIntegerAbi::kSuffixed64;
}

Status ValidateSpan(ConstMemoryView span) {
  if (!span.valid() || !IsCpu(span.space())) {
    return Status(ErrorCode::kMemoryAccess);
  }
  const auto begin = reinterpret_cast<std::uintptr_t>(span.data());
  if (span.size() > std::numeric_limits<std::uintptr_t>::max() - begin) {
    return Status(ErrorCode::kOverflow);
  }
  return Status::Ok();
}

bool Overlaps(ConstMemoryView left, ConstMemoryView right) {
  if (left.size() == 0 || right.size() == 0) {
    return false;
  }
  const auto left_begin = reinterpret_cast<std::uintptr_t>(left.data());
  const auto right_begin = reinterpret_cast<std::uintptr_t>(right.data());
  return left_begin < right_begin + right.size() &&
         right_begin < left_begin + left.size();
}

Result<std::size_t> RequiredBytes(extent_t entries, std::size_t entry_bytes) {
  if (entries < 0 || entry_bytes == 0) {
    return Status(ErrorCode::kInvalidArgument);
  }
  const auto count = static_cast<std::uint64_t>(entries);
  if (count > std::numeric_limits<std::size_t>::max() / entry_bytes) {
    return Status(ErrorCode::kOverflow);
  }
  return static_cast<std::size_t>(count) * entry_bytes;
}

Status ValidateRegion(const LapackWorkspaceRequirement& requirement,
                      ConstMemoryView storage, LapackIntegerAbi abi,
                      LapackWorkspaceKind kind) {
  if (requirement.minimum_entries < 0 ||
      requirement.preferred_entries < requirement.minimum_entries ||
      requirement.alignment == 0 ||
      (requirement.alignment & (requirement.alignment - 1)) != 0) {
    return Status(ErrorCode::kInvalidArgument);
  }
  // Pack/unpack storage is addressed by ASC, not passed as an ABI-sized
  // LWORK/IWORK count. Its dimensions are independently checked by the wrapper.
  if (kind != LapackWorkspaceKind::kLayoutConversion &&
      abi == LapackIntegerAbi::kLp64 &&
      requirement.preferred_entries >
          std::numeric_limits<std::int32_t>::max()) {
    return Status(ErrorCode::kOverflow);
  }
  const auto minimum =
      RequiredBytes(requirement.minimum_entries, requirement.entry_bytes);
  const auto preferred =
      RequiredBytes(requirement.preferred_entries, requirement.entry_bytes);
  if (!minimum.ok()) {
    return minimum.status();
  }
  if (!preferred.ok()) {
    return preferred.status();
  }
  Status validity = ValidateSpan(storage);
  if (!validity.ok()) {
    return validity;
  }
  if (storage.size() < *minimum ||
      (storage.size() != 0 && reinterpret_cast<std::uintptr_t>(storage.data()) %
                                      requirement.alignment !=
                                  0)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  return Status::Ok();
}

Status ValidateAbiRegion(const LapackWorkspaceRequirement& requirement,
                         LapackWorkspaceKind kind,
                         const LapackProviderIdentity& provider) {
  if (requirement.preferred_entries == 0) {
    return Status::Ok();
  }
  std::size_t expected_bytes = 0;
  if (kind == LapackWorkspaceKind::kInteger) {
    expected_bytes = provider.integer_abi == LapackIntegerAbi::kLp64 ? 4 : 8;
  } else if (kind == LapackWorkspaceKind::kLogical) {
    expected_bytes = provider.logical_bytes;
  } else if (kind == LapackWorkspaceKind::kPivotConversion) {
    expected_bytes = sizeof(index_t);
  }
  if (expected_bytes != 0 && requirement.entry_bytes != expected_bytes) {
    return Status(ErrorCode::kConfiguration);
  }
  return Status::Ok();
}

}  // namespace

Result<LapackPlanIdentity> LapackPlanIdentity::Create(
    std::string_view routine, LapackScalarKind scalar,
    std::span<const extent_t> dimensions, std::span<const std::int64_t> options,
    LapackProviderIdentity provider) {
  LapackPlanIdentity identity;
  if (routine.empty() || routine.size() >= identity.routine_.size() ||
      routine.find('\0') != std::string_view::npos ||
      dimensions.size() > identity.dimensions_.size() ||
      options.size() > identity.options_.size() ||
      scalar > LapackScalarKind::kMixedC128C64 ||
      !ValidAbi(provider.integer_abi) ||
      (provider.logical_bytes != 1 && provider.logical_bytes != 4 &&
       provider.logical_bytes != 8)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (provider.kind != LapackProviderKind::kNative &&
      provider.kind != LapackProviderKind::kReference) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if ((provider.kind == LapackProviderKind::kNative) !=
      (provider.integer_abi == LapackIntegerAbi::kNative64)) {
    return Status(ErrorCode::kConfiguration);
  }
  if (provider.kind == LapackProviderKind::kReference &&
      (std::ranges::all_of(
           provider.source_sha256,
           [](std::byte value) { return value == std::byte{}; }) ||
       std::ranges::all_of(provider.build_sha256, [](std::byte value) {
         return value == std::byte{};
       }))) {
    return Status(ErrorCode::kConfiguration);
  }
  for (extent_t dimension : dimensions) {
    if (dimension < 0 ||
        (provider.integer_abi == LapackIntegerAbi::kLp64 &&
         dimension > std::numeric_limits<std::int32_t>::max())) {
      return Status(dimension < 0 ? ErrorCode::kInvalidArgument
                                  : ErrorCode::kOverflow);
    }
  }
  std::ranges::copy(routine, identity.routine_.begin());
  std::ranges::copy(dimensions, identity.dimensions_.begin());
  std::ranges::copy(options, identity.options_.begin());
  identity.routine_size_ = routine.size();
  identity.dimension_count_ = dimensions.size();
  identity.option_count_ = options.size();
  identity.scalar_ = scalar;
  identity.provider_ = provider;
  return identity;
}

Result<extent_t> CheckedLapackQueryEntries(double value, LapackIntegerAbi abi,
                                           std::size_t entry_bytes) {
  if (!std::isfinite(value) || value < 0 || entry_bytes == 0 ||
      !ValidAbi(abi)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  const double rounded = std::ceil(value);
  // The upper-exclusive powers of two are exactly representable. Converting
  // INT64_MAX to double would round up to 2^63 and admit undefined conversion.
  const double upper_exclusive =
      abi == LapackIntegerAbi::kLp64 ? 0x1p31 : 0x1p63;
  if (rounded >= upper_exclusive) {
    return Status(ErrorCode::kOverflow);
  }
  const auto entries = static_cast<extent_t>(rounded);
  const auto bytes = RequiredBytes(entries, entry_bytes);
  if (!bytes.ok()) {
    return bytes.status();
  }
  return entries;
}

Status ValidateLapackWorkspace(
    const LapackWorkspacePlan& plan, const LapackPlanIdentity& actual_identity,
    const LapackWorkspace& workspace,
    std::span<const ConstMemoryView> forbidden_operands) {
  if (plan.identity != actual_identity) {
    return Status(ErrorCode::kInvalidState);
  }
  for (ConstMemoryView operand : forbidden_operands) {
    Status validity = ValidateSpan(operand);
    if (!validity.ok()) {
      return validity;
    }
  }
  std::size_t total = 0;
  for (std::size_t index = 0; index < workspace.regions.size(); ++index) {
    const ConstMemoryView region = workspace.regions[index];
    Status validity = ValidateRegion(plan.regions[index], region,
                                     plan.identity.provider().integer_abi,
                                     static_cast<LapackWorkspaceKind>(index));
    if (!validity.ok()) {
      return validity;
    }
    Status abi_validity = ValidateAbiRegion(
        plan.regions[index], static_cast<LapackWorkspaceKind>(index),
        plan.identity.provider());
    if (!abi_validity.ok()) {
      return abi_validity;
    }
    if (region.size() > plan.total_byte_limit - total) {
      return Status(ErrorCode::kOverflow);
    }
    total += region.size();
    for (std::size_t previous = 0; previous < index; ++previous) {
      if (Overlaps(region, workspace.regions[previous])) {
        return Status(ErrorCode::kInvalidArgument);
      }
    }
    for (ConstMemoryView operand : forbidden_operands) {
      if (Overlaps(region, operand)) {
        return Status(ErrorCode::kInvalidArgument);
      }
    }
  }
  return Status::Ok();
}

void InitializeLapackReport(const LapackPlanIdentity& identity,
                            LapackReport& report) noexcept {
  report = LapackReport{};
  std::ranges::copy(identity.routine(), report.routine.begin());
  report.provider = identity.provider();
}

Result<RawLapackPivotView> RawLapackPivotView::Create(const index_t* values,
                                                      extent_t size,
                                                      LapackFactorFamily family,
                                                      ConstMemoryView backing) {
  if (family > LapackFactorFamily::kAasen || !IsCpu(backing.space())) {
    return Status(ErrorCode::kInvalidArgument);
  }
  const auto bytes = RequiredBytes(size, sizeof(index_t));
  if (!bytes.ok()) {
    return bytes.status();
  }
  const ConstMemoryView reachable(values, *bytes, backing.space());
  const Status validity = ValidateSpan(reachable);
  const Status backing_validity = ValidateSpan(backing);
  if (!validity.ok()) {
    return validity;
  }
  if (!backing_validity.ok()) {
    return backing_validity;
  }
  if (size != 0) {
    const auto begin = reinterpret_cast<std::uintptr_t>(values);
    const auto backing_begin = reinterpret_cast<std::uintptr_t>(backing.data());
    if (begin % alignof(index_t) != 0 || begin < backing_begin ||
        begin + *bytes > backing_begin + backing.size()) {
      return Status(ErrorCode::kInvalidArgument);
    }
  }
  const auto view =
      DenseBlasVectorView<const index_t>::Create(values, size, 1, backing);
  if (!view.ok()) {
    return view.status();
  }
  return RawLapackPivotView(*view, family);
}

Status ValidateLuPivots(RawLapackPivotView pivots, extent_t rows) {
  if (pivots.family() != LapackFactorFamily::kLuPartialPivot || rows < 0 ||
      pivots.values().size() > static_cast<std::uint64_t>(rows)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  for (std::size_t index = 0; index < pivots.values().size(); ++index) {
    const index_t value = pivots.values()[index];
    if (value < static_cast<index_t>(index) + 1 || value > rows) {
      return Status(ErrorCode::kIndex);
    }
  }
  return Status::Ok();
}

Status ConvertLuPivotsToZeroBasedSwaps(RawLapackPivotView pivots, extent_t rows,
                                       std::span<index_t> destination) {
  Status validity = ValidateLuPivots(pivots, rows);
  if (!validity.ok()) {
    return validity;
  }
  if (destination.size() != pivots.values().size()) {
    return Status(ErrorCode::kShape);
  }
  const ConstMemoryView output(destination.data(), destination.size_bytes(),
                               MemorySpace::kHost);
  Status output_validity = ValidateSpan(output);
  if (!output_validity.ok()) {
    return output_validity;
  }
  if (Overlaps(pivots.reachable_storage(), output)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  for (std::size_t index = 0; index < destination.size(); ++index) {
    destination[index] = pivots.values()[index] - 1;
  }
  return Status::Ok();
}

}  // namespace asc
