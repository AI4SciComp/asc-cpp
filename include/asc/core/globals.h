// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/globals.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_GLOBALS_H_
#define ASC_GLOBALS_H_

/// @file globals.h
/// @brief Global I/O streams and pretty printing utilities
///
/// This module provides:
/// - Global output streams (mout, merr) with enable/disable capability
/// - Pretty printing with customizable formatting
/// - Type-specific print flags for width, precision, and alignment
///
/// @par Example - Using global streams:
/// @code
/// asc::mout << "Standard output\n";
/// asc::merr << "Error output\n";
/// asc::mout.Disable();  // Silence output
/// asc::mout << "This won't appear";
/// asc::mout.Enable();   // Re-enable output
/// @endcode
///
/// @par Example - Pretty printing:
/// @code
/// double x = 3.14159;
/// asc::PPrint<double, true>(std::cout, x);  // Aligned, formatted output
/// @endcode

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include "asc/core/config.h"

namespace asc {

namespace details {

template <int Value, class Seq>
struct RepeatIntImpl;

template <int Value, std::size_t... Is>
struct RepeatIntImpl<Value, std::index_sequence<Is...>> {
  using Type =
      std::integer_sequence<int, (static_cast<void>(Is), Value)...>;
};

}  // namespace details

/// @brief Generate an integer_sequence with repeated integer values.
template <int Value, std::size_t Count>
using RepeatInt =
    typename details::RepeatIntImpl<Value,
                                    std::make_index_sequence<Count>>::Type;

/// @brief Sentinel value for runtime-dynamic extents.
inline constexpr int kDynamicExtent = -1;

/// @brief Generate Count dynamic extents.
template <std::size_t Count>
using DynamicExtents = RepeatInt<kDynamicExtent, Count>;

/// @brief Concept for types convertible to another type.
template <typename From, typename To>
concept ConvertibleTo = std::is_convertible_v<From, To>;

/// @brief Concept for trivial types.
template <typename T>
concept Trivial = std::is_trivial_v<T>;

/// @brief Concept for copyable types.
template <typename T>
concept Copyable =
    std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T>;

/// @brief Concept for movable types.
template <typename T>
concept Movable =
    std::is_move_constructible_v<T> && std::is_move_assignable_v<T>;

/// @brief Concept for arithmetic types.
template <typename T>
concept Arithmetic = std::is_arithmetic_v<T>;

/// @brief Concept for integral types.
template <typename T>
concept Integral = std::is_integral_v<T>;

/// @brief Concept for signed integral types.
template <typename T>
concept SignedIntegral = Integral<T> && std::is_signed_v<T>;

/// @brief Concept for unsigned integral types.
template <typename T>
concept UnsignedIntegral = Integral<T> && std::is_unsigned_v<T>;

/// @brief Concept for floating-point types.
template <typename T>
concept FloatingPoint = std::is_floating_point_v<T>;

/// @brief Concept for string-like types.
template <typename T>
concept String = std::constructible_from<std::string_view, T>;

/// @brief Concept for arithmetic or string-like types.
template <typename T>
concept ArithmeticOrString = Arithmetic<T> || String<T>;

/// @brief Global output stream with enable/disable capability
///
/// OutStream extends std::ostream to allow runtime control of output.
/// This is useful for suppressing diagnostic messages or redirecting output.
class OutStream : public std::ostream {
 public:
  /// Construct an OutStream from the given stream @a out, by using its
  /// `rdbuf()`.
  explicit OutStream(std::ostream& os) : std::ostream(NULL) { SetStream(os); }

  /// Replace the `rdbuf()` and `tie()` of the OutStream with that of out,
  /// enabling output.
  void SetStream(std::ostream& os) {
    rdbuf(rdbuf_ = os.rdbuf());
    tie(tie_ = os.tie());
  }

  /// Enable output.
  void Enable();

  /// Disable output.
  void Disable();

  /// Check if output is enabled.
  bool IsEnabled() const { return (rdbuf() != NULL); }

 private:
  /// Pointer that stores the associated streambuf when output is disabled.
  std::streambuf* rdbuf_;

  /// Pointer that stores the tied ostream when output is disabled.
  std::ostream* tie_;
};

/// Global output stream used by the library for standard output. Initially it
/// uses the same std::streambuf as std::cout, however that can be changed.
extern OutStream mout;

/// Global stream used by the library for standard error output. Initially it
/// uses the same std::streambuf as std::cerr, however that can be changed.
extern OutStream merr;

/// @brief Format flags for pretty printing
///
/// PrintFlag defines formatting parameters for different types:
/// - kWidth: Field width for alignment (< 0 means no width)
/// - kPrecision: Decimal precision (< 0 means default)
/// - kLimit: Maximum number of elements to print for containers
///
/// @tparam T Type to be formatted
template <typename T>
struct PrintFlag {
  static constexpr int kWidth = -1;      ///< No fixed width
  static constexpr int kPrecision = -1;  ///< Default precision
  static constexpr int kLimit = 8;       ///< Print limit for containers
};

/// @brief Format flags for int type
template <>
struct PrintFlag<int> {
  static constexpr int kWidth = 8;       ///< 8-character width
  static constexpr int kPrecision = -1;  ///< No decimal precision
  static constexpr int kLimit = 8;       ///< Print up to 8 elements
};

/// @brief Format flags for float type
template <>
struct PrintFlag<float> {
  static constexpr int kWidth = 12;     ///< 12-character width
  static constexpr int kPrecision = 4;  ///< 4 decimal places
  static constexpr int kLimit = 8;      ///< Print up to 8 elements
};

/// @brief Format flags for double type
template <>
struct PrintFlag<double> {
  static constexpr int kWidth = 16;     ///< 16-character width
  static constexpr int kPrecision = 8;  ///< 8 decimal places
  static constexpr int kLimit = 8;      ///< Print up to 8 elements
};

/// @brief Pretty print a value with optional formatting
///
/// Prints a value using type-specific formatting from PrintFlag.
/// When Aligned is true, applies width and alignment.
///
/// @tparam T Value type
/// @tparam Aligned Whether to apply alignment (default: false)
/// @param os Output stream
/// @param x Value to print
///
/// @par Example - Basic printing:
/// @code
/// asc::PPrint(std::cout, 3.14159);  // Uses default double precision
/// @endcode
///
/// @par Example - Aligned printing:
/// @code
/// asc::PPrint<double, true>(std::cout, 3.14);  // Right-aligned in 16-char
/// field
/// @endcode
template <typename T, bool Aligned = false>
void PPrint(std::ostream& os, const T& x) {
  if (PrintFlag<T>::kPrecision >= 0) {
    os << std::fixed << std::setprecision(PrintFlag<T>::kPrecision);
  }
  if constexpr (Aligned) {
    os << std::right;
    if (PrintFlag<T>::kWidth > 0) {
      os << std::setw(PrintFlag<T>::kWidth);
    }
  }
  os << x;
}

}  // namespace asc

#endif  // ASC_GLOBALS_H_
