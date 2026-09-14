#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <span>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "installed_lu/normal_return_guard.h"
#include "scalar_svd_faults.h"
#include "scalar_svd_test_support.h"

namespace {
namespace scalar_svd = asc_scalar_svd_test;
namespace fault = asc_scalar_svd_fault;
using scalar_svd::TestContext;

template <typename Object>
auto Bytes(const Object& object) {
  std::array<std::byte, sizeof(Object)> bytes{};
  const auto view = std::as_bytes(std::span(&object, 1));
  std::copy(view.begin(), view.end(), bytes.begin());
  return bytes;
}

template <typename T>
struct Sample {
  asc::ReferenceLapackProvider provider = scalar_svd::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  scalar_svd::Output<T> output;
  asc::LapackReport report;
  std::array<asc::DenseBlasVectorView<T>, 6> Views() { return output.Views(); }
  void Seed() {
    report.called_provider = true;
    report.native_info = 79;
    report.diagnostic_index = 7;
    report.outcome = asc::LapackOutcome::kSuccess;
    report.output_validity = asc::LapackOutputValidity::kComplete;
  }
};

template <typename T>
void Rejected(TestContext& test, Sample<T>& sample,
              std::array<asc::DenseBlasVectorView<T>, 6> views,
              asc::ErrorCode expected, bool preserve_report, T f = 3, T g = 4,
              T h = 5) {
  const auto provider_bytes = Bytes(sample.provider);
  const auto report_bytes = Bytes(sample.report);
  const auto before = sample.output.values;
  fault::Reset(fault::Mode::kPass);
  const auto status = scalar_svd::WithoutAllocation(test, [&] {
    return scalar_svd::Execute(sample.provider, f, g, h, views, sample.report);
  });
  ASC_DENSE_TEST_EQ(test, status.code(), expected);
  ASC_DENSE_TEST_EQ(test, fault::Calls(), 0U);
  ASC_DENSE_TEST_EQ(test, Bytes(sample.provider), provider_bytes);
  ASC_DENSE_TEST_EQ(test, sample.output.values, before);
  if (preserve_report) {
    ASC_DENSE_TEST_EQ(test, Bytes(sample.report), report_bytes);
  } else {
    ASC_DENSE_TEST_CHECK(test, !sample.report.called_provider);
    ASC_DENSE_TEST_CHECK(test, !sample.report.native_info.has_value());
    ASC_DENSE_TEST_CHECK(test, !sample.report.diagnostic_index.has_value());
    ASC_DENSE_TEST_EQ(test, sample.report.output_validity,
                      asc::LapackOutputValidity::kUnchanged);
    ASC_DENSE_TEST_EQ(test, sample.report.outcome,
                      expected == asc::ErrorCode::kNumerical
                          ? asc::LapackOutcome::kAccuracyWarning
                          : asc::LapackOutcome::kNotRun);
  }
}

template <typename T>
void Metadata(TestContext& test) {
  Sample<T> sample;
  std::size_t count = 0;
  for (const asc::MutableMemoryView memory :
       {asc::MutableMemoryView{&sample.provider, sizeof(sample.provider),
                               scalar_svd::kHost},
        asc::MutableMemoryView{&sample.report, sizeof(sample.report),
                               scalar_svd::kHost}}) {
    for (std::size_t offset = 0; offset + sizeof(T) <= memory.size();
         offset += alignof(T)) {
      for (std::size_t operand = 0; operand < 6; ++operand) {
        sample.Seed();
        auto views = sample.Views();
        auto* bytes = static_cast<std::byte*>(memory.data());
        // Only a descriptor is formed over live metadata. The operation must
        // reject it before any numeric load; no unrelated T object is accessed.
        views[operand] = scalar_svd::Take(asc::DenseBlasVectorView<T>::Create(
            reinterpret_cast<T*>(bytes + offset), 1, 1, memory));
        Rejected(test, sample, views, asc::ErrorCode::kInvalidArgument, true);
        ++count;
      }
    }
  }
  std::printf("real scalar_svd metadata aliases=%zu\n", count);
}

template <typename T>
void Structural(TestContext& test) {
  Sample<T> sample;
  for (std::size_t operand = 0; operand < 6; ++operand) {
    for (int size : {0, 1, 2}) {
      for (int increment : {-1, 1, 2}) {
        if (size == 1 && increment == 1) {
          continue;
        }
        std::array<T, 8> values{};
        values.fill(T{-37});
        const auto before = values;
        auto views = sample.Views();
        views[operand] = scalar_svd::Take(asc::DenseBlasVectorView<T>::Create(
            values.data() + 2, size, increment,
            {values.data(), sizeof(values), scalar_svd::kHost}));
        sample.Seed();
        Rejected(test, sample, views, asc::ErrorCode::kShape, false);
        ASC_DENSE_TEST_EQ(test, values, before);
      }
    }
    for (const auto space :
         {asc::MemorySpace::kDevice, asc::MemorySpace::kManaged}) {
      std::array<T, 3> output{71, -19, 73};
      const auto before = output;
      auto views = sample.Views();
      views[operand] = scalar_svd::Take(asc::DenseBlasVectorView<T>::Create(
          output.data() + 1, 1, 1, {output.data(), sizeof(output), space}));
      sample.Seed();
      Rejected(test, sample, views, asc::ErrorCode::kMemoryAccess, false);
      ASC_DENSE_TEST_EQ(test, output, before);
    }
    for (std::size_t other = 0; other < 6; ++other) {
      if (operand == other) {
        continue;
      }
      auto views = sample.Views();
      views[operand] = views[other];
      sample.Seed();
      Rejected(test, sample, views, asc::ErrorCode::kInvalidArgument, false);
    }
  }
  const T infinity = std::numeric_limits<T>::infinity();
  const T nan = std::numeric_limits<T>::quiet_NaN();
  // All rejected nonfinite tuples remain local; wrappers must observe no entry.
  for (const T f : {T{3}, infinity, -infinity, nan}) {
    for (const T g : {T{4}, infinity, -infinity, nan}) {
      for (const T h : {T{5}, infinity, -infinity, nan}) {
        const int infinities = static_cast<int>(std::isinf(f)) +
                               static_cast<int>(std::isinf(g)) +
                               static_cast<int>(std::isinf(h));
        if (std::isnan(f) || std::isnan(g) || std::isnan(h) || infinities > 1) {
          sample.Seed();
          Rejected(test, sample, sample.Views(), asc::ErrorCode::kNumerical,
                   false, f, g, h);
        }
      }
    }
  }
}

template <typename T>
void Controls(TestContext& test) {
  Sample<T> sample;
  // Inputs are copied before execution and may originate in output objects.
  auto& v = sample.output.values;
  v[0][1] = 3;
  v[1][1] = 4;
  v[2][1] = 5;
  const auto views = sample.Views();
  sample.Seed();
  fault::Reset(fault::Mode::kPass);
  const auto status = scalar_svd::WithoutAllocation(test, [&] {
    return scalar_svd::Execute(sample.provider, v[0][1], v[1][1], v[2][1],
                               views, sample.report);
  });
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_EQ(test, fault::Calls(), 1U);
  ASC_DENSE_TEST_EQ(test, fault::UnexpectedNativeResults(), 0U);
  ASC_DENSE_TEST_CHECK(test, asc_scalar_svd_numerical::Accurate(
                                 T{3}, T{4}, T{5}, v[0][1], v[1][1], v[2][1],
                                 v[3][1], v[4][1], v[5][1]));
  ASC_DENSE_TEST_CHECK(test, !sample.report.native_info.has_value());
}

template <typename T>
void Run(TestContext& test) {
  Metadata<T>(test);
  Structural<T>(test);
  Controls<T>(test);
}
}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard guard;
  if (argc != 2) {
    return 2;
  }
  TestContext test;
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    Run<float>(test);
  } else if (scalar == "d") {
    Run<double>(test);
  } else {
    return 2;
  }
  return test.Finish();
}
