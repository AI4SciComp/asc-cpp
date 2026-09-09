#include "asc/sparse/io.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "../locale_test_support.h"
#include "allocation_observation.h"
#include "allocation_probe.h"
#include "asc/core/array_format.h"
#include "asc/core/array_io.h"
#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/io.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/sparse/compressed.h"
#include "asc/sparse/coordinate.h"
#include "test_support.h"

namespace {

using asc_sparse_test::TestContext;
using Shape2 = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;
constexpr auto kHost = asc::MemorySpace::kHost;

template <typename Value>
Value Take(asc::Result<Value> result) {
  if (!result.ok()) {
    std::abort();
  }
  return std::move(*result);
}

class Source final : public asc::ByteSource {
 public:
  explicit Source(std::span<const std::byte> bytes) : bytes_(bytes) {}
  explicit Source(std::string_view text)
      : bytes_(std::as_bytes(std::span(text.data(), text.size()))) {}
  asc::Result<std::size_t> ReadSome(std::span<std::byte> output) override {
    if (position_ == fail_after) {
      return asc::Status(asc::ErrorCode::kIo);
    }
    const auto count = std::min({output.size(), bytes_.size() - position_,
                                 max_chunk, fail_after - position_});
    if (count != 0) {
      std::copy_n(bytes_.data() + position_, count, output.data());
    }
    position_ += count;
    return count;
  }
  [[nodiscard]] std::size_t position() const { return position_; }
  std::size_t max_chunk = 3;
  std::size_t fail_after = std::numeric_limits<std::size_t>::max();

 private:
  std::span<const std::byte> bytes_;
  std::size_t position_ = 0;
};

class Sink final : public asc::ByteSink {
 public:
  asc::Result<std::size_t> WriteSome(
      std::span<const std::byte> bytes) override {
    if (zero_progress) {
      return std::size_t{0};
    }
    if (size_ == fail_after) {
      return asc::Status(asc::ErrorCode::kIo);
    }
    const auto count = std::min(
        {bytes.size(), storage_.size() - size_, max_chunk, fail_after - size_});
    if (count != 0) {
      std::copy_n(bytes.data(), count, storage_.data() + size_);
    }
    size_ += count;
    return count;
  }
  [[nodiscard]] std::span<const std::byte> bytes() const {
    return std::span(storage_).first(size_);
  }
  [[nodiscard]] std::string_view text() const {
    return {reinterpret_cast<const char*>(storage_.data()), size_};
  }
  std::size_t max_chunk = 2;
  std::size_t fail_after = std::numeric_limits<std::size_t>::max();
  bool zero_progress = false;

 private:
  std::array<std::byte, 16384> storage_{};
  std::size_t size_ = 0;
};

// Count every request, but only nonzero successful storage as live. Buffer does
// not call Deallocate for its null zero-byte allocation result.
class IoResource final : public asc::MemoryResource {
 public:
  explicit IoResource(asc::MemorySpace space = kHost) : space_(space) {}
  [[nodiscard]] asc::MemorySpace space() const noexcept override {
    return space_;
  }
  asc::Result<void*> Allocate(std::size_t bytes,
                              std::size_t alignment) override {
    if (++attempts_ == failure_) {
      if (long_error_) {
        return std::move(failure_status_);
      }
      return asc::Status(asc::ErrorCode::kAllocation);
    }
    auto result = host_.Allocate(bytes, alignment);
    if (result.ok() && bytes != 0) {
      ++live_;
    }
    return result;
  }
  void Deallocate(void* pointer, std::size_t bytes,
                  std::size_t alignment) noexcept override {
    host_.Deallocate(pointer, bytes, alignment);
    if (bytes != 0) {
      --live_;
    }
  }
  void FailOnAllocation(std::size_t attempt) { failure_ = attempt; }
  void FailWithLongError(std::size_t attempt) {
    failure_ = attempt;
    long_error_ = true;
    failure_status_ =
        asc::Status(asc::ErrorCode::kAllocation, std::string(256, 'm'),
                    std::string(256, 'p'), 91);
  }
  [[nodiscard]] std::size_t allocation_attempts() const { return attempts_; }
  [[nodiscard]] std::size_t live_allocations() const { return live_; }

