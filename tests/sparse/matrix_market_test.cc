#include "asc/sparse/matrix_market.h"

#include <algorithm>
#include <array>
#include <complex>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>

#include "allocation_observation.h"
#include "allocation_probe.h"
#include "asc/core/array_format.h"
#include "asc/core/array_io.h"
#include "asc/core/extents.h"
#include "asc/core/io.h"
#include "asc/core/matrix_market.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/sparse/compressed.h"
#include "asc/sparse/coordinate.h"
#include "test_support.h"

namespace {

using asc_sparse_test::TestContext;
using Shape = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;
using asc::MatrixMarketSymmetry;

template <typename View>
concept MutableRead = requires(
    asc::SparseMatrixMarketReader& reader, const View& view,
    asc::SparseMatrixMarketWorkspace<typename View::value_type> workspace) {
  asc::ReadSparseMatrixMarketInto(reader, view, workspace);
};
static_assert(MutableRead<asc::CoordinateView<double, 2>>);
static_assert(!MutableRead<asc::CoordinateView<const double, 2>>);
static_assert(!MutableRead<asc::CompressedSparseView<
                  const double, asc::SparseCompressedFormat::kCsr>>);
static_assert(!MutableRead<asc::CompressedSparseView<
                  const double, asc::SparseCompressedFormat::kCsc>>);
static_assert(!asc::internal_sparse_matrix_market::MatrixOwner<
              asc::CoordinateArray<double, asc::Extents<asc::kDynamicExtent>>>);

template <typename T>
T Take(asc::Result<T> result) {
  if (!result.ok()) {
    std::abort();
  }
  return std::move(*result);
}

class Source final : public asc::ByteSource {
 public:
  explicit Source(std::string_view input) : input_(input) {}
  asc::Result<std::size_t> ReadSome(std::span<std::byte> output) override {
    if (position == fail_after) {
      return std::move(failure);
    }
    const auto count = std::min({output.size(), input_.size() - position,
                                 fail_after - position, std::size_t{3}});
    for (std::size_t i = 0; i < count; ++i) {
      output[i] = static_cast<std::byte>(input_[position + i]);
    }
    position += count;
    return count;
  }
  std::size_t position = 0;
  std::size_t fail_after = std::numeric_limits<std::size_t>::max();
  asc::Status failure{asc::ErrorCode::kIo};

 private:
  std::string_view input_;
};

class Sink final : public asc::ByteSink {
 public:
  asc::Result<std::size_t> WriteSome(
      std::span<const std::byte> input) override {
    if (size == fail_after) {
      return std::move(failure);
    }
    const auto count = std::min(
        {input.size(), bytes.size() - size, fail_after - size, std::size_t{2}});
    std::copy_n(input.begin(), count, bytes.begin() + size);
    size += count;
    return count;
  }
  [[nodiscard]] std::string_view text() const {
    return {reinterpret_cast<const char*>(bytes.data()), size};
  }
  std::array<std::byte, 8192> bytes{};
  std::size_t size = 0;
  std::size_t fail_after = std::numeric_limits<std::size_t>::max();
  asc::Status failure{asc::ErrorCode::kIo};
};

class Resource final : public asc::MemoryResource {
 public:
  [[nodiscard]] asc::MemorySpace space() const noexcept override {
    return placement;
  }
  asc::Result<void*> Allocate(std::size_t bytes,
                              std::size_t alignment) override {
    if (++attempts == fail_on) {
      return std::move(failure);
    }
    auto result = host_.Allocate(bytes, alignment);
    if (result.ok() && bytes != 0) {
      ++live;
      ++nonempty_allocations;
    }
    return result;
  }
  void Deallocate(void* pointer, std::size_t bytes,
                  std::size_t alignment) noexcept override {
    host_.Deallocate(pointer, bytes, alignment);
    if (bytes != 0) {
      --live;
    }
  }
  std::size_t attempts = 0;
  std::size_t live = 0;
  std::size_t nonempty_allocations = 0;
  std::size_t fail_on = std::numeric_limits<std::size_t>::max();
  asc::Status failure{asc::ErrorCode::kAllocation};
  asc::MemorySpace placement = asc::MemorySpace::kHost;

