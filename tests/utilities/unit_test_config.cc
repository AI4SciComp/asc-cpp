#include <asc/core/contracts.h>
#include <asc/core/status.h>
#include <asc/utilities/config.h>
#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace asc {
namespace {

TEST(ConfigValueTest, RepresentsEveryAdvertisedAlternative) {
  const ConfigVector vector = {1.0, -2.5};
  const ConfigMatrix matrix = {{1.0, 2.0}, {3.0, 4.0}};

  EXPECT_TRUE(ConfigValue().IsEmpty());
  EXPECT_TRUE(ConfigValue(true).AsBool());
  EXPECT_EQ(ConfigValue(-7).AsInt(), -7);
  EXPECT_DOUBLE_EQ(ConfigValue(3.25).AsDouble(), 3.25);
  EXPECT_EQ(ConfigValue("text").AsString(), "text");
  EXPECT_EQ(ConfigValue(vector).AsVector(), vector);
  EXPECT_EQ(ConfigValue(matrix).AsMatrix(), matrix);

  EXPECT_DOUBLE_EQ(ConfigValue(11).AsDouble(), 11.0);
  EXPECT_EQ(ConfigValue(11.0).AsInt(), 11);
}

#ifdef ASC_USE_EXCEPTION
TEST(ConfigValueTest, CheckedIntegerAccessRejectsUnsafeConversions) {
  EXPECT_THROW(static_cast<void>(ConfigValue(1.5).AsInt()),
               ContractException);
  EXPECT_THROW(
      static_cast<void>(ConfigValue(std::numeric_limits<double>::infinity())
                            .AsInt()),
      ContractException);
  EXPECT_THROW(
      static_cast<void>(ConfigValue(
                           static_cast<double>(std::numeric_limits<int>::max())
                           * 2.0)
                            .AsInt()),
      ContractException);
  EXPECT_THROW(static_cast<void>(ConfigValue("1").AsInt()),
               ContractException);
}
#endif

TEST(ConfigParserTest, ParsesCanonicalAndCompatibilityValues) {
  Result<ConfigValue> empty = ConfigParser::TryParseValue(" null ");
  ASSERT_TRUE(empty.ok());
  EXPECT_TRUE(empty.value().IsEmpty());

  Result<ConfigValue> boolean = ConfigParser::TryParseValue("TRUE");
  ASSERT_TRUE(boolean.ok());
  EXPECT_TRUE(boolean.value().AsBool());

  Result<ConfigValue> integer = ConfigParser::TryParseValue("-42");
  ASSERT_TRUE(integer.ok());
  EXPECT_EQ(integer.value().AsInt(), -42);

  Result<ConfigValue> real = ConfigParser::TryParseValue("1.25e-2");
  ASSERT_TRUE(real.ok());
  EXPECT_DOUBLE_EQ(real.value().AsDouble(), 0.0125);

  Result<ConfigValue> string =
      ConfigParser::TryParseValue("\"line\\nquote\\\"slash\\\\tab\\t\"");
  ASSERT_TRUE(string.ok());
  EXPECT_EQ(string.value().AsString(), "line\nquote\"slash\\tab\t");

  Result<ConfigValue> legacy_string =
      ConfigParser::TryParseValue("legacy-token");
  ASSERT_TRUE(legacy_string.ok());
  EXPECT_EQ(legacy_string.value().AsString(), "legacy-token");

  Result<ConfigValue> vector =
      ConfigParser::TryParseValue("vector[1.0, -2.5, 3e2]");
  ASSERT_TRUE(vector.ok());
  EXPECT_EQ(vector.value().AsVector(), (ConfigVector{1.0, -2.5, 300.0}));

  Result<ConfigValue> matrix =
      ConfigParser::TryParseValue("matrix[[1.0, 2.0], [3.5, 4.0]]");
  ASSERT_TRUE(matrix.ok());
  EXPECT_EQ(matrix.value().AsMatrix(),
            (ConfigMatrix{{1.0, 2.0}, {3.5, 4.0}}));

  Result<ConfigValue> empty_vector =
      ConfigParser::TryParseValue("vector[]");
  ASSERT_TRUE(empty_vector.ok());
  EXPECT_TRUE(empty_vector.value().AsVector().empty());

  Result<ConfigValue> empty_matrix =
      ConfigParser::TryParseValue("matrix[]");
  ASSERT_TRUE(empty_matrix.ok());
  EXPECT_TRUE(empty_matrix.value().AsMatrix().empty());
}

TEST(ConfigParserTest, RejectsMalformedOverflowAndNonFiniteValues) {
  const std::string integer_overflow =
      std::to_string(static_cast<std::int64_t>(
                         std::numeric_limits<int>::max()) +
                     1);
  EXPECT_FALSE(ConfigParser::TryParseValue("").ok());
  EXPECT_FALSE(ConfigParser::TryParseValue(integer_overflow).ok());
  EXPECT_FALSE(ConfigParser::TryParseValue("1e9999").ok());
  EXPECT_FALSE(ConfigParser::TryParseValue("nan").ok());
  EXPECT_FALSE(ConfigParser::TryParseValue("inf").ok());
  EXPECT_FALSE(ConfigParser::TryParseValue("\"bad\\xescape\"").ok());
  EXPECT_FALSE(ConfigParser::TryParseValue("vector[1.0,]").ok());
  EXPECT_FALSE(ConfigParser::TryParseValue("matrix[[1.0], [2.0, 3.0]]").ok());
  EXPECT_FALSE(ConfigParser::TryParseValue("matrix[[1.0],").ok());
}

TEST(ConfigParserTest, SerializesCanonicalTextInLexicalKeyOrder) {
  ConfigParser parser;
  ASSERT_TRUE(parser.TryAddConfig("zeta", ConfigValue(7)).ok());
  ASSERT_TRUE(parser.TryAddConfig(
                        "message", ConfigValue("line\nquote\"slash\\tab\t"))
                  .ok());
  ASSERT_TRUE(parser.TryAddConfig("alpha", ConfigValue(true)).ok());
  ASSERT_TRUE(parser.TryAddConfig("empty", ConfigValue()).ok());
  ASSERT_TRUE(parser.TryAddConfig("vector",
                                  ConfigValue(ConfigVector{1.0, -2.5}))
                  .ok());
  ASSERT_TRUE(parser.TryAddConfig(
                        "matrix",
                        ConfigValue(ConfigMatrix{{1.0, 2.5}, {3.0, 4.0}}))
                  .ok());

  Result<std::string> serialized = parser.Serialize();
  ASSERT_TRUE(serialized.ok()) << serialized.status().message();
  EXPECT_EQ(serialized.value(),
            "alpha = true\n"
            "empty = null\n"
            "matrix = matrix[[1.0, 2.5], [3.0, 4.0]]\n"
            "message = \"line\\nquote\\\"slash\\\\tab\\t\"\n"
            "vector = vector[1.0, -2.5]\n"
            "zeta = 7\n");
}

TEST(ConfigParserTest, CanonicalTextRoundTripsEveryType) {
  const std::string input =
      "nothing = null\n"
      "boolean = false\n"
      "integer = -9\n"
      "real = 0.125\n"
      "string = \"value # not a comment\" # comment\n"
      "vector = vector[]\n"
      "matrix = matrix[[], []]\n";

  ConfigParser first;
  ASSERT_TRUE(first.TryLoadFromString(input, UnknownConfigKeyPolicy::kAdd)
                  .ok());
  Result<std::string> canonical = first.Serialize();
  ASSERT_TRUE(canonical.ok());

  ConfigParser second;
  ASSERT_TRUE(second
                  .TryLoadFromString(canonical.value(),
                                     UnknownConfigKeyPolicy::kAdd)
                  .ok());
  Result<std::string> repeated = second.Serialize();
  ASSERT_TRUE(repeated.ok());
  EXPECT_EQ(repeated.value(), canonical.value());
}

TEST(ConfigParserTest, ValidatorsAndFailedUpdatesAreTransactional) {
  ConfigParser parser;
  auto positive = [](const ConfigValue& value) {
    return value.IsInt() && value.AsInt() > 0;
  };
  ASSERT_TRUE(parser.TryAddConfig("count", ConfigValue(3), positive).ok());
  ASSERT_TRUE(parser.TryAddConfig("mode", ConfigValue("safe")).ok());

  const Status rejected = parser.TrySetConfig("count", ConfigValue(-1));
  EXPECT_FALSE(rejected.ok());
  EXPECT_EQ(rejected.code(), StatusCode::kInvalidArgument);
  EXPECT_NE(rejected.message().find("count"), std::string_view::npos);
  Result<ConfigValue> count = parser.FindConfig("count");
  ASSERT_TRUE(count.ok());
  EXPECT_EQ(count.value().AsInt(), 3);

  const Status load = parser.TryLoadFromString(
      "mode = \"fast\"\ncount = -2\n",
      UnknownConfigKeyPolicy::kReject);
  EXPECT_FALSE(load.ok());
  Result<ConfigValue> mode = parser.FindConfig("mode");
  ASSERT_TRUE(mode.ok());
  EXPECT_EQ(mode.value().AsString(), "safe");
  count = parser.FindConfig("count");
  ASSERT_TRUE(count.ok());
  EXPECT_EQ(count.value().AsInt(), 3);
}

TEST(ConfigParserTest, EnforcesNamesUnknownKeysAndDuplicateInput) {
  ConfigParser parser;
  ASSERT_TRUE(parser.TryAddConfig("known", ConfigValue(1)).ok());

  EXPECT_FALSE(parser.TryAddConfig("", ConfigValue(1)).ok());
  EXPECT_FALSE(parser.TryAddConfig("1bad", ConfigValue(1)).ok());
  EXPECT_FALSE(parser.TryAddConfig("bad key", ConfigValue(1)).ok());
  EXPECT_FALSE(parser.TrySetConfig("missing", ConfigValue(1)).ok());
  EXPECT_FALSE(parser.FindConfig("missing").ok());

  EXPECT_FALSE(parser.TryLoadFromString("known = 2\nunknown = 3\n").ok());
  EXPECT_FALSE(parser.HasConfig("unknown"));
  Result<ConfigValue> known = parser.FindConfig("known");
  ASSERT_TRUE(known.ok());
  EXPECT_EQ(known.value().AsInt(), 1);

  EXPECT_FALSE(parser.TryLoadFromString("known = 2\nknown = 3\n").ok());
  known = parser.FindConfig("known");
  ASSERT_TRUE(known.ok());
  EXPECT_EQ(known.value().AsInt(), 1);

  ASSERT_TRUE(parser
                  .TryLoadFromString("known = 2\nunknown = 3\n",
                                     UnknownConfigKeyPolicy::kAdd)
                  .ok());
  EXPECT_EQ(parser.FindConfig("known").value().AsInt(), 2);
  EXPECT_EQ(parser.FindConfig("unknown").value().AsInt(), 3);
}

TEST(ConfigParserTest, RejectsUnrepresentableStructuredValues) {
  ConfigParser parser;
  EXPECT_FALSE(parser
                   .TryAddConfig("ragged",
                                 ConfigValue(ConfigMatrix{{1.0}, {2.0, 3.0}}))
                   .ok());
  EXPECT_FALSE(parser
                   .TryAddConfig(
                       "nonfinite",
                       ConfigValue(std::numeric_limits<double>::infinity()))
                   .ok());
  EXPECT_FALSE(parser
                   .TryAddConfig(
                       "vector",
                       ConfigValue(ConfigVector{
                           1.0, std::numeric_limits<double>::quiet_NaN()}))
                   .ok());
}

TEST(ConfigParserTest, FileLoadingUsesStatusAndPreservesStateOnFailure) {
  ConfigParser parser;
  ASSERT_TRUE(parser.TryAddConfig("value", ConfigValue(1)).ok());

  const auto identity = reinterpret_cast<std::uintptr_t>(&parser);
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() /
      ("asc_cpp_utilities_config_" + std::to_string(identity) + ".cfg");
  {
    std::ofstream output(path);
    ASSERT_TRUE(output.good());
    output << "value = 8\n";
  }

  EXPECT_TRUE(parser.TryLoadFromFile(path.string()).ok());
  EXPECT_EQ(parser.FindConfig("value").value().AsInt(), 8);

  const std::filesystem::path missing = path.string() + ".missing";
  EXPECT_FALSE(parser.TryLoadFromFile(missing.string()).ok());
  EXPECT_EQ(parser.FindConfig("value").value().AsInt(), 8);

  std::error_code error;
  std::filesystem::remove(path, error);
}

}  // namespace
}  // namespace asc
