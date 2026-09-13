#include "asc/core/matrix_market.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "asc/core/array_io.h"
#include "asc/core/io.h"
#include "asc/core/result.h"
#include "asc/core/status.h"

namespace asc::internal_matrix_market {

Status Input::Fail(ErrorCode code) {
  if (status_.ok()) {
    status_ = Status(code);
  }
  return status_;
}

Result<int> Input::ReadByte() {
  if (!status_.ok()) {
    return status_;
  }
  if (eof_) {
    return -1;
  }
  // A nonseekable source needs one available budget byte even for its final
  // zero-byte EOF probe; a consumed extra byte cannot be put back safely.
  if (report_->input_bytes >= limits_.max_input_bytes) {
    return Fail(ErrorCode::kAllocation);
  }
  std::byte byte{};
  auto count = source_->ReadSome(std::span(&byte, 1));
  if (!count.ok()) {
    status_ =
        Status(count.status().code(), {}, {}, count.status().native_code());
    return status_;
  }
  if (*count > 1) {
    return Fail(ErrorCode::kIo);
  }
  if (*count == 0) {
    eof_ = true;
    return -1;
  }
  ++report_->input_bytes;
  const auto value = std::to_integer<unsigned char>(byte);
  if (value == 0 || value > 127 ||
      (value < 32 && value != '\t' && value != '\r' && value != '\n')) {
    return Fail(ErrorCode::kEncoding);
  }
  return static_cast<int>(value);
}

Result<int> Input::ReadLineByte(std::size_t& line_bytes) {
  auto byte = ReadByte();
  if (!byte.ok()) {
    return byte.status();
  }
  if (*byte != -1 && ++line_bytes > limits_.max_header_bytes) {
    return Fail(ErrorCode::kAllocation);
  }
  if (*byte == '\r') {
    byte = ReadByte();
    if (!byte.ok()) {
      return byte.status();
    }
    if (*byte != -1 && ++line_bytes > limits_.max_header_bytes) {
      return Fail(ErrorCode::kAllocation);
    }
    if (*byte != '\n') {
      return Fail(ErrorCode::kEncoding);
    }
  }
  return byte;
}

Status Input::SkipComment(std::size_t& line_bytes) {
  // A second banner is not a trailing comment in the frozen single-object
  // profile. Track only its bounded first word; long comments need no buffer.
  constexpr std::string_view kBanner = "%%MatrixMarket";
  std::size_t prefix = 1;  // The first percent byte was already consumed.
  bool matching = true;
  bool first_word = true;
  for (;;) {
    const auto byte = ReadLineByte(line_bytes);
    if (!byte.ok()) {
      return byte.status();
    }
    const bool end = *byte == -1 || *byte == '\n';
    if (end || *byte == ' ' || *byte == '\t') {
      if (first_word && matching && prefix == kBanner.size()) {
        return Fail(ErrorCode::kEncoding);
      }
      first_word = false;
      if (end) {
        return Status::Ok();
      }
    } else if (first_word && matching) {
      if (prefix == kBanner.size() || *byte != kBanner[prefix]) {
        matching = false;
      } else {
        ++prefix;
      }
    }
  }
}

Result<Record> Input::ReadRecord(std::span<std::byte> scratch, bool comments) {
  Record record;
  std::size_t used = 0;
  std::size_t start = 0;
  std::size_t line_bytes = 0;
  bool active = false;
  for (;;) {
    const auto byte = ReadLineByte(line_bytes);
    if (!byte.ok()) {
      return byte.status();
    }
    const bool end = *byte == -1 || *byte == '\n';
    if (end || *byte == ' ' || *byte == '\t') {
      if (active) {
        record.fields[record.count++] = std::string_view(
            reinterpret_cast<const char*>(scratch.data() + start),
            used - start);
        active = false;
      }
      if (end) {
        record.end_of_file = eof_ && record.count == 0;
        return record;
      }
      continue;
    }
    if (comments && record.count == 0 && !active && *byte == '%') {
      const auto status = SkipComment(line_bytes);
      if (!status.ok()) {
        return status;
      }
      record.end_of_file = eof_;
      return record;
    }
    if (!active) {
      if (record.count == record.fields.size()) {
        return Fail(ErrorCode::kEncoding);
      }
      active = true;
      start = used;
    }
    if (used == scratch.size() ||
        (comments && used - start >= limits_.max_token_bytes)) {
      return Fail(ErrorCode::kAllocation);
    }
    scratch[used++] = static_cast<std::byte>(*byte);
  }
}

Result<Record> Input::FirstRecord(std::span<std::byte> scratch) {
  return ReadRecord(scratch, false);
}

Result<Record> Input::NextRecord(std::span<std::byte> scratch) {
  for (;;) {
    auto record = ReadRecord(scratch, true);
    if (!record.ok() || record->count != 0 || record->end_of_file) {
      return record;
    }
  }
}

Status Input::Finish(std::span<std::byte> scratch) {
  auto record = NextRecord(scratch);
  if (!record.ok()) {
    return record.status();
  }
  return record->end_of_file ? Status::Ok() : Fail(ErrorCode::kEncoding);
}

bool WordEquals(std::string_view token, std::string_view lowercase) {
  if (token.size() != lowercase.size()) {
    return false;
  }
  for (std::size_t i = 0; i < token.size(); ++i) {
    char value = token[i];
    if (value >= 'A' && value <= 'Z') {
      value = static_cast<char>(value - 'A' + 'a');
    }
    if (value != lowercase[i]) {
      return false;
    }
  }
  return true;
}

Result<MatrixMarketField> ParseField(std::string_view token) {
  constexpr std::array<MatrixMarketField, 4> kFields{
      MatrixMarketField::kInteger, MatrixMarketField::kReal,
      MatrixMarketField::kComplex, MatrixMarketField::kPattern};
  for (const auto field : kFields) {
    if (WordEquals(token, FieldName(field))) {
      return field;
    }
  }
  return Status(ErrorCode::kEncoding);
}

Result<MatrixMarketSymmetry> ParseSymmetry(std::string_view token) {
  constexpr std::array<MatrixMarketSymmetry, 4> kSymmetries{
      MatrixMarketSymmetry::kGeneral, MatrixMarketSymmetry::kSymmetric,
      MatrixMarketSymmetry::kSkewSymmetric, MatrixMarketSymmetry::kHermitian};
  for (const auto symmetry : kSymmetries) {
    if (WordEquals(token, SymmetryName(symmetry))) {
      return symmetry;
    }
  }
  return Status(ErrorCode::kEncoding);
}

std::string_view FieldName(MatrixMarketField field) {
  switch (field) {
    case MatrixMarketField::kInteger:
      return "integer";
    case MatrixMarketField::kReal:
      return "real";
    case MatrixMarketField::kComplex:
      return "complex";
    case MatrixMarketField::kPattern:
      return "pattern";
  }
  return {};
}

std::string_view SymmetryName(MatrixMarketSymmetry symmetry) {
  switch (symmetry) {
    case MatrixMarketSymmetry::kGeneral:
      return "general";
    case MatrixMarketSymmetry::kSymmetric:
      return "symmetric";
    case MatrixMarketSymmetry::kSkewSymmetric:
      return "skew-symmetric";
    case MatrixMarketSymmetry::kHermitian:
      return "hermitian";
  }
  return {};
}

Result<std::uint64_t> ParseUnsigned(std::string_view token,
                                    std::size_t max_token) {
  if (token.empty() || token.front() < '0' || token.front() > '9') {
    return Status(ErrorCode::kEncoding);
  }
  return internal_array_io::ParseScalar<std::uint64_t>(token, max_token);
}

Status Text(internal_array_io::Output& output, std::string_view text) {
  return output.Write(std::as_bytes(std::span(text.data(), text.size())),
                      false);
}

}  // namespace asc::internal_matrix_market