 private:
  asc::HostMemoryResource host_;
};

template <typename T, int Kind>
using Owner =
    std::conditional_t<Kind == 0, asc::CoordinateArray<T, Shape>,
                       asc::CompressedSparseArray<
                           T, Kind == 1 ? asc::SparseCompressedFormat::kCsr
                                        : asc::SparseCompressedFormat::kCsc>>;

template <typename OwnerType>
asc::Result<OwnerType> Read(std::string_view text, Resource& resource,
                            asc::SparseMatrixMarketReport& report,
                            asc::SparseMatrixMarketReadOptions options = {},
                            asc::ArrayIoLimits limits = {}) {
  Source source(text);
  std::array<std::byte, 512> scratch{};
  return asc::ReadSparseMatrixMarket<OwnerType>(source, resource, scratch,
                                                limits, options, report);
}

template <typename View>
void CheckEntries(TestContext& test, const View& view,
                  std::span<const asc::index_t> coordinates,
                  std::span<const typename View::value_type> values) {
  using Traits = asc::internal_sparse_matrix_market::ViewTraits<View>;
  ASC_SPARSE_TEST_EQ(test, static_cast<std::size_t>(view.nnz()), values.size());
  for (std::size_t i = 0; i < values.size(); ++i) {
    const auto found = asc::internal_sparse_matrix_market::Find(
        view, {coordinates[2 * i], coordinates[2 * i + 1]});
    ASC_SPARSE_TEST_CHECK(test, found < static_cast<std::size_t>(view.nnz()));
    if (found < static_cast<std::size_t>(view.nnz())) {
      ASC_SPARSE_TEST_EQ(test, view.values()[found], values[i]);
      ASC_SPARSE_TEST_EQ(
          test, Traits::Coordinate(view, found),
          (std::array{coordinates[2 * i], coordinates[2 * i + 1]}));
    }
  }
}

std::size_t executed = 0;

template <typename T, int Kind>
void General(TestContext& test) {
  Resource resource;
  asc::SparseMatrixMarketReport report;
  asc_sparse_test::AllocationProbe probe;
  auto owner = Read<Owner<T, Kind>>(
      "%%MatrixMarket matrix coordinate integer general\n"
      "% unsorted independent rectangular records\r\n3 4 3\n"
      "3 4 5\n1 2 0\n2 1 7\n% trailing comment\n",
      resource, report);
  ASC_SPARSE_TEST_CHECK(test, owner.ok());
  if (!owner.ok()) {
    return;
  }
  auto view = Take(owner->view());
  constexpr std::array<asc::index_t, 6> kCoordinates{0, 1, 1, 0, 2, 3};
  const std::array<T, 3> values{T{0}, T{7}, T{5}};
  CheckEntries(test, view, kCoordinates, values);
  ASC_SPARSE_TEST_EQ(test, report.source_records, 3U);
  ASC_SPARSE_TEST_EQ(test, report.assembled_entries, 3U);
  ASC_SPARSE_TEST_CHECK(test, report.io.committed);
  Sink sink;
  std::array<std::byte, 512> scratch{};
  auto status =
      asc::WriteSparseMatrixMarket(view, sink, {}, {}, scratch, report);
  ASC_SPARSE_TEST_CHECK(test, status.ok());
  auto roundtrip = Read<Owner<T, Kind>>(sink.text(), resource, report);
  ASC_SPARSE_TEST_CHECK(test, roundtrip.ok());
  if (roundtrip.ok()) {
    CheckEntries(test, Take(roundtrip->view()), kCoordinates, values);
  }
  ASC_SPARSE_TEST_CHECK(
      test, asc_test::ProcessAllocationCountMatches(
                probe.count(), asc_test::ProcessVisibleResourceAllocationCount(
                                   resource.nonempty_allocations)));
  ++executed;
  std::cout << "sparse_matrix_market general kind=" << Kind
            << " scalar=" << asc::internal_array_format::ScalarName<T>()
            << '\n';
}

template <typename T, int Kind>
void Structured(TestContext& test) {
  Resource resource;
  asc::SparseMatrixMarketReport report;
  constexpr std::array<asc::index_t, 8> kCoordinates{0, 0, 0, 1, 1, 0, 1, 1};
  for (const auto symmetry :
       {MatrixMarketSymmetry::kSymmetric, MatrixMarketSymmetry::kSkewSymmetric,
        MatrixMarketSymmetry::kHermitian}) {
    if (symmetry == MatrixMarketSymmetry::kHermitian &&
        !asc::internal_array_format::kComplex<T>) {
      continue;
    }
    if (symmetry == MatrixMarketSymmetry::kSkewSymmetric &&
        std::is_unsigned_v<T>) {
      continue;
    }
    std::string text = "%%MatrixMarket matrix coordinate ";
    const bool complex = asc::internal_array_format::kComplex<T>;
    const char* const real_field = std::integral<T> ? "integer " : "real ";
    text += complex ? "complex " : real_field;
    text += asc::internal_matrix_market::SymmetryName(symmetry);
    const bool skew = symmetry == MatrixMarketSymmetry::kSkewSymmetric;
    text += skew ? "\n2 2 1\n" : "\n2 2 3\n";
    if (!skew) {
      text += complex ? "1 1 4 0\n" : "1 1 4\n";
    }
    text += complex ? "2 1 2 2\n" : "2 1 2\n";
    if (!skew) {
      text += complex ? "2 2 11 0\n" : "2 2 11\n";
    }
    const auto initial_allocations = resource.nonempty_allocations;
    asc_sparse_test::AllocationProbe probe;
    auto owner = Read<Owner<T, Kind>>(text, resource, report);
    ASC_SPARSE_TEST_CHECK(test, owner.ok());
    if (!owner.ok()) {
      continue;
    }
    T lower{2};
    T upper{2};
    if constexpr (asc::internal_array_format::kComplex<T>) {
      lower = T{2, 2};
      upper = skew ? T{-2, -2} : T{2, -2};
      if (symmetry == MatrixMarketSymmetry::kSymmetric) {
        upper = T{2, 2};
      }
    } else if constexpr (!std::is_unsigned_v<T>) {
      upper = skew ? T{-2} : T{2};
    }
    const std::array<T, 4> values{T{4}, upper, lower, T{11}};
    auto view = Take(owner->view());
    CheckEntries(
        test, view,
        skew ? std::span(kCoordinates).subspan(2, 4) : std::span(kCoordinates),
        skew ? std::span(values).subspan(1, 2) : std::span(values));
    Sink sink;
    std::array<std::byte, 512> scratch{};
    asc::SparseMatrixMarketWriteOptions write;
    write.symmetry = symmetry;
    auto status =
        asc::WriteSparseMatrixMarket(view, sink, {}, write, scratch, report);
    ASC_SPARSE_TEST_CHECK(test, status.ok());
    auto roundtrip = Read<Owner<T, Kind>>(sink.text(), resource, report);
    ASC_SPARSE_TEST_CHECK(test, roundtrip.ok());
    if (roundtrip.ok()) {
      CheckEntries(test, Take(roundtrip->view()),
                   skew ? std::span(kCoordinates).subspan(2, 4)
                        : std::span(kCoordinates),
                   skew ? std::span(values).subspan(1, 2) : std::span(values));
    }
    ASC_SPARSE_TEST_CHECK(
        test, asc_test::ProcessAllocationCountMatches(
                  probe.count(),
                  asc_test::ProcessVisibleResourceAllocationCount(
                      resource.nonempty_allocations - initial_allocations)));
    ++executed;
    std::cout << "sparse_matrix_market structured kind=" << Kind
              << " scalar=" << asc::internal_array_format::ScalarName<T>()
              << " symmetry="
              << asc::internal_matrix_market::SymmetryName(symmetry) << '\n';
  }
}

template <typename T>
void Scalar(TestContext& test) {
  General<T, 0>(test);
  General<T, 1>(test);
  General<T, 2>(test);
  Structured<T, 0>(test);
  Structured<T, 1>(test);
  Structured<T, 2>(test);
}

template <typename T, int Kind>
void Pattern(TestContext& test) {
  for (const bool symmetric : {false, true}) {
    Resource resource;
    asc::SparseMatrixMarketReport report;
    asc::SparseMatrixMarketReadOptions read;
    read.pattern_policy = asc::MatrixMarketPatternPolicy::kUnit;
    const char* const text =
        symmetric
            ? "%%MatrixMarket matrix coordinate pattern symmetric\n2 2 1\n2 1\n"
            : "%%MatrixMarket matrix coordinate pattern general\n2 3 1\n2 1\n";
    auto owner = Read<Owner<T, Kind>>(text, resource, report, read);
    ASC_SPARSE_TEST_CHECK(test, owner.ok());
    if (!owner.ok()) {
      continue;
    }
    auto view = Take(owner->view());
    ASC_SPARSE_TEST_EQ(test, view.nnz(), symmetric ? 2 : 1);
    for (asc::nnz_t i = 0; i < view.nnz(); ++i) {
      ASC_SPARSE_TEST_EQ(test, view.values()[i], T{1});
    }
    Sink sink;
    std::array<std::byte, 512> scratch{};
    asc::SparseMatrixMarketWriteOptions write;
    write.pattern_policy = asc::MatrixMarketPatternPolicy::kUnit;
    write.symmetry = symmetric ? MatrixMarketSymmetry::kSymmetric
                               : MatrixMarketSymmetry::kGeneral;
    ASC_SPARSE_TEST_CHECK(test, asc::WriteSparseMatrixMarket(
                                    view, sink, {}, write, scratch, report)
                                    .ok());
    ASC_SPARSE_TEST_EQ(test, sink.text(), text);
    ++executed;
  }
}

template <typename T>
void Patterns(TestContext& test) {
  Pattern<T, 0>(test);
  Pattern<T, 1>(test);
  Pattern<T, 2>(test);
}

void Policies(TestContext& test) {
  Resource resource;
  asc::SparseMatrixMarketReport report;
  constexpr std::string_view kDuplicates =
      "%%MatrixMarket matrix coordinate real symmetric\n2 2 3\n"
      "2 1 9007199254740992\n2 1 1\n2 1 -9007199254740992\n";
  asc::SparseMatrixMarketReadOptions options;
  auto rejected = Read<Owner<double, 0>>(kDuplicates, resource, report);
  ASC_SPARSE_TEST_CHECK(test, !rejected.ok());
  options.duplicate_policy = asc::DuplicatePolicy::kSum;
  auto summed = Read<Owner<double, 0>>(kDuplicates, resource, report, options);
  ASC_SPARSE_TEST_CHECK(test, summed.ok());
  if (summed.ok()) {
    auto view = Take(summed->view());
    ASC_SPARSE_TEST_EQ(test, view.nnz(), 2);
    ASC_SPARSE_TEST_EQ(test, view.values()[0], 0.0);
    ASC_SPARSE_TEST_EQ(test, view.values()[1], 0.0);
    ASC_SPARSE_TEST_EQ(test, report.duplicate_records, 2U);
    ASC_SPARSE_TEST_EQ(test, report.mirrored_entries, 1U);
  }
  options.zero_policy = asc::ExplicitZeroPolicy::kDrop;
  auto dropped = Read<Owner<double, 2>>(kDuplicates, resource, report, options);
  ASC_SPARSE_TEST_CHECK(test, dropped.ok());
  if (dropped.ok()) {
    ASC_SPARSE_TEST_EQ(test, Take(dropped->view()).nnz(), 0);
    ASC_SPARSE_TEST_EQ(test, report.dropped_zeros, 2U);
  }
  auto overflow = Read<Owner<std::int8_t, 1>>(
      "%%MatrixMarket matrix coordinate integer general\n1 1 2\n1 1 127\n1 1 "
      "1\n",
      resource, report, options);
  ASC_SPARSE_TEST_CHECK(test, !overflow.ok());
  ASC_SPARSE_TEST_EQ(test, overflow.status().code(), asc::ErrorCode::kOverflow);
  options = {};
  constexpr std::string_view kPattern =
      "%%MatrixMarket matrix coordinate pattern symmetric\n2 2 1\n2 1\n";
  auto no_unit = Read<Owner<double, 0>>(kPattern, resource, report, options);
  ASC_SPARSE_TEST_CHECK(test, !no_unit.ok());
  options.pattern_policy = asc::MatrixMarketPatternPolicy::kUnit;
  auto pattern = Read<Owner<double, 1>>(kPattern, resource, report, options);
  ASC_SPARSE_TEST_CHECK(test, pattern.ok());
  if (pattern.ok()) {
    auto view = Take(pattern->view());
    ASC_SPARSE_TEST_EQ(test, view.nnz(), 2);
    ASC_SPARSE_TEST_EQ(test, view.values()[0], 1.0);
    ASC_SPARSE_TEST_EQ(test, view.values()[1], 1.0);
  }
}

constexpr std::string_view kHermitian =
    "%%MatrixMarket matrix coordinate complex hermitian\n"
    "2 2 3\n1 1 4 0\n2 1 2 2\n2 2 11 0\n";

template <int Kind>
void Rollback(TestContext& test) {
  using T = std::complex<double>;
  Resource resource;
  asc::SparseMatrixMarketReport report;
  auto owner = Take(Read<Owner<T, Kind>>(kHermitian, resource, report));
  auto view = Take(owner.view());
  const std::array<T, 4> sentinels{T{51, 3}, T{52, 4}, T{53, 5}, T{54, 6}};
  std::array<asc::index_t, 16> coordinates{};
  std::array<T, 8> values{};
  std::array<std::byte, 512> scratch{};
  asc::SparseMatrixMarketWorkspace<T> workspace{coordinates, values};
  for (std::size_t position = 0; position <= kHermitian.size(); ++position) {
    std::copy(sentinels.begin(), sentinels.end(), view.values());
    Source source(kHermitian);
    source.fail_after = position;
    source.failure = asc::Status(asc::ErrorCode::kIo, std::string(256, 's'),
                                 std::string(256, 'p'), 71);
    asc_sparse_test::AllocationProbe probe;
    const auto status = asc::ReadSparseMatrixMarketInto(
        source, view, scratch, workspace, {}, {}, report);
    ASC_SPARSE_TEST_CHECK(test, !status.ok());
    ASC_SPARSE_TEST_EQ(test, report.io.input_bytes, position);
    ASC_SPARSE_TEST_CHECK(test, !report.io.committed);
    ASC_SPARSE_TEST_CHECK(
        test, std::equal(sentinels.begin(), sentinels.end(), view.values()));
    ASC_SPARSE_TEST_CHECK(
        test, asc_test::ProcessAllocationCountMatches(probe.count(), 0));
  }
  for (std::size_t size = 0; size < kHermitian.size() - 1; ++size) {
    Source source(kHermitian.substr(0, size));
    const auto status = asc::ReadSparseMatrixMarketInto(
        source, view, scratch, workspace, {}, {}, report);
    ASC_SPARSE_TEST_CHECK(test, !status.ok());
    ASC_SPARSE_TEST_CHECK(test, !report.io.committed);
    ASC_SPARSE_TEST_CHECK(
        test, std::equal(sentinels.begin(), sentinels.end(), view.values()));
  }
  Source source(kHermitian.substr(0, kHermitian.size() - 1));
  ASC_SPARSE_TEST_CHECK(
      test, asc::ReadSparseMatrixMarketInto(source, view, scratch, workspace,
                                            {}, {}, report)
                .ok());
  ASC_SPARSE_TEST_CHECK(test, report.io.committed);
  ASC_SPARSE_TEST_EQ(test, resource.attempts, Kind == 0 ? 4U : 6U);
  ++executed;
}

void Malformed(TestContext& test) {
  constexpr std::array<std::string_view, 20> kInputs{
      "%%MatrixMarket matrix array real general\n1 1\n1\n",
      "%%MatrixMarket vector coordinate real general\n1 1 0\n",
      "%%MatrixMarket matrix coordinate pattern hermitian\n1 1 0\n",
      "%%MatrixMarket matrix coordinate pattern skew-symmetric\n1 1 0\n",
      "%%MatrixMarket matrix coordinate integer hermitian\n1 1 0\n",
      "%%MatrixMarket matrix coordinate real hermitian\n1 1 0\n",
      "%%MatrixMarket matrix coordinate real symmetric\n2 3 0\n",
      "%%MatrixMarket matrix coordinate real general\n0 2 1\n1 1 1\n",
      "%%MatrixMarket matrix coordinate real general\n1 1 1\n0 1 1\n",
      "%%MatrixMarket matrix coordinate real general\n1 1 1\n1 2 1\n",
      "%%MatrixMarket matrix coordinate real general\n1 1 1\n+1 1 1\n",
      "%%MatrixMarket matrix coordinate real symmetric\n2 2 1\n1 2 1\n",
      "%%MatrixMarket matrix coordinate real skew-symmetric\n1 1 1\n1 1 0\n",
      "%%MatrixMarket matrix coordinate complex hermitian\n1 1 1\n1 1 1 2\n",
      "%%MatrixMarket matrix coordinate real general\n1 1 0\n1 1 1\n",
      "%%MatrixMarket matrix coordinate real general\n1 1 0\n%%MatrixMarket "
      "matrix coordinate real general\n",
      "%%MatrixMarket matrix coordinate real general\n1 1 1\n1 1 nan\n",
      "%%MatrixMarket matrix coordinate real general\n1 1 1\n1 1 1e-9999\n",
      "%%MatrixMarket matrix coordinate real general\n1 1 1\n1 1 2 garbage\n",
      "%%MatrixMarket matrix coordinate real general\n-1 1 0\n"};
  for (const auto input : kInputs) {
    Resource resource;
    asc::SparseMatrixMarketReport report;
    asc::SparseMatrixMarketReadOptions options;
    options.pattern_policy = asc::MatrixMarketPatternPolicy::kUnit;
    asc_sparse_test::AllocationProbe probe;
    {
      const auto owner = Read<Owner<std::complex<double>, 0>>(input, resource,
                                                              report, options);
      ASC_SPARSE_TEST_CHECK(test, !owner.ok());
    }
    ASC_SPARSE_TEST_CHECK(test, !report.io.committed);
    ASC_SPARSE_TEST_EQ(test, resource.live, 0U);
    ASC_SPARSE_TEST_CHECK(
        test,
        asc_test::ProcessAllocationCountMatches(
            probe.count(), asc_test::ProcessVisibleResourceAllocationCount(
                               resource.nonempty_allocations)));
    ++executed;
  }
}

template <int Kind>
void AllocationFailures(TestContext& test) {
  for (std::size_t request = 1; request <= (Kind == 0 ? 4U : 6U); ++request) {
    Resource resource;
    resource.fail_on = request;
    resource.failure =
        asc::Status(asc::ErrorCode::kAllocation, std::string(256, 'm'),
                    std::string(256, 'p'), 87);
    asc::SparseMatrixMarketReport report;
    asc_sparse_test::AllocationProbe probe;
    auto owner =
        Read<Owner<std::complex<double>, Kind>>(kHermitian, resource, report);
    ASC_SPARSE_TEST_CHECK(test, !owner.ok());
    ASC_SPARSE_TEST_CHECK(test, !report.io.committed);
    ASC_SPARSE_TEST_EQ(test, resource.live, 0U);
    ASC_SPARSE_TEST_EQ(test, resource.attempts, request);
    ASC_SPARSE_TEST_CHECK(
        test,
        asc_test::ProcessAllocationCountMatches(
            probe.count(),
            asc_test::ProcessVisibleResourceAllocationCount(request - 1)));
    ++executed;
  }
}

template <int Kind>
void EmptyBudgets(TestContext& test) {
  constexpr std::size_t kRequests = Kind == 0 ? 2 : 4;
  for (std::size_t cap = 0; cap <= kRequests; ++cap) {
    Resource resource;
    asc::SparseMatrixMarketReport report;
    asc::ArrayIoLimits limits;
    limits.max_allocations = cap;
    asc_sparse_test::AllocationProbe probe;
    {
      auto owner = Read<Owner<double, Kind>>(
          "%%MatrixMarket matrix coordinate real general\n0 0 0\n", resource,
          report, {}, limits);
      ASC_SPARSE_TEST_EQ(test, owner.ok(), cap == kRequests);
      ASC_SPARSE_TEST_EQ(test, report.required_allocations, kRequests);
      ASC_SPARSE_TEST_EQ(test, report.io.committed, cap == kRequests);
    }
    ASC_SPARSE_TEST_EQ(test, resource.attempts,
                       cap == kRequests ? kRequests : 0);
    ASC_SPARSE_TEST_EQ(test, resource.live, 0U);
    ASC_SPARSE_TEST_CHECK(
        test,
        asc_test::ProcessAllocationCountMatches(
            probe.count(), asc_test::ProcessVisibleResourceAllocationCount(
                               resource.nonempty_allocations)));
    ++executed;
  }
}

void SinkFailures(TestContext& test) {
  Resource resource;
  asc::SparseMatrixMarketReport report;
  auto owner =
      Take(Read<Owner<std::complex<double>, 2>>(kHermitian, resource, report));
  auto view = Take(owner.view());
  Sink complete;
  std::array<std::byte, 512> scratch{};
  asc::SparseMatrixMarketWriteOptions options;
  options.symmetry = MatrixMarketSymmetry::kHermitian;
  ASC_SPARSE_TEST_CHECK(test, asc::WriteSparseMatrixMarket(
                                  view, complete, {}, options, scratch, report)
                                  .ok());
  ASC_SPARSE_TEST_EQ(test, complete.text(), kHermitian);
  for (std::size_t position = 0; position < complete.size; ++position) {
    Sink sink;
    sink.fail_after = position;
    sink.failure = asc::Status(asc::ErrorCode::kIo, std::string(256, 'm'),
                               std::string(256, 'p'), 89);
    asc_sparse_test::AllocationProbe probe;
    const auto status =
        asc::WriteSparseMatrixMarket(view, sink, {}, options, scratch, report);
    ASC_SPARSE_TEST_EQ(test, status.code(), asc::ErrorCode::kIo);
    ASC_SPARSE_TEST_EQ(test, status.native_code(), 89);
    ASC_SPARSE_TEST_EQ(test, sink.size, position);
    ASC_SPARSE_TEST_EQ(test, report.io.output_bytes, position);
    ASC_SPARSE_TEST_EQ(test, sink.text(), complete.text().substr(0, position));
    ASC_SPARSE_TEST_CHECK(
        test, asc_test::ProcessAllocationCountMatches(probe.count(), 0));
  }
  ++executed;
}

void ZeroProjection(TestContext& test) {
  Resource resource;
  asc::SparseMatrixMarketReport report;
  auto owner = Take(Read<Owner<double, 0>>(
      "%%MatrixMarket matrix coordinate real general\n2 2 1\n1 2 0\n", resource,
      report));
  auto view = Take(owner.view());
  Sink sink;
  std::array<std::byte, 512> scratch{};
  asc::SparseMatrixMarketWriteOptions write;
  write.symmetry = MatrixMarketSymmetry::kSymmetric;
  ASC_SPARSE_TEST_CHECK(
      test, asc::WriteSparseMatrixMarket(view, sink, {}, write, scratch, report)
                .ok());
  ASC_SPARSE_TEST_EQ(
      test, sink.text(),
      "%%MatrixMarket matrix coordinate real symmetric\n2 2 1\n2 1 0\n");
  auto symmetric = Take(Read<Owner<double, 1>>(sink.text(), resource, report));
  auto symmetric_view = Take(symmetric.view());
  ASC_SPARSE_TEST_EQ(test, symmetric_view.nnz(), 2);
  ASC_SPARSE_TEST_EQ(test, symmetric_view.values()[0], 0.0);
  ASC_SPARSE_TEST_EQ(test, symmetric_view.values()[1], 0.0);
  std::array<asc::index_t, 8> coordinates{};
  std::array<double, 4> values{};
  asc::SparseMatrixMarketWorkspace<double> workspace{coordinates, values};
  view.values()[0] = 39;
  Source source(sink.text());
  ASC_SPARSE_TEST_EQ(test,
                     asc::ReadSparseMatrixMarketInto(source, view, scratch,
                                                     workspace, {}, {}, report)
                         .code(),
                     asc::ErrorCode::kShape);
  ASC_SPARSE_TEST_EQ(test, view.values()[0], 39.0);
  Sink invalid;
  ASC_SPARSE_TEST_CHECK(test, !asc::WriteSparseMatrixMarket(
                                   view, invalid, {}, write, scratch, report)
                                   .ok());
  ASC_SPARSE_TEST_EQ(test, invalid.size, 0U);
  ++executed;
}

void SkewZeroProjection(TestContext& test) {
  Resource resource;
  asc::SparseMatrixMarketReport report;
  auto owner = Take(Read<Owner<double, 2>>(
      "%%MatrixMarket matrix coordinate real general\n2 2 3\n"
      "1 1 0\n1 2 -2\n2 1 2\n",
      resource, report));
  auto view = Take(owner.view());
  Sink sink;
  std::array<std::byte, 512> scratch{};
  asc::SparseMatrixMarketWriteOptions write;
  write.symmetry = MatrixMarketSymmetry::kSkewSymmetric;
  ASC_SPARSE_TEST_CHECK(
      test, asc::WriteSparseMatrixMarket(view, sink, {}, write, scratch, report)
                .ok());
  ASC_SPARSE_TEST_EQ(test, report.omitted_diagonal_zeros, 1U);
  ASC_SPARSE_TEST_EQ(
      test, sink.text(),
      "%%MatrixMarket matrix coordinate real skew-symmetric\n2 2 1\n2 1 2\n");
  auto skew = Take(Read<Owner<double, 0>>(sink.text(), resource, report));
  auto skew_view = Take(skew.view());
  ASC_SPARSE_TEST_EQ(test, skew_view.nnz(), 2);
  ASC_SPARSE_TEST_EQ(test, skew_view.values()[0], -2.0);
  ASC_SPARSE_TEST_EQ(test, skew_view.values()[1], 2.0);
  std::array<asc::index_t, 8> coordinates{};
  std::array<double, 4> values{};
  asc::SparseMatrixMarketWorkspace<double> workspace{coordinates, values};
  Source source(sink.text());
  const std::array<double, 3> original{0, 2, -2};
  ASC_SPARSE_TEST_EQ(test,
                     asc::ReadSparseMatrixMarketInto(source, view, scratch,
                                                     workspace, {}, {}, report)
                         .code(),
                     asc::ErrorCode::kShape);
  ASC_SPARSE_TEST_CHECK(
      test, std::equal(original.begin(), original.end(), view.values()));
  view.values()[0] = 1;
  Sink invalid;
  ASC_SPARSE_TEST_CHECK(test, !asc::WriteSparseMatrixMarket(
                                   view, invalid, {}, write, scratch, report)
                                   .ok());
  ASC_SPARSE_TEST_EQ(test, invalid.size, 0U);
  ++executed;
}

void AliasesAndLimits(TestContext& test) {
  Resource resource;
  asc::SparseMatrixMarketReport report;
  constexpr std::string_view kInput =
      "%%MatrixMarket matrix coordinate real general\n2 2 2\n1 2 3\n2 1 4\n";
  auto owner = Take(Read<Owner<double, 0>>(kInput, resource, report));
  auto view = Take(owner.view());
  std::array<asc::index_t, 8> coordinates{};
  std::array<double, 4> values{};
  std::array<std::byte, 512> scratch{};
  asc::SparseMatrixMarketWorkspace<double> workspace{coordinates, values};
  for (const int alias : {0, 1, 2}) {
    Source source(kInput);
    auto work = workspace;
    std::span<std::byte> bytes(scratch);
    if (alias == 0) {
      work.values = {view.values(), 2};
    } else if (alias == 1) {
      work.coordinates = {const_cast<asc::index_t*>(view.coordinates()), 4};
    } else {
      bytes = std::as_writable_bytes(work.values);
    }
    const auto status = asc::ReadSparseMatrixMarketInto(source, view, bytes,
                                                        work, {}, {}, report);
    ASC_SPARSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidArgument);
    ASC_SPARSE_TEST_EQ(test, source.position, 0U);
  }
  asc::SparseMatrixMarketReadOptions options;
  options.max_sort_comparisons = 0;
  Source limited(kInput);
  const auto status = asc::ReadSparseMatrixMarketInto(
      limited, view, scratch, workspace, {}, options, report);
  ASC_SPARSE_TEST_EQ(test, status.code(), asc::ErrorCode::kAllocation);
  ASC_SPARSE_TEST_EQ(test, report.sort_comparisons, 0U);
  ASC_SPARSE_TEST_EQ(test, view.values()[0], 3.0);
  ASC_SPARSE_TEST_EQ(test, view.values()[1], 4.0);
  Sink complete;
  ASC_SPARSE_TEST_CHECK(test, asc::WriteSparseMatrixMarket(view, complete, {},
                                                           {}, scratch, report)
                                  .ok());
  asc::ArrayIoLimits limits;
  limits.max_output_bytes = complete.size - 1;
  Sink bounded;
  ASC_SPARSE_TEST_EQ(
      test,
      asc::WriteSparseMatrixMarket(view, bounded, limits, {}, scratch, report)
          .code(),
      asc::ErrorCode::kAllocation);
  ASC_SPARSE_TEST_EQ(test, bounded.size, 0U);
  ++executed;
}

