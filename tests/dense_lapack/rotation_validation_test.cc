#include <algorithm>
#include <array>
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
#include "asc/dense/providers/lapack_rotation.h"
#include "installed_lu/normal_return_guard.h"
#include "rotation_faults.h"
#include "rotation_test_support.h"

namespace {
namespace rotation = asc_rotation_test;
namespace fault = asc_rotation_fault;
using rotation::TestContext;

template <typename Object>
auto Bytes(const Object& object) {
  std::array<std::byte, sizeof(Object)> bytes{};
  const auto view = std::as_bytes(std::span(&object, 1));
  std::copy(view.begin(), view.end(), bytes.begin());
  return bytes;
}

template <typename T>
struct Sample {
  asc::ReferenceLapackProvider provider = rotation::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  rotation::Output<T> c;
  rotation::Output<T> s;
  rotation::Output<T> r;
  asc::LapackReport report;
  std::array<asc::DenseBlasVectorView<T>, 3> Views() {
    return {c.View(), s.View(), r.View()};
  }
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
              std::array<asc::DenseBlasVectorView<T>, 3> views,
              asc::ErrorCode expected, bool preserve_report, T f = 3, T g = 4) {
  const auto provider_bytes = Bytes(sample.provider);
  const auto report_bytes = Bytes(sample.report);
  const auto c = sample.c.values;
  const auto s = sample.s.values;
  const auto r = sample.r.values;
  fault::Reset(fault::Mode::kPass);
  const auto status = rotation::WithoutAllocation(test, [&] {
    return asc::Lartg(sample.provider, f, g, views[0], views[1], views[2],
                      sample.report);
  });
  ASC_DENSE_TEST_EQ(test, status.code(), expected);
  ASC_DENSE_TEST_EQ(test, fault::Calls(), 0U);
  ASC_DENSE_TEST_EQ(test, Bytes(sample.provider), provider_bytes);
  ASC_DENSE_TEST_EQ(test, sample.c.values, c);
  ASC_DENSE_TEST_EQ(test, sample.s.values, s);
  ASC_DENSE_TEST_EQ(test, sample.r.values, r);
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
                               rotation::kHost},
        asc::MutableMemoryView{&sample.report, sizeof(sample.report),
                               rotation::kHost}}) {
    for (std::size_t offset = 0; offset + sizeof(T) <= memory.size();
         offset += alignof(T)) {
      for (std::size_t operand = 0; operand < 3; ++operand) {
        sample.Seed();
        auto views = sample.Views();
        auto* bytes = static_cast<std::byte*>(memory.data());
        // Only a descriptor is formed over live metadata. The operation must
        // reject it before any numeric load; no unrelated T object is accessed.
        views[operand] = rotation::Take(asc::DenseBlasVectorView<T>::Create(
            reinterpret_cast<T*>(bytes + offset), 1, 1, memory));
        Rejected(test, sample, views, asc::ErrorCode::kInvalidArgument, true);
        ++count;
      }
    }
  }
  std::printf("real rotation metadata aliases=%zu\n", count);
}

template <typename T>
void Structural(TestContext& test) {
  Sample<T> sample;
  for (std::size_t operand = 0; operand < 3; ++operand) {
    for (int size : {0, 1, 2}) {
      for (int increment : {-1, 1, 2}) {
        if (size == 1 && increment == 1) {
          continue;
        }
        std::array<T, 8> values{};
        values.fill(T{-37});
        const auto before = values;
        auto views = sample.Views();
        views[operand] = rotation::Take(asc::DenseBlasVectorView<T>::Create(
            values.data() + 2, size, increment,
            {values.data(), sizeof(values), rotation::kHost}));
        sample.Seed();
        Rejected(test, sample, views, asc::ErrorCode::kShape, false);
        ASC_DENSE_TEST_EQ(test, values, before);
      }
    }
    for (const auto space :
         {asc::MemorySpace::kDevice, asc::MemorySpace::kManaged}) {
      rotation::Output<T> output;
      const auto before = output.values;
      auto views = sample.Views();
      views[operand] = rotation::Take(asc::DenseBlasVectorView<T>::Create(
          output.values.data() + 1, 1, 1,
          {output.values.data(), sizeof(output.values), space}));
      sample.Seed();
      Rejected(test, sample, views, asc::ErrorCode::kMemoryAccess, false);
      ASC_DENSE_TEST_EQ(test, output.values, before);
    }
    for (std::size_t other = 0; other < 3; ++other) {
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
  for (const T nonfinite : {infinity, -infinity, nan}) {
    sample.Seed();
    Rejected(test, sample, sample.Views(), asc::ErrorCode::kNumerical, false,
             nonfinite, T{4});
    sample.Seed();
    Rejected(test, sample, sample.Views(), asc::ErrorCode::kNumerical, false,
             T{3}, nonfinite);
    sample.Seed();
    Rejected(test, sample, sample.Views(), asc::ErrorCode::kNumerical, false,
             nonfinite, nonfinite);
  }
}

template <typename T>
void Controls(TestContext& test) {
  Sample<T> sample;
  // By-value inputs may come from an output object: both values are copied
  // before execution starts, and no old output value is subsequently read.
  sample.c.values[1] = 3;
  sample.s.values[1] = 4;
  const auto views = sample.Views();
  sample.Seed();
  fault::Reset(fault::Mode::kPass);
  const auto status = rotation::WithoutAllocation(test, [&] {
    return asc::Lartg(sample.provider, sample.c.values[1], sample.s.values[1],
                      views[0], views[1], views[2], sample.report);
  });
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_EQ(test, fault::Calls(), 1U);
  ASC_DENSE_TEST_EQ(test, fault::UnexpectedNativeResults(), 0U);
  ASC_DENSE_TEST_CHECK(test, asc_rotation_numerical::Accurate(
                                 T{3}, T{4}, sample.c.values[1],
                                 sample.s.values[1], sample.r.values[1]));
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