 private:
  asc::HostMemoryResource host_;
  asc::MemorySpace space_;
  std::size_t attempts_ = 0;
  std::size_t live_ = 0;
  std::size_t failure_ = std::numeric_limits<std::size_t>::max();
  bool long_error_ = false;
  asc::Status failure_status_;
};

template <typename T>
T Maximum() {
  if constexpr (asc::internal_array_format::kComplex<T>) {
    using Real = typename T::value_type;
    return T(std::numeric_limits<Real>::max(),
             -std::numeric_limits<Real>::max());
  } else {
    return std::numeric_limits<T>::max();
  }
}

template <typename T>
T Minimum() {
  if constexpr (asc::internal_array_format::kComplex<T>) {
    using Real = typename T::value_type;
    return T(std::numeric_limits<Real>::denorm_min(), -Real{0});
  } else {
    return std::numeric_limits<T>::lowest();
  }
}

template <typename T>
bool SameBits(T first, T second) {
  if constexpr (asc::internal_array_format::kComplex<T>) {
    return SameBits(first.real(), second.real()) &&
           SameBits(first.imag(), second.imag());
  } else {
    return std::bit_cast<std::array<std::byte, sizeof(T)>>(first) ==
           std::bit_cast<std::array<std::byte, sizeof(T)>>(second);
  }
}

template <typename View>
auto Load(asc::SparseArrayReader& reader, asc::MemoryResource& resource) {
  using Value = typename View::value_type;
  if constexpr (requires { View::kFormat; }) {
    return asc::ReadSparseArray<Value, View::kFormat>(reader, resource);
  } else {
    return asc::ReadSparseArray<Value, Shape2>(reader, resource);
  }
}

template <typename View>
void RoundTrip(TestContext& test, const View& view) {
  using T = typename View::value_type;
  for (bool binary : {false, true}) {
    std::array<std::byte, 128> scratch{};
    std::array<asc::extent_t, 2> metadata{};
    asc::ArrayIoLimits limits;
    asc::ArrayIoReport report;
    Sink sink;
    {
      asc_sparse_test::AllocationProbe probe;
      const auto status =
          binary
              ? asc::WriteSparseArrayBinary(view, sink, limits, scratch, report)
              : asc::WriteSparseArrayText(view, sink, limits, scratch, report);
      ASC_SPARSE_TEST_CHECK(test, status.ok());
      ASC_SPARSE_TEST_CHECK(
          test, asc_test::ProcessAllocationCountMatches(probe.count(), 0));
    }
    ASC_SPARSE_TEST_EQ(test, report.output_bytes, sink.bytes().size());
    Source source(sink.bytes());
    IoResource resource;
    {
      auto prepared =
          binary ? asc::SparseArrayReader::PrepareBinary(
                       source, metadata, scratch, limits, report, true)
                 : asc::SparseArrayReader::PrepareText(
                       source, metadata, scratch, limits, report, true);
      ASC_SPARSE_TEST_CHECK(test, prepared.ok());
      ASC_SPARSE_TEST_EQ(test, resource.allocation_attempts(), std::size_t{0});
      if (!prepared.ok()) {
        return;
      }
      auto owner = Load<View>(*prepared, resource);
      ASC_SPARSE_TEST_CHECK(test, owner.ok());
      if (!owner.ok()) {
        return;
      }
      const auto loaded = Take(owner->view());
      ASC_SPARSE_TEST_EQ(test, loaded.nnz(), view.nnz());
      for (asc::nnz_t i = 0; i < view.nnz(); ++i) {
        ASC_SPARSE_TEST_CHECK(test,
                              SameBits(loaded.values()[i], view.values()[i]));
      }
      Sink repeated;
      const auto written = binary
                               ? asc::WriteSparseArrayBinary(
                                     *owner, repeated, limits, scratch, report)
                               : asc::WriteSparseArrayText(
                                     *owner, repeated, limits, scratch, report);
      ASC_SPARSE_TEST_CHECK(test, written.ok());
      ASC_SPARSE_TEST_CHECK(
          test, std::equal(sink.bytes().begin(), sink.bytes().end(),
                           repeated.bytes().begin(), repeated.bytes().end()));
      std::array<T, 3> staged{};
      Source value_source(sink.bytes());
      {
        asc_sparse_test::AllocationProbe probe;
        const auto into = binary ? asc::ReadSparseArrayBinaryInto(
                                       value_source, loaded, std::span(staged),
                                       metadata, scratch, limits, report, true)
                                 : asc::ReadSparseArrayTextInto(
                                       value_source, loaded, std::span(staged),
                                       metadata, scratch, limits, report, true);
        ASC_SPARSE_TEST_CHECK(test, into.ok());
        ASC_SPARSE_TEST_CHECK(
            test, asc_test::ProcessAllocationCountMatches(probe.count(), 0));
      }
      ASC_SPARSE_TEST_CHECK(test, report.committed);
      ASC_SPARSE_TEST_EQ(test, report.input_bytes, sink.bytes().size());
      if (test.Finish() == 0) {
        std::cout << "roundtrip kind=" << static_cast<int>(prepared->kind())
                  << " scalar=" << static_cast<int>(prepared->scalar())
                  << " binary=" << binary << '\n';
      }
    }
    ASC_SPARSE_TEST_EQ(test, resource.live_allocations(), std::size_t{0});
  }
}

template <typename T>
void AllKinds(TestContext& test) {
  std::array<T, 3> values{T{0}, Maximum<T>(), Minimum<T>()};
  const std::array<asc::extent_t, 2> shape{2, 3};
  const std::array<asc::index_t, 6> coordinates{0, 0, 1, 1, 1, 2};
  auto coo = Take(asc::CoordinateView<T, 2>::Create(
      coordinates.data(), values.data(), shape, 3, kHost));
  RoundTrip(test, coo);
  const std::array<asc::nnz_t, 3> rows{0, 1, 3};
  const std::array<asc::index_t, 3> columns{0, 1, 2};
  auto csr = Take(asc::CsrView<T>::Create(rows.data(), columns.data(),
                                          values.data(), shape, 3, kHost));
  RoundTrip(test, csr);
  const std::array<asc::nnz_t, 4> offsets{0, 1, 2, 3};
  const std::array<asc::index_t, 3> indices{0, 1, 1};
  auto csc = Take(asc::CscView<T>::Create(offsets.data(), indices.data(),
                                          values.data(), shape, 3, kHost));
  RoundTrip(test, csc);
}

template <typename View, typename Reader>
void TruncationCases(TestContext& test, const View& view, const Sink& encoded,
                     bool binary, Reader read_into,
                     const std::array<typename View::value_type, 3>& original,
                     std::array<asc::extent_t, 2>& metadata,
                     std::array<std::byte, 128>& scratch,
                     asc::ArrayIoLimits& limits, asc::ArrayIoReport& report) {
  for (std::size_t end = 0; end < encoded.bytes().size(); ++end) {
    Source truncated(encoded.bytes().first(end));
    {
      asc_sparse_test::AllocationProbe probe;
      ASC_SPARSE_TEST_CHECK(test, !read_into(truncated).ok());
      ASC_SPARSE_TEST_CHECK(
          test, asc_test::ProcessAllocationCountMatches(probe.count(), 0));
    }
    ASC_SPARSE_TEST_CHECK(test, !report.committed);
    ASC_SPARSE_TEST_EQ(test, report.input_bytes, truncated.position());
    ASC_SPARSE_TEST_CHECK(
        test, std::equal(original.begin(), original.end(), view.values()));
    Source failed(encoded.bytes());
    failed.fail_after = end;
    ASC_SPARSE_TEST_CHECK(test, !read_into(failed).ok());
    ASC_SPARSE_TEST_EQ(test, report.input_bytes, failed.position());
    ASC_SPARSE_TEST_CHECK(
        test, std::equal(original.begin(), original.end(), view.values()));
    Sink short_sink;
    short_sink.fail_after = end;
    const auto saved = binary ? asc::WriteSparseArrayBinary(
                                    view, short_sink, limits, scratch, report)
                              : asc::WriteSparseArrayText(
                                    view, short_sink, limits, scratch, report);
    ASC_SPARSE_TEST_CHECK(test, !saved.ok());
    ASC_SPARSE_TEST_EQ(test, report.output_bytes, end);
    ASC_SPARSE_TEST_CHECK(
        test, std::equal(short_sink.bytes().begin(), short_sink.bytes().end(),
                         encoded.bytes().begin()));
    Source owner_source(encoded.bytes().first(end));
    auto reader = binary ? asc::SparseArrayReader::PrepareBinary(
                               owner_source, metadata, scratch, limits, report)
                         : asc::SparseArrayReader::PrepareText(
                               owner_source, metadata, scratch, limits, report);
    IoResource resource;
    if (reader.ok()) {
      auto owner = Load<View>(*reader, resource);
      ASC_SPARSE_TEST_CHECK(test, !owner.ok());
      ASC_SPARSE_TEST_CHECK(test, !reader->ready());
    }
    ASC_SPARSE_TEST_EQ(test, resource.live_allocations(), std::size_t{0});
    ASC_SPARSE_TEST_CHECK(test, !report.committed);
  }
}

template <typename View>
void ResourceCases(TestContext& test, const Sink& encoded, bool binary,
                   std::array<asc::extent_t, 2>& metadata,
                   std::array<std::byte, 128>& scratch,
                   asc::ArrayIoLimits& limits, asc::ArrayIoReport& report) {
  const std::size_t requests = requires { View::kFormat; } ? 5 : 4;
  for (std::size_t failure = 1; failure <= requests; ++failure) {
    Source source(encoded.bytes());
    auto reader = binary ? asc::SparseArrayReader::PrepareBinary(
                               source, metadata, scratch, limits, report)
                         : asc::SparseArrayReader::PrepareText(
                               source, metadata, scratch, limits, report);
    IoResource resource;
    resource.FailOnAllocation(failure);
    auto owner = Load<View>(*reader, resource);
    ASC_SPARSE_TEST_CHECK(test, !owner.ok());
    ASC_SPARSE_TEST_EQ(test, resource.allocation_attempts(), failure);
    ASC_SPARSE_TEST_EQ(test, resource.live_allocations(), std::size_t{0});
    ASC_SPARSE_TEST_CHECK(test, !report.committed);
  }
  for (std::size_t failure = 1; failure <= requests; ++failure) {
    Source source(encoded.bytes());
    auto reader = binary ? asc::SparseArrayReader::PrepareBinary(
                               source, metadata, scratch, limits, report)
                         : asc::SparseArrayReader::PrepareText(
                               source, metadata, scratch, limits, report);
    IoResource resource;
    resource.FailWithLongError(failure);
    {
      asc_sparse_test::AllocationProbe probe;
      auto owner = Load<View>(*reader, resource);
      ASC_SPARSE_TEST_CHECK(test, !owner.ok());
      ASC_SPARSE_TEST_CHECK(
          test,
          asc_test::ProcessAllocationCountMatches(
              probe.count(),
              asc_test::ProcessVisibleResourceAllocationCount(failure - 1)));
    }
    ASC_SPARSE_TEST_EQ(test, resource.allocation_attempts(), failure);
    ASC_SPARSE_TEST_EQ(test, resource.live_allocations(), std::size_t{0});
    ASC_SPARSE_TEST_CHECK(test, !report.committed);
  }
  for (std::size_t cap = 0; cap < requests; ++cap) {
    limits.max_allocations = cap;
    Source source(encoded.bytes());
    auto reader = binary ? asc::SparseArrayReader::PrepareBinary(
                               source, metadata, scratch, limits, report)
                         : asc::SparseArrayReader::PrepareText(
                               source, metadata, scratch, limits, report);
    IoResource resource;
    auto owner = Load<View>(*reader, resource);
    ASC_SPARSE_TEST_CHECK(test, !owner.ok());
    ASC_SPARSE_TEST_EQ(test, resource.allocation_attempts(), std::size_t{0});
  }
}

template <typename View, typename Reader>
void CorruptionCases(TestContext& test, const View& view, const Sink& encoded,
                     bool binary, Reader read_into,
                     const std::array<typename View::value_type, 3>& original,
                     std::array<typename View::value_type, 3>& staging,
                     std::array<asc::extent_t, 2>& metadata,
                     std::array<std::byte, 128>& scratch,
                     asc::ArrayIoLimits& limits, asc::ArrayIoReport& report) {
  limits = {};
  for (std::size_t byte = 0; byte < encoded.bytes().size(); ++byte) {
    std::array<std::byte, 16384> mutation{};
    std::copy(encoded.bytes().begin(), encoded.bytes().end(), mutation.begin());
    mutation[byte] ^= std::byte{0x80};
    Source source(
        std::span<const std::byte>(mutation).first(encoded.bytes().size()));
    ASC_SPARSE_TEST_CHECK(test, !read_into(source).ok());
    ASC_SPARSE_TEST_CHECK(test, !report.committed);
    ASC_SPARSE_TEST_CHECK(
        test, std::equal(original.begin(), original.end(), view.values()));
  }
  Sink stopped;
  stopped.zero_progress = true;
  ASC_SPARSE_TEST_CHECK(
      test, !(binary ? asc::WriteSparseArrayBinary(view, stopped, limits,
                                                   scratch, report)
                     : asc::WriteSparseArrayText(view, stopped, limits, scratch,
                                                 report))
                 .ok());
  ASC_SPARSE_TEST_EQ(test, report.output_bytes, std::size_t{0});
  Source alias(encoded.bytes());
  const auto target_bytes = std::as_writable_bytes(std::span(view.values(), 3));
  const auto aliased = binary ? asc::ReadSparseArrayBinaryInto(
                                    alias, view, std::span(staging), metadata,
                                    target_bytes, limits, report)
                              : asc::ReadSparseArrayTextInto(
                                    alias, view, std::span(staging), metadata,
                                    target_bytes, limits, report);
  ASC_SPARSE_TEST_CHECK(test, !aliased.ok());
  ASC_SPARSE_TEST_EQ(test, alias.position(), std::size_t{0});
  ASC_SPARSE_TEST_CHECK(
      test, std::equal(original.begin(), original.end(), view.values()));
}

template <typename View>
void ValidStructureMismatch(TestContext& test, const View& view,
                            const Sink& encoded) {
  auto bytes = std::array<std::byte, 256>{};
  std::copy(encoded.bytes().begin(), encoded.bytes().end(), bytes.begin());
  std::size_t offset = 80;  // COO first column coordinate after 72-byte header.
  if constexpr (requires { View::kFormat; }) {
    offset = View::kFormat == asc::SparseCompressedFormat::kCsr ? 96 : 104;
  }
  bytes[offset] =
      std::byte{1};  // A different, still valid canonical structure.
  const auto size = encoded.bytes().size();
  const auto checksum =
      asc::internal_array_io::UpdateCrc(std::uint32_t{0xffffffff},
                                        std::span(bytes).first(size - 4)) ^
      std::uint32_t{0xffffffff};
  ASC_SPARSE_TEST_CHECK(
      test,
      asc::EncodeLittleEndian(checksum, std::span(bytes).subspan(size - 4, 4))
          .ok());
  std::array<asc::extent_t, 2> metadata{};
  std::array<std::byte, 128> scratch{};
  asc::ArrayIoLimits limits;
  asc::ArrayIoReport report;
  Source valid(std::span<const std::byte>(bytes).first(size));
  auto reader = Take(asc::SparseArrayReader::PrepareBinary(
      valid, metadata, scratch, limits, report, true));
  IoResource resource;
  auto owner = Load<View>(reader, resource);
  ASC_SPARSE_TEST_CHECK(test, owner.ok());
  std::array<typename View::value_type, 3> original{};
  std::copy_n(view.values(), 3, original.data());
  std::array<typename View::value_type, 3> staging{};
  Source into(std::span<const std::byte>(bytes).first(size));
  const auto status = asc::ReadSparseArrayBinaryInto(
      into, view, std::span(staging), metadata, scratch, limits, report, true);
  ASC_SPARSE_TEST_CHECK(test, !status.ok());
  ASC_SPARSE_TEST_CHECK(test, !report.committed);
  ASC_SPARSE_TEST_CHECK(
      test, std::equal(original.begin(), original.end(), view.values()));
}

template <typename View>
void FailurePaths(TestContext& test, const View& view) {
  using T = typename View::value_type;
  std::array<T, 3> original{};
  std::copy_n(view.values(), 3, original.data());
  std::array<T, 3> staging{};
  std::array<asc::extent_t, 2> metadata{};
  std::array<std::byte, 128> scratch{};
  asc::ArrayIoLimits limits;
  asc::ArrayIoReport report;
  for (bool binary : {false, true}) {
    Sink encoded;
    ASC_SPARSE_TEST_CHECK(
        test, (binary ? asc::WriteSparseArrayBinary(view, encoded, limits,
                                                    scratch, report)
                      : asc::WriteSparseArrayText(view, encoded, limits,
                                                  scratch, report))
                  .ok());
    const auto read_into = [&](Source& source) {
      return binary
                 ? asc::ReadSparseArrayBinaryInto(source, view,
                                                  std::span(staging), metadata,
                                                  scratch, limits, report, true)
                 : asc::ReadSparseArrayTextInto(source, view,
                                                std::span(staging), metadata,
                                                scratch, limits, report, true);
    };
    TruncationCases(test, view, encoded, binary, read_into, original, metadata,
                    scratch, limits, report);
    ResourceCases<View>(test, encoded, binary, metadata, scratch, limits,
                        report);
    CorruptionCases(test, view, encoded, binary, read_into, original, staging,
                    metadata, scratch, limits, report);
    if (binary) {
      ValidStructureMismatch(test, view, encoded);
    }
    if (test.Finish() == 0) {
      std::cout << "failure_paths kind="
                << static_cast<int>(
                       asc::internal_sparse_io::ViewTraits<View>::kKind)
                << " binary=" << binary
                << " frame_bytes=" << encoded.bytes().size() << '\n';
    }
  }
}

void Failures(TestContext& test) {
  std::array<double, 3> values{0, 3, -2};
  const std::array<asc::extent_t, 2> shape{2, 3};
  const std::array<asc::index_t, 6> coordinates{0, 0, 1, 1, 1, 2};
  FailurePaths(test, Take(asc::CoordinateView<double, 2>::Create(
                         coordinates.data(), values.data(), shape, 3, kHost)));
  const std::array<asc::nnz_t, 3> rows{0, 1, 3};
  const std::array<asc::index_t, 3> columns{0, 1, 2};
  FailurePaths(
      test, Take(asc::CsrView<double>::Create(rows.data(), columns.data(),
                                              values.data(), shape, 3, kHost)));
  const std::array<asc::nnz_t, 4> offsets{0, 1, 2, 3};
  const std::array<asc::index_t, 3> indices{0, 1, 1};
  FailurePaths(
      test, Take(asc::CscView<double>::Create(offsets.data(), indices.data(),
                                              values.data(), shape, 3, kHost)));
}

template <typename Sequence>
struct DynamicShape;
template <std::size_t... Dimensions>
struct DynamicShape<std::index_sequence<Dimensions...>> {
  using Type = asc::Extents<(asc::kDynamicExtent +
                             static_cast<asc::extent_t>(0 * Dimensions))...>;
};

template <typename Owner, std::size_t Rank>
void EmptyCoordinateBudget(TestContext& test, const Sink& wire, bool binary,
                           std::array<asc::extent_t, Rank>& metadata,
                           std::array<std::byte, 64>& scratch,
                           asc::ArrayIoLimits& limits,
                           asc::ArrayIoReport& report) {
  for (std::size_t requests = 0; requests <= 2; ++requests) {
    Source bounded(wire.bytes());
    IoResource budget_resource;
    limits.max_allocations = requests;
    limits.max_staging_bytes = 0;
    limits.max_allocation_bytes = 0;
    {
      auto owner = binary ? asc::ReadSparseArrayBinary<Owner>(
                                bounded, budget_resource, metadata, scratch,
                                limits, report, true)
                          : asc::ReadSparseArrayText<Owner>(
                                bounded, budget_resource, metadata, scratch,
                                limits, report, true);
      ASC_SPARSE_TEST_EQ(test, owner.ok(), requests == 2);
      ASC_SPARSE_TEST_EQ(test, budget_resource.allocation_attempts(),
                         requests == 2 ? std::size_t{2} : std::size_t{0});
    }
    ASC_SPARSE_TEST_EQ(test, budget_resource.live_allocations(),
                       std::size_t{0});
  }
}

template <std::size_t Rank>
void CoordinateRanks(TestContext& test, bool empty) {
  using Shape = typename DynamicShape<std::make_index_sequence<Rank>>::Type;
  using Owner = asc::CoordinateArray<double, Shape>;
  std::array<asc::extent_t, Rank> shape{};
  shape.fill(2);
  const std::size_t nonempty_count = Rank == 0 ? 1 : 2;
  const std::size_t count = empty ? 0 : nonempty_count;
  std::array<asc::index_t, 2 * Rank> coordinates{};
  std::fill(coordinates.begin() + Rank, coordinates.end(), 1);
  std::array<double, 2> values{-0.0, 1};
  const auto view = Take(asc::CoordinateView<double, Rank>::Create(
      coordinates.data(), values.data(), shape, static_cast<asc::nnz_t>(count),
      kHost));
  for (bool binary : {false, true}) {
    std::array<std::byte, 64> scratch{};
    std::array<asc::extent_t, Rank> metadata{};
    asc::ArrayIoLimits limits;
    limits.max_token_bytes = 2;
    limits.max_logical_elements = std::numeric_limits<asc::extent_t>::max();
    limits.max_scratch_bytes = scratch.size() + sizeof(metadata);
    asc::ArrayIoReport report;
    Sink wire;
    ASC_SPARSE_TEST_CHECK(
        test,
        (binary
             ? asc::WriteSparseArrayBinary(view, wire, limits, scratch, report)
             : asc::WriteSparseArrayText(view, wire, limits, scratch, report))
            .ok());
    IoResource resource;
    Source source(wire.bytes());
    {
      auto owner =
          binary
              ? asc::ReadSparseArrayBinary<Owner>(source, resource, metadata,
                                                  scratch, limits, report, true)
              : asc::ReadSparseArrayText<Owner>(source, resource, metadata,
                                                scratch, limits, report, true);
      ASC_SPARSE_TEST_CHECK(test, owner.ok());
      if (!owner.ok()) {
        return;
      }
      ASC_SPARSE_TEST_CHECK(test, report.committed);
      const auto loaded = Take(owner->view());
      ASC_SPARSE_TEST_EQ(test, loaded.nnz(), static_cast<asc::nnz_t>(count));
      for (std::size_t i = 0; i < count; ++i) {
        ASC_SPARSE_TEST_CHECK(test, SameBits(loaded.values()[i], values[i]));
      }
      std::array<double, 2> staging{};
      Source into(wire.bytes());
      ASC_SPARSE_TEST_CHECK(
          test, (binary ? asc::ReadSparseArrayBinaryInto(
                              into, loaded, std::span(staging), metadata,
                              scratch, limits, report, true)
                        : asc::ReadSparseArrayTextInto(
                              into, loaded, std::span(staging), metadata,
                              scratch, limits, report, true))
                    .ok());
    }
    ASC_SPARSE_TEST_EQ(test, resource.live_allocations(), std::size_t{0});
    if (empty) {
      EmptyCoordinateBudget<Owner>(test, wire, binary, metadata, scratch,
                                   limits, report);
    }
    if (test.Finish() == 0) {
      std::cout << "coo_rank rank=" << Rank << " empty=" << empty
                << " binary=" << binary << '\n';
    }
  }
}

template <asc::SparseCompressedFormat Format>
void CompressedEmpty(TestContext& test) {
  using Owner = asc::CompressedSparseArray<double, Format>;
  const std::array<asc::extent_t, 2> shape{0, 0};
  const std::array<asc::nnz_t, 1> offsets{0};
  const auto view = Take(asc::CompressedSparseView<double, Format>::Create(
      offsets.data(), nullptr, nullptr, shape, 0, kHost));
  for (bool binary : {false, true}) {
    std::array<asc::extent_t, 2> metadata{};
    std::array<std::byte, 64> scratch{};
    asc::ArrayIoLimits limits;
    asc::ArrayIoReport report;
    Sink wire;
    ASC_SPARSE_TEST_CHECK(
        test,
        (binary
             ? asc::WriteSparseArrayBinary(view, wire, limits, scratch, report)
             : asc::WriteSparseArrayText(view, wire, limits, scratch, report))
            .ok());
    for (std::size_t requests = 0; requests <= 4; ++requests) {
      limits.max_allocations = requests;
      limits.max_staging_bytes = 16;
      limits.max_allocation_bytes = 16;
      IoResource resource;
      Source source(wire.bytes());
      {
        auto owner =
            binary
                ? asc::ReadSparseArrayBinary<Owner>(
                      source, resource, metadata, scratch, limits, report, true)
                : asc::ReadSparseArrayText<Owner>(source, resource, metadata,
                                                  scratch, limits, report,
                                                  true);
        ASC_SPARSE_TEST_EQ(test, owner.ok(), requests == 4);
        ASC_SPARSE_TEST_EQ(test, resource.allocation_attempts(),
                           requests == 4 ? std::size_t{4} : std::size_t{0});
      }
      ASC_SPARSE_TEST_EQ(test, resource.live_allocations(), std::size_t{0});
    }
  }
}

void AdjacentFrames(TestContext& test,
                    const std::array<std::byte, 100>& binary) {
  using Owner = asc::CoordinateArray<double, Shape2>;
  std::array<asc::extent_t, 2> metadata{};
  std::array<std::byte, 128> scratch{};
  asc::ArrayIoLimits limits;
  asc::ArrayIoReport report;
  IoResource resource;
  // Adjacent frame reading never consumes the next header. Moving transfers
  // the prepared payload cursor; consumed/moved cursors remain invalid.
  std::array<std::byte, 200> adjacent{};
  std::copy(binary.begin(), binary.end(), adjacent.begin());
  std::copy(binary.begin(), binary.end(), adjacent.begin() + binary.size());
  Source stream(adjacent);
  auto prepared = Take(asc::SparseArrayReader::PrepareBinary(
      stream, metadata, scratch, limits, report));
  auto moved = std::move(prepared);
  // ready() explicitly supports observing the invalid moved-from state.
  // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
  ASC_SPARSE_TEST_CHECK(test, !prepared.ready());
  auto first = asc::ReadSparseArray<double, Shape2>(moved, resource);
  ASC_SPARSE_TEST_CHECK(test, first.ok());
  ASC_SPARSE_TEST_EQ(test, stream.position(), binary.size());
  ASC_SPARSE_TEST_CHECK(
      test, (!asc::ReadSparseArray<double, Shape2>(moved, resource).ok()));
  auto second = asc::ReadSparseArrayBinary<Owner>(
      stream, resource, metadata, scratch, limits, report, true);
  ASC_SPARSE_TEST_CHECK(test, second.ok());
  ASC_SPARSE_TEST_EQ(test, stream.position(), adjacent.size());
}

void OwnerRejections(TestContext& test, std::string_view fixture) {
  using Owner = asc::CoordinateArray<double, Shape2>;
  std::array<asc::extent_t, 2> metadata{};
  std::array<std::byte, 128> scratch{};
  asc::ArrayIoLimits limits;
  asc::ArrayIoReport report;
  IoResource resource;
  // Header/static-shape/type/placement rejection precedes value allocation.
  for (auto placement : {asc::MemorySpace::kDevice, asc::MemorySpace::kManaged,
                         asc::MemorySpace::kPinnedHost}) {
    Source input(fixture);
    IoResource unavailable(placement);
    auto rejected = asc::ReadSparseArrayText<Owner>(
        input, unavailable, metadata, scratch, limits, report);
    ASC_SPARSE_TEST_CHECK(test, !rejected.ok());
    ASC_SPARSE_TEST_EQ(test, unavailable.allocation_attempts(), std::size_t{0});
  }
  Source static_source(fixture);
  IoResource static_resource;
  auto wrong_static = asc::ReadSparseArrayText<
      asc::CoordinateArray<double, asc::Extents<2, 1>>>(
      static_source, static_resource, metadata, scratch, limits, report);
  ASC_SPARSE_TEST_CHECK(test, !wrong_static.ok());
  ASC_SPARSE_TEST_EQ(test, static_resource.allocation_attempts(),
                     std::size_t{0});
}

void IntoBudgets(TestContext& test, const asc::CoordinateView<double, 2>& view,
                 std::span<const std::byte> binary, std::string_view fixture) {
  std::array<asc::extent_t, 2> metadata{};
  std::array<std::byte, 128> scratch{};
  asc::ArrayIoLimits limits;
  asc::ArrayIoReport report;
  // Resource caps do not silently expand to fit the frame; exact EOF budget
  // must include a spare byte for its bounded probe.
  std::array<double, 1> staging{};
  for (std::size_t cap = 0; cap <= binary.size(); ++cap) {
    limits.max_input_bytes = cap;
    Source input(binary);
    ASC_SPARSE_TEST_CHECK(test, !asc::ReadSparseArrayBinaryInto(
                                     input, view, std::span(staging), metadata,
                                     scratch, limits, report, true)
                                     .ok());
    ASC_SPARSE_TEST_CHECK(test, !report.committed);
    ASC_SPARSE_TEST_CHECK(test, report.input_bytes <= cap);
    ASC_SPARSE_TEST_EQ(test, view.values()[0], 2.0);
  }
  limits = {};
  for (const auto member :
       {&asc::ArrayIoLimits::max_header_bytes,
        &asc::ArrayIoLimits::max_token_bytes, &asc::ArrayIoLimits::max_rank,
        &asc::ArrayIoLimits::max_structure_bytes,
        &asc::ArrayIoLimits::max_decoded_bytes,
        &asc::ArrayIoLimits::max_staging_bytes,
        &asc::ArrayIoLimits::max_scratch_bytes}) {
    limits.*member = 0;
    Source input(fixture);
    ASC_SPARSE_TEST_CHECK(
        test, !asc::ReadSparseArrayTextInto(input, view, std::span(staging),
                                            metadata, scratch, limits, report)
                   .ok());
    ASC_SPARSE_TEST_CHECK(test, !report.committed);
    ASC_SPARSE_TEST_EQ(test, view.values()[0], 2.0);
    limits = {};
  }
}

void PathRoundTrips(TestContext& test,
                    const asc::CoordinateView<double, 2>& view) {
  using Owner = asc::CoordinateArray<double, Shape2>;
  std::array<asc::extent_t, 2> metadata{};
  std::array<std::byte, 128> scratch{};
  asc::ArrayIoLimits limits;
  asc::ArrayIoReport report;
  IoResource resource;
  // File tests use caller-provided external working directory, never source.
  const auto path = std::filesystem::current_path() / "sparse-io-roundtrip.asc";
  ASC_SPARSE_TEST_CHECK(
      test,
      asc::SaveSparseArrayText(path, view, asc::ArrayFileOverwrite::kTruncate,
                               limits, scratch, report)
          .ok());
  auto loaded = asc::LoadSparseArrayText<Owner>(path, resource, metadata,
                                                scratch, limits, report);
  ASC_SPARSE_TEST_CHECK(test, loaded.ok());
  ASC_SPARSE_TEST_CHECK(
      test,
      asc::SaveSparseArrayBinary(path, view, asc::ArrayFileOverwrite::kTruncate,
                                 limits, scratch, report)
          .ok());
  auto loaded_binary = asc::LoadSparseArrayBinary<Owner>(
      path, resource, metadata, scratch, limits, report);
  ASC_SPARSE_TEST_CHECK(test, loaded_binary.ok());
  ASC_SPARSE_TEST_CHECK(
      test, !asc::SaveSparseArrayText(
                 path, view,
                 std::bit_cast<asc::ArrayFileOverwrite>(std::uint8_t{255}),
                 limits, scratch, report)
                 .ok());
}

void IndependentFixtures(TestContext& test) {
  using Owner = asc::CoordinateArray<double, Shape2>;
  constexpr std::string_view kText =
      "ASCARRAY 1\nkind coo\nscalar f64\nrank 2\nshape 1 1\norder coo\ncount "
      "1\ncoordinates\n(0,0)\nvalues\n2\nend\n";
  // Independently encoded by tools/array_io/reference_codec.py, not C++
  // writers.
  constexpr std::string_view kHex =
      "415343415252420a01000000020a00000200000000000000010000000000000002000000"
      "000000001800000000000000480000000000000001000000000000000100000000000000"
      "00000000000000000000000000000000000000000000004014de9b3b";
  std::array<std::byte, 100> binary{};
  const auto digit = [](char value) {
    return value <= '9' ? value - '0' : value - 'a' + 10;
  };
  for (std::size_t i = 0; i < binary.size(); ++i) {
    binary[i] = static_cast<std::byte>(16 * digit(kHex[2 * i]) +
                                       digit(kHex[2 * i + 1]));
  }
  std::array<asc::extent_t, 2> metadata{};
  std::array<std::byte, 128> scratch{};
  asc::ArrayIoLimits limits;
  asc::ArrayIoReport report;
  IoResource resource;
  Source source(kText);
  auto owner = asc::ReadSparseArrayText<Owner>(source, resource, metadata,
                                               scratch, limits, report, true);
  ASC_SPARSE_TEST_CHECK(test, owner.ok());
  if (!owner.ok()) {
    return;
  }
  const auto view = Take(owner->view());
  ASC_SPARSE_TEST_EQ(test, view.values()[0], 2.0);
  Sink output;
  ASC_SPARSE_TEST_CHECK(
      test,
      asc::WriteSparseArrayBinary(view, output, limits, scratch, report).ok());
  ASC_SPARSE_TEST_CHECK(
      test, std::equal(binary.begin(), binary.end(), output.bytes().begin(),
                       output.bytes().end()));
  Source bytes(binary);
  auto decoded = asc::ReadSparseArrayBinary<Owner>(
      bytes, resource, metadata, scratch, limits, report, true);
  ASC_SPARSE_TEST_CHECK(test, decoded.ok());
  Sink text;
  ASC_SPARSE_TEST_CHECK(
      test,
      asc::WriteSparseArrayText(*decoded, text, limits, scratch, report).ok());
  ASC_SPARSE_TEST_EQ(test, text.text(), kText);
  constexpr std::string_view kCrlf =
      "ASCARRAY 1\r\nkind coo\r\nscalar f64\r\nrank 2\r\nshape 1 1\r\n"
      "order coo\r\ncount 1\r\ncoordinates\r\n(0,0)\r\nvalues\r\n2\r\nend\r\n";
  Source crlf(kCrlf);
  auto crlf_owner = asc::ReadSparseArrayText<Owner>(
      crlf, resource, metadata, scratch, limits, report, true);
  ASC_SPARSE_TEST_CHECK(test, crlf_owner.ok());
  AdjacentFrames(test, binary);
  OwnerRejections(test, kText);
  IntoBudgets(test, view, binary, kText);
  PathRoundTrips(test, view);
}

asc::ArrayIoLimits OverflowLimits() {
  asc::ArrayIoLimits limits;
  constexpr auto kMaximum =
      static_cast<std::uint64_t>(std::numeric_limits<asc::extent_t>::max());
  limits.max_extent = kMaximum;
  limits.max_logical_elements = kMaximum;
  limits.max_stored_elements = kMaximum;
  limits.max_structure_bytes = std::numeric_limits<std::size_t>::max();
  limits.max_decoded_bytes = std::numeric_limits<std::size_t>::max();
  limits.max_staging_bytes = std::numeric_limits<std::size_t>::max();
  limits.max_allocation_bytes = std::numeric_limits<std::size_t>::max();
  limits.max_input_bytes = std::numeric_limits<std::size_t>::max();
  return limits;
}

void OverflowTextHeaders(TestContext& test) {
  // Each tiny header reaches a different checked arithmetic rejection; no
  // enormous allocation or invalid span is needed to exercise these paths.
  constexpr std::array<std::string_view, 6> kHeaders{
      "ASCARRAY 1\nkind coo\nscalar f64\nrank 3\nshape 2147483647 2147483647 "
      "2147483647\norder coo\ncount 0\n",
      "ASCARRAY 1\nkind csr\nscalar f64\nrank 2\nshape 9223372036854775807 "
      "0\norder csr\ncount 0\n",
      "ASCARRAY 1\nkind coo\nscalar f64\nrank 3\nshape 9223372036854775807 "
      "1 1\norder coo\ncount 9223372036854775807\n",
      "ASCARRAY 1\nkind coo\nscalar f64\nrank 1\nshape 9223372036854775807\n"
      "order coo\ncount 9223372036854775807\n",
      "ASCARRAY 1\nkind coo\nscalar c128\nrank 1\nshape 1152921504606846976\n"
      "order coo\ncount 1152921504606846976\n",
      "ASCARRAY 1\nkind coo\nscalar c128\nrank 1\nshape 768614336404564651\n"
      "order coo\ncount 768614336404564651\n"};
  std::size_t checked = 0;
  for (const auto header : kHeaders) {
    Source source(header);
    std::array<asc::extent_t, 3> metadata{};
    std::array<std::byte, 128> scratch{};
    asc::ArrayIoReport report;
    const auto limits = OverflowLimits();
    asc::ErrorCode code;
    std::size_t allocations = 0;
    {
      asc_sparse_test::AllocationProbe probe;
      const auto reader = asc::SparseArrayReader::PrepareText(
          source, metadata, scratch, limits, report);
      code = reader.status().code();
      allocations = probe.count();
    }
    ASC_SPARSE_TEST_CHECK(test, code == asc::ErrorCode::kOverflow);
    ASC_SPARSE_TEST_CHECK(
        test, asc_test::ProcessAllocationCountMatches(allocations, 0));
    ASC_SPARSE_TEST_EQ(test, report.input_bytes, source.position());
    ASC_SPARSE_TEST_CHECK(test, !report.committed);
    ++checked;
  }
  std::cout << "SPARSE_IO_OVERFLOW text_headers=" << checked << '\n';
}

void OverflowStaging(TestContext& test) {
  Source source(
      "ASCARRAY 1\nkind coo\nscalar f64\nrank 1\nshape "
      "576460752303423488\norder coo\ncount 576460752303423488\n");
  std::array<asc::extent_t, 1> metadata{};
  std::array<std::byte, 128> scratch{};
  asc::ArrayIoReport report;
  const auto limits = OverflowLimits();
  auto reader = asc::SparseArrayReader::PrepareText(source, metadata, scratch,
                                                    limits, report);
  ASC_SPARSE_TEST_CHECK(test, reader.ok());
  if (!reader.ok()) {
    return;
  }
  IoResource resource;
  asc::ErrorCode code;
  std::size_t allocations = 0;
  {
    asc_sparse_test::AllocationProbe probe;
    const auto owner =
        asc::ReadSparseArray<double, asc::Extents<asc::kDynamicExtent>>(
            *reader, resource);
    code = owner.status().code();
    allocations = probe.count();
  }
  ASC_SPARSE_TEST_CHECK(test, code == asc::ErrorCode::kOverflow);
  ASC_SPARSE_TEST_CHECK(
      test, asc_test::ProcessAllocationCountMatches(allocations, 0));
  ASC_SPARSE_TEST_EQ(test, resource.allocation_attempts(), std::size_t{0});
  ASC_SPARSE_TEST_CHECK(test, !report.committed);
  std::cout << "SPARSE_IO_OVERFLOW staging_peak=1\n";
}

void OverflowBinaryHeader(TestContext& test) {
  std::array<std::byte, 64> frame{};
  constexpr std::string_view kMagic = "ASCARRB\n";
  std::copy(kMagic.begin(), kMagic.end(),
            reinterpret_cast<char*>(frame.data()));
  const auto encode = [&](auto value, std::size_t offset) {
    ASC_SPARSE_TEST_CHECK(
        test, asc::EncodeLittleEndian(
                  value, std::span(frame).subspan(offset, sizeof(value)))
                  .ok());
  };
  encode(std::uint16_t{1}, 8);
  encode(std::uint8_t{2}, 12);
  encode(std::uint8_t{10}, 13);
  encode(std::uint32_t{1}, 16);
  encode(std::uint64_t{1152921504606846975}, 24);
  encode(std::uint64_t{1152921504606846975}, 32);
  encode(std::uint64_t{18446744073709551600ULL}, 40);
  encode(std::uint64_t{64}, 48);
  encode(std::uint64_t{1152921504606846975}, 56);
  Source source(frame);
  std::array<asc::extent_t, 1> metadata{};
  std::array<std::byte, 128> scratch{};
  asc::ArrayIoReport report;
  const auto limits = OverflowLimits();
  asc::ErrorCode code;
  std::size_t allocations = 0;
  {
    asc_sparse_test::AllocationProbe probe;
    const auto reader = asc::SparseArrayReader::PrepareBinary(
        source, metadata, scratch, limits, report);
    code = reader.status().code();
    allocations = probe.count();
  }
  ASC_SPARSE_TEST_CHECK(test, code == asc::ErrorCode::kOverflow);
  ASC_SPARSE_TEST_CHECK(
      test, asc_test::ProcessAllocationCountMatches(allocations, 0));
  ASC_SPARSE_TEST_EQ(test, report.input_bytes, source.position());
  ASC_SPARSE_TEST_CHECK(test, !report.committed);
  std::cout << "SPARSE_IO_OVERFLOW binary_frames=1\n";
}

void LargeEmptyShapes(TestContext& test) {
  using Owner = asc::CoordinateArray<
      double, asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent,
                           asc::kDynamicExtent>>;
  constexpr std::array<std::string_view, 3> kShapes{"0 4294967296 4294967296",
                                                    "4294967296 0 4294967296",
                                                    "4294967296 4294967296 0"};
  for (const auto shape : kShapes) {
    const std::string frame =
        "ASCARRAY 1\nkind coo\nscalar f64\nrank 3\nshape " +
        std::string(shape) + "\norder coo\ncount 0\ncoordinates\nvalues\nend\n";
    Source source(frame);
    std::array<asc::extent_t, 3> metadata{};
    std::array<std::byte, 128> scratch{};
    asc::ArrayIoReport report;
    auto limits = OverflowLimits();
    limits.max_allocations = 2;
    limits.max_allocation_bytes = 0;
    limits.max_staging_bytes = 0;
    IoResource resource;
    {
      asc_sparse_test::AllocationProbe probe;
      const auto owner = asc::ReadSparseArrayText<Owner>(
          source, resource, metadata, scratch, limits, report, true);
      ASC_SPARSE_TEST_CHECK(test, owner.ok());
      ASC_SPARSE_TEST_CHECK(
          test, asc_test::ProcessAllocationCountMatches(probe.count(), 0));
    }
    ASC_SPARSE_TEST_EQ(test, resource.allocation_attempts(), std::size_t{2});
    ASC_SPARSE_TEST_EQ(test, resource.live_allocations(), std::size_t{0});
    ASC_SPARSE_TEST_CHECK(test, report.committed);
  }
  std::cout << "SPARSE_IO_LARGE_EMPTY zero_positions=" << kShapes.size()
            << '\n';
}

void MalformedText(TestContext& test) {
  constexpr std::array<std::string_view, 14> kBad{
      "ASCARRAY 1\nkind coo\nscalar f64\nrank 2\nshape 2 2\norder coo\ncount "
      "2\ncoordinates\n(0,0)\n(0,0)\nvalues\n1\n2\nend\n",
      "ASCARRAY 1\nkind coo\nscalar f64\nrank 2\nshape 2 2\norder coo\ncount "
      "2\ncoordinates\n(1,0)\n(0,1)\nvalues\n1\n2\nend\n",
      "ASCARRAY 1\nkind coo\nscalar f64\nrank 2\nshape 2 2\norder coo\ncount "
      "1\ncoordinates\n(2,0)\nvalues\n1\nend\n",
      "ASCARRAY 1\nkind coo\nscalar f64\nrank 2\nshape 2 2\norder coo\ncount "
      "1\ncoordinates\n(-1,0)\nvalues\n1\nend\n",
      "ASCARRAY 1\nkind csr\nscalar f64\nrank 2\nshape 2 2\norder csr\ncount "
      "2\noffsets\n0\n2\n2\nindices\n0\n0\nvalues\n1\n2\nend\n",
      "ASCARRAY 1\nkind csr\nscalar f64\nrank 2\nshape 2 2\norder csr\ncount "
      "2\noffsets\n1\n2\n2\nindices\n0\n1\nvalues\n1\n2\nend\n",
      "ASCARRAY 1\nkind csr\nscalar f64\nrank 2\nshape 2 2\norder csr\ncount "
      "2\noffsets\n0\n2\n1\nindices\n0\n1\nvalues\n1\n2\nend\n",
      "ASCARRAY 1\nkind csc\nscalar f64\nrank 2\nshape 2 2\norder csc\ncount "
      "2\noffsets\n0\n2\n2\nindices\n1\n0\nvalues\n1\n2\nend\n",
      "ASCARRAY 1\nkind coo\nscalar f64\nrank 2\nshape 0 2\norder coo\ncount "
      "1\ncoordinates\n(0,0)\nvalues\n1\nend\n",
      "ASCARRAY 2\nkind coo\nscalar f64\nrank 0\nshape\norder coo\ncount "
      "0\ncoordinates\nvalues\nend\n",
      "ASCARRAY 1\nkind coo\nscalar f64\nrank 0\nshape\norder coo\ncount "
      "2\ncoordinates\n()\n()\nvalues\n1\n2\nend\n",
      "ASCARRAY 1\nkind coo\nscalar f64\nrank 2\nshape 1 1\norder coo\ncount "
      "1\ncoordinates\n(0,0)\nvalues\n1e999\nend\n",
      "ASCARRAY 1\nkind coo\nscalar f64\nrank 2\nshape 1 1\norder coo\ncount "
      "1\ncoordinates\n(0,0)\nvalues\n1\nend\nextra",
      "ASCARRAY 1\nkind coo\nscalar f64\nrank 2\nshape  1 1\norder coo\ncount "
      "1\ncoordinates\n(0,0)\nvalues\n1\nend\n"};
  for (auto text : kBad) {
    Source source(text);
    std::array<asc::extent_t, 2> metadata{};
    std::array<std::byte, 128> scratch{};
    asc::ArrayIoLimits limits;
    asc::ArrayIoReport report;
    IoResource resource;
    {
      auto reader = asc::SparseArrayReader::PrepareText(
          source, metadata, scratch, limits, report, true);
      if (reader.ok()) {
        if (reader->kind() == asc::SparseArrayKind::kCoo) {
          ASC_SPARSE_TEST_CHECK(
              test,
              (!asc::ReadSparseArray<double, Shape2>(*reader, resource).ok()));
        } else if (reader->kind() == asc::SparseArrayKind::kCsr) {
          ASC_SPARSE_TEST_CHECK(
              test,
              (!asc::ReadSparseArray<double, asc::SparseCompressedFormat::kCsr>(
                    *reader, resource)
                    .ok()));
        } else {
          ASC_SPARSE_TEST_CHECK(
              test,
              (!asc::ReadSparseArray<double, asc::SparseCompressedFormat::kCsc>(
                    *reader, resource)
                    .ok()));
        }
      }
    }
    ASC_SPARSE_TEST_CHECK(test, !report.committed);
    ASC_SPARSE_TEST_EQ(test, resource.live_allocations(), std::size_t{0});
  }
}

template <typename Factory>
void FactoryFailure(TestContext& test, std::size_t failure, Factory factory) {
  IoResource resource;
  resource.FailWithLongError(failure);
  {
    asc_sparse_test::AllocationProbe probe;
    auto owner = factory(resource);
    ASC_SPARSE_TEST_CHECK(test, !owner.ok());
    ASC_SPARSE_TEST_EQ(test, owner.status().message().size(), std::size_t{256});
    ASC_SPARSE_TEST_EQ(test, owner.status().provider().size(),
                       std::size_t{256});
    ASC_SPARSE_TEST_EQ(test, owner.status().native_code(), std::int64_t{91});
    ASC_SPARSE_TEST_CHECK(
        test,
        asc_test::ProcessAllocationCountMatches(
            probe.count(),
            asc_test::ProcessVisibleResourceAllocationCount(failure - 1)));
  }
  ASC_SPARSE_TEST_EQ(test, resource.allocation_attempts(), failure);
  ASC_SPARSE_TEST_EQ(test, resource.live_allocations(), std::size_t{0});
}

template <typename T>
void FactoryDiagnostics(TestContext& test) {
  const auto shape = Take(Shape2::Create(2, 3));
  const std::array<asc::extent_t, 2> extents{2, 3};
  const std::array<asc::index_t, 4> coordinates{0, 0, 1, 2};
  std::array<T, 2> values{T(1, -2), T(3, 4)};
  const std::array<asc::nnz_t, 3> offsets{0, 1, 2};
  const std::array<asc::index_t, 2> indices{0, 2};
  const auto context = asc::ExecutionContext::Serial();
  const auto coo = Take(asc::CoordinateView<T, 2>::Create(
      coordinates.data(), values.data(), extents, 2, kHost));
  const auto csr = Take(asc::CsrView<T>::Create(
      offsets.data(), indices.data(), values.data(), extents, 2, kHost));
  for (std::size_t failure = 1; failure <= 2; ++failure) {
    FactoryFailure(test, failure, [&](IoResource& resource) {
      return asc::CoordinateArray<T, Shape2>::Create(resource, shape,
                                                     coordinates, values);
    });
    FactoryFailure(test, failure, [&](IoResource& resource) {
      return asc::CoordinateBuilder<T, Shape2>::Create(resource, shape, 4);
    });
    FactoryFailure(test, failure, [&](IoResource& resource) {
      return asc::ConvertToCoordinate(context, csr, resource);
    });
  }
  for (std::size_t failure = 1; failure <= 3; ++failure) {
    FactoryFailure(test, failure, [&](IoResource& resource) {
      return asc::CsrArray<T>::Create(resource, extents, offsets, indices,
                                      values);
    });
    FactoryFailure(test, failure, [&](IoResource& resource) {
      return asc::CsrArray<T>::FromCoordinate(context, coo, resource);
    });
    FactoryFailure(test, failure, [&](IoResource& resource) {
      return asc::CscArray<T>::FromCompressed(context, csr, resource);
    });
  }
}

template <typename T>
void ComplexLifecycle(TestContext& test) {
  const auto shape = Take(Shape2::Create(2, 3));
  const std::array<asc::index_t, 4> coordinates{0, 0, 1, 2};
  const std::array<T, 2> values{T(1, -2), T(0, -0.0)};
  const auto context = asc::ExecutionContext::Serial();
  IoResource resource;
  {
    auto owner = Take(asc::CoordinateArray<T, Shape2>::Create(
        resource, shape, coordinates, values));
    auto view = Take(owner.view());
    auto copied = Take(asc::CoordinateArray<T, Shape2>::Create(
        resource, shape, coordinates, std::span(view.values(), 2)));
    ASC_SPARSE_TEST_CHECK(test,
                          SameBits(Take(copied.view()).values()[0], values[0]));
    auto builder =
        Take(asc::CoordinateBuilder<T, Shape2>::Create(resource, shape, 4));
    ASC_SPARSE_TEST_CHECK(
        test, builder
                  .Add(std::span<const asc::index_t, 2>(coordinates.data(), 2),
                       values[0])
                  .ok());
    ASC_SPARSE_TEST_CHECK(test, builder
                                    .Add(std::span<const asc::index_t, 2>(
                                             coordinates.data() + 2, 2),
                                         values[1])
                                    .ok());
    auto finalized =
        Take(std::move(builder).Finalize(context, asc::DuplicatePolicy::kReject,
                                         asc::ExplicitZeroPolicy::kKeep));
    const auto final_view = Take(finalized.view());
    ASC_SPARSE_TEST_CHECK(test, SameBits(final_view.values()[1], values[1]));
    auto csr =
        Take(asc::CsrArray<T>::FromCoordinate(context, final_view, resource));
    auto csr_view = Take(csr.view());
    auto csc =
        Take(asc::CscArray<T>::FromCompressed(context, csr_view, resource));
    ASC_SPARSE_TEST_CHECK(test,
                          SameBits(Take(csc.view()).values()[0], values[0]));
    auto converted_coo =
        Take(asc::ConvertToCoordinate(context, Take(csc.view()), resource));
    ASC_SPARSE_TEST_CHECK(
        test, SameBits(Take(converted_coo.view()).values()[1], values[1]));
    for (std::size_t fail = 1; fail <= 3; ++fail) {
      IoResource failed;
      failed.FailOnAllocation(fail);
      ASC_SPARSE_TEST_CHECK(
          test,
          !asc::CscArray<T>::FromCoordinate(context, final_view, failed).ok());
      ASC_SPARSE_TEST_EQ(test, failed.live_allocations(), std::size_t{0});
      IoResource failed_conversion;
      failed_conversion.FailOnAllocation(fail);
      ASC_SPARSE_TEST_CHECK(test, !asc::CscArray<T>::FromCompressed(
                                       context, csr_view, failed_conversion)
                                       .ok());
      ASC_SPARSE_TEST_EQ(test, failed_conversion.live_allocations(),
                         std::size_t{0});
    }
  }
  ASC_SPARSE_TEST_EQ(test, resource.live_allocations(), std::size_t{0});
  for (std::size_t fail = 1; fail <= 2; ++fail) {
    IoResource failed;
    failed.FailOnAllocation(fail);
    ASC_SPARSE_TEST_CHECK(test, (!asc::CoordinateArray<T, Shape2>::Create(
                                      failed, shape, coordinates, values)
                                      .ok()));
    ASC_SPARSE_TEST_EQ(test, failed.live_allocations(), std::size_t{0});
    IoResource failed_builder;
    failed_builder.FailOnAllocation(fail);
    ASC_SPARSE_TEST_CHECK(test, (!asc::CoordinateBuilder<T, Shape2>::Create(
                                      failed_builder, shape, 4)
                                      .ok()));
    ASC_SPARSE_TEST_EQ(test, failed_builder.live_allocations(), std::size_t{0});
  }
}

}  // namespace

