#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "../allocation_observation.h"
#include "allocation_probe.h"
#include "asc/core/array_io.h"
#include "asc/core/contracts.h"
#include "asc/core/io.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/io.h"
#include "asc/dense/layout.h"
#include "asc/dense/view.h"
#include "test_support.h"

namespace {

using asc_dense_test::TestContext;

// Frozen hand-derived dense-i16 fixture: rank 1, shape (2), values -2, 513.
// Header [0,64), two little-endian scalar pairs [64,68), CRC [68,72).
// No production writer or reader creates this fixture or the expected counts.
constexpr std::array<std::uint8_t, 72> kWire{
    0x41, 0x53, 0x43, 0x41, 0x52, 0x52, 0x42, 0x0a, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x03, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xfe, 0xff, 0x01, 0x02, 0xd1, 0x9f, 0x13, 0xcc};
constexpr std::int64_t kNativeError = -1729;
constexpr std::byte kGuard{0xa5};

class Source final : public asc::ByteSource {
 public:
  Source(std::size_t boundary, bool fail) : boundary_(boundary), fail_(fail) {}

  asc::Result<std::size_t> ReadSome(std::span<std::byte> output) override {
    ++calls_;
    if (position_ == boundary_ && fail_) {
      return asc::Status(asc::ErrorCode::kIo, {}, {}, kNativeError);
    }
    const auto count = std::min({output.size(), kWire.size() - position_,
                                 boundary_ - position_, std::size_t{3}});
    const auto bytes = std::as_bytes(std::span(kWire));
    std::copy_n(bytes.subspan(position_).begin(), count, output.begin());
    position_ += count;
    return count;
  }

  [[nodiscard]] std::size_t position() const { return position_; }
  [[nodiscard]] std::size_t calls() const { return calls_; }

 private:
  std::size_t boundary_;
  bool fail_;
  std::size_t position_ = 0;
  std::size_t calls_ = 0;
};

class Sink final : public asc::ByteSink {
 public:
  Sink(std::size_t boundary, bool zero) : boundary_(boundary), zero_(zero) {
    storage_.fill(kGuard);
  }

  asc::Result<std::size_t> WriteSome(
      std::span<const std::byte> input) override {
    ++calls_;
    if (size_ == boundary_) {
      if (zero_) {
        return std::size_t{0};
      }
      return asc::Status(asc::ErrorCode::kIo, {}, {}, kNativeError);
    }
    const auto count =
        std::min({input.size(), boundary_ - size_, std::size_t{3}});
    std::copy_n(input.begin(), count, storage_.begin() + 1 + size_);
    size_ += count;
    return count;
  }

  [[nodiscard]] std::span<const std::byte> bytes() const {
    return std::span(storage_).subspan(1, size_);
  }
  [[nodiscard]] bool guards() const {
    return storage_.front() == kGuard &&
           std::all_of(storage_.begin() + 1 + size_, storage_.end(),
                       [](std::byte value) { return value == kGuard; });
  }
  [[nodiscard]] std::size_t calls() const { return calls_; }

