#include "asc/sparse/matrix_market.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

#include "asc/core/array_format.h"
#include "asc/core/array_io.h"
#include "asc/core/io.h"
#include "asc/core/matrix_market.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/sparse/coordinate.h"

namespace asc {
namespace internal_sparse_matrix_market {

Status ValidateOptions(const SparseMatrixMarketReadOptions& options) {
  if ((options.duplicate_policy != DuplicatePolicy::kReject &&
       options.duplicate_policy != DuplicatePolicy::kSum) ||
      (options.zero_policy != ExplicitZeroPolicy::kKeep &&
       options.zero_policy != ExplicitZeroPolicy::kDrop) ||
      (options.pattern_policy != MatrixMarketPatternPolicy::kUnspecified &&
       options.pattern_policy != MatrixMarketPatternPolicy::kUnit)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  return Status::Ok();
}

Status ValidateOptions(const SparseMatrixMarketWriteOptions& options) {
  if (internal_matrix_market::SymmetryName(options.symmetry).empty() ||
      (options.zero_policy != ExplicitZeroPolicy::kKeep &&
       options.zero_policy != ExplicitZeroPolicy::kDrop) ||
      (options.pattern_policy != MatrixMarketPatternPolicy::kUnspecified &&
       options.pattern_policy != MatrixMarketPatternPolicy::kUnit)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (options.pattern_policy == MatrixMarketPatternPolicy::kUnit &&
      options.symmetry != MatrixMarketSymmetry::kGeneral &&
      options.symmetry != MatrixMarketSymmetry::kSymmetric) {
    return Status(ErrorCode::kEncoding);
  }
  return Status::Ok();
}

Status ValidateShape(std::span<const extent_t, 2> shape,
                     const ArrayIoLimits& limits) {
  if (limits.max_rank < 2) {
    return Status(ErrorCode::kAllocation);
  }
  for (const auto extent : shape) {
    if (extent < 0 || static_cast<std::uint64_t>(extent) > limits.max_extent) {
      return Status(ErrorCode::kShape);
    }
  }
  const auto logical = internal_array_io::MultiplySize(shape[0], shape[1]);
  if (!logical.ok()) {
    return logical.status();
  }
  if (static_cast<std::uint64_t>(*logical) > limits.max_logical_elements) {
    return Status(ErrorCode::kShape);
  }
  return Status::Ok();
}

}  // namespace internal_sparse_matrix_market

SparseMatrixMarketReader::SparseMatrixMarketReader(
    SparseMatrixMarketReader&& other) noexcept
    : input_(std::move(other.input_)),
      scratch_(other.scratch_),
      options_(other.options_),
      report_(other.report_),
      shape_(other.shape_),
      field_(other.field_),
      symmetry_(other.symmetry_),
      records_(other.records_),
      capacity_(other.capacity_),
      final_bound_(other.final_bound_),
      require_eof_(other.require_eof_),
      ready_(std::exchange(other.ready_, false)) {}

Result<SparseMatrixMarketReader> SparseMatrixMarketReader::Prepare(
    ByteSource& source, std::span<std::byte> scratch,
    const ArrayIoLimits& limits, const SparseMatrixMarketReadOptions& options,
    SparseMatrixMarketReport& report, bool require_eof) {
  if (internal_array_format::Overlaps(&report, sizeof(report), scratch.data(),
                                      scratch.size())) {
    return Status(ErrorCode::kInvalidArgument);
  }
  report = {};
  auto status = internal_array_io::ValidateLimits(limits);
  if (!status.ok()) {
    return status;
  }
  status = internal_sparse_matrix_market::ValidateOptions(options);
  if (!status.ok()) {
    return status;
  }
  if (scratch.size() > limits.max_scratch_bytes) {
    return Status(ErrorCode::kAllocation);
  }
  SparseMatrixMarketReader reader(source, scratch, limits, options, report,
                                  require_eof);
  status = reader.ParseHeader();
  if (!status.ok()) {
    return status;
  }
  reader.ready_ = true;
  return reader;
}

Status SparseMatrixMarketReader::ParseHeader() {
  const auto banner = input_.FirstRecord(scratch_);
  if (!banner.ok()) {
    return banner.status();
  }
  if (banner->count != 5 || banner->fields[0] != "%%MatrixMarket" ||
      !internal_matrix_market::WordEquals(banner->fields[1], "matrix") ||
      !internal_matrix_market::WordEquals(banner->fields[2], "coordinate")) {
    return Fail(ErrorCode::kEncoding);
  }
  auto field = internal_matrix_market::ParseField(banner->fields[3]);
  auto symmetry = internal_matrix_market::ParseSymmetry(banner->fields[4]);
  if (!field.ok() || !symmetry.ok()) {
    return Fail(ErrorCode::kEncoding);
  }
  field_ = *field;
  symmetry_ = *symmetry;
  if ((symmetry_ == MatrixMarketSymmetry::kHermitian &&
       field_ != MatrixMarketField::kComplex) ||
      (field_ == MatrixMarketField::kPattern &&
       symmetry_ != MatrixMarketSymmetry::kGeneral &&
       symmetry_ != MatrixMarketSymmetry::kSymmetric)) {
    return Fail(ErrorCode::kEncoding);
  }
  if (field_ == MatrixMarketField::kPattern &&
      options_.pattern_policy != MatrixMarketPatternPolicy::kUnit) {
    return Fail(ErrorCode::kInvalidArgument);
  }
  return ParseDimensions();
}

Status SparseMatrixMarketReader::ParseDimensions() {
  const auto dimensions = input_.NextRecord(scratch_);
  if (!dimensions.ok()) {
    return dimensions.status();
  }
  if (dimensions->count != 3) {
    return Fail(ErrorCode::kEncoding);
  }
  const auto& limits = input_.limits();
  for (std::size_t i = 0; i < 2; ++i) {
    const auto extent = internal_matrix_market::ParseUnsigned(
        dimensions->fields[i], limits.max_token_bytes);
    if (!extent.ok()) {
      return Fail(extent.status().code());
    }
    const auto converted = internal_array_io::CastSize<extent_t>(*extent);
    if (!converted.ok()) {
      return Fail(converted.status().code());
    }
    shape_[i] = *converted;
  }
  auto status = internal_sparse_matrix_market::ValidateShape(shape_, limits);
  if (!status.ok()) {
    return Fail(status.code());
  }
  if (symmetry_ != MatrixMarketSymmetry::kGeneral && shape_[0] != shape_[1]) {
    return Fail(ErrorCode::kShape);
  }
  const auto count = internal_matrix_market::ParseUnsigned(
      dimensions->fields[2], limits.max_token_bytes);
  if (!count.ok()) {
    return Fail(count.status().code());
  }
  if (*count > limits.max_stored_elements ||
      ((shape_[0] == 0 || shape_[1] == 0) && *count != 0)) {
    return Fail(ErrorCode::kShape);
  }
  auto converted = internal_array_io::CastSize<std::size_t>(*count);
  if (!converted.ok()) {
    return Fail(converted.status().code());
  }
  records_ = *converted;
  const auto logical = static_cast<std::uint64_t>(shape_[0] * shape_[1]);
  auto expanded = std::min(*count, logical);
  if (symmetry_ != MatrixMarketSymmetry::kGeneral) {
    expanded = *count > logical / 2 ? logical : 2 * *count;
  }
  if (expanded > limits.max_stored_elements) {
    return Fail(ErrorCode::kAllocation);
  }
  converted = internal_array_io::CastSize<std::size_t>(expanded);
  if (!converted.ok()) {
    return Fail(converted.status().code());
  }
  final_bound_ = *converted;
  capacity_ = std::max(records_, final_bound_);
  const auto slots = internal_array_io::MultiplySize(capacity_, std::size_t{2});
  if (!slots.ok()) {
    return Fail(slots.status().code());
  }
  report_->required_coordinates = *slots;
  report_->required_values = capacity_;
  report_->io.section = ArrayIoSection::kPayload;
  return Status::Ok();
}

}  // namespace asc
