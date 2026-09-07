#ifndef ASC_CORE_ARRAY_FORMAT_H_
#define ASC_CORE_ARRAY_FORMAT_H_

/**
 * @file
 * @brief Storage-neutral scalar spelling and bounded display policies.
 *
 * Core defines tokens and budgets only. Dense and Sparse separately own
 * shape, traversal, schema and storage interpretation. No provider is needed.
 * New array printers retain a failing ByteSink's ErrorCode and native_code,
 * but discard its owning message/provider strings to avoid diagnostic-copy
 * allocation. Reported accepted-byte progress remains available. Caller sink
 * implementation behavior is outside the printer's allocation guarantee.
 * @ingroup asc_core
 */

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

#include "asc/core/export.h"
#include "asc/core/io.h"
#include "asc/core/result.h"
#include "asc/core/status.h"

namespace asc {

/** @brief Portable scalar identity; values are the ASC binary-v1 wire codes.
 * @ingroup asc_core
 */
enum class ArrayScalarCode : std::uint8_t {
  kI8 = 1,     ///< Signed eight-bit integer.
  kU8 = 2,     ///< Unsigned eight-bit integer.
  kI16 = 3,    ///< Signed sixteen-bit integer.
  kU16 = 4,    ///< Unsigned sixteen-bit integer.
  kI32 = 5,    ///< Signed thirty-two-bit integer.
  kU32 = 6,    ///< Unsigned thirty-two-bit integer.
  kI64 = 7,    ///< Signed sixty-four-bit integer.
  kU64 = 8,    ///< Unsigned sixty-four-bit integer.
  kF32 = 9,    ///< IEEE binary32 real.
  kF64 = 10,   ///< IEEE binary64 real.
  kC64 = 11,   ///< Complex pair of binary32 components.
  kC128 = 12,  ///< Complex pair of binary64 components.
};

/** @brief Locale-independent display notation for floating components.
 * @ingroup asc_core
 */
enum class ArrayFloatFormat : std::uint8_t {
  kGeneral,     ///< Precision counts significant decimal digits.
  kFixed,       ///< Precision counts digits following the decimal point.
  kScientific,  ///< Fractional precision with an explicit decimal exponent.
};

/**
 * @brief Storage-independent bounded preview policy; zero limits mean zero.
 *
 * Owners interpret row/column/slice selection. Limits never request hidden
 * allocation. Precision must be 1 through 32. Read concurrently only while
 * unmodified; CPU value printers borrow this object for the synchronous call.
 * @ingroup asc_core
 */
struct ArrayPrintOptions {
  std::size_t max_elements = 64;  ///< Maximum fully displayed values.
  std::size_t max_rows = 8;       ///< Maximum selected displayed rows.
  std::size_t max_columns = 8;    ///< Maximum selected displayed columns.
  std::size_t max_slices = 4;     ///< Maximum selected two-dimensional slices.
  std::size_t max_output_bytes =
      16384;          ///< Maximum bytes accepted by the sink.
  int precision = 6;  ///< Decimal precision, in [1,32].
  ArrayFloatFormat float_format = ArrayFloatFormat::kGeneral;  ///< Notation.
  bool edge_preview = true;   ///< Select beginning/end instead of prefix only.
  bool show_metadata = true;  ///< Include owner-specific type/shape summary.
};

/**
 * @brief Synchronous display progress, retained even when the sink fails.
 *
 * Initialized before validation. Counts include only accepted bytes and
 * complete scalar tokens. No output ownership or sink rollback is implied.
 * A successful truncated preview visibly marks omitted values. The caller
 * must not access this report concurrently with the operation.
 * @ingroup asc_core
 */
struct ArrayPrintReport {
  std::size_t values_displayed = 0;  ///< Complete scalar tokens accepted.
  std::size_t output_bytes = 0;      ///< Bytes actually accepted by the sink.
  bool truncated = false;            ///< At least one value was omitted.
};

namespace internal_array_format {

inline constexpr std::string_view kTruncation = "... (truncated)\n";

template <typename T>
inline constexpr bool kComplex = std::same_as<T, std::complex<float>> ||
                                 std::same_as<T, std::complex<double>>;

template <typename T>
inline constexpr bool kWireScalar =
    (std::integral<T> && !std::same_as<T, bool> &&
     (sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8) &&
     std::numeric_limits<T>::digits ==
         static_cast<int>(sizeof(T) * 8) - (std::is_signed_v<T> ? 1 : 0)) ||
    (std::same_as<T, float> && sizeof(float) == 4 &&
     std::numeric_limits<float>::is_iec559) ||
    (std::same_as<T, double> && sizeof(double) == 8 &&
     std::numeric_limits<double>::is_iec559) ||
    (kComplex<T> && sizeof(float) == 4 && sizeof(double) == 8 &&
     std::numeric_limits<float>::is_iec559 &&
     std::numeric_limits<double>::is_iec559);

template <typename T>
constexpr std::string_view ScalarName() {
  if constexpr (std::same_as<T, std::complex<float>>) {
    return "c64";
  }
  if constexpr (std::same_as<T, std::complex<double>>) {
    return "c128";
  }
  if constexpr (std::same_as<T, float>) {
    return "f32";
  }
  if constexpr (std::same_as<T, double>) {
    return "f64";
  }
  if constexpr (std::is_signed_v<T>) {
    if constexpr (sizeof(T) == 1) {
      return "i8";
    }
    if constexpr (sizeof(T) == 2) {
      return "i16";
    }
    if constexpr (sizeof(T) == 4) {
      return "i32";
    }
    if constexpr (sizeof(T) == 8) {
      return "i64";
    }
  } else {
    if constexpr (sizeof(T) == 1) {
      return "u8";
    }
    if constexpr (sizeof(T) == 2) {
      return "u16";
    }
    if constexpr (sizeof(T) == 4) {
      return "u32";
    }
    if constexpr (sizeof(T) == 8) {
      return "u64";
    }
  }
  return {};
}

ASC_CORE_EXPORT Status ValidateOptions(const ArrayPrintOptions& options);
ASC_CORE_EXPORT bool Overlaps(const void* first, std::size_t first_bytes,
                              const void* second, std::size_t second_bytes);

template <typename T>
Result<std::size_t> FormatScalar(T value, std::span<char> scratch,
                                 ArrayFloatFormat format, int precision) {
  if constexpr (!kWireScalar<T>) {
    return Status(ErrorCode::kUnsupported);
  } else {
    if (scratch.empty()) {
      return Status(ErrorCode::kAllocation);
    }
    char* const begin = scratch.data();
    char* const end = begin + scratch.size();
    if constexpr (kComplex<T>) {
      if (scratch.size() < 3) {
        return Status(ErrorCode::kAllocation);
      }
      begin[0] = '(';
      auto real =
          FormatScalar(value.real(), scratch.subspan(1), format, precision);
      if (!real.ok()) {
        return real.status();
      }
      if (*real > scratch.size() - 3) {
        return Status(ErrorCode::kAllocation);
      }
      begin[1 + *real] = ',';
      auto imag = FormatScalar(value.imag(), scratch.subspan(2 + *real), format,
                               precision);
      if (!imag.ok()) {
        return imag.status();
      }
      if (*imag >= scratch.size() - 2 - *real) {
        return Status(ErrorCode::kAllocation);
      }
      begin[2 + *real + *imag] = ')';
      return 3 + *real + *imag;
    } else if constexpr (std::integral<T>) {
      const auto result = std::to_chars(begin, end, value);
      if (result.ec != std::errc{}) {
        return Status(ErrorCode::kAllocation);
      }
      return static_cast<std::size_t>(result.ptr - begin);
    } else {
      if (!std::isfinite(value)) {
        std::string_view token = std::signbit(value) ? "-inf" : "inf";
        if (std::isnan(value)) {
          token = "nan";
        }
        if (scratch.size() < token.size()) {
          return Status(ErrorCode::kAllocation);
        }
        for (std::size_t i = 0; i < token.size(); ++i) {
          begin[i] = token[i];
        }
        return token.size();
      }
      std::chars_format notation = std::chars_format::general;
      if (format == ArrayFloatFormat::kFixed) {
        notation = std::chars_format::fixed;
      }
      if (format == ArrayFloatFormat::kScientific) {
        notation = std::chars_format::scientific;
      }
      const auto result = std::to_chars(begin, end, value, notation, precision);
      if (result.ec != std::errc{}) {
        return Status(ErrorCode::kAllocation);
      }
      return static_cast<std::size_t>(result.ptr - begin);
    }
  }
}

// Storage-neutral output counting. A null sink measures only metadata bytes;
// it does not read values or allocate. Owning-module templates reserve their
// own closing punctuation before writing optional preview tokens.
class ASC_CORE_EXPORT Output {
 public:
  Output(ByteSink* sink, std::size_t limit, ArrayPrintReport& report)
      : sink_(sink), limit_(limit), report_(report) {}
  [[nodiscard]] bool Fits(std::size_t bytes, std::size_t reserve = 0) const;
  bool Write(std::string_view text);
  [[nodiscard]] const Status& status() const { return status_; }
  ArrayPrintReport& report() { return report_; }
  void Fail(ErrorCode code) {
    if (status_.ok()) {
      status_ = Status(code);
    }
  }

