#include <bit>
#include <cfenv>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>

#include "allocation_observation.h"
#include "allocation_probe.h"
#include "array_parse_cases.h"
#include "asc/core/array_io.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "test_support.h"

namespace {

using asc_core_test::TestContext;

template <typename T>
using Bits = std::conditional_t<sizeof(T) == 4, std::uint32_t, std::uint64_t>;

template <typename T>
asc::Result<T> Parse(std::string_view token) {
  if constexpr (std::is_same_v<T, float>) {
    return asc::internal_array_io::ParseFiniteFloat(token);
  } else {
    return asc::internal_array_io::ParseFiniteDouble(token);
  }
}

template <typename T>
void Expect(TestContext& test, std::string_view token, std::uint64_t bits,
            bool overflow = false) {
  asc_sparse_test::AllocationProbe allocations;
  auto parsed = Parse<T>(token);
  const auto count = allocations.count();
  ASC_TEST_EQ(test, parsed.ok(), !overflow);
  if (overflow) {
    ASC_TEST_EQ(test, parsed.status().code(), asc::ErrorCode::kOverflow);
  } else if (parsed.ok()) {
    ASC_TEST_EQ(test, std::bit_cast<Bits<T>>(*parsed),
                static_cast<Bits<T>>(bits));
  }
  ASC_TEST_CHECK(test, asc_test::ProcessAllocationCountMatches(count, 0));
}

template <typename T, std::size_t N>
void Boundaries(TestContext& test,
                const asc_core_test::DecimalCase (&cases)[N]) {
  constexpr auto kSign = std::uint64_t{1} << (sizeof(T) * 8 - 1);
  for (const auto& value : cases) {
    Expect<T>(test, value.token, value.bits, value.overflow);
    const auto negative = std::string("-") + std::string(value.token);
    Expect<T>(test, negative, value.bits | kSign, value.overflow);
  }
}

void LongSuffixes(TestContext& test) {
  const std::string zeros(1200, '0');
  const std::string nines(1200, '9');
  constexpr std::string_view kFloatHalf = "1.000000059604644775390625";
  constexpr std::string_view kDoubleHalf =
      "1.00000000000000011102230246251565404236316680908203125";
  Expect<float>(test, std::string(kFloatHalf) + zeros, 0x3f800000);
  Expect<float>(test, std::string(kFloatHalf) + zeros + "1", 0x3f800001);
  Expect<float>(test, "1.000000059604644775390624" + nines, 0x3f800000);
  Expect<double>(test, std::string(kDoubleHalf) + zeros, 0x3ff0000000000000ULL);
  Expect<double>(test, std::string(kDoubleHalf) + zeros + "1",
                 0x3ff0000000000001ULL);
  Expect<double>(
      test, "1.00000000000000011102230246251565404236316680908203124" + nines,
      0x3ff0000000000000ULL);
  Expect<double>(test, "0." + zeros + "1e1201", 0x3ff0000000000000ULL);
  Expect<double>(test, "1" + zeros + "e-1200", 0x3ff0000000000000ULL);
  Expect<double>(test, "-0e999999999999999999999999", 0x8000000000000000ULL);
  Expect<double>(test, "0.0e-999999999999999999999999", 0);
  Expect<double>(test, "1e999999999999999999999999", 0, true);
  Expect<double>(test, "1e-999999999999999999999999", 0, true);
}

template <typename T>
void Differential(TestContext& test) {
  // Secondary fidelity check only. Exact independently generated midpoint
  // fixtures above are the mathematical rounding oracle on every platform.
  if constexpr (requires(const char* first, const char* last, T& value) {
                  std::from_chars(first, last, value);
                }) {
    std::uint64_t state = 0x76c3a59ef48126b1ULL;
    for (int iteration = 0; iteration < 2000; ++iteration) {
      state ^= state << 13;
      state ^= state >> 7;
      state ^= state << 17;
      const auto token =
          std::to_string(state) + "e" + std::to_string(iteration % 740 - 390);
      T expected{};
      const auto reference =
          std::from_chars(token.data(), token.data() + token.size(), expected);
      const bool overflow = reference.ec == std::errc::result_out_of_range;
      Expect<T>(test, token, std::bit_cast<Bits<T>>(expected), overflow);
    }
  }
}

void Invalid(TestContext& test) {
  for (const std::string_view token :
       {"", "+", "-", ".", "1e", "1e+", "inf", "nan", "0x1p0", "1,5", " 1",
        "1 ", "1..0"}) {
    auto parsed = Parse<double>(token);
    ASC_TEST_CHECK(test, !parsed.ok());
    ASC_TEST_EQ(test, parsed.status().code(), asc::ErrorCode::kEncoding);
  }
}

}  // namespace

int main() {
  TestContext test;
  const int previous = std::fegetround();
  for (int mode : {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO}) {
    ASC_TEST_EQ(test, std::fesetround(mode), 0);
    Boundaries<float>(test, asc_core_test::kFloatDecimalCases);
    Boundaries<double>(test, asc_core_test::kDoubleDecimalCases);
    LongSuffixes(test);
  }
  ASC_TEST_EQ(test, std::fesetround(previous), 0);
  Differential<float>(test);
  Differential<double>(test);
  Invalid(test);
  return test.Finish();
}
