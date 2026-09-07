#include "asc/core/array_io.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <utility>

#include "asc/core/array_format.h"
#include "asc/core/io.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/io.h"

namespace asc {
namespace {

Status ValidateBuffers(std::span<extent_t> metadata,
                       std::span<std::byte> scratch,
                       const ArrayIoLimits& limits) {
  Status status = internal_array_io::ValidateLimits(limits);
  if (!status.ok()) {
    return status;
  }
  auto metadata_bytes =
      internal_array_io::MultiplySize(metadata.size(), sizeof(extent_t));
  if (!metadata_bytes.ok()) {
    return metadata_bytes.status();
  }
  if (*metadata_bytes > limits.max_scratch_bytes ||
      scratch.size() > limits.max_scratch_bytes - *metadata_bytes) {
    return Status(ErrorCode::kAllocation);
  }
  if (internal_array_format::Overlaps(metadata.data(), *metadata_bytes,
                                      scratch.data(), scratch.size())) {
    return Status(ErrorCode::kInvalidArgument);
  }
  return Status::Ok();
}

Result<std::uint64_t> Unsigned(internal_array_io::Input& input,
                               std::span<std::byte> scratch, char separator) {
  const std::span<char> text(reinterpret_cast<char*>(scratch.data()),
                             scratch.size());
  auto token = input.Token(text, separator);
  if (!token.ok()) {
    return token.status();
  }
  if (token->empty()) {
    return input.Fail(ErrorCode::kEncoding);
  }
  for (char character : *token) {
    if (character < '0' || character > '9') {
      return input.Fail(ErrorCode::kEncoding);
    }
  }
  auto result = internal_array_io::ParseScalar<std::uint64_t>(
      *token, input.limits().max_token_bytes);
  if (!result.ok()) {
    return input.Fail(result.status().code());
  }
  return *result;
}

Status HeaderLine(internal_array_io::Input& input, std::string_view text) {
  Status status = input.Expect(text);
  if (!status.ok()) {
    return status;
  }
  return input.EndOfLine();
}

template <typename T>
T HeaderValue(std::span<const std::byte> bytes, std::size_t offset) {
  return *DecodeLittleEndian<T>(bytes.subspan(offset, sizeof(T)));
}

template <typename T>
void EncodeHeaderValue(T value, std::span<std::byte> bytes,
                       std::size_t offset) {
  // Exact fixed-size subspans were validated before encoding this envelope.
  const Status status =
      EncodeLittleEndian(value, bytes.subspan(offset, sizeof(T)));
  static_cast<void>(status);
}

bool HeaderInteger(internal_array_format::TextBuffer& header,
                   std::uint64_t value, const ArrayIoLimits& limits) {
  const std::size_t before = header.size();
  return header.Integer(value) &&
         header.size() - before <= limits.max_token_bytes;
}

}  // namespace

DenseArrayReader::DenseArrayReader(DenseArrayReader&& other) noexcept
    : input_(std::move(other.input_)),
      shape_(other.shape_),
      scratch_(other.scratch_),
      scalar_(other.scalar_),
      count_(other.count_),
      binary_(other.binary_),
      require_eof_(other.require_eof_),
      ready_(std::exchange(other.ready_, false)) {}

Result<DenseArrayReader> DenseArrayReader::PrepareText(
    ByteSource& source, std::span<extent_t> metadata,
    std::span<std::byte> scratch, const ArrayIoLimits& limits,
    ArrayIoReport& report, bool require_eof) {
  report = {};
  Status status = ValidateBuffers(metadata, scratch, limits);
  if (!status.ok()) {
    return status;
  }
  DenseArrayReader result(source, metadata, scratch, limits, report, false,
                          require_eof);
  status = result.ParseTextHeader();
  if (!status.ok()) {
    return status;
  }
  result.ready_ = true;
  return result;
}

Result<DenseArrayReader> DenseArrayReader::PrepareBinary(
    ByteSource& source, std::span<extent_t> metadata,
    std::span<std::byte> scratch, const ArrayIoLimits& limits,
    ArrayIoReport& report, bool require_eof) {
  report = {};
  Status status = ValidateBuffers(metadata, scratch, limits);
  if (!status.ok()) {
    return status;
  }
  DenseArrayReader result(source, metadata, scratch, limits, report, true,
                          require_eof);
  status = result.ParseBinaryHeader();
  if (!status.ok()) {
    return status;
  }
  result.ready_ = true;
  return result;
}

Status DenseArrayReader::ParseTextHeader() {
  Status status = HeaderLine(input_, "ASCARRAY 1");
  if (!status.ok()) {
    return status;
  }
  status = HeaderLine(input_, "kind dense");
  if (!status.ok()) {
    return status;
  }
  status = input_.Expect("scalar ");
  if (!status.ok()) {
    return status;
  }
  const std::span<char> text(reinterpret_cast<char*>(scratch_.data()),
                             scratch_.size());
  // Scalar names are bounded keywords, not numeric tokens controlled by the
  // caller's numeric-token cap (a one-digit scalar value may use f64).
  auto name = input_.Line(text, 4);
  if (!name.ok()) {
    return name.status();
  }
  auto scalar = internal_array_io::ParseScalarCode(*name);
  if (!scalar.ok()) {
    return Fail(scalar.status().code());
  }
  scalar_ = *scalar;
  status = input_.Expect("rank ");
  if (!status.ok()) {
    return status;
  }
  auto rank = Unsigned(input_, scratch_, '\n');
  if (!rank.ok()) {
    return rank.status();
  }
  if (*rank > input_.limits().max_rank || *rank > shape_.size()) {
    return Fail(ErrorCode::kAllocation);
  }
  shape_ = shape_.first(static_cast<std::size_t>(*rank));
  status = input_.Expect("shape");
  if (!status.ok()) {
    return status;
  }
  status = shape_.empty() ? input_.EndOfLine() : input_.Expect(" ");
  if (!status.ok()) {
    return status;
  }
  for (std::size_t dimension = 0; dimension < shape_.size(); ++dimension) {
    auto extent =
        Unsigned(input_, scratch_, dimension + 1 == shape_.size() ? '\n' : ' ');
    if (!extent.ok()) {
      return extent.status();
    }
    if (*extent > input_.limits().max_extent) {
      return Fail(ErrorCode::kShape);
    }
    shape_[dimension] = static_cast<extent_t>(*extent);
  }
  status = HeaderLine(input_, "order dim0");
  if (!status.ok()) {
    return status;
  }
  status = input_.Expect("count ");
  if (!status.ok()) {
    return status;
  }
  auto count = Unsigned(input_, scratch_, '\n');
  if (!count.ok()) {
    return count.status();
  }
  count_ = *count;
  status = internal_dense_io::ValidateShape(shape_, count_, scalar_,
                                            input_.limits());
  if (!status.ok()) {
    return Fail(status.code());
  }
  input_.report().section = ArrayIoSection::kPayload;
  return HeaderLine(input_, "data");
}

Status DenseArrayReader::ParseBinaryHeader() {
  if (scratch_.size() < 56) {
    return Fail(ErrorCode::kAllocation);
  }
  Status status = input_.Read(scratch_.first(56));
  if (!status.ok()) {
    return status;
  }
  const auto header = scratch_.first(56);
  const std::string_view magic(reinterpret_cast<const char*>(header.data()), 8);
  if (magic != "ASCARRB\n") {
    return Fail(ErrorCode::kEncoding);
  }
  if (HeaderValue<std::uint16_t>(header, 8) != 1 ||
      HeaderValue<std::uint16_t>(header, 10) != 0) {
    return Fail(ErrorCode::kVersion);
  }
  const auto scalar = HeaderValue<std::uint8_t>(header, 13);
  if (HeaderValue<std::uint8_t>(header, 12) != 1 || scalar < 1 || scalar > 12 ||
      HeaderValue<std::uint16_t>(header, 14) != 0 ||
      HeaderValue<std::uint32_t>(header, 20) != 0) {
    return Fail(ErrorCode::kEncoding);
  }
  scalar_ = static_cast<ArrayScalarCode>(scalar);
  const auto rank = HeaderValue<std::uint32_t>(header, 16);
  if (rank > input_.limits().max_rank || rank > shape_.size()) {
    return Fail(ErrorCode::kAllocation);
  }
  count_ = HeaderValue<std::uint64_t>(header, 24);
  const auto payload_bytes = HeaderValue<std::uint64_t>(header, 40);
  const std::uint64_t header_bytes = 56 + std::uint64_t{8} * rank;
  if (HeaderValue<std::uint64_t>(header, 32) != 0 ||
      HeaderValue<std::uint64_t>(header, 48) != header_bytes) {
    return Fail(ErrorCode::kEncoding);
  }
  if (header_bytes > input_.limits().max_header_bytes) {
    return Fail(ErrorCode::kAllocation);
  }
  shape_ = shape_.first(rank);
  for (extent_t& destination : shape_) {
    status = input_.Read(scratch_.first(8));
    if (!status.ok()) {
      return status;
    }
    const auto extent = HeaderValue<std::uint64_t>(scratch_, 0);
    if (extent > input_.limits().max_extent) {
      return Fail(ErrorCode::kShape);
    }
    destination = static_cast<extent_t>(extent);
  }
  status = internal_dense_io::ValidateShape(shape_, count_, scalar_,
                                            input_.limits());
  if (!status.ok()) {
    return Fail(status.code());
  }
  const auto expected = internal_array_io::MultiplySize(
      count_, internal_array_io::ScalarWidth(scalar_));
  if (!expected.ok()) {
    return Fail(ErrorCode::kOverflow);
  }
  if (payload_bytes != *expected) {
    return Fail(ErrorCode::kEncoding);
  }
  auto frame_bytes = internal_array_io::AddSize(header_bytes, payload_bytes);
  if (!frame_bytes.ok()) {
    return Fail(ErrorCode::kOverflow);
  }
  frame_bytes = internal_array_io::AddSize(*frame_bytes, std::uint64_t{4});
  if (!frame_bytes.ok()) {
    return Fail(ErrorCode::kOverflow);
  }
  if (*frame_bytes > input_.limits().max_input_bytes) {
    return Fail(ErrorCode::kAllocation);
  }
  input_.report().section = ArrayIoSection::kPayload;
  return Status::Ok();
}

Status DenseArrayReader::Trailer() {
  input_.report().section = ArrayIoSection::kTrailer;
  Status status;
  if (binary_) {
    const auto expected = input_.checksum();
    if (scratch_.size() < 4) {
      return Fail(ErrorCode::kAllocation);
    }
    status = input_.Read(scratch_.first(4), false);
    if (!status.ok()) {
      return status;
    }
    if (HeaderValue<std::uint32_t>(scratch_, 0) != expected) {
      return Fail(ErrorCode::kEncoding);
    }
  } else {
    status = HeaderLine(input_, "end");
    if (!status.ok()) {
      return status;
    }
  }
  if (require_eof_) {
    status = input_.EndOfFile(!binary_);
    if (!status.ok()) {
      return status;
    }
  }
  input_.report().section = ArrayIoSection::kComplete;
  return Status::Ok();
}

namespace internal_dense_io {

Status ValidateShape(std::span<const extent_t> shape, std::uint64_t count,
                     ArrayScalarCode scalar, const ArrayIoLimits& limits) {
  if (shape.size() > limits.max_rank) {
    return Status(ErrorCode::kAllocation);
  }
  bool empty = false;
  for (extent_t extent : shape) {
    if (extent < 0 || static_cast<std::uint64_t>(extent) > limits.max_extent) {
      return Status(ErrorCode::kShape);
    }
    empty = empty || extent == 0;
  }
  std::uint64_t logical = empty ? 0 : 1;
  if (!empty) {
    for (extent_t extent : shape) {
      if (extent != 0 && logical > std::numeric_limits<std::uint64_t>::max() /
                                       static_cast<std::uint64_t>(extent)) {
        return Status(ErrorCode::kOverflow);
      }
      auto product = internal_array_io::MultiplySize(
          logical, static_cast<std::uint64_t>(extent));
      if (!product.ok()) {
        return Status(ErrorCode::kOverflow);
      }
      logical = *product;
    }
  }
  if (logical > limits.max_logical_elements || logical != count) {
    return Status(ErrorCode::kShape);
  }
  const auto width = internal_array_io::ScalarWidth(scalar);
  if (width == 0) {
    return Status(ErrorCode::kEncoding);
  }
  if (logical > std::numeric_limits<std::uint64_t>::max() / width) {
    return Status(ErrorCode::kOverflow);
  }
  auto bytes = internal_array_io::MultiplySize(logical, width);
  if (!bytes.ok()) {
    return Status(ErrorCode::kOverflow);
  }
  if (*bytes > limits.max_decoded_bytes ||
      logical > std::numeric_limits<std::size_t>::max()) {
    return Status(ErrorCode::kAllocation);
  }
  return Status::Ok();
}

Status WriteHeader(std::span<const extent_t> shape, std::uint64_t count,
                   ArrayScalarCode scalar, bool binary,
                   const ArrayIoLimits& limits, std::span<std::byte> scratch,
                   internal_array_io::Output& output, ArrayIoReport& report) {
  if (!binary) {
    const std::span<char> text(reinterpret_cast<char*>(scratch.data()),
                               scratch.size());
    internal_array_format::TextBuffer header(text);
    header.Write("ASCARRAY 1\nkind dense\nscalar ");
    header.Write(internal_array_io::ScalarName(scalar));
    header.Write("\nrank ");
    if (!HeaderInteger(header, shape.size(), limits)) {
      return Status(ErrorCode::kAllocation);
    }
    header.Write("\nshape");
    for (extent_t extent : shape) {
      header.Write(" ");
      if (!HeaderInteger(header, static_cast<std::uint64_t>(extent), limits)) {
        return Status(ErrorCode::kAllocation);
      }
    }
    header.Write("\norder dim0\ncount ");
    if (!HeaderInteger(header, count, limits)) {
      return Status(ErrorCode::kAllocation);
    }
    header.Write("\n");
    if (!header.ok() || header.size() > limits.max_header_bytes) {
      return Status(ErrorCode::kAllocation);
    }
    Status status = output.Text(header.text());
    if (!status.ok()) {
      return status;
    }
    report.section = ArrayIoSection::kPayload;
    return output.Text("data\n");
  }
  const std::uint64_t header_bytes = 56 + std::uint64_t{8} * shape.size();
  if (header_bytes > limits.max_header_bytes || scratch.size() < 56) {
    return Status(ErrorCode::kAllocation);
  }
  const auto payload = internal_array_io::MultiplySize(
      count, internal_array_io::ScalarWidth(scalar));
  if (!payload.ok()) {
    return payload.status();
  }
  auto header = scratch.first(56);
  for (std::byte& byte : header) {
    byte = std::byte{0};
  }
  constexpr std::string_view kMagic = "ASCARRB\n";
  for (std::size_t i = 0; i < kMagic.size(); ++i) {
    header[i] = static_cast<std::byte>(kMagic[i]);
  }
  EncodeHeaderValue<std::uint16_t>(1, header, 8);
  EncodeHeaderValue<std::uint8_t>(1, header, 12);
  EncodeHeaderValue(static_cast<std::uint8_t>(scalar), header, 13);
  EncodeHeaderValue(static_cast<std::uint32_t>(shape.size()), header, 16);
  EncodeHeaderValue(count, header, 24);
  EncodeHeaderValue(*payload, header, 40);
  EncodeHeaderValue(header_bytes, header, 48);
  Status status = output.Write(header);
  if (!status.ok()) {
    return status;
  }
  for (extent_t extent : shape) {
    EncodeHeaderValue(static_cast<std::uint64_t>(extent), scratch, 0);
    status = output.Write(scratch.first(8));
    if (!status.ok()) {
      return status;
    }
  }
  return Status::Ok();
}

}  // namespace internal_dense_io
}  // namespace asc
