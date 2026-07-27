#include "asc/core/io.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

#include "test_support.h"

namespace {

static_assert(!std::is_copy_constructible_v<asc::File>);
static_assert(!std::is_copy_assignable_v<asc::File>);
static_assert(std::is_nothrow_move_constructible_v<asc::File>);
static_assert(std::is_nothrow_move_assignable_v<asc::File>);
static_assert(std::is_nothrow_destructible_v<asc::File>);
static_assert(noexcept(std::declval<asc::File&>().Close()));

class PartialSource final : public asc::ByteSource {
 public:
  PartialSource(std::vector<std::byte> bytes, std::size_t maximum_chunk)
      : bytes_(std::move(bytes)), maximum_chunk_(maximum_chunk) {}

  asc::Result<std::size_t> ReadSome(std::span<std::byte> destination) override {
    ++calls_;
    if (fail_after_.has_value() && position_ >= *fail_after_) {
      return asc::Status(asc::ErrorCode::kIo, "injected source failure");
    }
    const std::size_t count = std::min(
        {destination.size(), maximum_chunk_, bytes_.size() - position_});
    std::copy_n(bytes_.begin() + static_cast<std::ptrdiff_t>(position_), count,
                destination.begin());
    position_ += count;
    return count;
  }

  void FailAfter(std::size_t position) { fail_after_ = position; }
  [[nodiscard]] int calls() const noexcept { return calls_; }

 private:
  std::vector<std::byte> bytes_;
  std::size_t maximum_chunk_;
  std::size_t position_ = 0;
  std::optional<std::size_t> fail_after_;
  int calls_ = 0;
};

class PartialSink final : public asc::ByteSink {
 public:
  explicit PartialSink(std::size_t maximum_chunk)
      : maximum_chunk_(maximum_chunk) {}

  asc::Result<std::size_t> WriteSome(
      std::span<const std::byte> source) override {
    ++calls_;
    if (zero_progress_) {
      return std::size_t{0};
    }
    if (fail_after_.has_value() && bytes_.size() >= *fail_after_) {
      return asc::Status(asc::ErrorCode::kIo, "injected sink failure");
    }
    const std::size_t count = std::min(source.size(), maximum_chunk_);
    bytes_.insert(bytes_.end(), source.begin(),
                  source.begin() + static_cast<std::ptrdiff_t>(count));
    return count;
  }

  void FailAfter(std::size_t position) { fail_after_ = position; }
  void SetZeroProgress() { zero_progress_ = true; }
  [[nodiscard]] const std::vector<std::byte>& bytes() const noexcept {
    return bytes_;
  }
  [[nodiscard]] int calls() const noexcept { return calls_; }

