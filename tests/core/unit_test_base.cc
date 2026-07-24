#include <asc/core/contracts.h>
#include <asc/core/memory_space.h>
#include <asc/core/status.h>

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <type_traits>
#include <utility>

namespace asc {
namespace {

static_assert(!std::is_convertible_v<Status, bool>);
static_assert(!std::is_convertible_v<Result<int>, bool>);

TEST(StatusTest, DefaultAndNamedSuccessHaveNoDiagnosticPayload) {
  const Status default_status;
  const Status named_status = Status::Ok();

  EXPECT_TRUE(default_status.ok());
  EXPECT_TRUE(static_cast<bool>(default_status));
  EXPECT_EQ(default_status.code(), StatusCode::kOk);
  EXPECT_TRUE(default_status.message().empty());
  EXPECT_TRUE(default_status.provider().empty());
  EXPECT_EQ(default_status.provider_code(), 0);
  EXPECT_TRUE(named_status.ok());
}

TEST(StatusTest, FailureCarriesStableCodeAndMessage) {
  const Status status(StatusCode::kInvalidArgument, "negative extent");

  EXPECT_FALSE(status.ok());
  EXPECT_FALSE(static_cast<bool>(status));
  EXPECT_EQ(status.code(), StatusCode::kInvalidArgument);
  EXPECT_EQ(status.message(), "negative extent");
  EXPECT_TRUE(status.provider().empty());
  EXPECT_EQ(status.provider_code(), 0);
}

TEST(StatusTest, ProviderFailurePreservesProviderDiagnostics) {
  const Status status = Status::FromProvider(
      StatusCode::kBackendError, "allocation failed", "test-provider", -17);

  EXPECT_EQ(status.code(), StatusCode::kBackendError);
  EXPECT_EQ(status.message(), "allocation failed");
  EXPECT_EQ(status.provider(), "test-provider");
  EXPECT_EQ(status.provider_code(), -17);
}

TEST(ResultTest, StoresAndMutatesAValue) {
  Result<int> result(41);
  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(result.status().ok());
  EXPECT_EQ(result.value(), 41);

  result.value() += 1;
  EXPECT_EQ(result.value(), 42);
}

TEST(ResultTest, SupportsConstAndRvalueValueAccess) {
  const Result<std::string> const_result(std::string("value"));
  EXPECT_EQ(const_result.value(), "value");

  Result<std::string> movable(std::string("moved"));
  std::string value = std::move(movable).value();
  EXPECT_EQ(value, "moved");
}

TEST(ResultTest, SupportsMoveOnlyValues) {
  Result<std::unique_ptr<int>> result(std::make_unique<int>(7));
  ASSERT_TRUE(result.ok());

  std::unique_ptr<int> value = std::move(result).value();
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, 7);
}

TEST(ResultTest, StoresFailureWithoutAValue) {
  Result<int> result(Status(StatusCode::kOutOfRange, "index 8"));

  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), StatusCode::kOutOfRange);
  EXPECT_EQ(result.status().message(), "index 8");
}

#ifdef ASC_USE_EXCEPTION
TEST(StatusTest, RejectsOkCodeInFailureConstructor) {
  EXPECT_THROW(
      static_cast<void>(Status(StatusCode::kOk, "not a failure")),
      ContractException);
}

TEST(ResultTest, RejectsOkStatusAsFailure) {
  EXPECT_THROW(static_cast<void>(Result<int>(Status::Ok())),
               ContractException);
}

TEST(ResultTest, RejectsValueAccessOnFailure) {
  Result<int> result(Status(StatusCode::kUnavailable, "not present"));
  EXPECT_THROW(static_cast<void>(result.value()), ContractException);
}
#endif

TEST(MemorySpaceTest, HostAccessibilityIsExplicit) {
  EXPECT_TRUE(IsHostAccessible(MemorySpace::kHost));
  EXPECT_TRUE(IsHostAccessible(MemorySpace::kPinnedHost));
  EXPECT_FALSE(IsHostAccessible(MemorySpace::kDevice));
  EXPECT_TRUE(IsHostAccessible(MemorySpace::kManaged));
}

TEST(MemorySpaceTest, DeviceAccessibilityIsExplicit) {
  EXPECT_FALSE(IsDeviceAccessible(MemorySpace::kHost));
  EXPECT_FALSE(IsDeviceAccessible(MemorySpace::kPinnedHost));
  EXPECT_TRUE(IsDeviceAccessible(MemorySpace::kDevice));
  EXPECT_TRUE(IsDeviceAccessible(MemorySpace::kManaged));
}

TEST(MemorySpaceTest, ManagedSpaceRemainsDistinctFromHost) {
  EXPECT_NE(MemorySpace::kManaged, MemorySpace::kHost);
  EXPECT_TRUE(IsHostAccessible(MemorySpace::kManaged));
  EXPECT_TRUE(IsDeviceAccessible(MemorySpace::kManaged));
}

#ifdef ASC_USE_EXCEPTION
TEST(ContractTest, RequireIsStatementSafeAndEvaluatesConditionOnce) {
  int evaluations = 0;
  if (true)
    ASC_REQUIRE(++evaluations == 1, "single evaluation");
  else
    ADD_FAILURE() << "ASC_REQUIRE changed control flow";

  EXPECT_EQ(evaluations, 1);
}

TEST(ContractTest, RequireReportsExpressionMessageAndKind) {
  try {
    ASC_REQUIRE(false, "required value " << 9);
    FAIL() << "ASC_REQUIRE did not fail";
  } catch (const ContractException& exception) {
    const std::string message = exception.what();
    EXPECT_NE(message.find("precondition"), std::string::npos);
    EXPECT_NE(message.find("false"), std::string::npos);
    EXPECT_NE(message.find("required value 9"), std::string::npos);
    EXPECT_NE(message.find("unit_test_base.cc"), std::string::npos);
  }
}

TEST(ContractTest, EnsureUsesPostconditionKind) {
  try {
    ASC_ENSURE(false, "postcondition details");
    FAIL() << "ASC_ENSURE did not fail";
  } catch (const ContractException& exception) {
    const std::string message = exception.what();
    EXPECT_NE(message.find("postcondition"), std::string::npos);
    EXPECT_NE(message.find("postcondition details"), std::string::npos);
  }
}
#endif

}  // namespace
}  // namespace asc
