#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <span>
#include <string_view>
#include <vector>

#include "asc/core/execution.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/layout.h"
#include "asc/dense/view.h"
#include "asc/random/dense.h"
#include "asc/random/distribution.h"
#include "asc/random/engine.h"
#include "asc/random/generator.h"
#include "test_support.h"

namespace {

using asc_random_dense_test::TestContext;

void CheckNear(TestContext& test, double actual, double expected,
               double tolerance, std::string_view label) {
  if (std::abs(actual - expected) > tolerance) {
    std::cerr << std::setprecision(17) << label << ": actual=" << actual
              << " expected=" << expected << " tolerance=" << tolerance << '\n';
    ASC_RANDOM_DENSE_TEST_CHECK(test, false);
  }
}

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

class CountingGenerator {
 public:
  explicit CountingGenerator(
      std::size_t fail_at = std::numeric_limits<std::size_t>::max())
      : fail_at_(fail_at) {}

  asc::Result<double> operator()() {
    if (calls_ == fail_at_) {
      ++calls_;
      return asc::Status(asc::ErrorCode::kNumerical,
                         "intentional generator failure");
    }
    return static_cast<double>(++calls_);
  }

  [[nodiscard]] std::size_t calls() const noexcept { return calls_; }

 private:
  std::size_t calls_ = 0;
  std::size_t fail_at_;
};

class WordSequenceEngine {
 public:
  using result_type = std::uint32_t;

  [[nodiscard]] static constexpr result_type min() noexcept { return 0; }
  [[nodiscard]] static constexpr result_type max() noexcept {
    return std::numeric_limits<result_type>::max();
  }
  [[nodiscard]] result_type operator()() noexcept {
    ++calls_;
    return calls_ == 1 ? 0U : UINT32_C(0x80000000);
  }
  [[nodiscard]] std::size_t calls() const noexcept { return calls_; }

 private:
  std::size_t calls_ = 0;
};

class ZeroEngine {
 public:
  using result_type = std::uint32_t;

  [[nodiscard]] static constexpr result_type min() noexcept { return 0; }
  [[nodiscard]] static constexpr result_type max() noexcept {
    return std::numeric_limits<result_type>::max();
  }
  [[nodiscard]] result_type operator()() noexcept {
    ++calls_;
    return 0;
  }
  [[nodiscard]] std::size_t calls() const noexcept { return calls_; }

