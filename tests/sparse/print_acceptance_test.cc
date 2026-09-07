#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "../array_display_test_support.h"
#include "allocation_observation.h"
#include "allocation_probe.h"
#include "asc/core/array_format.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/sparse/compressed.h"
#include "asc/sparse/coordinate.h"
#include "asc/sparse/print.h"

namespace {

using asc_array_display_test::Sink;
using asc_array_display_test::TestContext;

template <typename Value>
void ScalarKinds(TestContext& test, Value value, std::string_view token) {
  const std::array<asc::index_t, 2> coordinate{1, 2};
  const std::array<asc::nnz_t, 3> row_offsets{0, 0, 1};
  const std::array<asc::nnz_t, 4> column_offsets{0, 0, 0, 1};
  const asc::index_t row = 1;
  const asc::index_t column = 2;
  const std::array<asc::extent_t, 2> shape{2, 3};
  const auto coo = asc::CoordinateView<const Value, 2>::Create(
      coordinate.data(), &value, shape, 1, asc::MemorySpace::kHost);
  const auto csr = asc::CsrView<const Value>::Create(
      row_offsets.data(), &column, &value, shape, 1, asc::MemorySpace::kHost);
  const auto csc = asc::CscView<const Value>::Create(
      column_offsets.data(), &row, &value, shape, 1, asc::MemorySpace::kHost);
  const std::string expected = "(1,2) = " + std::string(token) + "\n";
  std::array<std::byte, 1024> scratch{};
  asc::ArrayPrintOptions options;
  options.show_metadata = false;
  options.max_rows = 0;
  options.max_columns = 0;
  options.max_slices = 0;
  const auto check = [&](const auto& view) {
    Sink sink;
    asc::ArrayPrintReport report{777, 888, true};
    asc::Status status;
    std::size_t allocations = 0;
    {
      asc_sparse_test::AllocationProbe probe;
      status = asc::PrintArray(view, sink, options, scratch, report);
      allocations = probe.count();
    }
    test.Check(status.ok() && sink.text() == expected,
               "each Sparse format must spell the scalar numerically");
    test.Check(report.values_displayed == 1 && !report.truncated &&
                   report.output_bytes == expected.size(),
               "Sparse must ignore Dense row, column and slice caps");
    test.Check(asc_test::ProcessAllocationCountMatches(allocations, 0),
               "Sparse scalar display must not allocate");
  };
  check(*coo);
  check(*csr);
  check(*csc);
}

void EveryScalar(TestContext& test) {
  ScalarKinds(test, std::int8_t{-128}, "-128");
  ScalarKinds(test, std::uint8_t{255}, "255");
  ScalarKinds(test, std::int16_t{-32768}, "-32768");
  ScalarKinds(test, std::uint16_t{65535}, "65535");
  ScalarKinds(test, std::numeric_limits<std::int32_t>::min(), "-2147483648");
  ScalarKinds(test, std::numeric_limits<std::uint32_t>::max(), "4294967295");
  ScalarKinds(test, std::numeric_limits<std::int64_t>::min(),
              "-9223372036854775808");
  ScalarKinds(test, std::numeric_limits<std::uint64_t>::max(),
              "18446744073709551615");
  ScalarKinds(test, -0.0F, "-0");
  ScalarKinds(test, 1.25, "1.25");
  ScalarKinds(
      test, std::complex<float>{-0.0F, std::numeric_limits<float>::infinity()},
      "(-0,inf)");
  ScalarKinds(test,
              std::complex<double>{std::numeric_limits<double>::quiet_NaN(),
                                   -std::numeric_limits<double>::infinity()},
              "(nan,-inf)");
  std::puts("display sparse all twelve wire scalars across COO, CSR and CSC");
}

template <asc::SparseCompressedFormat Format>
void Progress(TestContext& test, std::string_view expected) {
  const std::array<asc::nnz_t, 5> offsets{0, 2, 2, 3, 5};
  const std::array<asc::index_t, 5> indices{1, 3, 0, 1, 2};
  const std::array<int, 5> values{101, 102, 103, 104, 105};
  const auto view = asc::CompressedSparseView<const int, Format>::Create(
      offsets.data(), indices.data(), values.data(),
      std::array<asc::extent_t, 2>{4, 4}, 5, asc::MemorySpace::kHost);
  std::array<std::byte, 1024> scratch{};
  asc::ArrayPrintOptions options;
  options.show_metadata = false;
  constexpr std::array<std::string_view, 5> kTokens{"101", "102", "103", "104",
                                                    "105"};
  Sink sink;
  asc::ArrayPrintReport report;
  test.Check(asc::PrintArray(*view, sink, options, scratch, report).ok() &&
                 sink.text() == expected,
             "compressed display must preserve its canonical traversal");
  asc_array_display_test::FailureBoundaries(
      test,
      [&](Sink& output, asc::ArrayPrintReport& progress) {
        return asc::PrintArray(*view, output, options, scratch, progress);
      },
      expected, kTokens);
  asc_array_display_test::BudgetBoundaries(
      test,
      [&](Sink& output, asc::ArrayPrintReport& progress, std::size_t budget) {
        auto bounded = options;
        bounded.max_output_bytes = budget;
        return asc::PrintArray(*view, output, bounded, scratch, progress);
      },
      kTokens, expected.size() + 64);
}

template <typename View>
void RejectScratch(TestContext& test, const View& view,
                   std::span<std::byte> scratch) {
  const std::vector<std::byte> before(scratch.begin(), scratch.end());
  Sink sink;
  asc::ArrayPrintReport report{777, 888, true};
  asc::Status status;
  std::size_t allocations = 0;
  {
    asc_sparse_test::AllocationProbe probe;
    status = asc::PrintArray(view, sink, {}, scratch, report);
    allocations = probe.count();
  }
  test.Check(
      status.code() == asc::ErrorCode::kInvalidArgument && sink.text().empty(),
      "scratch overlapping input structure or descriptor must reject");
  test.Check(std::equal(before.begin(), before.end(), scratch.begin()),
             "scratch rejection must preserve every aliased input byte");
  asc_array_display_test::CheckReset(test, report);
  test.Check(asc_test::ProcessAllocationCountMatches(allocations, 0),
             "scratch preflight rejection must not allocate");
}

template <asc::SparseCompressedFormat Format>
void CompressedAliases(TestContext& test) {
  std::array<asc::nnz_t, 4> offsets{0, 1, 1, 2};
  std::array<asc::index_t, 2> indices{0, 2};
  std::array<int, 2> values{101, 102};
  auto view = asc::CompressedSparseView<const int, Format>::Create(
      offsets.data(), indices.data(), values.data(),
      std::array<asc::extent_t, 2>{3, 3}, 2, asc::MemorySpace::kHost);
  RejectScratch(test, *view, std::as_writable_bytes(std::span(offsets)));
  RejectScratch(test, *view, std::as_writable_bytes(std::span(indices)));
  RejectScratch(test, *view, std::as_writable_bytes(std::span(values)));
  RejectScratch(test, *view, std::as_writable_bytes(std::span(&*view, 1)));
}

void CoordinateAliases(TestContext& test) {
  std::array<asc::index_t, 4> coordinates{0, 0, 2, 2};
  std::array<int, 2> values{101, 102};
  auto view = asc::CoordinateView<const int, 2>::Create(
      coordinates.data(), values.data(), std::array<asc::extent_t, 2>{3, 3}, 2,
      asc::MemorySpace::kHost);
  RejectScratch(test, *view, std::as_writable_bytes(std::span(coordinates)));
  RejectScratch(test, *view, std::as_writable_bytes(std::span(values)));
  RejectScratch(test, *view, std::as_writable_bytes(std::span(&*view, 1)));
  std::puts(
      "display sparse scratch aliases: COO coordinates and all view objects");
}

void PreviewAndInvalidSink(TestContext& test) {
  const std::array<asc::index_t, 5> coordinates{0, 1, 2, 3, 4};
  const std::array<int, 5> values{101, 102, 103, 104, 105};
  const auto view = asc::CoordinateView<const int, 1>::Create(
      coordinates.data(), values.data(), std::array<asc::extent_t, 1>{5}, 5,
      asc::MemorySpace::kHost);
  std::array<std::byte, 1024> scratch{};
  asc::ArrayPrintOptions options;
  options.show_metadata = false;
  options.max_elements = 3;
  asc::ArrayPrintReport report;
  Sink edge;
  test.Check(
      asc::PrintArray(*view, edge, options, scratch, report).ok() &&
          edge.text() ==
              "(0) = 101\n(1) = 102\n...\n(4) = 105\n... (truncated)\n" &&
          report.values_displayed == 3 && report.truncated,
      "Sparse odd edge preview must retain the first two and last entry");
  options.edge_preview = false;
  Sink prefix;
  test.Check(
      asc::PrintArray(*view, prefix, options, scratch, report).ok() &&
          prefix.text() == "(0) = 101\n(1) = 102\n(2) = 103\n... (truncated)\n",
      "Sparse prefix preview must show the omitted tail");
  options.max_elements = 64;
  for (const bool invalid : {false, true}) {
    Sink sink;
    sink.invalid_progress = invalid;
    sink.zero_progress = !invalid;
    report = {777, 888, true};
    const auto status = asc::PrintArray(*view, sink, options, scratch, report);
    test.Check(status.code() == asc::ErrorCode::kIo && sink.text().empty(),
               "invalid or zero sink progress must reject");
    asc_array_display_test::CheckReset(test, report);
  }
  std::puts("display sparse previews: odd edges, prefix and invalid sinks");
}

template <asc::SparseCompressedFormat Format>
void LongEmptyOuterRange(TestContext& test) {
  constexpr asc::extent_t kOuter = 1000000;
  std::vector<asc::nnz_t> offsets(kOuter + 1, 0);
  std::fill(offsets.begin() + 500001, offsets.end(), 1);
  offsets.back() = 2;
  const std::array<asc::index_t, 2> indices{0, 1};
  const std::array<int, 2> values{101, 102};
  const auto shape = Format == asc::SparseCompressedFormat::kCsr
                         ? std::array<asc::extent_t, 2>{kOuter, 2}
                         : std::array<asc::extent_t, 2>{2, kOuter};
  const auto view = asc::CompressedSparseView<const int, Format>::Create(
      offsets.data(), indices.data(), values.data(), shape, 2,
      asc::MemorySpace::kHost);
  std::array<std::byte, 1024> scratch{};
  asc::ArrayPrintOptions options;
  options.show_metadata = false;
  Sink sink;
  asc::ArrayPrintReport report;
  test.Check(asc::PrintArray(*view, sink, options, scratch, report).ok(),
             "legal million-entry offset backing must display successfully");
  const std::string_view expected =
      Format == asc::SparseCompressedFormat::kCsr
          ? "(500000,0) = 101\n(999999,1) = 102\n"
          : "(0,500000) = 101\n(1,999999) = 102\n";
  test.Check(sink.text() == expected && report.values_displayed == 2 &&
                 !report.truncated,
             "compressed outer lookup must recover distant exact coordinates");
}

}  // namespace

int main() {
  TestContext test;
  EveryScalar(test);
  Progress<asc::SparseCompressedFormat::kCsr>(
      test,
      "(0,1) = 101\n(0,3) = 102\n(2,0) = 103\n(3,1) = 104\n(3,2) = 105\n");
  Progress<asc::SparseCompressedFormat::kCsc>(
      test,
      "(1,0) = 101\n(3,0) = 102\n(0,2) = 103\n(1,3) = 104\n(2,3) = 105\n");
  CoordinateAliases(test);
  CompressedAliases<asc::SparseCompressedFormat::kCsr>(test);
  CompressedAliases<asc::SparseCompressedFormat::kCsc>(test);
  PreviewAndInvalidSink(test);
  LongEmptyOuterRange<asc::SparseCompressedFormat::kCsr>(test);
  LongEmptyOuterRange<asc::SparseCompressedFormat::kCsc>(test);
  std::puts(
      "display sparse compressed traversal: both formats, failure and budget "
      "sweeps, distant offsets");
  return test.Finish();
}
