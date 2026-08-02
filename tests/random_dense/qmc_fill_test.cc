#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <span>
#include <vector>

#include "asc/core/execution.h"
#include "asc/core/types.h"
#include "asc/dense/layout.h"
#include "asc/dense/view.h"
#include "asc/random/dense.h"
#include "asc/random/engine.h"
#include "asc/random/quasi.h"
#include "test_support.h"

namespace {

using asc_random_dense_test::TestContext;

template <typename Element, std::size_t Rank, typename Layout>
asc::Result<asc::DenseView<Element, Rank>> MakeView(
    Element* data, const std::array<asc::extent_t, Rank>& extents,
    Layout layout) {
  auto mapping = asc::DenseLayout<Rank>::Create(extents, layout);
  if (!mapping.ok()) {
    return mapping.status();
  }
  return asc::DenseView<Element, Rank>::Create(data, *mapping,
                                               asc::MemorySpace::kHost);
}

template <typename Real>
void CheckLogical(TestContext& test, asc::DenseView<Real, 2> view,
                  std::span<const Real> expected) {
  const auto samples = static_cast<std::size_t>(view.extents()[0]);
  const auto dimensions = static_cast<std::size_t>(view.extents()[1]);
  for (std::size_t sample = 0; sample < samples; ++sample) {
    for (std::size_t dimension = 0; dimension < dimensions; ++dimension) {
      const std::size_t physical =
          sample * static_cast<std::size_t>(view.strides()[0]) +
          dimension * static_cast<std::size_t>(view.strides()[1]);
      ASC_RANDOM_DENSE_TEST_EQ(
          test, std::bit_cast<std::uint64_t>(view.data()[physical]),
          std::bit_cast<std::uint64_t>(
              expected[sample * dimensions + dimension]));
    }
  }
}

void CheckLatin(TestContext& test) {
  constexpr std::size_t kSamples = 8;
  constexpr std::size_t kDimensions = 3;
  constexpr std::array<asc::extent_t, 2> kExtents{kSamples, kDimensions};
  constexpr std::array<asc::stride_t, 2> kStrides{1, 12};
  std::array<double, 32> output{};
  output.fill(-1.0);
  std::array<std::uint32_t, 32> workspace{};
  auto output_view = MakeView(output.data(), kExtents,
                              asc::LayoutStride<2>{.strides = kStrides});
  auto workspace_view = MakeView(workspace.data(), kExtents,
                                 asc::LayoutStride<2>{.strides = kStrides});
  ASC_RANDOM_DENSE_TEST_CHECK(test, output_view.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(test, workspace_view.ok());
  if (!output_view.ok() || !workspace_view.ok()) {
    return;
  }

  asc::Pcg32 dense_engine(41, 7);
  asc::Pcg32 scalar_engine = dense_engine;
  std::array<std::uint32_t, kSamples * kDimensions> scalar_workspace{};
  std::array<double, kSamples * kDimensions> scalar_output{};
  const auto dense_midpoints = asc::FillDenseLatinHypercubeMidpoints(
      asc::ExecutionContext::Serial(), *output_view, dense_engine,
      *workspace_view);
  const auto scalar_midpoints = asc::GenerateLatinHypercubeMidpoints(
      kSamples, kDimensions, scalar_engine,
      std::span<std::uint32_t>(scalar_workspace),
      std::span<double>(scalar_output));
  ASC_RANDOM_DENSE_TEST_CHECK(test, dense_midpoints.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(test, scalar_midpoints.ok());
  CheckLogical(test, *output_view, std::span<const double>(scalar_output));
  ASC_RANDOM_DENSE_TEST_EQ(test, dense_engine.ExportState(),
                           scalar_engine.ExportState());
  for (std::size_t hole :
       {std::size_t{8}, std::size_t{9}, std::size_t{10}, std::size_t{11},
        std::size_t{20}, std::size_t{21}, std::size_t{22}, std::size_t{23}}) {
    ASC_RANDOM_DENSE_TEST_EQ(test, output[hole], -1.0);
  }

  output.fill(-2.0);
  dense_engine = asc::Pcg32(43, 11);
  scalar_engine = dense_engine;
  const auto dense_jitter = asc::FillDenseLatinHypercubeJittered(
      asc::ExecutionContext::Serial(), *output_view, dense_engine,
      *workspace_view);
  const auto scalar_jitter = asc::GenerateLatinHypercubeJittered(
      kSamples, kDimensions, scalar_engine,
      std::span<std::uint32_t>(scalar_workspace),
      std::span<double>(scalar_output));
  ASC_RANDOM_DENSE_TEST_CHECK(test, dense_jitter.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(test, scalar_jitter.ok());
  CheckLogical(test, *output_view, std::span<const double>(scalar_output));
  ASC_RANDOM_DENSE_TEST_EQ(test, dense_engine.ExportState(),
                           scalar_engine.ExportState());

  for (std::size_t dimension = 0; dimension < kDimensions; ++dimension) {
    std::array<bool, kSamples> occupied{};
    for (std::size_t sample = 0; sample < kSamples; ++sample) {
      const std::size_t physical = sample + 12 * dimension;
      const auto stratum =
          static_cast<std::size_t>(output[physical] * kSamples);
      ASC_RANDOM_DENSE_TEST_CHECK(test, stratum < kSamples);
      if (stratum < kSamples) {
        ASC_RANDOM_DENSE_TEST_CHECK(test, !occupied[stratum]);
        occupied[stratum] = true;
      }
    }
  }
}

template <typename Fill>
void CheckIndexedFill(TestContext& test, Fill fill,
                      void (*generate_point)(std::uint64_t,
                                             std::span<double>)) {
  constexpr std::size_t kSamples = 4;
  constexpr std::size_t kDimensions = 3;
  constexpr std::array<asc::extent_t, 2> kExtents{kSamples, kDimensions};
  constexpr std::array<asc::stride_t, 2> kStrides{1, 6};
  constexpr std::array<asc::extent_t, 1> kWorkspaceExtents{kDimensions};
  constexpr std::array<asc::stride_t, 1> kWorkspaceStrides{2};
  std::array<double, 16> output{};
  output.fill(-3.0);
  std::array<double, 5> workspace{};
  auto output_view = MakeView(output.data(), kExtents,
                              asc::LayoutStride<2>{.strides = kStrides});
  auto workspace_view =
      MakeView(workspace.data(), kWorkspaceExtents,
               asc::LayoutStride<1>{.strides = kWorkspaceStrides});
  ASC_RANDOM_DENSE_TEST_CHECK(test, output_view.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(test, workspace_view.ok());
  if (!output_view.ok() || !workspace_view.ok()) {
    return;
  }
  const auto status = fill(*output_view, *workspace_view);
  ASC_RANDOM_DENSE_TEST_CHECK(test, status.ok());
  std::array<double, kSamples * kDimensions> expected{};
  for (std::size_t sample = 0; sample < kSamples; ++sample) {
    generate_point(5 + sample, std::span<double>(expected).subspan(
                                   sample * kDimensions, kDimensions));
  }
  CheckLogical(test, *output_view, std::span<const double>(expected));
  for (std::size_t hole :
       {std::size_t{4}, std::size_t{5}, std::size_t{10}, std::size_t{11}}) {
    ASC_RANDOM_DENSE_TEST_EQ(test, output[hole], -3.0);
  }
}

void GenerateHalton(std::uint64_t index, std::span<double> output) {
  const auto status = asc::GenerateHaltonPoint(index, output);
  if (!status.ok()) {
    std::abort();
  }
}

void GenerateSobol(std::uint64_t index, std::span<double> output) {
  const auto status = asc::GenerateSobolPoint(index, output);
  if (!status.ok()) {
    std::abort();
  }
}

void CheckHaltonAndSobol(TestContext& test) {
  CheckIndexedFill(
      test,
      [](asc::DenseView<double, 2> output,
         asc::DenseView<double, 1> workspace) {
        return asc::FillDenseHalton(asc::ExecutionContext::Serial(), output, 5,
                                    workspace);
      },
      GenerateHalton);
  CheckIndexedFill(
      test,
      [](asc::DenseView<double, 2> output,
         asc::DenseView<double, 1> workspace) {
        return asc::FillDenseSobol(asc::ExecutionContext::Serial(), output, 5,
                                   workspace);
      },
      GenerateSobol);
}

void CheckHammersleyAndScrambling(TestContext& test) {
  constexpr std::array<asc::extent_t, 2> kExtents{3, 3};
  constexpr std::array<asc::extent_t, 1> kWorkspaceExtents{3};
  std::array<double, 9> output{};
  std::array<double, 3> workspace{};
  auto output_view = MakeView(output.data(), kExtents, asc::LayoutRight{});
  auto workspace_view =
      MakeView(workspace.data(), kWorkspaceExtents, asc::LayoutLeft{});
  ASC_RANDOM_DENSE_TEST_CHECK(test, output_view.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(test, workspace_view.ok());
  if (!output_view.ok() || !workspace_view.ok()) {
    return;
  }

  auto status = asc::FillDenseHammersley(asc::ExecutionContext::Serial(),
                                         *output_view, 2, 8, *workspace_view);
  ASC_RANDOM_DENSE_TEST_CHECK(test, status.ok());
  std::array<double, 3> expected{};
  for (std::size_t sample = 0; sample < 3; ++sample) {
    status = asc::GenerateHammersleyPoint(2 + sample, 8,
                                          std::span<double>(expected));
    ASC_RANDOM_DENSE_TEST_CHECK(test, status.ok());
    for (std::size_t dimension = 0; dimension < 3; ++dimension) {
      ASC_RANDOM_DENSE_TEST_EQ(
          test, std::bit_cast<std::uint64_t>(output[sample * 3 + dimension]),
          std::bit_cast<std::uint64_t>(expected[dimension]));
    }
  }

  const std::array<std::uint32_t, 2> base_two{0, 1};
  const std::array<std::uint32_t, 3> base_three{0, 2, 1};
  const std::array<std::uint32_t, 5> base_five{0, 2, 4, 1, 3};
  const std::array<std::span<const std::uint32_t>, 3> permutations{
      base_two, base_three, base_five};
  status = asc::FillDenseScrambledHalton(asc::ExecutionContext::Serial(),
                                         *output_view, 3, permutations,
                                         *workspace_view);
  ASC_RANDOM_DENSE_TEST_CHECK(test, status.ok());
  for (std::size_t sample = 0; sample < 3; ++sample) {
    status = asc::GenerateScrambledHaltonPoint(
        3 + sample,
        std::span<const std::span<const std::uint32_t>>(permutations),
        std::span<double>(expected));
    ASC_RANDOM_DENSE_TEST_CHECK(test, status.ok());
    for (std::size_t dimension = 0; dimension < 3; ++dimension) {
      ASC_RANDOM_DENSE_TEST_EQ(
          test, std::bit_cast<std::uint64_t>(output[sample * 3 + dimension]),
          std::bit_cast<std::uint64_t>(expected[dimension]));
    }
  }
}

void CheckFailures(TestContext& test) {
  constexpr std::array<asc::extent_t, 2> kExtents{2, 2};
  constexpr std::array<asc::extent_t, 1> kWorkspaceExtents{2};
  std::array<double, 4> output{};
  output.fill(17.0);
  std::array<double, 2> workspace{};
  auto output_view = MakeView(output.data(), kExtents, asc::LayoutRight{});
  auto workspace_view =
      MakeView(workspace.data(), kWorkspaceExtents, asc::LayoutLeft{});
  const auto overflow = asc::FillDenseSobol(
      asc::ExecutionContext::Serial(), *output_view,
      std::numeric_limits<std::uint64_t>::max(), *workspace_view);
  ASC_RANDOM_DENSE_TEST_CHECK(test, !overflow.ok());
  ASC_RANDOM_DENSE_TEST_EQ(test, overflow.code(), asc::ErrorCode::kOverflow);
  for (double value : output) {
    ASC_RANDOM_DENSE_TEST_EQ(test, value, 17.0);
  }

  const auto bad_total = asc::FillDenseHammersley(
      asc::ExecutionContext::Serial(), *output_view, 0, 0, *workspace_view);
  ASC_RANDOM_DENSE_TEST_CHECK(test, !bad_total.ok());
  ASC_RANDOM_DENSE_TEST_EQ(test, bad_total.code(),
                           asc::ErrorCode::kInvalidArgument);

  auto alias_workspace =
      MakeView(output.data(), kWorkspaceExtents, asc::LayoutLeft{});
  const auto alias = asc::FillDenseHalton(asc::ExecutionContext::Serial(),
                                          *output_view, 0, *alias_workspace);
  ASC_RANDOM_DENSE_TEST_CHECK(test, !alias.ok());
  ASC_RANDOM_DENSE_TEST_EQ(test, alias.code(),
                           asc::ErrorCode::kInvalidArgument);
}

}  // namespace

int main() {
  TestContext test;
  CheckLatin(test);
  CheckHaltonAndSobol(test);
  CheckHammersleyAndScrambling(test);
  CheckFailures(test);
  return test.Finish();
}
