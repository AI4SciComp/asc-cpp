#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <span>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_machine.h"
#include "installed_lu/normal_return_guard.h"
#include "machine_constants_faults.h"
#include "machine_constants_test_support.h"

namespace {
namespace machine_constants = asc_machine_constants_test;
namespace fault = asc_machine_constants_fault;
using machine_constants::TestContext;

template <typename Object>
auto Bytes(const Object& object) {
  std::array<std::byte, sizeof(Object)> bytes{};
  const auto view = std::as_bytes(std::span(&object, 1));
  std::copy(view.begin(), view.end(), bytes.begin());
  return bytes;
}

template <typename T>
struct Sample {
  asc::ReferenceLapackProvider provider = machine_constants::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  machine_constants::Output<T> c;
  asc::LapackReport report;
  std::array<asc::DenseBlasVectorView<T>, 1> Views() { return {c.View()}; }
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
              std::array<asc::DenseBlasVectorView<T>, 1> views,
              asc::ErrorCode expected, bool preserve_report,
              asc::LapackMachineParameter parameter =
                  asc::LapackMachineParameter::kRelativePrecision) {
  const auto provider_bytes = Bytes(sample.provider);
  const auto report_bytes = Bytes(sample.report);
  const auto c = sample.c.values;
  fault::Reset(fault::Mode::kPass);
  const auto status = machine_constants::WithoutAllocation(test, [&] {
    return asc::Lamch(sample.provider, parameter, views[0], sample.report);
  });
  ASC_DENSE_TEST_EQ(test, status.code(), expected);
  ASC_DENSE_TEST_EQ(test, fault::Calls(), 0U);
  ASC_DENSE_TEST_EQ(test, Bytes(sample.provider), provider_bytes);
  ASC_DENSE_TEST_EQ(test, sample.c.values, c);
  if (preserve_report) {
    ASC_DENSE_TEST_EQ(test, Bytes(sample.report), report_bytes);
  } else {
    ASC_DENSE_TEST_CHECK(test, !sample.report.called_provider);
    ASC_DENSE_TEST_CHECK(test, !sample.report.native_info.has_value());
    ASC_DENSE_TEST_CHECK(test, !sample.report.diagnostic_index.has_value());
    ASC_DENSE_TEST_EQ(test, sample.report.output_validity,
                      asc::LapackOutputValidity::kUnchanged);
    ASC_DENSE_TEST_EQ(test, sample.report.outcome, asc::LapackOutcome::kNotRun);
  }
}

template <typename T>
void Metadata(TestContext& test) {
  Sample<T> sample;
  std::size_t count = 0;
  for (const asc::MutableMemoryView memory :
       {asc::MutableMemoryView{&sample.provider, sizeof(sample.provider),
                               machine_constants::kHost},
        asc::MutableMemoryView{&sample.report, sizeof(sample.report),
                               machine_constants::kHost}}) {
    for (std::size_t offset = 0; offset + sizeof(T) <= memory.size();
         offset += alignof(T)) {
      for (std::size_t operand = 0; operand < 1; ++operand) {
        sample.Seed();
        auto views = sample.Views();
        auto* bytes = static_cast<std::byte*>(memory.data());
        // Only a descriptor is formed over live metadata. The operation must
        // reject it before any numeric load; no unrelated T object is accessed.
        views[operand] =
            machine_constants::Take(asc::DenseBlasVectorView<T>::Create(
                reinterpret_cast<T*>(bytes + offset), 1, 1, memory));
        Rejected(test, sample, views, asc::ErrorCode::kInvalidArgument, true);
        ++count;
      }
    }
  }
  std::printf("real machine_constants metadata aliases=%zu\n", count);
}

template <typename T>
void Structural(TestContext& test) {
  Sample<T> sample;
  for (std::size_t operand = 0; operand < 1; ++operand) {
    for (int size : {0, 1, 2}) {
      for (int increment : {-1, 1, 2}) {
        if (size == 1 && increment == 1) {
          continue;
        }
        std::array<T, 8> values{};
        values.fill(T{-37});
        const auto before = values;
        auto views = sample.Views();
        views[operand] =
            machine_constants::Take(asc::DenseBlasVectorView<T>::Create(
                values.data() + 2, size, increment,
                {values.data(), sizeof(values), machine_constants::kHost}));
        sample.Seed();
        Rejected(test, sample, views, asc::ErrorCode::kShape, false);
        ASC_DENSE_TEST_EQ(test, values, before);
      }
    }
    for (const auto space :
         {asc::MemorySpace::kDevice, asc::MemorySpace::kManaged}) {
      machine_constants::Output<T> output;
      const auto before = output.values;
      auto views = sample.Views();
      views[operand] =
          machine_constants::Take(asc::DenseBlasVectorView<T>::Create(
              output.values.data() + 1, 1, 1,
              {output.values.data(), sizeof(output.values), space}));
      sample.Seed();
      Rejected(test, sample, views, asc::ErrorCode::kMemoryAccess, false);
      ASC_DENSE_TEST_EQ(test, output.values, before);
    }
  }
}

template <typename T>
void Controls(TestContext& test) {
  Sample<T> sample;
  for (const auto fixture : asc_machine_constants_numerical::Cases<T>()) {
    const auto views = sample.Views();
    sample.Seed();
    fault::Reset(fault::Mode::kPass);
    const auto status = machine_constants::WithoutAllocation(test, [&] {
      return asc::Lamch(sample.provider, fixture.parameter, views[0],
                        sample.report);
    });
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_EQ(test, fault::Calls(), 1U);
    ASC_DENSE_TEST_EQ(test, fault::UnexpectedNativeResults(), 0U);
    ASC_DENSE_TEST_EQ(test,
                      asc_machine_constants_numerical::Bits(sample.c.values[1]),
                      asc_machine_constants_numerical::Bits(fixture.expected));
    ASC_DENSE_TEST_CHECK(test, !sample.report.native_info.has_value());
  }
  for (unsigned raw = 10; raw <= 255; ++raw) {
    sample.Seed();
    Rejected(test, sample, sample.Views(), asc::ErrorCode::kInvalidArgument,
             false, static_cast<asc::LapackMachineParameter>(raw));
  }
  std::printf("246 invalid LAMCH selectors rejected locally\n");
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
