#include <array>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <span>
#include <string_view>

#include "../array_display_test_support.h"
#include "allocation_observation.h"
#include "allocation_probe.h"
#include "asc/core/array_format.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/layout.h"
#include "asc/dense/print.h"
#include "asc/dense/view.h"

namespace {

using asc_array_display_test::Sink;
using asc_array_display_test::TestContext;

constexpr std::array<std::string_view, 16> kTokens{
    "100", "101", "102", "103", "104", "105", "106", "107",
    "108", "109", "110", "111", "112", "113", "114", "115"};
constexpr std::string_view kSlices =
    "slice axes=(0,1) fixed=(2:0,3:0)\n[[100, 102],\n [101, 103]]\n"
    "slice axes=(0,1) fixed=(2:1,3:0)\n[[104, 106],\n [105, 107]]\n"
    "slice axes=(0,1) fixed=(2:0,3:1)\n[[108, 110],\n [109, 111]]\n"
    "slice axes=(0,1) fixed=(2:1,3:1)\n[[112, 114],\n [113, 115]]\n";

void Slices(TestContext& test) {
  std::array<int, 16> left{};
  std::array<int, 16> right{};
  for (std::size_t r = 0; r < 2; ++r) {
    for (std::size_t c = 0; c < 2; ++c) {
      for (std::size_t k = 0; k < 2; ++k) {
        for (std::size_t l = 0; l < 2; ++l) {
          const std::size_t position = r + 2 * c + 4 * k + 8 * l;
          left[position] = static_cast<int>(100 + position);
          right[((r * 2 + c) * 2 + k) * 2 + l] = left[position];
        }
      }
    }
  }
  const auto left_mapping = asc::DenseLayout<4>::Create(
      std::array<asc::extent_t, 4>{2, 2, 2, 2}, asc::LayoutLeft{});
  const auto right_mapping = asc::DenseLayout<4>::Create(
      std::array<asc::extent_t, 4>{2, 2, 2, 2}, asc::LayoutRight{});
  const auto left_view = asc::DenseView<const int, 4>::Create(
      left.data(), *left_mapping, asc::MemorySpace::kHost);
  const auto right_view = asc::DenseView<const int, 4>::Create(
      right.data(), *right_mapping, asc::MemorySpace::kHost);
  std::array<std::byte, 1024> scratch{};
  asc::ArrayPrintOptions options;
  options.show_metadata = false;
  for (const auto& view : {*left_view, *right_view}) {
    Sink sink;
    asc::ArrayPrintReport report;
    test.Check(asc::PrintArray(view, sink, options, scratch, report).ok(),
               "rank-four display must succeed for either physical layout");
    test.Check(sink.text() == kSlices && report.values_displayed == 16 &&
                   !report.truncated,
               "rank-four slices must enumerate dimension two fastest");
    asc_array_display_test::FailureBoundaries(
        test,
        [&](Sink& output, asc::ArrayPrintReport& progress) {
          return asc::PrintArray(view, output, options, scratch, progress);
        },
        kSlices, kTokens);
    asc_array_display_test::BudgetBoundaries(
        test,
        [&](Sink& output, asc::ArrayPrintReport& progress, std::size_t budget) {
          auto bounded = options;
          bounded.max_output_bytes = budget;
          return asc::PrintArray(view, output, bounded, scratch, progress);
        },
        kTokens, kSlices.size() + 64);
  }
  std::puts(
      "display dense rank4 both layouts: ordering, byte budgets, failure "
      "progress");
}

void CombinedPreview(TestContext& test) {
  std::array<int, 81> values{};
  for (std::size_t i = 0; i < values.size(); ++i) {
    values[i] = static_cast<int>(100 + i);
  }
  const auto mapping = asc::DenseLayout<4>::Create(
      std::array<asc::extent_t, 4>{3, 3, 3, 3}, asc::LayoutLeft{});
  const auto view = asc::DenseView<const int, 4>::Create(
      values.data(), *mapping, asc::MemorySpace::kHost);
  std::array<std::byte, 1024> scratch{};
  asc::ArrayPrintOptions options;
  options.show_metadata = false;
  options.max_rows = 2;
  options.max_columns = 2;
  options.max_slices = 3;
  options.max_elements = 9;
  asc::ArrayPrintReport report;
  Sink edge;
  test.Check(asc::PrintArray(*view, edge, options, scratch, report).ok(),
             "combined edge preview must succeed");
  test.Check(
      edge.text() ==
          "slice axes=(0,1) fixed=(2:0,3:0)\n"
          "[[100, ..., 106],\n ...,\n [102, ..., 108]]\n"
          "slice axes=(0,1) fixed=(2:1,3:0)\n"
          "[[109, ..., 115],\n ...,\n [111, ..., 117]]\n"
          "...\nslice axes=(0,1) fixed=(2:2,3:2)\n"
          "[[172, ...],\n ...]\n... (truncated)\n",
      "independent odd slice selection must precede the global value cap");
  test.Check(report.values_displayed == 9 && report.truncated,
             "combined preview must report the exact global cap");
  options.edge_preview = false;
  Sink prefix;
  test.Check(asc::PrintArray(*view, prefix, options, scratch, report).ok(),
             "combined prefix preview must succeed");
  test.Check(prefix.text() ==
                 "slice axes=(0,1) fixed=(2:0,3:0)\n"
                 "[[100, 103, ...],\n [101, 104, ...],\n ...]\n"
                 "slice axes=(0,1) fixed=(2:1,3:0)\n"
                 "[[109, 112, ...],\n [110, 113, ...],\n ...]\n"
                 "slice axes=(0,1) fixed=(2:2,3:0)\n"
                 "[[118, ...],\n ...]\n... (truncated)\n",
             "prefix selection must mark omitted row, column and value tails");
  std::puts("display dense combined preview: odd edges, prefix, global cap");
}

void ZeroLimits(TestContext& test) {
  const std::array<int, 8> values{100, 101, 102, 103, 104, 105, 106, 107};
  const auto mapping = asc::DenseLayout<3>::Create(
      std::array<asc::extent_t, 3>{2, 2, 2}, asc::LayoutLeft{});
  const auto view = asc::DenseView<const int, 3>::Create(
      values.data(), *mapping, asc::MemorySpace::kHost);
  std::array<std::byte, 1024> scratch{};
  constexpr std::array<std::size_t asc::ArrayPrintOptions::*, 4> kLimits{
      &asc::ArrayPrintOptions::max_elements, &asc::ArrayPrintOptions::max_rows,
      &asc::ArrayPrintOptions::max_columns,
      &asc::ArrayPrintOptions::max_slices};
  for (const auto limit : kLimits) {
    asc::ArrayPrintOptions options;
    options.show_metadata = false;
    options.*limit = 0;
    Sink sink;
    asc::ArrayPrintReport report{777, 888, false};
    test.Check(asc::PrintArray(*view, sink, options, scratch, report).ok(),
               "each zero preview limit must permit visible truncation");
    test.Check(report.values_displayed == 0 && report.truncated &&
                   sink.text().ends_with("... (truncated)\n"),
               "each zero preview limit must suppress all selected values");
    asc_array_display_test::Balanced(test, sink.text());
  }
  std::puts("display dense zero limits: elements, rows, columns, slices");
}

void ComplexProgressAndInvalidSink(TestContext& test) {
  const std::array<std::complex<double>, 2> values{
      std::complex<double>{101.25, -202.5}, {303.75, 404.5}};
  const auto mapping = asc::DenseLayout<1>::Create(
      std::array<asc::extent_t, 1>{2}, asc::LayoutLeft{});
  const auto view = asc::DenseView<const std::complex<double>, 1>::Create(
      values.data(), *mapping, asc::MemorySpace::kHost);
  std::array<std::byte, 1024> scratch{};
  asc::ArrayPrintOptions options;
  options.show_metadata = false;
  constexpr std::array<std::string_view, 2> kComplexTokens{"(101.25,-202.5)",
                                                           "(303.75,404.5)"};
  asc_array_display_test::FailureBoundaries(
      test,
      [&](Sink& output, asc::ArrayPrintReport& report) {
        return asc::PrintArray(*view, output, options, scratch, report);
      },
      "[(101.25,-202.5), (303.75,404.5)]\n", kComplexTokens);
  for (const bool invalid : {false, true}) {
    Sink sink;
    sink.invalid_progress = invalid;
    sink.zero_progress = !invalid;
    asc::ArrayPrintReport report{777, 888, true};
    asc::Status status;
    std::size_t allocations = 0;
    {
      asc_dense_test::AllocationProbe probe;
      status = asc::PrintArray(*view, sink, options, scratch, report);
      allocations = probe.count();
    }
    test.Check(
        status.code() == asc::ErrorCode::kIo && sink.text().empty(),
        "invalid or zero sink progress must fail without accepted bytes");
    test.Check(report.output_bytes == 0 && report.values_displayed == 0,
               "runtime sink failure must report no accepted bytes or tokens");
    test.Check(asc_test::ProcessAllocationCountMatches(allocations, 0),
               "invalid sink progress must not allocate");
  }
  std::puts("display dense complex tokens: failure progress and invalid sinks");
}

}  // namespace

int main() {
  TestContext test;
  Slices(test);
  CombinedPreview(test);
  ZeroLimits(test);
  ComplexProgressAndInvalidSink(test);
  return test.Finish();
}
