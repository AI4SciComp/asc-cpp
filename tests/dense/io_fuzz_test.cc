#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "../allocation_observation.h"
#include "allocation_probe.h"
#include "asc/core/array_io.h"
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
constexpr std::string_view kText =
    "ASCARRAY 1\nkind dense\nscalar f64\nrank 2\nshape 2 3\n"
    "order dim0\ncount 6\ndata\n1\n4\n2\n5\n3\n6\nend\n";

class Source final : public asc::ByteSource {
 public:
  Source(std::span<const std::byte> bytes, std::size_t chunk)
      : bytes_(bytes), chunk_(chunk) {}
  asc::Result<std::size_t> ReadSome(std::span<std::byte> destination) override {
    const auto count = std::min({destination.size(), bytes_.size(), chunk_});
    std::copy_n(bytes_.begin(), count, destination.begin());
    bytes_ = bytes_.subspan(count);
    position += count;
    return count;
  }
  std::size_t position = 0;

 private:
  std::span<const std::byte> bytes_;
  std::size_t chunk_;
};

class Sink final : public asc::ByteSink {
 public:
  asc::Result<std::size_t> WriteSome(
      std::span<const std::byte> bytes) override {
    if (bytes.size() > storage.size() - size) {
      return asc::Status(asc::ErrorCode::kAllocation);
    }
    std::copy(bytes.begin(), bytes.end(), storage.begin() + size);
    size += bytes.size();
    return bytes.size();
  }
  std::array<std::byte, 512> storage{};
  std::size_t size = 0;
};

std::uint64_t Next(std::uint64_t& state) {
  // Fixed local corpus generator; intentionally independent of ASC Random.
  state ^= state << 13;
  state ^= state >> 7;
  state ^= state << 17;
  return state;
}

void CheckInput(TestContext& test, std::span<const std::byte> bytes,
                bool binary, std::uint64_t selector) {
  Source source(bytes, 1 + selector % 17);
  std::array<double, 12> target{};
  target.fill(-912.5);
  const auto original = target;
  std::array<double, 6> staging{};
  std::array<std::byte, 256> scratch{};
  std::array<asc::extent_t, 2> metadata{};
  auto mapping = asc::DenseLayout<2>::Create(std::array<asc::extent_t, 2>{2, 3},
                                             asc::LayoutStride<2>{{1, 4}});
  auto view = asc::DenseView<double, 2>::Create(target.data(), *mapping,
                                                asc::MemorySpace::kHost);
  asc::ArrayIoLimits limits;
  limits.max_input_bytes = selector % 5 == 0 ? selector % 180 : 1024;
  limits.max_header_bytes = 128;
  limits.max_token_bytes = 64;
  limits.max_rank = 2;
  limits.max_extent = 6;
  limits.max_logical_elements = 6;
  limits.max_decoded_bytes = 48;
  limits.max_staging_bytes = 48;
  limits.max_scratch_bytes = 272;
  limits.max_allocations = 0;
  limits.max_allocation_bytes = 0;
  asc::ArrayIoReport report;
  asc::Status status;
  std::size_t allocations = 0;
  {
    asc_dense_test::AllocationProbe probe;
    auto reader = binary ? asc::DenseArrayReader::PrepareBinary(
                               source, metadata, scratch, limits, report, true)
                         : asc::DenseArrayReader::PrepareText(
                               source, metadata, scratch, limits, report, true);
    status = reader.ok()
                 ? asc::ReadDenseArrayInto(*reader, *view, std::span(staging))
                 : reader.status();
    allocations = probe.count();
  }
  ASC_DENSE_TEST_CHECK(test,
                       asc_test::ProcessAllocationCountMatches(allocations, 0));
  ASC_DENSE_TEST_EQ(test, source.position, report.input_bytes);
  ASC_DENSE_TEST_CHECK(test, report.input_bytes <= limits.max_input_bytes);
  ASC_DENSE_TEST_EQ(test, report.committed, status.ok());
  if (!status.ok()) {
    ASC_DENSE_TEST_EQ(test, target, original);
  }
  for (std::size_t index : {2, 3, 6, 7, 10, 11}) {
    ASC_DENSE_TEST_EQ(test, target[index], original[index]);
  }
}

void Mutate(TestContext& test, std::span<const std::byte> seed, bool binary) {
  std::uint64_t state = 0x6a09e667f3bcc909ULL;
  for (std::size_t iteration = 0; iteration < 4096; ++iteration) {
    std::array<std::byte, 512> candidate{};
    std::copy(seed.begin(), seed.end(), candidate.begin());
    std::size_t size = seed.size();
    const auto selector = Next(state);
    switch (iteration % 4) {
      case 0:
        candidate[Next(state) % size] ^= static_cast<std::byte>(Next(state));
        break;
      case 1:
        size = Next(state) % (size + 1);
        break;
      case 2:
        for (std::size_t i = 0; i < 1 + selector % 8; ++i) {
          candidate[Next(state) % size] = static_cast<std::byte>(Next(state));
        }
        break;
      default:
        candidate[size++] = static_cast<std::byte>(Next(state));
        break;
    }
    CheckInput(test, std::span(candidate).first(size), binary, selector);
  }
}

}  // namespace

int main() {
  // Bounded deterministic mutation fuzzing, not coverage-guided libFuzzer.
  // The ordinary I/O test supplies independent exact-byte binary fixtures.
  TestContext test;
  const auto text = std::as_bytes(std::span(kText.data(), kText.size()));
  CheckInput(test, text, false, 1);
  Mutate(test, text, false);
  std::array<double, 6> values{1, 4, 2, 5, 3, 6};
  auto mapping =
      asc::DenseLayout<2>::Create(std::array<asc::extent_t, 2>{2, 3});
  auto view = asc::DenseView<double, 2>::Create(values.data(), *mapping,
                                                asc::MemorySpace::kHost);
  std::array<std::byte, 256> scratch{};
  asc::ArrayIoReport report;
  Sink sink;
  ASC_DENSE_TEST_CHECK(
      test, asc::WriteDenseArrayBinary(*view, sink, {}, scratch, report).ok());
  const auto binary = std::span(sink.storage).first(sink.size);
  CheckInput(test, binary, true, 1);
  Mutate(test, binary, true);
  return test.Finish();
}