int main() {
  TestContext test;
  AllKinds<std::int8_t>(test);
  AllKinds<std::uint8_t>(test);
  AllKinds<std::int16_t>(test);
  AllKinds<std::uint16_t>(test);
  AllKinds<std::int32_t>(test);
  AllKinds<std::uint32_t>(test);
  AllKinds<std::int64_t>(test);
  AllKinds<std::uint64_t>(test);
  AllKinds<float>(test);
  AllKinds<double>(test);
  AllKinds<std::complex<float>>(test);
  AllKinds<std::complex<double>>(test);
  Failures(test);
  for (bool empty : {false, true}) {
    CoordinateRanks<0>(test, empty);
    CoordinateRanks<1>(test, empty);
    CoordinateRanks<3>(test, empty);
    CoordinateRanks<32>(test, empty);
  }
  CompressedEmpty<asc::SparseCompressedFormat::kCsr>(test);
  CompressedEmpty<asc::SparseCompressedFormat::kCsc>(test);
  IndependentFixtures(test);
  OverflowTextHeaders(test);
  OverflowStaging(test);
  OverflowBinaryHeader(test);
  LargeEmptyShapes(test);
  MalformedText(test);
  ComplexLifecycle<std::complex<float>>(test);
  ComplexLifecycle<std::complex<double>>(test);
  FactoryDiagnostics<std::complex<float>>(test);
  FactoryDiagnostics<std::complex<double>>(test);
  {
    asc_locale_test::LocaleGuard locale;
    ASC_SPARSE_TEST_CHECK(test, asc_locale_test::LocaleControl());
    AllKinds<float>(test);
    AllKinds<double>(test);
    AllKinds<std::complex<float>>(test);
    AllKinds<std::complex<double>>(test);
    IndependentFixtures(test);
    MalformedText(test);
  }
  return test.Finish();
}
