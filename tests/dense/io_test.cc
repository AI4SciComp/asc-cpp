#include "asc/dense/io.h"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>

#include "../allocation_observation.h"
#include "allocation_probe.h"
#include "asc/core/array_format.h"
#include "asc/core/array_io.h"
#include "asc/core/contracts.h"
#include "asc/core/extents.h"
#include "asc/core/io.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/array.h"
#include "asc/dense/layout.h"
#include "asc/dense/view.h"
#include "counting_memory_resource.h"
#include "test_support.h"

namespace {

using asc_dense_test::TestContext;
using Shape1 = asc::Extents<asc::kDynamicExtent>;
using Shape2 = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;
constexpr std::string_view kText =
    "ASCARRAY 1\nkind dense\nscalar f64\nrank 2\nshape 2 3\n"
    "order dim0\ncount 6\ndata\n1\n4\n2\n5\n3\n6\nend\n";
constexpr std::array<std::uint8_t, 72> kBinary{
    0x41, 0x53, 0x43, 0x41, 0x52, 0x52, 0x42, 0x0a, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x03, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xfe, 0xff, 0x01, 0x02, 0xd1, 0x9f, 0x13, 0xcc};

class Source final : public asc::ByteSource {
 public:
  explicit Source(std::span<const std::byte> bytes) : bytes_(bytes) {}
  explicit Source(std::string_view text)
      : bytes_(std::as_bytes(std::span(text.data(), text.size()))) {}
  asc::Result<std::size_t> ReadSome(std::span<std::byte> output) override {
    if (position_ == fail_after) {
      if (!error.ok()) {
        return std::move(error);
      }
      return asc::Status(asc::ErrorCode::kIo);
    }
    const auto count = std::min({output.size(), bytes_.size() - position_,
                                 max_chunk, fail_after - position_});
    std::copy_n(bytes_.data() + position_, count, output.data());
    position_ += count;
    return count;
  }
  [[nodiscard]] std::size_t position() const { return position_; }
  std::size_t max_chunk = std::numeric_limits<std::size_t>::max();
  std::size_t fail_after = std::numeric_limits<std::size_t>::max();
  asc::Status error;

 private:
  std::span<const std::byte> bytes_;
  std::size_t position_ = 0;
};

class Sink final : public asc::ByteSink {
 public:
  asc::Result<std::size_t> WriteSome(
      std::span<const std::byte> input) override {
    if (zero_progress) {
      return std::size_t{0};
    }
    if (size_ == fail_after) {
      if (!error.ok()) {
        return std::move(error);
      }
      return asc::Status(asc::ErrorCode::kIo);
    }
    const auto count = std::min(
        {input.size(), data_.size() - size_, max_chunk, fail_after - size_});
    std::copy_n(input.data(), count, data_.data() + size_);
    size_ += count;
    return count;
  }
  [[nodiscard]] std::span<const std::byte> bytes() const {
    return std::span(data_).first(size_);
  }
  [[nodiscard]] std::string_view text() const {
    return {reinterpret_cast<const char*>(data_.data()), size_};
  }
  std::size_t max_chunk = std::numeric_limits<std::size_t>::max();
  std::size_t fail_after = std::numeric_limits<std::size_t>::max();
  bool zero_progress = false;
  asc::Status error;

