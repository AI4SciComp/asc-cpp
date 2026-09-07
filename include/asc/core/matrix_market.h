#ifndef ASC_CORE_MATRIX_MARKET_H_
#define ASC_CORE_MATRIX_MARKET_H_

/** @file
 * @brief Storage-neutral Matrix Market lexical and scalar policies.
 *
 * Dense and Sparse own their shapes, representations, assembly and
 * transactions. Core supplies only bounded host byte/token/scalar operations,
 * with no storage dependency, allocation, transfer, synchronization or provider
 * selection.
 * @ingroup asc_core
 */

#include <array>
#include <bit>
#include <cmath>
#include <complex>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <type_traits>

#include "asc/core/array_format.h"
#include "asc/core/array_io.h"
#include "asc/core/export.h"
#include "asc/core/io.h"
#include "asc/core/result.h"
#include "asc/core/status.h"

namespace asc {

/** @brief Matrix Market scalar-field identity, independent of storage kind.
 *
 * The owning reader validates the field against the explicit destination type.
 * Integer-to-floating input requires exact representability; real/complex input
 * never silently converts to an integer or discards an imaginary component.
 * These constant-size policy values allocate nothing and imply no data access.
 * @ingroup asc_core
 */
enum class MatrixMarketField : std::uint8_t {
  kInteger,  ///< Signed decimal integer syntax, checked in the destination.
  kReal,     ///< One finite real component per scalar record.
  kComplex,  ///< Two finite real components, real then imaginary.
  kPattern,  ///< No scalar tokens; requires an explicit value interpretation.
};

/** @brief Matrix Market symmetry interpretation; owning modules validate it.
 *
 * Structured forms require a square matrix and valid lower-half/diagonal
 * records. An ordinary view is not made symmetric merely by selecting a flag:
 * writers validate the whole represented matrix before selecting its triangle.
 * A policy value itself performs no allocation, host/device access or work.
 * @ingroup asc_core
 */
enum class MatrixMarketSymmetry : std::uint8_t {
  kGeneral,        ///< No implied mirror entries.
  kSymmetric,      ///< Lower off-diagonal entries mirror without conjugation.
  kSkewSymmetric,  ///< Strict lower entries mirror by checked negation.
  kHermitian,  ///< Complex lower entries mirror by conjugation; real diagonal.
};

/** @brief Explicit scalar interpretation for value-free pattern records.
 *
 * Only coordinate pattern general/symmetric forms are supported by the profile.
 * This is a bounded host interchange policy, not permission to initialize
 * device memory, allocate storage, discard non-unit output values or choose a
 * provider.
 * @ingroup asc_core
 */
enum class MatrixMarketPatternPolicy : std::uint8_t {
  kUnspecified,  ///< Reject a pattern field until the caller chooses a policy.
  kUnit,         ///< Each retained pattern entry has exact value one (1+0i).
};

namespace internal_matrix_market {

// Token views borrow the caller's scratch until its next use. No shape or
// representation is interpreted here; five fields suffice for the fixed banner.
struct Record {
  std::array<std::string_view, 5> fields{};
  std::size_t count = 0;
  bool end_of_file = false;
  [[nodiscard]] std::span<const std::string_view> tokens() const {
    return std::span(fields).first(count);
  }
};

class ASC_CORE_EXPORT Input {
 public:
  Input(ByteSource& source, ArrayIoLimits limits, ArrayIoReport& report)
      : source_(&source), limits_(limits), report_(&report) {}
  Input(const Input&) = delete;
  Input& operator=(const Input&) = delete;
  Input(Input&&) noexcept = default;
  Input& operator=(Input&&) = delete;
  ~Input() = default;
  Result<Record> FirstRecord(std::span<std::byte> scratch);
  Result<Record> NextRecord(std::span<std::byte> scratch);
  Status Finish(std::span<std::byte> scratch);
  Status Fail(ErrorCode code);
  [[nodiscard]] const ArrayIoLimits& limits() const { return limits_; }
  [[nodiscard]] const Status& status() const { return status_; }
  ArrayIoReport& report() { return *report_; }

