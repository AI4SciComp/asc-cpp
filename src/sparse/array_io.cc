#include "asc/core/array_io.h"

#include <algorithm>
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
#include "asc/sparse/io.h"

namespace asc {
namespace {

std::string_view KindName(SparseArrayKind kind) {
  switch (kind) {
    case SparseArrayKind::kCoo:
      return "coo";
    case SparseArrayKind::kCsr:
      return "csr";
    case SparseArrayKind::kCsc:
      return "csc";
  }
  return {};
}

Status ValidateBuffers(std::span<extent_t> metadata,
                       std::span<std::byte> scratch,
                       const ArrayIoLimits& limits) {
  Status status = internal_array_io::ValidateLimits(limits);
  if (!status.ok()) {
    return status;
  }
  auto bytes =
      internal_array_io::MultiplySize(metadata.size(), sizeof(extent_t));
  if (!bytes.ok()) {
    return bytes.status();
  }
  if (*bytes > limits.max_scratch_bytes ||
      scratch.size() > limits.max_scratch_bytes - *bytes) {
    return Status(ErrorCode::kAllocation);
  }
  if (internal_array_format::Overlaps(metadata.data(), *bytes, scratch.data(),
                                      scratch.size())) {
    return Status(ErrorCode::kInvalidArgument);
  }
  return Status::Ok();
}

Result<std::uint64_t> Unsigned(internal_array_io::Input& input,
                               std::span<std::byte> scratch, char separator) {
  auto token = input.Token(
      std::span(reinterpret_cast<char*>(scratch.data()), scratch.size()),
      separator);
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
  auto value = internal_array_io::ParseScalar<std::uint64_t>(
      *token, input.limits().max_token_bytes);
  if (!value.ok()) {
    return input.Fail(value.status().code());
  }
  return *value;
}

template <typename T>
T Decode(std::span<const std::byte> buffer, std::size_t position) {
  // The enclosing fixed envelope/subspan size was checked before this call.
  return *DecodeLittleEndian<T>(buffer.subspan(position, sizeof(T)));
}

template <typename T>
void Encode(T value, std::span<std::byte> buffer, std::size_t position) {
  const auto status =
      EncodeLittleEndian(value, buffer.subspan(position, sizeof(T)));
  static_cast<void>(status);
}

Status HeaderText(std::string_view text, internal_array_io::Output& output,
                  const ArrayIoLimits& limits, ArrayIoReport& report) {
  if (report.output_bytes > limits.max_header_bytes ||
      text.size() > limits.max_header_bytes - report.output_bytes) {
    return Status(ErrorCode::kAllocation);
  }
  return output.Text(text);
}

Status HeaderInteger(std::uint64_t value, std::span<std::byte> scratch,
                     internal_array_io::Output& output,
                     const ArrayIoLimits& limits, ArrayIoReport& report) {
  internal_array_format::TextBuffer token(
      std::span(reinterpret_cast<char*>(scratch.data()), scratch.size()));
  if (!token.Integer(value) || token.size() > limits.max_token_bytes) {
    return Status(ErrorCode::kAllocation);
  }
  return HeaderText(token.text(), output, limits, report);
}

}  // namespace

SparseArrayReader::SparseArrayReader(SparseArrayReader&& other) noexcept
    : input_(std::move(other.input_)),
      shape_(other.shape_),
      scratch_(other.scratch_),
      kind_(other.kind_),
      scalar_(other.scalar_),
      count_(other.count_),
      binary_(other.binary_),
      require_eof_(other.require_eof_),
      ready_(std::exchange(other.ready_, false)),
      structure_read_(std::exchange(other.structure_read_, false)) {}

Result<SparseArrayReader> SparseArrayReader::PrepareText(
    ByteSource& source, std::span<extent_t> metadata,
    std::span<std::byte> scratch, const ArrayIoLimits& limits,
    ArrayIoReport& report, bool require_eof) {
  report = {};
  const auto status = ValidateBuffers(metadata, scratch, limits);
  if (!status.ok()) {
    return status;
  }
  SparseArrayReader reader(source, metadata, scratch, limits, report, false,
                           require_eof);
  const auto parsed = reader.ParseTextHeader();
  if (!parsed.ok()) {
    return parsed;
  }
  reader.ready_ = true;
  return reader;
}

Result<SparseArrayReader> SparseArrayReader::PrepareBinary(
    ByteSource& source, std::span<extent_t> metadata,
    std::span<std::byte> scratch, const ArrayIoLimits& limits,
    ArrayIoReport& report, bool require_eof) {
  report = {};
  const auto status = ValidateBuffers(metadata, scratch, limits);
  if (!status.ok()) {
    return status;
  }
  SparseArrayReader reader(source, metadata, scratch, limits, report, true,
                           require_eof);
  const auto parsed = reader.ParseBinaryHeader();
  if (!parsed.ok()) {
    return parsed;
  }
  reader.ready_ = true;
  return reader;
}

Status SparseArrayReader::Line(std::string_view text) {
  const auto status = input_.Expect(text);
  return status.ok() ? input_.EndOfLine() : status;
}

Result<index_t> SparseArrayReader::ReadIndex(char separator) {
  std::uint64_t value = 0;
  if (binary_) {
    if (scratch_.size() < 8) {
      return Fail(ErrorCode::kAllocation);
    }
    const auto status = input_.Read(scratch_.first(8));
    if (!status.ok()) {
      return status;
    }
    value = Decode<std::uint64_t>(scratch_, 0);
  } else {
    auto parsed = Unsigned(input_, scratch_, separator);
    if (!parsed.ok()) {
      return parsed.status();
    }
    value = *parsed;
  }
  if (value > static_cast<std::uint64_t>(std::numeric_limits<index_t>::max())) {
    return Fail(ErrorCode::kOverflow);
  }
  return static_cast<index_t>(value);
}

Status SparseArrayReader::ParseTextPrefix() {
  Status status = Line("ASCARRAY 1");
  if (!status.ok()) {
    return status;
  }
  status = input_.Expect("kind ");
  if (!status.ok()) {
    return status;
  }
  const auto text =
      std::span(reinterpret_cast<char*>(scratch_.data()), scratch_.size());
  auto kind = input_.Line(text, 5);
  if (!kind.ok()) {
    return kind.status();
  }
  if (*kind == "coo") {
    kind_ = SparseArrayKind::kCoo;
  } else if (*kind == "csr") {
    kind_ = SparseArrayKind::kCsr;
  } else if (*kind == "csc") {
    kind_ = SparseArrayKind::kCsc;
  } else {
    return Fail(ErrorCode::kEncoding);
  }
  status = input_.Expect("scalar ");
  if (!status.ok()) {
    return status;
  }
  auto name = input_.Line(text, 4);
  if (!name.ok()) {
    return name.status();
  }
  auto scalar = internal_array_io::ParseScalarCode(*name);
  if (!scalar.ok()) {
    return Fail(scalar.status().code());
  }
  scalar_ = *scalar;
  return Status::Ok();
}

Status SparseArrayReader::ParseTextHeader() {
  Status status = ParseTextPrefix();
  if (!status.ok()) {
    return status;
  }
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
  status = input_.Expect("order ");
  if (!status.ok()) {
    return status;
  }
  status = Line(KindName(kind_));
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
  status = internal_sparse_io::ValidateShape(kind_, shape_, *count, scalar_,
                                             input_.limits());
  if (!status.ok()) {
    return Fail(status.code());
  }
  count_ = static_cast<std::size_t>(*count);
  input_.report().section = ArrayIoSection::kPayload;
  return Status::Ok();
}

Status SparseArrayReader::ParseBinaryHeader() {
  if (scratch_.size() < 56) {
    return Fail(ErrorCode::kAllocation);
  }
  Status status = input_.Read(scratch_.first(56));
  if (!status.ok()) {
    return status;
  }
  const auto header = scratch_.first(56);
  if (std::string_view(reinterpret_cast<const char*>(header.data()), 8) !=
      "ASCARRB\n") {
    return Fail(ErrorCode::kEncoding);
  }
  if (Decode<std::uint16_t>(header, 8) != 1 ||
      Decode<std::uint16_t>(header, 10) != 0) {
    return Fail(ErrorCode::kVersion);
  }
  const auto kind = Decode<std::uint8_t>(header, 12);
  const auto scalar = Decode<std::uint8_t>(header, 13);
  if (kind < 2 || kind > 4 || scalar < 1 || scalar > 12 ||
      Decode<std::uint16_t>(header, 14) != 0 ||
      Decode<std::uint32_t>(header, 20) != 0) {
    return Fail(ErrorCode::kEncoding);
  }
  kind_ = static_cast<SparseArrayKind>(kind);
  scalar_ = static_cast<ArrayScalarCode>(scalar);
  const auto rank = Decode<std::uint32_t>(header, 16);
  const auto count = Decode<std::uint64_t>(header, 24);
  const auto structure_count = Decode<std::uint64_t>(header, 32);
  const auto payload_bytes = Decode<std::uint64_t>(header, 40);
  const std::uint64_t header_bytes = 56 + std::uint64_t{8} * rank;
  if (rank > input_.limits().max_rank || rank > shape_.size() ||
      header_bytes > input_.limits().max_header_bytes) {
    return Fail(ErrorCode::kAllocation);
  }
  if (Decode<std::uint64_t>(header, 48) != header_bytes) {
    return Fail(ErrorCode::kEncoding);
  }
  shape_ = shape_.first(rank);
  for (extent_t& destination : shape_) {
    status = input_.Read(scratch_.first(8));
    if (!status.ok()) {
      return status;
    }
    const auto extent = Decode<std::uint64_t>(scratch_, 0);
    if (extent > input_.limits().max_extent) {
      return Fail(ErrorCode::kShape);
    }
    destination = static_cast<extent_t>(extent);
  }
  return ValidateBinaryPayload(count, structure_count, payload_bytes,
                               header_bytes);
}

Status SparseArrayReader::ValidateBinaryPayload(std::uint64_t count,
                                                std::uint64_t structure_count,
                                                std::uint64_t payload_bytes,
                                                std::uint64_t header_bytes) {
  auto status = internal_sparse_io::ValidateShape(kind_, shape_, count, scalar_,
                                                  input_.limits());
  if (!status.ok()) {
    return Fail(status.code());
  }
  const auto expected =
      internal_sparse_io::StructureCount(kind_, shape_, count);
  if (!expected.ok()) {
    return Fail(expected.status().code());
  }
  if (structure_count != *expected) {
    return Fail(ErrorCode::kEncoding);
  }
  const auto scalar_bytes = count * internal_array_io::ScalarWidth(scalar_);
  const auto expected_bytes =
      static_cast<std::uint64_t>(*expected) * 8 + scalar_bytes;
  if (payload_bytes != expected_bytes) {
    return Fail(ErrorCode::kEncoding);
  }
  auto frame = internal_array_io::AddSize(header_bytes, payload_bytes);
  if (!frame.ok()) {
    return Fail(ErrorCode::kOverflow);
  }
  frame = internal_array_io::AddSize(*frame, std::uint64_t{4});
  if (!frame.ok()) {
    return Fail(ErrorCode::kOverflow);
  }
  if (*frame > input_.limits().max_input_bytes) {
    return Fail(ErrorCode::kAllocation);
  }
  count_ = static_cast<std::size_t>(count);
  input_.report().section = ArrayIoSection::kPayload;
  return Status::Ok();
}

Status SparseArrayReader::Trailer() {
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
    if (Decode<std::uint32_t>(scratch_, 0) != expected) {
      return Fail(ErrorCode::kEncoding);
    }
  } else {
    status = Line("end");
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

namespace internal_sparse_io {

Result<std::size_t> StructureCount(SparseArrayKind kind,
                                   std::span<const extent_t> shape,
                                   std::uint64_t count) {
  if (count > std::numeric_limits<std::size_t>::max()) {
    return Status(ErrorCode::kOverflow);
  }
  if (kind == SparseArrayKind::kCoo) {
    return internal_array_io::MultiplySize(static_cast<std::size_t>(count),
                                           shape.size());
  }
  if ((kind != SparseArrayKind::kCsr && kind != SparseArrayKind::kCsc) ||
      shape.size() != 2) {
    return Status(ErrorCode::kShape);
  }
  auto outer = internal_array_io::AddSize(
      shape[kind == SparseArrayKind::kCsr ? 0 : 1], extent_t{1});
  if (!outer.ok()) {
    return outer.status();
  }
  auto converted = internal_array_io::CastSize<std::size_t>(*outer);
  if (!converted.ok()) {
    return converted.status();
  }
  return internal_array_io::AddSize(*converted,
                                    static_cast<std::size_t>(count));
}

Status ValidateShape(SparseArrayKind kind, std::span<const extent_t> shape,
                     std::uint64_t count, ArrayScalarCode scalar,
                     const ArrayIoLimits& limits) {
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
      auto product = internal_array_io::MultiplySize(
          logical, static_cast<std::uint64_t>(extent));
      if (!product.ok()) {
        return Status(ErrorCode::kOverflow);
      }
      logical = *product;
    }
  }
  if (logical > limits.max_logical_elements || count > logical ||
      count > limits.max_stored_elements) {
    return Status(ErrorCode::kShape);
  }
  if (logical >
          static_cast<std::uint64_t>(std::numeric_limits<extent_t>::max()) ||
      count > static_cast<std::uint64_t>(std::numeric_limits<nnz_t>::max())) {
    return Status(ErrorCode::kOverflow);
  }
  const auto width = internal_array_io::ScalarWidth(scalar);
  if (width == 0) {
    return Status(ErrorCode::kEncoding);
  }
  auto structure = StructureCount(kind, shape, count);
  if (!structure.ok()) {
    return structure.status();
  }
  auto structure_bytes =
      internal_array_io::MultiplySize(*structure, sizeof(index_t));
  if (!structure_bytes.ok() ||
      count > std::numeric_limits<std::size_t>::max()) {
    return Status(ErrorCode::kOverflow);
  }
  auto value_bytes =
      internal_array_io::MultiplySize(static_cast<std::size_t>(count), width);
  if (!value_bytes.ok()) {
    return Status(ErrorCode::kOverflow);
  }
  auto decoded = internal_array_io::AddSize(*structure_bytes, *value_bytes);
  if (!decoded.ok()) {
    return Status(ErrorCode::kOverflow);
  }
  if (*structure_bytes > limits.max_structure_bytes ||
      *decoded > limits.max_decoded_bytes) {
    return Status(ErrorCode::kAllocation);
  }
  return Status::Ok();
}

Status ValidateStructure(SparseArrayKind kind, std::span<const extent_t> shape,
                         std::size_t count,
                         std::span<const index_t> structure) {
  auto expected = StructureCount(kind, shape, count);
  if (!expected.ok()) {
    return expected.status();
  }
  if (structure.size() != *expected) {
    return Status(ErrorCode::kShape);
  }
  if (kind == SparseArrayKind::kCoo) {
    for (std::size_t entry = 0; entry < count; ++entry) {
      bool greater = false;
      bool differs = false;
      for (std::size_t dimension = 0; dimension < shape.size(); ++dimension) {
        const auto value = structure[entry * shape.size() + dimension];
        if (value < 0 || value >= shape[dimension]) {
          return Status(ErrorCode::kIndex);
        }
        if (entry > 0 && !differs) {
          const auto previous =
              structure[(entry - 1) * shape.size() + dimension];
          differs = value != previous;
          greater = value > previous;
        }
      }
      if (entry > 0 && !greater) {
        return Status(ErrorCode::kEncoding);
      }
    }
    return Status::Ok();
  }
  const auto outer =
      static_cast<std::size_t>(shape[kind == SparseArrayKind::kCsr ? 0 : 1]);
  const auto inner = shape[kind == SparseArrayKind::kCsr ? 1 : 0];
  if (structure[0] != 0 || structure[outer] != static_cast<index_t>(count)) {
    return Status(ErrorCode::kEncoding);
  }
  for (std::size_t segment = 0; segment < outer; ++segment) {
    const auto begin = structure[segment];
    const auto end = structure[segment + 1];
    if (begin < 0 || end < begin || end > static_cast<index_t>(count)) {
      return Status(ErrorCode::kIndex);
    }
    index_t previous = -1;
    for (index_t entry = begin; entry < end; ++entry) {
      const auto value = structure[outer + 1 + static_cast<std::size_t>(entry)];
      if (value < 0 || value >= inner) {
        return Status(ErrorCode::kIndex);
      }
      if (value <= previous) {
        return Status(ErrorCode::kEncoding);
      }
      previous = value;
    }
  }
  return Status::Ok();
}

Status WriteIndex(std::uint64_t value, bool binary, std::string_view separator,
                  const ArrayIoLimits& limits, std::span<std::byte> scratch,
                  internal_array_io::Output& output) {
  if (binary) {
    if (scratch.size() < 8) {
      return Status(ErrorCode::kAllocation);
    }
    Encode(value, scratch, 0);
    return output.Write(scratch.first(8));
  }
  internal_array_format::TextBuffer token(
      std::span(reinterpret_cast<char*>(scratch.data()), scratch.size()));
  if (!token.Integer(value) || token.size() > limits.max_token_bytes) {
    return Status(ErrorCode::kAllocation);
  }
  const auto status = output.Text(token.text());
  return status.ok() ? output.Text(separator) : status;
}

namespace {

Status WriteTextHeader(SparseArrayKind kind, std::span<const extent_t> shape,
                       std::size_t count, ArrayScalarCode scalar,
                       const ArrayIoLimits& limits,
                       std::span<std::byte> scratch,
                       internal_array_io::Output& output,
                       ArrayIoReport& report) {
  Status status = HeaderText("ASCARRAY 1\nkind ", output, limits, report);
  if (!status.ok()) {
    return status;
  }
  status = HeaderText(KindName(kind), output, limits, report);
  if (!status.ok()) {
    return status;
  }
  status = HeaderText("\nscalar ", output, limits, report);
  if (!status.ok()) {
    return status;
  }
  status =
      HeaderText(internal_array_io::ScalarName(scalar), output, limits, report);
  if (!status.ok()) {
    return status;
  }
  status = HeaderText("\nrank ", output, limits, report);
  if (!status.ok()) {
    return status;
  }
  status = HeaderInteger(shape.size(), scratch, output, limits, report);
  if (!status.ok()) {
    return status;
  }
  status = HeaderText("\nshape", output, limits, report);
  if (!status.ok()) {
    return status;
  }
  for (extent_t extent : shape) {
    status = HeaderText(" ", output, limits, report);
    if (!status.ok()) {
      return status;
    }
    status = HeaderInteger(static_cast<std::uint64_t>(extent), scratch, output,
                           limits, report);
    if (!status.ok()) {
      return status;
    }
  }
  status = HeaderText("\norder ", output, limits, report);
  if (!status.ok()) {
    return status;
  }
  status = HeaderText(KindName(kind), output, limits, report);
  if (!status.ok()) {
    return status;
  }
  status = HeaderText("\ncount ", output, limits, report);
  if (!status.ok()) {
    return status;
  }
  status = HeaderInteger(count, scratch, output, limits, report);
  if (!status.ok()) {
    return status;
  }
  return HeaderText("\n", output, limits, report);
}

}  // namespace

Status WriteHeader(SparseArrayKind kind, std::span<const extent_t> shape,
                   std::size_t count, ArrayScalarCode scalar, bool binary,
                   const ArrayIoLimits& limits, std::span<std::byte> scratch,
                   internal_array_io::Output& output, ArrayIoReport& report) {
  if (!binary) {
    return WriteTextHeader(kind, shape, count, scalar, limits, scratch, output,
                           report);
  }
  const std::uint64_t header_bytes = 56 + std::uint64_t{8} * shape.size();
  if (header_bytes > limits.max_header_bytes || scratch.size() < 56) {
    return Status(ErrorCode::kAllocation);
  }
  const auto structure = StructureCount(kind, shape, count);
  if (!structure.ok()) {
    return structure.status();
  }
  auto header = scratch.first(56);
  std::fill(header.begin(), header.end(), std::byte{0});
  constexpr std::string_view kMagic = "ASCARRB\n";
  for (std::size_t i = 0; i < kMagic.size(); ++i) {
    header[i] = static_cast<std::byte>(kMagic[i]);
  }
  Encode<std::uint16_t>(1, header, 8);
  Encode(static_cast<std::uint8_t>(kind), header, 12);
  Encode(static_cast<std::uint8_t>(scalar), header, 13);
  Encode(static_cast<std::uint32_t>(shape.size()), header, 16);
  Encode(static_cast<std::uint64_t>(count), header, 24);
  Encode(static_cast<std::uint64_t>(*structure), header, 32);
  Encode(static_cast<std::uint64_t>(*structure) * 8 +
             count * internal_array_io::ScalarWidth(scalar),
         header, 40);
  Encode(header_bytes, header, 48);
  Status status = output.Write(header);
  if (!status.ok()) {
    return status;
  }
  for (extent_t extent : shape) {
    Encode(static_cast<std::uint64_t>(extent), scratch, 0);
    status = output.Write(scratch.first(8));
    if (!status.ok()) {
      return status;
    }
  }
  return Status::Ok();
}

}  // namespace internal_sparse_io
}  // namespace asc
