// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/string.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_STRING_H_
#define ASC_STRING_H_

#include <cstddef>
#include <limits>
#include <string>
#include <string_view>

#include "asc/core/error.h"

namespace asc {

inline char ToLowerAscii(char ch) {
  return ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch - 'A' + 'a') : ch;
}

inline bool EqualsIgnoreCase(std::string_view lhs, std::string_view rhs) {
  if (lhs.size() != rhs.size()) return false;
  for (std::size_t i = 0; i < lhs.size(); ++i) {
    if (ToLowerAscii(lhs[i]) != ToLowerAscii(rhs[i])) return false;
  }
  return true;
}

inline bool IsAsciiWhitespace(char ch) {
  return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

inline std::string_view TrimView(std::string_view text) {
  std::size_t first = 0;
  while (first < text.size() && IsAsciiWhitespace(text[first])) {
    ++first;
  }

  std::size_t last = text.size();
  while (last > first && IsAsciiWhitespace(text[last - 1])) {
    --last;
  }

  return text.substr(first, last - first);
}

inline std::string Trim(std::string_view text) {
  std::string_view trimmed = TrimView(text);
  return std::string(trimmed);
}

inline std::string_view RemoveCommentView(std::string_view line,
                                          char comment = '#') {
  const std::size_t pos = line.find(comment);
  return pos == std::string_view::npos ? line : line.substr(0, pos);
}

inline std::string RemoveComment(std::string_view line, char comment = '#') {
  return std::string(RemoveCommentView(line, comment));
}

inline bool IsDigitAscii(char ch) { return ch >= '0' && ch <= '9'; }

inline int ParsePositiveInt(std::string_view text) {
  ASC_VERIFY(!text.empty(), "Expected positive integer.");
  int value = 0;
  for (char ch : text) {
    ASC_VERIFY(IsDigitAscii(ch), "Expected positive integer.");
    const int digit = ch - '0';
    ASC_VERIFY(value <= (std::numeric_limits<int>::max() - digit) / 10,
                  "Integer value is too large.");
    value = value * 10 + digit;
  }
  return value;
}

}  // namespace asc

#endif  // ASC_STRING_H_