 private:
  Result<int> ReadByte();
  Result<int> ReadLineByte(std::size_t& line_bytes);
  Status SkipComment(std::size_t& line_bytes);
  Result<Record> ReadRecord(std::span<std::byte> scratch, bool comments);
  ByteSource* source_;
  ArrayIoLimits limits_;
  ArrayIoReport* report_;
  Status status_;
  bool eof_ = false;
};

ASC_CORE_EXPORT bool WordEquals(std::string_view token,
                                std::string_view lowercase);
ASC_CORE_EXPORT Result<MatrixMarketField> ParseField(std::string_view token);
ASC_CORE_EXPORT Result<MatrixMarketSymmetry> ParseSymmetry(
    std::string_view token);
ASC_CORE_EXPORT std::string_view FieldName(MatrixMarketField field);
ASC_CORE_EXPORT std::string_view SymmetryName(MatrixMarketSymmetry symmetry);
ASC_CORE_EXPORT Result<std::uint64_t> ParseUnsigned(std::string_view token,
                                                    std::size_t max_token);
ASC_CORE_EXPORT Status Text(internal_array_io::Output& output,
                            std::string_view text);

template <typename T>
constexpr MatrixMarketField ScalarField() {
  if constexpr (std::integral<T>) {
    return MatrixMarketField::kInteger;
  } else if constexpr (internal_array_format::kComplex<T>) {
    return MatrixMarketField::kComplex;
  } else {
    return MatrixMarketField::kReal;
  }
}

template <typename T>
bool Finite(T value) {
  if constexpr (std::integral<T>) {
    return true;
  } else if constexpr (internal_array_format::kComplex<T>) {
    return std::isfinite(value.real()) && std::isfinite(value.imag());
  } else {
    return std::isfinite(value);
  }
}

template <typename T>
bool IsZero(T value) {
  return value == T{0};
}

template <typename T>
Result<T> CheckedSum(T left, T right) {
  if constexpr (std::integral<T>) {
    if constexpr (std::is_signed_v<T>) {
      if ((right > 0 && left > std::numeric_limits<T>::max() - right) ||
          (right < 0 && left < std::numeric_limits<T>::min() - right)) {
        return Status(ErrorCode::kOverflow);
      }
    } else if (left > std::numeric_limits<T>::max() - right) {
      return Status(ErrorCode::kOverflow);
    }
    return static_cast<T>(left + right);
  } else if constexpr (internal_array_format::kComplex<T>) {
    auto real = CheckedSum(left.real(), right.real());
    auto imag = CheckedSum(left.imag(), right.imag());
    if (!real.ok() || !imag.ok()) {
      return Status(ErrorCode::kOverflow);
    }
    return T(*real, *imag);
  } else {
    const T result = left + right;
    if (!Finite(result)) {
      return Status(ErrorCode::kOverflow);
    }
    return result;
  }
}

template <typename T>
Result<T> CheckedNegate(T value) {
  if constexpr (std::integral<T>) {
    if constexpr (std::is_signed_v<T>) {
      if (value == std::numeric_limits<T>::min()) {
        return Status(ErrorCode::kOverflow);
      }
    } else if (value != 0) {
      return Status(ErrorCode::kOverflow);
    }
    return static_cast<T>(-value);
  } else {
    if (!Finite(value)) {
      return Status(ErrorCode::kOverflow);
    }
    return -value;
  }
}

template <typename T>
T Conjugate(T value) {
  if constexpr (internal_array_format::kComplex<T>) {
    return T(value.real(), -value.imag());
  } else {
    return value;
  }
}

template <std::floating_point T>
Result<T> ParseExactInteger(std::string_view token) {
  // Fixed, component-bounded magnitude limbs permit exact large powers without
  // imposing a uint64 source limit or an allocating arbitrary-precision parser.
  constexpr std::size_t kLimbs =
      (std::numeric_limits<T>::max_exponent + 31) / 32;
  std::array<std::uint32_t, kLimbs> magnitude{};
  auto digits = token;
  if (!digits.empty() && (digits.front() == '+' || digits.front() == '-')) {
    digits.remove_prefix(1);
  }
  if (digits.empty()) {
    return Status(ErrorCode::kEncoding);
  }
  for (const char digit : digits) {
    if (digit < '0' || digit > '9') {
      return Status(ErrorCode::kEncoding);
    }
    std::uint64_t carry = static_cast<std::uint64_t>(digit - '0');
    for (auto& limb : magnitude) {
      const std::uint64_t value = std::uint64_t{10} * limb + carry;
      limb = static_cast<std::uint32_t>(value);
      carry = value >> 32;
    }
    if (carry != 0) {
      return Status(ErrorCode::kOverflow);
    }
  }
  std::size_t significant = 0;
  for (std::size_t i = magnitude.size(); i != 0; --i) {
    if (magnitude[i - 1] != 0) {
      significant = (i - 1) * 32 + std::bit_width(magnitude[i - 1]);
      break;
    }
  }
  std::size_t trailing = 0;
  if (significant != 0) {
    for (const auto limb : magnitude) {
      if (limb != 0) {
        trailing += static_cast<std::size_t>(std::countr_zero(limb));
        break;
      }
      trailing += 32;
    }
  }
  if (significant - trailing > std::numeric_limits<T>::digits) {
    return Status(ErrorCode::kOverflow);
  }
  // Exactness was proved using integer limbs, before the component conversion.
  return internal_array_io::ParseFinite<T>(token);
}

template <typename T>
Status ValidateField(MatrixMarketField field,
                     MatrixMarketPatternPolicy pattern) {
  if constexpr (!internal_array_format::kWireScalar<T>) {
    return Status(ErrorCode::kUnsupported);
  } else {
    if (pattern != MatrixMarketPatternPolicy::kUnspecified &&
        pattern != MatrixMarketPatternPolicy::kUnit) {
      return Status(ErrorCode::kInvalidArgument);
    }
    if (field == MatrixMarketField::kPattern) {
      return pattern == MatrixMarketPatternPolicy::kUnit
                 ? Status::Ok()
                 : Status(ErrorCode::kInvalidArgument);
    }
    if (field == MatrixMarketField::kInteger) {
      return Status::Ok();
    }
    if (field == MatrixMarketField::kReal && !std::integral<T>) {
      return Status::Ok();
    }
    if (field == MatrixMarketField::kComplex &&
        internal_array_format::kComplex<T>) {
      return Status::Ok();
    }
    return Status(ErrorCode::kUnsupported);
  }
}

template <typename T>
Result<T> ParseValue(MatrixMarketField field,
                     std::span<const std::string_view> tokens,
                     MatrixMarketPatternPolicy pattern, std::size_t max_token) {
  const auto valid = ValidateField<T>(field, pattern);
  if (!valid.ok()) {
    return valid;
  }
  if (field == MatrixMarketField::kPattern) {
    return tokens.empty() ? Result<T>(T{1}) : Status(ErrorCode::kEncoding);
  }
  const std::size_t required = field == MatrixMarketField::kComplex ? 2 : 1;
  if (tokens.size() != required) {
    return Status(ErrorCode::kEncoding);
  }
  for (const auto token : tokens) {
    if (token.empty() || token.size() > max_token) {
      return Status(ErrorCode::kEncoding);
    }
  }
  if constexpr (internal_array_format::kComplex<T>) {
    using Real = typename T::value_type;
    if (field != MatrixMarketField::kComplex) {
      auto real = ParseValue<Real>(field, tokens, pattern, max_token);
      if (!real.ok()) {
        return real.status();
      }
      return T(*real, Real{0});
    }
    auto real = ParseValue<Real>(MatrixMarketField::kReal, tokens.first(1),
                                 pattern, max_token);
    auto imag = ParseValue<Real>(MatrixMarketField::kReal, tokens.subspan(1),
                                 pattern, max_token);
    if (!real.ok()) {
      return real.status();
    }
    if (!imag.ok()) {
      return imag.status();
    }
    return T(*real, *imag);
  } else if constexpr (std::integral<T>) {
    return internal_array_io::ParseScalar<T>(tokens[0], max_token);
  } else {
    if (field == MatrixMarketField::kInteger) {
      return ParseExactInteger<T>(tokens[0]);
    }
    if (!internal_array_io::FiniteRealSyntax(tokens[0])) {
      return tokens[0] == "inf" || tokens[0] == "-inf" || tokens[0] == "nan"
                 ? Status(ErrorCode::kOverflow)
                 : Status(ErrorCode::kEncoding);
    }
    return internal_array_io::ParseFinite<T>(tokens[0]);
  }
}

template <typename T>
Status WriteComponent(T value, std::span<std::byte> scratch,
                      std::size_t max_token,
                      internal_array_io::Output& output) {
  auto size = internal_array_format::FormatScalar(
      value, std::span(reinterpret_cast<char*>(scratch.data()), scratch.size()),
      ArrayFloatFormat::kGeneral, std::numeric_limits<T>::max_digits10);
  if (!size.ok()) {
    return size.status();
  }
  if (*size > max_token) {
    return Status(ErrorCode::kAllocation);
  }
  return output.Write(scratch.first(*size), false);
}

template <typename T>
Status WriteValue(T value, MatrixMarketField field,
                  std::span<std::byte> scratch, std::size_t max_token,
                  internal_array_io::Output& output) {
  if constexpr (!internal_array_format::kWireScalar<T>) {
    return Status(ErrorCode::kUnsupported);
  } else {
    if (!Finite(value)) {
      return Status(ErrorCode::kOverflow);
    }
    if (field == MatrixMarketField::kPattern) {
      return value == T{1} ? Status::Ok() : Status(ErrorCode::kEncoding);
    }
    if (field != ScalarField<T>()) {
      return Status(ErrorCode::kUnsupported);
    }
    if constexpr (internal_array_format::kComplex<T>) {
      auto status = WriteComponent(value.real(), scratch, max_token, output);
      if (!status.ok()) {
        return status;
      }
      status = Text(output, " ");
      if (!status.ok()) {
        return status;
      }
      return WriteComponent(value.imag(), scratch, max_token, output);
    } else {
      return WriteComponent(value, scratch, max_token, output);
    }
  }
}

}  // namespace internal_matrix_market
}  // namespace asc

#endif  // ASC_CORE_MATRIX_MARKET_H_
