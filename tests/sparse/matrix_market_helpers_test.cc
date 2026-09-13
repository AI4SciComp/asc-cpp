#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "allocation_observation.h"
#include "allocation_probe.h"
#include "asc/core/array_format.h"
#include "asc/core/array_io.h"
#include "asc/core/io.h"
#include "asc/core/matrix_market.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "test_support.h"

namespace {

using asc::MatrixMarketField;
using asc::MatrixMarketPatternPolicy;
using asc_sparse_test::TestContext;
namespace mm = asc::internal_matrix_market;

class Source final : public asc::ByteSource {
 public:
  explicit Source(std::string_view input) : input_(input) {}
  asc::Result<std::size_t> ReadSome(std::span<std::byte> output) override {
    if (position == fail_after) {
      return std::move(failure);
    }
    const auto count = std::min(
        {output.size(), input_.size() - position, fail_after - position});
    for (std::size_t i = 0; i < count; ++i) {
      output[i] = static_cast<std::byte>(input_[position + i]);
    }
    position += count;
    return count;
  }
  std::size_t position = 0;
  std::size_t fail_after = std::numeric_limits<std::size_t>::max();
  asc::Status failure{asc::ErrorCode::kIo};

 private:
  std::string_view input_;
};

class Sink final : public asc::ByteSink {
 public:
  asc::Result<std::size_t> WriteSome(
      std::span<const std::byte> input) override {
    if (size == fail_after) {
      return std::move(failure);
    }
    const auto count = std::min(
        {input.size(), bytes.size() - size, fail_after - size, std::size_t{2}});
    std::copy_n(input.begin(), count, bytes.begin() + size);
    size += count;
    return count;
  }
  [[nodiscard]] std::string_view text() const {
    return {reinterpret_cast<const char*>(bytes.data()), size};
  }
  std::array<std::byte, 512> bytes{};
  std::size_t size = 0;
  std::size_t fail_after = std::numeric_limits<std::size_t>::max();
  asc::Status failure{asc::ErrorCode::kIo};
};

template <typename T>
auto Integer(std::string_view token, std::size_t cap = 512) {
  const std::array<std::string_view, 1> tokens{token};
  return mm::ParseValue<T>(MatrixMarketField::kInteger, tokens,
                           MatrixMarketPatternPolicy::kUnspecified, cap);
}

template <typename T>
void Exact(TestContext& test, std::string_view token, T expected) {
  asc_sparse_test::AllocationProbe probe;
  const auto value = Integer<T>(token);
  ASC_SPARSE_TEST_CHECK(test, value.ok());
  if (value.ok()) {
    ASC_SPARSE_TEST_EQ(test, *value, expected);
  }
  ASC_SPARSE_TEST_CHECK(
      test, asc_test::ProcessAllocationCountMatches(probe.count(), 0));
}

template <typename T>
void Inexact(TestContext& test, std::string_view token) {
  asc_sparse_test::AllocationProbe probe;
  const auto value = Integer<T>(token);
  ASC_SPARSE_TEST_CHECK(test, !value.ok());
  ASC_SPARSE_TEST_CHECK(test,
                        value.status().code() == asc::ErrorCode::kOverflow);
  ASC_SPARSE_TEST_CHECK(
      test, asc_test::ProcessAllocationCountMatches(probe.count(), 0));
}

void IntegerExactness(TestContext& test) {
  Exact<float>(test, "16777216", 0x1p24F);
  Inexact<float>(test, "16777217");
  Exact<float>(test, "16777218", 16777218.0F);
  Exact<double>(test, "9007199254740992", 0x1p53);
  Inexact<double>(test, "9007199254740993");
  Exact<double>(test, "9007199254740994", 9007199254740994.0);
  Exact<float>(test, "1267650600228229401496703205376", 0x1p100F);
  Exact<double>(test, "1267650600228229401496703205376", 0x1p100);
  Inexact<double>(test, "1267650600228229401496703205377");
  Exact<float>(test, "340282346638528859811704183484516925440",
               std::numeric_limits<float>::max());
  Inexact<float>(test, "340282366920938463463374607431768211456");
  Exact<std::int64_t>(test, "-9223372036854775808",
                      std::numeric_limits<std::int64_t>::min());
  Exact<std::uint64_t>(test, "18446744073709551615",
                       std::numeric_limits<std::uint64_t>::max());
  Exact<double>(test, "-9223372036854775808", -0x1p63);
  Inexact<std::int64_t>(test, "9223372036854775808");
  ASC_SPARSE_TEST_CHECK(test, !Integer<std::uint64_t>("-0").ok());
  ASC_SPARSE_TEST_CHECK(test, !Integer<double>("100", 2).ok());
  ASC_SPARSE_TEST_CHECK(test, !Integer<double>("1e2").ok());
  const auto negative_zero = Integer<double>("-0");
  ASC_SPARSE_TEST_CHECK(test, negative_zero.ok());
  ASC_SPARSE_TEST_CHECK(test, std::signbit(*negative_zero));
}

void LargeIntegers(TestContext& test) {
  // Decimal fixtures computed with independent Python integers; expectations
  // use hexadecimal C++ floating constants, not the conversion under test.
  constexpr std::string_view kPower1023 =
      "898846567431157953864652595394512366808988489471153286367150405788663379"
      "027504815663542386612037680105600569399356966788293948844072083112464237"
      "153197370621888839467124327426381511098006230470597265414760425028844190"
      "753411712314407369565552704136185816752553422931491199736229692398581524"
      "17678164812112068608";
  constexpr std::string_view kMaximum =
      "179769313486231570814527423731704356798070567525844996598917476803157260"
      "780028538760589558632766878171540458953514382464234321326889464182768467"
      "546703537516986049910576551282076245490090389328944075868508455133942304"
      "583236903222948165808559332123348274797826204144723168738177180919299881"
      "250404026184124858368";
  constexpr std::string_view kOverflow =
      "179769313486231590772930519078902473361797697894230657273430081157732675"
      "805500963132708477322407536021120113879871393357658789768814416622492847"
      "430639474124377767893424865485276302219601246094119453082952085005768838"
      "150682342462881473913110540827237163350510684586298239947245938479716304"
      "835356329624224137216";
  Exact<double>(test, kPower1023, 0x1p1023);
  Exact<double>(test, kMaximum, std::numeric_limits<double>::max());
  Inexact<double>(test, kOverflow);
}

template <typename T>
void ScalarPolicies(TestContext& test) {
  constexpr auto kNone = MatrixMarketPatternPolicy::kUnspecified;
  constexpr auto kUnit = MatrixMarketPatternPolicy::kUnit;
  const std::array<std::string_view, 1> one{"1"};
  const std::array<std::string_view, 2> complex{"2", "-3"};
  asc_sparse_test::AllocationProbe probe;
  ASC_SPARSE_TEST_CHECK(
      test,
      !mm::ParseValue<T>(MatrixMarketField::kPattern, {}, kNone, 32).ok());
  const auto pattern =
      mm::ParseValue<T>(MatrixMarketField::kPattern, {}, kUnit, 32);
  ASC_SPARSE_TEST_CHECK(test, pattern.ok());
  ASC_SPARSE_TEST_EQ(test, *pattern, T{1});
  const auto real = mm::ParseValue<T>(MatrixMarketField::kReal, one, kNone, 32);
  ASC_SPARSE_TEST_CHECK(test, real.ok() == !std::is_integral_v<T>);
  const auto pair =
      mm::ParseValue<T>(MatrixMarketField::kComplex, complex, kNone, 32);
  ASC_SPARSE_TEST_CHECK(test,
                        pair.ok() == asc::internal_array_format::kComplex<T>);
  if constexpr (asc::internal_array_format::kComplex<T>) {
    ASC_SPARSE_TEST_EQ(test, *pair, T(2, -3));
    ASC_SPARSE_TEST_EQ(test, real->imag(), 0);
    ASC_SPARSE_TEST_CHECK(test, !std::signbit(real->imag()));
  }
  Sink sink;
  asc::ArrayIoReport report;
  asc::internal_array_io::Output output(sink, 512, report);
  std::array<std::byte, 64> scratch{};
  ASC_SPARSE_TEST_CHECK(
      test,
      mm::WriteValue(T{1}, mm::ScalarField<T>(), scratch, 64, output).ok());
  ASC_SPARSE_TEST_EQ(test, sink.text(),
                     asc::internal_array_format::kComplex<T> ? "1 0" : "1");
  ASC_SPARSE_TEST_EQ(test, report.output_bytes, sink.text().size());
  ASC_SPARSE_TEST_CHECK(test, !mm::WriteValue(T{2}, MatrixMarketField::kPattern,
                                              scratch, 64, output)
                                   .ok());
  ASC_SPARSE_TEST_CHECK(
      test, asc_test::ProcessAllocationCountMatches(probe.count(), 0));
}

void Arithmetic(TestContext& test) {
  asc_sparse_test::AllocationProbe probe;
  ASC_SPARSE_TEST_CHECK(test,
                        !mm::CheckedSum(std::int8_t{127}, std::int8_t{1}).ok());
  ASC_SPARSE_TEST_CHECK(
      test, !mm::CheckedSum(std::uint8_t{255}, std::uint8_t{1}).ok());
  ASC_SPARSE_TEST_CHECK(test,
                        !mm::CheckedSum(std::numeric_limits<double>::max(),
                                        std::numeric_limits<double>::max())
                             .ok());
  ASC_SPARSE_TEST_CHECK(
      test, !mm::CheckedNegate(std::numeric_limits<std::int64_t>::min()).ok());
  ASC_SPARSE_TEST_CHECK(test, !mm::CheckedNegate(std::uint64_t{1}).ok());
  ASC_SPARSE_TEST_EQ(test, *mm::CheckedNegate(std::uint64_t{0}),
                     std::uint64_t{0});
  const auto sum =
      mm::CheckedSum(std::complex<double>(1, -2), std::complex<double>(-1, 3));
  ASC_SPARSE_TEST_EQ(test, *sum, std::complex<double>(0, 1));
  for (const auto* text : {"inf", "-inf", "nan", "1e999", "1e-999"}) {
    const std::array<std::string_view, 1> token{text};
    ASC_SPARSE_TEST_CHECK(
        test,
        !mm::ParseValue<double>(MatrixMarketField::kReal, token,
                                MatrixMarketPatternPolicy::kUnspecified, 32)
             .ok());
  }
  ASC_SPARSE_TEST_CHECK(
      test, asc_test::ProcessAllocationCountMatches(probe.count(), 0));
}

void Records(TestContext& test) {
  Source source(
      "\t%%MatrixMarket MATRIX coordinate ReAl general\t\r\n"
      "% comment longer than token and scratch capacity: "
      "abcdefghijklmnopqrstuvwxyz\n"
      " \t\r\n2\t3 1\n  1\t2 3\t\n% final comment");
  asc::ArrayIoLimits limits;
  limits.max_token_bytes = 1;
  asc::ArrayIoReport report;
  mm::Input input(source, limits, report);
  std::array<std::byte, 48> scratch{};
  asc_sparse_test::AllocationProbe probe;
  auto banner = input.FirstRecord(scratch);
  ASC_SPARSE_TEST_CHECK(test, banner.ok());
  ASC_SPARSE_TEST_EQ(test, banner->count, std::size_t{5});
  ASC_SPARSE_TEST_EQ(test, banner->fields[0], "%%MatrixMarket");
  ASC_SPARSE_TEST_CHECK(test, mm::WordEquals(banner->fields[1], "matrix"));
  ASC_SPARSE_TEST_CHECK(test, mm::ParseField(banner->fields[3]).ok());
  auto dimensions = input.NextRecord(scratch);
  ASC_SPARSE_TEST_CHECK(test, dimensions.ok());
  ASC_SPARSE_TEST_EQ(test, dimensions->count, std::size_t{3});
  auto value = input.NextRecord(scratch);
  ASC_SPARSE_TEST_CHECK(test, value.ok());
  ASC_SPARSE_TEST_EQ(test, value->fields[2], "3");
  ASC_SPARSE_TEST_CHECK(test, input.Finish(scratch).ok());
  ASC_SPARSE_TEST_EQ(test, report.input_bytes, source.position);
  ASC_SPARSE_TEST_CHECK(
      test, asc_test::ProcessAllocationCountMatches(probe.count(), 0));
}

void RecordFailures(TestContext& test) {
  constexpr std::array<std::string_view, 7> kMalformed{
      "1\r2",
      "1 2 3 4 5 6\n",
      "1\n2\n",
      std::string_view("1\0\n", 3),
      "1\x80\n",
      "1\v\n",
      "1\n%%MatrixMarket matrix array real general\n"};
  for (const auto text : kMalformed) {
    Source source(text);
    asc::ArrayIoLimits limits;
    asc::ArrayIoReport report;
    mm::Input input(source, limits, report);
    std::array<std::byte, 48> scratch{};
    asc_sparse_test::AllocationProbe probe;
    const auto record = input.NextRecord(scratch);
    ASC_SPARSE_TEST_CHECK(test, !record.ok() || !input.Finish(scratch).ok());
    ASC_SPARSE_TEST_EQ(test, report.input_bytes, source.position);
    ASC_SPARSE_TEST_CHECK(
        test, asc_test::ProcessAllocationCountMatches(probe.count(), 0));
  }
  for (std::size_t cap = 0; cap <= 4; ++cap) {
    Source source("1 2");
    asc::ArrayIoLimits limits;
    limits.max_input_bytes = cap;
    asc::ArrayIoReport report;
    mm::Input input(source, limits, report);
    std::array<std::byte, 8> scratch{};
    const auto record = input.NextRecord(scratch);
    ASC_SPARSE_TEST_CHECK(test, record.ok() == (cap == 4));
    ASC_SPARSE_TEST_CHECK(test, report.input_bytes <= cap);
  }
}

void LongFailures(TestContext& test) {
  const std::string message(256, 'm');
  for (std::size_t fail = 0; fail != 4; ++fail) {
    Source source("1 2\n");
    source.fail_after = fail;
    source.failure = asc::Status(asc::ErrorCode::kIo, message, message, 73);
    asc::ArrayIoReport report;
    mm::Input input(source, {}, report);
    std::array<std::byte, 48> scratch{};
    asc_sparse_test::AllocationProbe probe;
    const auto record = input.NextRecord(scratch);
    ASC_SPARSE_TEST_CHECK(test, !record.ok());
    ASC_SPARSE_TEST_EQ(test, record.status().native_code(), 73);
    ASC_SPARSE_TEST_EQ(test, report.input_bytes, fail);
    ASC_SPARSE_TEST_CHECK(test, record.status().message().empty());
    ASC_SPARSE_TEST_CHECK(
        test, asc_test::ProcessAllocationCountMatches(probe.count(), 0));
  }
  Sink sink;
  sink.fail_after = 1;
  sink.failure = asc::Status(asc::ErrorCode::kIo, message, message, 91);
  asc::ArrayIoReport report;
  asc::internal_array_io::Output output(sink, 512, report);
  std::array<std::byte, 128> scratch{};
  asc_sparse_test::AllocationProbe probe;
  const auto status =
      mm::WriteValue(std::complex<double>(2, 3), MatrixMarketField::kComplex,
                     scratch, 128, output);
  ASC_SPARSE_TEST_CHECK(test, !status.ok());
  ASC_SPARSE_TEST_EQ(test, status.native_code(), 91);
  ASC_SPARSE_TEST_EQ(test, report.output_bytes, std::size_t{1});
  ASC_SPARSE_TEST_CHECK(
      test, asc_test::ProcessAllocationCountMatches(probe.count(), 0));
}

}  // namespace

int main() {
  TestContext test;
  IntegerExactness(test);
  LargeIntegers(test);
  ScalarPolicies<std::int8_t>(test);
  ScalarPolicies<std::uint8_t>(test);
  ScalarPolicies<std::int16_t>(test);
  ScalarPolicies<std::uint16_t>(test);
  ScalarPolicies<std::int32_t>(test);
  ScalarPolicies<std::uint32_t>(test);
  ScalarPolicies<std::int64_t>(test);
  ScalarPolicies<std::uint64_t>(test);
  ScalarPolicies<float>(test);
  ScalarPolicies<double>(test);
  ScalarPolicies<std::complex<float>>(test);
  ScalarPolicies<std::complex<double>>(test);
  Arithmetic(test);
  Records(test);
  RecordFailures(test);
  LongFailures(test);
  std::cout << "matrix_market_helpers scalar_types=12\n";
  return test.Finish();
}
