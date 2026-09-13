#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <numeric>
#include <span>
#include <type_traits>
#include <vector>

#include "array_display_test_support.h"
#include "asc/core/array_format.h"
#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/dense/layout.h"
#include "asc/dense/print.h"
#include "asc/dense/view.h"
#include "asc/sparse/compressed.h"
#include "asc/sparse/coordinate.h"
#include "asc/sparse/print.h"
#include "observation.h"

namespace {

using asc_array_display_test::Sink;
using asc_array_display_test::TestContext;
using asc_display_observation::Region;

void ObservationControl(TestContext& test) {
  std::array<int, 4> values{101, 102, 103, 104};
  std::array<std::uint32_t, sizeof(values)> reads{};
  std::array<Region, 1> regions{{{std::as_bytes(std::span(values)), reads}}};
  asc_display_observation::Start(regions);
  const volatile int* watched = values.data();
  const int first = watched[1];
  const int second = watched[1];
  asc_display_observation::Stop();
  test.Check(first == 102 && second == 102,
             "the positive control must read live values");
  for (std::size_t byte = 0; byte < reads.size(); ++byte) {
    test.Check(reads[byte] == (byte / sizeof(int) == 1 ? 2U : 0U),
               "the observer must detect exactly two selected loads");
  }
  std::puts(
      "read observer positive control: repeated reads detected, omitted bytes "
      "untouched");
}

template <typename Value>
void Dense(TestContext& test) {
  std::array<Value, 70> values{};
  for (std::size_t i = 0; i < values.size(); ++i) {
    values[i] = Value(static_cast<int>(i + 1));
  }
  std::array<std::uint32_t, sizeof(values)> reads{};
  std::array<Region, 1> regions{{{std::as_bytes(std::span(values)), reads}}};
  const auto mapping = asc::DenseLayout<2>::Create(
      std::array<asc::extent_t, 2>{3, 7}, asc::LayoutStride<2>{{1, 10}});
  const auto view = asc::DenseView<const Value, 2>::Create(
      values.data(), *mapping, asc::MemorySpace::kHost);
  asc::ArrayPrintOptions options;
  options.show_metadata = false;
  options.max_rows = 2;
  options.max_columns = 3;
  std::array<std::byte, 1024> scratch{};
  asc::ArrayPrintReport report;
  Sink sink;
  asc_display_observation::Start(regions);
  const auto status = asc::PrintArray(*view, sink, options, scratch, report);
  asc_display_observation::Stop();
  test.Check(
      status.ok() && report.values_displayed == 6 && report.truncated,
      "strided Dense preview must emit the six independently selected values");
  constexpr std::array<std::size_t, 6> kSelected{0, 10, 60, 2, 12, 62};
  for (std::size_t byte = 0; byte < reads.size(); ++byte) {
    const bool selected = std::find(kSelected.begin(), kSelected.end(),
                                    byte / sizeof(Value)) != kSelected.end();
    test.Check(reads[byte] == (selected ? 1U : 0U),
               "Dense must read each selected backing byte once and no padding "
               "or omission");
  }
  options.max_elements = 0;
  Sink zero;
  asc_display_observation::Start(regions);
  const auto zero_status =
      asc::PrintArray(*view, zero, options, scratch, report);
  asc_display_observation::Stop();
  test.Check(
      zero_status.ok() && std::all_of(reads.begin(), reads.end(),
                                      [](auto count) { return count == 0; }),
      "zero Dense preview must perform no input value read");
}

template <typename Value>
void Coordinate(TestContext& test) {
  const std::array<asc::index_t, 8> coordinates{0, 1, 2, 3, 4, 5, 6, 7};
  std::array<Value, 8> values{};
  std::fill(values.begin(), values.end(), Value{7});
  std::array<std::uint32_t, sizeof(values)> reads{};
  std::array<Region, 1> regions{{{std::as_bytes(std::span(values)), reads}}};
  const auto view = asc::CoordinateView<const Value, 1>::Create(
      coordinates.data(), values.data(), std::array<asc::extent_t, 1>{8}, 8,
      asc::MemorySpace::kHost);
  asc::ArrayPrintOptions options;
  options.max_elements = 3;
  std::array<std::byte, 1024> scratch{};
  asc::ArrayPrintReport report;
  Sink sink;
  asc_display_observation::Start(regions);
  const auto status = asc::PrintArray(*view, sink, options, scratch, report);
  asc_display_observation::Stop();
  test.Check(status.ok() && report.values_displayed == 3 && report.truncated,
             "COO preview must emit first two and last stored value");
  for (std::size_t byte = 0; byte < reads.size(); ++byte) {
    const std::size_t index = byte / sizeof(Value);
    test.Check(
        reads[byte] == (index == 0 || index == 1 || index == 7 ? 1U : 0U),
        "COO must read exactly the selected stored scalar bytes once");
  }
}

template <typename Value, asc::SparseCompressedFormat Format>
void Compressed(TestContext& test) {
  constexpr asc::extent_t kOuter = 65536;
  std::vector<asc::nnz_t> offsets(kOuter + 1, 0);
  for (std::size_t i = 0; i < offsets.size(); ++i) {
    offsets[i] = static_cast<asc::nnz_t>(i > 1) +
                 static_cast<asc::nnz_t>(i > kOuter / 2) +
                 static_cast<asc::nnz_t>(i > kOuter - 1);
  }
  const std::array<asc::index_t, 3> indices{0, 0, 0};
  const std::array<Value, 3> values{Value{101}, Value{102}, Value{103}};
  std::array<std::uint32_t, sizeof(values)> reads{};
  std::vector<std::uint32_t> offset_reads(offsets.size() * sizeof(asc::nnz_t),
                                          0);
  std::array<Region, 2> regions{
      {{std::as_bytes(std::span(values)), reads},
       {std::as_bytes(std::span(offsets)), offset_reads}}};
  const auto shape = Format == asc::SparseCompressedFormat::kCsr
                         ? std::array<asc::extent_t, 2>{kOuter, 1}
                         : std::array<asc::extent_t, 2>{1, kOuter};
  const auto view = asc::CompressedSparseView<const Value, Format>::Create(
      offsets.data(), indices.data(), values.data(), shape, 3,
      asc::MemorySpace::kHost);
  std::array<std::byte, 1024> scratch{};
  asc::ArrayPrintReport report;
  Sink sink;
  asc_display_observation::Start(regions);
  const auto status = asc::PrintArray(*view, sink, {}, scratch, report);
  asc_display_observation::Stop();
  test.Check(status.ok() && report.values_displayed == 3 && !report.truncated,
             "compressed display must emit all three distant stored values");
  test.Check(
      std::all_of(reads.begin(), reads.end(), [](auto n) { return n == 1; }),
      "compressed display must read each value byte exactly once");
  const auto loads =
      std::accumulate(offset_reads.begin(), offset_reads.end(), std::size_t{0});
  test.Check(loads > 0 && loads <= std::size_t{3} * 20 * sizeof(asc::nnz_t),
             "three binary searches must stay under twenty offset loads each");
  std::printf(
      "compressed scalar_bytes=%zu complex=%d format=%d value_bytes=%zu "
      "offset_bytes=%zu backing=%zu\n",
      sizeof(Value), static_cast<int>(!std::is_arithmetic_v<Value>),
      static_cast<int>(Format), sizeof(values), loads,
      offsets.size() * sizeof(asc::nnz_t));
}

template <typename Value, std::size_t Rank>
void HigherRankCase(TestContext& test, bool edge, bool metadata,
                    std::size_t limit) {
  static_assert(Rank == 3 || Rank == 4);
  std::array<Value, 420> values{};
  values.fill(Value{7});
  std::array<std::uint32_t, sizeof(values)> reads{};
  std::array<Region, 1> regions{{{std::as_bytes(std::span(values)), reads}}};
  std::array<asc::extent_t, Rank> shape{};
  std::array<asc::stride_t, Rank> strides{};
  shape[0] = 3;
  shape[1] = 7;
  shape[2] = 3;
  strides[0] = 1;
  strides[1] = 10;
  strides[2] = 70;
  if constexpr (Rank == 4) {
    shape[3] = 2;
    strides[3] = 210;
  }
  const auto mapping =
      asc::DenseLayout<Rank>::Create(shape, asc::LayoutStride<Rank>{strides});
  const auto view = asc::DenseView<const Value, Rank>::Create(
      values.data(), *mapping, asc::MemorySpace::kHost);
  asc::ArrayPrintOptions options;
  options.show_metadata = metadata;
  options.edge_preview = edge;
  options.max_rows = 2;
  options.max_columns = 3;
  options.max_slices = 2;
  options.max_elements = limit;
  std::array<std::byte, 1024> scratch{};
  asc::ArrayPrintReport report;
  Sink sink;
  asc_display_observation::Start(regions);
  const auto status = asc::PrintArray(*view, sink, options, scratch, report);
  asc_display_observation::Stop();
  const std::size_t displayed = std::min(limit, std::size_t{12});
  test.Check(
      status.ok() && report.values_displayed == displayed && report.truncated,
      "higher rank combined preview must report its selected prefix");
  const std::array<std::size_t, 6> slice =
      edge ? std::array<std::size_t, 6>{0, 10, 60, 2, 12, 62}
           : std::array<std::size_t, 6>{0, 10, 20, 1, 11, 21};
  constexpr std::size_t kLastSlice = Rank == 3 ? 140 : 350;
  const std::size_t second = edge ? kLastSlice : 70;
  std::array<bool, 420> expected{};
  for (std::size_t ordinal = 0; ordinal < displayed; ++ordinal) {
    expected[slice[ordinal % 6] + (ordinal >= 6 ? second : 0)] = true;
  }
  for (std::size_t byte = 0; byte < reads.size(); ++byte) {
    test.Check(reads[byte] == (expected[byte / sizeof(Value)] ? 1U : 0U),
               "higher rank selected bytes read once; omitted/padding bytes "
               "never read");
  }
  std::printf(
      "higher rank reads scalar_bytes=%zu complex=%d rank=%zu edge=%d "
      "metadata=%d limit=%zu displayed=%zu\n",
      sizeof(Value), static_cast<int>(!std::is_arithmetic_v<Value>), Rank,
      static_cast<int>(edge), static_cast<int>(metadata), limit, displayed);
}

template <typename Value>
void HigherRanks(TestContext& test) {
  for (const bool edge : {false, true}) {
    for (const bool metadata : {false, true}) {
      for (const std::size_t limit :
           {std::size_t{0}, std::size_t{9}, std::size_t{64}}) {
        HigherRankCase<Value, 3>(test, edge, metadata, limit);
        HigherRankCase<Value, 4>(test, edge, metadata, limit);
      }
    }
  }
}

template <typename Value>
void Scalar(TestContext& test) {
  Dense<Value>(test);
  Coordinate<Value>(test);
  Compressed<Value, asc::SparseCompressedFormat::kCsr>(test);
  Compressed<Value, asc::SparseCompressedFormat::kCsc>(test);
  HigherRanks<Value>(test);
}

}  // namespace

int main() {
  TestContext test;
  ObservationControl(test);
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
  std::puts(
      "load observation: twelve Dense/COO scalar types, zero Dense cap, "
      "all-width CSR/CSC offset bound; 288 higher-rank Dense cases");
  return test.Finish();
}
