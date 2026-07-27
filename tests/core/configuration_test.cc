#include "asc/core/configuration.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

#include "asc/core/contracts.h"
#include "test_support.h"

namespace {

using Value = asc::ConfigurationValue;
using ValueType = asc::ConfigurationValueType;

static_assert(!std::is_constructible_v<Value, const char*>);
static_assert(!std::is_constructible_v<Value, char*>);
static_assert(!std::is_constructible_v<Value, std::string>);
static_assert(
    static_cast<std::uint8_t>(asc::ConfigurationOriginKind::kCommandLine) == 2);

Value ValidUtf8(std::string value) {
  auto result = Value::Utf8String(std::move(value));
  ASC_CHECK_MESSAGE(result.ok(), "A test fixture must contain valid UTF-8");
  return std::move(*result);
}

void CheckValueAlternatives(asc_core_test::TestContext& context) {
  const Value null_value;
  ASC_TEST_EQ(context, null_value.type(), ValueType::kNull);
  ASC_TEST_CHECK(context, null_value.is_null());

  const Value bool_value(true);
  ASC_TEST_EQ(context, bool_value.type(), ValueType::kBool);
  ASC_TEST_EQ(context, *bool_value.AsBool(), true);
  ASC_TEST_CHECK(context, !bool_value.AsSignedInteger().ok());

  const Value signed_value(std::int64_t{-17});
  ASC_TEST_EQ(context, signed_value.type(), ValueType::kSignedInteger);
  ASC_TEST_EQ(context, *signed_value.AsSignedInteger(), -17);
  ASC_TEST_CHECK(context, !signed_value.AsUnsignedInteger().ok());
  ASC_TEST_EQ(context, Value(-1).type(), ValueType::kSignedInteger);

  const Value unsigned_value(std::uint64_t{23});
  ASC_TEST_EQ(context, unsigned_value.type(), ValueType::kUnsignedInteger);
  ASC_TEST_EQ(context, *unsigned_value.AsUnsignedInteger(), 23U);
  ASC_TEST_CHECK(context, !unsigned_value.AsSignedInteger().ok());
  ASC_TEST_EQ(context, Value(1U).type(), ValueType::kUnsignedInteger);

  const Value double_value(2.5);
  ASC_TEST_EQ(context, double_value.type(), ValueType::kDouble);
  ASC_TEST_EQ(context, *double_value.AsDouble(), 2.5);

  const Value string_value = ValidUtf8("text");
  ASC_TEST_EQ(context, string_value.type(), ValueType::kString);
  ASC_TEST_EQ(context, string_value.AsString()->get(), std::string("text"));

  const Value list_value(Value::List{Value(nullptr), Value(std::int64_t{5})});
  ASC_TEST_EQ(context, list_value.type(), ValueType::kList);
  ASC_TEST_EQ(context, list_value.AsList()->get().size(), std::size_t{2});

  const Value object_value(Value::Object{{"field", Value(std::uint64_t{9})}});
  ASC_TEST_EQ(context, object_value.type(), ValueType::kObject);
  ASC_TEST_EQ(context, object_value.AsObject()->get().size(), std::size_t{1});

  for (std::uint8_t index = 0; index <= 7; ++index) {
    ASC_TEST_CHECK(
        context, !asc::ConfigurationValueTypeName(static_cast<ValueType>(index))
                      .empty());
  }
}

void CheckUtf8(asc_core_test::TestContext& context) {
  const std::string valid = std::string("ASCII ") + "\xc2\xa2 " +
                            "\xe2\x82\xac " + "\xf0\x9f\x98\x80";
  const auto valid_value = Value::Utf8String(valid);
  ASC_TEST_CHECK(context, valid_value.ok());
  ASC_TEST_EQ(context, valid_value->AsString()->get(), valid);
  ASC_TEST_CHECK(context, Value::Utf8String("").ok());
  ASC_TEST_CHECK(context, Value::Utf8String(std::string("a\0b", 3)).ok());

  const std::string invalid_values[] = {
      std::string("\x80", 1),
      std::string("\xc2", 1),
      std::string("\xe2\x82", 2),
      std::string("\xf0\x9f\x98", 3),
      std::string("\xc0\xaf", 2),
      std::string("\xe0\x80\xaf", 3),
      std::string("\xed\xa0\x80", 3),
      std::string("\xf4\x90\x80\x80", 4),
      std::string("\xf5\x80\x80\x80", 4),
      std::string("\xe2\x28\xa1", 3),
  };
  for (const std::string& invalid : invalid_values) {
    const auto result = Value::Utf8String(invalid);
    ASC_TEST_CHECK(context, !result.ok());
    ASC_TEST_EQ(context, result.status().code(), asc::ErrorCode::kEncoding);
  }
}

asc::ConfigurationSchema MakeSchema(asc_core_test::TestContext& context) {
  asc::ConfigurationSchema root(ValueType::kObject);
  ASC_TEST_CHECK(context,
                 root.SetSizeBounds(std::size_t{2}, std::size_t{8}).ok());

  asc::ConfigurationSchema name(ValueType::kString);
  name.SetRequired(true);
  ASC_TEST_CHECK(context,
                 name.SetSizeBounds(std::size_t{2}, std::size_t{16}).ok());
  ASC_TEST_CHECK(context, root.AddField("name", std::move(name)).ok());

  asc::ConfigurationSchema count(ValueType::kSignedInteger);
  ASC_TEST_CHECK(context, count.SetSignedBounds(-4, 8).ok());
  ASC_TEST_CHECK(context, root.AddField("count", std::move(count)).ok());

  asc::ConfigurationSchema threads(ValueType::kUnsignedInteger);
  threads.SetDefault(Value(std::uint64_t{2}));
  ASC_TEST_CHECK(context, threads.SetUnsignedBounds(1, 64).ok());
  ASC_TEST_CHECK(context, root.AddField("threads", std::move(threads)).ok());

  asc::ConfigurationSchema secret(ValueType::kString);
  secret.SetSensitive(true);
  ASC_TEST_CHECK(context, root.AddField("secret", std::move(secret)).ok());

  asc::ConfigurationSchema legacy(ValueType::kBool);
  legacy.SetDeprecated(true);
  ASC_TEST_CHECK(context, root.AddField("legacy", std::move(legacy)).ok());

  asc::ConfigurationSchema items(ValueType::kList);
  ASC_TEST_CHECK(context,
                 items.SetSizeBounds(std::size_t{1}, std::size_t{3}).ok());
  ASC_TEST_CHECK(context, root.AddField("items", std::move(items)).ok());

  asc::ConfigurationSchema nested(ValueType::kObject);
  nested.SetSensitive(true);
  asc::ConfigurationSchema escaped(ValueType::kString);
  escaped.SetRequired(true);
  ASC_TEST_CHECK(context, nested.AddField("a/b~c", std::move(escaped)).ok());
  ASC_TEST_CHECK(context, root.AddField("nested", std::move(nested)).ok());
  return root;
}

Value MakeValidInput() {
  return Value(Value::Object{
      {"count", Value(std::int64_t{7})},
      {"items", Value(Value::List{ValidUtf8("first")})},
      {"legacy", Value(false)},
      {"name", ValidUtf8("solver")},
      {"nested", Value(Value::Object{{"a/b~c", ValidUtf8("private")}})},
      {"secret", ValidUtf8("token-value")},
  });
}

void CheckSuccessfulValidation(asc_core_test::TestContext& context) {
  auto schema = MakeSchema(context);
  const Value input = MakeValidInput();
  const auto configuration = asc::ValidateConfiguration(
      schema, input,
      asc::ConfigurationOrigin::Programmatic("unit-test", "line:19"));
  ASC_TEST_CHECK(context, configuration.ok());

  const auto name = configuration->Find("/name");
  ASC_TEST_CHECK(context, name.ok());
  ASC_TEST_EQ(context, name->get().AsString()->get(), std::string("solver"));

  const auto threads = configuration->Find("/threads");
  ASC_TEST_CHECK(context, threads.ok());
  ASC_TEST_EQ(context, *threads->get().AsUnsignedInteger(), 2U);

  const auto escaped = configuration->Find("/nested/a~1b~0c");
  ASC_TEST_CHECK(context, escaped.ok());
  ASC_TEST_EQ(context, escaped->get().AsString()->get(),
              std::string("private"));
  ASC_TEST_EQ(context, configuration->Find("/items/0")->get().AsString()->get(),
              std::string("first"));

  const auto explicit_origin = configuration->Origin("/name");
  ASC_TEST_CHECK(context, explicit_origin.ok());
  ASC_TEST_EQ(context, explicit_origin->kind,
              asc::ConfigurationOriginKind::kProgrammatic);
  ASC_TEST_EQ(context, *explicit_origin->source_label,
              std::string("unit-test"));
  ASC_TEST_EQ(context, *explicit_origin->location, std::string("line:19"));

  const auto default_origin = configuration->Origin("/threads");
  ASC_TEST_CHECK(context, default_origin.ok());
  ASC_TEST_EQ(context, default_origin->kind,
              asc::ConfigurationOriginKind::kDefault);
  ASC_TEST_CHECK(context, !default_origin->source_label.has_value());

  ASC_TEST_EQ(context, *configuration->IsSensitive("/secret"), true);
  ASC_TEST_EQ(context, *configuration->IsSensitive("/nested/a~1b~0c"), true);
  ASC_TEST_EQ(context, *configuration->IsDeprecated("/legacy"), true);
  ASC_TEST_EQ(context, *configuration->IsDeprecated("/name"), false);
  ASC_TEST_CHECK(context, !configuration->Origin("/missing").ok());

  ASC_TEST_EQ(context,
              asc::RenderConfigurationValue(
                  configuration->Find("/secret")->get(), true),
              std::string("<redacted>"));
  ASC_TEST_CHECK(context, asc::RenderConfigurationValue(
                              configuration->Find("/secret")->get(), true)
                                  .find("token-value") == std::string::npos);
}

void CheckValidationFailures(asc_core_test::TestContext& context) {
  auto schema = MakeSchema(context);

  Value unknown = MakeValidInput();
  auto unknown_object = unknown.AsObject()->get();
  Value::Object with_unknown = unknown_object;
  with_unknown.emplace("unexpected", Value(true));
  const auto unknown_result =
      asc::ValidateConfiguration(schema, Value(std::move(with_unknown)));
  ASC_TEST_CHECK(context, !unknown_result.ok());
  ASC_TEST_EQ(context, unknown_result.status().code(),
              asc::ErrorCode::kConfiguration);

  Value::Object missing_name = unknown_object;
  missing_name.erase("name");
  const auto missing =
      asc::ValidateConfiguration(schema, Value(std::move(missing_name)));
  ASC_TEST_CHECK(context, !missing.ok());

  Value::Object wrong_type = unknown_object;
  wrong_type["count"] = Value(std::uint64_t{7});
  const auto no_numeric_conversion =
      asc::ValidateConfiguration(schema, Value(std::move(wrong_type)));
  ASC_TEST_CHECK(context, !no_numeric_conversion.ok());

  Value::Object outside_bound = unknown_object;
  outside_bound["count"] = Value(std::int64_t{9});
  const auto bound_failure =
      asc::ValidateConfiguration(schema, Value(std::move(outside_bound)));
  ASC_TEST_CHECK(context, !bound_failure.ok());

  // Failed validation must not mutate its input, even after a late nested
  // failure.
  Value::Object nested_failure = unknown_object;
  nested_failure["nested"] =
      Value(Value::Object{{"wrong", ValidUtf8("value")}});
  const Value transactional_input(std::move(nested_failure));
  const auto transaction =
      asc::ValidateConfiguration(schema, transactional_input);
  ASC_TEST_CHECK(context, !transaction.ok());
  ASC_TEST_CHECK(context,
                 transactional_input.AsObject()->get().contains("nested"));
  ASC_TEST_CHECK(context, transactional_input.AsObject()
                              ->get()
                              .find("nested")
                              ->second.AsObject()
                              ->get()
                              .contains("wrong"));

  ASC_TEST_CHECK(context, !schema.SetSignedBounds(0, 1).ok());
  ASC_TEST_CHECK(context, !schema.SetSizeBounds(9, 1).ok());

  asc::ConfigurationSchema bounded_signed(ValueType::kSignedInteger);
  ASC_TEST_CHECK(context, bounded_signed.SetSignedBounds(0, 10).ok());
  ASC_TEST_CHECK(context, !bounded_signed.SetSignedBounds(9, 1).ok());
  ASC_TEST_EQ(context, *bounded_signed.signed_minimum(), 0);
  ASC_TEST_EQ(context, *bounded_signed.signed_maximum(), 10);

  asc::ConfigurationSchema duplicate_root(ValueType::kObject);
  ASC_TEST_CHECK(
      context, duplicate_root
                   .AddField("same", asc::ConfigurationSchema(ValueType::kBool))
                   .ok());
  ASC_TEST_CHECK(
      context,
      !duplicate_root
           .AddField("same", asc::ConfigurationSchema(ValueType::kDouble))
           .ok());
  ASC_TEST_EQ(context, duplicate_root.fields().find("same")->second.type(),
              ValueType::kBool);

  asc::ConfigurationSchema invalid_default(ValueType::kObject);
  asc::ConfigurationSchema field(ValueType::kSignedInteger);
  field.SetDefault(ValidUtf8("not-an-integer"));
  ASC_TEST_CHECK(context,
                 invalid_default.AddField("field", std::move(field)).ok());
  const auto bad_default =
      asc::ValidateConfiguration(invalid_default, Value(Value::Object{}));
  ASC_TEST_CHECK(context, !bad_default.ok());
  ASC_TEST_CHECK(context, bad_default.status().message().find(
                              "schema default") != std::string::npos);

  asc::ConfigurationSchema bounded_double(ValueType::kDouble);
  ASC_TEST_CHECK(context, bounded_double.SetDoubleBounds(-1.0, 1.0).ok());
  const auto nan_result = asc::ValidateConfiguration(
      bounded_double, Value(std::numeric_limits<double>::quiet_NaN()));
  ASC_TEST_CHECK(context, !nan_result.ok());

  const auto valid = asc::ValidateConfiguration(schema, MakeValidInput());
  ASC_TEST_CHECK(context, valid.ok());
  ASC_TEST_CHECK(context, !valid->Find("missing-leading-slash").ok());
  ASC_TEST_CHECK(context, !valid->Find("/items/not-an-index").ok());
  ASC_TEST_CHECK(context, !valid->Find("/items/99").ok());
  ASC_TEST_CHECK(context, !valid->Find("/name/child").ok());
  ASC_TEST_CHECK(context, !valid->Find("/nested/bad~2escape").ok());
}

void CheckEffectiveObjectSizeBounds(asc_core_test::TestContext& context) {
  asc::ConfigurationSchema maximum_schema(ValueType::kObject);
  ASC_TEST_CHECK(context, maximum_schema.SetSizeBounds(std::nullopt, 1).ok());
  ASC_TEST_CHECK(
      context,
      maximum_schema
          .AddField("explicit", asc::ConfigurationSchema(ValueType::kBool))
          .ok());
  asc::ConfigurationSchema default_field(ValueType::kBool);
  default_field.SetDefault(Value(true));
  ASC_TEST_CHECK(
      context,
      maximum_schema.AddField("defaulted", std::move(default_field)).ok());

  const Value input(Value::Object{{"explicit", Value(false)}});
  const auto exceeds_after_default =
      asc::ValidateConfiguration(maximum_schema, input);
  ASC_TEST_CHECK(context, !exceeds_after_default.ok());
  ASC_TEST_EQ(context, exceeds_after_default.status().code(),
              asc::ErrorCode::kConfiguration);
  ASC_TEST_EQ(context, input.AsObject()->get().size(), std::size_t{1});
  ASC_TEST_CHECK(context, !input.AsObject()->get().contains("defaulted"));

  asc::ConfigurationSchema minimum_schema(ValueType::kObject);
  ASC_TEST_CHECK(context, minimum_schema.SetSizeBounds(2, std::nullopt).ok());
  ASC_TEST_CHECK(
      context,
      minimum_schema
          .AddField("explicit", asc::ConfigurationSchema(ValueType::kBool))
          .ok());
  asc::ConfigurationSchema minimum_default(ValueType::kBool);
  minimum_default.SetDefault(Value(true));
  ASC_TEST_CHECK(
      context,
      minimum_schema.AddField("defaulted", std::move(minimum_default)).ok());
  const auto reaches_minimum =
      asc::ValidateConfiguration(minimum_schema, input);
  ASC_TEST_CHECK(context, reaches_minimum.ok());
  ASC_TEST_EQ(context, reaches_minimum->value().AsObject()->get().size(),
              std::size_t{2});
}

void CheckLayeredOrigins(asc_core_test::TestContext& context) {
  auto schema = MakeSchema(context);
  const Value input = MakeValidInput();
  const asc::ConfigurationOrigins origins{
      {"/name", asc::ConfigurationOrigin::CommandLine("argv", "0")},
      {"/nested", asc::ConfigurationOrigin::CommandLine("argv", "2")},
  };
  const auto configuration = asc::ValidateConfigurationWithOrigins(
      schema, input, origins,
      asc::ConfigurationOrigin::Programmatic("fallback"));
  ASC_TEST_CHECK(context, configuration.ok());
  ASC_TEST_EQ(context, configuration->Origin("/name")->kind,
              asc::ConfigurationOriginKind::kCommandLine);
  ASC_TEST_EQ(context, *configuration->Origin("/name")->location,
              std::string("0"));
  ASC_TEST_EQ(context, configuration->Origin("/nested/a~1b~0c")->kind,
              asc::ConfigurationOriginKind::kCommandLine);
  ASC_TEST_EQ(context, *configuration->Origin("/nested/a~1b~0c")->location,
              std::string("2"));
  ASC_TEST_EQ(context, configuration->Origin("/count")->kind,
              asc::ConfigurationOriginKind::kProgrammatic);
  ASC_TEST_EQ(context, configuration->Origin("/threads")->kind,
              asc::ConfigurationOriginKind::kDefault);

  asc::ConfigurationOrigins invalid_origins = origins;
  invalid_origins.emplace("/missing",
                          asc::ConfigurationOrigin::CommandLine("argv", "4"));
  const auto invalid =
      asc::ValidateConfigurationWithOrigins(schema, input, invalid_origins);
  ASC_TEST_CHECK(context, !invalid.ok());
  ASC_TEST_EQ(context, invalid.status().code(), asc::ErrorCode::kConfiguration);
}

}  // namespace

int main() {
  asc_core_test::TestContext context;
  CheckValueAlternatives(context);
  CheckUtf8(context);
  CheckSuccessfulValidation(context);
  CheckValidationFailures(context);
  CheckEffectiveObjectSizeBounds(context);
  CheckLayeredOrigins(context);
  return context.Finish();
}
