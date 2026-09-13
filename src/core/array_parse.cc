#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <type_traits>

#include "asc/core/array_io.h"
#include "asc/core/result.h"
#include "asc/core/status.h"

namespace asc::internal_array_io {
namespace {

// Every binary64 rounding boundary has the form m*2^e with m < 2^54 and
// e >= -1075. For e >= 0 it has at most 309 decimal digits; for e < 0 its
// decimal coefficient m*5^(-e) has fewer than 800 significant digits.
// Retaining 1100 digits therefore leaves
// no boundary strictly inside a discarded decimal suffix interval: a sticky
// nonzero suffix can affect a tie, but cannot cross another boundary.
constexpr std::size_t kRetainedDigits = 1100;
// For admitted decimal orders [-400,400], powers of ten reach at most
// 10^1500 (4983 bits). Scaling the numerator to a binary64 quantum needs at
// most 4729 bits. All storage has fixed capacity on the stack.
constexpr std::size_t kWords = 192;

class Unsigned {
 public:
  explicit Unsigned(std::uint32_t value = 0) { words_[0] = value; }

  bool Multiply(std::uint32_t factor) {
    std::uint64_t carry = 0;
    for (auto& word : words_) {
      const std::uint64_t value = std::uint64_t{word} * factor + carry;
      word = static_cast<std::uint32_t>(value);
      carry = value >> 32;
    }
    return carry == 0;
  }

  bool Add(std::uint32_t value) {
    std::uint64_t carry = value;
    for (auto& word : words_) {
      carry += word;
      word = static_cast<std::uint32_t>(carry);
      carry >>= 32;
    }
    return carry == 0;
  }

  [[nodiscard]] int Width() const {
    for (std::size_t i = kWords; i != 0; --i) {
      if (words_[i - 1] != 0) {
        return static_cast<int>((i - 1) * 32) +
               (32 - std::countl_zero(words_[i - 1]));
      }
    }
    return 0;
  }

  bool Shift(int count) {
    if (count < 0 || Width() + count > static_cast<int>(kWords * 32)) {
      return false;
    }
    const auto whole = static_cast<std::size_t>(count / 32);
    const auto part = static_cast<unsigned int>(count % 32);
    for (std::size_t i = kWords; i != 0; --i) {
      const auto index = i - 1;
      std::uint32_t value = index >= whole ? words_[index - whole] << part : 0;
      if (part != 0 && index > whole) {
        value |= words_[index - whole - 1] >> (32 - part);
      }
      words_[index] = value;
    }
    return true;
  }

  [[nodiscard]] int Compare(const Unsigned& other) const {
    for (std::size_t i = kWords; i != 0; --i) {
      if (words_[i - 1] != other.words_[i - 1]) {
        return words_[i - 1] < other.words_[i - 1] ? -1 : 1;
      }
    }
    return 0;
  }

  void Subtract(const Unsigned& other) {
    std::uint64_t borrow = 0;
    for (std::size_t i = 0; i < kWords; ++i) {
      const std::uint64_t subtrahend = std::uint64_t{other.words_[i]} + borrow;
      const std::uint64_t value = words_[i];
      words_[i] = static_cast<std::uint32_t>(value - subtrahend);
      borrow = value < subtrahend ? 1 : 0;
    }
  }

