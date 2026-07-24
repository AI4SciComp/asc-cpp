#include <asc/core/event.h>

#include <gtest/gtest.h>

#include <type_traits>
#include <utility>

namespace asc {
namespace {

static_assert(std::is_nothrow_copy_constructible_v<Event>);
static_assert(std::is_nothrow_copy_assignable_v<Event>);
static_assert(std::is_nothrow_move_constructible_v<Event>);
static_assert(std::is_nothrow_move_assignable_v<Event>);

TEST(EventTest, DefaultEventIsCompletedSuccessfulSerialWork) {
  const Event event;

  EXPECT_TRUE(event.IsReady());
  EXPECT_EQ(event.GetBackend(), BackendKind::kSerial);
  const Status status = event.Wait();
  EXPECT_TRUE(status.ok());
}

TEST(EventTest, WaitIsIdempotent) {
  const Event event;

  const Status first = event.Wait();
  const Status second = event.Wait();
  EXPECT_TRUE(first.ok());
  EXPECT_TRUE(second.ok());
  EXPECT_TRUE(event.IsReady());
}

TEST(EventTest, CopiesShareACompletedStateContract) {
  const Event event;
  const Event copy = event;
  Event assigned;
  assigned = event;

  EXPECT_TRUE(copy.IsReady());
  EXPECT_TRUE(copy.Wait().ok());
  EXPECT_EQ(copy.GetBackend(), event.GetBackend());
  EXPECT_TRUE(assigned.IsReady());
  EXPECT_TRUE(assigned.Wait().ok());
}

TEST(EventTest, MovedHandlesRemainSafelyDestructible) {
  Event source;
  Event destination(std::move(source));

  EXPECT_TRUE(destination.IsReady());
  EXPECT_TRUE(destination.Wait().ok());
  EXPECT_EQ(destination.GetBackend(), BackendKind::kSerial);
}

}  // namespace
}  // namespace asc
