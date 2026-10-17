#include "asc/core/array_io.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>

#include "asc/core/array_format.h"
#include "asc/core/io.h"
#include "asc/core/result.h"
#include "asc/core/status.h"

namespace asc::internal_array_io {
namespace {

constexpr std::array<std::string_view, 12> kScalars{
    "i8",  "u8",  "i16", "u16", "i32", "u32",
    "i64", "u64", "f32", "f64", "c64", "c128"};
constexpr std::array<std::size_t, 12> kWidths{1, 1, 2, 2, 4, 4,
                                              8, 8, 4, 8, 8, 16};

class CountingSink final : public ByteSink {
 public:
  CountingSink(ByteSink& sink, ArrayIoReport& report, std::uint32_t* crc)
      : sink_(sink), report_(report), crc_(crc) {}
  Result<std::size_t> WriteSome(std::span<const std::byte> bytes) override {
    auto count = sink_.WriteSome(bytes);
    if (!count.ok()) {
      return Status(count.status().code(), {}, {},
                    count.status().native_code());
    }
    if (*count > bytes.size() || (!bytes.empty() && *count == 0)) {
      return Status(ErrorCode::kIo);
    }
    report_.output_bytes += *count;
    if (crc_ != nullptr) {
      *crc_ = UpdateCrc(*crc_, bytes.first(*count));
    }
    return *count;
  }

