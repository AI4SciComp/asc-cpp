#include "asc/utilities/command_line.h"

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "asc/core/configuration.h"
#include "asc/core/contracts.h"
#include "asc/core/status.h"
#include "test_support.h"

namespace {

using asc::CommandLineOption;
using asc::ConfigurationSchema;
using asc::ConfigurationValue;
using asc::ConfigurationValueType;

ConfigurationValue Utf8(std::string value) {
  auto result = ConfigurationValue::Utf8String(std::move(value));
  ASC_CHECK_MESSAGE(result.ok(), "A test fixture must contain valid UTF-8");
  return std::move(*result);
}

ConfigurationSchema MakeSchema(asc_utilities_test::TestContext& context,
                               bool include_unsupported_fields = false) {
  ConfigurationSchema root(ConfigurationValueType::kObject);

  ConfigurationSchema verbose(ConfigurationValueType::kBool);
  verbose.SetDefault(ConfigurationValue(false));
  ASC_UTILITIES_TEST_CHECK(context,
                           root.AddField("verbose", std::move(verbose)).ok());

  ConfigurationSchema count(ConfigurationValueType::kSignedInteger);
  count.SetDefault(ConfigurationValue(std::int64_t{2}));
  ASC_UTILITIES_TEST_CHECK(context, count.SetSignedBounds(-100, 100).ok());
  ASC_UTILITIES_TEST_CHECK(context,
                           root.AddField("count", std::move(count)).ok());

  ConfigurationSchema jobs(ConfigurationValueType::kUnsignedInteger);
  jobs.SetDefault(ConfigurationValue(std::uint64_t{4}));
  ASC_UTILITIES_TEST_CHECK(context,
                           root.AddField("jobs", std::move(jobs)).ok());

  ConfigurationSchema ratio(ConfigurationValueType::kDouble);
  ratio.SetDefault(ConfigurationValue(0.5));
  ASC_UTILITIES_TEST_CHECK(context,
                           root.AddField("ratio", std::move(ratio)).ok());

  ConfigurationSchema name(ConfigurationValueType::kString);
  name.SetDefault(Utf8("default-name"));
  ASC_UTILITIES_TEST_CHECK(context,
                           root.AddField("name", std::move(name)).ok());

  ConfigurationSchema secret(ConfigurationValueType::kString);
  secret.SetDefault(Utf8("default-secret"));
  secret.SetSensitive(true);
  ASC_UTILITIES_TEST_CHECK(
      context, secret.SetSizeBounds(std::nullopt, std::size_t{32}).ok());
  ASC_UTILITIES_TEST_CHECK(context,
                           root.AddField("secret", std::move(secret)).ok());

  if (include_unsupported_fields) {
    ASC_UTILITIES_TEST_CHECK(
        context, root.AddField("items", ConfigurationSchema(
                                            ConfigurationValueType::kList))
                     .ok());
    ASC_UTILITIES_TEST_CHECK(
        context, root.AddField("nested", ConfigurationSchema(
                                             ConfigurationValueType::kObject))
                     .ok());
    ASC_UTILITIES_TEST_CHECK(
        context, root.AddField("nothing", ConfigurationSchema(
                                              ConfigurationValueType::kNull))
                     .ok());
  }
  return root;
}

std::vector<CommandLineOption> MakeOptions() {
  return {
      {"verbose", 'v', "/verbose", "", "Enable detailed output."},
      {"count", 'c', "/count", "COUNT", "Set the signed count."},
      {"jobs", 'j', "/jobs", "JOBS", "Set the worker count."},
      {"ratio", 'r', "/ratio", "RATIO", "Set the floating ratio."},
      {"name", 'n', "/name", "NAME", "Set a UTF-8 name."},
      {"secret", 's', "/secret", "SECRET", "Set a sensitive value."},
  };
}

asc::Result<asc::CommandLineParseResult> Parse(
    const asc::CommandLineParser& parser,
    std::initializer_list<std::string_view> arguments) {
  const std::vector<std::string_view> copied(arguments);
  return parser.Parse(copied);
}

const ConfigurationValue& Find(const asc::Configuration& configuration,
                               std::string_view path) {
  auto value = configuration.Find(path);
  ASC_CHECK_MESSAGE(value.ok(), "The test schema must contain the path");
  return value->get();
}

void CheckOptionTableValidation(asc_utilities_test::TestContext& context) {
  auto valid =
      asc::CommandLineParser::Create(MakeSchema(context), MakeOptions());
  ASC_UTILITIES_TEST_CHECK(context, valid.ok());
  ASC_UTILITIES_TEST_EQ(context, valid->options().size(), std::size_t{6});
  ASC_UTILITIES_TEST_EQ(context, valid->schema().type(),
                        ConfigurationValueType::kObject);

  ConfigurationSchema scalar_root(ConfigurationValueType::kSignedInteger);
  scalar_root.SetDefault(ConfigurationValue(std::int64_t{3}));
  const auto invalid_root = asc::CommandLineParser::Create(
      std::move(scalar_root),
      {{"value", std::nullopt, "", "VALUE", "Set the root value."}});
  ASC_UTILITIES_TEST_CHECK(context, !invalid_root.ok());
  ASC_UTILITIES_TEST_EQ(context, invalid_root.status().code(),
                        asc::ErrorCode::kInvalidArgument);

  auto duplicate_long = MakeOptions();
  duplicate_long[1].long_name = duplicate_long[0].long_name;
  ASC_UTILITIES_TEST_CHECK(
      context, !asc::CommandLineParser::Create(MakeSchema(context),
                                               std::move(duplicate_long))
                    .ok());

  auto duplicate_short = MakeOptions();
  duplicate_short[1].short_name = duplicate_short[0].short_name;
  ASC_UTILITIES_TEST_CHECK(
      context, !asc::CommandLineParser::Create(MakeSchema(context),
                                               std::move(duplicate_short))
                    .ok());

  auto duplicate_path = MakeOptions();
  duplicate_path[1].configuration_path = duplicate_path[0].configuration_path;
  ASC_UTILITIES_TEST_CHECK(
      context, !asc::CommandLineParser::Create(MakeSchema(context),
                                               std::move(duplicate_path))
                    .ok());

  auto empty_long = MakeOptions();
  empty_long[0].long_name.clear();
  ASC_UTILITIES_TEST_CHECK(
      context, !asc::CommandLineParser::Create(MakeSchema(context),
                                               std::move(empty_long))
                    .ok());

  auto leading_dash = MakeOptions();
  leading_dash[0].long_name = "--verbose";
  ASC_UTILITIES_TEST_CHECK(
      context, !asc::CommandLineParser::Create(MakeSchema(context),
                                               std::move(leading_dash))
                    .ok());

  auto non_ascii = MakeOptions();
  non_ascii[0].long_name = "\xc3\xa9";
  ASC_UTILITIES_TEST_CHECK(
      context,
      !asc::CommandLineParser::Create(MakeSchema(context), std::move(non_ascii))
           .ok());

  auto missing_path = MakeOptions();
  missing_path[0].configuration_path = "/missing";
  ASC_UTILITIES_TEST_CHECK(
      context, !asc::CommandLineParser::Create(MakeSchema(context),
                                               std::move(missing_path))
                    .ok());

  auto invalid_pointer = MakeOptions();
  invalid_pointer[0].configuration_path = "/bad~2escape";
  ASC_UTILITIES_TEST_CHECK(
      context, !asc::CommandLineParser::Create(MakeSchema(context),
                                               std::move(invalid_pointer))
                    .ok());

  auto negation_conflict = MakeOptions();
  negation_conflict[4].long_name = "no-verbose";
  ASC_UTILITIES_TEST_CHECK(
      context, !asc::CommandLineParser::Create(MakeSchema(context),
                                               std::move(negation_conflict))
                    .ok());

  const auto unsupported_schema = MakeSchema(context, true);
  for (std::string path : {"/items", "/nested", "/nothing"}) {
    auto unsupported = MakeOptions();
    unsupported.push_back(
        {"unsupported", 'u', std::move(path), "VALUE", "Unsupported."});
    const auto result = asc::CommandLineParser::Create(unsupported_schema,
                                                       std::move(unsupported));
    ASC_UTILITIES_TEST_CHECK(context, !result.ok());
    ASC_UTILITIES_TEST_EQ(context, result.status().code(),
                          asc::ErrorCode::kUnsupported);
  }
}

void CheckSyntaxAndTypes(asc_utilities_test::TestContext& context) {
  const auto parser =
      asc::CommandLineParser::Create(MakeSchema(context), MakeOptions());
  ASC_UTILITIES_TEST_CHECK(context, parser.ok());

  const auto split =
      Parse(*parser, {"--count", "-17", "--jobs", "23", "--ratio", "-2.5",
                      "--name", "\xe6\xb1\x82\xe8\xa7\xa3\xe5\x99\xa8"});
  ASC_UTILITIES_TEST_CHECK(context, split.ok());
  ASC_UTILITIES_TEST_EQ(
      context, *Find(split->configuration, "/count").AsSignedInteger(), -17);
  ASC_UTILITIES_TEST_EQ(
      context, *Find(split->configuration, "/jobs").AsUnsignedInteger(), 23U);
  ASC_UTILITIES_TEST_EQ(context,
                        *Find(split->configuration, "/ratio").AsDouble(), -2.5);
  ASC_UTILITIES_TEST_EQ(context,
                        Find(split->configuration, "/name").AsString()->get(),
                        std::string("\xe6\xb1\x82\xe8\xa7\xa3\xe5\x99\xa8"));
  ASC_UTILITIES_TEST_CHECK(context, split->positional_arguments.empty());

  const auto equals =
      Parse(*parser, {"--count=-100", "--jobs=18446744073709551615",
                      "--ratio=1.25e-3", "--name=path with spaces"});
  ASC_UTILITIES_TEST_CHECK(context, equals.ok());
  ASC_UTILITIES_TEST_EQ(
      context, *Find(equals->configuration, "/count").AsSignedInteger(), -100);
  ASC_UTILITIES_TEST_EQ(
      context, *Find(equals->configuration, "/jobs").AsUnsignedInteger(),
      std::numeric_limits<std::uint64_t>::max());
  ASC_UTILITIES_TEST_EQ(
      context, *Find(equals->configuration, "/ratio").AsDouble(), 1.25e-3);
  ASC_UTILITIES_TEST_EQ(context,
                        Find(equals->configuration, "/name").AsString()->get(),
                        std::string("path with spaces"));

  const auto short_form =
      Parse(*parser, {"-c", "9", "-j", "7", "-r", "3.5", "-n", "short"});
  ASC_UTILITIES_TEST_CHECK(context, short_form.ok());
  ASC_UTILITIES_TEST_EQ(
      context, *Find(short_form->configuration, "/count").AsSignedInteger(), 9);

  const auto signed_min = Parse(*parser, {"--count", "-9223372036854775808"});
  ASC_UTILITIES_TEST_CHECK(context, !signed_min.ok());
  ASC_UTILITIES_TEST_EQ(context, signed_min.status().code(),
                        asc::ErrorCode::kConfiguration);

  ConfigurationSchema endpoint_schema(ConfigurationValueType::kObject);
  ASC_UTILITIES_TEST_CHECK(
      context,
      endpoint_schema
          .AddField("signed",
                    ConfigurationSchema(ConfigurationValueType::kSignedInteger))
          .ok());
  const auto endpoint_parser = asc::CommandLineParser::Create(
      std::move(endpoint_schema),
      {{"signed", std::nullopt, "/signed", "SIGNED", "Set an integer."}});
  ASC_UTILITIES_TEST_CHECK(context, endpoint_parser.ok());
  const auto exact_minimum =
      Parse(*endpoint_parser, {"--signed=-9223372036854775808"});
  ASC_UTILITIES_TEST_CHECK(context, exact_minimum.ok());
  ASC_UTILITIES_TEST_EQ(
      context, *Find(exact_minimum->configuration, "/signed").AsSignedInteger(),
      std::numeric_limits<std::int64_t>::min());

  for (std::initializer_list<std::string_view> invalid :
       {std::initializer_list<std::string_view>{"--count=101"},
        {"--count=-9223372036854775809"},
        {"--jobs=-1"},
        {"--jobs=18446744073709551616"},
        {"--ratio=1e10000"},
        {"--ratio=1.0junk"},
        {"--count="},
        {"--jobs=+2"}}) {
    ASC_UTILITIES_TEST_CHECK(context, !Parse(*parser, invalid).ok());
  }
}

void CheckBooleansAndDuplicates(asc_utilities_test::TestContext& context) {
  const auto parser =
      asc::CommandLineParser::Create(MakeSchema(context), MakeOptions());
  ASC_UTILITIES_TEST_CHECK(context, parser.ok());

  for (std::initializer_list<std::string_view> arguments :
       {std::initializer_list<std::string_view>{"--verbose"},
        {"--verbose=true"},
        {"-v"}}) {
    const auto result = Parse(*parser, arguments);
    ASC_UTILITIES_TEST_CHECK(context, result.ok());
    ASC_UTILITIES_TEST_EQ(
        context, *Find(result->configuration, "/verbose").AsBool(), true);
  }
  for (std::initializer_list<std::string_view> arguments :
       {std::initializer_list<std::string_view>{"--no-verbose"},
        {"--verbose=false"}}) {
    const auto result = Parse(*parser, arguments);
    ASC_UTILITIES_TEST_CHECK(context, result.ok());
    ASC_UTILITIES_TEST_EQ(
        context, *Find(result->configuration, "/verbose").AsBool(), false);
  }

  for (std::initializer_list<std::string_view> duplicate :
       {std::initializer_list<std::string_view>{"--verbose", "-v"},
        {"--verbose", "--no-verbose"},
        {"--count=1", "-c", "2"},
        {"--name", "first", "--name=second"}}) {
    const auto result = Parse(*parser, duplicate);
    ASC_UTILITIES_TEST_CHECK(context, !result.ok());
    ASC_UTILITIES_TEST_EQ(context, result.status().code(),
                          asc::ErrorCode::kInvalidArgument);
  }
}

void CheckPositionalsAndFailures(asc_utilities_test::TestContext& context) {
  const auto parser =
      asc::CommandLineParser::Create(MakeSchema(context), MakeOptions());
  ASC_UTILITIES_TEST_CHECK(context, parser.ok());

  const auto positional = Parse(
      *parser, {"input", "--name", "configured", "--", "--jobs", "-v", "tail"});
  ASC_UTILITIES_TEST_CHECK(context, positional.ok());
  ASC_UTILITIES_TEST_EQ(context, positional->positional_arguments.size(),
                        std::size_t{4});
  ASC_UTILITIES_TEST_EQ(context, positional->positional_arguments[0],
                        std::string("input"));
  ASC_UTILITIES_TEST_EQ(context, positional->positional_arguments[1],
                        std::string("--jobs"));
  ASC_UTILITIES_TEST_EQ(context, positional->positional_arguments[2],
                        std::string("-v"));
  ASC_UTILITIES_TEST_EQ(context, positional->positional_arguments[3],
                        std::string("tail"));

  for (std::initializer_list<std::string_view> invalid :
       {std::initializer_list<std::string_view>{"--unknown"},
        {"-z"},
        {"---verbose"},
        {"-vv"},
        {"-v=true"},
        {"--count"},
        {"-c"}}) {
    ASC_UTILITIES_TEST_CHECK(context, !Parse(*parser, invalid).ok());
  }

  const auto no_response_file = Parse(*parser, {"@settings.conf"});
  ASC_UTILITIES_TEST_CHECK(context, no_response_file.ok());
  ASC_UTILITIES_TEST_EQ(context, no_response_file->positional_arguments.front(),
                        std::string("@settings.conf"));
}

void CheckNestedJsonPointer(asc_utilities_test::TestContext& context) {
  ConfigurationSchema root(ConfigurationValueType::kObject);
  ConfigurationSchema nested(ConfigurationValueType::kObject);
  ConfigurationSchema leaf(ConfigurationValueType::kSignedInteger);
  leaf.SetDefault(ConfigurationValue(std::int64_t{0}));
  ASC_UTILITIES_TEST_CHECK(context,
                           nested.AddField("a/b~c", std::move(leaf)).ok());
  ASC_UTILITIES_TEST_CHECK(context,
                           root.AddField("nested", std::move(nested)).ok());

  const auto parser = asc::CommandLineParser::Create(
      std::move(root), {{"escaped", std::nullopt, "/nested/a~1b~0c", "VALUE",
                         "Set an escaped path."}});
  ASC_UTILITIES_TEST_CHECK(context, parser.ok());
  const auto parsed = Parse(*parser, {"--escaped=-7"});
  ASC_UTILITIES_TEST_CHECK(context, parsed.ok());
  ASC_UTILITIES_TEST_EQ(
      context,
      *Find(parsed->configuration, "/nested/a~1b~0c").AsSignedInteger(), -7);
}

void CheckOriginsRollbackAndRedaction(
    asc_utilities_test::TestContext& context) {
  const auto parser =
      asc::CommandLineParser::Create(MakeSchema(context), MakeOptions());
  ASC_UTILITIES_TEST_CHECK(context, parser.ok());

  const auto defaults = Parse(*parser, {});
  ASC_UTILITIES_TEST_CHECK(context, defaults.ok());
  ASC_UTILITIES_TEST_EQ(context, defaults->configuration.Origin("/name")->kind,
                        asc::ConfigurationOriginKind::kDefault);

  const auto explicit_value =
      Parse(*parser, {"leading", "--name", "configured"});
  ASC_UTILITIES_TEST_CHECK(context, explicit_value.ok());
  const auto origin = explicit_value->configuration.Origin("/name");
  ASC_UTILITIES_TEST_CHECK(context, origin.ok());
  ASC_UTILITIES_TEST_EQ(context, origin->kind,
                        asc::ConfigurationOriginKind::kCommandLine);
  ASC_UTILITIES_TEST_CHECK(context, origin->location.has_value());
  ASC_UTILITIES_TEST_EQ(context, *origin->location, std::string("1"));

  const std::string sensitive_value = "do-not-echo-this-secret";
  const auto redacted = Parse(
      *parser, {"--name=valid", "--secret", sensitive_value, "--count=1000"});
  ASC_UTILITIES_TEST_CHECK(context, !redacted.ok());
  ASC_UTILITIES_TEST_CHECK(context, redacted.status().message().find(
                                        sensitive_value) == std::string::npos);

  const auto late_failure = Parse(
      *parser,
      {"--name=would-be-partial", "positional", "--jobs=18446744073709551616"});
  ASC_UTILITIES_TEST_CHECK(context, !late_failure.ok());

  for (const std::string& malformed :
       {"-s" + sensitive_value, "-s=" + sensitive_value,
        "--secret" + sensitive_value, "--no-secret" + sensitive_value}) {
    const std::vector<std::string_view> arguments = {malformed};
    const auto malformed_result = parser->Parse(arguments);
    ASC_UTILITIES_TEST_CHECK(context, !malformed_result.ok());
    ASC_UTILITIES_TEST_CHECK(
        context, malformed_result.status().message().find(sensitive_value) ==
                     std::string::npos);
  }
}

void CheckActionableDiagnostics(asc_utilities_test::TestContext& context) {
  const auto parser =
      asc::CommandLineParser::Create(MakeSchema(context), MakeOptions());
  ASC_UTILITIES_TEST_CHECK(context, parser.ok());

  const auto unknown = Parse(*parser, {"--mystery"});
  ASC_UTILITIES_TEST_CHECK(context, !unknown.ok());
  ASC_UTILITIES_TEST_EQ(context, unknown.status().code(),
                        asc::ErrorCode::kInvalidArgument);
  ASC_UTILITIES_TEST_CHECK(context, unknown.status().message().find(
                                        "--mystery") != std::string::npos);
  ASC_UTILITIES_TEST_CHECK(
      context, unknown.status().message().find("argv[0]") != std::string::npos);

  const auto invalid_number =
      Parse(*parser, {"leading", "--count=not-a-number"});
  ASC_UTILITIES_TEST_CHECK(context, !invalid_number.ok());
  ASC_UTILITIES_TEST_EQ(context, invalid_number.status().code(),
                        asc::ErrorCode::kInvalidArgument);
  ASC_UTILITIES_TEST_CHECK(context, invalid_number.status().message().find(
                                        "--count") != std::string::npos);
  ASC_UTILITIES_TEST_CHECK(context, invalid_number.status().message().find(
                                        "argv[1]") != std::string::npos);
  ASC_UTILITIES_TEST_CHECK(context, invalid_number.status().message().find(
                                        "not-a-number") != std::string::npos);
  ASC_UTILITIES_TEST_CHECK(context, invalid_number.status().message().find(
                                        "signed integer") != std::string::npos);

  const auto outside_bound = Parse(*parser, {"--count=101"});
  ASC_UTILITIES_TEST_CHECK(context, !outside_bound.ok());
  ASC_UTILITIES_TEST_EQ(context, outside_bound.status().code(),
                        asc::ErrorCode::kConfiguration);
  ASC_UTILITIES_TEST_CHECK(context, outside_bound.status().message().find(
                                        "argv[0]") != std::string::npos);
  ASC_UTILITIES_TEST_CHECK(context, outside_bound.status().message().find(
                                        "--count") != std::string::npos);
  ASC_UTILITIES_TEST_CHECK(context, outside_bound.status().message().find(
                                        "101") != std::string::npos);
  ASC_UTILITIES_TEST_CHECK(context, outside_bound.status().message().find(
                                        "signed integer") != std::string::npos);
  ASC_UTILITIES_TEST_CHECK(context, outside_bound.status().message().find(
                                        "bounds") != std::string::npos);
  ASC_UTILITIES_TEST_CHECK(context, outside_bound.status().message().find(
                                        "[-100, 100]") != std::string::npos);

  const std::string invalid_utf8("\xff", 1);
  const std::vector<std::string_view> invalid_sensitive_arguments = {
      "--secret", invalid_utf8};
  const auto invalid_sensitive = parser->Parse(invalid_sensitive_arguments);
  ASC_UTILITIES_TEST_CHECK(context, !invalid_sensitive.ok());
  ASC_UTILITIES_TEST_EQ(context, invalid_sensitive.status().code(),
                        asc::ErrorCode::kEncoding);
  ASC_UTILITIES_TEST_CHECK(context, invalid_sensitive.status().message().find(
                                        "argv[0]") != std::string::npos);
  ASC_UTILITIES_TEST_CHECK(context, invalid_sensitive.status().message().find(
                                        "--secret") != std::string::npos);
  ASC_UTILITIES_TEST_CHECK(context, invalid_sensitive.status().message().find(
                                        "<redacted>") != std::string::npos);
  ASC_UTILITIES_TEST_CHECK(context, invalid_sensitive.status().message().find(
                                        invalid_utf8) == std::string::npos);

  const std::string long_secret(40, 'x');
  const auto sensitive_bound = Parse(*parser, {"--secret", long_secret});
  ASC_UTILITIES_TEST_CHECK(context, !sensitive_bound.ok());
  ASC_UTILITIES_TEST_EQ(context, sensitive_bound.status().code(),
                        asc::ErrorCode::kConfiguration);
  ASC_UTILITIES_TEST_CHECK(context, sensitive_bound.status().message().find(
                                        "argv[0]") != std::string::npos);
  ASC_UTILITIES_TEST_CHECK(context, sensitive_bound.status().message().find(
                                        "--secret") != std::string::npos);
  ASC_UTILITIES_TEST_CHECK(context, sensitive_bound.status().message().find(
                                        "<redacted>") != std::string::npos);
  ASC_UTILITIES_TEST_CHECK(context, sensitive_bound.status().message().find(
                                        "bounds") != std::string::npos);
  ASC_UTILITIES_TEST_CHECK(context, sensitive_bound.status().message().find(
                                        long_secret) == std::string::npos);
}

void CheckHelp(asc_utilities_test::TestContext& context) {
  const auto parser =
      asc::CommandLineParser::Create(MakeSchema(context), MakeOptions());
  ASC_UTILITIES_TEST_CHECK(context, parser.ok());

  const std::string first = parser->RenderHelp("asc solver");
  const std::string second = parser->RenderHelp("asc solver");
  ASC_UTILITIES_TEST_EQ(context, first, second);
  ASC_UTILITIES_TEST_CHECK(
      context, first.find("Usage: asc solver") != std::string::npos);
  ASC_UTILITIES_TEST_CHECK(context,
                           first.find("--[no-]verbose") != std::string::npos);
  ASC_UTILITIES_TEST_CHECK(context, first.find("-v") != std::string::npos);
  ASC_UTILITIES_TEST_CHECK(context, first.find("COUNT") != std::string::npos);
  ASC_UTILITIES_TEST_CHECK(
      context, first.find("Enable detailed output.") != std::string::npos);
  ASC_UTILITIES_TEST_CHECK(context, !first.empty() && first.back() == '\n');
}

}  // namespace

int main() {
  asc_utilities_test::TestContext context;
  CheckOptionTableValidation(context);
  CheckSyntaxAndTypes(context);
  CheckBooleansAndDuplicates(context);
  CheckPositionalsAndFailures(context);
  CheckNestedJsonPointer(context);
  CheckOriginsRollbackAndRedaction(context);
  CheckActionableDiagnostics(context);
  CheckHelp(context);
  return context.Finish();
}