 private:
  std::size_t maximum_chunk_;
  std::vector<std::byte> bytes_;
  std::optional<std::size_t> fail_after_;
  bool zero_progress_ = false;
  int calls_ = 0;
};

class OverreportingSource final : public asc::ByteSource {
 public:
  asc::Result<std::size_t> ReadSome(std::span<std::byte> destination) override {
    return destination.size() + 1U;
  }
};

class OverreportingSink final : public asc::ByteSink {
 public:
  asc::Result<std::size_t> WriteSome(
      std::span<const std::byte> source) override {
    return source.size() + 1U;
  }
};

std::vector<std::byte> Bytes(std::string_view text) {
  const auto span = std::as_bytes(std::span(text.data(), text.size()));
  return {span.begin(), span.end()};
}

void CheckPartialIo(asc_core_test::TestContext& context) {
  const std::vector<std::byte> expected = Bytes("abcdef");
  PartialSource source(expected, 2);
  std::array<std::byte, 6> destination{};
  ASC_TEST_CHECK(
      context, asc::ReadExact(source, std::span<std::byte>(destination)).ok());
  ASC_TEST_CHECK(context, std::equal(destination.begin(), destination.end(),
                                     expected.begin(), expected.end()));
  ASC_TEST_EQ(context, source.calls(), 3);

  PartialSource zero_source(expected, 2);
  ASC_TEST_CHECK(context,
                 asc::ReadExact(zero_source, std::span<std::byte>()).ok());
  ASC_TEST_EQ(context, zero_source.calls(), 0);

  PartialSource empty_source({}, 2);
  std::array<std::byte, 1> one_byte{};
  const asc::Status eof =
      asc::ReadExact(empty_source, std::span<std::byte>(one_byte));
  ASC_TEST_EQ(context, eof.code(), asc::ErrorCode::kEndOfFile);

  PartialSource short_source(Bytes("ab"), 1);
  std::array<std::byte, 3> three_bytes{};
  const asc::Status short_input =
      asc::ReadExact(short_source, std::span<std::byte>(three_bytes));
  ASC_TEST_EQ(context, short_input.code(), asc::ErrorCode::kEndOfFile);

  PartialSource failing_source(expected, 2);
  failing_source.FailAfter(2);
  const asc::Status source_failure =
      asc::ReadExact(failing_source, std::span<std::byte>(destination));
  ASC_TEST_EQ(context, source_failure.code(), asc::ErrorCode::kIo);

  OverreportingSource overreporting_source;
  const asc::Status source_contract =
      asc::ReadExact(overreporting_source, std::span<std::byte>(one_byte));
  ASC_TEST_EQ(context, source_contract.code(), asc::ErrorCode::kInternal);

  PartialSink sink(2);
  ASC_TEST_CHECK(
      context, asc::WriteAll(sink, std::span<const std::byte>(expected)).ok());
  ASC_TEST_CHECK(context, sink.bytes() == expected);
  ASC_TEST_EQ(context, sink.calls(), 3);

  PartialSink zero_sink(2);
  ASC_TEST_CHECK(context,
                 asc::WriteAll(zero_sink, std::span<const std::byte>()).ok());
  ASC_TEST_EQ(context, zero_sink.calls(), 0);

  PartialSink no_progress_sink(2);
  no_progress_sink.SetZeroProgress();
  const asc::Status no_progress =
      asc::WriteAll(no_progress_sink, std::span<const std::byte>(expected));
  ASC_TEST_EQ(context, no_progress.code(), asc::ErrorCode::kIo);

  PartialSink failing_sink(2);
  failing_sink.FailAfter(2);
  const asc::Status sink_failure =
      asc::WriteAll(failing_sink, std::span<const std::byte>(expected));
  ASC_TEST_EQ(context, sink_failure.code(), asc::ErrorCode::kIo);

  OverreportingSink overreporting_sink;
  const asc::Status sink_contract =
      asc::WriteAll(overreporting_sink, std::span<const std::byte>(expected));
  ASC_TEST_EQ(context, sink_contract.code(), asc::ErrorCode::kInternal);
}

template <typename T>
void CheckRoundTrip(asc_core_test::TestContext& context, T value) {
  std::array<std::byte, sizeof(T)> encoded{};
  ASC_TEST_CHECK(
      context,
      asc::EncodeLittleEndian(value, std::span<std::byte>(encoded)).ok());
  const auto decoded =
      asc::DecodeLittleEndian<T>(std::span<const std::byte>(encoded));
  ASC_TEST_CHECK(context, decoded.ok());
  if constexpr (std::floating_point<T>) {
    using Bits =
        std::conditional_t<sizeof(T) == 4, std::uint32_t, std::uint64_t>;
    ASC_TEST_EQ(context, std::bit_cast<Bits>(*decoded),
                std::bit_cast<Bits>(value));
  } else {
    ASC_TEST_EQ(context, *decoded, value);
  }
}

void CheckLittleEndian(asc_core_test::TestContext& context) {
  std::array<std::byte, 2> encoded{};
  ASC_TEST_CHECK(context,
                 asc::EncodeLittleEndian(std::uint16_t{0x1234}, encoded).ok());
  ASC_TEST_EQ(context, encoded[0], std::byte{0x34});
  ASC_TEST_EQ(context, encoded[1], std::byte{0x12});
  std::array<std::byte, 4> one_float{};
  ASC_TEST_CHECK(
      context,
      asc::EncodeLittleEndian(1.0F, std::span<std::byte>(one_float)).ok());
  const std::array<std::byte, 4> expected_one_float = {
      std::byte{0x00}, std::byte{0x00}, std::byte{0x80}, std::byte{0x3f}};
  ASC_TEST_CHECK(context, one_float == expected_one_float);

  CheckRoundTrip(context, std::int8_t{-2});
  CheckRoundTrip(context, std::uint8_t{250});
  CheckRoundTrip(context, std::int16_t{-1234});
  CheckRoundTrip(context, std::uint16_t{54321});
  CheckRoundTrip(context, std::int32_t{-1234567});
  CheckRoundTrip(context, std::uint32_t{3456789012U});
  CheckRoundTrip(context, std::numeric_limits<std::int64_t>::min());
  CheckRoundTrip(context, std::numeric_limits<std::uint64_t>::max());
  CheckRoundTrip(context, -0.0F);
  CheckRoundTrip(context, 1.0F);
  CheckRoundTrip(context, -0.0);
  CheckRoundTrip(context, 1.0);
  CheckRoundTrip(context, std::numeric_limits<double>::quiet_NaN());

  std::array<std::byte, 3> wrong_size = {std::byte{0x5a}, std::byte{0x5a},
                                         std::byte{0x5a}};
  const asc::Status encode_failure = asc::EncodeLittleEndian(
      std::uint16_t{1}, std::span<std::byte>(wrong_size));
  ASC_TEST_EQ(context, encode_failure.code(), asc::ErrorCode::kEncoding);
  ASC_TEST_CHECK(context, std::all_of(wrong_size.begin(), wrong_size.end(),
                                      [](std::byte value) {
                                        return value == std::byte{0x5a};
                                      }));
  const auto decode_failure = asc::DecodeLittleEndian<std::uint32_t>(
      std::span<const std::byte>(wrong_size));
  ASC_TEST_CHECK(context, !decode_failure.ok());
  ASC_TEST_EQ(context, decode_failure.status().code(),
              asc::ErrorCode::kEncoding);
}

void CheckFiles(asc_core_test::TestContext& context) {
  const std::filesystem::path path =
      std::filesystem::current_path() / "asc core io test file.txt";
  const std::filesystem::path second_path =
      std::filesystem::current_path() / "asc core io second file.txt";
  std::error_code cleanup_error;
  std::filesystem::remove(path, cleanup_error);
  cleanup_error.clear();
  std::filesystem::remove(second_path, cleanup_error);

  const std::string text = "portable\ntext\n";
  ASC_TEST_CHECK(context, asc::WriteTextFile(path, text).ok());

  const auto exact_text = asc::ReadTextFile(path, text.size());
  ASC_TEST_CHECK(context, exact_text.ok());
  ASC_TEST_EQ(context, *exact_text, text);
  const auto limited_text = asc::ReadTextFile(path, text.size() - 1U);
  ASC_TEST_CHECK(context, !limited_text.ok());
  ASC_TEST_EQ(context, limited_text.status().code(), asc::ErrorCode::kOverflow);
  ASC_TEST_CHECK(context, asc::WriteTextFile(second_path, "").ok());
  const auto empty_text = asc::ReadTextFile(second_path, 0);
  ASC_TEST_CHECK(context, empty_text.ok());
  ASC_TEST_CHECK(context, empty_text->empty());

  auto reader = asc::File::OpenRead(path);
  ASC_TEST_CHECK(context, reader.ok());
  ASC_TEST_CHECK(context, reader->is_open());
  ASC_TEST_CHECK(context, reader->readable());
  ASC_TEST_CHECK(context, !reader->writable());
  const std::vector<std::byte> bytes = Bytes("x");
  ASC_TEST_EQ(
      context,
      reader->WriteSome(std::span<const std::byte>(bytes)).status().code(),
      asc::ErrorCode::kInvalidState);
  ASC_TEST_EQ(context, reader->Flush().code(), asc::ErrorCode::kInvalidState);

  asc::File moved_reader(std::move(*reader));
  ASC_TEST_CHECK(context, moved_reader.is_open());
  ASC_TEST_CHECK(context, !reader->is_open());
  ASC_TEST_CHECK(context, moved_reader.Close().ok());
  ASC_TEST_CHECK(context, moved_reader.Close().ok());
  std::array<std::byte, 1> byte{};
  ASC_TEST_EQ(context, moved_reader.ReadSome(byte).status().code(),
              asc::ErrorCode::kInvalidState);

  auto first_writer = asc::File::OpenWrite(path);
  auto second_writer = asc::File::OpenWrite(second_path);
  ASC_TEST_CHECK(context, first_writer.ok());
  ASC_TEST_CHECK(context, second_writer.ok());
  ASC_TEST_CHECK(context, first_writer->Close().ok());
  *first_writer = std::move(*second_writer);
  ASC_TEST_CHECK(context, first_writer->is_open());
  ASC_TEST_CHECK(context, !second_writer->is_open());
  ASC_TEST_EQ(context, first_writer->ReadSome(byte).status().code(),
              asc::ErrorCode::kInvalidState);
  ASC_TEST_CHECK(context, first_writer->Close().ok());

  const auto missing = asc::File::OpenRead(path.string() + ".does-not-exist");
  ASC_TEST_CHECK(context, !missing.ok());
  ASC_TEST_EQ(context, missing.status().code(), asc::ErrorCode::kIo);

  cleanup_error.clear();
  std::filesystem::remove(path, cleanup_error);
  ASC_TEST_CHECK(context, !cleanup_error);
  cleanup_error.clear();
  std::filesystem::remove(second_path, cleanup_error);
  ASC_TEST_CHECK(context, !cleanup_error);
}

}  // namespace

int main() {
  asc_core_test::TestContext context;
  CheckPartialIo(context);
  CheckLittleEndian(context);
  CheckFiles(context);
  return context.Finish();
}