 private:
  ByteSink& sink_;
  ArrayIoReport& report_;
  std::uint32_t* crc_;
};

}  // namespace

Status ValidateLimits(const ArrayIoLimits& limits) {
  constexpr auto kMaximum =
      static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
  if (limits.max_rank > std::numeric_limits<std::uint32_t>::max() ||
      limits.max_extent > kMaximum || limits.max_logical_elements > kMaximum ||
      limits.max_stored_elements > kMaximum) {
    return Status(ErrorCode::kInvalidArgument);
  }
  return Status::Ok();
}

Result<ArrayScalarCode> ParseScalarCode(std::string_view name) {
  for (std::size_t i = 0; i < kScalars.size(); ++i) {
    if (name == kScalars[i]) {
      return static_cast<ArrayScalarCode>(i + 1);
    }
  }
  return Status(ErrorCode::kEncoding);
}

std::string_view ScalarName(ArrayScalarCode code) {
  const auto value = static_cast<std::size_t>(code);
  if (value == 0 || value > kScalars.size()) {
    return {};
  }
  return kScalars[value - 1];
}

std::size_t ScalarWidth(ArrayScalarCode code) {
  const auto value = static_cast<std::size_t>(code);
  if (value == 0 || value > kWidths.size()) {
    return 0;
  }
  return kWidths[value - 1];
}

bool FiniteRealSyntax(std::string_view token) {
  std::size_t position = 0;
  if (position < token.size() &&
      (token[position] == '+' || token[position] == '-')) {
    ++position;
  }
  std::size_t digits = 0;
  while (position < token.size() && token[position] >= '0' &&
         token[position] <= '9') {
    ++position;
    ++digits;
  }
  if (position < token.size() && token[position] == '.') {
    ++position;
    while (position < token.size() && token[position] >= '0' &&
           token[position] <= '9') {
      ++position;
      ++digits;
    }
  }
  if (digits == 0) {
    return false;
  }
  if (position < token.size() &&
      (token[position] == 'e' || token[position] == 'E')) {
    ++position;
    if (position < token.size() &&
        (token[position] == '+' || token[position] == '-')) {
      ++position;
    }
    const std::size_t first = position;
    while (position < token.size() && token[position] >= '0' &&
           token[position] <= '9') {
      ++position;
    }
    if (position == first) {
      return false;
    }
  }
  return position == token.size();
}

std::uint32_t UpdateCrc(std::uint32_t state, std::span<const std::byte> bytes) {
  for (std::byte byte : bytes) {
    state ^= std::to_integer<std::uint32_t>(byte);
    for (int bit = 0; bit < 8; ++bit) {
      state = (state >> 1) ^ ((state & 1U) == 0 ? 0U : 0xedb88320U);
    }
  }
  return state;
}

Status Input::Fail(ErrorCode code) {
  if (status_.ok()) {
    status_ = Status(code);
  }
  return status_;
}

Status Input::Read(std::span<std::byte> bytes, bool checksum) {
  if (!status_.ok()) {
    return status_;
  }
  if (report_->input_bytes > limits_.max_input_bytes ||
      bytes.size() > limits_.max_input_bytes - report_->input_bytes) {
    return Fail(ErrorCode::kAllocation);
  }
  if (report_->section == ArrayIoSection::kHeader &&
      (report_->input_bytes > limits_.max_header_bytes ||
       bytes.size() > limits_.max_header_bytes - report_->input_bytes)) {
    return Fail(ErrorCode::kAllocation);
  }
  while (!bytes.empty()) {
    auto count = source_->ReadSome(bytes);
    if (!count.ok()) {
      status_ =
          Status(count.status().code(), {}, {}, count.status().native_code());
      return status_;
    }
    if (*count > bytes.size()) {
      return Fail(ErrorCode::kIo);
    }
    if (*count == 0) {
      return Fail(ErrorCode::kEndOfFile);
    }
    report_->input_bytes += *count;
    if (checksum) {
      crc_ = UpdateCrc(crc_, bytes.first(*count));
    }
    bytes = bytes.subspan(*count);
  }
  return Status::Ok();
}

Status Input::Expect(std::string_view text) {
  for (char expected : text) {
    std::byte byte{};
    Status status = Read(std::span(&byte, 1));
    if (!status.ok()) {
      return status;
    }
    if (byte != static_cast<std::byte>(expected)) {
      return Fail(ErrorCode::kEncoding);
    }
  }
  return Status::Ok();
}

Status Input::EndOfLine() {
  std::byte byte{};
  Status status = Read(std::span(&byte, 1));
  if (!status.ok()) {
    return status;
  }
  if (byte == std::byte{'\r'}) {
    return Expect("\n");
  }
  return byte == std::byte{'\n'} ? Status::Ok() : Fail(ErrorCode::kEncoding);
}

Result<std::string_view> Input::Line(std::span<char> scratch, std::size_t cap) {
  std::size_t size = 0;
  for (;;) {
    std::byte byte{};
    Status status = Read(std::span(&byte, 1));
    if (!status.ok()) {
      return status;
    }
    if (byte == std::byte{'\r'}) {
      status = Expect("\n");
      if (!status.ok()) {
        return status;
      }
      return std::string_view(scratch.data(), size);
    }
    if (byte == std::byte{'\n'}) {
      return std::string_view(scratch.data(), size);
    }
    const auto character = std::to_integer<unsigned int>(byte);
    if (character < 32 || character > 126) {
      return Fail(ErrorCode::kEncoding);
    }
    if (size == scratch.size() || size == cap) {
      return Fail(ErrorCode::kAllocation);
    }
    scratch[size++] = static_cast<char>(character);
  }
}

Result<std::string_view> Input::Token(std::span<char> scratch, char separator) {
  std::size_t size = 0;
  for (;;) {
    std::byte byte{};
    Status status = Read(std::span(&byte, 1));
    if (!status.ok()) {
      return status;
    }
    if (byte == static_cast<std::byte>(separator)) {
      return std::string_view(scratch.data(), size);
    }
    if (separator == '\n' && byte == std::byte{'\r'}) {
      status = Expect("\n");
      if (!status.ok()) {
        return status;
      }
      return std::string_view(scratch.data(), size);
    }
    const auto character = std::to_integer<unsigned int>(byte);
    if (character < 33 || character > 126) {
      return Fail(ErrorCode::kEncoding);
    }
    if (size == scratch.size() || size == limits_.max_token_bytes) {
      return Fail(ErrorCode::kAllocation);
    }
    scratch[size++] = static_cast<char>(character);
  }
}

Status Input::EndOfFile(bool text_whitespace) {
  if (!status_.ok()) {
    return status_;
  }
  bool after_cr = false;
  for (;;) {
    // The unseekable-source EOF probe also needs a byte of budget. Reject
    // before requesting it so an over-limit byte is never silently consumed.
    if (report_->input_bytes == limits_.max_input_bytes) {
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
      return after_cr ? Fail(ErrorCode::kEncoding) : Status::Ok();
    }
    ++report_->input_bytes;
    if (!text_whitespace) {
      return Fail(ErrorCode::kEncoding);
    }
    if (after_cr && byte != std::byte{'\n'}) {
      return Fail(ErrorCode::kEncoding);
    }
    after_cr = byte == std::byte{'\r'};
    if (byte != std::byte{' '} && byte != std::byte{'\t'} &&
        byte != std::byte{'\n'} && byte != std::byte{'\r'}) {
      return Fail(ErrorCode::kEncoding);
    }
  }
}

Status Output::Write(std::span<const std::byte> bytes, bool checksum) {
  if (!status_.ok()) {
    return status_;
  }
  if (report_->output_bytes > limit_ ||
      bytes.size() > limit_ - report_->output_bytes) {
    status_ = Status(ErrorCode::kAllocation);
    return status_;
  }
  CountingSink counted(*sink_, *report_, checksum ? &crc_ : nullptr);
  status_ = WriteAll(counted, bytes);
  return status_;
}

Status Output::Text(std::string_view text) {
  return Write(std::as_bytes(std::span(text.data(), text.size())));
}

}  // namespace asc::internal_array_io