 private:
  ByteSink* sink_;
  std::size_t limit_;
  ArrayPrintReport& report_;
  Status status_;
};

// Caller-buffer token assembly only: no array/schema knowledge.
class TextBuffer {
 public:
  explicit TextBuffer(std::span<char> scratch) : scratch_(scratch) {}
  bool Write(std::string_view text) {
    if (!ok_ || text.size() > scratch_.size() - size_) {
      ok_ = false;
      return false;
    }
    for (char character : text) {
      scratch_[size_++] = character;
    }
    return true;
  }
  template <typename T>
  bool Scalar(T value, ArrayFloatFormat format, int precision) {
    if (!ok_) {
      return false;
    }
    auto written =
        FormatScalar(value, scratch_.subspan(size_), format, precision);
    if (!written.ok()) {
      ok_ = false;
      return false;
    }
    size_ += *written;
    return true;
  }
  template <typename T>
  bool Integer(T value) {
    return Scalar(value, ArrayFloatFormat::kGeneral, 6);
  }
  [[nodiscard]] bool ok() const { return ok_; }
  [[nodiscard]] std::size_t size() const { return size_; }
  [[nodiscard]] std::string_view text() const {
    return {scratch_.data(), size_};
  }

 private:
  std::span<char> scratch_;
  std::size_t size_ = 0;
  bool ok_ = true;
};

inline std::size_t SelectedCount(std::size_t count, std::size_t limit) {
  return count < limit ? count : limit;
}

inline std::size_t SelectedIndex(std::size_t ordinal, std::size_t count,
                                 std::size_t limit, bool edge) {
  if (count <= limit || !edge || ordinal < limit / 2 + limit % 2) {
    return ordinal;
  }
  return count - (limit - ordinal);
}

template <typename Integer>
bool IntegerToken(Output& out, Integer value, std::span<char> scratch) {
  auto size = FormatScalar(value, scratch, ArrayFloatFormat::kGeneral, 6);
  if (!size.ok()) {
    out.Fail(size.status().code());
    return false;
  }
  return out.Write(std::string_view(scratch.data(), *size));
}

}  // namespace internal_array_format
}  // namespace asc

#endif  // ASC_CORE_ARRAY_FORMAT_H_