void OverflowHeaders(TestContext& test) {
  constexpr auto kMaximum =
      static_cast<std::uint64_t>(std::numeric_limits<asc::extent_t>::max());
  asc::ArrayIoLimits limits;
  limits.max_extent = kMaximum;
  limits.max_logical_elements = kMaximum;
  limits.max_stored_elements = kMaximum;
  for (const char* const input :
       {"%%MatrixMarket matrix coordinate real "
        "general\n4294967296 4294967296 0\n",
        "%%MatrixMarket matrix coordinate real general\n0 "
        "9223372036854775807 0\n",
        "%%MatrixMarket matrix coordinate real general\n1 1 "
        "9223372036854775807\n"}) {
    Resource resource;
    asc::SparseMatrixMarketReport report;
    asc_sparse_test::AllocationProbe probe;
    auto owner = Read<Owner<double, 2>>(input, resource, report, {}, limits);
    ASC_SPARSE_TEST_CHECK(test, !owner.ok());
    ASC_SPARSE_TEST_EQ(test, owner.status().code(), asc::ErrorCode::kOverflow);
    ASC_SPARSE_TEST_EQ(test, resource.attempts, 0U);
    ASC_SPARSE_TEST_CHECK(
        test, asc_test::ProcessAllocationCountMatches(probe.count(), 0));
    ++executed;
  }
  Resource resource;
  asc::SparseMatrixMarketReport report;
  auto empty = Read<Owner<double, 0>>(
      "%%MatrixMarket matrix coordinate real general\n0 9223372036854775807 "
      "0\n",
      resource, report, {}, limits);
  ASC_SPARSE_TEST_CHECK(test, empty.ok());
  ASC_SPARSE_TEST_EQ(test, resource.attempts, 2U);
}

