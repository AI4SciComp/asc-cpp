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
#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/sparse/compressed.h"
#include "asc/sparse/coordinate.h"
#include "asc/sparse/print.h"
namespace {
namespace modes = asc_array_display_modes_test;
using modes::Sink;
using modes::Take;
using modes::TestContext;

template <typename Object>
auto Print(TestContext& test, const Object& object, Sink& sink,
           const asc::ArrayPrintOptions& options, std::span<std::byte> scratch,
           asc::ArrayPrintReport& report) {
  return modes::WithoutAllocation<asc_sparse_test::AllocationProbe>(test, [&] {
    return asc::PrintArray(object, sink, options, scratch, report);
  });
}
template <typename T, typename View>
void NumericView(TestContext& test, const View& view) {
  constexpr std::array<std::string_view, 5> kCoordinates{
      "(0,0) = ", "(1,1) = ", "(2,2) = ", "(3,3) = ", "(4,4) = "};
  std::array<std::byte, 1026> storage{};
  storage.front() = std::byte{91};
  storage.back() = std::byte{93};
  const auto scratch = std::span(storage).subspan(1, 1024);
  for (const auto& fixture : modes::kCases) {
    const auto tokens = modes::Tokens<T>(fixture);
    std::string expected;
    for (std::size_t i = 0; i < tokens.size(); ++i) {
      expected += kCoordinates[i];
      expected += tokens[i];
      expected += '\n';
    }
    asc::ArrayPrintOptions options;
    options.show_metadata = false;
    options.float_format = fixture.format;
    options.precision = fixture.precision;
    Sink sink;
    sink.chunk = 3;
    asc::ArrayPrintReport report{777, 888, true};
    test.Check(Print(test, view, sink, options, scratch, report).ok(),
               "every sparse precision/notation fixture must print");
    test.Check(sink.text() == expected,
               "independent literal coordinate/value spelling must match");
    test.Check(report.output_bytes == expected.size() &&
                   report.values_displayed == 5 && !report.truncated,
               "complete sparse numeric fixture must have exact progress");
    test.Check(
        storage.front() == std::byte{91} && storage.back() == std::byte{93},
        "sparse numeric formatting must preserve scratch redzones");
    asc_array_display_test::Balanced(test, sink.text());
  }
}
template <typename T>
void Numeric(TestContext& test) {
  const auto values = modes::Values<T>();
  const std::array<asc::index_t, 10> coordinates{0, 0, 1, 1, 2, 2, 3, 3, 4, 4};
  const std::array<asc::nnz_t, 6> offsets{0, 1, 2, 3, 4, 5};
  const std::array<asc::index_t, 5> indices{0, 1, 2, 3, 4};
  const std::array<asc::extent_t, 2> shape{5, 5};
  const auto coo = Take(asc::CoordinateView<const T, 2>::Create(
      coordinates.data(), values.data(), shape, 5, asc::MemorySpace::kHost));
  const auto csr = Take(asc::CsrView<const T>::Create(
      offsets.data(), indices.data(), values.data(), shape, 5,
      asc::MemorySpace::kHost));
  const auto csc = Take(asc::CscView<const T>::Create(
      offsets.data(), indices.data(), values.data(), shape, 5,
      asc::MemorySpace::kHost));
  NumericView<T>(test, coo);
  NumericView<T>(test, csr);
  NumericView<T>(test, csc);
}
template <typename T, typename View>
void RoundingView(TestContext& test, const View& view) {
  std::array<std::byte, 1024> scratch{};
  for (auto format :
       {asc::ArrayFloatFormat::kGeneral, asc::ArrayFloatFormat::kFixed,
        asc::ArrayFloatFormat::kScientific}) {
    for (int precision = 1; precision <= 32; ++precision) {
      const auto expected = modes::RoundingExpected<T>(format, precision, true);
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
template <typename T>
void Rounding(TestContext& test) {
  const auto values = modes::RoundingValues<T>();
  const std::array<asc::index_t, 8> coordinates{0, 0, 1, 1, 2, 2, 3, 3};
  const std::array<asc::nnz_t, 5> offsets{0, 1, 2, 3, 4};
  const std::array<asc::index_t, 4> indices{0, 1, 2, 3};
  const std::array<asc::extent_t, 2> shape{4, 4};
  const auto coo = Take(asc::CoordinateView<const T, 2>::Create(
      coordinates.data(), values.data(), shape, 4, asc::MemorySpace::kHost));
  const auto csr = Take(asc::CsrView<const T>::Create(
      offsets.data(), indices.data(), values.data(), shape, 4,
      asc::MemorySpace::kHost));
  const auto csc = Take(asc::CscView<const T>::Create(
      offsets.data(), indices.data(), values.data(), shape, 4,
      asc::MemorySpace::kHost));
  RoundingView<T>(test, coo);
  RoundingView<T>(test, csr);
  RoundingView<T>(test, csc);
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
  const auto view = Take(asc::CoordinateView<const std::int64_t, 0>::Create(
      nullptr, &value, std::array<asc::extent_t, 0>{}, 1,
      asc::MemorySpace::kHost));
  constexpr std::array<std::string_view, 1> kTokens{"3141592653589793238"};
  std::array<std::byte, 1024> scratch{};
  for (const bool metadata : {false, true}) {
    asc::ArrayPrintOptions options;
    options.show_metadata = metadata;
    const std::string_view expected =
        metadata
            ? "coo scalar=i64 shape=() stored=1\n() = 3141592653589793238\n"
            : "() = 3141592653589793238\n";
    const std::string_view omitted =
        metadata ? "coo scalar=i64 shape=() stored=1\n... (truncated)\n"
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

template <typename Object>
void Empty(TestContext& test, const Object& object) {
  std::array<std::byte, 1024> scratch{};
  asc::ArrayPrintOptions options;
  options.show_metadata = false;
  options.max_elements = 0;
  options.max_rows = 0;
  options.max_columns = 0;
  options.max_slices = 0;
  options.max_output_bytes = 3;
  Sink sink;
  asc::ArrayPrintReport report{777, 888, true};
  test.Check(Print(test, object, sink, options, scratch, report).ok(),
             "valid empty sparse shape must fit its exact three-byte display");
  test.Check(sink.text() == "[]\n" && report.output_bytes == 3 &&
                 report.values_displayed == 0 && !report.truncated,
             "empty sparse output is [], with no manufactured shape line or "
             "truncation");
}
template <std::size_t Rank>
void EmptyCoordinate(TestContext& test, std::array<asc::extent_t, Rank> shape) {
  const auto view = Take(asc::CoordinateView<const double, Rank>::Create(
      nullptr, nullptr, shape, 0, asc::MemorySpace::kHost));
  Empty(test, view);
}
template <asc::SparseCompressedFormat Format>
void EmptyCompressed(TestContext& test) {
  modes::Resource resource;
  const std::array<asc::nnz_t, 4> offsets{};
  for (const auto shape :
       {std::array<asc::extent_t, 2>{0, 0}, std::array<asc::extent_t, 2>{0, 3},
        std::array<asc::extent_t, 2>{3, 0},
        std::array<asc::extent_t, 2>{2, 3}}) {
    const auto view =
        Take(asc::CompressedSparseView<const double, Format>::Create(
            offsets.data(), nullptr, nullptr, shape, 0,
            asc::MemorySpace::kHost));
    const auto outer =
        Format == asc::SparseCompressedFormat::kCsr ? shape[0] : shape[1];
    const auto owner = Take(asc::CompressedSparseArray<double, Format>::Create(
        resource, shape,
        std::span(offsets).first(static_cast<std::size_t>(outer) + 1),
        std::span<const asc::index_t>{}, std::span<const double>{}));
    const auto attempts = resource.attempts();
    Empty(test, view);
    Empty(test, owner);
    test.Check(resource.attempts() == attempts,
               "empty printing must not allocate through its resource");
  }
}
void EmptyShapes(TestContext& test) {
  EmptyCoordinate(test, std::array<asc::extent_t, 0>{});
  EmptyCoordinate(test, std::array<asc::extent_t, 1>{0});
  EmptyCoordinate(test, std::array<asc::extent_t, 2>{0, 3});
  EmptyCoordinate(test, std::array<asc::extent_t, 2>{3, 0});
  EmptyCoordinate(test, std::array<asc::extent_t, 3>{2, 0, 3});
  EmptyCompressed<asc::SparseCompressedFormat::kCsr>(test);
  EmptyCompressed<asc::SparseCompressedFormat::kCsc>(test);
  modes::Resource resource;
  auto builder = Take(asc::CoordinateBuilder<int, asc::Extents<0>>::Create(
      resource, Take(asc::Extents<0>::Create()), 0));
  const auto owner = Take(std::move(builder).Finalize(
      asc::ExecutionContext::Serial(), asc::DuplicatePolicy::kReject,
      asc::ExplicitZeroPolicy::kKeep));
  const auto attempts = resource.attempts();
  Empty(test, owner);
  test.Check(resource.attempts() == attempts,
             "empty COO owner printing must not allocate");
}
template <typename Object>
void Aliases(TestContext& test, Object& object, std::string_view expected) {
  modes::ObjectAliases(test, object,
                       [&](Sink& sink, std::span<std::byte> scratch,
                           asc::ArrayPrintReport& report) {
                         return Print(test, object, sink, {}, scratch, report);
                       });
  std::array<std::byte, 1024> scratch{};
  Sink sink;
  asc::ArrayPrintOptions options;
  options.show_metadata = false;
  asc::ArrayPrintReport report;
  test.Check(
      Print(test, object, sink, options, scratch, report).ok() &&
          sink.text() == expected,
      "alias rejection must preserve usable sparse ownership/structure/values");
}
template <asc::SparseCompressedFormat Format>
void CompressedAliases(TestContext& test) {
  modes::Resource resource;
  const std::array<asc::nnz_t, 3> offsets{0, 1, 1};
  const std::array<asc::index_t, 1> indices{1};
  const std::array<int, 1> values{7};
  auto owner = Take(asc::CompressedSparseArray<int, Format>::Create(
      resource, std::array<asc::extent_t, 2>{2, 2}, offsets, indices, values));
  auto view = Take(owner.view());
  auto const_view = Take(std::as_const(owner).view());
  const auto attempts = resource.attempts();
  const std::string_view expected = Format == asc::SparseCompressedFormat::kCsr
                                        ? "(0,1) = 7\n"
                                        : "(1,0) = 7\n";
  Aliases(test, owner, expected);
  Aliases(test, view, expected);
  Aliases(test, const_view, expected);
  test.Check(resource.attempts() == attempts,
             "owner alias checks must not allocate");
}
void CoordinateAliases(TestContext& test) {
  modes::Resource resource;
  auto builder = Take(asc::CoordinateBuilder<int, asc::Extents<2>>::Create(
      resource, Take(asc::Extents<2>::Create()), 1));
  test.Check(builder.Add(std::array<asc::index_t, 1>{1}, 7).ok(),
             "valid fixture insertion");
  auto owner = Take(std::move(builder).Finalize(
      asc::ExecutionContext::Serial(), asc::DuplicatePolicy::kReject,
      asc::ExplicitZeroPolicy::kKeep));
  auto view = Take(owner.view());
  auto const_view = Take(std::as_const(owner).view());
  const auto attempts = resource.attempts();
  Aliases(test, owner, "(1) = 7\n");
  Aliases(test, view, "(1) = 7\n");
  Aliases(test, const_view, "(1) = 7\n");
  test.Check(resource.attempts() == attempts,
             "COO owner alias checks must not allocate");
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
      "2304 additional exact rounding/precision/locale profiles executed.");
  ScalarBudget(test);
  EmptyShapes(test);
  CoordinateAliases(test);
  CompressedAliases<asc::SparseCompressedFormat::kCsr>(test);
  CompressedAliases<asc::SparseCompressedFormat::kCsc>(test);
  std::puts(
      "Sparse modes: 144 precision/notation/locale fixtures; rank0 "
      "budgets/failing sinks with and without metadata; 22 empty owner/view "
      "cases; 27 live object aliases.");
  return test.Finish();
}
