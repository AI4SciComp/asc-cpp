// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: tests/unit_test_optparser.cc
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#include <gtest/gtest.h>
#include <cstdint>
#include <limits>
#include <fstream>
#include <sstream>
#include <filesystem>
#include "common/common.h"
#include "asc/core/casts.h"
#include "asc/core/math.h"
#include "asc/core/string.h"
#include "asc/utilities/config.h"
#include "asc/utilities/optparser.h"
#include "asc/utilities/timer.h"

namespace asc {

namespace {

#ifdef ASC_USE_EXCEPTION
#define EXPECT_ASC_FAILURE(statement) EXPECT_THROW(statement, ErrorException)
#else
#define EXPECT_ASC_FAILURE(statement) EXPECT_DEATH(statement, "ASC")
#endif

}  // namespace

// ============================================================================
// Core utility tests
// ============================================================================

TEST(CoreStringTest, TrimCommentsCaseAndPositiveInt) {
  EXPECT_EQ(Trim(" \tAlpha \r\n"), "Alpha");
  EXPECT_EQ(RemoveComment("value = 3 # comment"), "value = 3 ");
  EXPECT_EQ(RemoveComment("value = 3 ; comment", ';'), "value = 3 ");
  EXPECT_TRUE(EqualsIgnoreCase("Periodic", "periodic"));
  EXPECT_FALSE(EqualsIgnoreCase("Periodic", "dirichlet"));
  EXPECT_TRUE(IsDigitAscii('7'));
  EXPECT_FALSE(IsDigitAscii('x'));
  EXPECT_EQ(ParsePositiveInt("12345"), 12345);
  EXPECT_ASC_FAILURE(ParsePositiveInt("12x"));
}

TEST(CoreCastsTest, FromStringConsumesCompleteInput) {
  EXPECT_EQ(FromString<int>(" 42 "), 42);
  EXPECT_DOUBLE_EQ(FromString<double>(" 2.5 "), 2.5);
  EXPECT_TRUE(FromString<bool>(" YES "));
  EXPECT_FALSE(FromString<bool>(" off "));

  EXPECT_ASC_FAILURE(FromString<int>("42abc"));
  EXPECT_ASC_FAILURE(FromString<double>("2.5abc"));
  EXPECT_ASC_FAILURE(FromString<bool>("maybe"));
}

TEST(CoreNumericTest, FuzzyComparisonsAndCombinatorics) {
  EXPECT_TRUE(FuzzyEqual(1.0e8, 1.0e8 + 1.0, 1.0e-8));
  EXPECT_FALSE(FuzzyNotEqual(1.0e8, 1.0e8 + 1.0, 1.0e-8));
  EXPECT_TRUE(FuzzyNotEqual(1.0, 1.1, 1.0e-12));
  EXPECT_TRUE(FuzzyLess(1.0, 1.0 + 2.0e-6, 1.0e-6));
  EXPECT_FALSE(FuzzyLess(1.0, 1.0 + 0.5e-6, 1.0e-6));
  EXPECT_TRUE(FuzzyGreater(1.0 + 2.0e-6, 1.0, 1.0e-6));
  EXPECT_FALSE(FuzzyGreater(1.0 + 0.5e-6, 1.0, 1.0e-6));

  EXPECT_EQ(Factorial<int>(5), 120);
  EXPECT_EQ(Binomial<int>(5, 2), 10);
  EXPECT_EQ(Binomial<int>(3, 4), 0);
  EXPECT_ASC_FAILURE(Factorial<int>(-1));
  EXPECT_ASC_FAILURE(Binomial<int>(1, -1));
}

TEST(ConfigParserTest, ParsesGenericValuesAndRejectsOverflow) {
  EXPECT_TRUE(ConfigParser::ParseValue("TRUE").AsBool());
  EXPECT_FALSE(ConfigParser::ParseValue("false").AsBool());
  EXPECT_EQ(ConfigParser::ParseValue("42").AsInt(), 42);
  EXPECT_DOUBLE_EQ(ConfigParser::ParseValue("0.25").AsDouble(), 0.25);
  EXPECT_EQ(ConfigParser::ParseValue("42abc").AsString(), "42abc");

  const std::string too_large =
      std::to_string(static_cast<std::int64_t>(std::numeric_limits<int>::max()) +
                     1);
  EXPECT_ASC_FAILURE(ConfigParser::ParseValue(too_large));
}

TEST(ConfigParserTest, UsesStandardContainersForStructuredValues) {
  const ConfigVector vector = {1.0_r, 2.0_r, 3.0_r};
  const ConfigMatrix matrix = {{1.0_r, 2.0_r}, {3.0_r, 4.0_r}};

  const ConfigValue vector_value(vector);
  const ConfigValue matrix_value(matrix);

  EXPECT_EQ(vector_value.AsVector(), vector);
  EXPECT_EQ(matrix_value.AsMatrix(), matrix);
}

TEST(TimerTest, StopReturnsTheRecordedInterval) {
  Timer timer;
  timer.Start();
  const real_t elapsed = timer.Stop();

  EXPECT_GE(elapsed, 0.0_r);
  EXPECT_EQ(timer.TotalTime(), elapsed);
  EXPECT_EQ(timer.LastTime(), elapsed);
}

// ============================================================================
// Basic Option Tests
// ============================================================================

// Test adding and retrieving options
TEST(OptionParserTest, AddOptionAndRetrieveOptions) {
  OptionParser parser("Test parser");

  // AddOption integer option
  auto int_opt =
      parser.AddOption<Variable<int>>("i", "int-option", "An integer option");

  // AddOption double option
  auto double_opt = parser.AddOption<Variable<double>>("d", "double-option",
                                                       "A double option");

  // AddOption string option
  auto str_opt = parser.AddOption<Variable<std::string>>("s", "string-option",
                                                         "A string option");

  // Retrieve by short name
  auto retrieved_int = parser.GetOption<Variable<int>>("i");
  ASSERT_NE(retrieved_int, nullptr);
  EXPECT_EQ(retrieved_int, int_opt);

  // Retrieve by long name
  auto retrieved_double =
      parser.GetOption<Variable<double>>(std::string("double-option"));
  ASSERT_NE(retrieved_double, nullptr);
  EXPECT_EQ(retrieved_double, double_opt);

  EXPECT_ASC_FAILURE(parser.GetOption<Variable<int>>("missing-option"));
  EXPECT_ASC_FAILURE(parser.AddOption<Variable<int>>(
      "i", "duplicate-short", "Duplicate short option"));
}

// Test default values
TEST(OptionParserTest, DefaultValues) {
  OptionParser parser;

  // AddOption option with default value
  auto int_opt = parser.AddOption<Variable<int>>("i", "integer",
                                                 "Integer with default", 42);

  auto double_opt = parser.AddOption<Variable<double>>(
      "d", "double", "Double with default", 3.14);

  auto str_opt = parser.AddOption<Variable<std::string>>(
      "s", "string", "String with default", std::string("hello"));

  // Check defaults are set
  EXPECT_TRUE(int_opt->HasDefault());
  EXPECT_TRUE(double_opt->HasDefault());
  EXPECT_TRUE(str_opt->HasDefault());

  // Values should be defaults before parsing
  EXPECT_EQ(int_opt->GetValue(), "42");
  EXPECT_EQ(double_opt->GetValue(), "3.14");
  EXPECT_EQ(str_opt->GetValue(), "hello");
}

// Test switch options
TEST(OptionParserTest, SwitchOptions) {
  OptionParser parser;

  bool flag = false;
  auto switch_opt =
      parser.AddOption<Switch>("v", "verbose", "Verbose mode", &flag);

  // Initially not set
  EXPECT_FALSE(switch_opt->IsSet());
  EXPECT_FALSE(flag);

  // Parse with switch
  const char* argv[] = {"program", "-v"};
  parser.Parse(2, argv);

  EXPECT_TRUE(switch_opt->IsSet());
  EXPECT_TRUE(flag);
}

// ============================================================================
// Parsing Tests
// ============================================================================

// Test command-line argument parsing
TEST(OptionParserTest, ParseCommandLineArguments) {
  OptionParser parser;

  int int_val = 0;
  double double_val = 0.0;
  std::string str_val;

  parser.AddOption<Variable<int>>("i", "int", "Integer", 0, &int_val);
  parser.AddOption<Variable<double>>("d", "double", "Double", 0.0, &double_val);
  parser.AddOption<Variable<std::string>>("s", "string", "String",
                                          std::string(""), &str_val);

  const char* argv[] = {"program", "-i", "123",        "-d",
                        "3.14159", "-s", "test_string"};

  parser.Parse(7, argv);

  EXPECT_EQ(int_val, 123);
  EXPECT_DOUBLE_EQ(double_val, 3.14159);
  EXPECT_EQ(str_val, "test_string");
}

// Test parsing with long option names
TEST(OptionParserTest, ParseLongOptionNames) {
  OptionParser parser;

  int value = 0;
  parser.AddOption<Variable<int>>("n", "number", "A number", 0, &value);

  const char* argv[] = {"program", "--number", "999"};
  parser.Parse(3, argv);

  EXPECT_EQ(value, 999);
}

// Test parsing with mixed short and long names
TEST(OptionParserTest, ParseMixedOptions) {
  OptionParser parser;

  int val1 = 0, val2 = 0;
  parser.AddOption<Variable<int>>("a", "alpha", "First", 0, &val1);
  parser.AddOption<Variable<int>>("b", "beta", "Second", 0, &val2);

  const char* argv[] = {"program", "-a", "100", "--beta", "200"};

  parser.Parse(5, argv);

  EXPECT_EQ(val1, 100);
  EXPECT_EQ(val2, 200);
}

// Test parsing from file
TEST(OptionParserTest, ParseFromFile) {
  namespace fs = std::filesystem;

  // Create temporary config file
  const fs::path file = fs::temp_directory_path() / "test_optparser.ini";
  {
    std::ofstream ofs(file);
    ofs << "# Test config file\n";
    ofs << "int-value = 42\n";
    ofs << "double-value = 2.718\n";
    ofs << "string-value = from_file\n";
    ofs << "\n";
    ofs << "# Comment line\n";
    ofs << "another-int = 999\n";
  }
  ASSERT_TRUE(fs::exists(file)) << "Temp config file was not created: " << file;

  OptionParser parser;

  int int_val = 0;
  double double_val = 0.0;
  std::string str_val;
  int another_int = 0;

  parser.AddOption<Variable<int>>("i", "int-value", "Int", 0, &int_val);
  parser.AddOption<Variable<double>>("d", "double-value", "Double", 0.0,
                                     &double_val);
  parser.AddOption<Variable<std::string>>("s", "string-value", "String",
                                          std::string(""), &str_val);
  parser.AddOption<Variable<int>>("a", "another-int", "Another", 0,
                                  &another_int);

  parser.Parse(file.string().c_str());

  EXPECT_EQ(int_val, 42);
  EXPECT_DOUBLE_EQ(double_val, 2.718);
  EXPECT_EQ(str_val, "from_file");
  EXPECT_EQ(another_int, 999);

  // Clean up
  std::error_code ec;
  fs::remove(file, ec);
}

// ============================================================================
// Value Assignment Tests
// ============================================================================

// Test AssignTo functionality
TEST(OptionParserTest, AssignToPointer) {
  OptionParser parser;

  int external_var = 0;
  auto opt = parser.AddOption<Variable<int>>("v", "value", "Value");

  // Initially not assigned
  EXPECT_EQ(external_var, 0);

  // Assign to external variable
  opt->AssignTo(&external_var);

  const char* argv[] = {"program", "-v", "777"};
  parser.Parse(3, argv);

  EXPECT_EQ(external_var, 777);
}

// Test updating assigned pointer with default value
TEST(OptionParserTest, AssignToWithDefault) {
  OptionParser parser;

  int external_var = 999;  // Initial value
  auto opt = parser.AddOption<Variable<int>>("v", "value", "Value with default",
                                             42, &external_var);

  // Should have default value before parsing
  EXPECT_EQ(external_var, 42);

  // Parse without setting the option
  const char* argv[] = {"program"};
  parser.Parse(1, argv);

  // Should still have default
  EXPECT_EQ(external_var, 42);
}

TEST(OptionParserTest, MissingConfigFileFailsInReleasePath) {
  OptionParser parser;
  EXPECT_ASC_FAILURE(parser.Parse("missing_asc_config_file.ini"));
}

// ============================================================================
// Type Conversion Tests
// ============================================================================

// Test parsing different numeric types
TEST(OptionParserTest, NumericTypes) {
  OptionParser parser;

  int int_val = 0;
  float float_val = 0.0f;
  double double_val = 0.0;

  parser.AddOption<Variable<int>>("i", "int", "Int", 0, &int_val);
  parser.AddOption<Variable<float>>("f", "float", "Float", 0.0f, &float_val);
  parser.AddOption<Variable<double>>("d", "double", "Double", 0.0, &double_val);

  const char* argv[] = {"program", "-i", "123", "-f", "1.23", "-d", "4.56789"};

  parser.Parse(7, argv);

  EXPECT_EQ(int_val, 123);
  EXPECT_FLOAT_EQ(float_val, 1.23f);
  EXPECT_DOUBLE_EQ(double_val, 4.56789);
}

// ============================================================================
// IsSet Tests
// ============================================================================

// Test IsSet functionality
TEST(OptionParserTest, IsSetFlag) {
  OptionParser parser;

  auto opt1 = parser.AddOption<Variable<int>>("a", "alpha", "Alpha", 10);
  auto opt2 = parser.AddOption<Variable<int>>("b", "beta", "Beta", 20);

  const char* argv[] = {"program", "-a", "100"};
  parser.Parse(3, argv);

  EXPECT_TRUE(opt1->IsSet());
  EXPECT_FALSE(opt2->IsSet());  // Not set, should use default
}

// ============================================================================
// Edge Cases and Error Handling
// ============================================================================

// Test empty parser
TEST(OptionParserTest, EmptyParser) {
  OptionParser parser;

  const char* argv[] = {"program"};

  // Should not crash with no options defined
  EXPECT_NO_THROW(parser.Parse(1, argv));
}

// Test option with spaces in string value
TEST(OptionParserTest, StringWithSpaces) {
  OptionParser parser;

  std::string str_val;
  parser.AddOption<Variable<std::string>>("s", "string", "String",
                                          std::string(""), &str_val);

  const char* argv[] = {"program", "-s", "hello world"};

  parser.Parse(3, argv);

  EXPECT_EQ(str_val, "hello world");
}

// ============================================================================
// Help and Usage Tests
// ============================================================================

// Test PrintHelp
TEST(OptionParserTest, PrintHelp) {
  OptionParser parser("Test application");

  parser.AddOption<Variable<int>>("i", "int", "Integer option", 42);
  parser.AddOption<Variable<std::string>>("s", "string", "String option");
  parser.AddOption<Switch>("v", "verbose", "Verbose mode");

  std::ostringstream oss;
  parser.PrintHelp(oss);

  std::string help = oss.str();

  // Check that help message contains expected content
  EXPECT_NE(help.find("Test application"), std::string::npos);
  EXPECT_NE(help.find("i"), std::string::npos);
  EXPECT_NE(help.find("--int"), std::string::npos);
  EXPECT_NE(help.find("Integer option"), std::string::npos);
}

// Test PrintUsage
TEST(OptionParserTest, PrintUsage) {
  OptionParser parser("Test app");

  parser.AddOption<Variable<int>>("i", "int", "Int");
  parser.AddOption<Switch>("v", "verbose", "Verbose");

  std::ostringstream oss;
  parser.PrintUsage(oss);

  std::string usage = oss.str();

  // Check that usage message is generated
  EXPECT_FALSE(usage.empty());
}

// ============================================================================
// Multiple Option Values
// ============================================================================

// Test multiple options of same type
TEST(OptionParserTest, MultipleOptionsOfSameType) {
  OptionParser parser;

  int val1 = 0, val2 = 0, val3 = 0;

  parser.AddOption<Variable<int>>("a", "alpha", "Alpha", 0, &val1);
  parser.AddOption<Variable<int>>("b", "beta", "Beta", 0, &val2);
  parser.AddOption<Variable<int>>("c", "gamma", "Gamma", 0, &val3);

  const char* argv[] = {"program", "-a", "1", "-b", "2", "-c", "3"};

  parser.Parse(7, argv);

  EXPECT_EQ(val1, 1);
  EXPECT_EQ(val2, 2);
  EXPECT_EQ(val3, 3);
}

}  // namespace asc