 private:
  std::size_t boundary_;
  bool zero_;
  std::array<std::byte, kWire.size() + 2> storage_{};
  std::size_t size_ = 0;
  std::size_t calls_ = 0;
};

asc::ArrayIoReport DirtyReport() {
  return {
      777, 888, 999, true, asc::ArrayIoSection::kComplete, asc::ErrorCode::kIo};
}

void CheckReport(TestContext& test, const asc::ArrayIoReport& report,
                 std::size_t boundary, bool read, bool complete) {
  const auto values =
      boundary < 64 ? 0 : std::min((boundary - 64) / 2, std::size_t{2});
  auto section = asc::ArrayIoSection::kTrailer;
  if (complete) {
    section = asc::ArrayIoSection::kComplete;
  } else if (boundary < 64) {
    section = asc::ArrayIoSection::kHeader;
  } else if (boundary < 68) {
    section = asc::ArrayIoSection::kPayload;
  }
  ASC_DENSE_TEST_EQ(test, report.input_bytes, read ? boundary : 0);
  ASC_DENSE_TEST_EQ(test, report.output_bytes, read ? 0 : boundary);
  ASC_DENSE_TEST_EQ(test, report.values_processed, values);
  ASC_DENSE_TEST_EQ(test, report.section, section);
  ASC_DENSE_TEST_EQ(test, report.committed, read && complete);
  ASC_DENSE_TEST_EQ(test, report.cleanup_error, asc::ErrorCode::kOk);
}

auto PaddedView(std::array<std::int16_t, 7>& values) {
  auto mapping = asc::DenseLayout<1>::Create(std::array<asc::extent_t, 1>{2},
                                             asc::LayoutStride<1>{{3}});
  ASC_CHECK(mapping.ok());
  auto view = asc::DenseView<std::int16_t, 1>::Create(
      values.data() + 1, *mapping, asc::MemorySpace::kHost);
  ASC_CHECK(view.ok());
  return *view;
}

void ReadBoundary(TestContext& test, std::size_t boundary, bool require_eof,
                  bool fail) {
  const std::array<std::int16_t, 7> original{1000, 91,   2222, 3333,
                                             92,   4444, 5000};
  auto destination = original;
  const auto view = PaddedView(destination);
  std::array<std::int16_t, 4> staging{-32000, 11, 12, 32000};
  std::array<asc::extent_t, 3> metadata{12345, 0, 54321};
  std::array<std::byte, 1026> scratch{};
  scratch.fill(kGuard);
  auto report = DirtyReport();
  asc::ArrayIoLimits limits;
  Source source(boundary, fail);
  const bool complete = boundary == kWire.size() && (!require_eof || !fail);
  {
    asc_dense_test::AllocationProbe probe;
    auto reader = asc::DenseArrayReader::PrepareBinary(
        source, std::span(metadata).subspan(1, 1),
        std::span(scratch).subspan(1, 1024), limits, report, require_eof);
    const auto status =
        reader.ok() ? asc::ReadDenseArrayInto(*reader, view,
                                              std::span(staging).subspan(1, 2))
                    : reader.status();
    ASC_DENSE_TEST_EQ(test, status.ok(), complete);
    if (!complete) {
      ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kIo);
      ASC_DENSE_TEST_EQ(test, status.native_code(), kNativeError);
      ASC_DENSE_TEST_CHECK(test, status.message().empty());
      ASC_DENSE_TEST_CHECK(test, status.provider().empty());
    }
    CheckReport(test, report, boundary, true, complete);
    if (reader.ok()) {
      ASC_DENSE_TEST_CHECK(test, !reader->ready());
      const auto calls = source.calls();
      const auto retry = asc::ReadDenseArrayInto(
          *reader, view, std::span(staging).subspan(1, 2));
      // A consumed successful cursor rejects reuse with InvalidState. A
      // failed source retains its original callback error and native code;
      // either state prevents another callback or destination commit.
      ASC_DENSE_TEST_EQ(
          test, retry.code(),
          complete ? asc::ErrorCode::kInvalidState : asc::ErrorCode::kIo);
      ASC_DENSE_TEST_EQ(test, retry.native_code(), complete ? 0 : kNativeError);
      ASC_DENSE_TEST_CHECK(test, retry.message().empty());
      ASC_DENSE_TEST_CHECK(test, retry.provider().empty());
      ASC_DENSE_TEST_EQ(test, source.calls(), calls);
      CheckReport(test, report, boundary, true, complete);
    }
    ASC_DENSE_TEST_CHECK(
        test, asc_test::ProcessAllocationCountMatches(probe.count(), 0));
  }
  auto expected = original;
  if (complete) {
    expected[1] = -2;
    expected[4] = 513;
  }
  const auto actual_bytes = std::as_bytes(std::span(destination));
  const auto expected_bytes = std::as_bytes(std::span(expected));
  ASC_DENSE_TEST_CHECK(
      test, std::equal(actual_bytes.begin(), actual_bytes.end(),
                       expected_bytes.begin(), expected_bytes.end()));
  ASC_DENSE_TEST_EQ(test, source.position(), boundary);
  ASC_DENSE_TEST_EQ(test, staging.front(), -32000);
  ASC_DENSE_TEST_EQ(test, staging.back(), 32000);
  ASC_DENSE_TEST_EQ(test, metadata.front(), 12345);
  ASC_DENSE_TEST_EQ(test, metadata.back(), 54321);
  ASC_DENSE_TEST_EQ(test, scratch.front(), kGuard);
  ASC_DENSE_TEST_EQ(test, scratch.back(), kGuard);
  ASC_DENSE_TEST_CHECK(test, source.calls() <= kWire.size() + 1);
}

void WriteBoundary(TestContext& test, std::size_t boundary, bool zero) {
  std::array<std::int16_t, 7> values{1000, -2, 2222, 3333, 513, 4444, 5000};
  const auto original = values;
  const auto view = PaddedView(values);
  std::array<std::byte, 1026> scratch{};
  scratch.fill(kGuard);
  auto report = DirtyReport();
  asc::ArrayIoLimits limits;
  Sink sink(boundary, zero);
  {
    asc_dense_test::AllocationProbe probe;
    const auto status = asc::WriteDenseArrayBinary(
        view, sink, limits, std::span(scratch).subspan(1, 1024), report);
    ASC_DENSE_TEST_EQ(test, status.ok(), boundary == kWire.size());
    if (boundary != kWire.size()) {
      ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kIo);
      ASC_DENSE_TEST_EQ(test, status.native_code(), zero ? 0 : kNativeError);
      ASC_DENSE_TEST_CHECK(test, status.message().empty());
      ASC_DENSE_TEST_CHECK(test, status.provider().empty());
    }
    CheckReport(test, report, boundary, false, boundary == kWire.size());
    ASC_DENSE_TEST_CHECK(
        test, asc_test::ProcessAllocationCountMatches(probe.count(), 0));
  }
  const auto expected = std::as_bytes(std::span(kWire)).first(boundary);
  ASC_DENSE_TEST_CHECK(test,
                       std::equal(sink.bytes().begin(), sink.bytes().end(),
                                  expected.begin(), expected.end()));
  ASC_DENSE_TEST_CHECK(test, sink.guards());
  ASC_DENSE_TEST_EQ(test, scratch.front(), kGuard);
  ASC_DENSE_TEST_EQ(test, scratch.back(), kGuard);
  ASC_DENSE_TEST_EQ(test, values, original);
  ASC_DENSE_TEST_CHECK(test, sink.calls() <= kWire.size() + 1);
}

}  // namespace

int main() {
  TestContext test;
  for (std::size_t boundary = 0; boundary < kWire.size(); ++boundary) {
    ReadBoundary(test, boundary, false, true);
    WriteBoundary(test, boundary, false);
    WriteBoundary(test, boundary, true);
  }
  // A framed read must not touch the next byte. A whole-file read must perform
  // the EOF probe before commit, including propagating a failure at that probe.
  ReadBoundary(test, kWire.size(), false, true);
  ReadBoundary(test, kWire.size(), true, true);
  ReadBoundary(test, kWire.size(), true, false);
  WriteBoundary(test, kWire.size(), false);
  return test.Finish();
}
