#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "../array_display_modes_test_support.h"
#include "../array_display_test_support.h"
#include "allocation_probe.h"
#include "asc/core/array_format.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/dense/array.h"
#include "asc/dense/layout.h"
#include "asc/dense/print.h"
#include "asc/dense/view.h"
namespace {
namespace modes = asc_array_display_modes_test;
using modes::Sink;
using modes::Take;
using modes::TestContext;

template <typename Object>
auto Print(TestContext& test, const Object& object, Sink& sink,
           const asc::ArrayPrintOptions& options, std::span<std::byte> scratch,
           asc::ArrayPrintReport& report) {
  return modes::WithoutAllocation<asc_dense_test::AllocationProbe>(test, [&] {
    return asc::PrintArray(object, sink, options, scratch, report);
  });
}

template <typename T>
void Numeric(TestContext& test) {
  const auto values = modes::Values<T>();
  const auto mapping = Take(asc::DenseLayout<1>::Create(
      std::array<asc::extent_t, 1>{5}, asc::LayoutLeft{}));
  const auto view = Take(asc::DenseView<const T, 1>::Create(
      values.data(), mapping, asc::MemorySpace::kHost));
  std::array<std::byte, 1026> storage{};
  storage.front() = std::byte{91};
  storage.back() = std::byte{93};
  const auto scratch = std::span(storage).subspan(1, 1024);
  for (const auto& fixture : modes::kCases) {
    const auto tokens = modes::Tokens<T>(fixture);
    std::string expected = "[";
    for (std::size_t i = 0; i < tokens.size(); ++i) {
      if (i != 0) {
        expected += ", ";
      }
      expected += tokens[i];
    }
    expected += "]\n";
    asc::ArrayPrintOptions options;
    options.show_metadata = false;
    options.float_format = fixture.format;
    options.precision = fixture.precision;
    Sink sink;
    sink.chunk = 3;
    asc::ArrayPrintReport report{777, 888, true};
    test.Check(Print(test, view, sink, options, scratch, report).ok(),
               "all supported precision/mode fixtures must print");
    test.Check(sink.text() == expected,
               "independent literal real/complex spelling must match");
    test.Check(report.output_bytes == expected.size() &&
                   report.values_displayed == 5 && !report.truncated,
               "complete numeric fixture must retain exact output progress");
    test.Check(
        storage.front() == std::byte{91} && storage.back() == std::byte{93},
        "numeric formatting must preserve scratch redzones");
    asc_array_display_test::Balanced(test, sink.text());
  }
}
template <typename T>
void Rounding(TestContext& test) {
  const auto values = modes::RoundingValues<T>();
  const auto mapping = Take(asc::DenseLayout<1>::Create(
      std::array<asc::extent_t, 1>{4}, asc::LayoutLeft{}));
  const auto view = Take(asc::DenseView<const T, 1>::Create(
      values.data(), mapping, asc::MemorySpace::kHost));
  std::array<std::byte, 1024> scratch{};
  for (auto format :
       {asc::ArrayFloatFormat::kGeneral, asc::ArrayFloatFormat::kFixed,
        asc::ArrayFloatFormat::kScientific}) {
    for (int precision = 1; precision <= 32; ++precision) {
      const auto expected =
          modes::RoundingExpected<T>(format, precision, false);
      asc::ArrayPrintOptions options;
      options.show_metadata = false;
      options.float_format = format;
      options.precision = precision;
      Sink sink;
      sink.chunk = 3;
      asc::ArrayPrintReport report;
      test.Check(Print(test, view, sink, options, scratch, report).ok(),
                 "every valid precision must print exact dyadic boundaries");
      test.Check(sink.text() == expected,
                 "ties, exponent carry and component precision use independent "
                 "literals");
      test.Check(report.output_bytes == expected.size() &&
                     report.values_displayed == 4 && !report.truncated,
                 "rounding fixtures have complete bounded progress");
    }
  }
}
void AllNumeric(TestContext& test) {
  Numeric<float>(test);
  Numeric<double>(test);
  Numeric<std::complex<float>>(test);
  Numeric<std::complex<double>>(test);
  Rounding<float>(test);
  Rounding<double>(test);
  Rounding<std::complex<float>>(test);
  Rounding<std::complex<double>>(test);
}

void ScalarBudget(TestContext& test) {
  const std::int64_t value = 3141592653589793238;
  const auto mapping = Take(asc::DenseLayout<0>::Create(
      std::array<asc::extent_t, 0>{}, asc::LayoutLeft{}));
  const auto view = Take(asc::DenseView<const std::int64_t, 0>::Create(
      &value, mapping, asc::MemorySpace::kHost));
  constexpr std::array<std::string_view, 1> kTokens{"3141592653589793238"};
  std::array<std::byte, 1024> scratch{};
  for (const bool metadata : {false, true}) {
    asc::ArrayPrintOptions options;
    options.show_metadata = metadata;
    const std::string_view expected =
        metadata ? "dense scalar=i64 shape=() count=1\n3141592653589793238\n"
                 : "3141592653589793238\n";
    const std::string_view omitted =
        metadata ? "dense scalar=i64 shape=() count=1\n... (truncated)\n"
                 : "... (truncated)\n";
    asc_array_display_test::BudgetBoundaries(
        test,
        [&](Sink& sink, asc::ArrayPrintReport& report, std::size_t budget) {
          auto bounded = options;
          bounded.max_output_bytes = budget;
          auto status = Print(test, view, sink, bounded, scratch, report);
          modes::CheckRankZeroBudget(test, status, sink, report, budget,
                                     expected, omitted);
          return status;
        },
        kTokens, expected.size() + 64);
    asc_array_display_test::FailureBoundaries(
        test,
        [&](Sink& sink, asc::ArrayPrintReport& report) {
          auto status = Print(test, view, sink, options, scratch, report);
          modes::CheckRankZeroFailureTruncation(test, report, false, 0);
          return status;
        },
        expected, kTokens);
    constexpr std::string_view kMarker = "... (truncated)\n";
    const std::size_t header_bytes = omitted.size() - kMarker.size();
    asc_array_display_test::FailureBoundaries(
        test,
        [&](Sink& sink, asc::ArrayPrintReport& report) {
          auto bounded = options;
          bounded.max_output_bytes = omitted.size();
          auto status = Print(test, view, sink, bounded, scratch, report);
          modes::CheckRankZeroFailureTruncation(test, report, true,
                                                header_bytes);
          return status;
        },
        omitted, std::array<std::string_view, 0>{});
  }
}

template <std::size_t Rank, typename Layout>
void Empty(TestContext& test, std::array<asc::extent_t, Rank> shape,
           Layout layout, std::string_view expected) {
  const auto mapping = Take(asc::DenseLayout<Rank>::Create(shape, layout));
  const auto view = Take(asc::DenseView<const double, Rank>::Create(
      nullptr, mapping, asc::MemorySpace::kHost));
  std::array<std::byte, 1024> scratch{};
  asc::ArrayPrintOptions options;
  options.show_metadata = false;
  options.max_elements = 0;
  options.max_rows = 0;
  options.max_columns = 0;
  options.max_slices = 0;
  options.max_output_bytes = expected.size();
  Sink sink;
  asc::ArrayPrintReport report{777, 888, true};
  test.Check(Print(test, view, sink, options, scratch, report).ok(),
             "valid empty views must print at the exact shape-line byte limit");
  test.Check(sink.text() == expected &&
                 report.output_bytes == expected.size() &&
                 report.values_displayed == 0 && !report.truncated,
             "metadata-suppressed empty shape must remain visible, without "
             "truncation");
}
void EmptyShapes(TestContext& test) {
  const auto run = [&]<typename Layout>(Layout layout) {
    Empty(test, std::array<asc::extent_t, 1>{0}, layout, "empty shape=(0)\n");
    Empty(test, std::array<asc::extent_t, 2>{0, 3}, layout,
          "empty shape=(0,3)\n");
    Empty(test, std::array<asc::extent_t, 2>{3, 0}, layout,
          "empty shape=(3,0)\n");
    Empty(test, std::array<asc::extent_t, 3>{2, 0, 3}, layout,
          "empty shape=(2,0,3)\n");
    Empty(test, std::array<asc::extent_t, 4>{2, 3, 0, 4}, layout,
          "empty shape=(2,3,0,4)\n");
  };
  run(asc::LayoutLeft{});
  run(asc::LayoutRight{});
  modes::Resource resource;
  const auto shape = Take(asc::Extents<2, 0, 3>::Create());
  const auto left_owner = Take(
      asc::DenseArray<double, asc::Extents<2, 0, 3>>::Create(resource, shape));
  const auto right_owner =
      Take(asc::DenseArray<double, asc::Extents<2, 0, 3>>::Create(
          resource, shape, asc::LayoutRight{}));
  const auto attempts = resource.attempts();
  for (const auto* owner : {&left_owner, &right_owner}) {
    std::array<std::byte, 1024> scratch{};
    Sink sink;
    asc::ArrayPrintOptions options;
    options.show_metadata = false;
    options.max_elements = 0;
    asc::ArrayPrintReport report;
    test.Check(Print(test, *owner, sink, options, scratch, report).ok() &&
                   sink.text() == "empty shape=(2,0,3)\n" &&
                   report.values_displayed == 0 && !report.truncated,
               "empty owners must retain shape without metadata");
  }
  test.Check(resource.attempts() == attempts,
             "empty owner printing must not allocate");
}
void OwnerAndViewAliases(TestContext& test) {
  modes::Resource resource;
  auto owner = Take(asc::DenseArray<double, asc::Extents<2>>::Create(
      resource, Take(asc::Extents<2>::Create())));
  auto view = Take(owner.view());
  auto const_view = Take(std::as_const(owner).view());
  const auto attempts = resource.attempts();
  const auto check = [&]<typename Object>(Object& object) {
    modes::ObjectAliases(test, object,
                         [&](Sink& sink, std::span<std::byte> scratch,
                             asc::ArrayPrintReport& report) {
                           return Print(test, object, sink, {}, scratch,
                                        report);
                         });
    std::array<std::byte, 1024> scratch{};
    Sink sink;
    asc::ArrayPrintOptions options;
    options.show_metadata = false;
    asc::ArrayPrintReport report;
    test.Check(Print(test, object, sink, options, scratch, report).ok() &&
                   sink.text() == "[0, 0]\n",
               "rejected scratch aliases must leave the owner/view usable");
  };
  check(owner);
  check(view);
  check(const_view);
  test.Check(resource.attempts() == attempts,
             "printing and alias rejection must not allocate through the owner "
             "resource");
}
}  // namespace
int main() {
  TestContext test;
  AllNumeric(test);
  {
    const modes::LocaleGuard locale_guard;
    modes::LocaleControl(test);
    AllNumeric(test);
  }
  std::puts(
      "768 additional exact rounding/precision/locale profiles executed.");
  ScalarBudget(test);
  EmptyShapes(test);
  OwnerAndViewAliases(test);
  std::puts(
      "Dense modes: 48 precision/notation/locale fixtures; rank0 "
      "budgets/failing sinks with and without metadata; 12 empty owner/view "
      "cases; 9 "
      "live object aliases.");
  return test.Finish();
}