 private:
  std::size_t calls_ = 0;
};

void CheckPseudoFill(TestContext& test) {
  constexpr std::array<asc::extent_t, 2> kExtents{2, 3};
  constexpr std::array<asc::stride_t, 2> kStrides{1, 4};
  std::array<double, 10> storage{};
  storage.fill(-1.0);
  auto view = MakeView(storage.data(), kExtents,
                       asc::LayoutStride<2>{.strides = kStrides});
  ASC_RANDOM_DENSE_TEST_CHECK(test, view.ok());
  if (!view.ok()) {
    return;
  }
  CountingGenerator generator;
  const auto status =
      asc::FillDensePseudo(asc::ExecutionContext::Serial(), *view, generator);
  ASC_RANDOM_DENSE_TEST_CHECK(test, status.ok());
  ASC_RANDOM_DENSE_TEST_EQ(test, generator.calls(), std::size_t{6});
  ASC_RANDOM_DENSE_TEST_EQ(test, storage[0], 1.0);
  ASC_RANDOM_DENSE_TEST_EQ(test, storage[1], 2.0);
  ASC_RANDOM_DENSE_TEST_EQ(test, storage[4], 3.0);
  ASC_RANDOM_DENSE_TEST_EQ(test, storage[5], 4.0);
  ASC_RANDOM_DENSE_TEST_EQ(test, storage[8], 5.0);
  ASC_RANDOM_DENSE_TEST_EQ(test, storage[9], 6.0);
  for (std::size_t hole :
       {std::size_t{2}, std::size_t{3}, std::size_t{6}, std::size_t{7}}) {
    ASC_RANDOM_DENSE_TEST_EQ(test, storage[hole], -1.0);
  }

  storage.fill(-7.0);
  CountingGenerator failing(2);
  const auto failed =
      asc::FillDensePseudo(asc::ExecutionContext::Serial(), *view, failing);
  ASC_RANDOM_DENSE_TEST_CHECK(test, !failed.ok());
  ASC_RANDOM_DENSE_TEST_EQ(test, failed.code(), asc::ErrorCode::kNumerical);
  ASC_RANDOM_DENSE_TEST_EQ(test, storage[0], 1.0);
  ASC_RANDOM_DENSE_TEST_EQ(test, storage[1], 2.0);
  ASC_RANDOM_DENSE_TEST_EQ(test, storage[4], -7.0);
}

struct MultivariateFixture {
  std::array<double, 3> mean{1.0, -2.0, 0.5};
  std::array<double, 9> covariance{1.0,  0.3,  -0.2, 0.3, 1.25,
                                   0.25, -0.2, 0.25, 0.75};
  std::array<double, 9> factor{};
};

bool PrepareFixture(MultivariateFixture& fixture,
                    asc::DenseView<const double, 1>& mean_view,
                    asc::DenseView<const double, 2>& covariance_view,
                    asc::DenseView<double, 2>& factor_view, TestContext& test) {
  constexpr std::array<asc::extent_t, 1> kMeanExtents{3};
  constexpr std::array<asc::extent_t, 2> kMatrixExtents{3, 3};
  auto mutable_mean =
      MakeView(fixture.mean.data(), kMeanExtents, asc::LayoutLeft{});
  auto mutable_covariance =
      MakeView(fixture.covariance.data(), kMatrixExtents, asc::LayoutRight{});
  auto factor =
      MakeView(fixture.factor.data(), kMatrixExtents, asc::LayoutRight{});
  ASC_RANDOM_DENSE_TEST_CHECK(test, mutable_mean.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(test, mutable_covariance.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(test, factor.ok());
  if (!mutable_mean.ok() || !mutable_covariance.ok() || !factor.ok()) {
    return false;
  }
  mean_view = *mutable_mean;
  covariance_view = *mutable_covariance;
  factor_view = *factor;
  const auto prepared = asc::PrepareDenseMultivariateNormal(
      asc::ExecutionContext::Serial(), mean_view, covariance_view, factor_view);
  ASC_RANDOM_DENSE_TEST_CHECK(test, prepared.ok());
  return prepared.ok();
}

void CheckPreparationAndKnownAnswer(TestContext& test) {
  constexpr std::array<asc::extent_t, 1> kMeanExtents{2};
  constexpr std::array<asc::extent_t, 2> kMatrixExtents{2, 2};
  std::array<double, 2> mean_data{2.0, -1.0};
  std::array<double, 4> covariance_data{4.0, 2.0, 2.0, 3.0};
  std::array<double, 4> factor_data{-9.0, -9.0, -9.0, -9.0};
  auto mutable_mean =
      MakeView(mean_data.data(), kMeanExtents, asc::LayoutLeft{});
  auto mutable_covariance =
      MakeView(covariance_data.data(), kMatrixExtents, asc::LayoutRight{});
  auto factor =
      MakeView(factor_data.data(), kMatrixExtents, asc::LayoutRight{});
  ASC_RANDOM_DENSE_TEST_CHECK(test, mutable_mean.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(test, mutable_covariance.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(test, factor.ok());
  if (!mutable_mean.ok() || !mutable_covariance.ok() || !factor.ok()) {
    return;
  }
  asc::DenseView<const double, 1> mean = *mutable_mean;
  asc::DenseView<const double, 2> covariance = *mutable_covariance;
  auto prepared = asc::PrepareDenseMultivariateNormal(
      asc::ExecutionContext::Serial(), mean, covariance, *factor);
  ASC_RANDOM_DENSE_TEST_CHECK(test, prepared.ok());
  CheckNear(test, factor_data[0], 2.0, 1.0e-15, "factor(0,0)");
  CheckNear(test, factor_data[1], 0.0, 0.0, "factor(0,1)");
  CheckNear(test, factor_data[2], 1.0, 1.0e-15, "factor(1,0)");
  CheckNear(test, factor_data[3], std::sqrt(2.0), 1.0e-15, "factor(1,1)");

  constexpr std::array<asc::extent_t, 2> kOutputExtents{1, 2};
  constexpr std::array<asc::extent_t, 1> kWorkspaceExtents{2};
  std::array<double, 2> output{-8.0, -8.0};
  std::array<double, 2> workspace{};
  auto output_view =
      MakeView(output.data(), kOutputExtents, asc::LayoutRight{});
  auto workspace_view =
      MakeView(workspace.data(), kWorkspaceExtents, asc::LayoutLeft{});
  ASC_RANDOM_DENSE_TEST_CHECK(test, output_view.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(test, workspace_view.ok());
  auto normal = asc::NormalDistribution<double>::Create(0.0, 1.0);
  asc::NormalGenerator<asc::Pcg32, double> generator(asc::Pcg32(42, 54),
                                                     *normal);
  if (output_view.ok() && workspace_view.ok()) {
    const auto sampled = asc::FillDenseMultivariateNormal(
        asc::ExecutionContext::Serial(), *output_view, mean,
        asc::DenseView<const double, 2>(*factor), generator, *workspace_view);
    ASC_RANDOM_DENSE_TEST_CHECK(test, sampled.ok());
    CheckNear(test, output[0], 1.5938183188431687, 1.0e-15,
              "known multivariate sample x0");
    CheckNear(test, output[1], -1.2160147831927506, 1.0e-15,
              "known multivariate sample x1");
  }

  covariance_data = {1.0, 2.0, 2.0, 1.0};
  factor_data.fill(17.0);
  const auto indefinite = asc::PrepareDenseMultivariateNormal(
      asc::ExecutionContext::Serial(), mean, covariance, *factor);
  ASC_RANDOM_DENSE_TEST_CHECK(test, !indefinite.ok());
  ASC_RANDOM_DENSE_TEST_EQ(test, indefinite.code(), asc::ErrorCode::kNumerical);

  covariance_data = {1.0, 0.25, 0.5, 1.0};
  factor_data.fill(19.0);
  const auto asymmetric = asc::PrepareDenseMultivariateNormal(
      asc::ExecutionContext::Serial(), mean, covariance, *factor);
  ASC_RANDOM_DENSE_TEST_CHECK(test, !asymmetric.ok());
  ASC_RANDOM_DENSE_TEST_EQ(test, asymmetric.code(),
                           asc::ErrorCode::kInvalidArgument);
  for (double value : factor_data) {
    ASC_RANDOM_DENSE_TEST_EQ(test, value, 19.0);
  }
}

void CheckMultivariateStatistics(TestContext& test) {
  constexpr std::size_t kSampleCount = 65536;
  constexpr std::size_t kDimension = 3;
  MultivariateFixture fixture;
  asc::DenseView<const double, 1> mean_view = [] {
    static std::array<double, 1> value{};
    return *MakeView<const double>(
        value.data(), std::array<asc::extent_t, 1>{1}, asc::LayoutLeft{});
  }();
  asc::DenseView<const double, 2> covariance_view = [] {
    static std::array<double, 1> value{};
    return *MakeView<const double>(
        value.data(), std::array<asc::extent_t, 2>{1, 1}, asc::LayoutLeft{});
  }();
  std::array<double, 1> placeholder{};
  auto placeholder_factor =
      MakeView(placeholder.data(), std::array<asc::extent_t, 2>{1, 1},
               asc::LayoutLeft{});
  asc::DenseView<double, 2> factor_view = *placeholder_factor;
  if (!PrepareFixture(fixture, mean_view, covariance_view, factor_view, test)) {
    return;
  }

  std::vector<double> output(kSampleCount * kDimension);
  std::array<double, kDimension> workspace{};
  auto output_view = MakeView(
      output.data(),
      std::array<asc::extent_t, 2>{static_cast<asc::extent_t>(kSampleCount),
                                   static_cast<asc::extent_t>(kDimension)},
      asc::LayoutRight{});
  auto workspace_view = MakeView(
      workspace.data(),
      std::array<asc::extent_t, 1>{static_cast<asc::extent_t>(kDimension)},
      asc::LayoutLeft{});
  auto normal = asc::NormalDistribution<double>::Create(0.0, 1.0);
  asc::NormalGenerator<asc::Pcg32, double> generator(
      asc::Pcg32(UINT64_C(0x243f6a8885a308d3), 17), *normal);
  const auto sampled = asc::FillDenseMultivariateNormal(
      asc::ExecutionContext::Serial(), *output_view, mean_view, factor_view,
      generator, *workspace_view);
  ASC_RANDOM_DENSE_TEST_CHECK(test, sampled.ok());
  if (!sampled.ok()) {
    return;
  }

  std::array<double, kDimension> observed_mean{};
  std::array<double, kDimension * kDimension> observed_covariance{};
  for (std::size_t sample = 0; sample < kSampleCount; ++sample) {
    for (std::size_t row = 0; row < kDimension; ++row) {
      observed_mean[row] += output[sample * kDimension + row];
      for (std::size_t column = 0; column < kDimension; ++column) {
        observed_covariance[row * kDimension + column] +=
            output[sample * kDimension + row] *
            output[sample * kDimension + column];
      }
    }
  }
  for (double& value : observed_mean) {
    value /= static_cast<double>(kSampleCount);
  }
  for (std::size_t row = 0; row < kDimension; ++row) {
    CheckNear(test, observed_mean[row], fixture.mean[row], 0.04,
              "multivariate mean");
    for (std::size_t column = 0; column < kDimension; ++column) {
      const double covariance = observed_covariance[row * kDimension + column] /
                                    static_cast<double>(kSampleCount) -
                                observed_mean[row] * observed_mean[column];
      CheckNear(test, covariance, fixture.covariance[row * kDimension + column],
                0.06, "multivariate covariance");
    }
  }
}

void CheckSphereStatistics(std::size_t dimension, TestContext& test) {
  constexpr std::size_t kSampleCount = 131072;
  std::vector<double> output(kSampleCount * dimension);
  std::vector<double> workspace(dimension);
  auto output_view = MakeView(
      output.data(),
      std::array<asc::extent_t, 2>{static_cast<asc::extent_t>(kSampleCount),
                                   static_cast<asc::extent_t>(dimension)},
      asc::LayoutRight{});
  auto workspace_view = MakeView(
      workspace.data(),
      std::array<asc::extent_t, 1>{static_cast<asc::extent_t>(dimension)},
      asc::LayoutLeft{});
  asc::Pcg32 engine(UINT64_C(0x13198a2e03707344) + dimension, 29);
  const auto sampled = asc::FillDenseUnitSphere(
      asc::ExecutionContext::Serial(), *output_view, engine, *workspace_view);
  ASC_RANDOM_DENSE_TEST_CHECK(test, sampled.ok());
  if (!sampled.ok()) {
    return;
  }

  std::vector<double> means(dimension);
  std::vector<double> moments(dimension * dimension);
  for (std::size_t sample = 0; sample < kSampleCount; ++sample) {
    double squared_norm = 0.0;
    for (std::size_t row = 0; row < dimension; ++row) {
      const double value = output[sample * dimension + row];
      squared_norm = std::fma(value, value, squared_norm);
      means[row] += value;
      for (std::size_t column = 0; column < dimension; ++column) {
        moments[row * dimension + column] +=
            value * output[sample * dimension + column];
      }
    }
    CheckNear(test, squared_norm, 1.0, 2.0e-12, "unit-sphere norm");
  }
  for (std::size_t row = 0; row < dimension; ++row) {
    means[row] /= static_cast<double>(kSampleCount);
    CheckNear(test, means[row], 0.0, 0.015, "unit-sphere mean");
    for (std::size_t column = 0; column < dimension; ++column) {
      moments[row * dimension + column] /= static_cast<double>(kSampleCount);
      const double expected = row == column ? 1.0 / dimension : 0.0;
      const double tolerance = row == column ? 0.02 : 0.015;
      CheckNear(test, moments[row * dimension + column], expected, tolerance,
                "unit-sphere moment");
    }
  }
}

void CheckSphereFailureAndDimensionOne(TestContext& test) {
  constexpr std::array<asc::extent_t, 2> kOneDimensionalExtents{2, 1};
  constexpr std::array<asc::extent_t, 1> kOneWorkspaceExtents{1};
  std::array<double, 2> signs{};
  std::array<double, 1> one_workspace{};
  auto signs_view =
      MakeView(signs.data(), kOneDimensionalExtents, asc::LayoutRight{});
  auto one_workspace_view =
      MakeView(one_workspace.data(), kOneWorkspaceExtents, asc::LayoutLeft{});
  WordSequenceEngine words;
  const auto signs_status = asc::FillDenseUnitSphere(
      asc::ExecutionContext::Serial(), *signs_view, words, *one_workspace_view);
  ASC_RANDOM_DENSE_TEST_CHECK(test, signs_status.ok());
  ASC_RANDOM_DENSE_TEST_EQ(test, signs[0], -1.0);
  ASC_RANDOM_DENSE_TEST_EQ(test, signs[1], 1.0);
  ASC_RANDOM_DENSE_TEST_EQ(test, words.calls(), std::size_t{2});

  constexpr std::array<asc::extent_t, 2> kExtents{1, 2};
  constexpr std::array<asc::extent_t, 1> kWorkspaceExtents{2};
  std::array<double, 2> output{31.0, 37.0};
  std::array<double, 2> workspace{};
  auto output_view = MakeView(output.data(), kExtents, asc::LayoutRight{});
  auto workspace_view =
      MakeView(workspace.data(), kWorkspaceExtents, asc::LayoutLeft{});
  ZeroEngine zero;
  const auto exhausted = asc::FillDenseUnitSphere(
      asc::ExecutionContext::Serial(), *output_view, zero, *workspace_view);
  ASC_RANDOM_DENSE_TEST_CHECK(test, !exhausted.ok());
  ASC_RANDOM_DENSE_TEST_EQ(test, exhausted.code(), asc::ErrorCode::kNumerical);
  ASC_RANDOM_DENSE_TEST_EQ(
      test, zero.calls(),
      asc::kUnitSphereMaximumAttemptsVersion1 * std::size_t{8});
  ASC_RANDOM_DENSE_TEST_EQ(test, output[0], 31.0);
  ASC_RANDOM_DENSE_TEST_EQ(test, output[1], 37.0);
}

void CheckAdvancedParameters(TestContext& test) {
  constexpr std::array<asc::extent_t, 1> kVectorExtents{2};
  constexpr std::array<asc::extent_t, 2> kMatrixExtents{2, 2};
  std::array<double, 2> mean_data{std::numeric_limits<double>::infinity(), 0.0};
  std::array<double, 4> covariance_data{1.0, 0.0, 0.0, 1.0};
  std::array<double, 4> factor_data{};
  factor_data.fill(41.0);
  auto mutable_mean =
      MakeView(mean_data.data(), kVectorExtents, asc::LayoutLeft{});
  auto mutable_covariance =
      MakeView(covariance_data.data(), kMatrixExtents, asc::LayoutRight{});
  auto factor =
      MakeView(factor_data.data(), kMatrixExtents, asc::LayoutRight{});
  ASC_RANDOM_DENSE_TEST_CHECK(test, mutable_mean.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(test, mutable_covariance.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(test, factor.ok());
  if (!mutable_mean.ok() || !mutable_covariance.ok() || !factor.ok()) {
    return;
  }
  asc::DenseView<const double, 1> mean = *mutable_mean;
  asc::DenseView<const double, 2> covariance = *mutable_covariance;
  const auto nonfinite = asc::PrepareDenseMultivariateNormal(
      asc::ExecutionContext::Serial(), mean, covariance, *factor);
  ASC_RANDOM_DENSE_TEST_CHECK(test, !nonfinite.ok());
  ASC_RANDOM_DENSE_TEST_EQ(test, nonfinite.code(),
                           asc::ErrorCode::kInvalidArgument);
  for (double value : factor_data) {
    ASC_RANDOM_DENSE_TEST_EQ(test, value, 41.0);
  }

  mean_data[0] = 0.0;
  auto alias_factor =
      MakeView(covariance_data.data(), kMatrixExtents, asc::LayoutRight{});
  const auto alias = asc::PrepareDenseMultivariateNormal(
      asc::ExecutionContext::Serial(), mean, covariance, *alias_factor);
  ASC_RANDOM_DENSE_TEST_CHECK(test, !alias.ok());
  ASC_RANDOM_DENSE_TEST_EQ(test, alias.code(),
                           asc::ErrorCode::kInvalidArgument);
  ASC_RANDOM_DENSE_TEST_EQ(test, covariance_data,
                           (std::array<double, 4>{1.0, 0.0, 0.0, 1.0}));

  factor_data = {1.0, 0.0, 0.0, 1.0};
  std::array<double, 4> output_data{};
  output_data.fill(43.0);
  std::array<double, 2> workspace_data{};
  auto output =
      MakeView(output_data.data(), kMatrixExtents, asc::LayoutRight{});
  auto workspace =
      MakeView(workspace_data.data(), kVectorExtents, asc::LayoutLeft{});
  auto shifted_normal = asc::NormalDistribution<double>::Create(1.0, 1.0);
  asc::NormalGenerator<asc::Pcg32, double> shifted_generator(asc::Pcg32(47, 53),
                                                             *shifted_normal);
  const auto initial_state = shifted_generator.engine().ExportState();
  const auto shifted = asc::FillDenseMultivariateNormal(
      asc::ExecutionContext::Serial(), *output, mean,
      asc::DenseView<const double, 2>(*factor), shifted_generator, *workspace);
  ASC_RANDOM_DENSE_TEST_CHECK(test, !shifted.ok());
  ASC_RANDOM_DENSE_TEST_EQ(test, shifted.code(),
                           asc::ErrorCode::kInvalidArgument);
  ASC_RANDOM_DENSE_TEST_EQ(test, shifted_generator.engine().ExportState(),
                           initial_state);
  for (double value : output_data) {
    ASC_RANDOM_DENSE_TEST_EQ(test, value, 43.0);
  }

  constexpr std::array<asc::extent_t, 1> kShortWorkspaceExtents{1};
  std::array<double, 1> short_workspace_data{};
  auto short_workspace = MakeView(short_workspace_data.data(),
                                  kShortWorkspaceExtents, asc::LayoutLeft{});
  asc::Pcg32 sphere_engine(59, 61);
  const auto sphere_initial_state = sphere_engine.ExportState();
  const auto bad_sphere =
      asc::FillDenseUnitSphere(asc::ExecutionContext::Serial(), *output,
                               sphere_engine, *short_workspace);
  ASC_RANDOM_DENSE_TEST_CHECK(test, !bad_sphere.ok());
  ASC_RANDOM_DENSE_TEST_EQ(test, bad_sphere.code(), asc::ErrorCode::kShape);
  ASC_RANDOM_DENSE_TEST_EQ(test, sphere_engine.ExportState(),
                           sphere_initial_state);
  for (double value : output_data) {
    ASC_RANDOM_DENSE_TEST_EQ(test, value, 43.0);
  }
}

}  // namespace

int main() {
  TestContext test;
  CheckPseudoFill(test);
  CheckPreparationAndKnownAnswer(test);
  CheckMultivariateStatistics(test);
  for (std::size_t dimension :
       {std::size_t{1}, std::size_t{2}, std::size_t{3}, std::size_t{8}}) {
    CheckSphereStatistics(dimension, test);
  }
  CheckSphereFailureAndDimensionOne(test);
  CheckAdvancedParameters(test);
  return test.Finish();
}