void PathRoundTrip(TestContext& test) {
  Resource resource;
  asc::SparseMatrixMarketReport report;
  auto owner =
      Take(Read<Owner<std::complex<double>, 1>>(kHermitian, resource, report));
  auto view = Take(owner.view());
  // CTest and standalone evidence runs use an external build/evidence cwd.
  const auto path =
      std::filesystem::current_path() / "sparse-matrix-market-roundtrip.mtx";
  std::array<std::byte, 512> scratch{};
  asc::SparseMatrixMarketWriteOptions options;
  options.symmetry = MatrixMarketSymmetry::kHermitian;
  ASC_SPARSE_TEST_CHECK(
      test, asc::SaveSparseMatrixMarket(path, view,
                                        asc::ArrayFileOverwrite::kTruncate, {},
                                        options, scratch, report)
                .ok());
  auto loaded = asc::LoadSparseMatrixMarket<Owner<std::complex<double>, 2>>(
      path, resource, scratch, {}, {}, report);
  ASC_SPARSE_TEST_CHECK(test, loaded.ok());
  ASC_SPARSE_TEST_CHECK(test, report.io.committed);
  if (loaded.ok()) {
    auto result = Take(loaded->view());
    const auto lower = asc::internal_sparse_matrix_market::Find(result, {1, 0});
    ASC_SPARSE_TEST_EQ(test, result.values()[lower],
                       (std::complex<double>{2, 2}));
  }
  auto absent = asc::LoadSparseMatrixMarket<Owner<double, 0>>(
      path / "not-a-directory", resource, scratch, {}, {}, report);
  ASC_SPARSE_TEST_CHECK(test, !absent.ok());
  ASC_SPARSE_TEST_CHECK(test, !report.io.committed);
  std::error_code error;
  ASC_SPARSE_TEST_CHECK(test, std::filesystem::remove(path, error));
  ASC_SPARSE_TEST_CHECK(test, !error);
  ++executed;
}

