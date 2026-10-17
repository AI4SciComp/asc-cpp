#ifndef ASC_TESTS_ARRAY_DISPLAY_MODES_TEST_SUPPORT_H_
#define ASC_TESTS_ARRAY_DISPLAY_MODES_TEST_SUPPORT_H_

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "allocation_observation.h"
#include "array_display_test_support.h"
#include "asc/core/array_format.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "locale_test_support.h"

namespace asc_array_display_modes_test {
using asc_array_display_test::Sink;
using asc_array_display_test::TestContext;

template <typename T>
T Take(asc::Result<T> result) {
  if (!result.ok()) {
    std::abort();
  }
  return std::move(*result);
}

template <typename Probe, typename Function>
auto WithoutAllocation(TestContext& test, Function function) {
  Probe probe;
  auto result = function();
  const auto count = probe.count();
  test.Check(asc_test::ProcessAllocationCountMatches(count, 0),
             "printing must not allocate in an exact-observation lane");
  return result;
}

class Resource final : public asc::MemoryResource {
 public:
  [[nodiscard]] asc::MemorySpace space() const noexcept override {
    return asc::MemorySpace::kHost;
  }
  asc::Result<void*> Allocate(std::size_t bytes,
                              std::size_t alignment) override {
    ++attempts_;
    return host_.Allocate(bytes, alignment);
  }
  void Deallocate(void* pointer, std::size_t bytes,
                  std::size_t alignment) noexcept override {
    host_.Deallocate(pointer, bytes, alignment);
  }
  [[nodiscard]] std::size_t attempts() const noexcept { return attempts_; }

