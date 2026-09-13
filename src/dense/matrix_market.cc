#include "asc/dense/matrix_market.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <string_view>
#include <utility>

#include "asc/core/array_format.h"
#include "asc/core/array_io.h"
#include "asc/core/io.h"
#include "asc/core/matrix_market.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"

namespace asc {
namespace internal_dense_matrix_market {

Result<std::size_t> ValidateHeader(std::span<const extent_t, 2> shape,
                                   MatrixMarketField field,
                                   MatrixMarketSymmetry symmetry,
                                   const ArrayIoLimits& limits) {
  if (limits.max_rank < 2) {
    return Status(ErrorCode::kAllocation);
  }
  if (field == MatrixMarketField::kPattern ||
      internal_matrix_market::FieldName(field).empty() ||
      internal_matrix_market::SymmetryName(symmetry).empty()) {
    return Status(field == MatrixMarketField::kPattern
                      ? ErrorCode::kEncoding
                      : ErrorCode::kInvalidArgument);
  }
  for (const auto extent : shape) {
    if (extent < 0) {
      return Status(ErrorCode::kOverflow);
    }
    if (static_cast<std::uint64_t>(extent) > limits.max_extent) {
      return Status(ErrorCode::kAllocation);
    }
  }
  if (symmetry == MatrixMarketSymmetry::kHermitian &&
      field != MatrixMarketField::kComplex) {
    return Status(ErrorCode::kEncoding);
  }
  if (symmetry != MatrixMarketSymmetry::kGeneral && shape[0] != shape[1]) {
    return Status(ErrorCode::kShape);
  }
  auto product = internal_array_io::MultiplySize(shape[0], shape[1]);
  if (!product.ok()) {
    return product.status();
  }
  if (static_cast<std::uint64_t>(*product) > limits.max_logical_elements) {
    return Status(ErrorCode::kAllocation);
  }
  auto count = internal_array_io::CastSize<std::size_t>(*product);
  if (!count.ok()) {
    return count.status();
  }
  std::size_t stored = *count;
  if (symmetry != MatrixMarketSymmetry::kGeneral) {
    auto n = internal_array_io::CastSize<std::size_t>(shape[0]);
    if (!n.ok()) {
      return n.status();
    }
    if (*n == 0) {
      stored = 0;
    } else {
      std::size_t left = *n;
      std::size_t right =
          symmetry == MatrixMarketSymmetry::kSkewSymmetric ? *n - 1 : *n + 1;
      if (left % 2 == 0) {
        left /= 2;
      } else {
        right /= 2;
      }
      auto triangle = internal_array_io::MultiplySize(left, right);
      if (!triangle.ok()) {
        return triangle.status();
      }
      stored = *triangle;
    }
  }
  if (stored > limits.max_stored_elements) {
    return Status(ErrorCode::kAllocation);
  }
  return stored;
}

Status WriteHeader(std::span<const extent_t, 2> shape, MatrixMarketField field,
                   MatrixMarketSymmetry symmetry, const ArrayIoLimits& limits,
                   std::span<std::byte> scratch,
                   internal_array_io::Output& output) {
  constexpr std::string_view kBanner = "%%MatrixMarket matrix array ";
  const auto field_name = internal_matrix_market::FieldName(field);
  const auto symmetry_name = internal_matrix_market::SymmetryName(symmetry);
  if (kBanner.size() + field_name.size() + symmetry_name.size() + 2 >
      limits.max_header_bytes) {
    return Status(ErrorCode::kAllocation);
  }
  std::size_t dimension_bytes = 2;  // Separating space and final LF.
  for (const auto extent : shape) {
    auto number =
        internal_dense_matrix_market::ValueBytes(extent, scratch, limits);
    if (!number.ok()) {
      return number.status();
    }
    auto next = internal_array_io::AddSize(dimension_bytes, *number);
    if (!next.ok()) {
      return next.status();
    }
    dimension_bytes = *next;
  }
  if (dimension_bytes > limits.max_header_bytes) {
    return Status(ErrorCode::kAllocation);
  }
  for (const auto text : {kBanner, field_name, std::string_view(" "),
                          symmetry_name, std::string_view("\n")}) {
    auto status = internal_matrix_market::Text(output, text);
    if (!status.ok()) {
      return status;
    }
  }
  for (std::size_t i = 0; i < shape.size(); ++i) {
    auto status = internal_matrix_market::WriteValue(
        shape[i], MatrixMarketField::kInteger, scratch, limits.max_token_bytes,
        output);
    if (status.ok()) {
      status = internal_matrix_market::Text(output, i == 0 ? " " : "\n");
    }
    if (!status.ok()) {
      return status;
    }
  }
  return Status::Ok();
}

}  // namespace internal_dense_matrix_market

DenseMatrixMarketReader::DenseMatrixMarketReader(
    DenseMatrixMarketReader&& other) noexcept
    : input_(std::move(other.input_)),
      scratch_(other.scratch_),
      shape_(other.shape_),
      field_(other.field_),
      symmetry_(other.symmetry_),
      count_(other.count_),
      stored_count_(other.stored_count_),
      ready_(std::exchange(other.ready_, false)) {}

Result<DenseMatrixMarketReader> DenseMatrixMarketReader::Prepare(
    ByteSource& source, std::span<std::byte> scratch,
    const ArrayIoLimits& limits, ArrayIoReport& report) {
  if (internal_array_format::Overlaps(scratch.data(), scratch.size(), &report,
                                      sizeof(report)) ||
      internal_array_format::Overlaps(scratch.data(), scratch.size(), &source,
                                      sizeof(source))) {
    return Status(ErrorCode::kInvalidArgument);
  }
  report = {};
  auto status = internal_array_io::ValidateLimits(limits);
  if (!status.ok()) {
    return status;
  }
  if (scratch.size() > limits.max_scratch_bytes) {
    return Status(ErrorCode::kAllocation);
  }
  DenseMatrixMarketReader reader(source, scratch, limits, report);
  status = reader.ParseHeader();
  if (!status.ok()) {
    return status;
  }
  reader.ready_ = true;
  return reader;
}

Status DenseMatrixMarketReader::ParseHeader() {
  auto banner = input_.FirstRecord(scratch_);
  if (!banner.ok()) {
    return banner.status();
  }
  if (banner->count != 5 || banner->fields[0] != "%%MatrixMarket" ||
      !internal_matrix_market::WordEquals(banner->fields[1], "matrix")) {
    return input_.Fail(ErrorCode::kEncoding);
  }
  if (internal_matrix_market::WordEquals(banner->fields[2], "coordinate")) {
    return input_.Fail(ErrorCode::kUnsupported);
  }
  if (!internal_matrix_market::WordEquals(banner->fields[2], "array")) {
    return input_.Fail(ErrorCode::kEncoding);
  }
  auto field = internal_matrix_market::ParseField(banner->fields[3]);
  auto symmetry = internal_matrix_market::ParseSymmetry(banner->fields[4]);
  if (!field.ok() || !symmetry.ok()) {
    return input_.Fail(ErrorCode::kEncoding);
  }
  field_ = *field;
  symmetry_ = *symmetry;
  if (field_ == MatrixMarketField::kPattern ||
      (symmetry_ == MatrixMarketSymmetry::kHermitian &&
       field_ != MatrixMarketField::kComplex)) {
    return input_.Fail(ErrorCode::kEncoding);
  }
  auto dimensions = input_.NextRecord(scratch_);
  if (!dimensions.ok()) {
    return dimensions.status();
  }
  if (dimensions->count != 2) {
    return input_.Fail(ErrorCode::kEncoding);
  }
  for (std::size_t i = 0; i < shape_.size(); ++i) {
    auto extent = internal_matrix_market::ParseUnsigned(
        dimensions->fields[i], input_.limits().max_token_bytes);
    if (!extent.ok()) {
      return input_.Fail(extent.status().code());
    }
    auto converted = internal_array_io::CastSize<extent_t>(*extent);
    if (!converted.ok()) {
      return input_.Fail(converted.status().code());
    }
    shape_[i] = *converted;
  }
  auto stored = internal_dense_matrix_market::ValidateHeader(
      shape_, field_, symmetry_, input_.limits());
  if (!stored.ok()) {
    return input_.Fail(stored.status().code());
  }
  stored_count_ = *stored;
  count_ =
      static_cast<std::size_t>(shape_[0]) * static_cast<std::size_t>(shape_[1]);
  return Status::Ok();
}

}  // namespace asc