template <int Kind>
void ResourceBudgets(TestContext& test) {
  constexpr std::size_t kPeak = Kind == 0 ? 256 : 272;
  constexpr std::size_t kStructure = Kind == 0 ? 64 : 88;
  constexpr std::size_t kDecoded = Kind == 0 ? 128 : 120;
  constexpr std::size_t kRequests = Kind == 0 ? 4 : 6;
  for (const int limit_kind : {0, 1, 2, 3, 4, 5, 6}) {
    Resource resource;
    asc::ArrayIoLimits limits;
    asc::SparseMatrixMarketReport report;
    switch (limit_kind) {
      case 0:
        limits.max_staging_bytes = kPeak - 1;
        break;
      case 1:
        limits.max_allocation_bytes = kPeak - 1;
        break;
      case 2:
        limits.max_structure_bytes = kStructure - 1;
        break;
      case 3:
        limits.max_decoded_bytes = kDecoded - 1;
        break;
      case 4:
        limits.max_allocations = kRequests - 1;
        break;
      case 5:
        limits.max_scratch_bytes = 511;
        break;
      case 6:
        resource.placement = asc::MemorySpace::kDevice;
        break;
      default:
        std::abort();
    }
    asc_sparse_test::AllocationProbe probe;
    auto owner = Read<Owner<std::complex<double>, Kind>>(kHermitian, resource,
                                                         report, {}, limits);
    ASC_SPARSE_TEST_CHECK(test, !owner.ok());
    ASC_SPARSE_TEST_EQ(test, resource.attempts, 0U);
    ASC_SPARSE_TEST_CHECK(test, !report.io.committed);
    ASC_SPARSE_TEST_CHECK(
        test, asc_test::ProcessAllocationCountMatches(probe.count(), 0));
    ++executed;
  }
  Resource resource;
  asc::SparseMatrixMarketReport report;
  asc::ArrayIoLimits limits;
  limits.max_staging_bytes = kPeak;
  limits.max_allocation_bytes = kPeak;
  limits.max_structure_bytes = kStructure;
  limits.max_decoded_bytes = kDecoded;
  limits.max_allocations = kRequests;
  auto owner = Read<Owner<std::complex<double>, Kind>>(kHermitian, resource,
                                                       report, {}, limits);
  ASC_SPARSE_TEST_CHECK(test, owner.ok());
  ASC_SPARSE_TEST_EQ(test, report.required_staging_bytes, kPeak);
  ASC_SPARSE_TEST_EQ(test, resource.attempts, kRequests);
}