 private:
  asc::HostMemoryResource host_;
  std::size_t attempts_ = 0;
};

using asc_locale_test::LocaleGuard;
inline void LocaleControl(TestContext& test) {
  test.Check(asc_locale_test::LocaleControl(),
             "custom non-classic locale must affect its positive control");
}

struct FormatCase {
  asc::ArrayFloatFormat format;
  int precision;
  std::string_view positive;
  std::string_view negative;
  std::string_view negative_zero;
  std::string_view positive_zero;
};
inline constexpr std::array<FormatCase, 6> kCases{
    {{asc::ArrayFloatFormat::kGeneral, 1, "1", "-1e+01", "-0", "0"},
     {asc::ArrayFloatFormat::kGeneral, 32, "1.25", "-12.5", "-0", "0"},
     {asc::ArrayFloatFormat::kFixed, 1, "1.2", "-12.5", "-0.0", "0.0"},
     {asc::ArrayFloatFormat::kFixed, 32, "1.25000000000000000000000000000000",
      "-12.50000000000000000000000000000000",
      "-0.00000000000000000000000000000000",
      "0.00000000000000000000000000000000"},
     {asc::ArrayFloatFormat::kScientific, 1, "1.2e+00", "-1.2e+01", "-0.0e+00",
      "0.0e+00"},
     {asc::ArrayFloatFormat::kScientific, 32,
      "1.25000000000000000000000000000000e+00",
      "-1.25000000000000000000000000000000e+01",
      "-0.00000000000000000000000000000000e+00",
      "0.00000000000000000000000000000000e+00"}}};

template <typename T>
std::array<T, 5> Values() {
  if constexpr (std::is_floating_point_v<T>) {
    return {T{1.25}, T{-12.5}, -T{0}, std::numeric_limits<T>::infinity(),
            std::numeric_limits<T>::quiet_NaN()};
  } else {
    using Real = typename T::value_type;
    const auto infinity = std::numeric_limits<Real>::infinity();
    const auto nan = std::numeric_limits<Real>::quiet_NaN();
    return {T{1.25, -12.5}, T{-12.5, 1.25}, T{-Real{0}, Real{0}},
            T{infinity, -infinity}, T{nan, nan}};
  }
}

template <typename T>
std::array<std::string, 5> Tokens(const FormatCase& fixture) {
  if constexpr (std::is_floating_point_v<T>) {
    return {std::string(fixture.positive), std::string(fixture.negative),
            std::string(fixture.negative_zero), "inf", "nan"};
  } else {
    return {"(" + std::string(fixture.positive) + "," +
                std::string(fixture.negative) + ")",
            "(" + std::string(fixture.negative) + "," +
                std::string(fixture.positive) + ")",
            "(" + std::string(fixture.negative_zero) + "," +
                std::string(fixture.positive_zero) + ")",
            "(inf,-inf)", "(nan,nan)"};
  }
}

// Exact dyadic inputs make the rounding oracle independent of libstdc++ or
// ASC formatting. The tiny magnitude is exactly 13/131072; the literals below
// cover exponent carry at -4 and precision, ties-to-even and all32 precisions.
inline std::string TinyToken(asc::ArrayFloatFormat format, int precision) {
  constexpr std::array<std::string_view, 13> kGeneral{"0.0001",
                                                      "9.9e-05",
                                                      "9.92e-05",
                                                      "9.918e-05",
                                                      "9.9182e-05",
                                                      "9.91821e-05",
                                                      "9.918213e-05",
                                                      "9.9182129e-05",
                                                      "9.91821289e-05",
                                                      "9.918212891e-05",
                                                      "9.9182128906e-05",
                                                      "9.91821289062e-05",
                                                      "9.918212890625e-05"};
  constexpr std::array<std::string_view, 17> kFixed{"0.0",
                                                    "0.00",
                                                    "0.000",
                                                    "0.0001",
                                                    "0.00010",
                                                    "0.000099",
                                                    "0.0000992",
                                                    "0.00009918",
                                                    "0.000099182",
                                                    "0.0000991821",
                                                    "0.00009918213",
                                                    "0.000099182129",
                                                    "0.0000991821289",
                                                    "0.00009918212891",
                                                    "0.000099182128906",
                                                    "0.0000991821289062",
                                                    "0.00009918212890625"};
  const auto digits = static_cast<std::size_t>(precision);
  if (format == asc::ArrayFloatFormat::kGeneral) {
    return std::string(kGeneral[std::min(digits, kGeneral.size()) - 1]);
  }
  if (format == asc::ArrayFloatFormat::kFixed) {
    return digits <= kFixed.size()
               ? std::string(kFixed[digits - 1])
               : std::string(kFixed.back()) + std::string(digits - 17, '0');
  }
  if (digits < 12) {
    return std::string(kGeneral[digits]);
  }
  return "9.918212890625" + std::string(digits - 12, '0') + "e-05";
}

inline std::array<std::string, 4> RoundingRealTokens(
    asc::ArrayFloatFormat format, int precision) {
  const auto digits = static_cast<std::size_t>(precision);
  std::array<std::string, 4> tokens;
  if (format == asc::ArrayFloatFormat::kGeneral) {
    constexpr std::array<std::string_view, 3> kQuarter{"1", "1.2", "1.25"};
    constexpr std::array<std::string_view, 4> kEighth{"-1", "-1.4", "-1.38",
                                                      "-1.375"};
    tokens[0] = kQuarter[std::min(digits, kQuarter.size()) - 1];
    tokens[1] = kEighth[std::min(digits, kEighth.size()) - 1];
    tokens[2] = precision < 3 ? "1e+02" : "99.5";
  } else {
    tokens[0] = precision == 1 ? "1.2" : "1.25" + std::string(digits - 2, '0');
    if (precision < 3) {
      tokens[1] = precision == 1 ? "-1.4" : "-1.38";
    } else {
      tokens[1] = "-1.375" + std::string(digits - 3, '0');
    }
    if (format == asc::ArrayFloatFormat::kScientific) {
      tokens[0] += "e+00";
      tokens[1] += "e+00";
      tokens[2] = precision == 1
                      ? "1.0e+02"
                      : "9.95" + std::string(digits - 2, '0') + "e+01";
    } else {
      tokens[2] = "99.5" + std::string(digits - 1, '0');
    }
  }
  tokens[3] = "-" + TinyToken(format, precision);
  return tokens;
}

template <typename T>
std::array<T, 4> RoundingValues() {
  if constexpr (std::is_floating_point_v<T>) {
    return {T{1.25}, T{-1.375}, T{99.5}, T{-0x1.ap-14}};
  } else {
    return {T{1.25, -1.375}, T{-1.375, 1.25}, T{99.5, -0x1.ap-14},
            T{-0x1.ap-14, 99.5}};
  }
}

template <typename T>
std::string RoundingExpected(asc::ArrayFloatFormat format, int precision,
                             bool sparse) {
  auto tokens = RoundingRealTokens(format, precision);
  if constexpr (!std::is_floating_point_v<T>) {
    const auto real = tokens;
    tokens = {"(" + real[0] + "," + real[1] + ")",
              "(" + real[1] + "," + real[0] + ")",
              "(" + real[2] + "," + real[3] + ")",
              "(" + real[3] + "," + real[2] + ")"};
  }
  constexpr std::array<std::string_view, 4> kCoordinates{
      "(0,0) = ", "(1,1) = ", "(2,2) = ", "(3,3) = "};
  std::string output = sparse ? "" : "[";
  for (std::size_t i = 0; i < tokens.size(); ++i) {
    if (sparse) {
      output += kCoordinates[i];
    } else if (i != 0) {
      output += ", ";
    }
    output += tokens[i];
    if (sparse) {
      output += '\n';
    }
  }
  if (!sparse) {
    output += "]\n";
  }
  return output;
}

// Independent literal outputs determine all three rank-zero budget regions:
// too small for the required header/marker, omission only, and one full value.
// No production formatter or preview helper constructs these expectations.
inline void CheckRankZeroBudget(TestContext& test, const asc::Status& status,
                                const Sink& sink,
                                const asc::ArrayPrintReport& report,
                                std::size_t budget, std::string_view complete,
                                std::string_view omitted) {
  test.Check(omitted.size() < complete.size(),
             "rank-zero fixture must have an omission-only budget region");
  if (budget < omitted.size()) {
    test.Check(status.code() == asc::ErrorCode::kAllocation,
               "insufficient rank-zero header/marker budget must fail with "
               "the size/resource error");
    test.Check(sink.text().empty(),
               "rank-zero minimum-budget failure must precede all output");
    asc_array_display_test::CheckReset(test, report);
    return;
  }
  const bool truncated = budget < complete.size();
  const std::string_view expected = truncated ? omitted : complete;
  test.Check(status.ok(), "every sufficient rank-zero budget must succeed");
  test.Check(sink.text() == expected,
             "every rank-zero budget must have its independent literal layout");
  test.Check(report.output_bytes == expected.size() &&
                 report.values_displayed == (truncated ? 0 : 1) &&
                 report.truncated == truncated,
             "every rank-zero budget must retain its exact three-field report");
}

// A sink failure preserves the preview decision already reached. A complete
// preview has made no omission decision. For an omission-only preview, failure
// during the metadata line precedes that decision; failure at/after its end
// retains true even when no marker byte was accepted.
inline void CheckRankZeroFailureTruncation(TestContext& test,
                                           const asc::ArrayPrintReport& report,
                                           bool omission_preview,
                                           std::size_t header_bytes) {
  const bool expected = omission_preview && report.output_bytes >= header_bytes;
  test.Check(
      report.truncated == expected,
      "rank-zero sink failure must retain the completed preview decision");
}

template <typename Object, typename Print>
void ObjectAliases(TestContext& test, Object& object, Print print) {
  const auto storage = std::as_writable_bytes(std::span(&object, 1));
  const std::vector<std::byte> before(storage.begin(), storage.end());
  for (const auto bytes : {storage, storage.first(1), storage.last(1)}) {
    Sink sink;
    asc::ArrayPrintReport report{777, 888, true};
    const auto status = print(sink, bytes, report);
    test.Check(status.code() == asc::ErrorCode::kInvalidArgument &&
                   sink.text().empty(),
               "live owner/view object scratch must reject before output");
    test.Check(
        std::equal(before.begin(), before.end(), storage.begin()),
        "preflight must preserve the complete live object representation");
    asc_array_display_test::CheckReset(test, report);
  }
}

}  // namespace asc_array_display_modes_test
#endif  // ASC_TESTS_ARRAY_DISPLAY_MODES_TEST_SUPPORT_H_
