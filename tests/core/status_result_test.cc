#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "test_support.h"

namespace {

struct ThrowingMoveConstruction {
  ThrowingMoveConstruction() = default;
  ~ThrowingMoveConstruction() = default;
  ThrowingMoveConstruction(const ThrowingMoveConstruction&) = delete;
  ThrowingMoveConstruction& operator=(const ThrowingMoveConstruction&) = delete;
  ThrowingMoveConstruction(ThrowingMoveConstruction&& other) noexcept(false) {
    static_cast<void>(other);
  }
  ThrowingMoveConstruction& operator=(
      ThrowingMoveConstruction&& other) noexcept {
    static_cast<void>(other);
    return *this;
  }
};

static_assert(!std::is_copy_constructible_v<asc::Result<std::unique_ptr<int>>>);
static_assert(!std::is_copy_assignable_v<asc::Result<std::unique_ptr<int>>>);
static_assert(std::is_move_constructible_v<asc::Result<std::unique_ptr<int>>>);
static_assert(std::is_move_assignable_v<asc::Result<std::unique_ptr<int>>>);
static_assert(noexcept(asc::Status(asc::ErrorCode::kIo)));
static_assert(!std::is_nothrow_move_constructible_v<ThrowingMoveConstruction>);
static_assert(std::is_nothrow_move_assignable_v<ThrowingMoveConstruction>);
static_assert(
    !std::is_nothrow_move_assignable_v<asc::Result<ThrowingMoveConstruction>>);

void CheckErrorCodes(asc_core_test::TestContext& context) {
  using asc::ErrorCode;
  constexpr ErrorCode kCodes[] = {
      ErrorCode::kOk,
      ErrorCode::kInvalidArgument,
      ErrorCode::kShape,
      ErrorCode::kIndex,
      ErrorCode::kOverflow,
      ErrorCode::kInvalidState,
      ErrorCode::kAllocation,
      ErrorCode::kMemoryAccess,
      ErrorCode::kMemoryTransfer,
      ErrorCode::kUnsupported,
      ErrorCode::kUnavailable,
      ErrorCode::kProvider,
      ErrorCode::kNumerical,
      ErrorCode::kConfiguration,
      ErrorCode::kIo,
      ErrorCode::kEndOfFile,
      ErrorCode::kEncoding,
      ErrorCode::kVersion,
      ErrorCode::kInternal,
  };
  for (std::uint32_t index = 0; index < std::size(kCodes); ++index) {
    ASC_TEST_EQ(context, static_cast<std::uint32_t>(kCodes[index]), index);
    ASC_TEST_CHECK(context, !asc::ErrorCodeName(kCodes[index]).empty());
  }
}

void CheckStatus(asc_core_test::TestContext& context) {
  const asc::Status ok;
  ASC_TEST_CHECK(context, ok.ok());
  ASC_TEST_EQ(context, ok.code(), asc::ErrorCode::kOk);
  ASC_TEST_CHECK(context, ok.message().empty());
  ASC_TEST_CHECK(context, ok.provider().empty());
  ASC_TEST_EQ(context, ok.native_code(), 0);
  ASC_TEST_CHECK(context, asc::Status::Ok().ok());

  const asc::Status provider_error(asc::ErrorCode::kProvider, "bad call",
                                   "test-provider", -127);
  ASC_TEST_CHECK(context, !provider_error.ok());
  ASC_TEST_EQ(context, provider_error.code(), asc::ErrorCode::kProvider);
  ASC_TEST_EQ(context, provider_error.message(), std::string("bad call"));
  ASC_TEST_EQ(context, provider_error.provider(), std::string("test-provider"));
  ASC_TEST_EQ(context, provider_error.native_code(), -127);
  ASC_TEST_CHECK(context, provider_error.ToString().find("test-provider") !=
                              std::string::npos);
  ASC_TEST_CHECK(context,
                 provider_error.ToString().find("-127") != std::string::npos);
}

void CheckResult(asc_core_test::TestContext& context) {
  asc::Result<int> value(37);
  ASC_TEST_CHECK(context, value.ok());
  ASC_TEST_CHECK(context, value.status().ok());
  ASC_TEST_EQ(context, value.value(), 37);
  ASC_TEST_EQ(context, *value, 37);
  *value = 41;
  ASC_TEST_EQ(context, value.value(), 41);

  const asc::Result<std::string> text(std::string("core"));
  ASC_TEST_CHECK(context, text.ok());
  ASC_TEST_EQ(context, text->size(), std::size_t{4});

  asc::Result<std::unique_ptr<int>> move_only(std::make_unique<int>(73));
  asc::Result<std::unique_ptr<int>> moved(std::move(move_only));
  ASC_TEST_CHECK(context, moved.ok());
  ASC_TEST_EQ(context, **moved, 73);

  const asc::Result<int> failure(
      asc::Status(asc::ErrorCode::kUnavailable, "backend unavailable"));
  ASC_TEST_CHECK(context, !failure.ok());
  ASC_TEST_EQ(context, failure.status().code(), asc::ErrorCode::kUnavailable);

  const asc::Result<asc::Status> status_value(asc::Status::Ok());
  ASC_TEST_CHECK(context, status_value.ok());
  ASC_TEST_CHECK(context, status_value->ok());

  const auto status_failure = asc::Result<asc::Status>::Failure(
      asc::Status(asc::ErrorCode::kInternal, "test failure"));
  ASC_TEST_CHECK(context, !status_failure.ok());
  ASC_TEST_EQ(context, status_failure.status().code(),
              asc::ErrorCode::kInternal);
}

}  // namespace

int main() {
  asc_core_test::TestContext context;
  CheckErrorCodes(context);
  CheckStatus(context);
  CheckResult(context);
  return context.Finish();
}