void NumericFailures(TestContext& test) {
  asc::SparseMatrixMarketReadOptions options;
  options.duplicate_policy = asc::DuplicatePolicy::kSum;
  for (const char* const input :
       {"%%MatrixMarket matrix coordinate real general\n1 1 2\n1 1 1e308\n1 1 "
        "1e308\n",
        "%%MatrixMarket matrix coordinate complex general\n1 1 2\n1 1 0 "
        "1e308\n1 1 0 1e308\n"}) {
    Resource resource;
    asc::SparseMatrixMarketReport report;
    auto owner =
        Read<Owner<std::complex<double>, 1>>(input, resource, report, options);
    ASC_SPARSE_TEST_EQ(test, owner.status().code(), asc::ErrorCode::kOverflow);
    ASC_SPARSE_TEST_EQ(test, resource.live, 0U);
    ASC_SPARSE_TEST_CHECK(test, !report.io.committed);
    ++executed;
  }
  Resource resource;
  asc::SparseMatrixMarketReport report;
  auto minimum = Read<Owner<std::int64_t, 0>>(
      "%%MatrixMarket matrix coordinate integer skew-symmetric\n2 2 1\n2 1 "
      "-9223372036854775808\n",
      resource, report);
  ASC_SPARSE_TEST_EQ(test, minimum.status().code(), asc::ErrorCode::kOverflow);
  auto unsigned_value = Read<Owner<std::uint64_t, 0>>(
      "%%MatrixMarket matrix coordinate integer skew-symmetric\n2 2 1\n2 1 1\n",
      resource, report);
  ASC_SPARSE_TEST_EQ(test, unsigned_value.status().code(),
                     asc::ErrorCode::kOverflow);
  auto unsigned_zero = Read<Owner<std::uint64_t, 0>>(
      "%%MatrixMarket matrix coordinate integer skew-symmetric\n2 2 1\n2 1 0\n",
      resource, report);
  ASC_SPARSE_TEST_CHECK(test, unsigned_zero.ok());
  if (unsigned_zero.ok()) {
    ASC_SPARSE_TEST_EQ(test, Take(unsigned_zero->view()).nnz(), 2);
  }
}