 private:
  std::array<std::byte, 8192> data_{};
  std::size_t size_ = 0;
};

template <typename T, std::size_t Rank, typename Layout = asc::LayoutLeft>
auto View(T* data, const std::array<asc::extent_t, Rank>& shape,
          Layout layout = {}) {
  auto mapping = asc::DenseLayout<Rank>::Create(shape, layout);
  ASC_CHECK(mapping.ok());
  return asc::DenseView<T, Rank>::Create(data, *mapping,
                                         asc::MemorySpace::kHost);
}

void TestIndependentTextAndLayouts(TestContext& test) {
  std::array<std::byte, 1024> scratch{};
  std::array<asc::extent_t, 2> metadata{};
  asc::ArrayIoLimits limits;
  asc::ArrayIoReport report;
  asc_dense_test::CountingMemoryResource resource;
  Source source(kText);
  auto reader = asc::DenseArrayReader::PrepareText(source, metadata, scratch,
                                                   limits, report, true);
  ASC_DENSE_TEST_CHECK(test, reader.ok());
  ASC_DENSE_TEST_EQ(test, resource.allocation_requests(), std::size_t{0});
  ASC_DENSE_TEST_EQ(test, reader->scalar(), asc::ArrayScalarCode::kF64);
  ASC_DENSE_TEST_EQ(test, reader->count(), std::uint64_t{6});
  const auto header_end = source.position();
  auto owner = asc::ReadDenseArray<double, Shape2>(*reader, resource,
                                                   asc::LayoutRight{});
  ASC_DENSE_TEST_CHECK(test, owner.ok());
  ASC_DENSE_TEST_CHECK(test, source.position() > header_end);
  ASC_DENSE_TEST_EQ(test, report.input_bytes, kText.size());
  ASC_DENSE_TEST_CHECK(test, report.committed && !reader->ready());
  ASC_DENSE_TEST_EQ(test, resource.allocation_requests(), std::size_t{1});
  auto view = owner->view();
  const std::array<double, 6> rows{1, 2, 3, 4, 5, 6};
  ASC_DENSE_TEST_CHECK(test,
                       std::equal(rows.begin(), rows.end(), view->data()));
  Sink sink;
  ASC_DENSE_TEST_CHECK(
      test,
      asc::WriteDenseArrayText(*owner, sink, limits, scratch, report).ok());
  ASC_DENSE_TEST_EQ(test, sink.text(), kText);
  ASC_DENSE_TEST_EQ(test, report.output_bytes, kText.size());
  ASC_DENSE_TEST_EQ(test, report.values_processed, std::size_t{6});
  ASC_DENSE_TEST_CHECK(test, !report.committed);

  Source left_source(kText);
  auto left = asc::ReadDenseArrayText<double, Shape2>(
      left_source, resource, asc::LayoutLeft{}, metadata, scratch, limits,
      report);
  ASC_DENSE_TEST_CHECK(test, left.ok());
  const std::array<double, 6> columns{1, 4, 2, 5, 3, 6};
  auto left_view = left->view();
  ASC_DENSE_TEST_CHECK(
      test, std::equal(columns.begin(), columns.end(), left_view->data()));
}

void TestIndependentBinary(TestContext& test) {
  std::array<std::int16_t, 2> values{-2, 513};
  auto view = View(values.data(), std::array<asc::extent_t, 1>{2});
  std::array<std::byte, 1024> scratch{};
  std::array<asc::extent_t, 1> metadata{};
  asc::ArrayIoLimits limits;
  asc::ArrayIoReport report;
  Sink sink;
  ASC_DENSE_TEST_CHECK(
      test,
      asc::WriteDenseArrayBinary(*view, sink, limits, scratch, report).ok());
  const auto expected = std::as_bytes(std::span(kBinary));
  ASC_DENSE_TEST_CHECK(
      test, std::equal(expected.begin(), expected.end(), sink.bytes().begin(),
                       sink.bytes().end()));
  Source source(expected);
  asc_dense_test::CountingMemoryResource resource;
  auto owner = asc::ReadDenseArrayBinary<std::int16_t, Shape1>(
      source, resource, asc::LayoutRight{}, metadata, scratch, limits, report);
  ASC_DENSE_TEST_CHECK(test, owner.ok());
  auto decoded = owner->view();
  ASC_DENSE_TEST_EQ(test, decoded->data()[0], -2);
  ASC_DENSE_TEST_EQ(test, decoded->data()[1], 513);
  ASC_DENSE_TEST_CHECK(test, report.committed);
}

template <typename T>
bool SameScalarBits(T left, T right) {
  if constexpr (std::is_same_v<T, std::complex<float>> ||
                std::is_same_v<T, std::complex<double>>) {
    return SameScalarBits(left.real(), right.real()) &&
           SameScalarBits(left.imag(), right.imag());
  } else {
    return std::bit_cast<std::array<std::byte, sizeof(T)>>(left) ==
           std::bit_cast<std::array<std::byte, sizeof(T)>>(right);
  }
}

template <typename T>
void CheckScalarRoundTrips(TestContext& test, const std::array<T, 3>& values) {
  auto view = View(values.data(), std::array<asc::extent_t, 1>{3});
  std::array<std::byte, 1024> scratch{};
  std::array<asc::extent_t, 1> metadata{};
  asc::ArrayIoLimits limits;
  asc::ArrayIoReport report;
  asc_dense_test::CountingMemoryResource resource;
  for (bool binary : {false, true}) {
    Sink sink;
    auto status =
        binary
            ? asc::WriteDenseArrayBinary(*view, sink, limits, scratch, report)
            : asc::WriteDenseArrayText(*view, sink, limits, scratch, report);
    ASC_DENSE_TEST_CHECK(test, status.ok());
    Source source(sink.bytes());
    source.max_chunk = 1;
    auto reader = binary ? asc::DenseArrayReader::PrepareBinary(
                               source, metadata, scratch, limits, report)
                         : asc::DenseArrayReader::PrepareText(
                               source, metadata, scratch, limits, report);
    ASC_DENSE_TEST_CHECK(test, reader.ok());
    auto owner =
        asc::ReadDenseArray<T, Shape1>(*reader, resource, asc::LayoutLeft{});
    ASC_DENSE_TEST_CHECK(test, owner.ok());
    if (!owner.ok()) {
      continue;
    }
    auto decoded = owner->view();
    for (std::size_t i = 0; i < values.size(); ++i) {
      ASC_DENSE_TEST_CHECK(test, SameScalarBits(values[i], decoded->data()[i]));
    }
  }
  ASC_DENSE_TEST_EQ(test, resource.live_allocations(), std::size_t{0});
}

template <typename T>
void CheckInteger(TestContext& test) {
  CheckScalarRoundTrips(test, std::array<T, 3>{0, std::numeric_limits<T>::min(),
                                               std::numeric_limits<T>::max()});
}

void TestAllScalarCodes(TestContext& test) {
  CheckInteger<char>(test);
  CheckInteger<wchar_t>(test);
  CheckInteger<char8_t>(test);
  CheckInteger<char16_t>(test);
  CheckInteger<char32_t>(test);
  CheckInteger<std::int8_t>(test);
  CheckInteger<std::uint8_t>(test);
  CheckInteger<std::int16_t>(test);
  CheckInteger<std::uint16_t>(test);
  CheckInteger<std::int32_t>(test);
  CheckInteger<std::uint32_t>(test);
  CheckInteger<std::int64_t>(test);
  CheckInteger<std::uint64_t>(test);
  CheckScalarRoundTrips(
      test,
      std::array<float, 3>{-0.0F, std::numeric_limits<float>::denorm_min(),
                           std::numeric_limits<float>::max()});
  CheckScalarRoundTrips(
      test,
      std::array<double, 3>{-0.0, std::numeric_limits<double>::denorm_min(),
                            std::numeric_limits<double>::max()});
  CheckScalarRoundTrips(
      test, std::array<std::complex<float>, 3>{
                std::complex<float>{-0.0F, 1.25F}, std::complex<float>{2, -3},
                std::complex<float>{4, 5}});
  CheckScalarRoundTrips(
      test, std::array<std::complex<double>, 3>{
                std::complex<double>{-0.0, 1.25}, std::complex<double>{2, -3},
                std::complex<double>{4, 5}});
}

void TestEveryTextTruncationAndRollback(TestContext& test) {
  const std::array<double, 10> original{91, 92,  999, 999, 93,
                                        94, 999, 999, 95,  96};
  std::array<std::byte, 1024> scratch{};
  std::array<asc::extent_t, 2> metadata{};
  std::array<double, 6> staging{};
  asc::ArrayIoLimits limits;
  asc::ArrayIoReport report;
  for (std::size_t boundary = 0; boundary < kText.size(); ++boundary) {
    auto destination = original;
    auto view = View(destination.data(), std::array<asc::extent_t, 2>{2, 3},
                     asc::LayoutStride<2>{{1, 4}});
    Source source(kText.substr(0, boundary));
    auto reader = asc::DenseArrayReader::PrepareText(source, metadata, scratch,
                                                     limits, report);
    bool failed = !reader.ok();
    if (reader.ok()) {
      failed =
          !asc::ReadDenseArrayInto(*reader, *view, std::span(staging)).ok();
    }
    ASC_DENSE_TEST_CHECK(test, failed);
    ASC_DENSE_TEST_EQ(test, destination, original);
    ASC_DENSE_TEST_EQ(test, report.input_bytes, source.position());
    ASC_DENSE_TEST_CHECK(test, !report.committed);
  }
  auto destination = original;
  auto view = View(destination.data(), std::array<asc::extent_t, 2>{2, 3},
                   asc::LayoutStride<2>{{1, 4}});
  Source source(kText);
  auto reader = asc::DenseArrayReader::PrepareText(source, metadata, scratch,
                                                   limits, report);
  asc_dense_test::AllocationProbe probe;
  ASC_DENSE_TEST_CHECK(
      test, asc::ReadDenseArrayInto(*reader, *view, std::span(staging)).ok());
  ASC_DENSE_TEST_CHECK(
      test, asc_test::ProcessAllocationCountMatches(probe.count(), 0));
  const std::array<double, 10> expected{1, 4, 999, 999, 2, 5, 999, 999, 3, 6};
  ASC_DENSE_TEST_EQ(test, destination, expected);
}

void TestBinaryCorruptionAndRollback(TestContext& test) {
  std::array<std::byte, 1024> scratch{};
  std::array<asc::extent_t, 1> metadata{};
  std::array<std::int16_t, 2> staging{};
  asc::ArrayIoLimits limits;
  asc::ArrayIoReport report;
  const auto original_bytes = std::as_bytes(std::span(kBinary));
  for (std::size_t position = 0; position < kBinary.size(); ++position) {
    for (bool truncate : {false, true}) {
      auto bytes = kBinary;
      bytes[position] ^= 1;
      Source source(truncate ? original_bytes.first(position)
                             : std::as_bytes(std::span(bytes)));
      std::array<std::int16_t, 2> values{8, 9};
      auto view = View(values.data(), std::array<asc::extent_t, 1>{2});
      auto reader = asc::DenseArrayReader::PrepareBinary(
          source, metadata, scratch, limits, report);
      bool failed = !reader.ok();
      if (reader.ok()) {
        failed =
            !asc::ReadDenseArrayInto(*reader, *view, std::span(staging)).ok();
      }
      ASC_DENSE_TEST_CHECK(test, failed);
      ASC_DENSE_TEST_EQ(test, values, (std::array<std::int16_t, 2>{8, 9}));
      ASC_DENSE_TEST_CHECK(test, !report.committed);
      ASC_DENSE_TEST_EQ(test, report.input_bytes, source.position());
    }
  }
}

void TestSourceAndSinkFailures(TestContext& test) {
  std::array<std::byte, 1024> scratch{};
  std::array<asc::extent_t, 2> metadata{};
  asc::ArrayIoLimits limits;
  asc::ArrayIoReport report;
  std::array<double, 6> values{1, 4, 2, 5, 3, 6};
  auto view = View(values.data(), std::array<asc::extent_t, 2>{2, 3});
  for (std::size_t boundary = 0; boundary < kText.size(); ++boundary) {
    Source source(kText);
    source.fail_after = boundary;
    asc_dense_test::CountingMemoryResource resource;
    auto owner = asc::ReadDenseArrayText<double, Shape2>(
        source, resource, asc::LayoutLeft{}, metadata, scratch, limits, report);
    ASC_DENSE_TEST_CHECK(test, !owner.ok());
    ASC_DENSE_TEST_EQ(test, resource.live_allocations(), std::size_t{0});
    ASC_DENSE_TEST_EQ(test, report.input_bytes, boundary);
    Sink sink;
    sink.fail_after = boundary;
    sink.max_chunk = 2;
    ASC_DENSE_TEST_CHECK(
        test,
        !asc::WriteDenseArrayText(*view, sink, limits, scratch, report).ok());
    ASC_DENSE_TEST_EQ(test, report.output_bytes, boundary);
    ASC_DENSE_TEST_EQ(test, sink.text(), kText.substr(0, boundary));
  }
  Sink zero;
  zero.zero_progress = true;
  ASC_DENSE_TEST_CHECK(
      test,
      !asc::WriteDenseArrayText(*view, zero, limits, scratch, report).ok());
  ASC_DENSE_TEST_EQ(test, report.output_bytes, std::size_t{0});
}

void TestAllocationAndTypeFailures(TestContext& test) {
  std::array<std::byte, 1024> scratch{};
  std::array<asc::extent_t, 2> metadata{};
  asc::ArrayIoLimits limits;
  asc::ArrayIoReport report;
  asc_dense_test::CountingMemoryResource resource;
  Source mismatched(kText);
  auto wrong = asc::ReadDenseArrayText<float, Shape2>(
      mismatched, resource, asc::LayoutLeft{}, metadata, scratch, limits,
      report);
  ASC_DENSE_TEST_CHECK(test, !wrong.ok());
  ASC_DENSE_TEST_EQ(test, resource.allocation_requests(), std::size_t{0});
  Source static_wrong(kText);
  auto wrong_shape = asc::ReadDenseArrayText<double, asc::Extents<3, 2>>(
      static_wrong, resource, asc::LayoutLeft{}, metadata, scratch, limits,
      report);
  ASC_DENSE_TEST_CHECK(test, !wrong_shape.ok());
  ASC_DENSE_TEST_EQ(test, resource.allocation_requests(), std::size_t{0});
  resource.FailRequest(0);
  Source failed_allocation(kText);
  auto failed = asc::ReadDenseArrayText<double, Shape2>(
      failed_allocation, resource, asc::LayoutLeft{}, metadata, scratch, limits,
      report);
  ASC_DENSE_TEST_CHECK(test, !failed.ok());
  ASC_DENSE_TEST_EQ(test, resource.live_allocations(), std::size_t{0});
  resource.DisableFailure();
  limits.max_allocation_bytes = 47;
  Source limited(kText);
  auto denied = asc::ReadDenseArrayText<double, Shape2>(
      limited, resource, asc::LayoutLeft{}, metadata, scratch, limits, report);
  ASC_DENSE_TEST_CHECK(test, !denied.ok());
  ASC_DENSE_TEST_EQ(test, resource.allocation_requests(), std::size_t{1});
}

void TestAdjacentFramesAndTrailingTransaction(TestContext& test) {
  Sink adjacent;
  const auto text_bytes = std::as_bytes(std::span(kText.data(), kText.size()));
  ASC_DENSE_TEST_CHECK(test, adjacent.WriteSome(text_bytes).ok());
  ASC_DENSE_TEST_CHECK(test, adjacent.WriteSome(text_bytes).ok());
  Source source(adjacent.bytes());
  std::array<std::byte, 1024> scratch{};
  std::array<asc::extent_t, 2> metadata{};
  asc::ArrayIoLimits limits;
  asc::ArrayIoReport report;
  asc_dense_test::CountingMemoryResource resource;
  for (std::size_t frame = 1; frame <= 2; ++frame) {
    auto owner = asc::ReadDenseArrayText<double, Shape2>(
        source, resource, asc::LayoutLeft{}, metadata, scratch, limits, report);
    ASC_DENSE_TEST_CHECK(test, owner.ok());
    ASC_DENSE_TEST_EQ(test, source.position(), frame * kText.size());
  }
  Source extra(adjacent.bytes());
  auto reader = asc::DenseArrayReader::PrepareText(extra, metadata, scratch,
                                                   limits, report, true);
  std::array<double, 6> values{8, 8, 8, 8, 8, 8};
  const auto before = values;
  std::array<double, 6> staging{};
  auto view = View(values.data(), std::array<asc::extent_t, 2>{2, 3});
  ASC_DENSE_TEST_CHECK(
      test, !asc::ReadDenseArrayInto(*reader, *view, std::span(staging)).ok());
  ASC_DENSE_TEST_EQ(test, values, before);
  ASC_DENSE_TEST_CHECK(test, !reader->ready() && !report.committed);
  const auto consumed = extra.position();
  ASC_DENSE_TEST_CHECK(
      test, !asc::ReadDenseArrayInto(*reader, *view, std::span(staging)).ok());
  ASC_DENSE_TEST_EQ(test, extra.position(), consumed);
}

void TestScalarZeroAndEmptyRanks(TestContext& test) {
  std::array<std::byte, 1024> scratch{};
  std::array<asc::extent_t, 3> metadata{};
  asc::ArrayIoLimits limits;
  asc::ArrayIoReport report;
  asc_dense_test::CountingMemoryResource resource;
  float scalar = -0.0F;
  auto view = View(&scalar, std::array<asc::extent_t, 0>{});
  for (bool binary : {false, true}) {
    Sink sink;
    auto status =
        binary
            ? asc::WriteDenseArrayBinary(*view, sink, limits, scratch, report)
            : asc::WriteDenseArrayText(*view, sink, limits, scratch, report);
    ASC_DENSE_TEST_CHECK(test, status.ok());
    Source source(sink.bytes());
    auto reader = binary ? asc::DenseArrayReader::PrepareBinary(
                               source, metadata, scratch, limits, report)
                         : asc::DenseArrayReader::PrepareText(
                               source, metadata, scratch, limits, report);
    auto owner = asc::ReadDenseArray<float, asc::Extents<>>(*reader, resource,
                                                            asc::LayoutRight{});
    ASC_DENSE_TEST_CHECK(test, owner.ok());
    auto result = owner->view();
    ASC_DENSE_TEST_CHECK(test, SameScalarBits(result->data()[0], scalar));
  }
  auto empty = View(static_cast<double*>(nullptr),
                    std::array<asc::extent_t, 3>{0, 2, 3});
  Sink sink;
  ASC_DENSE_TEST_CHECK(
      test,
      asc::WriteDenseArrayBinary(*empty, sink, limits, scratch, report).ok());
  Source source(sink.bytes());
  auto owner =
      asc::ReadDenseArrayBinary<double,
                                asc::Extents<0, asc::kDynamicExtent, 3>>(
          source, resource, asc::LayoutLeft{}, metadata, scratch, limits,
          report);
  ASC_DENSE_TEST_CHECK(test, owner.ok());
  ASC_DENSE_TEST_EQ(test, owner->extents().logical_size(), asc::extent_t{0});
  ASC_DENSE_TEST_CHECK(test, report.committed);
}

void TestIndependentLimitCaps(TestContext& test) {
  std::array<asc::ArrayIoLimits, 11> limits{};
  limits[0].max_input_bytes = 8;
  limits[1].max_header_bytes = 10;
  limits[2].max_token_bytes = 0;
  limits[3].max_rank = 1;
  limits[4].max_extent = 2;
  limits[5].max_logical_elements = 5;
  limits[6].max_decoded_bytes = 47;
  limits[7].max_staging_bytes = 47;
  limits[8].max_allocations = 0;
  limits[9].max_allocation_bytes = 47;
  limits[10].max_scratch_bytes = 1024;
  for (const auto& limit : limits) {
    std::array<std::byte, 1024> scratch{};
    std::array<asc::extent_t, 2> metadata{};
    asc::ArrayIoReport report;
    asc_dense_test::CountingMemoryResource resource;
    Source source(kText);
    auto owner = asc::ReadDenseArrayText<double, Shape2>(
        source, resource, asc::LayoutLeft{}, metadata, scratch, limit, report);
    ASC_DENSE_TEST_CHECK(test, !owner.ok());
    ASC_DENSE_TEST_EQ(test, resource.allocation_requests(), std::size_t{0});
    ASC_DENSE_TEST_CHECK(test, report.input_bytes <= limit.max_input_bytes);
    ASC_DENSE_TEST_EQ(test, report.input_bytes, source.position());
  }
  // A numeric token cap of one still permits the bounded f64 scalar keyword.
  std::array<std::byte, 1024> scratch{};
  std::array<asc::extent_t, 2> metadata{};
  asc::ArrayIoLimits limit;
  limit.max_token_bytes = 1;
  asc::ArrayIoReport report;
  asc_dense_test::CountingMemoryResource resource;
  Source source(kText);
  auto owner = asc::ReadDenseArrayText<double, Shape2>(
      source, resource, asc::LayoutLeft{}, metadata, scratch, limit, report);
  ASC_DENSE_TEST_CHECK(test, owner.ok());
}

void TestMalformedTokensAndCrlf(TestContext& test) {
  constexpr std::string_view kPrefix =
      "ASCARRAY 1\nkind dense\nscalar f32\nrank 1\nshape 1\norder dim0\ncount "
      "1\ndata\n";
  for (std::string_view token : {"+inf", "infinity", "-nan", "0x1p0", "1e39",
                                 "1e-46", "1 ", "(1,2)", "1,2", "", "1e+"}) {
    const std::string text =
        std::string(kPrefix) + std::string(token) + "\nend\n";
    Source source(text);
    std::array<std::byte, 1024> scratch{};
    std::array<asc::extent_t, 1> metadata{};
    asc::ArrayIoReport report;
    asc_dense_test::CountingMemoryResource resource;
    auto owner = asc::ReadDenseArrayText<float, Shape1>(
        source, resource, asc::LayoutLeft{}, metadata, scratch, {}, report);
    ASC_DENSE_TEST_CHECK(test, !owner.ok());
    ASC_DENSE_TEST_EQ(test, resource.live_allocations(), std::size_t{0});
    ASC_DENSE_TEST_CHECK(test, !report.committed);
  }
  std::string crlf;
  for (char character : kText) {
    if (character == '\n') {
      crlf += '\r';
    }
    crlf += character;
  }
  crlf += " \t\r\n";
  Source source(crlf);
  std::array<std::byte, 1024> scratch{};
  std::array<asc::extent_t, 2> metadata{};
  asc::ArrayIoReport report;
  auto reader = asc::DenseArrayReader::PrepareText(source, metadata, scratch,
                                                   {}, report, true);
  asc_dense_test::CountingMemoryResource resource;
  auto owner =
      asc::ReadDenseArray<double, Shape2>(*reader, resource, asc::LayoutLeft{});
  ASC_DENSE_TEST_CHECK(test, owner.ok());
  ASC_DENSE_TEST_EQ(test, report.input_bytes, crlf.size());
}

void TestStagingAliasesAndPlacement(TestContext& test) {
  for (int scenario = 0; scenario < 4; ++scenario) {
    // Byte access to live double storage makes the alias case a valid typed
    // span; no misaligned pointer or nonexistent double lifetime is forged.
    std::array<double, 128> scratch_storage{};
    const auto scratch = std::as_writable_bytes(std::span(scratch_storage));
    std::array<asc::extent_t, 2> metadata{};
    std::array<double, 6> target{9, 9, 9, 9, 9, 9};
    const auto original = target;
    std::array<double, 6> staging{};
    Source source(kText);
    asc::ArrayIoReport report;
    auto reader = asc::DenseArrayReader::PrepareText(source, metadata, scratch,
                                                     {}, report);
    auto mapping =
        asc::DenseLayout<2>::Create(std::array<asc::extent_t, 2>{2, 3});
    auto view = asc::DenseView<double, 2>::Create(
        target.data(), *mapping,
        scenario == 2 ? asc::MemorySpace::kDevice : asc::MemorySpace::kHost);
    std::span<double> stage(staging);
    if (scenario == 0) {
      stage = target;
    }
    if (scenario == 1) {
      stage = stage.first(5);
    }
    if (scenario == 3) {
      stage = std::span<double>(scratch_storage).first(6);
    }
    const auto before = source.position();
    ASC_DENSE_TEST_CHECK(test,
                         !asc::ReadDenseArrayInto(*reader, *view, stage).ok());
    ASC_DENSE_TEST_EQ(test, source.position(), before);
    ASC_DENSE_TEST_EQ(test, target, original);
    ASC_DENSE_TEST_CHECK(test, !report.committed);
  }
}

void TestBinaryQuietNanComponents(TestContext& test) {
  const double negative_nan =
      std::bit_cast<double>(std::uint64_t{0xfff8000000000042});
  const double positive_nan =
      std::bit_cast<double>(std::uint64_t{0x7ff8000000000123});
  const std::array<std::complex<double>, 2> values{
      std::complex<double>{negative_nan, -0.0},
      std::complex<double>{0.0, positive_nan}};
  auto view = View(values.data(), std::array<asc::extent_t, 1>{2});
  std::array<std::byte, 1024> scratch{};
  std::array<asc::extent_t, 1> metadata{};
  asc::ArrayIoReport report;
  Sink sink;
  ASC_DENSE_TEST_CHECK(
      test, asc::WriteDenseArrayBinary(*view, sink, {}, scratch, report).ok());
  Source source(sink.bytes());
  asc_dense_test::CountingMemoryResource resource;
  auto owner = asc::ReadDenseArrayBinary<std::complex<double>, Shape1>(
      source, resource, asc::LayoutLeft{}, metadata, scratch, {}, report);
  ASC_DENSE_TEST_CHECK(test, owner.ok());
  auto result = owner->view();
  ASC_DENSE_TEST_CHECK(test, SameScalarBits(result->data()[0], values[0]));
  ASC_DENSE_TEST_CHECK(test, SameScalarBits(result->data()[1], values[1]));
}

void TestBinaryInvalidEnvelopeWithValidCrc(TestContext& test) {
  constexpr std::string_view kCrcCheck = "123456789";
  const auto check =
      std::as_bytes(std::span(kCrcCheck.data(), kCrcCheck.size()));
  const auto first =
      asc::internal_array_io::UpdateCrc(0xffffffffU, check.first(4));
  ASC_DENSE_TEST_EQ(
      test,
      asc::internal_array_io::UpdateCrc(first, check.subspan(4)) ^ 0xffffffffU,
      std::uint32_t{0xcbf43926});
  // The checksum here is deliberately independent of the production helper.
  for (std::size_t offset :
       {std::size_t{8}, std::size_t{10}, std::size_t{12}, std::size_t{13},
        std::size_t{14}, std::size_t{16}, std::size_t{20}, std::size_t{24},
        std::size_t{32}, std::size_t{40}, std::size_t{48}, std::size_t{56}}) {
    auto bytes = kBinary;
    bytes[offset] ^= 0x80;
    std::uint32_t crc = 0xffffffffU;
    for (std::size_t i = 0; i < bytes.size() - 4; ++i) {
      crc ^= bytes[i];
      for (int bit = 0; bit < 8; ++bit) {
        crc = (crc >> 1) ^ ((crc & 1U) != 0 ? 0xedb88320U : 0U);
      }
    }
    crc ^= 0xffffffffU;
    for (std::size_t i = 0; i < 4; ++i) {
      bytes[bytes.size() - 4 + i] = static_cast<std::uint8_t>(crc >> (8 * i));
    }
    Source source(std::as_bytes(std::span(bytes)));
    std::array<std::byte, 1024> scratch{};
    std::array<asc::extent_t, 1> metadata{};
    asc::ArrayIoReport report;
    asc_dense_test::CountingMemoryResource resource;
    auto owner = asc::ReadDenseArrayBinary<std::int16_t, Shape1>(
        source, resource, asc::LayoutLeft{}, metadata, scratch, {}, report);
    ASC_DENSE_TEST_CHECK(test, !owner.ok());
    ASC_DENSE_TEST_EQ(test, resource.allocation_requests(), std::size_t{0});
    ASC_DENSE_TEST_CHECK(test, !report.committed);
  }
}

void TestTextSpecialValues(TestContext& test) {
  const std::array<double, 4> values{std::numeric_limits<double>::infinity(),
                                     -std::numeric_limits<double>::infinity(),
                                     std::numeric_limits<double>::quiet_NaN(),
                                     -0.0};
  auto view = View(values.data(), std::array<asc::extent_t, 1>{4});
  std::array<std::byte, 1024> scratch{};
  std::array<asc::extent_t, 1> metadata{};
  asc::ArrayIoReport report;
  Sink sink;
  ASC_DENSE_TEST_CHECK(
      test, asc::WriteDenseArrayText(*view, sink, {}, scratch, report).ok());
  ASC_DENSE_TEST_CHECK(test,
                       sink.text().ends_with("inf\n-inf\nnan\n-0\nend\n"));
  Source source(sink.bytes());
  asc_dense_test::CountingMemoryResource resource;
  auto owner = asc::ReadDenseArrayText<double, Shape1>(
      source, resource, asc::LayoutLeft{}, metadata, scratch, {}, report);
  ASC_DENSE_TEST_CHECK(test, owner.ok());
  auto result = owner->view();
  ASC_DENSE_TEST_EQ(test, result->data()[0], values[0]);
  ASC_DENSE_TEST_EQ(test, result->data()[1], values[1]);
  ASC_DENSE_TEST_CHECK(test, std::isnan(result->data()[2]));
  ASC_DENSE_TEST_CHECK(test, std::signbit(result->data()[3]));
}

asc::Result<std::filesystem::path> NewTemporaryDirectory() {
  std::error_code error;
  const auto temporary = std::filesystem::temp_directory_path(error);
  if (error) {
    return asc::Status(asc::ErrorCode::kIo);
  }
  const auto stamp =
      std::chrono::steady_clock::now().time_since_epoch().count();
  std::filesystem::path directory;
  bool created = false;
  for (int attempt = 0; attempt < 100 && !created; ++attempt) {
    directory = temporary / ("asc-dense-io-" + std::to_string(stamp) + "-" +
                             std::to_string(attempt));
    created = std::filesystem::create_directory(directory, error);
    if (error) {
      return asc::Status(asc::ErrorCode::kIo);
    }
  }
  if (!created) {
    return asc::Status(asc::ErrorCode::kIo);
  }
  return directory;
}

void TestPathRoundTripsAndFailures(TestContext& test) {
  auto temporary = NewTemporaryDirectory();
  ASC_DENSE_TEST_CHECK(test, temporary.ok());
  if (!temporary.ok()) {
    return;
  }
  const auto& directory = *temporary;
  std::error_code error;
  const auto path = directory / "array.asc";
  const std::array<double, 6> values{1, 4, 2, 5, 3, 6};
  auto view = View(values.data(), std::array<asc::extent_t, 2>{2, 3});
  std::array<std::byte, 1024> scratch{};
  std::array<asc::extent_t, 2> metadata{};
  asc::ArrayIoReport report;
  asc_dense_test::CountingMemoryResource resource;
  for (bool binary : {false, true}) {
    auto status = binary ? asc::SaveDenseArrayBinary(
                               path, *view, asc::ArrayFileOverwrite::kTruncate,
                               {}, scratch, report)
                         : asc::SaveDenseArrayText(
                               path, *view, asc::ArrayFileOverwrite::kTruncate,
                               {}, scratch, report);
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_EQ(test, report.cleanup_error, asc::ErrorCode::kOk);
    auto owner = binary ? asc::LoadDenseArrayBinary<double, Shape2>(
                              path, resource, asc::LayoutLeft{}, metadata,
                              scratch, {}, report)
                        : asc::LoadDenseArrayText<double, Shape2>(
                              path, resource, asc::LayoutLeft{}, metadata,
                              scratch, {}, report);
    ASC_DENSE_TEST_CHECK(test, owner.ok());
    ASC_DENSE_TEST_CHECK(test, report.committed);
    ASC_DENSE_TEST_EQ(test, report.cleanup_error, asc::ErrorCode::kOk);
    auto result = owner->view();
    for (std::size_t i = 0; i < values.size(); ++i) {
      ASC_DENSE_TEST_EQ(test, result->data()[i], values[i]);
    }
  }
  auto file = asc::File::OpenWrite(path);
  ASC_DENSE_TEST_CHECK(test, file.ok());
  const std::string trailing = std::string(kText) + "x";
  ASC_DENSE_TEST_CHECK(
      test, asc::WriteAll(*file, std::as_bytes(std::span(trailing.data(),
                                                         trailing.size())))
                .ok());
  ASC_DENSE_TEST_CHECK(test, file->Flush().ok());
  ASC_DENSE_TEST_CHECK(test, file->Close().ok());
  auto invalid = asc::LoadDenseArrayText<double, Shape2>(
      path, resource, asc::LayoutLeft{}, metadata, scratch, {}, report);
  ASC_DENSE_TEST_CHECK(test, !invalid.ok());
  ASC_DENSE_TEST_CHECK(test, !report.committed);
  ASC_DENSE_TEST_EQ(test, resource.live_allocations(), std::size_t{0});
  auto absent = asc::LoadDenseArrayText<double, Shape2>(
      directory / "absent", resource, asc::LayoutLeft{}, metadata, scratch, {},
      report);
  ASC_DENSE_TEST_CHECK(test, !absent.ok());
  ASC_DENSE_TEST_EQ(test, report.input_bytes, std::size_t{0});
  ASC_DENSE_TEST_CHECK(
      test, !asc::SaveDenseArrayText(directory / "absent" / "file", *view,
                                     asc::ArrayFileOverwrite::kTruncate, {},
                                     scratch, report)
                 .ok());
  ASC_DENSE_TEST_EQ(test, report.output_bytes, std::size_t{0});
#if defined(__linux__)
  // The standard Linux failing device checks an actual buffered flush failure.
  ASC_DENSE_TEST_CHECK(
      test, !asc::SaveDenseArrayText("/dev/full", *view,
                                     asc::ArrayFileOverwrite::kTruncate, {},
                                     scratch, report)
                 .ok());
  ASC_DENSE_TEST_EQ(test, report.section, asc::ArrayIoSection::kTrailer);
#endif
  ASC_DENSE_TEST_CHECK(test, std::filesystem::remove(path, error));
  ASC_DENSE_TEST_CHECK(test, !error);
  ASC_DENSE_TEST_CHECK(test, std::filesystem::remove(directory, error));
  ASC_DENSE_TEST_CHECK(test, !error);
}

void TestRankThreeStridedAndDirectRounding(TestContext& test) {
  std::array<double, 20> values{};
  values.fill(-999.0);
  const std::array<asc::extent_t, 3> shape{2, 2, 2};
  const std::array<asc::stride_t, 3> strides{1, 4, 12};
  auto mapping =
      asc::DenseLayout<3>::Create(shape, asc::LayoutStride<3>{strides});
  auto view = asc::DenseView<double, 3>::Create(values.data(), *mapping,
                                                asc::MemorySpace::kHost);
  for (std::size_t ordinal = 0; ordinal < 8; ++ordinal) {
    const auto offset =
        ordinal % 2 + 4 * ((ordinal / 2) % 2) + 12 * (ordinal / 4);
    values[offset] = static_cast<double>(ordinal + 1);
  }
  const auto original = values;
  std::array<std::byte, 1024> scratch{};
  std::array<asc::extent_t, 3> metadata{};
  asc::ArrayIoReport report;
  using Shape3 = asc::Extents<2, asc::kDynamicExtent, 2>;
  for (bool binary : {false, true}) {
    Sink sink;
    auto status =
        binary ? asc::WriteDenseArrayBinary(*view, sink, {}, scratch, report)
               : asc::WriteDenseArrayText(*view, sink, {}, scratch, report);
    ASC_DENSE_TEST_CHECK(test, status.ok());
    if (!binary) {
      ASC_DENSE_TEST_CHECK(
          test, sink.text().ends_with("data\n1\n2\n3\n4\n5\n6\n7\n8\nend\n"));
    }
    Source source(sink.bytes());
    asc_dense_test::CountingMemoryResource resource;
    auto owner = binary ? asc::ReadDenseArrayBinary<double, Shape3>(
                              source, resource, asc::LayoutRight{}, metadata,
                              scratch, {}, report)
                        : asc::ReadDenseArrayText<double, Shape3>(
                              source, resource, asc::LayoutRight{}, metadata,
                              scratch, {}, report);
    ASC_DENSE_TEST_CHECK(test, owner.ok());
    auto result = owner->view();
    constexpr std::array<double, 8> kRight{1, 5, 3, 7, 2, 6, 4, 8};
    for (std::size_t i = 0; i < kRight.size(); ++i) {
      ASC_DENSE_TEST_EQ(test, result->data()[i], kRight[i]);
    }
    ASC_DENSE_TEST_EQ(test, values, original);
  }
  constexpr std::string_view kRounding =
      "ASCARRAY 1\nkind dense\nscalar f32\nrank 1\nshape 1\norder dim0\n"
      "count 1\ndata\n1.0000000596046447753906250000000001\nend\n";
  Source source(kRounding);
  asc_dense_test::CountingMemoryResource resource;
  auto owner = asc::ReadDenseArrayText<float, Shape1>(
      source, resource, asc::LayoutLeft{}, metadata, scratch, {}, report);
  ASC_DENSE_TEST_CHECK(test, owner.ok());
  auto result = owner->view();
  ASC_DENSE_TEST_EQ(test, std::bit_cast<std::uint32_t>(result->data()[0]),
                    std::uint32_t{0x3f800001});
}

template <typename T, std::size_t Rank>
void CheckEofBudgets(TestContext& test, std::span<const std::byte> bytes,
                     const std::array<asc::extent_t, Rank>& shape,
                     bool binary) {
  for (int scenario = 0; scenario < 6; ++scenario) {
    std::array<std::byte, 512> input{};
    std::copy(bytes.begin(), bytes.end(), input.begin());
    std::size_t size = bytes.size();
    if (scenario >= 3) {
      input[size++] = scenario == 3 ? std::byte{'X'} : std::byte{' '};
    }
    Source source{std::span(input).first(size)};
    std::array<T, 6> target{};
    target.fill(T{9});
    const auto original = target;
    auto view = View(target.data(), shape);
    std::array<T, 6> staging{};
    std::array<asc::extent_t, Rank> metadata{};
    std::array<std::byte, 1024> scratch{};
    asc::ArrayIoLimits limits;
    limits.max_input_bytes =
        bytes.size() + (scenario >= 2 ? 1 : 0) + (scenario == 5 ? 1 : 0);
    asc::ArrayIoReport report;
    auto reader =
        binary ? asc::DenseArrayReader::PrepareBinary(
                     source, metadata, scratch, limits, report, scenario != 0)
               : asc::DenseArrayReader::PrepareText(
                     source, metadata, scratch, limits, report, scenario != 0);
    auto status = reader.ok() ? asc::ReadDenseArrayInto(*reader, *view,
                                                        std::span(staging))
                              : reader.status();
    const bool expected =
        scenario == 0 || scenario == 2 || (!binary && scenario == 5);
    ASC_DENSE_TEST_EQ(test, status.ok(), expected);
    ASC_DENSE_TEST_EQ(test, report.committed, expected);
    ASC_DENSE_TEST_EQ(test, report.input_bytes, source.position());
    ASC_DENSE_TEST_CHECK(test, report.input_bytes <= limits.max_input_bytes);
    if (!expected) {
      ASC_DENSE_TEST_EQ(test, target, original);
    }
  }
}

void TestWholeFileProbeBudgets(TestContext& test) {
  CheckEofBudgets<double>(test,
                          std::as_bytes(std::span(kText.data(), kText.size())),
                          std::array<asc::extent_t, 2>{2, 3}, false);
  CheckEofBudgets<std::int16_t>(test, std::as_bytes(std::span(kBinary)),
                                std::array<asc::extent_t, 1>{2}, true);
}

void TestMovedLongSourceErrors(TestContext& test) {
  for (std::size_t cutoff :
       {std::size_t{0}, kText.find("data\n") + 8, kText.size()}) {
    Source source(kText);
    source.fail_after = cutoff;
    source.error = asc::Status(asc::ErrorCode::kIo, std::string(8192, 'x'),
                               std::string(4096, 'p'), 1729);
    std::array<double, 8> target{9, 9, 77, 9, 9, 77, 9, 9};
    const auto original = target;
    auto view = View(target.data(), std::array<asc::extent_t, 2>{2, 3},
                     asc::LayoutStride<2>{{1, 3}});
    std::array<double, 6> staging{};
    std::array<std::byte, 1024> scratch{};
    std::array<asc::extent_t, 2> metadata{};
    asc::ArrayIoReport report;
    asc::Status status;
    std::size_t allocations = 0;
    {
      asc_dense_test::AllocationProbe probe;
      auto reader = asc::DenseArrayReader::PrepareText(
          source, metadata, scratch, {}, report, true);
      status = reader.ok()
                   ? asc::ReadDenseArrayInto(*reader, *view, std::span(staging))
                   : reader.status();
      allocations = probe.count();
    }
    ASC_DENSE_TEST_CHECK(
        test, asc_test::ProcessAllocationCountMatches(allocations, 0));
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kIo);
    ASC_DENSE_TEST_EQ(test, status.native_code(), std::int64_t{1729});
    ASC_DENSE_TEST_CHECK(test,
                         status.message().empty() && status.provider().empty());
    ASC_DENSE_TEST_EQ(test, report.input_bytes, cutoff);
    ASC_DENSE_TEST_EQ(test, target, original);
    ASC_DENSE_TEST_CHECK(test, !report.committed);
  }
}

