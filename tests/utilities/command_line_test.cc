#include "asc/utilities/command_line.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "asc/core/configuration.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "test_support.h"

namespace {

using asc::CommandLineOption;
using asc::ConfigurationSchema;
using asc::ConfigurationValue;
using asc::ConfigurationValueType;
using asc_utilities_test::TestContext;

ConfigurationValue Utf8(std::string value) {
  auto result = ConfigurationValue::Utf8String(std::move(value));
  if (!result.ok()) {
    return {};
  }
  return std::move(*result);
}

ConfigurationSchema MakeSchema(TestContext& context) {
  ConfigurationSchema root(ConfigurationValueType::kObject);

  ConfigurationSchema flag(ConfigurationValueType::kBool);
  flag.SetDefault(ConfigurationValue(false));
  ASC_UTILITIES_TEST_CHECK(context,
                           root.AddField("flag", std::move(flag)).ok());

  ConfigurationSchema count(ConfigurationValueType::kSignedInteger);
  count.SetDefault(ConfigurationValue(std::int64_t{0}));
  ASC_UTILITIES_TEST_CHECK(context, count.SetSignedBounds(-100, 100).ok());
  ASC_UTILITIES_TEST_CHECK(context,
                           root.AddField("count", std::move(count)).ok());

  ConfigurationSchema wide_signed(ConfigurationValueType::kSignedInteger);
  wide_signed.SetDefault(ConfigurationValue(std::int64_t{0}));
  ASC_UTILITIES_TEST_CHECK(
      context, root.AddField("wide-signed", std::move(wide_signed)).ok());

  ConfigurationSchema size(ConfigurationValueType::kUnsignedInteger);
  size.SetDefault(ConfigurationValue(std::uint64_t{1}));
  ASC_UTILITIES_TEST_CHECK(context,
                           root.AddField("size", std::move(size)).ok());

  ConfigurationSchema ratio(ConfigurationValueType::kDouble);
  ratio.SetDefault(ConfigurationValue(1.0));
  ASC_UTILITIES_TEST_CHECK(context, ratio.SetDoubleBounds(-10.0, 10.0).ok());
  ASC_UTILITIES_TEST_CHECK(context,
                           root.AddField("ratio", std::move(ratio)).ok());

  ConfigurationSchema name(ConfigurationValueType::kString);
  name.SetDefault(Utf8("default"));
  ASC_UTILITIES_TEST_CHECK(context, name.SetSizeBounds(1, 16).ok());
  ASC_UTILITIES_TEST_CHECK(context,
                           root.AddField("name", std::move(name)).ok());

  ConfigurationSchema secret(ConfigurationValueType::kString);
  secret.SetDefault(Utf8("safe"));
  secret.SetSensitive(true);
  ASC_UTILITIES_TEST_CHECK(context, secret.SetSizeBounds(1, 4).ok());
  ASC_UTILITIES_TEST_CHECK(context,
                           root.AddField("secret", std::move(secret)).ok());

  ConfigurationSchema group(ConfigurationValueType::kObject);
  ConfigurationSchema nested(ConfigurationValueType::kSignedInteger);
  nested.SetDefault(ConfigurationValue(std::int64_t{4}));
  ASC_UTILITIES_TEST_CHECK(context,
                           group.AddField("value", std::move(nested)).ok());
  ASC_UTILITIES_TEST_CHECK(context,
                           root.AddField("group", std::move(group)).ok());

  ASC_UTILITIES_TEST_CHECK(
      context,
      root.AddField("list", ConfigurationSchema(ConfigurationValueType::kList))
          .ok());
  ASC_UTILITIES_TEST_CHECK(
      context,
      root.AddField("null", ConfigurationSchema(ConfigurationValueType::kNull))
          .ok());
  return root;
}

std::vector<CommandLineOption> MakeOptions() {
  return {
      {"flag", 'f', "/flag", "", "Enable the flag."},
      {"count", 'c', "/count", "COUNT", "Set a bounded count."},
      {"wide-signed", 'w', "/wide-signed", "INTEGER",
       "Set any signed 64-bit integer."},
      {"size", 'z', "/size", "SIZE", "Set an unsigned size."},
      {"ratio", 'r', "/ratio", "RATIO", "Set a floating ratio."},
      {"name", 'n', "/name", "NAME", "Set a UTF-8 name."},
      {"secret", 's', "/secret", "SECRET", "Set a sensitive value."},
      {"nested", 'v', "/group/value", "VALUE", "Set a nested value."},
  };
}

asc::Result<asc::CommandLineParser> MakeParser(TestContext& context) {
  auto options = MakeOptions();
  auto parser = asc::CommandLineParser::Create(
      MakeSchema(context), std::span<const CommandLineOption>(options));
  ASC_UTILITIES_TEST_CHECK(context, parser.ok());
  return parser;
}

template <std::size_t Size>
asc::Result<asc::CommandLineParseResult> Parse(
    const asc::CommandLineParser& parser,
    const std::array<std::string_view, Size>& arguments) {
  return parser.Parse(std::span<const std::string_view>(arguments));
}

void CheckCreation(TestContext& context) {
  auto options = MakeOptions();
  auto parser = asc::CommandLineParser::Create(
      MakeSchema(context), std::span<const CommandLineOption>(options));
  ASC_UTILITIES_TEST_CHECK(context, parser.ok());
  if (!parser.ok()) {
    return;
  }

  options.front().long_name = "mutated";
  ASC_UTILITIES_TEST_EQ(context, parser->options().front().long_name,
                        std::string("flag"));

  auto duplicate_long = MakeOptions();
  duplicate_long[1].long_name = duplicate_long[0].long_name;
  ASC_UTILITIES_TEST_CHECK(
      context, !asc::CommandLineParser::Create(
                    MakeSchema(context),
                    std::span<const CommandLineOption>(duplicate_long))
                    .ok());

  auto duplicate_short = MakeOptions();
  duplicate_short[1].short_name = duplicate_short[0].short_name;
  ASC_UTILITIES_TEST_CHECK(
      context, !asc::CommandLineParser::Create(
                    MakeSchema(context),
                    std::span<const CommandLineOption>(duplicate_short))
                    .ok());

  auto duplicate_destination = MakeOptions();
  duplicate_destination[1].destination = duplicate_destination[0].destination;
  ASC_UTILITIES_TEST_CHECK(
      context, !asc::CommandLineParser::Create(
                    MakeSchema(context),
                    std::span<const CommandLineOption>(duplicate_destination))
                    .ok());

  const std::vector<std::string> invalid_names = {
      "", "--bad", "-bad", "bad name", std::string("bad\xff", 4)};
  for (const std::string& invalid_name : invalid_names) {
    auto invalid = MakeOptions();
    invalid[0].long_name = invalid_name;
    ASC_UTILITIES_TEST_CHECK(
        context,
        !asc::CommandLineParser::Create(
             MakeSchema(context), std::span<const CommandLineOption>(invalid))
             .ok());
  }

  for (char invalid_short : {'-', '\0', ' ', static_cast<char>(0xff)}) {
    auto invalid = MakeOptions();
    invalid[0].short_name = invalid_short;
    ASC_UTILITIES_TEST_CHECK(
        context,
        !asc::CommandLineParser::Create(
             MakeSchema(context), std::span<const CommandLineOption>(invalid))
             .ok());
  }

  for (std::string destination :
       {"/group", "/list", "/null", "/missing", "missing-slash"}) {
    const std::array unsupported = {
        CommandLineOption{"unsupported", std::nullopt, std::move(destination),
                          "VALUE", "Unsupported."},
    };
    const auto result = asc::CommandLineParser::Create(
        MakeSchema(context), std::span<const CommandLineOption>(unsupported));
    ASC_UTILITIES_TEST_CHECK(context, !result.ok());
  }

  const std::array object_option = {
      CommandLineOption{"object", std::nullopt, "/group", "VALUE", "Object."},
  };
  const auto unsupported = asc::CommandLineParser::Create(
      MakeSchema(context), std::span<const CommandLineOption>(object_option));
  ASC_UTILITIES_TEST_EQ(context, unsupported.status().code(),
                        asc::ErrorCode::kUnsupported);
}

void CheckBasicParsing(TestContext& context) {
  auto parser_result = MakeParser(context);
  if (!parser_result.ok()) {
    return;
  }
  const asc::CommandLineParser& parser = *parser_result;

  const auto parsed =
      Parse(parser, std::array<std::string_view, 10>{
                        "input-a", "--name=solver", "-c", "-7", "--ratio",
                        "-0.25", "--size=42", "-v", "9", "input-b"});
  ASC_UTILITIES_TEST_CHECK(context, parsed.ok());
  if (!parsed.ok()) {
    return;
  }

  ASC_UTILITIES_TEST_EQ(context, parsed->positional_arguments.size(),
                        std::size_t{2});
  ASC_UTILITIES_TEST_EQ(context, parsed->positional_arguments[0],
                        std::string("input-a"));
  ASC_UTILITIES_TEST_EQ(context, parsed->positional_arguments[1],
                        std::string("input-b"));
  ASC_UTILITIES_TEST_EQ(
      context, parsed->configuration.Find("/name")->get().AsString()->get(),
      std::string("solver"));
  ASC_UTILITIES_TEST_EQ(
      context, *parsed->configuration.Find("/count")->get().AsSignedInteger(),
      -7);
  ASC_UTILITIES_TEST_EQ(
      context, *parsed->configuration.Find("/ratio")->get().AsDouble(), -0.25);
  ASC_UTILITIES_TEST_EQ(
      context, *parsed->configuration.Find("/size")->get().AsUnsignedInteger(),
      42U);
  ASC_UTILITIES_TEST_EQ(
      context,
      *parsed->configuration.Find("/group/value")->get().AsSignedInteger(), 9);

  const auto name_origin = parsed->configuration.Origin("/name");
  const auto count_origin = parsed->configuration.Origin("/count");
  ASC_UTILITIES_TEST_CHECK(context, name_origin.ok());
  ASC_UTILITIES_TEST_CHECK(context, count_origin.ok());
  if (!name_origin.ok() || !count_origin.ok()) {
    return;
  }
  ASC_UTILITIES_TEST_EQ(context, name_origin->kind,
                        asc::ConfigurationOriginKind::kCommandLine);
  const auto& name_location = name_origin->location;
  ASC_UTILITIES_TEST_CHECK(context, name_location.has_value());
  if (name_location.has_value()) {
    ASC_UTILITIES_TEST_EQ(context, *name_location, std::string("1"));
  }
  const auto& count_location = count_origin->location;
  ASC_UTILITIES_TEST_CHECK(context, count_location.has_value());
  if (count_location.has_value()) {
    ASC_UTILITIES_TEST_EQ(context, *count_location, std::string("2"));
  }
  ASC_UTILITIES_TEST_EQ(context, parsed->configuration.Origin("/flag")->kind,
                        asc::ConfigurationOriginKind::kDefault);

  const auto terminated = Parse(
      parser,
      std::array<std::string_view, 4>{"--name", "before", "--", "--flag"});
  ASC_UTILITIES_TEST_CHECK(context, terminated.ok());
  ASC_UTILITIES_TEST_EQ(context, terminated->positional_arguments.size(),
                        std::size_t{1});
  ASC_UTILITIES_TEST_EQ(context, terminated->positional_arguments[0],
                        std::string("--flag"));
  ASC_UTILITIES_TEST_EQ(
      context, terminated->configuration.Find("/name")->get().AsString()->get(),
      std::string("before"));
}

void CheckBooleanForms(TestContext& context) {
  auto parser_result = MakeParser(context);
  if (!parser_result.ok()) {
    return;
  }
  const asc::CommandLineParser& parser = *parser_result;

  const auto positive =
      Parse(parser, std::array<std::string_view, 1>{"--flag"});
  ASC_UTILITIES_TEST_CHECK(context, positive.ok());
  ASC_UTILITIES_TEST_EQ(
      context, *positive->configuration.Find("/flag")->get().AsBool(), true);

  const auto negative =
      Parse(parser, std::array<std::string_view, 1>{"--no-flag"});
  ASC_UTILITIES_TEST_CHECK(context, negative.ok());
  ASC_UTILITIES_TEST_EQ(
      context, *negative->configuration.Find("/flag")->get().AsBool(), false);

  const auto explicit_false =
      Parse(parser, std::array<std::string_view, 1>{"--flag=false"});
  ASC_UTILITIES_TEST_CHECK(context, explicit_false.ok());
  ASC_UTILITIES_TEST_EQ(
      context, *explicit_false->configuration.Find("/flag")->get().AsBool(),
      false);

  const auto short_form =
      Parse(parser, std::array<std::string_view, 2>{"-f", "positional"});
  ASC_UTILITIES_TEST_CHECK(context, short_form.ok());
  ASC_UTILITIES_TEST_EQ(
      context, *short_form->configuration.Find("/flag")->get().AsBool(), true);
  ASC_UTILITIES_TEST_EQ(context, short_form->positional_arguments.size(),
                        std::size_t{1});

  ASC_UTILITIES_TEST_CHECK(
      context,
      !Parse(parser, std::array<std::string_view, 1>{"--no-count"}).ok());
  ASC_UTILITIES_TEST_CHECK(
      context,
      !Parse(parser, std::array<std::string_view, 1>{"--flag=1"}).ok());
  ASC_UTILITIES_TEST_CHECK(
      context,
      !Parse(parser, std::array<std::string_view, 1>{"--no-flag=false"}).ok());
}

void CheckNumericBoundaries(TestContext& context) {
  auto parser_result = MakeParser(context);
  if (!parser_result.ok()) {
    return;
  }
  const asc::CommandLineParser& parser = *parser_result;

  const auto signed_min = Parse(
      parser,
      std::array<std::string_view, 2>{"--wide-signed", "-9223372036854775808"});
  ASC_UTILITIES_TEST_CHECK(context, signed_min.ok());
  ASC_UTILITIES_TEST_EQ(
      context,
      *signed_min->configuration.Find("/wide-signed")->get().AsSignedInteger(),
      std::numeric_limits<std::int64_t>::min());

  const auto signed_max = Parse(
      parser,
      std::array<std::string_view, 1>{"--wide-signed=9223372036854775807"});
  ASC_UTILITIES_TEST_CHECK(context, signed_max.ok());
  ASC_UTILITIES_TEST_EQ(
      context,
      *signed_max->configuration.Find("/wide-signed")->get().AsSignedInteger(),
      std::numeric_limits<std::int64_t>::max());

  const auto unsigned_max = Parse(
      parser, std::array<std::string_view, 1>{"--size=18446744073709551615"});
  ASC_UTILITIES_TEST_CHECK(context, unsigned_max.ok());
  ASC_UTILITIES_TEST_EQ(
      context,
      *unsigned_max->configuration.Find("/size")->get().AsUnsignedInteger(),
      std::numeric_limits<std::uint64_t>::max());

  const auto exact_double =
      Parse(parser, std::array<std::string_view, 1>{"--ratio=0.125"});
  ASC_UTILITIES_TEST_CHECK(context, exact_double.ok());
  ASC_UTILITIES_TEST_EQ(
      context, *exact_double->configuration.Find("/ratio")->get().AsDouble(),
      0.125);

  const auto scientific_double =
      Parse(parser, std::array<std::string_view, 1>{"--ratio=1.25e-1"});
  ASC_UTILITIES_TEST_CHECK(context, scientific_double.ok());
  ASC_UTILITIES_TEST_EQ(
      context,
      *scientific_double->configuration.Find("/ratio")->get().AsDouble(),
      0.125);

  for (std::string_view argument :
       {"--wide-signed=9223372036854775808",
        "--wide-signed=-9223372036854775809", "--wide-signed=1.0",
        "--wide-signed=1x", "--size=-1", "--size=18446744073709551616",
        "--size=1.0", "--ratio=+0.5", "--ratio= 0.5", "--ratio=0.5 ",
        "--ratio=0x1p0", "--ratio=1,5", "--ratio=1x", "--ratio=1e9999",
        "--ratio=1e-9999", "--ratio=nan"}) {
    ASC_UTILITIES_TEST_CHECK(
        context,
        !Parse(parser, std::array<std::string_view, 1>{argument}).ok());
  }
}

void CheckFailuresAndRollback(TestContext& context) {
  auto parser_result = MakeParser(context);
  if (!parser_result.ok()) {
    return;
  }
  const asc::CommandLineParser& parser = *parser_result;

  for (const std::vector<std::string_view>& arguments :
       std::vector<std::vector<std::string_view>>{
           {"--unknown"},
           {"--name"},
           {"-n"},
           {"-nattached"},
           {"-fn"},
           {"---name"},
           {"--count", "101"},
           {"--flag", "-f"},
           {"--flag", "--no-flag"},
           {"--name=first", "-n", "second"},
       }) {
    ASC_UTILITIES_TEST_CHECK(
        context,
        !parser.Parse(std::span<const std::string_view>(arguments)).ok());
  }

  const std::string invalid_utf8("bad\xc3\x28", 5);
  const std::array invalid_utf8_arguments = {
      std::string_view("--name"),
      std::string_view(invalid_utf8),
  };
  ASC_UTILITIES_TEST_CHECK(
      context,
      !parser.Parse(std::span<const std::string_view>(invalid_utf8_arguments))
           .ok());

  const auto before =
      Parse(parser, std::array<std::string_view, 1>{"--name=before"});
  ASC_UTILITIES_TEST_CHECK(context, before.ok());

  const auto failed = Parse(
      parser, std::array<std::string_view, 2>{"--flag", "--secret=top-secret"});
  ASC_UTILITIES_TEST_CHECK(context, !failed.ok());
  ASC_UTILITIES_TEST_CHECK(context, failed.status().message().find(
                                        "top-secret") == std::string::npos);

  const auto after =
      Parse(parser, std::array<std::string_view, 1>{"--name=after"});
  ASC_UTILITIES_TEST_CHECK(context, after.ok());
  ASC_UTILITIES_TEST_EQ(
      context, before->configuration.Find("/name")->get().AsString()->get(),
      std::string("before"));
  ASC_UTILITIES_TEST_EQ(
      context, after->configuration.Find("/name")->get().AsString()->get(),
      std::string("after"));

  ASC_UTILITIES_TEST_CHECK(
      context, !Parse(parser, std::array<std::string_view, 1>{"--help"}).ok());
}

void CheckHelp(TestContext& context) {
  auto parser_result = MakeParser(context);
  if (!parser_result.ok()) {
    return;
  }
  const asc::CommandLineParser& parser = *parser_result;

  const std::string first = parser.RenderHelp();
  std::string caller_copy = first;
  caller_copy.assign("changed");
  const std::string second = parser.RenderHelp();
  ASC_UTILITIES_TEST_EQ(context, first, second);
  ASC_UTILITIES_TEST_CHECK(context, first.find("--flag") != std::string::npos);
  ASC_UTILITIES_TEST_CHECK(context, first.find("-f") != std::string::npos);
  ASC_UTILITIES_TEST_CHECK(context, first.find("--name") != std::string::npos);
  ASC_UTILITIES_TEST_CHECK(
      context, first.find("Set a UTF-8 name.") != std::string::npos);
}

}  // namespace

int main() {
  TestContext context;
  CheckCreation(context);
  CheckBasicParsing(context);
  CheckBooleanForms(context);
  CheckNumericBoundaries(context);
  CheckFailuresAndRollback(context);
  CheckHelp(context);
  return context.Finish();
}
