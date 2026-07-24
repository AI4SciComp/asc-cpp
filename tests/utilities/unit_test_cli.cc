#include <asc/core/contracts.h>
#include <asc/core/status.h>
#include <asc/utilities/cli.h>
#include <asc/utilities/optparser.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>

namespace asc {
namespace {

TEST(OptionParserTest, ParsesShortLongEqualsAndNegativeNumericValues) {
  OptionParser parser;
  int integer = 0;
  double decimal = 0.0;
  double exponent = 0.0;

  parser.AddOption<Variable<int>>("i", "integer", "integer", 0, &integer);
  parser.AddOption<Variable<double>>("d", "decimal", "decimal", 0.0,
                                     &decimal);
  parser.AddOption<Variable<double>>("e", "exponent", "exponent", 0.0,
                                     &exponent);

  const char* argv[] = {"program", "-i", "-7", "--decimal", "-0.25",
                        "--exponent=-1e-6"};
  const Status status = parser.TryParse(6, argv);

  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_EQ(integer, -7);
  EXPECT_DOUBLE_EQ(decimal, -0.25);
  EXPECT_DOUBLE_EQ(exponent, -1.0e-6);
}

TEST(OptionParserTest, SwitchDoesNotConsumeFollowingOption) {
  OptionParser parser;
  bool verbose = false;
  int count = 0;
  parser.AddOption<Switch>("v", "verbose", "verbose", &verbose);
  parser.AddOption<Variable<int>>("n", "count", "count", 0, &count);

  const char* argv[] = {"program", "-v", "-n", "-3"};
  const Status status = parser.TryParse(4, argv);

  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_TRUE(verbose);
  EXPECT_EQ(count, -3);
}

TEST(OptionParserTest, ValueOptionConsumesHyphenPrefixedString) {
  OptionParser parser;
  std::string text;
  parser.AddOption<Variable<std::string>>(
      "t", "text", "text", std::string("default"), &text);

  const char* argv[] = {"program", "--text", "--not-an-option"};
  const Status status = parser.TryParse(3, argv);

  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_EQ(text, "--not-an-option");
}

TEST(OptionParserTest, RepeatedOccurrencesUseLastValue) {
  OptionParser parser;
  int value = 0;
  parser.AddOption<Variable<int>>("n", "number", "number", 0, &value);

  const char* argv[] = {"program", "-n", "1", "--number=2", "-n", "3"};
  const Status status = parser.TryParse(6, argv);

  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_EQ(value, 3);
}

TEST(OptionParserTest, SuccessfulRepeatedParseResetsDefaultsAndSetState) {
  OptionParser parser;
  int value = 4;
  auto option =
      parser.AddOption<Variable<int>>("n", "number", "number", 4, &value);

  const char* first[] = {"program", "-n", "9"};
  ASSERT_TRUE(parser.TryParse(3, first).ok());
  EXPECT_TRUE(option->IsSet());
  EXPECT_EQ(value, 9);

  const char* second[] = {"program"};
  ASSERT_TRUE(parser.TryParse(1, second).ok());
  EXPECT_FALSE(option->IsSet());
  EXPECT_EQ(value, 4);
}

TEST(OptionParserTest, FailedParseLeavesCommittedValuesUnchanged) {
  OptionParser parser;
  int count = 1;
  std::string mode = "safe";
  auto count_option =
      parser.AddOption<Variable<int>>("n", "count", "count", 1, &count);
  auto mode_option = parser.AddOption<Variable<std::string>>(
      "m", "mode", "mode", std::string("safe"), &mode);

  const char* committed[] = {"program", "--count=5", "--mode", "fast"};
  ASSERT_TRUE(parser.TryParse(4, committed).ok());
  ASSERT_TRUE(count_option->IsSet());
  ASSERT_TRUE(mode_option->IsSet());

  const char* failed[] = {"program", "--count=8", "position"};
  EXPECT_FALSE(parser.TryParse(3, failed).ok());
  EXPECT_EQ(count, 5);
  EXPECT_EQ(mode, "fast");
  EXPECT_EQ(count_option->GetValue(), "5");
  EXPECT_EQ(mode_option->GetValue(), "fast");
  EXPECT_TRUE(count_option->IsSet());
  EXPECT_TRUE(mode_option->IsSet());
}

TEST(OptionParserTest, ReportsUnknownMissingInvalidAndPositionalInputs) {
  OptionParser parser;
  parser.AddOption<Variable<int>, kRequired>("n", "number", "number");

  const char* unknown[] = {"program", "--unknown"};
  EXPECT_FALSE(parser.TryParse(2, unknown).ok());

  const char* missing[] = {"program", "--number"};
  EXPECT_FALSE(parser.TryParse(2, missing).ok());

  const char* invalid[] = {"program", "--number", "not-an-integer"};
  EXPECT_FALSE(parser.TryParse(3, invalid).ok());

  const std::string overflow =
      std::to_string(static_cast<std::int64_t>(
                         std::numeric_limits<int>::max()) +
                     1);
  const char* overflow_argv[] = {"program", "--number", overflow.c_str()};
  EXPECT_FALSE(parser.TryParse(3, overflow_argv).ok());

  const char* positional[] = {"program", "input.dat"};
  EXPECT_FALSE(parser.TryParse(2, positional).ok());

  const char* after_separator[] = {"program", "--number", "1", "--",
                                   "input.dat"};
  EXPECT_FALSE(parser.TryParse(5, after_separator).ok());

  const char* required[] = {"program"};
  EXPECT_FALSE(parser.TryParse(1, required).ok());
}

TEST(OptionParserTest, LongNamesAndLargeHelpTextAreNotTruncated) {
  OptionParser parser("large help");
  std::string description;
  for (int line = 0; line < 600; ++line) {
    description += "description-line-" + std::to_string(line) + '\n';
  }
  int value = 0;
  auto option = parser.AddOption<Variable<int>>(
      "long-short-name", "long-option-name", description, 0, &value);

  EXPECT_STREQ(option->GetShortName(), "long-short-name");
  const char* argv[] = {"program", "-long-short-name", "7"};
  ASSERT_TRUE(parser.TryParse(3, argv).ok());
  EXPECT_EQ(value, 7);

  const std::string help = parser.GetHelp();
  EXPECT_NE(help.find("description-line-0"), std::string::npos);
  EXPECT_NE(help.find("description-line-599"), std::string::npos);
  EXPECT_NE(help.find("long-short-name"), std::string::npos);
  EXPECT_NE(help.find("long-option-name"), std::string::npos);
}

TEST(OptionParserTest, ExplicitSinksMatchGeneratedHelpAndUsage) {
  OptionParser parser("application");
  parser.AddOption<Variable<int>>("n", "number", "number", 3);
  parser.AddOption<Switch>("v", "verbose", "verbose");

  std::ostringstream help;
  parser.PrintHelp(help);
  EXPECT_EQ(help.str(), parser.GetHelp());

  std::ostringstream usage;
  parser.PrintUsage(usage);
  EXPECT_EQ(usage.str(), parser.GetUsage());
}

TEST(OptionParserTest, FileParsingIsTransactionalAndSupportsNegativeValues) {
  OptionParser parser;
  double value = 1.0;
  parser.AddOption<Variable<double>>("v", "value", "value", 1.0, &value);

  const auto identity = reinterpret_cast<std::uintptr_t>(&parser);
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() /
      ("asc_cpp_utilities_cli_" + std::to_string(identity) + ".cfg");
  {
    std::ofstream output(path);
    ASSERT_TRUE(output.good());
    output << "# compatibility CLI file\nvalue = -2.5e-3\n";
  }

  ASSERT_TRUE(parser.TryParseFile(path.string()).ok());
  EXPECT_DOUBLE_EQ(value, -2.5e-3);

  {
    std::ofstream output(path);
    ASSERT_TRUE(output.good());
    output << "value = 4.0\nunknown = 2\n";
  }
  EXPECT_FALSE(parser.TryParseFile(path.string()).ok());
  EXPECT_DOUBLE_EQ(value, -2.5e-3);
  EXPECT_FALSE(parser.TryParseFile(path.string() + ".missing").ok());
  EXPECT_DOUBLE_EQ(value, -2.5e-3);

  std::error_code error;
  std::filesystem::remove(path, error);
}

#ifdef ASC_USE_EXCEPTION
TEST(OptionParserTest, RejectsInvalidAndDuplicateDeclarations) {
  OptionParser parser;
  parser.AddOption<Variable<int>>("n", "number", "number");

  EXPECT_ANY_THROW(
      parser.AddOption<Variable<int>>("n", "other", "other"));
  EXPECT_ANY_THROW(
      parser.AddOption<Variable<int>>("o", "number", "other"));
  EXPECT_ANY_THROW(parser.AddOption<Variable<int>>("", "", "empty"));
  EXPECT_ANY_THROW(
      parser.AddOption<Variable<int>>("bad name", "valid", "bad"));
  EXPECT_ANY_THROW(
      parser.AddOption<Variable<int>>("valid", "bad=name", "bad"));
}
#endif

}  // namespace
}  // namespace asc