 private:
  std::array<std::uint32_t, kWords> words_{};
};

struct Decimal {
  Unsigned coefficient;
  int order = 0;
  int retained = 0;
  bool negative = false;
  bool sticky = false;
};

// Combine signed magnitudes before clipping. A large exponent can cancel a
// long mantissa's leading zeros. The exponent cap exceeds every admitted
// token length by 1024, so saturation cannot conceal a representable result.
int Order(std::int64_t base, bool negative, std::uint64_t exponent) {
  const auto magnitude = static_cast<std::uint64_t>(base < 0 ? -base : base);
  const bool base_negative = base < 0;
  std::uint64_t result = 0;
  bool result_negative = negative;
  if (base_negative == negative) {
    result = std::min(magnitude, std::uint64_t{401}) +
             std::min(exponent, std::uint64_t{401});
  } else if (magnitude > exponent) {
    result = magnitude - exponent;
    result_negative = base_negative;
  } else {
    result = exponent - magnitude;
  }
  const auto clipped = static_cast<int>(std::min(result, std::uint64_t{401}));
  return result_negative ? -clipped : clipped;
}

Result<Decimal> ReadDecimal(std::string_view token) {
  if (!FiniteRealSyntax(token)) {
    return Status(ErrorCode::kEncoding);
  }
  if (token.size() >
      static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max())) {
    return Status(ErrorCode::kOverflow);
  }
  Decimal value;
  value.negative = token.front() == '-';
  if (token.front() == '-' || token.front() == '+') {
    token.remove_prefix(1);
  }
  std::int64_t integer_digits = 0;
  std::int64_t leading_zeros = 0;
  bool fraction = false;
  bool significant = false;
  std::size_t cursor = 0;
  for (; cursor < token.size() && token[cursor] != 'e' && token[cursor] != 'E';
       ++cursor) {
    const char character = token[cursor];
    if (character == '.') {
      fraction = true;
      continue;
    }
    integer_digits += fraction ? 0 : 1;
    if (!significant && character == '0') {
      ++leading_zeros;
      continue;
    }
    significant = true;
    const auto digit = static_cast<std::uint32_t>(character - '0');
    if (value.retained < static_cast<int>(kRetainedDigits)) {
      if (!value.coefficient.Multiply(10) || !value.coefficient.Add(digit)) {
        return Status(ErrorCode::kOverflow);
      }
      ++value.retained;
    } else {
      value.sticky = value.sticky || digit != 0;
    }
  }
  bool exponent_negative = false;
  std::uint64_t exponent = 0;
  if (cursor != token.size()) {
    ++cursor;
    exponent_negative = token[cursor] == '-';
    if (token[cursor] == '-' || token[cursor] == '+') {
      ++cursor;
    }
    constexpr auto kCap =
        std::uint64_t{std::numeric_limits<std::int64_t>::max()} + 1024;
    for (; cursor < token.size(); ++cursor) {
      const auto digit = static_cast<std::uint64_t>(token[cursor] - '0');
      exponent = exponent > (kCap - digit) / 10 ? kCap : exponent * 10 + digit;
    }
  }
  value.order =
      Order(integer_digits - leading_zeros, exponent_negative, exponent);
  return value;
}

Result<std::uint64_t> RoundedQuotient(Unsigned numerator,
                                      const Unsigned& denominator,
                                      bool sticky) {
  std::uint64_t quotient = 0;
  const int top = numerator.Width() - denominator.Width();
  if (top >= 64) {
    return Status(ErrorCode::kOverflow);
  }
  for (int bit = top; bit >= 0; --bit) {
    auto shifted = denominator;
    if (!shifted.Shift(bit)) {
      return Status(ErrorCode::kOverflow);
    }
    if (numerator.Compare(shifted) >= 0) {
      numerator.Subtract(shifted);
      quotient |= std::uint64_t{1} << bit;
    }
  }
  if (!numerator.Shift(1)) {
    return Status(ErrorCode::kOverflow);
  }
  const auto comparison = numerator.Compare(denominator);
  if (comparison > 0 || (comparison == 0 && (sticky || (quotient & 1) != 0))) {
    ++quotient;
  }
  return quotient;
}

Result<int> BinaryExponent(const Unsigned& numerator,
                           const Unsigned& denominator) {
  int exponent = numerator.Width() - denominator.Width();
  auto compared = exponent < 0 ? numerator : denominator;
  if (!compared.Shift(exponent < 0 ? -exponent : exponent)) {
    return Status(ErrorCode::kOverflow);
  }
  const auto comparison = exponent < 0 ? compared.Compare(denominator)
                                       : numerator.Compare(compared);
  return exponent - (comparison < 0 ? 1 : 0);
}

template <typename T>
Result<T> Convert(const Decimal& decimal) {
  using Bits = std::conditional_t<sizeof(T) == 4, std::uint32_t, std::uint64_t>;
  constexpr int kDigits = std::numeric_limits<T>::digits;
  constexpr int kMinimum = std::numeric_limits<T>::min_exponent - 1;
  constexpr int kMaximum = std::numeric_limits<T>::max_exponent - 1;
  constexpr Bits kSign = Bits{1} << (sizeof(T) * 8 - 1);
  const Bits sign = decimal.negative ? kSign : 0;
  if (decimal.retained == 0) {
    return std::bit_cast<T>(sign);
  }
  if (decimal.order < -400 || decimal.order > 400) {
    return Status(ErrorCode::kOverflow);
  }
  auto numerator = decimal.coefficient;
  Unsigned denominator(1);
  const int power = decimal.order - decimal.retained;
  auto& scaled = power < 0 ? denominator : numerator;
  for (int i = 0; i < (power < 0 ? -power : power); ++i) {
    if (!scaled.Multiply(10)) {
      return Status(ErrorCode::kOverflow);
    }
  }
  auto exponent_result = BinaryExponent(numerator, denominator);
  if (!exponent_result.ok()) {
    return exponent_result.status();
  }
  int exponent = *exponent_result;
  const int quantum = std::max(exponent, kMinimum) - (kDigits - 1);
  if (!(quantum < 0 ? numerator.Shift(-quantum) : denominator.Shift(quantum))) {
    return Status(ErrorCode::kOverflow);
  }
  auto rounded = RoundedQuotient(numerator, denominator, decimal.sticky);
  if (!rounded.ok()) {
    return rounded.status();
  }
  if (*rounded == 0) {
    return Status(ErrorCode::kOverflow);
  }
  if (exponent < kMinimum) {
    return std::bit_cast<T>(sign | static_cast<Bits>(*rounded));
  }
  if (*rounded == (std::uint64_t{1} << kDigits)) {
    *rounded >>= 1;
    ++exponent;
  }
  if (exponent > kMaximum) {
    return Status(ErrorCode::kOverflow);
  }
  const int biased_exponent = exponent - kMinimum + 1;
  const auto encoded_exponent = static_cast<Bits>(biased_exponent);
  const auto fraction =
      static_cast<Bits>(*rounded - (std::uint64_t{1} << (kDigits - 1)));
  return std::bit_cast<T>(sign | (encoded_exponent << (kDigits - 1)) |
                          fraction);
}

template <typename T>
Result<T> Parse(std::string_view token) {
  const auto decimal = ReadDecimal(token);
  if (!decimal.ok()) {
    return decimal.status();
  }
  return Convert<T>(*decimal);
}

}  // namespace

Result<float> ParseFiniteFloat(std::string_view token) {
  return Parse<float>(token);
}

Result<double> ParseFiniteDouble(std::string_view token) {
  return Parse<double>(token);
}

}  // namespace asc::internal_array_io
