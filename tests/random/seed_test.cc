#include "asc/random/seed.h"

#include <cstdint>
#include <limits>
#include <random>
#include <type_traits>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "test_support.h"

namespace {

using SeedFunction = asc::Result<std::uint64_t> (*)(std::random_device& source);
static_assert(
    std::is_same_v<decltype(&asc::AcquireNondeterministicSeed), SeedFunction>);

void CheckExplicitAcquisition(asc_random_test::TestContext& context) {
  std::random_device source;
  const auto seed = asc::AcquireNondeterministicSeed(source);
  const bool full_width =
      source.min() == 0 &&
      source.max() == std::numeric_limits<std::uint32_t>::max();
  if (!full_width) {
    ASC_RANDOM_TEST_EQ(context, seed.status().code(),
                       asc::ErrorCode::kUnsupported);
    return;
  }
  if (!seed.ok()) {
    ASC_RANDOM_TEST_EQ(context, seed.status().code(),
                       asc::ErrorCode::kUnavailable);
  }
}

}  // namespace

int main() {
  asc_random_test::TestContext context;
  CheckExplicitAcquisition(context);
  return context.Finish();
}