void ReaderStateAliases(TestContext& test) {
  Resource resource;
  asc::SparseMatrixMarketReport report;
  constexpr std::string_view kInput =
      "%%MatrixMarket matrix coordinate integer general\n1 1 1\n1 1 2\n";
  auto owner = Take(Read<Owner<std::size_t, 0>>(kInput, resource, report));
  auto view = Take(owner.view());
  std::array<std::byte, 512> scratch{};
  std::array<std::size_t, 1> values{};
  std::array<asc::index_t, 2> coordinates{};
  Source source(kInput);
  auto reader = Take(
      asc::SparseMatrixMarketReader::Prepare(source, scratch, {}, {}, report));
  const auto before = source.position;
  asc::SparseMatrixMarketWorkspace<std::size_t> metadata_alias{
      std::span(const_cast<asc::extent_t*>(reader.shape().data()), 2), values};
  ASC_SPARSE_TEST_EQ(
      test,
      asc::ReadSparseMatrixMarketInto(reader, view, metadata_alias).code(),
      asc::ErrorCode::kInvalidArgument);
  ASC_SPARSE_TEST_EQ(test, source.position, before);
  ASC_SPARSE_TEST_EQ(test, reader.shape()[0], 1);
  ASC_SPARSE_TEST_EQ(test, reader.shape()[1], 1);
  Source another(kInput);
  report.source_records = 42;
  asc::SparseMatrixMarketWorkspace<std::size_t> report_alias{
      coordinates, std::span(&report.source_records, 1)};
  ASC_SPARSE_TEST_EQ(test,
                     asc::ReadSparseMatrixMarketInto(
                         another, view, scratch, report_alias, {}, {}, report)
                         .code(),
                     asc::ErrorCode::kInvalidArgument);
  ASC_SPARSE_TEST_EQ(test, another.position, 0U);
  ASC_SPARSE_TEST_EQ(test, report.source_records, 42U);
  ASC_SPARSE_TEST_EQ(test, view.values()[0], 2U);
  const std::array<asc::extent_t, 2> shape{1, 1};
  const std::array<asc::index_t, 2> single{0, 0};
  auto report_view = Take(asc::CoordinateView<std::size_t, 2>::Create(
      single.data(), &report.source_records, shape, 1,
      asc::MemorySpace::kHost));
  Sink sink;
  ASC_SPARSE_TEST_EQ(
      test,
      asc::WriteSparseMatrixMarket(report_view, sink, {}, {}, scratch, report)
          .code(),
      asc::ErrorCode::kInvalidArgument);
  ASC_SPARSE_TEST_EQ(test, report.source_records, 42U);
  ASC_SPARSE_TEST_EQ(test, sink.size, 0U);
  ++executed;
}

}  // namespace

