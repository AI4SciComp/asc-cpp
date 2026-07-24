// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/casts.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_CASTS_H_
#define ASC_CASTS_H_

#include <cstddef>
#include <cctype>
#include <exception>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>

#include "asc/core/error.h"

namespace asc {

namespace cast_detail {

inline void VerifyConsumed(const std::string& s, std::size_t pos) {
  while (pos < s.size() &&
         std::isspace(static_cast<unsigned char>(s[pos])) != 0) {
    ++pos;
  }
  ASC_VERIFY(pos == s.size(), "failed to consume full string: " << s);
}

}  // namespace cast_detail

// ----------------------------------------------------------------------
// Static casts
// ----------------------------------------------------------------------

/// @brief Convert a value to a string.
/// @tparam T Type of the value
/// @param x Value to convert
/// @return String representation of the value
template <typename T>
inline std::string ToString(const T& x) {
  std::ostringstream out;
  out << x;
  ASC_VERIFY(!out.fail(), "failed to convert value to string");
  return out.str();
}

template <>
inline std::string ToString<std::string>(const std::string& x) {
  return x;
}

/// @brief Convert a boolean value to a string.
/// @param x Boolean value to convert
/// @return "true" if x is true, "false" otherwise
template <>
inline std::string ToString<bool>(const bool& x) {
  return (x ? "true" : "false");
}

/// @brief Convert a string to a value of type T.
/// @tparam T Type to convert to
/// @param s String to convert
/// @return Value of type T
template <typename T>
inline T FromString(std::string s) {
  T value{};
  std::istringstream in(s);
  in >> value;
  ASC_VERIFY(!in.fail(), "failed to convert string: " << s);
  in >> std::ws;
  ASC_VERIFY(in.eof(), "failed to consume full string: " << s);
  return value;
}

template <>
inline std::string FromString<std::string>(std::string s) {
  return s;
}

/// @brief Convert a string to an int.
/// @param s String to convert
/// @return Converted int value
template <>
inline int FromString<int>(std::string s) {
  try {
    std::size_t pos = 0;
    int value = std::stoi(s, &pos);
    cast_detail::VerifyConsumed(s, pos);
    return value;
  } catch (const std::exception&) {
    ASC_VERIFY(false, "failed to convert string to int: " << s);
    return 0;
  }
}

/// @brief Convert a string to a float.
/// @param s String to convert
/// @return Converted float value
template <>
inline float FromString<float>(std::string s) {
  try {
    std::size_t pos = 0;
    float value = std::stof(s, &pos);
    cast_detail::VerifyConsumed(s, pos);
    return value;
  } catch (const std::exception&) {
    ASC_VERIFY(false, "failed to convert string to float: " << s);
    return 0.0f;
  }
}

/// @brief Convert a string to a double.
/// @param s String to convert
/// @return Converted double value
template <>
inline double FromString<double>(std::string s) {
  try {
    std::size_t pos = 0;
    double value = std::stod(s, &pos);
    cast_detail::VerifyConsumed(s, pos);
    return value;
  } catch (const std::exception&) {
    ASC_VERIFY(false, "failed to convert string to double: " << s);
    return 0.0;
  }
}

/// @brief Convert a string to a long double.
/// @param s String to convert
/// @return Converted long double value
template <>
inline long double FromString<long double>(std::string s) {
  try {
    std::size_t pos = 0;
    long double value = std::stold(s, &pos);
    cast_detail::VerifyConsumed(s, pos);
    return value;
  } catch (const std::exception&) {
    ASC_VERIFY(false, "failed to convert string to long double: " << s);
    return 0.0L;
  }
}

/// @brief Convert a string to a boolean.
/// @param s String to convert
/// @return Converted boolean value
template <>
inline bool FromString<bool>(std::string s) {
  std::size_t first = 0;
  while (first < s.size() &&
         std::isspace(static_cast<unsigned char>(s[first])) != 0) {
    ++first;
  }
  std::size_t last = s.size();
  while (last > first &&
         std::isspace(static_cast<unsigned char>(s[last - 1])) != 0) {
    --last;
  }
  s = s.substr(first, last - first);
  for (char& ch : s) {
    if (ch >= 'A' && ch <= 'Z') {
      ch = static_cast<char>(ch - 'A' + 'a');
    }
  }
  if (s == "true" || s == "1" || s == "yes" || s == "on") return true;
  if (s == "false" || s == "0" || s == "no" || s == "off" || s.empty()) {
    return false;
  }
  ASC_VERIFY(false, "failed to convert string to bool: " << s);
  return false;
}

/// @brief Convert a size_t value to an int.
/// @param s Size value to convert
/// @return Converted int value
int SizeToInt(size_t);

/// @brief Convert a uint32_t value to a float in [0, 1).
/// @param x Value to convert
/// @return Converted float value
float UInt32ToFloat(uint32_t);

/// @brief Convert a uint64_t value to a double in [0, 1).
/// @param x Value to convert
/// @return Converted double value
double UInt64ToDouble(uint64_t);

// ----------------------------------------------------------------------
// Dynamic casts
// ----------------------------------------------------------------------
/// @brief Perform dynamic cast from From* to To*.
/// @tparam From Source type
/// @tparam To Target type
/// @param x Pointer to source object
/// @return Pointer to target object, or nullptr if cast fails
template <typename From, typename To>
constexpr To* DynamicCast(From* x) noexcept {
  return dynamic_cast<To*>(x);
}

/// @brief Perform dynamic cast from From& to To&.
/// @tparam From Source type
/// @tparam To Target type
/// @param x Reference to source object
/// @return Reference to target object
template <typename From, typename To>
constexpr To& DynamicCast(From& x) {
  return dynamic_cast<To&>(x);
}

/// @brief Perform dynamic cast from std::shared_ptr<From> to
/// std::shared_ptr<To>.
/// @tparam From Source type
/// @tparam To Target type
/// @param x Shared pointer to source object
/// @return Shared pointer to target object or nullptr if cast fails
template <typename From, typename To>
constexpr std::shared_ptr<To> DynamicCast(std::shared_ptr<From> x) noexcept {
  return std::dynamic_pointer_cast<To>(x);
}

// ----------------------------------------------------------------------
// Underlying casts.
// ----------------------------------------------------------------------
/// @brief Convert an enumerator to its underlying type.
/// @tparam Enum Type of the enumerator
/// @param e Enum to convert
/// @return Value of the underlying type
template <typename Enum>
constexpr typename std::underlying_type<Enum>::type UnderlyingCast(
    Enum e) noexcept {
  return static_cast<typename std::underlying_type<Enum>::type>(e);
}

}  // namespace asc

#endif  // ASC_CASTS_H_
