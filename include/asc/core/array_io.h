#ifndef ASC_CORE_ARRAY_IO_H_
#define ASC_CORE_ARRAY_IO_H_

/**
 * @file
 * @brief Storage-neutral bounded byte/scalar codecs and I/O resource
 * accounting.
 *
 * Core does not interpret Dense/Sparse shapes or archive schemas. The owning
 * module uses these helpers with explicit source, sink and caller resources.
 * New array stream boundaries retain source/sink ErrorCode and native_code
 * but discard owning message/provider strings instead of copying unbounded
 * diagnostics. Section and actual-byte progress remain in ArrayIoReport.
 * This policy does not change Core File, WriteAll or resource-owner errors.
 * @ingroup asc_core
 */

#include <array>
#include <charconv>
#include <cmath>
#include <complex>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>

#include "asc/core/array_format.h"
#include "asc/core/export.h"
#include "asc/core/io.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"

namespace asc {

/**
 * @brief Explicit bounds for synchronous native array I/O; zero means zero.
 *
 * Owners interpret element/shape budgets. Byte counts include every relevant
 * temporary, metadata and payload allocation. Limits are copied by prepared
 * readers. Raising a limit never disables checked arithmetic or placement
 * validation. Fields use byte units unless documented as counts.
 * @ingroup asc_core
 */
struct ArrayIoLimits {
  std::size_t max_input_bytes = 67108864;   ///< Consumed bytes per frame/file.
  std::size_t max_output_bytes = 67108864;  ///< Accepted bytes per saved frame.
  std::size_t max_header_bytes = 65536;     ///< Complete envelope/header bytes.
  std::size_t max_token_bytes =
      256;                    ///< Whole scalar token, punctuation included.
  std::size_t max_rank = 32;  ///< Maximum rank before metadata allocation.
  std::uint64_t max_extent =
      2147483647;  ///< Each extent, including empty shapes.
  std::uint64_t max_logical_elements =
      16777216;  ///< Checked logical element count.
  std::uint64_t max_stored_elements = 16777216;  ///< Stored Sparse value count.
  std::size_t max_structure_bytes = 67108864;  ///< Decoded index/offset bytes.
  std::size_t max_decoded_bytes = 67108864;    ///< Structure plus typed values.
  std::size_t max_staging_bytes = 67108864;    ///< Simultaneously live staging.
  std::size_t max_allocation_bytes =
      134217728;                          ///< Cumulative requested bytes.
  std::size_t max_allocations = 16;       ///< Successful allocation requests.
  std::size_t max_scratch_bytes = 65536;  ///< Caller parser/metadata scratch.
};

/** @brief Current bounded-I/O envelope phase; diagnostics contain no input
 * dump.
 * @ingroup asc_core
 */
enum class ArrayIoSection : std::uint8_t {
  kHeader,    ///< Validating/encoding metadata before numerical payload.
  kPayload,   ///< Reading/staging or writing logical/stored values.
  kTrailer,   ///< Validating terminator, checksum or required file EOF.
  kComplete,  ///< Entire selected object boundary was successfully processed.
};

/**
 * @brief Explicit destructive intent for array path-save conveniences.
 *
 * No default is provided by save APIs. Core File opens with create/truncate;
 * this policy does not promise exclusive creation, atomic replacement or
 * durable filesystem synchronization.
 * @ingroup asc_core
 */
enum class ArrayFileOverwrite : std::uint8_t {
  kTruncate,  ///< Create the file or truncate the existing path before writing.
};

/**
 * @brief Synchronous stream progress retained through validation/I/O failure.
 *
 * Owners initialize this report before work. A consumed source and a written
 * sink cannot generally roll back. `committed` concerns a destination owner or
 * staged view only; writes do not claim sink atomicity. Caller keeps this
 * object alive and exclusively accessible until a prepared reader is finished.
 * @ingroup asc_core
 */
struct ArrayIoReport {
  std::size_t input_bytes =
      0;  ///< Actual bytes consumed, including short reads.
  std::size_t output_bytes =
      0;  ///< Actual bytes accepted, including short writes.
  std::size_t values_processed =
      0;                   ///< Complete staged or emitted scalar values.
  bool committed = false;  ///< A complete read result was published/copied.
  ArrayIoSection section =
      ArrayIoSection::kHeader;  ///< Phase reached before failure.
  ErrorCode cleanup_error =
      ErrorCode::kOk;  ///< Secondary close failure, preserving a primary error.
};

namespace internal_array_io {

// Array parsers must reject hostile size arithmetic without first constructing
// the owning diagnostics of the existing public Core Checked* operations.
// These unsupported helpers accept only nonnegative size-domain operands.
template <CheckedInteger T>
Result<T> AddSize(T left, T right) {
  if constexpr (std::is_signed_v<T>) {
    if (left < 0 || right < 0) {
      return Status(ErrorCode::kOverflow);
    }
  }
  if (left > std::numeric_limits<T>::max() - right) {
    return Status(ErrorCode::kOverflow);
  }
  return static_cast<T>(left + right);
}

template <CheckedInteger T>
Result<T> MultiplySize(T left, T right) {
  if constexpr (std::is_signed_v<T>) {
    if (left < 0 || right < 0) {
      return Status(ErrorCode::kOverflow);
    }
  }
  if (right != 0 && left > std::numeric_limits<T>::max() / right) {
    return Status(ErrorCode::kOverflow);
  }
  return static_cast<T>(left * right);
}

template <CheckedInteger To, CheckedInteger From>
Result<To> CastSize(From value) {
  if constexpr (std::is_signed_v<From>) {
    if (value < 0) {
      return Status(ErrorCode::kOverflow);
    }
  }
  if (!std::in_range<To>(value)) {
    return Status(ErrorCode::kOverflow);
  }
  return static_cast<To>(value);
}

ASC_CORE_EXPORT Status ValidateLimits(const ArrayIoLimits& limits);
ASC_CORE_EXPORT Result<ArrayScalarCode> ParseScalarCode(std::string_view name);
ASC_CORE_EXPORT std::string_view ScalarName(ArrayScalarCode code);
ASC_CORE_EXPORT std::size_t ScalarWidth(ArrayScalarCode code);
ASC_CORE_EXPORT bool FiniteRealSyntax(std::string_view token);
ASC_CORE_EXPORT std::uint32_t UpdateCrc(std::uint32_t state,
                                        std::span<const std::byte> bytes);

template <typename T>
ArrayScalarCode ScalarCode() {
  return *ParseScalarCode(internal_array_format::ScalarName<T>());
}

template <typename T>
Result<T> ParseFinite(std::string_view token) {
  if (token.front() == '+') {
    token.remove_prefix(1);
  }
  // Character integer aliases are missing from some from_chars overload sets.
  // A checked full-width integer avoids any real-type intermediate/narrowing.
  using Parsed = std::conditional_t<
      std::integral<T>,
      std::conditional_t<std::is_signed_v<T>, std::int64_t, std::uint64_t>, T>;
  Parsed value{};
  const auto result =
      std::from_chars(token.data(), token.data() + token.size(), value);
  if (result.ec == std::errc::result_out_of_range) {
    return Status(ErrorCode::kOverflow);
  }
  if (result.ec != std::errc{} || result.ptr != token.data() + token.size()) {
    return Status(ErrorCode::kEncoding);
  }
  if constexpr (std::floating_point<T>) {
    if (!std::isfinite(value)) {
      return Status(ErrorCode::kOverflow);
    }
  } else if (value < static_cast<Parsed>(std::numeric_limits<T>::min()) ||
             value > static_cast<Parsed>(std::numeric_limits<T>::max())) {
    return Status(ErrorCode::kOverflow);
  }
  return static_cast<T>(value);
}

template <typename T>
Result<T> ParseScalar(std::string_view token, std::size_t max_token_bytes) {
  if constexpr (!internal_array_format::kWireScalar<T>) {
    return Status(ErrorCode::kUnsupported);
  } else {
    if (token.empty() || token.size() > max_token_bytes) {
      return Status(ErrorCode::kEncoding);
    }
    if constexpr (internal_array_format::kComplex<T>) {
      if (token.front() != '(' || token.back() != ')') {
        return Status(ErrorCode::kEncoding);
      }
      token.remove_prefix(1);
      token.remove_suffix(1);
      const auto comma = token.find(',');
      if (comma == std::string_view::npos ||
          token.find(',', comma + 1) != std::string_view::npos) {
        return Status(ErrorCode::kEncoding);
      }
      using Real = typename T::value_type;
      auto real = ParseScalar<Real>(token.substr(0, comma), max_token_bytes);
      if (!real.ok()) {
        return real.status();
      }
      auto imag = ParseScalar<Real>(token.substr(comma + 1), max_token_bytes);
      if (!imag.ok()) {
        return imag.status();
      }
      return T(*real, *imag);
    } else {
      if constexpr (std::floating_point<T>) {
        if (token == "inf") {
          return std::numeric_limits<T>::infinity();
        }
        if (token == "-inf") {
          return -std::numeric_limits<T>::infinity();
        }
        if (token == "nan") {
          return std::numeric_limits<T>::quiet_NaN();
        }
        if (!FiniteRealSyntax(token)) {
          return Status(ErrorCode::kEncoding);
        }
      } else {
        if constexpr (std::unsigned_integral<T>) {
          if (token.front() == '-') {
            return Status(ErrorCode::kEncoding);
          }
        }
        const std::size_t start =
            token.front() == '-' || token.front() == '+' ? 1 : 0;
        if (start == token.size()) {
          return Status(ErrorCode::kEncoding);
        }
        for (std::size_t i = start; i < token.size(); ++i) {
          if (token[i] < '0' || token[i] > '9') {
            return Status(ErrorCode::kEncoding);
          }
        }
      }
      return ParseFinite<T>(token);
    }
  }
}

template <typename T>
Status EncodeScalar(T value, std::span<std::byte> output) {
  if constexpr (!internal_array_format::kWireScalar<T>) {
    return Status(ErrorCode::kUnsupported);
  } else if constexpr (internal_array_format::kComplex<T>) {
    using Real = typename T::value_type;
    if (output.size() != 2 * sizeof(Real)) {
      return Status(ErrorCode::kEncoding);
    }
    Status status =
        EncodeLittleEndian(value.real(), output.first(sizeof(Real)));
    if (!status.ok()) {
      return status;
    }
    return EncodeLittleEndian(value.imag(), output.subspan(sizeof(Real)));
  } else {
    if (output.size() != sizeof(T)) {
      return Status(ErrorCode::kEncoding);
    }
    return EncodeLittleEndian(value, output);
  }
}

template <typename T>
Result<T> DecodeScalar(std::span<const std::byte> input) {
  if constexpr (!internal_array_format::kWireScalar<T>) {
    return Status(ErrorCode::kUnsupported);
  } else if constexpr (internal_array_format::kComplex<T>) {
    using Real = typename T::value_type;
    if (input.size() != 2 * sizeof(Real)) {
      return Status(ErrorCode::kEncoding);
    }
    auto real = DecodeLittleEndian<Real>(input.first(sizeof(Real)));
    if (!real.ok()) {
      return real.status();
    }
    auto imag = DecodeLittleEndian<Real>(input.subspan(sizeof(Real)));
    if (!imag.ok()) {
      return imag.status();
    }
    return T(*real, *imag);
  } else {
    if (input.size() != sizeof(T)) {
      return Status(ErrorCode::kEncoding);
    }
    return DecodeLittleEndian<T>(input);
  }
}

// No over-read: even a token boundary requests only its next byte. A failed
// source stays failed. Caller owns source, scratch and report lifetimes.
class Input {
 public:
  Input(ByteSource& source, ArrayIoLimits limits, ArrayIoReport& report)
      : source_(&source), limits_(limits), report_(&report) {}
  ASC_CORE_EXPORT Status Read(std::span<std::byte> bytes, bool checksum = true);
  ASC_CORE_EXPORT Result<std::string_view> Line(std::span<char> scratch,
                                                std::size_t cap);
  ASC_CORE_EXPORT Status Expect(std::string_view text);
  ASC_CORE_EXPORT Status EndOfLine();
  ASC_CORE_EXPORT Result<std::string_view> Token(std::span<char> scratch,
                                                 char separator);
  ASC_CORE_EXPORT Status EndOfFile(bool text_whitespace);
  ASC_CORE_EXPORT Status Fail(ErrorCode code);
  [[nodiscard]] const Status& status() const { return status_; }
  [[nodiscard]] const ArrayIoLimits& limits() const { return limits_; }
  [[nodiscard]] std::uint32_t checksum() const { return crc_ ^ 0xffffffffU; }
  ArrayIoReport& report() { return *report_; }

 private:
  ByteSource* source_;
  ArrayIoLimits limits_;
  ArrayIoReport* report_;
  std::uint32_t crc_ = 0xffffffffU;
  Status status_;
};

class Output {
 public:
  Output(ByteSink& sink, std::size_t limit, ArrayIoReport& report)
      : sink_(&sink), limit_(limit), report_(&report) {}
  ASC_CORE_EXPORT Status Write(std::span<const std::byte> bytes,
                               bool checksum = true);
  ASC_CORE_EXPORT Status Text(std::string_view text);
  [[nodiscard]] std::uint32_t checksum() const { return crc_ ^ 0xffffffffU; }
  [[nodiscard]] const Status& status() const { return status_; }

 private:
  ByteSink* sink_;
  std::size_t limit_;
  ArrayIoReport* report_;
  std::uint32_t crc_ = 0xffffffffU;
  Status status_;
};

}  // namespace internal_array_io
}  // namespace asc

#endif  // ASC_CORE_ARRAY_IO_H_
