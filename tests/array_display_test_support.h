#ifndef ASC_TESTS_ARRAY_DISPLAY_TEST_SUPPORT_H_
#define ASC_TESTS_ARRAY_DISPLAY_TEST_SUPPORT_H_

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <source_location>
#include <span>
#include <string_view>

#include "asc/core/array_format.h"
#include "asc/core/io.h"
#include "asc/core/result.h"
#include "asc/core/status.h"

namespace asc_array_display_test {

class TestContext {
 public:
  void Check(bool condition, std::string_view reason,
             std::source_location location = std::source_location::current()) {
    if (!condition) {
      std::fprintf(stderr, "%s:%u: %.*s\n", location.file_name(),
                   location.line(), static_cast<int>(reason.size()),
                   reason.data());
      ++failures_;
    }
  }

  [[nodiscard]] int Finish() const { return failures_ == 0 ? 0 : 1; }

 private:
  int failures_ = 0;
};

class Sink final : public asc::ByteSink {
 public:
  asc::Result<std::size_t> WriteSome(
      std::span<const std::byte> bytes) override {
    if (invalid_progress) {
      return bytes.size() + 1;
    }
    if (zero_progress) {
      return std::size_t{0};
    }
    if (size_ == fail_after) {
      return asc::Status(asc::ErrorCode::kIo, {}, {}, 1729);
    }
    const std::size_t count = std::min(
        {bytes.size(), chunk, fail_after - size_, data_.size() - size_});
    for (std::size_t i = 0; i < count; ++i) {
      data_[size_++] = static_cast<char>(bytes[i]);
    }
    return count;
  }

  [[nodiscard]] std::string_view text() const { return {data_.data(), size_}; }

  std::size_t chunk = std::numeric_limits<std::size_t>::max();
  std::size_t fail_after = std::numeric_limits<std::size_t>::max();
  bool invalid_progress = false;
  bool zero_progress = false;

 private:
  std::array<char, 32768> data_{};
  std::size_t size_ = 0;
};

inline void CheckReset(TestContext& test, const asc::ArrayPrintReport& report) {
  test.Check(report.output_bytes == 0 && report.values_displayed == 0 &&
                 !report.truncated,
             "validation must reset every report field");
}

// Fixtures use unique numeric tokens, distinct from metadata and coordinates.
// The caller supplies their exact spelling independently of the formatter.
inline std::size_t CompleteTokens(std::string_view complete,
                                  std::span<const std::string_view> tokens,
                                  std::size_t accepted) {
  std::size_t count = 0;
  for (const auto token : tokens) {
    const std::size_t position = complete.find(token);
    if (position != std::string_view::npos &&
        position + token.size() <= accepted) {
      ++count;
    }
  }
  return count;
}

template <typename Print>
void FailureBoundaries(TestContext& test, Print print,
                       std::string_view expected,
                       std::span<const std::string_view> tokens) {
  for (const auto token : tokens) {
    test.Check(expected.find(token) != std::string_view::npos,
               "oracle token must occur in the literal expected output");
  }
  for (std::size_t boundary = 0; boundary < expected.size(); ++boundary) {
    Sink sink;
    sink.fail_after = boundary;
    sink.chunk = 2;
    asc::ArrayPrintReport report{777, 888, true};
    const auto status = print(sink, report);
    test.Check(
        status.code() == asc::ErrorCode::kIo && status.native_code() == 1729,
        "a failing sink must retain its error and native code");
    test.Check(sink.text() == expected.substr(0, boundary),
               "a sink failure must preserve the exact accepted prefix");
    test.Check(report.output_bytes == boundary,
               "the report must count every accepted short-write byte");
    test.Check(
        report.values_displayed == CompleteTokens(expected, tokens, boundary),
        "the report must count exactly the fully accepted value tokens");
  }
}

inline void Balanced(TestContext& test, std::string_view text) {
  int depth = 0;
  for (const char value : text) {
    if (value == '[') {
      ++depth;
    } else if (value == ']') {
      --depth;
      test.Check(depth >= 0, "no unmatched closing display bracket");
    }
    test.Check(static_cast<unsigned char>(value) < 128 && value != '\r',
               "display must use ASCII with LF line endings");
  }
  test.Check(depth == 0, "successful output must have balanced brackets");
}

template <typename Print>
void BudgetBoundaries(TestContext& test, Print print,
                      std::span<const std::string_view> tokens,
                      std::size_t maximum) {
  bool complete_seen = false;
  bool truncated_seen = false;
  for (std::size_t budget = 0; budget <= maximum; ++budget) {
    Sink sink;
    asc::ArrayPrintReport report{777, 888, true};
    const auto status = print(sink, report, budget);
    test.Check(sink.text().size() <= budget &&
                   report.output_bytes == sink.text().size(),
               "every budget must bound actual bytes and report progress");
    test.Check(report.values_displayed ==
                   CompleteTokens(sink.text(), tokens, sink.text().size()),
               "every budget must count exactly the fully emitted values");
    if (status.ok()) {
      Balanced(test, sink.text());
      if (report.truncated) {
        truncated_seen = true;
        test.Check(
            sink.text().ends_with("... (truncated)\n"),
            "every successful truncated output must end with its marker");
      } else {
        complete_seen = true;
        test.Check(report.values_displayed == tokens.size(),
                   "successful complete output must emit every fixture value");
      }
    }
  }
  test.Check(complete_seen && truncated_seen,
             "the sweep must actually exercise complete and truncated success");
}

}  // namespace asc_array_display_test

#endif  // ASC_TESTS_ARRAY_DISPLAY_TEST_SUPPORT_H_
