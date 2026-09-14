#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <type_traits>

#include "../../src/dense/lapack/internal_tridiagonal.h"
#include "../../src/dense/lapack/internal_vector_conjugation_counts.h"
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_vector_conjugation.h"
#include "installed_lu/normal_return_guard.h"
#include "vector_conjugation_test_support.h"
namespace {
namespace conj = asc_vector_conjugation_test;
thread_local std::size_t g_calls = 0;
thread_local lapack_int g_n = 0;
thread_local lapack_int g_increment = 0;
thread_local bool g_omit = false;
void Counts(conj::TestContext& test) {
  using asc::internal_vector_conjugation_counts::Storage;
  constexpr auto kMaximum = std::numeric_limits<asc::stride_t>::max();
  constexpr auto kMinimum = std::numeric_limits<asc::stride_t>::min();
  for (const asc::extent_t limit :
       {static_cast<asc::extent_t>(std::numeric_limits<std::int32_t>::max()),
        std::numeric_limits<asc::extent_t>::max()}) {
    ASC_DENSE_TEST_EQ(test, *Storage(0, kMinimum, limit), 0);
    ASC_DENSE_TEST_EQ(test, *Storage(1, kMinimum, limit), 1);
    ASC_DENSE_TEST_EQ(test, *Storage(1, kMaximum, limit), 1);
    ASC_DENSE_TEST_EQ(test, *Storage(limit - 1, 0, limit), 1);
    ASC_DENSE_TEST_EQ(test, *Storage(limit - 1, 1, limit), limit - 1);
    ASC_DENSE_TEST_EQ(test, *Storage(limit - 1, -1, limit), limit - 1);
    ASC_DENSE_TEST_CHECK(test, !Storage(limit, 0, limit).ok());
    ASC_DENSE_TEST_CHECK(test, !Storage(-1, 1, limit).ok());
    ASC_DENSE_TEST_CHECK(test, !Storage(2, kMinimum, limit).ok());
    const auto positive = (limit - 1) / 13;
    ASC_DENSE_TEST_EQ(test, *Storage(13, positive, limit), 1 + 12 * positive);
    ASC_DENSE_TEST_CHECK(test, !Storage(13, positive + 1, limit).ok());
    const auto negative = (limit - 1) / 12;
    ASC_DENSE_TEST_EQ(test, *Storage(13, -negative, limit), 1 + 12 * negative);
    ASC_DENSE_TEST_CHECK(test, !Storage(13, -negative - 1, limit).ok());
  }
  ASC_DENSE_TEST_CHECK(test, !Storage(1, 1, 0).ok());
}
template <typename T>
void ScalarAndEmpty(conj::TestContext& test,
                    const asc::ReferenceLapackProvider& provider) {
  using Complex = std::complex<T>;
  asc::LapackReport report;
  for (asc::stride_t increment : {std::numeric_limits<asc::stride_t>::min(),
                                  std::numeric_limits<asc::stride_t>::max()}) {
    conj::Buffer<T> scalar(1, 1, Complex{3, 4});
    g_calls = 0;
    ASC_DENSE_TEST_CHECK(
        test, asc::Lacgv(provider, 1, increment, scalar.View(), report).ok());
    ASC_DENSE_TEST_EQ(test, g_calls, std::size_t{1});
    ASC_DENSE_TEST_EQ(test, g_n, 1);
    ASC_DENSE_TEST_EQ(test, g_increment, 1);
    ASC_DENSE_TEST_CHECK(test, conj::Same(scalar.values[1], Complex{3, -4}));
    conj::Buffer<T> empty(0, 1, Complex{});
    g_calls = 0;
    ASC_DENSE_TEST_CHECK(
        test, asc::Lacgv(provider, 0, increment, empty.View(), report).ok());
    ASC_DENSE_TEST_EQ(test, g_calls, std::size_t{0});
    ASC_DENSE_TEST_CHECK(test, !report.called_provider);
  }
}
template <typename T>
void Run(conj::TestContext& test) {
  using Complex = std::complex<T>;
  const auto provider = conj::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  conj::Buffer<T> buffer(5, 2, Complex{3, 4});
  const auto before = buffer.values;
  asc::LapackReport report;
  const auto reject = [&](asc::extent_t n, asc::stride_t increment,
                          asc::extent_t size, asc::stride_t view_increment,
                          asc::MemorySpace space, asc::ErrorCode expected) {
    const auto view = conj::Take(asc::DenseBlasVectorView<Complex>::Create(
        buffer.values.data() + 1, size, view_increment,
        {buffer.values.data(), sizeof(buffer.values), space}));
    report.called_provider = true;
    report.native_info = 79;
    g_calls = 0;
    const auto status = conj::WithoutAllocation(
        test, [&] { return asc::Lacgv(provider, n, increment, view, report); });
    ASC_DENSE_TEST_EQ(test, status.code(), expected);
    ASC_DENSE_TEST_EQ(test, g_calls, std::size_t{0});
    ASC_DENSE_TEST_CHECK(test, !report.called_provider);
    ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
    for (std::size_t i = 0; i < before.size(); ++i) {
      ASC_DENSE_TEST_CHECK(test, conj::Same(buffer.values[i], before[i]));
    }
  };
  reject(-1, 1, 1, 1, conj::kHost, asc::ErrorCode::kInvalidArgument);
  reject(asc::internal_tridiagonal::kLimit, 0, 1, 1, conj::kHost,
         asc::ErrorCode::kOverflow);
  reject(2, asc::internal_tridiagonal::kLimit, 1, 1, conj::kHost,
         asc::ErrorCode::kOverflow);
  reject(2, std::numeric_limits<asc::stride_t>::min(), 1, 1, conj::kHost,
         asc::ErrorCode::kOverflow);
  for (asc::extent_t size : {0, 1, 8, 10}) {
    reject(5, 2, size, 1, conj::kHost, asc::ErrorCode::kShape);
  }
  reject(5, 2, 9, 2, conj::kHost, asc::ErrorCode::kShape);
  reject(5, 2, 9, 1, asc::MemorySpace::kDevice, asc::ErrorCode::kMemoryAccess);
  reject(5, 2, 9, 1, asc::MemorySpace::kManaged, asc::ErrorCode::kMemoryAccess);
  // Aliased metadata is rejected without accessing a nonexistent complex
  // object.
  const auto alias = conj::Take(asc::DenseBlasVectorView<Complex>::Create(
      reinterpret_cast<Complex*>(&report), 1, 1,
      {&report, sizeof(report), conj::kHost}));
  report.called_provider = true;
  report.native_info = 79;
  g_calls = 0;
  ASC_DENSE_TEST_EQ(test, asc::Lacgv(provider, 1, 1, alias, report).code(),
                    asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, g_calls, std::size_t{0});
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, *report.native_info, 79);
  ScalarAndEmpty<T>(test, provider);
  // No INFO channel can distinguish normal return without numeric writes.
  // Keep this limitation explicit; do not invent a numerical certificate.
  g_omit = true;
  g_calls = 0;
  ASC_DENSE_TEST_CHECK(test,
                       asc::Lacgv(provider, 5, 2, buffer.View(), report).ok());
  ASC_DENSE_TEST_EQ(test, g_calls, std::size_t{1});
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
  for (std::size_t i = 0; i < before.size(); ++i) {
    ASC_DENSE_TEST_CHECK(test, conj::Same(buffer.values[i], before[i]));
  }
  g_omit = false;
}
}  // namespace
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" decltype(LAPACK_clacgv) __real_clacgv_;
extern "C" void __wrap_clacgv_(const lapack_int* n, std::complex<float>* data,
                               const lapack_int* increment) {
  ++g_calls;
  g_n = *n;
  g_increment = *increment;
  if (!g_omit) {
    __real_clacgv_(n, data, increment);
  }
}
extern "C" decltype(LAPACK_zlacgv) __real_zlacgv_;
extern "C" void __wrap_zlacgv_(const lapack_int* n, std::complex<double>* data,
                               const lapack_int* increment) {
  ++g_calls;
  g_n = *n;
  g_increment = *increment;
  if (!g_omit) {
    __real_zlacgv_(n, data, increment);
  }
}
static_assert(
    std::is_same_v<decltype(__wrap_clacgv_), decltype(LAPACK_clacgv)>);
static_assert(
    std::is_same_v<decltype(__wrap_zlacgv_), decltype(LAPACK_zlacgv)>);
// NOLINTEND(bugprone-reserved-identifier)
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard guard;
  if (argc != 2) {
    return 2;
  }
  const std::string_view scalar(argv[1]);
  conj::TestContext test;
  if (scalar == "counts") {
    Counts(test);
  } else if (scalar == "c") {
    Run<float>(test);
  } else if (scalar == "z") {
    Run<double>(test);
  } else {
    return 2;
  }
  return test.Finish();
}
