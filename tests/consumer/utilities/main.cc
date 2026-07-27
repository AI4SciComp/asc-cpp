#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

#include "asc/utilities.h"

int main() {
  asc::ConfigurationSchema schema(asc::ConfigurationValueType::kObject);
  asc::ConfigurationSchema count(asc::ConfigurationValueType::kSignedInteger);
  count.SetDefault(asc::ConfigurationValue(std::int64_t{1}));
  if (!schema.AddField("count", std::move(count)).ok()) {
    return 1;
  }

  auto parser = asc::CommandLineParser::Create(
      std::move(schema), {{"count", 'c', "/count", "COUNT", "Set the count."}});
  if (!parser.ok()) {
    return 2;
  }
  const std::vector<std::string_view> arguments = {"--count=-4"};
  const auto parsed = parser->Parse(arguments);
  if (!parsed.ok()) {
    return 3;
  }
  const auto count_value = parsed->configuration.Find("/count");
  if (!count_value.ok() ||
      *count_value->get().AsSignedInteger() != std::int64_t{-4}) {
    return 4;
  }

  asc::Timer timer;
  if (!timer.Start().ok()) {
    return 5;
  }
  const auto interval = timer.Stop();
  return interval.ok() && *interval >= asc::Timer::Duration::zero() ? 0 : 6;
}
