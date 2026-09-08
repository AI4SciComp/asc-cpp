#ifndef ASC_TESTS_ARRAY_DISPLAY_MODES_TEST_SUPPORT_H_
#define ASC_TESTS_ARRAY_DISPLAY_MODES_TEST_SUPPORT_H_

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <locale>
#include <span>
#include <sstream>
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

// An unnamed custom C++ locale avoids dependence on optional OS locale packs.
// The positive ostream control proves the active decimal/grouping behavior;
// this does not claim a named C setlocale locale was installed.
class CommaPunctuation final : public std::numpunct<char> {
 protected:
  [[nodiscard]] char do_decimal_point() const override { return ','; }
  [[nodiscard]] char do_thousands_sep() const override { return '.'; }
  [[nodiscard]] std::string do_grouping() const override { return "\3"; }
};
class LocaleGuard {
 public:
  LocaleGuard()
      : original_(std::locale::global(
            std::locale(std::locale::classic(), new CommaPunctuation))) {}
  ~LocaleGuard() { std::locale::global(original_); }
  LocaleGuard(const LocaleGuard&) = delete;
  LocaleGuard& operator=(const LocaleGuard&) = delete;
  LocaleGuard(LocaleGuard&&) = delete;
  LocaleGuard& operator=(LocaleGuard&&) = delete;

 private:
  std::locale original_;
};
inline void LocaleControl(TestContext& test) {
  std::ostringstream stream;
  stream << 1234.5;
  test.Check(stream.str() == "1.234,5",
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