int main() {
  TestContext test;
  Scalar<std::int8_t>(test);
  Scalar<std::uint8_t>(test);
  Scalar<std::int16_t>(test);
  Scalar<std::uint16_t>(test);
  Scalar<std::int32_t>(test);
  Scalar<std::uint32_t>(test);
  Scalar<std::int64_t>(test);
  Scalar<std::uint64_t>(test);
  Scalar<float>(test);
  Scalar<double>(test);
  Scalar<std::complex<float>>(test);
  Scalar<std::complex<double>>(test);
  Patterns<std::int8_t>(test);
  Patterns<std::uint8_t>(test);
  Patterns<std::int16_t>(test);
  Patterns<std::uint16_t>(test);
  Patterns<std::int32_t>(test);
  Patterns<std::uint32_t>(test);
  Patterns<std::int64_t>(test);
  Patterns<std::uint64_t>(test);
  Patterns<float>(test);
  Patterns<double>(test);
  Patterns<std::complex<float>>(test);
  Patterns<std::complex<double>>(test);
  Policies(test);
  Rollback<0>(test);
  Rollback<1>(test);
  Rollback<2>(test);
  Malformed(test);
  AllocationFailures<0>(test);
  AllocationFailures<1>(test);
  AllocationFailures<2>(test);
  EmptyBudgets<0>(test);
  EmptyBudgets<1>(test);
  EmptyBudgets<2>(test);
  SinkFailures(test);
  ZeroProjection(test);
  SkewZeroProjection(test);
  AliasesAndLimits(test);
  OverflowHeaders(test);
  PathRoundTrip(test);
  ResourceBudgets<0>(test);
  ResourceBudgets<1>(test);
  ResourceBudgets<2>(test);
  NumericFailures(test);
  ReaderStateAliases(test);
  std::cout << "sparse_matrix_market executed=" << executed << '\n';
  return test.Finish();
}