void TestMovedLongSinkErrors(TestContext& test) {
  for (bool binary : {false, true}) {
    std::array<double, 6> values{1, 4, 2, 5, 3, 6};
    auto view = View(values.data(), std::array<asc::extent_t, 2>{2, 3});
    std::array<std::byte, 1024> scratch{};
    asc::ArrayIoReport report;
    Sink sink;
    sink.fail_after = 7;
    sink.error = asc::Status(asc::ErrorCode::kIo, std::string(8192, 'x'),
                             std::string(4096, 'p'), 1729);
    asc::Status status;
    std::size_t allocations = 0;
    {
      asc_dense_test::AllocationProbe probe;
      status =
          binary ? asc::WriteDenseArrayBinary(*view, sink, {}, scratch, report)
                 : asc::WriteDenseArrayText(*view, sink, {}, scratch, report);
      allocations = probe.count();
    }
    ASC_DENSE_TEST_CHECK(
        test, asc_test::ProcessAllocationCountMatches(allocations, 0));
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kIo);
    ASC_DENSE_TEST_EQ(test, status.native_code(), std::int64_t{1729});
    ASC_DENSE_TEST_CHECK(test,
                         status.message().empty() && status.provider().empty());
    ASC_DENSE_TEST_EQ(test, report.output_bytes, std::size_t{7});
  }
}

}  // namespace

int main() {
  TestContext test;
  TestIndependentTextAndLayouts(test);
  TestIndependentBinary(test);
  TestAllScalarCodes(test);
  TestEveryTextTruncationAndRollback(test);
  TestBinaryCorruptionAndRollback(test);
  TestSourceAndSinkFailures(test);
  TestAllocationAndTypeFailures(test);
  TestAdjacentFramesAndTrailingTransaction(test);
  TestScalarZeroAndEmptyRanks(test);
  TestIndependentLimitCaps(test);
  TestMalformedTokensAndCrlf(test);
  TestStagingAliasesAndPlacement(test);
  TestBinaryQuietNanComponents(test);
  TestBinaryInvalidEnvelopeWithValidCrc(test);
  TestTextSpecialValues(test);
  TestPathRoundTripsAndFailures(test);
  TestRankThreeStridedAndDirectRounding(test);
  TestWholeFileProbeBudgets(test);
  TestMovedLongSourceErrors(test);
  TestMovedLongSinkErrors(test);
  return test.Finish();
}
